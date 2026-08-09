#include "mach32_ramdac.h"

/*
 * Sorted by achieved pixel clock (centi-MHz). For each CLK_SEL (except "external"=6),
 * we list both /2 and /1 variants. The corresponding entry in clk_sel_index_sorted is:
 *   bits 3:0 = CLK_SEL (0..15)
 *   bit 7    = 1 if CLK_DIV (/2) is used
 */

static const UWORD ati1811_1_10Khz[PIXEL_CLOCK_INDEX_COUNT] = {
    1600, 1800, 1995, 2245, 2245, 2517, 2832, 3200, 3250, 3600, 3750, 3991,  4000,  4490,  4490,
    4620, 5000, 5035, 5500, 5664, 6300, 6500, 6750, 7500, 8000, 9240, 10000, 11000, 12600, 13500,
};

static const UBYTE ati1811_1_clkIndices[PIXEL_CLOCK_INDEX_COUNT] = {
    0x89, 0x83, 0x8C, 0x87, 0x8D, 0x84, 0x85, 0x09, 0x8F, 0x03, 0x8E, 0x0C, 0x8B, 0x07, 0x0D,
    0x82, 0x80, 0x04, 0x8A, 0x05, 0x81, 0x0F, 0x88, 0x0E, 0x0B, 0x02, 0x00, 0x0A, 0x01, 0x08};

ULONG HzForClockIndex(ULONG index)
{
    UWORD centi = ati1811_1_10Khz[index];
    if (centi == (UWORD)~0u)
        return 0;
    return (ULONG)centi * 10000UL;
}

/* Bt481 / REG688000: HiColor uses ~2× synthesizer vs logical dot rate; 24/32 bpp truecolor uses ~3×. */
static ULONG logicalToSynthHzMultiplier(RGBFTYPE fmt)
{
    switch (fmt) {
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
        return 2;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
    case RGBFB_R8G8B8A8:
    case RGBFB_B8G8R8A8:
    case RGBFB_A8R8G8B8:
    case RGBFB_A8B8G8R8:
        return 3;
    default:
        return 1;
    }
}

ULONG HzForClockIndexAsLogicalDotsPerSecond(ULONG index, RGBFTYPE format)
{
    ULONG raw   = HzForClockIndex(index);
    ULONG mult  = logicalToSynthHzMultiplier(format);
    if (mult <= 1)
        return raw;
    return raw / mult;
}

LONG ResolveModeInfoPixelClock(struct ModeInfo *mi, ULONG targetHz, RGBFTYPE format)
{
    DFUNC(VERBOSE, "targetHz=%lu fmt=%ld\n", targetHz, (ULONG)format);

    UWORD bestErr  = 0xFFFF;
    WORD bestIndex = 0;

    ULONG matchHz = targetHz * logicalToSynthHzMultiplier(format);

    UWORD target10Khz = (ULONG)(matchHz + 5000UL) / 10000UL;

    for (WORD i = 0; i < PIXEL_CLOCK_INDEX_COUNT; i++) {
        UWORD freq10khz = ati1811_1_10Khz[i];

        UWORD err = (freq10khz > target10Khz) ? (freq10khz - target10Khz) : (target10Khz - freq10khz);
        if (err < bestErr) {
            bestErr   = err;
            bestIndex = i;
        }
    }

    DFUNC(VERBOSE, "bestIndex=%ld, synthHz=%lu logicalDotsPerS=%lu\n", (ULONG)bestIndex, HzForClockIndex((ULONG)bestIndex),
          targetHz);

    UBYTE selEnc = ati1811_1_clkIndices[bestIndex];
    mi->PixelClock = targetHz;

    /* Use the Tseng-style ModeInfo union members: pll1.Clock / pll2.ClockDivide. */
    mi->pll1.Clock       = selEnc;
    mi->pll2.ClockDivide = (selEnc & 0x80) ? (UBYTE)2 : (UBYTE)1;

    return (LONG)bestIndex;
}

/* Brooktree Bt481A Command Register B  */
#define BT481_CMD_B_7_5_IRE  BIT(5)
#define BT481_CMD_B_8BIT_DAC BIT(1)

/* Bt481A datasheet "Automatic Detection Without The RS2 Line": indirect read of Command Register B = 0x1Eh. */
#define BT481_CMD_B_SIGNATURE 0x1E

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

