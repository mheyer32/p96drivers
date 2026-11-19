#include "mach64GM.h"
#include "mach64_chip.h"
#include "mach64GT.h"
#include "mach64_common.h"

using namespace MmioReg;
using namespace PllReg;

/* PLL_EXT_CNTL bit3: feedback path is ×4. Fold into N so 2*R*N/(M*P) matches. */
#define GM_PLL_MFB_TIMES_4 0x08

#define BUS_MASTER_DIS_GT        BIT(6)
#define BUS_MASTER_DIS_GT_MASK   BIT(6)
#define BUS_PCI_RETRY_EN_GT      BIT(15)
#define BUS_PCI_RETRY_EN_GT_MASK BIT(15)
#define BUS_FIFO_WS_GT(x)        ((x) << 16)
#define BUS_FIFO_WS_GT_MASK      (0xF << 16)

static const UBYTE gmXclkPostDiv[] = {1, 2, 4, 8, 3};

static void gmCaptureMemoryPll(Mach64Driver &bi)
{
    DRIVER_LOCALS(bi);
    ChipSpecific &cs = getChipSpecific(bi);

    UBYTE liveM = READ_PLL(PLL_REF_DIV);
    UBYTE ext   = READ_PLL(PLL_EXT_CNTL);
    UBYTE n     = READ_PLL(PLL_MCLK_FB_DIV);
    UBYTE code  = ext & 7;
    UBYTE post  = code < ARRAY_SIZE(gmXclkPostDiv) ? gmXclkPostDiv[code] : 1;
    UWORD fb    = n;

    if (liveM && liveM != cs.referenceDivider) {
        D(INFO, "PLL_REF_DIV live %ld, ROM %ld\n", (ULONG)liveM, (ULONG)cs.referenceDivider);
        cs.referenceDivider = liveM;
    }
    if (ext & GM_PLL_MFB_TIMES_4)
        fb = (UWORD)n << 1;
    if (code >= ARRAY_SIZE(gmXclkPostDiv))
        D(WARN, "PLL_EXT_CNTL XCLK source %ld\n", (ULONG)code);

    cs.mclkFBDiv   = fb;
    cs.mclkPostDiv = post;

    if (cs.referenceFrequency && cs.referenceDivider && post) {
        ULONG khz10     = computeFrequencyKhz10(cs.referenceFrequency, fb, cs.referenceDivider, post);
        bi.MemoryClock = khz10 * 10000UL;
    }
    D(INFO, "BIOS MCLK N=%ld post=%ld EXT=%02lx GEN=%02lx VCLK=%02lx MemoryClock %ld Hz\n", (ULONG)fb, (ULONG)post,
      (ULONG)ext, (ULONG)READ_PLL(PLL_GEN_CNTL), (ULONG)READ_PLL(PLL_VCLK_CNTL), bi.MemoryClock);
}

/* BIOS already programmed MEM_CNTL / the memory PLL. Find the size that does not alias. */
static BOOL gmProbeFramebuffer(Mach64Driver &bi)
{
    LOCAL_SYSBASE(bi);
    Mach64BlkIo blk = asMach64(bi)->blkIo();

    D(INFO, "BIOS MEM_CNTL=0x%08lx MEM_ADDR_CONFIG=0x%08lx CONFIG_STAT0=0x%08lx\n", blk.readL(BlkIoReg::MEM_CNTL),
      blk.readL(BlkIoReg::MEM_ADDR_CONFIG), blk.readL(BlkIoReg::CONFIG_STAT0));

    static const ULONG sizes[] = {0x800000, 0x400000, 0x200000, 0x100000};
    volatile UBYTE *fb         = bi.MemoryBase;
    for (unsigned i = 0; i < ARRAY_SIZE(sizes); ++i) {
        ULONG size              = sizes[i];
        ULONG mid               = size >> 1;
        ULONG high              = size - 2048;
        volatile ULONG *at0     = (volatile ULONG *)fb;
        volatile ULONG *atM     = (volatile ULONG *)(fb + mid);
        volatile ULONG *atH     = (volatile ULONG *)(fb + high);
        *at0                    = 0;
        *atM                    = mid | 1;
        *atH                    = high | 1;
        CacheClearU();
        ULONG r0 = *at0;
        ULONG rM = *atM;
        ULONG rH = *atH;
        D(VERBOSE, "FB %ld: 0=%08lx mid=%08lx high=%08lx\n", size, r0, rM, rH);
        if (r0 == 0 && rM == (mid | 1) && rH == (high | 1)) {
            bi.MemorySize = size;
            if (bi.MemoryIOBase >= bi.MemoryBase && bi.MemoryIOBase < bi.MemoryBase + size)
                bi.MemorySize = (ULONG)(bi.MemoryIOBase - bi.MemoryBase) & ~63UL;
            D(INFO, "Framebuffer size %ld\n", bi.MemorySize);
            return TRUE;
        }
    }
    DFUNC(ERROR, "Framebuffer size probe failed\n");
    return FALSE;
}

static ULONG computeVCLKFrequencyKhz10_GM(const Mach64Driver &bi, const struct PLLValue *pllValues)
{
    return computeFrequencyKhz10FromPllValue(bi, pllValues, g_VPLLPostDivider);
}

BOOL InitMach64GM(Mach64Driver &bi)
{
    DFUNC(INFO, "ASIC %ld, continuing after option ROM\n", (ULONG)getAsicVersion(bi));
    DRIVER_LOCALS(bi);
    Mach64BlkIo blk = drv->blkIo();

    gmCaptureMemoryPll(bi);
    if (!getChipSpecific(bi).referenceDivider || !getChipSpecific(bi).referenceFrequency) {
        DFUNC(ERROR, "PLL reference missing\n");
        return FALSE;
    }
    if (!gmProbeFramebuffer(bi))
        return FALSE;

    blk.writeMaskL(BlkIoReg::BUS_CNTL, BUS_MASTER_DIS_GT_MASK | BUS_PCI_RETRY_EN_GT_MASK | BUS_FIFO_WS_GT_MASK,
                   BUS_MASTER_DIS_GT | BUS_PCI_RETRY_EN_GT | BUS_FIFO_WS_GT(0xF));

    mmio.writeMaskL(static_cast<MmioReg::Id>(GUI_CNTL), CMDFIFO_SIZE_MODE_MASK, CMDFIFO_SIZE_MODE(0b00));
    waitFifo(bi, 4);
    mmio.writeL(static_cast<MmioReg::Id>(DP_SET_GUI_ENGINE), 0);
    mmio.writeL(static_cast<MmioReg::Id>(SCALE_3D_CNTL), 0xc0);
    delayMilliSeconds(3);
    mmio.writeL(static_cast<MmioReg::Id>(SETUP_CNTL), 0x00);
    delayMilliSeconds(3);
    mmio.writeL(static_cast<MmioReg::Id>(SCALE_3D_CNTL), 0x00);

    getChipSpecific(bi).computeVCLKFrequency = computeVCLKFrequencyKhz10_GM;
    bi.SetClock                              = SetClock_GT;
    InitVClockPLLTable(bi, reinterpret_cast<const BYTE *>(g_VPLLPostDivider), ARRAY_SIZE(g_VPLLPostDivider));
    if (!bi.PixelClockCount[CHUNKY]) {
        DFUNC(ERROR, "VCLK table empty\n");
        return FALSE;
    }
    return TRUE;
}
