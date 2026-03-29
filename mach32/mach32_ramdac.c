#include "mach32_ramdac.h"

/* CONFIG_STATUS_1[11:9] RAMDAC type — REG688000-15: 4 = Bt481/Bt482 */
#define CFG_DAC_BT481 4u

/*
 * ATI 18811-1 pixel clocks — REG688000-15 Appendix E (MHz), table “18811-1 Clock Chip”.
 * CLK_SEL[5:2] selects one of 16 on-chip clocks; left-to-right is 0..7 (first row), 8..15
 * (second row). “External” (select 6) is not a fixed MHz from the synthesizer — stored as 0.
 * Frequencies are in centi-MHz (100 * MHz, e.g. 44.90 MHz -> 4490) so they fit in UWORD.
 */
#define NUM_PIXEL_CLOCK_SEL 16u

/* Same 16 entries sorted by increasing centi-MHz; parallel array gives CLK_SEL index (0..15). */
static const UWORD pixel_clock_sorted_centi_mhz[NUM_PIXEL_CLOCK_SEL] = {
    0, 3200, 3600, 3991, 4490, 4490, 5035, 5664, 6500, 7500, 8000, 9240, 10000, 11000, 12600, 13500,
};

static const UBYTE clk_sel_index_sorted[NUM_PIXEL_CLOCK_SEL] = {
    6, 9, 3, 12, 7, 13, 4, 5, 15, 14, 11, 2, 0, 10, 1, 8,
};

/* Brooktree Bt481A Command Register B — init example (datasheet §Initializing).
 * 8-bit MPU path, 7.5 IRE pedestal, sync-on-green enabled in ATI sample ($6A).
 * Use $2A for 0 IRE blanking pedestal (B5=0) if the monitor path expects it. */
#define BT481_CMD_B_INIT_7_5_IRE 0x6Au
#define BT481_CMD_B_INIT_0_IRE   0x2Au
/* Bt481A datasheet “Automatic Detection Without The RS2 Line”: indirect read of Command Register B = 0x1Eh. */
#define BT481_CMD_B_SIGNATURE 0x1Eu

static const struct svga_pll g_svga_pll = {
    .m_min     = 1,
    .m_max     = 255,
    .n_min     = 1,
    .n_max     = 255,
    .r_min     = 0,
    .r_max     = 3,
    .f_vco_min = 100000,
    .f_vco_max = 250000,
    .f_base    = 14318,
};

static void generic_getPllParams(BoardInfo_t *bi, const struct svga_pll **pll, UWORD *maxFreqMhz)
{
    (void)bi;
    *pll        = &g_svga_pll;
    *maxFreqMhz = 135;
}

static void generic_packPllToModeInfo(BoardInfo_t *bi, UWORD m, UWORD n, UWORD r, struct ModeInfo *mi)
{
    (void)bi;
    mi->pll1.Numerator   = (UBYTE)m;
    mi->pll2.Denominator = (UBYTE)n;
    (void)r;
}

static void generic_setClock(BoardInfo_t *bi)
{
    (void)bi;
    /* Pixel / MCLK come from ATI clock synthesizer (ICS/18811) via CLOCK_SEL (4AEEh); not on Bt481. */
}

static ULONG generic_setMemoryClock(BoardInfo_t *bi, ULONG clockHz)
{
    (void)bi;
    (void)clockHz;
    return 0;
}

/*
 * Bt481 “RS2 grounded” extended register access (Bt481A datasheet §Automatic Detection).
 * Maps to VGA 3C6/3C8; on Mach32 use DAC_MASK / DAC_W_INDEX.
 */
static void bt481_enterExtended(BoardInfo_t *bi)
{
    REGBASE();
    DAC_ENABLE_RS2()
    W_REG(DAC_MASK, 0x01); /* Command A: A0=1 → extended regisetr access enabled */
    DAC_DISABLE_RS2();

    // W_REG(DAC_MASK, 0xFF);
    // //Read Pixel Read Mask Register 4 times consecutively, so that the next write will be directed to Command Register A.
    // R_REG(DAC_MASK);
    // R_REG(DAC_MASK);
    // R_REG(DAC_MASK);
    // R_REG(DAC_MASK);
    // W_REG(DAC_MASK, 0x01); /* Command A: A0=1 → extended set enabled */
}

