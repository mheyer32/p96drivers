#include "mach64_vbios.h"
#include "mach64_chip.h"

#include <exec/memory.h>
#include <exec/tasks.h>
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/openpci.h>

struct Library *PCIInitBase;
#include <proto/pciinit.h>

using namespace BlkIoReg;

/* POST runs on the caller's stack. Picasso96's stack is far smaller than the ROM needs. */
#define VBIOS_STACK (128UL * 1024UL)

static const char *piStatus(uint32_t status)
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

static BOOL dropClaim(Mach64Driver &drv, struct Node **saved)
{
    LOCAL_OPENPCIBASE(drv);
    struct Node *owner = NULL;
    *saved             = NULL;
    GetBoardAttrs(getCardData(&drv)->board, PRM_BoardOwner, (Tag)&owner, TAG_END);
    if (!owner)
        return TRUE;
#ifndef TESTEXE
    if (owner != &getCardData(&drv)->boardNode)
        return TRUE;
#endif
    if (!SetBoardAttrs(getCardData(&drv)->board, PRM_BoardOwner, (Tag)NULL, TAG_END)) {
        DFUNC(ERROR, "Could not drop board claim for option ROM\n");
        return FALSE;
    }
    *saved = owner;
    return TRUE;
}

static void restoreClaim(Mach64Driver &drv, struct Node *saved)
{
    if (!saved)
        return;
    LOCAL_OPENPCIBASE(drv);
    SetBoardAttrs(getCardData(&drv)->board, PRM_BoardOwner, (Tag)saved, TAG_END);
}

struct VbiosJob
{
    PCIInitContext *ctx;
    const PCIInitImage *image;
    uint32_t status;
};

static struct VbiosJob vbiosJob;
static struct StackSwapStruct vbiosStack;
static struct ExecBase *vbiosSysBase;

/* StackSwap exchanges SP. Every value this function touches lives in static storage. */
static __attribute__((noinline)) void runRom(void)
{
    static struct ExecBase *SysBase;
    SysBase = vbiosSysBase;
    StackSwap(&vbiosStack);
    asm volatile("" ::: "memory");
    vbiosJob.status = RunOptionROM(vbiosJob.ctx, vbiosJob.image);
    asm volatile("" ::: "memory");
    SysBase = vbiosSysBase;
    StackSwap(&vbiosStack);
}

/* Frequency table: ref_clock_freq is in 10 kHz units (1432 → 14.32 MHz). */
static void printRefClock(Mach64Driver &drv, const PCIInitImage *image)
{
    struct PCIInitROMInfo info;
    const UBYTE *rom;
    UWORD atiOff;
    UWORD freqOff;
    const Mach64RomHeader *hdr;
    const FrequencyTable *ft;
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
    if (atiOff < 2 || (ULONG)atiOff - 2 + sizeof(Mach64RomHeader) > info.source_bytes) {
        D(WARN, "ROM ref clock: bad ATI header ptr 0x%lx\n", (ULONG)atiOff);
        return;
    }

    hdr     = (const Mach64RomHeader *)(rom + atiOff - 2);
    freqOff = swapw(hdr->freq_table_ptr);
    if (!freqOff || (ULONG)freqOff + sizeof(FrequencyTable) > info.source_bytes) {
        D(WARN, "ROM ref clock: bad frequency table ptr 0x%lx\n", (ULONG)freqOff);
        return;
    }

    ft      = (const FrequencyTable *)(rom + freqOff);
    refFreq = swapw(ft->ref_clock_freq);
    refDiv  = swapw(ft->ref_clock_divider);

    if (drv.MemoryBase)
        liveM = ReadPLL(drv, PllReg::PLL_REF_DIV);

    D(INFO, "ROM reference clock %ld.%02ld MHz (table %ld), divider %ld, PLL_REF_DIV %ld\n", (ULONG)(refFreq / 100),
      (ULONG)(refFreq % 100), (ULONG)refFreq, (ULONG)refDiv, (ULONG)liveM);
}

BOOL RunVBIOS(Mach64Driver &drv)
{
    LOCAL_SYSBASE(drv);
    BOOL ok               = FALSE;
    uint32_t status       = PI_OK;
    struct Node *claim    = NULL;
    PCIInitImage *image   = NULL;
    PCIInitContext *ctx   = NULL;
    APTR stack            = NULL;
    struct pci_dev *board = getCardData(&drv)->board;
    /* LoadOptionROMFromCard rejects PII_Hardware; CreatePCIContext needs it. */
    struct TagItem loadTags[] = {{PII_Error, (ULONG)&status}, {TAG_DONE, 0}};
    struct TagItem runTags[]  = {{PII_Error, (ULONG)&status}, {PII_Hardware, 1}, {TAG_DONE, 0}};

    PCIInitBase = OpenLibrary((STRPTR) "pciinit.library", PCIINIT_LIBRARY_VERSION);
    if (!PCIInitBase) {
        DFUNC(ERROR, "Cannot open pciinit.library v%ld\n", (ULONG)PCIINIT_LIBRARY_VERSION);
        return FALSE;
    }

    {
        LOCAL_OPENPCIBASE(drv);
        UWORD cmd = pci_read_config_word(PCI_COMMAND, board);
        pci_write_config_word(PCI_COMMAND, cmd | PCI_COMMAND_MEMORY | PCI_COMMAND_IO, board);
    }
    /* Warm reinit leaves VGA decode and the expansion ROM disabled.
     * Block I/O, so this does not depend on the linear aperture. */
    if (drv.RegisterBase) {
        drv.writeMaskConfigCntl(CFG_VGA_DIS_MASK, 0);
        drv.blkIo().writeMaskL(BUS_CNTL, BUS_ROM_DIS_MASK, 0);
    }

    if (!dropClaim(drv, &claim))
        goto out;

    image = LoadOptionROMFromCard(board, loadTags);
    if (!image) {
        DFUNC(ERROR, "LoadOptionROMFromCard: %s\n", piStatus(status));
        goto out;
    }

    ctx = CreatePCIContext(board, runTags);
    if (!ctx) {
        DFUNC(ERROR, "CreatePCIContext: %s\n", piStatus(status));
        goto out;
    }

    stack = AllocMem(VBIOS_STACK, MEMF_PUBLIC | MEMF_FAST);
    if (!stack)
        stack = AllocMem(VBIOS_STACK, MEMF_PUBLIC);
    if (!stack) {
        DFUNC(ERROR, "Option ROM stack\n");
        goto out;
    }
    vbiosStack.stk_Lower   = stack;
    vbiosStack.stk_Upper   = (ULONG)stack + VBIOS_STACK;
    vbiosStack.stk_Pointer = (APTR)vbiosStack.stk_Upper;
    vbiosJob.ctx           = ctx;
    vbiosJob.image         = image;
    vbiosJob.status        = PI_EXECUTION_FAILED;
    vbiosSysBase           = (struct ExecBase *)SysBase;

    runRom();
    status = vbiosJob.status;
    ok     = status == PI_OK;
    if (!ok)
        DFUNC(ERROR, "Option ROM POST failed: %s\n", piStatus(status));
    printRefClock(drv, image);

out:
    if (ctx)
        DeletePCIContext(ctx);
    if (image)
        FreeOptionROM(image);
    if (stack)
        FreeMem(stack, VBIOS_STACK);
    restoreClaim(drv, claim);
    CloseLibrary(PCIInitBase);
    PCIInitBase = NULL;
    return ok;
}