static UBYTE vfifoDepthForMode(BoardInfo_t *bi)
{
    UBYTE depth;
    ULONG clockHz = 0;
    UWORD width   = 0;

    switch (bi->RGBFormat) {
    case RGBFB_CLUT:
    case RGBFB_NONE:
        depth = 6;
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
        depth = 9;
        break;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
    case RGBFB_R8G8B8A8:
    case RGBFB_B8G8R8A8:
    case RGBFB_A8R8G8B8:
    case RGBFB_A8B8G8R8:
        depth = 14;
        break;
    default:
        depth = 14;
        break;
    }

    if (bi->ModeInfo) {
        clockHz = bi->ModeInfo->PixelClock;
        width   = bi->ModeInfo->Width;
    }

    /*
     * CLOCK_SEL VFIFO_DEPTH: refill starts when FIFO occupancy drops below this
     * (REG688000-15). Raise at high dot clocks / wide lines so CRT keeps priority
     * when the GE is busy — otherwise the display FIFO underruns.
     */
    if (clockHz >= 100000000UL)
        depth += 2;
    else if (clockHz >= 80000000UL)
        depth += 1;

    // if (width > 1024)
    //     depth += 4;
    // else if (width > 800)
    //     depth += 2;

    if (depth > 15)
        depth = 15;
    return depth;
}

static void generic_setClock(BoardInfo_t *bi)
{
    const struct ModeInfo *mi = bi->ModeInfo;

    UBYTE selEnc = mi->pll1.Clock;
    UBYTE sel    = selEnc & 0x0F;
    UBYTE div2   = (selEnc & 0x80) != 0;
    UBYTE vfifo  = vfifoDepthForMode(bi);
    UWORD bits   = CLK_SEL(sel) | VFIFO_DEPTH(vfifo);
    if (div2) {
        bits |= CLK_DIV;
    }
    DFUNC(INFO, "SetClock idx=%ld (/%ld) clock=%luHz vfifo=%ld\n", (ULONG)sel, (ULONG)mi->pll2.ClockDivide,
          mi->PixelClock, (ULONG)vfifo);

    REGBASE();
    W_IO_MASK_W(CLOCK_SEL, CLK_SEL_MASK | CLK_DIV_MASK | VFIFO_DEPTH_MASK | PASS_THROUGH_DISABLE_MASK,
                bits | PASS_THROUGH_DISABLE);
}

static ULONG generic_setMemoryClock(BoardInfo_t *bi, ULONG clockHz)
{
    (void)bi;
    (void)clockHz;
    return 0;
}

/*
 * Enter/exit Bt481A extended register access via the consecutive-read sequence
 * (Bt481A datasheet "Automatic Detection Without The RS2 Line").
 * A write to DAC_W_INDEX resets the internal read counter, then 4 consecutive
 * reads of DAC_MASK prime it so that the next WRITE to DAC_MASK is redirected
 * to Command Register A instead of the pixel read mask.
 */
static void bt481_enterExtended(BoardInfo_t *bi)
{
    REGBASE();
    DAC_ENABLE_RS2()
    delayMicroSeconds(2);
    W_REG(DAC_MASK, 0x01); /* Command A: A0=1 → extended register access enabled */
    delayMicroSeconds(2);
    DAC_DISABLE_RS2();
    delayMicroSeconds(2);

    // Alternative access mode for cards that have RS2 grounded
    // W_REG(DAC_MASK, 0xFF);
    // Read Pixel Read Mask Register 4 times consecutively, so that the next write will be directed to Command Register
    // A. R_REG(DAC_MASK); R_REG(DAC_MASK); R_REG(DAC_MASK); R_REG(DAC_MASK); W_REG(DAC_MASK, 0x01); /* Command A: A0=1
    // → extended set enabled */
}

static void bt481_exitExtended(BoardInfo_t *bi)
{
    REGBASE();
    DAC_ENABLE_RS2()
    W_REG(DAC_MASK, 0x00); /* Command A: clear A0, exit extended register access*/
    delayMicroSeconds(2);
    DAC_DISABLE_RS2()
    delayMicroSeconds(2);

    // Alternative access mode for cards that have RS2 grounded
    // W_REG(DAC_W_INDEX, 0x00);
    // W_REG(DAC_MASK, 0xFF);
    // R_REG(DAC_MASK);
    // R_REG(DAC_MASK);
    // R_REG(DAC_MASK);
    // R_REG(DAC_MASK);
    // W_REG(DAC_MASK, 0x00); /* Command A: clear A0 */
}