static void bt481_exitExtended(BoardInfo_t *bi)
{
    REGBASE();
    DAC_ENABLE_RS2()
    W_REG(DAC_MASK, 0x00); /* Command A: clear A0, exit extended register access*/
    DAC_DISABLE_RS2()

    // W_REG(DAC_W_INDEX, 0x00);
    // W_REG(DAC_MASK, 0xFF);
    // R_REG(DAC_MASK);
    // R_REG(DAC_MASK);
    // R_REG(DAC_MASK);
    // R_REG(DAC_MASK);
    // W_REG(DAC_MASK, 0x00); /* Command A: clear A0 */
}

/*
 * Brooktree Bt481A automatic detection (RS2 grounded): enter extended access, address Command Register B,
 * read signature from the pixel read mask data port. Always runs bt481_exitExtended() to restore DAC_MASK/A0.
 */
BOOL initBt481(BoardInfo_t *bi)
{
    REGBASE();
    bt481_enterExtended(bi);
    W_REG(DAC_W_INDEX, 0x02);
    delayMicroSeconds(2);
    UBYTE sig = R_REG(DAC_MASK);

    if (sig != BT481_CMD_B_SIGNATURE)
    {
        DFUNC(ERROR, "Bt481 init failed: expected signature 0x%02x, got 0x%02x\n", BT481_CMD_B_SIGNATURE, sig);
        bt481_exitExtended(bi);

        return FALSE;
    }


    bt481_exitExtended(bi);

    return TRUE;
}

static void bt481_writeCommandRegisterB(BoardInfo_t *bi, UBYTE value)
{
    REGBASE();
    /* Indirect access: addr reg = 2 → Command B via “read mask” data port (datasheet Table 5). */
    W_REG( DAC_W_INDEX, 0x02);
    delayMicroSeconds(2);
    W_REG( DAC_MASK, value);
    delayMicroSeconds(2);
}

static void bt481_apply_ext_ge_for_format(BoardInfo_t *bi, RGBFTYPE fmt)
{
    // /* Preserve monitor alias bits 3:0 and ALIAS_ENA (bit 3 field in doc); only touch documented DAC fields. */
    // UWORD w    = ext_ge_read(bi);
    // UWORD keep = (UWORD)(w & (UWORD)0x000Fu); /* MONITOR_ALIAS 2:0 + ALIAS_ENA */

    // UWORD ge = keep;

    // switch (fmt) {
    // case RGBFB_CLUT:
    //     ge |= (1u << EXTGE_PIXEL_WIDTH_SHIFT); /* 8 bpp */
    //     ge |= EXTGE_DAC_8BIT_BIT;
    //     ge &= (UWORD)~EXTGE_MULTPIX_BIT;
    //     break;
    // case RGBFB_R5G6B5PC:
    // case RGBFB_R5G6B5:
    //     ge |= (2u << EXTGE_PIXEL_WIDTH_SHIFT); /* 16 bpp */
    //     ge |= EXTGE_DAC_8BIT_BIT;
    //     ge |= (1u << EXTGE_16COLOR_MODE_SHIFT); /* 5:6:5 */
    //     ge &= (UWORD)~EXTGE_MULTPIX_BIT;
    //     break;
    // case RGBFB_R5G5B5PC:
    // case RGBFB_R5G5B5:
    //     ge |= (2u << EXTGE_PIXEL_WIDTH_SHIFT);
    //     ge |= EXTGE_DAC_8BIT_BIT;
    //     ge &= (UWORD)~EXTGE_16COLOR_MODE_MASK; /* 5:5:5 */
    //     ge &= (UWORD)~EXTGE_MULTPIX_BIT;
    //     break;
    // case RGBFB_R8G8B8:
    // case RGBFB_B8G8R8:
    //     ge |= (3u << EXTGE_PIXEL_WIDTH_SHIFT); /* 24 bpp */
    //     ge |= EXTGE_DAC_8BIT_BIT;
    //     if (fmt == RGBFB_B8G8R8)
    //         ge |= EXTGE_24COLOR_ORDER_BIT;
    //     else
    //         ge &= (UWORD)~EXTGE_24COLOR_ORDER_BIT;
    //     ge &= (UWORD)~EXTGE_24BPP_CFG_BIT; /* 3 bytes/pixel */
    //     break;
    // default:
    //     ge |= (1u << EXTGE_PIXEL_WIDTH_SHIFT);
    //     ge |= EXTGE_DAC_8BIT_BIT;
    //     break;
    // }

    // /* RS[3:2] to Bt481 = 0 for standard command/palette access (Appx F type 4). */
    // ge &= (UWORD)~EXTGE_DAC_EXT_ADDR_MASK;

    // REGBASE();
    // W_IO_W(EXT_GE_CONFIG, ge);
    // delayMicroSeconds(10);
}

