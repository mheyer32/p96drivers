#include "mach64GM.h"
#include "chip_mach64.h"
#include "mach64GT.h"
#include "mach64_common.h"

#include <exec/memory.h>
#include <exec/tasks.h>
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/openpci.h>

struct Library *PCIInitBase;
#include <proto/pciinit.h>

using namespace MmioReg;
using namespace PllReg;

#ifdef __cplusplus
extern "C" {
#endif

/* POST runs on the caller's stack. Picasso96's stack is far smaller than the ROM needs. */
#define GM_BIOS_STACK (128UL * 1024UL)

/* PLL_EXT_CNTL bit3: feedback path is ×4. Fold into N so 2*R*N/(M*P) matches. */
#define GM_PLL_MFB_TIMES_4 0x08

#define BUS_MASTER_DIS_GT        BIT(6)
#define BUS_MASTER_DIS_GT_MASK   BIT(6)
#define BUS_PCI_RETRY_EN_GT      BIT(15)
#define BUS_PCI_RETRY_EN_GT_MASK BIT(15)
#define BUS_FIFO_WS_GT(x)        ((x) << 16)
#define BUS_FIFO_WS_GT_MASK      (0xF << 16)

static const UBYTE gmXclkPostDiv[] = {1, 2, 4, 8, 3};

static const char *gmPiStatus(uint32_t status)
{
    static const char *const names[] = {"ok",
                                        "bad argument",
                                        "out of memory",
                                        "I/O error",
                                        "malformed ROM",
                                        "unsupported ROM",
                                        "OpenPCI unavailable",
                                        "card owned",
                                        "no ROM",
                                        "ROM identity mismatch",
                                        "hardware execution backend unavailable",
                                        "invalid context state",
                                        "execution failed",
                                        "VBE service failed",
                                        "unsupported graphics mode"};
    return status < ARRAY_SIZE(names) ? names[status] : "unknown status";
}

static BOOL gmDropClaim(struct BoardInfo *bi, struct Node **saved)
{
    LOCAL_OPENPCIBASE();
    struct Node *owner = NULL;
    *saved             = NULL;
    GetBoardAttrs(getCardData(bi)->board, PRM_BoardOwner, (Tag)&owner, TAG_END);
    if (!owner)
        return TRUE;
#ifndef TESTEXE
    if (owner != &getCardData(bi)->boardNode)
        return TRUE;
#endif
    if (!SetBoardAttrs(getCardData(bi)->board, PRM_BoardOwner, (Tag)NULL, TAG_END)) {
        DFUNC(ERROR, "Could not drop board claim for option ROM\n");
        return FALSE;
    }
    *saved = owner;
    return TRUE;
}

static void gmRestoreClaim(struct BoardInfo *bi, struct Node *saved)
{
    if (!saved)
        return;
    LOCAL_OPENPCIBASE();
    SetBoardAttrs(getCardData(bi)->board, PRM_BoardOwner, (Tag)saved, TAG_END);
}

struct GmBiosJob
{
    PCIInitContext *ctx;
    const PCIInitImage *image;
    uint32_t status;
};

static struct GmBiosJob gmBiosJob;
static struct StackSwapStruct gmBiosStack;
static struct ExecBase *gmSysBase;

/* StackSwap exchanges SP. Every value this function touches lives in static storage. */
static __attribute__((noinline)) void gmRunRom(void)
{
    static struct ExecBase *SysBase;
    SysBase = gmSysBase;
    StackSwap(&gmBiosStack);
    asm volatile("" ::: "memory");
    gmBiosJob.status = RunOptionROM(gmBiosJob.ctx, gmBiosJob.image);
    asm volatile("" ::: "memory");
    SysBase = gmSysBase;
    StackSwap(&gmBiosStack);
}

/* Frequency table: ref_clock_freq is in 10 kHz units (1432 → 14.32 MHz). */
static void gmPrintRefClock(struct BoardInfo *bi, const PCIInitImage *image)
{
    struct PCIInitROMInfo info;
    const UBYTE *rom;
    UWORD atiOff;
    UWORD freqOff;
    const Mach64RomHeader_t *hdr;
    const FrequencyTable_t *ft;
    UWORD refFreq;
    UWORD refDiv;
    UBYTE liveM = 0;

    info.size = sizeof(info);
    if (GetOptionROMInfo(image, &info) != PI_OK || !info.source || info.source_bytes < 0x4a) {
        D(WARN, "ROM ref clock: no image info\n");
        return;
    }

    rom    = info.source;
    atiOff = swapw(*(const UWORD *)(rom + 0x48));
    if (atiOff < 2 || (ULONG)atiOff - 2 + sizeof(Mach64RomHeader_t) > info.source_bytes) {
        D(WARN, "ROM ref clock: bad ATI header ptr 0x%lx\n", (ULONG)atiOff);
        return;
    }

    hdr     = (const Mach64RomHeader_t *)(rom + atiOff - 2);
    freqOff = swapw(hdr->freq_table_ptr);
    if (!freqOff || (ULONG)freqOff + sizeof(FrequencyTable_t) > info.source_bytes) {
        D(WARN, "ROM ref clock: bad frequency table ptr 0x%lx\n", (ULONG)freqOff);
        return;
    }

    ft      = (const FrequencyTable_t *)(rom + freqOff);
    refFreq = swapw(ft->ref_clock_freq);
    refDiv  = swapw(ft->ref_clock_divider);

    if (bi->MemoryBase)
        liveM = ReadPLL(bi, PLL_REF_DIV);

    D(INFO, "ROM reference clock %ld.%02ld MHz (table %ld), divider %ld, PLL_REF_DIV %ld\n",
      (ULONG)(refFreq / 100), (ULONG)(refFreq % 100), (ULONG)refFreq, (ULONG)refDiv, (ULONG)liveM);
}

BOOL mach64GMRunBios(struct BoardInfo *bi)
{
    LOCAL_SYSBASE();
    BOOL ok               = FALSE;
    uint32_t status       = PI_OK;
    struct Node *claim    = NULL;
    PCIInitImage *image   = NULL;
    PCIInitContext *ctx   = NULL;
    APTR stack            = NULL;
    struct pci_dev *board = getCardData(bi)->board;
    /* LoadOptionROMFromCard rejects PII_Hardware; CreatePCIContext needs it. */
    struct TagItem loadTags[] = {{PII_Error, (ULONG)&status}, {TAG_DONE, 0}};
    struct TagItem runTags[]  = {{PII_Error, (ULONG)&status}, {PII_Hardware, 1}, {TAG_DONE, 0}};

    PCIInitBase = OpenLibrary((STRPTR) "pciinit.library", PCIINIT_LIBRARY_VERSION);
    if (!PCIInitBase) {
        DFUNC(ERROR, "Cannot open pciinit.library v%ld\n", (ULONG)PCIINIT_LIBRARY_VERSION);
        return FALSE;
    }

    {
        LOCAL_OPENPCIBASE();
        UWORD cmd = pci_read_config_word(PCI_COMMAND, board);
        pci_write_config_word(PCI_COMMAND, cmd | PCI_COMMAND_MEMORY | PCI_COMMAND_IO, board);
    }
    /* Warm reinit leaves VGA decode and the expansion ROM disabled.
     * Block I/O, so this does not depend on the linear aperture. */
    if (bi->RegisterBase) {
        asMach64(bi)->writeMaskConfigCntl(CFG_VGA_DIS_MASK, 0);
        asMach64(bi)->blkIo().writeMaskL(BlkIoReg::BUS_CNTL, BUS_ROM_DIS_MASK, 0);
    }

    if (!gmDropClaim(bi, &claim))
        goto out;

    image = LoadOptionROMFromCard(board, loadTags);
    if (!image) {
        DFUNC(ERROR, "LoadOptionROMFromCard: %s\n", gmPiStatus(status));
        goto out;
    }

    ctx = CreatePCIContext(board, runTags);
    if (!ctx) {
        DFUNC(ERROR, "CreatePCIContext: %s\n", gmPiStatus(status));
        goto out;
    }

    stack = AllocMem(GM_BIOS_STACK, MEMF_PUBLIC | MEMF_FAST);
    if (!stack)
        stack = AllocMem(GM_BIOS_STACK, MEMF_PUBLIC);
    if (!stack) {
        DFUNC(ERROR, "Option ROM stack\n");
        goto out;
    }
    gmBiosStack.stk_Lower   = stack;
    gmBiosStack.stk_Upper   = (ULONG)stack + GM_BIOS_STACK;
    gmBiosStack.stk_Pointer = (APTR)gmBiosStack.stk_Upper;
    gmBiosJob.ctx           = ctx;
    gmBiosJob.image         = image;
    gmBiosJob.status        = PI_EXECUTION_FAILED;
    gmSysBase               = (struct ExecBase *)SysBase;

    gmRunRom();
    status = gmBiosJob.status;
    ok     = status == PI_OK;
    if (!ok)
        DFUNC(ERROR, "Option ROM POST failed: %s\n", gmPiStatus(status));
    gmPrintRefClock(bi, image);

out:
    if (ctx)
        DeletePCIContext(ctx);
    if (image)
        FreeOptionROM(image);
    if (stack)
        FreeMem(stack, GM_BIOS_STACK);
    gmRestoreClaim(bi, claim);
    CloseLibrary(PCIInitBase);
    PCIInitBase = NULL;
    return ok;
}

static void gmCaptureMemoryPll(struct BoardInfo *bi)
{
    DRIVER_LOCALS(bi);
    ChipSpecific_t *cs = getChipSpecific(bi);

    UBYTE liveM = READ_PLL(PLL_REF_DIV);
    UBYTE ext   = READ_PLL(PLL_EXT_CNTL);
    UBYTE n     = READ_PLL(PLL_MCLK_FB_DIV);
    UBYTE code  = ext & 7;
    UBYTE post  = code < ARRAY_SIZE(gmXclkPostDiv) ? gmXclkPostDiv[code] : 1;
    UWORD fb    = n;

    if (liveM && liveM != cs->referenceDivider) {
        D(INFO, "PLL_REF_DIV live %ld, ROM %ld\n", (ULONG)liveM, (ULONG)cs->referenceDivider);
        cs->referenceDivider = liveM;
    }
    if (ext & GM_PLL_MFB_TIMES_4)
        fb = (UWORD)n << 1;
    if (code >= ARRAY_SIZE(gmXclkPostDiv))
        D(WARN, "PLL_EXT_CNTL XCLK source %ld\n", (ULONG)code);

    cs->mclkFBDiv   = fb;
    cs->mclkPostDiv = post;

    if (cs->referenceFrequency && cs->referenceDivider && post) {
        ULONG khz10      = computeFrequencyKhz10(cs->referenceFrequency, fb, cs->referenceDivider, post);
        bi->MemoryClock  = khz10 * 10000UL;
    }
    D(INFO, "BIOS MCLK N=%ld post=%ld EXT=%02lx GEN=%02lx VCLK=%02lx MemoryClock %ld Hz\n", (ULONG)fb, (ULONG)post,
      (ULONG)ext, (ULONG)READ_PLL(PLL_GEN_CNTL), (ULONG)READ_PLL(PLL_VCLK_CNTL), bi->MemoryClock);
}

/* BIOS already programmed MEM_CNTL / the memory PLL. Find the size that does not alias. */
static BOOL gmProbeFramebuffer(struct BoardInfo *bi)
{
    LOCAL_SYSBASE();
    Mach64BlkIo blk = asMach64(bi)->blkIo();

    D(INFO, "BIOS MEM_CNTL=0x%08lx MEM_ADDR_CONFIG=0x%08lx CONFIG_STAT0=0x%08lx\n", blk.readL(BlkIoReg::MEM_CNTL),
      blk.readL(BlkIoReg::MEM_ADDR_CONFIG), blk.readL(BlkIoReg::CONFIG_STAT0));

    static const ULONG sizes[] = {0x800000, 0x400000, 0x200000, 0x100000};
    volatile UBYTE *fb         = bi->MemoryBase;
    for (unsigned i = 0; i < ARRAY_SIZE(sizes); ++i) {
        ULONG size = sizes[i];
        ULONG mid  = size >> 1;
        ULONG high = size - 2048;
        volatile ULONG *at0 = (volatile ULONG *)fb;
        volatile ULONG *atM = (volatile ULONG *)(fb + mid);
        volatile ULONG *atH = (volatile ULONG *)(fb + high);
        *at0                = 0;
        *atM                = mid | 1;
        *atH                = high | 1;
        CacheClearU();
        ULONG r0 = *at0;
        ULONG rM = *atM;
        ULONG rH = *atH;
        D(VERBOSE, "FB %ld: 0=%08lx mid=%08lx high=%08lx\n", size, r0, rM, rH);
        if (r0 == 0 && rM == (mid | 1) && rH == (high | 1)) {
            bi->MemorySize = size;
            if (bi->MemoryIOBase >= bi->MemoryBase && bi->MemoryIOBase < bi->MemoryBase + size)
                bi->MemorySize = (ULONG)(bi->MemoryIOBase - bi->MemoryBase) & ~63UL;
            D(INFO, "Framebuffer size %ld\n", bi->MemorySize);
            return TRUE;
        }
    }
    DFUNC(ERROR, "Framebuffer size probe failed\n");
    return FALSE;
}

static ULONG computeVCLKFrequencyKhz10_GM(const struct BoardInfo *bi, const struct PLLValue *pllValues)
{
    return computeFrequencyKhz10FromPllValue(bi, pllValues, g_VPLLPostDivider);
}

static void ASM SetClock_GM(__REGA0(struct BoardInfo *bi))
{
    asMach64(bi)->setClock_GT();
}

BOOL InitMach64GM(struct BoardInfo *bi)
{
    DFUNC(INFO, "ASIC %ld, continuing after option ROM\n", (ULONG)getAsicVersion(bi));
    DRIVER_LOCALS(bi);
    Mach64BlkIo blk = drv->blkIo();

    gmCaptureMemoryPll(bi);
    if (!getChipSpecific(bi)->referenceDivider || !getChipSpecific(bi)->referenceFrequency) {
        DFUNC(ERROR, "PLL reference missing\n");
        return FALSE;
    }
    if (!gmProbeFramebuffer(bi))
        return FALSE;

    blk.writeMaskL(BlkIoReg::BUS_CNTL,
                   BUS_MASTER_DIS_GT_MASK | BUS_PCI_RETRY_EN_GT_MASK | BUS_FIFO_WS_GT_MASK,
                   BUS_MASTER_DIS_GT | BUS_PCI_RETRY_EN_GT | BUS_FIFO_WS_GT(0xF));

    mmio.writeMaskL(static_cast<MmioReg::Id>(GUI_CNTL), CMDFIFO_SIZE_MODE_MASK, CMDFIFO_SIZE_MODE(0b00));
    waitFifo(bi, 4);
    mmio.writeL(static_cast<MmioReg::Id>(DP_SET_GUI_ENGINE), 0);
    mmio.writeL(static_cast<MmioReg::Id>(SCALE_3D_CNTL), 0xc0);
    delayMilliSeconds(3);
    mmio.writeL(static_cast<MmioReg::Id>(SETUP_CNTL), 0x00);
    delayMilliSeconds(3);
    mmio.writeL(static_cast<MmioReg::Id>(SCALE_3D_CNTL), 0x00);

    getChipSpecific(bi)->computeVCLKFrequency = computeVCLKFrequencyKhz10_GM;
    bi->SetClock                              = SetClock_GM;
    InitVClockPLLTable(bi, reinterpret_cast<const BYTE *>(g_VPLLPostDivider), ARRAY_SIZE(g_VPLLPostDivider));
    if (!bi->PixelClockCount[CHUNKY]) {
        DFUNC(ERROR, "VCLK table empty\n");
        return FALSE;
    }
    return TRUE;
}

#ifdef __cplusplus
}
#endif