static void bt481_writeCommandRegisterB(BoardInfo_t *bi, UBYTE value)
{
    REGBASE();
    /* Indirect access: addr reg = 2, then data via "read mask" port (datasheet Table 5). */
    W_REG(DAC_W_INDEX, 0x02);
    delayMicroSeconds(2);
    W_REG(DAC_MASK, value);
}

/*
 * Brooktree Bt481A detection and mode init: enter extended access, verify
 * Command Register B, then program 8-bit DAC and BLACKLEVEL pedestal bit.
 */
BOOL initBt481(BoardInfo_t *bi)
{
    REGBASE();
    bt481_enterExtended(bi);
    W_REG(DAC_W_INDEX, 0x02);  // read Command register B
    delayMicroSeconds(2);
    UBYTE sig = R_REG(DAC_MASK);

    /* Power-up signature 0x1E, or warm-boot leftover of bits we program. */
    if (sig != BT481_CMD_B_SIGNATURE && (sig & ~(BT481_CMD_B_7_5_IRE | BT481_CMD_B_8BIT_DAC)) != 0) {
        DFUNC(ERROR, "Bt481 init failed: expected signature 0x%02x, got 0x%02x\n", BT481_CMD_B_SIGNATURE, sig);
        bt481_exitExtended(bi);

        return FALSE;
    }

    UBYTE cmdB = 0;
    if (!(bi->CardFlags & CFF_BLACKLEVEL_BLACK))
        cmdB |= BT481_CMD_B_7_5_IRE;
    if (bi->BitsPerCannon == 8) {
        cmdB |= BT481_CMD_B_8BIT_DAC;
    }

    delayMicroSeconds(2);
    bt481_writeCommandRegisterB(bi, cmdB);

    bt481_exitExtended(bi);

    return TRUE;
}

/* Brooktree Table 2: RS=000 palette addr write, RS=001 palette RAM data. */
static void bt481_write_palette_addr_write(BoardInfo_t *bi, UBYTE addr)
{
    REGBASE();
    W_REG(DAC_W_INDEX, addr);
    delayMicroSeconds(2);
}

static void bt481_write_rgb(BoardInfo_t *bi, UBYTE r, UBYTE g, UBYTE b)
{
    REGBASE();
    W_REG(DAC_DATA, r);
    W_REG(DAC_DATA, g);
    W_REG(DAC_DATA, b);
}

static void bt481_setDac(BoardInfo_t *bi, RGBFTYPE format)
{
#define BT481_MODE(x) ((x) << 4)
#define BT481_BGR     BIT(1)

    UBYTE dacMode = 0;  // CLUT
    switch (format) {
    case RGBFB_R5G6B5PC:
        dacMode = BT481_MODE(0b1110);  // 5:6:5 Single Edge mode
        break;
    case RGBFB_R5G5B5PC:
        dacMode = BT481_MODE(0b1010);  // 5:5:5 Single Edge mode
        break;
    case RGBFB_R8G8B8:
    case RGBFB_R8G8B8A8:
    case RGBFB_A8R8G8B8:
        dacMode = BT481_MODE(0b1111);  // 8:8:8 Single Edge
        break;
    case RGBFB_B8G8R8:
    case RGBFB_B8G8R8A8:
    case RGBFB_A8B8G8R8:
        dacMode = BT481_MODE(0b1111) | BT481_BGR;  // 8:8:8 Single Edge, BGR order
        break;
    default:
        break;
    }

    REGBASE();

    // If using 32bit dual-edge mode passes a 8-bit VGA "overlay" in the last byte.
    // Mask that out.
    format == RGBFB_CLUT ? W_REG(DAC_MASK, 0xFF) : W_REG(DAC_MASK, 0x00);

    DAC_ENABLE_RS2();
    W_REG(DAC_W_INDEX, 0x01); /* Command Register A */
    delayMicroSeconds(2);
    W_REG(DAC_MASK, dacMode);
    DAC_DISABLE_RS2();
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
        if (initBt481(bi) == FALSE) {
            DFUNC(ERROR, "CONFIG_DAC type %lu detected but no Bt481 signature found; using generic stub ops\n",
                  dacType);
            return FALSE;
        }
        getChipData(bi)->ramdacOps = &bt481_ramdac_ops;
        DFUNC(INFO, "RamdacOps: Brooktree Bt481/Bt482\n");
    } else {
        return FALSE;
    }

    return TRUE;
}