/* Brooktree Table 2: RS=000 palette addr write, RS=001 palette RAM data. */
static void bt481_write_palette_addr_write(BoardInfo_t *bi, UBYTE addr)
{
    REGBASE();
    W_REG( DAC_W_INDEX, addr);
    delayMicroSeconds(2);
}

static void bt481_write_rgb(BoardInfo_t *bi, UBYTE r, UBYTE g, UBYTE b)
{
    REGBASE();
    W_REG( DAC_DATA, r);
    W_REG( DAC_DATA, g);
    W_REG( DAC_DATA, b);
}

/*
 * Bt481 Command Register A (RS=110): A7=0 selects 256-color LUT path (datasheet §Command Register A).
 * Written through mode-control path per ATI type-4 = register “6h”. Here we drive pseudo-color by
 * clearing extended true-color after programming Command B; full RS=110 MMIO mux is board-specific.
 */
static void bt481_set_pseudocolor_path(BoardInfo_t *bi)
{
    REGBASE();
    bt481_enterExtended(bi);
    bt481_writeCommandRegisterB(bi, BT481_CMD_B_INIT_0_IRE);
    /* Command A via extended: force A7..A4 = 0 by closing extended and using default palette path. */
    bt481_exitExtended(bi);
    W_REG( DAC_MASK, 0xFF);
}

static void bt481_setDac(BoardInfo_t *bi, RGBFTYPE format)
{
    LOCAL_SYSBASE();

    bt481_apply_ext_ge_for_format(bi, format);

    if (format == RGBFB_CLUT) {
        bt481_set_pseudocolor_path(bi);
        /* Linear ramp — OS/tooling will call SetColorArray for real LUT. */
        bt481_write_palette_addr_write(bi, 0);
        for (UWORD i = 0; i < 256; i++) {
            UBYTE v = (UBYTE)i;
            bt481_write_rgb(bi, v, v, v);
        }
        DFUNC(INFO, "Bt481: 8-bit pseudo-color DAC path, palette defaulted\n");
        return;
    }

    /* 16/24 bpp: program extended command B for 8-bit DAC + IRE; true-color A[7:4] needs MODE_CONTROL. */
    bt481_enterExtended(bi);
    bt481_writeCommandRegisterB(bi, BT481_CMD_B_INIT_0_IRE);
    bt481_exitExtended(bi);

    if (format == RGBFB_R5G6B5PC || format == RGBFB_R5G6B5 || format == RGBFB_R5G5B5PC || format == RGBFB_R5G5B5) {
        DFUNC(INFO, "Bt481: EXT_GE 16 bpp; TODO command A true-color 5:6:5/5:5:5 (RS=110)\n");
    } else if (format == RGBFB_R8G8B8 || format == RGBFB_B8G8R8) {
        DFUNC(INFO, "Bt481: EXT_GE 24 bpp; TODO command A 8:8:8 bypass (RS=110)\n");
    } else {
        DFUNC(WARN, "Bt481: unhandled RGBFTYPE %d — EXT_GE only\n", (int)format);
    }
}

static const RamdacOps_t bt481_ramdac_ops = {
    .getPllParams      = generic_getPllParams,
    .packPllToModeInfo = generic_packPllToModeInfo,
    .setClock          = generic_setClock,
    .setMemoryClock    = generic_setMemoryClock,
    .setDac            = bt481_setDac,
};

BOOL InitRAMDAC(BoardInfo_t *bi, DACType dacType)
{
    if (dacType == BT481) {
        if (initBt481(bi) == FALSE)
        {
            DFUNC(ERROR, "CONFIG_DAC type %lu detected but no Bt481 signature found; using generic stub ops\n", dacType);
            return FALSE;
        }
        getChipData(bi)->ramdacOps = &bt481_ramdac_ops;
        DFUNC(INFO, "RamdacOps: Brooktree Bt481/Bt482\n");
    } else {
        return FALSE;
    }

    return TRUE;
}
