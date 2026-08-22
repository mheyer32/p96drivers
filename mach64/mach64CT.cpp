#include "mach64CT.h"
#include "chip_mach64.h"
#include "mach64_common.h"

using namespace MmioReg;
using namespace PllReg;

#ifdef __cplusplus
extern "C" {
#endif

#define VCLK_SRC_SEL(x)   ((x))
#define VCLK_SRC_SEL_MASK (0x3)
#define PLL_PRESET        BIT(2)
#define PLL_PRESET_MASK   BIT(2)
#define VCLK0_POST_MASK   (0x3)
#define VCLK0_POST(x)     (x)

/* PLL_VCLK_CNTL bit 3 — divert to external clock (Appendix J); must be clear for BedRock. */
#define PLL_EXT_CLK_EN      BIT(3)
#define PLL_EXT_CLK_EN_MASK BIT(3)

static const UBYTE g_VPLLPostDivider[] = {1, 2, 4, 8};

static const UBYTE g_VPLLPostDividerCodes[] = {
    // *1,    *2,   *4,   *8,
    0b00, 0b01, 0b10, 0b11};

/* MCLK post-div in MCLK_SRC_SEL[6:4] (Appendix J / VT). 0b100 = CPUCLK while programming. */
static const UBYTE g_MPLLPostDividers[]     = {1, 2, 4, 8};
static const UBYTE g_MPLLPostDividerCodes[] = {0b000, 0b001, 0b010, 0b011};

void SetMemoryClock_CT(BoardInfo_t *bi, UWORD freqKhz10)
{
    DFUNC(VERBOSE, "Setting Memory Clock to %ld0 KHz\n", (ULONG)freqKhz10);
    DRIVER_LOCALS(bi);

    if (freqKhz10 < 1475)
        freqKhz10 = 1475;
    /* CT DRAM MCLK limit (Appendix J); ROM DRAM entry is ~50.5 MHz. */
    if (freqKhz10 > 6800)
        freqKhz10 = 6800;

    PLLValue_t pllValues;
    if (!computePLLValues(bi, freqKhz10, g_MPLLPostDividers, ARRAY_SIZE(g_MPLLPostDividers), &pllValues)) {
        DFUNC(ERROR, "Failed to compute MCLK PLL values\n");
        return;
    }

    ResetEngine(bi);

    /* Appendix J §J.4: hold MCLK on CPUCLK, program FB, lock, switch to PLLMCLK/P, drop EXT_CLK_EN. */
    WRITE_PLL_MASK(PLL_GEN_CNTL,
                   PLL_OVERRIDE_MASK | PLL_MRESET_MASK | OSC_EN_MASK | MCLK_SRC_SEL_MASK | PLL_EXT_CLK_EN_MASK,
                   OSC_EN | MCLK_SRC_SEL(0b100) | PLL_EXT_CLK_EN);
    WRITE_PLL(PLL_MCLK_FB_DIV, pllValues.N);
    delayMilliSeconds(5);
    WRITE_PLL_MASK(PLL_GEN_CNTL, MCLK_SRC_SEL_MASK | PLL_EXT_CLK_EN_MASK,
                   MCLK_SRC_SEL(g_MPLLPostDividerCodes[pllValues.Pidx]));
    delayMilliSeconds(1);

    ChipSpecific_t *cs = getChipSpecific(bi);
    cs->mclkFBDiv      = pllValues.N;
    cs->mclkPostDiv    = g_MPLLPostDividers[pllValues.Pidx];

    D(INFO, "MCLK N=%ld P=%ld PLL_GEN_CNTL=0x%02lx PLL_MCLK_FB_DIV=0x%02lx\n", (ULONG)pllValues.N,
      (ULONG)cs->mclkPostDiv, (ULONG)READ_PLL(PLL_GEN_CNTL), (ULONG)READ_PLL(PLL_MCLK_FB_DIV));
}

static ULONG computeVCLKFrequencyKhz10_CT(const struct BoardInfo *bi, const struct PLLValue *pllValues)
{
    return computeFrequencyKhz10FromPllValue(bi, pllValues, g_VPLLPostDivider);
}

/*
 * Display FIFO LWM for CT DRAM (RRG: higher LWM → more page faults → slower GE).
 * Reuse depth/clock breakpoints (same as VT MAGIC_FIFO); only program
 * CRTC_FIFO_LWM — do not touch CT bit21 (EXTRA_FIFO_READ) or OVERFILL.
 */
typedef struct
{
    UWORD clock10k;
    UBYTE lwm;
} FifoEntry_CT_t;

static const FifoEntry_CT_t g_fifo8_CT[] = {
    {4600, 0x4}, {5600, 0x6}, {7000, 0x6}, {9400, 0x8}, {10000, 0xa}, {11300, 0xc}, {14000, 0xc}, {24000, 0xe},
};
static const FifoEntry_CT_t g_fifo16_CT[] = {
    {2200, 0x6}, {3400, 0x6}, {4200, 0xa}, {5500, 0xa},  {6600, 0xc},
    {7000, 0xc}, {7600, 0xe}, {7900, 0xe}, {24000, 0xe},
};
static const FifoEntry_CT_t g_fifo24_CT[] = {
    {2000, 0x8}, {3400, 0xa}, {3800, 0xa}, {4200, 0xa}, {4500, 0xc}, {5200, 0xc}, {24000, 0xe},
};
static const FifoEntry_CT_t g_fifo32_CT[] = {
    {3000, 0xa},
    {3500, 0xe},
    {4200, 0xe},
    {24000, 0xe},
};

static UBYTE lookupFifoLwm_CT(const FifoEntry_CT_t *tab, UBYTE n, ULONG clock10k)
{
    for (UBYTE i = 0; i < n; i++) {
        if (clock10k <= tab[i].clock10k)
            return tab[i].lwm;
    }
    return tab[n - 1].lwm;
}

void AdjustCrtcFifo_CT(struct BoardInfo *bi)
{
    const struct ModeInfo *mi = bi->ModeInfo;
    if (!mi)
        return;

    ULONG clock10k = mi->PixelClock / 10000;
    UBYTE lwm;

    switch (mi->Depth) {
    case 15:
    case 16:
        lwm = lookupFifoLwm_CT(g_fifo16_CT, ARRAY_SIZE(g_fifo16_CT), clock10k);
        break;
    case 24:
        lwm = lookupFifoLwm_CT(g_fifo24_CT, ARRAY_SIZE(g_fifo24_CT), clock10k);
        break;
    case 32:
        lwm = lookupFifoLwm_CT(g_fifo32_CT, ARRAY_SIZE(g_fifo32_CT), clock10k);
        break;
    default:
        lwm = lookupFifoLwm_CT(g_fifo8_CT, ARRAY_SIZE(g_fifo8_CT), clock10k);
        break;
    }

    DFUNC(INFO, "clk %ld0kHz depth %ld → LWM=%ld (was max 15)\n", clock10k, (ULONG)mi->Depth, (ULONG)lwm);

    DRIVER_LOCALS(bi);
    mmio.writeMaskL(CRTC_GEN_CNTL, CRTC_FIFO_LWM_MASK, CRTC_FIFO_LWM(lwm));
}

void ASM Mach64Driver::setClock_CT()
{
    DFUNC(VERBOSE, "\n");
    DRIVER_LOCALS(this);

    struct ModeInfo *mi = ModeInfo;

#ifdef DBG
    const ChipSpecific_t *cs = getConstChipSpecific(drv);
    ULONG minVClkKhz10       = 2 * cs->referenceFrequency * 128 / (cs->referenceDivider * 8);

    D(CHATTY, "minimum VCLK %ldHz\n", minVClkKhz10 * 10000);
    if (mi->PixelClock < minVClkKhz10 * 10000) {
        DFUNC(ERROR, "PixelClock %ldHz is too low, minimum is %ldHz\n", mi->PixelClock, minVClkKhz10 * 10000);
        return;
    }
    D(INFO, "SetClock_CT: %ld Hz, N=%ld Pidx=%ld\n", mi->PixelClock, (ULONG)mi->pll1.Numerator,
      (ULONG)mi->pll2.Denominator);
#endif

    /* Select VCLK0 before programming. */
    mmio.writeMaskL(CLOCK_CNTL, CLOCK_SEL_MASK | CLOCK_STROBE_MASK, CLOCK_SEL(0) | CLOCK_STROBE);

    /* BedRock VPLL: CPUCLK → reset → program N/P → unreset → PLLVCLK. */
    WRITE_PLL_MASK(PLL_VCLK_CNTL, VCLK_SRC_SEL_MASK | PLL_EXT_CLK_EN_MASK, VCLK_SRC_SEL(0b00));
    delayMilliSeconds(5);
    WRITE_PLL_MASK(PLL_VCLK_CNTL, PLL_PRESET_MASK, PLL_PRESET);

    WRITE_PLL(PLL_VCLK0_FB_DIV, mi->pll1.Numerator);
    BYTE postDivCode = g_VPLLPostDividerCodes[mi->pll2.Denominator];
    WRITE_PLL_MASK(PLL_VCLK_POST_DIV, VCLK0_POST_MASK, VCLK0_POST(postDivCode));

    WRITE_PLL_MASK(PLL_VCLK_CNTL, PLL_PRESET_MASK, 0);
    delayMilliSeconds(5);
    WRITE_PLL_MASK(PLL_VCLK_CNTL, VCLK_SRC_SEL_MASK | PLL_EXT_CLK_EN_MASK, VCLK_SRC_SEL(0b11));

    delayMilliSeconds(5);

    /* Re-select / strobe VCLK0 into the CRTC (CLOCK_CNTL|STROBE). */
    mmio.writeMaskL(CLOCK_CNTL, CLOCK_SEL_MASK | CLOCK_STROBE_MASK, CLOCK_SEL(0) | CLOCK_STROBE);

    /* Lower display FIFO LWM → fewer DRAM page faults → more GE bandwidth (RRG). */
    AdjustCrtcFifo_CT(this);
}

static void ASM SetClock_CT(__REGA0(struct BoardInfo *bi))
{
    asMach64(bi)->setClock_CT();
}

/*
 * CT MEM_CNTL (RRG-S00700-05 §3-67/3-68 letters p/q/r) — DRAM, not GX VRAM latches.
 * MEM_SIZE (p) bits 0–2: 0=reserved, 1=1M, 2=2M, 3=4M. Never program GX latch/bndry bits.
 *
 * Cold VBIOS table @7953 programs MEM_CNTL=0x04F1 (1M + CT timing). Strap often 0x1800
 * (size reserved, wrong mid/cyc). Size-detect RMWs only bits 0–2.
 */
#define MEM_SIZE_CT_MASK 0x7u
#define MEM_SIZE_CT_1M   1
#define MEM_SIZE_CT_2M   2
#define MEM_SIZE_CT_4M   3
#define MEM_CNTL_CT_COLD 0x04F1u
/* FUN_7934: CONFIG_STAT0 = (read & 0xF8) | 0x39 — DRAM + DUAL_CAS + bit4 + CLOCK_EN */
#define CONFIG_STAT0_CT_DRAM      0x39u
#define CONFIG_STAT0_CT_DRAM_MASK 0x3Fu
/* Cold BUS_CNTL from same table (4EEC/4EEE). */
#define BUS_CNTL_CT_COLD 0x600020F8u

typedef struct
{
    ULONG reserved_hi : 19;     /* Bits 13-31: PIX_WIDTH/BNDRY etc. — leave alone */
    ULONG mem_refresh_rate : 2; /* Bits 11-12: MEM_REFRESH_RATE (r) */
    ULONG mem_cyc_lnth : 2;     /* Bits 9-10: MEM_CYC_LNTH (q), CT encoding */
    ULONG reserved_mid : 6;     /* Bits 3-8 */
    ULONG mem_size : 3;         /* Bits 0-2: MEM_SIZE (p) */
} MEM_CNTL_CT_t;

static void print_MEM_CNTL_CT(ULONG raw)
{
    MEM_CNTL_CT_t *r           = reinterpret_cast<MEM_CNTL_CT_t *>(&raw);
    static const char *sizes[] = {"reserved", "1M", "2M", "4M", "?", "?", "?", "?"};

    D(INFO, "MEM_CNTL_CT 0x%08lx size=%ld (%s) cyc=%ld refrate=%ld\n", raw, (ULONG)r->mem_size, sizes[r->mem_size & 7],
      (ULONG)r->mem_cyc_lnth, (ULONG)r->mem_refresh_rate);
}

static void print_CONFIG_CNTL_CT(ULONG raw)
{
    ULONG apSize = raw & CFG_MEM_AP_SIZE_MASK;
    D(INFO, "CONFIG_CNTL 0x%08lx AP_SIZE=%ld (%s) VGA_AP_EN=%ld VGA_DIS=%ld AP_LOC=%ld\n", raw, apSize,
      apSize == CFG_MEM_AP_SIZE_8M ? "2x8M" : (apSize == 0 ? "off/rsvd" : "?"), !!(raw & CFG_MEM_VGA_AP_EN),
      !!(raw & CFG_VGA_DIS), (raw & CFG_MEM_AP_LOC_MASK) >> 4);
}

static void print_CONFIG_STAT0_CT(ULONG raw)
{
    ULONG memType              = raw & CFG_MEM_TYPE_CT_MASK;
    static const char *types[] = {"DISABLE", "DRAM", "EDO", "rsvd3", "rsvd4", "rsvd5", "rsvd6", "rsvd7"};

    D(INFO, "CONFIG_STAT0 0x%08lx mem_type=%ld (%s) dual_cas=%ld clock_en=%ld\n", raw, memType, types[memType],
      !!(raw & CFG_DUAL_CAS_EN_CT), !!(raw & CFG_CLOCK_EN_CT));
    if (memType == CFG_MEM_TYPE_CT_DISABLE) {
        D(WARN, "CT CFG_MEM_TYPE=0 → memory access disabled (RRG)\n");
    } else if (memType == CFG_MEM_TYPE_CT_DRAM || memType == CFG_MEM_TYPE_CT_EDO) {
        D(INFO, "CT DRAM/EDO access enabled (CFG_MEM_TYPE=%ld)\n", memType);
    } else {
        D(WARN, "CT CFG_MEM_TYPE=%ld reserved on CT — not a documented DRAM/EDO strap\n", memType);
    }
}

static BOOL probeMemorySize(BoardInfo_t *bi)
{
    DFUNC(VERBOSE, "\n");
    DRIVER_LOCALS(bi);
    LOCAL_SYSBASE();

    ULONG memCntlSave = mmio.readL(MEM_CNTL);
    ULONG configStat0 = mmio.readL(CONFIG_STAT0);
    print_MEM_CNTL_CT(memCntlSave);
    print_CONFIG_CNTL_CT(drv->readConfigCntl());
    print_CONFIG_STAT0_CT(configStat0);

    /*
     * VBIOS FUN_7934 / table @7953 before size detect:
     * CONFIG_STAT0 ← (r&~7)|0x39 (DRAM+DUAL_CAS+bit4+CLOCK_EN), MEM_CNTL=0x04F1,
     * BUS_CNTL=0x600020F8. Straps alone leave size=reserved and dual_cas=0 (256Kx4).
     */
    {
        ULONG busSave = mmio.readL(BUS_CNTL);
        D(INFO, "CT cold DRAM bring-up (was STAT0=0x%08lx MEM_CNTL=0x%08lx BUS=0x%08lx)\n", configStat0, memCntlSave,
          busSave);
        mmio.writeMaskL(CONFIG_STAT0, CONFIG_STAT0_CT_DRAM_MASK, CONFIG_STAT0_CT_DRAM);
        mmio.writeL(MEM_CNTL, MEM_CNTL_CT_COLD);
        mmio.writeL(BUS_CNTL, BUS_CNTL_CT_COLD);
        flushWrites();
        print_CONFIG_STAT0_CT(mmio.readL(CONFIG_STAT0));
        print_MEM_CNTL_CT(mmio.readL(MEM_CNTL));
        D(INFO, "BUS_CNTL now 0x%08lx\n", mmio.readL(BUS_CNTL));
    }

    static const ULONG memorySizes[] = {0x400000, 0x200000, 0x100000};
    static const ULONG memoryCodes[] = {MEM_SIZE_CT_4M, MEM_SIZE_CT_2M, MEM_SIZE_CT_1M};

    volatile ULONG *framebuffer = (volatile ULONG *)bi->MemoryBase;
    framebuffer[0]              = 0;

    for (int i = 0; i < ARRAY_SIZE(memorySizes); i++) {
        bi->MemorySize = memorySizes[i];
        D(VERBOSE, "\nProbing CT memory size %ld (MEM_SIZE=%ld) via LFB\n", bi->MemorySize, memoryCodes[i]);

        mmio.writeMaskL(MEM_CNTL, MEM_SIZE_CT_MASK, memoryCodes[i]);
        flushWrites();
        CacheClearU();

        volatile ULONG *highOffset = framebuffer + (bi->MemorySize >> 2) - 512 - 1;
        volatile ULONG *lowOffset  = framebuffer + (bi->MemorySize >> 3);
        *framebuffer               = 0;
        *highOffset                = (ULONG)highOffset;
        *lowOffset                 = (ULONG)lowOffset;
        CacheClearU();

        ULONG readbackHigh = *highOffset;
        ULONG readbackLow  = *lowOffset;
        ULONG readbackZero = *framebuffer;

        D(VERBOSE, "Probing memory at 0x%lx ?= 0x%lx; 0x%lx ?= 0x%lx, 0x0 ?= 0x%lx\n", highOffset, readbackHigh,
          lowOffset, readbackLow, readbackZero);

        if (readbackHigh == (ULONG)highOffset && readbackLow == (ULONG)lowOffset && readbackZero == 0) {
            D(INFO, "CT MemorySize %ld (MEM_SIZE=%ld, LFB)\n", bi->MemorySize, memoryCodes[i]);
            print_MEM_CNTL_CT(mmio.readL(MEM_CNTL));
            return TRUE;
        }
    }

    mmio.writeL(MEM_CNTL, memCntlSave);
    flushWrites();
    D(VERBOSE, "CT LFB memory size probe failed.\n\n");
    return FALSE;
}

BOOL InitMach64CT(struct BoardInfo *bi)
{
    DFUNC(INFO, "\n");
    DRIVER_LOCALS(bi);

    ULONG chipID = mmio.readL(CONFIG_CHIP_ID);
    DFUNC(INFO, "Mach64 CT init (CONFIG_CHIP_ID 0x%04lx, rev 0x%02lx)\n", chipID & 0xFFFF, chipID >> 24);

    ChipSpecific_t *cs = getChipSpecific(bi);

    if (!cs->referenceDivider)
        cs->referenceDivider = 0x36;

    /* Appendix J CT/ET VCO ≤ 135 MHz — ROM max_pclk can be higher. */
    if (cs->maxPClock > 13500)
        cs->maxPClock = 13500;

    /* Match ROM M; take PLL out of override/reset; OSC on; VCLK from PLL (Appendix J §J.4). */
    WRITE_PLL(PLL_REF_DIV, cs->referenceDivider);
    WRITE_PLL_MASK(PLL_GEN_CNTL, PLL_OVERRIDE_MASK | PLL_MRESET_MASK | OSC_EN_MASK | MCLK_SRC_SEL_MASK,
                   OSC_EN | MCLK_SRC_SEL(0b100));
    WRITE_PLL_MASK(PLL_VCLK_CNTL, PLL_PRESET_MASK | PLL_EXT_CLK_EN_MASK | VCLK_SRC_SEL_MASK, VCLK_SRC_SEL(0b11));
    delayMilliSeconds(5);

    D(INFO, "PLL_REF_DIV=%ld PLL_GEN_CNTL=0x%02lx PLL_VCLK_CNTL=0x%02lx\n", (ULONG)READ_PLL(PLL_REF_DIV),
      (ULONG)READ_PLL(PLL_GEN_CNTL), (ULONG)READ_PLL(PLL_VCLK_CNTL));

    cs->computeVCLKFrequency = computeVCLKFrequencyKhz10_CT;
    bi->SetClock             = SetClock_CT;

    InitVClockPLLTable(bi, reinterpret_cast<const BYTE *>(g_VPLLPostDivider), ARRAY_SIZE(g_VPLLPostDivider));

    mmio.writeMaskL(CRTC_GEN_CNTL,
                    CRTC_ENABLE_MASK | CRTC_EXT_DISP_EN_MASK | CRTC_DISP_REQ_ENB_MASK | VGA_XCRT_CNT_EN_MASK |
                        CRTC_DISPLAY_DIS_MASK | VGA_ATI_LINEAR_MASK,
                    CRTC_ENABLE | CRTC_EXT_DISP_EN | VGA_XCRT_CNT_EN);

    /* DRAM needs internal MCLK before LFB probe (cold boot without VBIOS). */
    SetMemoryClock(bi, resolveMemoryClockKhz10(bi));

    if (!probeMemorySize(bi)) {
        return FALSE;
    }

    return TRUE;
}

#ifdef __cplusplus
}
#endif
