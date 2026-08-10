#include "mach64VT.h"
#include "chip_mach64.h"
#include "mach64_common.h"

using namespace MmioReg;
using namespace PllReg;

#ifdef __cplusplus
#ifndef _Static_assert
#define _Static_assert static_assert
#endif
extern "C" {
#endif

#define BUS_PCI_RETRY_EN_VT      BIT(15)
#define BUS_PCI_RETRY_EN_VT_MASK BIT(15)
#define BUS_FIFO_WS_VT(x)        ((x) << 16)
#define BUS_FIFO_WS_VT_MASK      (0xF << 16)

void AdjustDSP(struct BoardInfo *bi, UBYTE vclkFBDiv, UBYTE vclkPostDiv);

static const UBYTE g_VPLLPostDivider[] = {1, 2, 4, 8};

static const UBYTE g_VPLLPostDividerCodes[] = {
    // *1,    *2,   *4,   *8,
    0b00, 0b01, 0b10, 0b11};

static const UBYTE g_MPLLPostDividers[] = {1, 2, 4, 8};

static const UBYTE g_MPLLPostDividerCodes[] = {
    // *1,  *2,   *4,   *8
    0b000, 0b001, 0b010, 0b011};

#define VCLK_SRC_SEL(x)   ((x))
#define VCLK_SRC_SEL_MASK (0x3)
#define PLL_PRESET        BIT(2)
#define PLL_PRESET_MASK   BIT(2)
#define VCLK0_POST_MASK   (0x3)
#define VCLK0_POST(x)     (x)
#define DCLK_BY2_EN       BIT(7)
#define DCLK_BY2_EN_MASK  BIT(7)

static const UWORD defaultRegs_VT[] = {
    0x00a2, 0x6007,  // BUS_CNTL upper
    0x00a0, 0x20f8,  // BUS_CNTL lower
    0x0018, 0x0000,  // CRTC_INT_CNTL lower
    0x001c, 0x0200,  // CRTC_GEN_CNTL lower
    0x001e, 0x040b,  // CRTC_GEN_CNTL upper
    0x00d2, 0x0000,  // GEN_TEST_CNTL upper
    // 0x00e4, 0x0020,   // CONFIG_STAT0 lower — kills Expansion ROM (MEM_TYPE=DISABLE); set below
    0x00b0, 0x0021,   // MEM_CNTL lower
    0x00b2, 0x0801,   // MEM_CNTL upper
    0x00d0, 0x0000,   // GEN_TEST_CNTL lower
    0x001e, 0x0000,   // CRTC_GEN_CNTL upper
    0x0080, 0x0000,   // SCRATCH_REG0 lower
    0x0082, 0x0000,   // SCRATCH_REG0 upper
    0x0084, 0x0000,   // SCRATCH_REG1 lower
    0x0086, 0x0000,   // SCRATCH_REG1 upper
    0x00c4, 0x0000,   // DAC_CNTL lower
    0x00c6, 0x8000,   // DAC_CNTL upper
    0x007a, 0x0000,   // GP_IO lower
    0x00d0, 0x0100};  // GEN_TEST_CNTL lower

typedef struct
{
    ULONG reserved2 : 3;         // Bits 29-31: Reserved
    ULONG mem_oe_select : 2;     // Bits 27-28: OEb(0) Pin Function Select
    ULONG mem_pix_width : 3;     // Bits 24-26: Memory Pixel Width
    ULONG reserved : 1;          // Bit 23: Reserved
    ULONG cde_pullback : 1;      // Bit 22: CDE Pullback
    ULONG low_latency_mode : 1;  // Bit 21: Low Latency Mode
    ULONG mem_tile_select : 2;   // Bits 19-20: Memory Tile Select
    ULONG mem_sdram_reset : 1;   // Bit 18: SDRAM Reset
    ULONG dll_gain_cntl : 2;     // Bits 16-17: DLL Gain Control
    ULONG mem_actv_pre : 2;      // Bits 14-15: SDRAM Active to Precharge Minimum Latency
    ULONG dll_reset : 1;         // Bit 13: DLL Reset
    ULONG mem_refresh_rate : 2;  // Bits 11-12: Memory refresh rate
    ULONG mem_cyc_lnth : 2;      // Bits 9-10: DRAM non-page memory cycle length
    ULONG mem_cyc_lnth_aux : 2;  // Bits 7-8: SDRAM Actv to Cmd cycle length
    ULONG mem_refresh : 4;       // Bits 3-6: Refresh Cycle Length
    ULONG mem_size : 3;          // Bits 0-2: Memory size
} MEM_CNTL_t;

static void print_MEM_CNTL_Register(const MEM_CNTL_t *reg)
{
    D(0, "MEM_CNTL_Register:\n");
    D(0, "  mem_size         : 0x%lx\n", reg->mem_size);
    D(0, "  mem_refresh      : 0x%lx\n", reg->mem_refresh);
    D(0, "  mem_cyc_lnth_aux : 0x%lx\n", reg->mem_cyc_lnth_aux);
    D(0, "  mem_cyc_lnth     : 0x%lx\n", reg->mem_cyc_lnth);
    D(0, "  mem_refresh_rate : 0x%lx\n", reg->mem_refresh_rate);
    D(0, "  dll_reset        : 0x%lx\n", reg->dll_reset);
    D(0, "  mem_actv_pre     : 0x%lx\n", reg->mem_actv_pre);
    D(0, "  dll_gain_cntl    : 0x%lx\n", reg->dll_gain_cntl);
    D(0, "  mem_sdram_reset  : 0x%lx\n", reg->mem_sdram_reset);
    D(0, "  mem_tile_select  : 0x%lx\n", reg->mem_tile_select);
    D(0, "  low_latency_mode : 0x%lx\n", reg->low_latency_mode);
    D(0, "  cde_pullback     : 0x%lx\n", reg->cde_pullback);
    D(0, "  reserved         : 0x%lx\n", reg->reserved);
    D(0, "  mem_pix_width    : 0x%lx\n", reg->mem_pix_width);
    D(0, "  mem_oe_select    : 0x%lx\n", reg->mem_oe_select);
    D(0, "  reserved2        : 0x%lx\n", reg->reserved2);
}

static ULONG computeVCLKrequencyKhz10_VT(const struct BoardInfo *bi, const struct PLLValue *pllValues)
{
    return computeFrequencyKhz10FromPllValue(bi, pllValues, g_VPLLPostDivider);
}

/*
 * Display FIFO LWM for VT (MAGIC_FIFO / no GTB_DSP). Empirical values from
 * PSEUDO_EDO 2M @60MHz tables (InitMach64VT forces PSEUDO_EDO). fifo_val low
 * nibble = CRTC_FIFO_LWM; bit5 → CRTC_DISPREQ_ONLY (SDK CRTC_FIFO byte write).
 * Dot clocks are 10 kHz units (2518 = 25.18 MHz).
 */
typedef struct
{
    UWORD clock10k;
    UBYTE fifoVal;
} FifoEntry_t;

static const FifoEntry_t g_fifo8[] = {
    {4600, 0x04}, {5600, 0x06}, {7000, 0x06}, {9400, 0x08}, {10000, 0x0a}, {11300, 0x0c}, {14000, 0x0c}, {24000, 0x0e},
};
static const FifoEntry_t g_fifo16[] = {
    {2200, 0x06}, {3400, 0x06}, {4200, 0x0a}, {5500, 0x0a},  {6600, 0x0c},
    {7000, 0x0c}, {7600, 0x0e}, {7900, 0x0e}, {24000, 0x0e},
};
static const FifoEntry_t g_fifo24[] = {
    {2000, 0x08}, {3400, 0x0a}, {3800, 0x0a}, {4200, 0x0a}, {4500, 0x0c}, {5200, 0x28}, {24000, 0x2a},
};
static const FifoEntry_t g_fifo32[] = {
    {3000, 0x0a},
    {3500, 0x0e},
    {4200, 0x0e},
    {24000, 0x2a},
};

static UBYTE lookupFifoVal(const FifoEntry_t *tab, UBYTE n, ULONG clock10k)
{
    for (UBYTE i = 0; i < n; i++) {
        if (clock10k <= tab[i].clock10k)
            return tab[i].fifoVal;
    }
    return tab[n - 1].fifoVal;
}

void AdjustCrtcFifo_VT(struct BoardInfo *bi)
{
    const struct ModeInfo *mi = bi->ModeInfo;
    if (!mi)
        return;

    ULONG clock10k = mi->PixelClock / 10000;
    UBYTE fifoVal;

    switch (mi->Depth) {
    case 15:
    case 16:
        fifoVal = lookupFifoVal(g_fifo16, ARRAY_SIZE(g_fifo16), clock10k);
        break;
    case 24:
        fifoVal = lookupFifoVal(g_fifo24, ARRAY_SIZE(g_fifo24), clock10k);
        break;
    case 32:
        fifoVal = lookupFifoVal(g_fifo32, ARRAY_SIZE(g_fifo32), clock10k);
        break;
    default:
        fifoVal = lookupFifoVal(g_fifo8, ARRAY_SIZE(g_fifo8), clock10k);
        break;
    }

    UBYTE lwm      = fifoVal & 0x0F;
    ULONG extra    = (fifoVal & 0x20) ? CRTC_DISPREQ_ONLY_VT : 0;
    UBYTE overfill = (fifoVal & 0x20) ? 2 : 1;

    DFUNC(VERBOSE, "clk %ld0kHz depth %ld → fifo=0x%02lx LWM=%ld overfill=%ld dispreq=%ld\n", clock10k,
          (ULONG)mi->Depth, (ULONG)fifoVal, (ULONG)lwm, (ULONG)overfill, !!(extra));

    DRIVER_LOCALS(bi);
    mmio.writeMaskL(CRTC_GEN_CNTL,
                    CRTC_FIFO_LWM_MASK | CRTC_FIFO_OVERFILL_VT_MASK | CRTC_DISPREQ_ONLY_VT_MASK | CRTC_LOCK_REGS_MASK,
                    CRTC_FIFO_LWM(lwm) | CRTC_FIFO_OVERFILL_VT(overfill) | extra);
}

void ASM Mach64Driver::setClock_VT()
{
    DFUNC(VERBOSE, "\n");
    DRIVER_LOCALS(this);

    struct ModeInfo *mi = ModeInfo;
    ULONG pixelClock    = mi->PixelClock;

#ifdef DBG
    const ChipSpecific_t *cs = getConstChipSpecific(this);
    ULONG minVClkKhz10       = 2 * cs->referenceFrequency * 128 / (cs->referenceDivider * 8);

    D(CHATTY, "minimm VCLK %ldHz\n", minVClkKhz10 * 10000);
    if (pixelClock < minVClkKhz10 * 10000) {
        DFUNC(ERROR, "PixelClock %ldHz is too low, minimum is %ldHz\n", pixelClock, minVClkKhz10 * 10000);
        return;
    }

    D(CHATTY, "SetClock: PixelClock %ldHz -> %ldHz \n", ModeInfo->PixelClock, pixelClock);
#endif

    // Temporarily select CPUCLK as VCLK source, bring VPLL into reset
    WRITE_PLL_MASK(PLL_VCLK_CNTL, VCLK_SRC_SEL_MASK, VCLK_SRC_SEL(0b00));
    delayMilliSeconds(5);
    WRITE_PLL_MASK(PLL_VCLK_CNTL, PLL_PRESET_MASK, PLL_PRESET);

    WRITE_PLL(PLL_VCLK0_FB_DIV, mi->pll1.Numerator);
    BYTE postDivCode = g_VPLLPostDividerCodes[mi->pll2.Denominator];
    WRITE_PLL_MASK(PLL_VCLK_POST_DIV, VCLK0_POST_MASK, VCLK0_POST(postDivCode));

    /* VT A3/A4: CRTC FIFO LWM. VT3+ (asic rev ≥ 1): GTB DSP as well. */
    AdjustCrtcFifo_VT(this);
    if (getAsicVersion(this) >= 1)
        AdjustDSP(this, mi->pll1.Numerator, g_VPLLPostDivider[mi->pll2.Denominator]);

    WRITE_PLL_MASK(PLL_VCLK_CNTL, PLL_PRESET_MASK, 0);
    delayMilliSeconds(5);
    WRITE_PLL_MASK(PLL_VCLK_CNTL, VCLK_SRC_SEL_MASK, VCLK_SRC_SEL(0b11));

    delayMilliSeconds(5);
}

static void ASM SetClock_VT(__REGA0(struct BoardInfo *bi))
{
    asMach64(bi)->setClock_VT();
}

static BOOL probeMemorySize(BoardInfo_t *bi)
{
    DFUNC(VERBOSE, "\n");

    Mach64BlkIo blk = asMach64(bi)->blkIo();
    LOCAL_SYSBASE();

    static const ULONG memorySizes[] = {0x400000, 0x200000, 0x100000, 0x80000};
    static const ULONG memoryCodes[] = {3, 2, 1, 0};

    volatile ULONG *framebuffer = (volatile ULONG *)bi->MemoryBase;
    framebuffer[0]              = 0;

    for (int i = 0; i < ARRAY_SIZE(memorySizes); i++) {
        bi->MemorySize = memorySizes[i];
        D(VERBOSE, "\nProbing memory size %ld\n", bi->MemorySize);

        blk.writeMaskL(BlkIoReg::MEM_CNTL, 0x7, memoryCodes[i]);

        flushWrites();

        CacheClearU();

        // Probe the last and the first longword for the current segment,
        // as well as offset 0 to check for wrap arounds
        volatile ULONG *highOffset = framebuffer + (bi->MemorySize >> 2) - 512 - 1;
        volatile ULONG *lowOffset  = framebuffer + (bi->MemorySize >> 3);
        // Probe  memory
        *framebuffer = 0;
        *highOffset  = (ULONG)highOffset;
        *lowOffset   = (ULONG)lowOffset;

        CacheClearU();

        ULONG readbackHigh = *highOffset;
        ULONG readbackLow  = *lowOffset;
        ULONG readbackZero = *framebuffer;

        D(VERBOSE, "Probing memory at 0x%lx ?= 0x%lx; 0x%lx ?= 0x%lx, 0x0 ?= 0x%lx\n", highOffset, readbackHigh,
          lowOffset, readbackLow, readbackZero);

        if (readbackHigh == (ULONG)highOffset && readbackLow == (ULONG)lowOffset && readbackZero == 0) {
            D(VERBOSE, "Memory size sucessfully probed.\n\n");
            return TRUE;
        }
    }
    D(VERBOSE, "Memory size probe failed.\n\n");
    return FALSE;
}

void SetMemoryClock_VT(BoardInfo_t *bi, UWORD freqKhz10)
{
    DFUNC(VERBOSE, "Setting Memory Clock to %ld0 KHz\n", (ULONG)freqKhz10);
    DRIVER_LOCALS(bi);

    if (freqKhz10 < 1475) {
        freqKhz10 = 1475;
    }
    // SGRAM is rated up to 100Mhz, Core engine up to 83Mhz
    if (freqKhz10 > 6800) {
        freqKhz10 = 6800;
    }
    PLLValue_t pllValues;
    if (!computePLLValues(bi, freqKhz10, g_MPLLPostDividers, ARRAY_SIZE(g_MPLLPostDividers), &pllValues)) {
        DFUNC(ERROR, "Failed to compute PLL values\n");
        return;
    }

    Mach64BlkIo blk = drv->blkIo();

    // Reset GUI Controller
    blk.writeMaskL(BlkIoReg::GEN_TEST_CNTL, GEN_GUI_RESETB_MASK, GEN_GUI_RESETB);
    blk.writeMaskL(BlkIoReg::GEN_TEST_CNTL, GEN_GUI_RESETB_MASK, 0);

    // Clock MCLK from CPUCLK
    WRITE_PLL_MASK(PLL_GEN_CNTL, MCLK_SRC_SEL_MASK, MCLK_SRC_SEL(0b100));

    WRITE_PLL(PLL_MCLK_FB_DIV, pllValues.N);
    delayMilliSeconds(5);
    WRITE_PLL_MASK(PLL_GEN_CNTL, MCLK_SRC_SEL_MASK, MCLK_SRC_SEL(g_MPLLPostDividerCodes[pllValues.Pidx]));

    ChipSpecific_t *cs = getChipSpecific(bi);
    cs->mclkFBDiv      = pllValues.N;
    cs->mclkPostDiv    = g_MPLLPostDividers[pllValues.Pidx];

    return;
}

BOOL InitMach64VT(struct BoardInfo *bi)
{
    DFUNC(INFO, "\n");
    DRIVER_LOCALS(bi);
    Mach64BlkIo blk = drv->blkIo();

    WRITE_PLL(PLL_MACRO_CNTL, 0xa0);  // PLL_DUTY_CYC = 5

    // VFC_CNTL seem to exist only for VT-A4
    // WRITE_PLL(PLL_VFC_CNTL, 0x1b);
    // WRITE_PLL(PLL_VFC_CNTL, 0x03);

    ChipSpecific_t *cs = getChipSpecific(bi);

    cs->referenceDivider = 36;

    WRITE_PLL(PLL_REF_DIV, cs->referenceDivider);
    WRITE_PLL(PLL_VCLK_CNTL, 0x00);

    // FCP_REF = CPUCLK
    // WRITE_PLL(PLL_FCP_CNTL, 0xc0);
    WRITE_PLL(PLL_FCP_CNTL, 0x40);

    WRITE_PLL(PLL_XCLK_CNTL, 0x00);
    WRITE_PLL(PLL_VCLK_POST_DIV, 0x9c);

    // VCLK = PLLVCLK/(VCLK Post Divider; VCLK_INVERT enabled
    WRITE_PLL(PLL_VCLK_CNTL, 0x0b);
    WRITE_PLL(PLL_MACRO_CNTL, 0x90);

    WRITE_PLL(PLL_GEN_CNTL, OSC_EN | MCLK_SRC_SEL(0b100));

    WriteDefaultRegList(bi, defaultRegs_VT, ARRAY_SIZE(defaultRegs_VT));

    // These values are being set by the Mach64VT BIOS, but are inline with the code, not somewhere in an accessible
    // table. CONFIG_STAT0 is apparently not strapped to the right memory type and only by setting it here we get access
    // to the framebuffer memory.
    blk.writeMaskL(BlkIoReg::CONFIG_STAT0, CFG_MEM_TYPE_CT_MASK | CFG_DUAL_CAS_EN_CT_MASK | CFG_CLOCK_EN_CT_MASK,
                   CFG_MEM_TYPE_CT(CFG_MEM_TYPE_VT_PSEUDO_EDO) | CFG_DUAL_CAS_EN_CT | CFG_CLOCK_EN_CT);

    // blk.writeMaskB(BlkIoReg::CONFIG_STAT0, 0, ~0xf8, 0x3b);

    blk.writeMaskL(BlkIoReg::BUS_CNTL, BUS_ROM_DIS_MASK | BUS_PCI_RETRY_EN_VT_MASK | BUS_FIFO_WS_VT_MASK,
                   BUS_PCI_RETRY_EN_VT | BUS_FIFO_WS_VT(0xF));

    cs->computeVCLKFrequency = computeVCLKrequencyKhz10_VT;
    bi->SetClock             = SetClock_VT;

    InitVClockPLLTable(bi, reinterpret_cast<const BYTE *>(g_VPLLPostDivider), ARRAY_SIZE(g_VPLLPostDivider));

    ULONG memCntlRaw   = blk.readL(BlkIoReg::MEM_CNTL);
    MEM_CNTL_t memCntl = *reinterpret_cast<const MEM_CNTL_t *>(&memCntlRaw);
    print_MEM_CNTL_Register(&memCntl);

    blk.writeMaskL(BlkIoReg::CRTC_GEN_CNTL,
                   CRTC_ENABLE_MASK | CRTC_EXT_DISP_EN_MASK | CRTC_DISP_REQ_ENB_MASK | VGA_XCRT_CNT_EN_MASK |
                       CRTC_DISPLAY_DIS_MASK | VGA_ATI_LINEAR_MASK | CRTC_LOCK_REGS_MASK,
                   CRTC_ENABLE | CRTC_EXT_DISP_EN | VGA_XCRT_CNT_EN);

    SetMemoryClock(bi, resolveMemoryClockKhz10(bi));

    if (!probeMemorySize(bi))
        return FALSE;

    return TRUE;
}

#ifdef __cplusplus
}
#endif
