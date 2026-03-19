#include "s3ramdac.h"
#include "chip_s3trio64.h"
#include "s3trio64_common.h"

// External RAMDAC helpers (kept private to this translation unit)
static BOOL CheckForSDAC(struct BoardInfo *bi);
static BOOL InitSDAC(struct BoardInfo *bi);
static BOOL CheckForRGB524(struct BoardInfo *bi);
static BOOL InitRGB524(struct BoardInfo *bi);

static void integrated_getPllParams(struct BoardInfo *bi, const struct svga_pll **pll, UWORD *maxFreqMhz);
static void externalSdac_getPllParams(struct BoardInfo *bi, const struct svga_pll **pll, UWORD *maxFreqMhz);

static void integrated_packPllToModeInfo(struct BoardInfo *bi, UWORD m, UWORD n, UWORD r, struct ModeInfo *mi);
static void aurora64_packPllToModeInfo(struct BoardInfo *bi, UWORD m, UWORD n, UWORD r, struct ModeInfo *mi);
static void sdac_packPllToModeInfo(struct BoardInfo *bi, UWORD m, UWORD n, UWORD r, struct ModeInfo *mi);

static void integrated_setClock(struct BoardInfo *bi);
static void sdac_setClock(struct BoardInfo *bi);

static ULONG integrated_setMemoryClock(struct BoardInfo *bi, ULONG clockHz);
static ULONG sdac_setMemoryClock(struct BoardInfo *bi, ULONG clockHz);

static void integrated_setDac(struct BoardInfo *bi, RGBFTYPE format);
static void sdac_setDac(struct BoardInfo *bi, RGBFTYPE format);

// PLL limits used by svga_compute_pll (see common.h struct svga_pll)
static const struct svga_pll s3trio64_pll = {3, 129, 3, 33, 0, 3, 35000, 240000, 14318};
static const struct svga_pll s3sdac_pll   = {3, 129, 3, 33, 0, 3, 60000, 270000, 14318};

static const RamdacOps_t integrated_ops = {
    .getPllParams      = integrated_getPllParams,
    .packPllToModeInfo = integrated_packPllToModeInfo,
    .setClock          = integrated_setClock,
    .setMemoryClock    = integrated_setMemoryClock,
    .setDac            = integrated_setDac,
};

static const RamdacOps_t aurora64_ops = {
    .getPllParams      = integrated_getPllParams,
    .packPllToModeInfo = aurora64_packPllToModeInfo,
    .setClock          = integrated_setClock,
    .setMemoryClock    = integrated_setMemoryClock,
    .setDac            = integrated_setDac,
};

static const RamdacOps_t sdac_ops = {
    .getPllParams      = externalSdac_getPllParams,
    .packPllToModeInfo = sdac_packPllToModeInfo,
    .setClock          = sdac_setClock,
    .setMemoryClock    = sdac_setMemoryClock,
    .setDac            = sdac_setDac,
};

const RamdacOps_t *getRamdacOps(struct BoardInfo *bi)
{
    return getChipData(bi)->ramdacOps;
}

static void integrated_getPllParams(struct BoardInfo *bi, const struct svga_pll **pll, UWORD *maxFreqMhz)
{
    const ChipFamily_t family = getChipData(bi)->chipFamily;
    *pll                      = &s3trio64_pll;
    *maxFreqMhz               = (family >= TRIO64V2) ? 170 : 135;
}

static void externalSdac_getPllParams(struct BoardInfo *bi, const struct svga_pll **pll, UWORD *maxFreqMhz)
{
    (void)bi;
    *pll        = &s3sdac_pll;
    *maxFreqMhz = 135;
}

static void integrated_packPllToModeInfo(struct BoardInfo *bi, UWORD m, UWORD n, UWORD r, struct ModeInfo *mi)
{
    (void)bi;
    mi->pll1.Numerator   = (UBYTE)(m - 2);
    mi->pll2.Denominator = (UBYTE)((r << 5) | (n - 2));
}

static void aurora64_packPllToModeInfo(struct BoardInfo *bi, UWORD m, UWORD n, UWORD r, struct ModeInfo *mi)
{
    (void)bi;
    // Aurora64 (86CM65) uses SR12 layout: N in bits 5-0, R in bits 7-6
    mi->pll1.Numerator   = (UBYTE)(m - 2);
    mi->pll2.Denominator = (UBYTE)((r << 6) | (n - 2));
}

static void sdac_packPllToModeInfo(struct BoardInfo *bi, UWORD m, UWORD n, UWORD r, struct ModeInfo *mi)
{
    (void)bi;
    mi->pll1.Numerator   = (UBYTE)(m - 2);
    mi->pll2.Denominator = (UBYTE)((r << 5) | (n - 2));
}

static void integrated_setClock(struct BoardInfo *bi)
{
    REGBASE();

    struct ModeInfo *mi = bi->ModeInfo;

#ifdef DBG
    // Decode PLL values from ModeInfo packing (Trio64-style) and print effective frequency.
    // Aurora64 uses a different SR12 layout; its packPllToModeInfo uses r<<6, but setClock is shared.
    // So decode based on chip family here.
    {
        const ChipFamily_t family = getChipData(bi)->chipFamily;
        UWORD n_reg = (family == AURORA64PLUS) ? (mi->pll2.Denominator & 0x3F) + 2 : (mi->pll2.Denominator & 0x1F) + 2;
        UWORD r_reg = (family == AURORA64PLUS) ? (mi->pll2.Denominator >> 6) & 0x03 : (mi->pll2.Denominator >> 5) & 0x03;
        UWORD m_reg = (mi->pll1.Numerator + 2);
        ULONG f_vco = (s3trio64_pll.f_base * m_reg) / n_reg;
        ULONG frequencyKhz = f_vco >> r_reg;
        D(VERBOSE, "         Actual PixelClock %ldHz M: %ld N: %ld, R: %ld\n", frequencyKhz * 1000, (ULONG)m_reg,
          (ULONG)n_reg, (ULONG)r_reg);
    }
#endif

    // SR15 bit 4 selects DCLK = VCLK/2 (used for low clocks / pixel multiplex)
    if (mi->PixelClock < (ULONG)24000 * 1000 || mi->Flags & GMF_DOUBLECLOCK) {
        W_SR_MASK(0x15, BIT(4), BIT(4));
    } else {
        W_SR_MASK(0x15, BIT(4), 0);
    }

    W_SR(0x12, mi->pll2.Denominator);  // write N and R
    W_SR(0x13, mi->pll1.Numerator);    // write M

    // Load new DCLK frequency (SR15 bit 1: 0->1)
    UBYTE sr15 = R_SR(0x15) & ~BIT(1);
    W_SR(0x15, sr15);
    W_SR(0x15, sr15 | BIT(1));
}

static void sdac_setClock(struct BoardInfo *bi)
{
    REGBASE();

    struct ModeInfo *mi = bi->ModeInfo;

#ifdef DBG
    {
        UWORD n_reg = (mi->pll2.Denominator & 0x1F) + 2;
        UWORD r_reg = (mi->pll2.Denominator >> 5) & 0x03;
        UWORD m_reg = (mi->pll1.Numerator + 2);
        ULONG f_vco = (s3sdac_pll.f_base * m_reg) / n_reg;
        ULONG frequencyKhz = f_vco >> r_reg;
        D(VERBOSE, "         Actual PixelClock %ldHz M: %ld N: %ld, R: %ld\n", frequencyKhz * 1000, (ULONG)m_reg,
          (ULONG)n_reg, (ULONG)r_reg);
    }
#endif

    DAC_ENABLE_RS2();

    // CLK0 f2 parameters (f0/1 are not programmable on some SDAC variants; this matches existing code path)
    W_REG(SDAC_WR_ADR, 0x02);
    W_REG(SDAC_PLL_PARAM, mi->pll1.Numerator);
    W_REG(SDAC_PLL_PARAM, mi->pll2.Denominator);

    DAC_DISABLE_RS2();
}

static ULONG integrated_setMemoryClock(struct BoardInfo *bi, ULONG clockHz)
{
    REGBASE();

    USHORT m, n, r;
    UBYTE regval;

    int currentKhz = svga_compute_pll(&s3trio64_pll, clockHz / 1000, &m, &n, &r);
    if (currentKhz < 0) {
        return clockHz;
    }

    W_SR(0x10, (r << 5) | (n - 2));
    W_SR(0x11, m - 2);

    regval = R_SR(0x15) & ~BIT(0);
    W_SR(0x15, regval);
    W_SR(0x15, regval | BIT(0));
    W_SR(0x15, regval);

    // Keep existing perf tweak behavior (SR0x0A bit7 and SR15 bit7)
    if (clockHz <= 57000000) {
        W_SR_MASK(0xA, BIT(7), BIT(7));
        if (clockHz >= 55000000) {
            W_SR_MASK(0x15, BIT(7), BIT(7));
        }
        else
        {
            W_SR_MASK(0x15, BIT(7), 0);
        }
    } else {
        W_SR_MASK(0xA, BIT(7), 0x00);
        W_SR_MASK(0x15, BIT(7), 0x00);
    }

    return (ULONG)currentKhz * 1000;
}

static ULONG sdac_setMemoryClock(struct BoardInfo *bi, ULONG clockHz)
{
    REGBASE();

    USHORT m, n, r;

    int currentKhz = svga_compute_pll(&s3sdac_pll, clockHz / 1000, &m, &n, &r);
    if (currentKhz < 0) {
        return clockHz;
    }

    DAC_ENABLE_RS2();
    W_REG(SDAC_WR_ADR, 0x0A);
    W_REG(SDAC_PLL_PARAM, m - 2);
    W_REG(SDAC_PLL_PARAM, (r << 5) | (n - 2));
    DAC_DISABLE_RS2();

    return (ULONG)currentKhz * 1000;
}

// Integrated DAC (Trio64, Aurora64, etc.): CR67 pixel format. region unused (single DAC).
static void integrated_setDac(struct BoardInfo *bi, RGBFTYPE format)
{
    static const UBYTE DAC_ColorModes[] = {
        0x00,  // RGBFB_NONE
        0x00,  // RGBFB_CLUT
        0x70,  // RGBFB_R8G8B8
        0x70,  // RGBFB_B8G8R8
        0x50,  // RGBFB_R5G6B5PC
        0x30,  // RGBFB_R5G5B5PC
        0xd0,  // RGBFB_A8R8G8B8
        0xd0,  // RGBFB_A8B8G8R8
        0xd0,  // RGBFB_R8G8B8A8
        0xd0,  // RGBFB_B8G8R8A8
        0x50,  // RGBFB_R5G6B5
        0x30,  // RGBFB_R5G5B5
        0x50,  // RGBFB_B5G6R5PC
        0x30,  // RGBFB_B5G5R5PC
        0x00,  // RGBFB_YUV422CGX
        0x00,  // RGBFB_YUV411
        0x00,  // RGBFB_YUV411PC
        0x00,  // RGBFB_YUV422
        0x00,  // RGBFB_YUV422PC
        0x00,  // RGBFB_YUV422PA
        0x00,  // RGBFB_YUV422PAPC
    };

    REGBASE();

    UBYTE dacMode;
    if ((format == RGBFB_CLUT) && ((bi->ModeInfo->Flags & GMF_DOUBLECLOCK) != 0)) {
        D(INFO, "Setting 8bit multiplex DAC mode\n");
        dacMode = 0x11;
    } else {
        dacMode = DAC_ColorModes[format];
    }
    W_CR_MASK(0x67, 0xF1, dacMode);
}

// SDAC/GENDAC (Vision864, Vision968 fallback):
static void sdac_setDac(struct BoardInfo *bi, RGBFTYPE format)
{
    static const UBYTE SDAC_ColorModes[] = {
        0x00,  // RGBFB_NONE
        0x00,  // RGBFB_CLUT
        0x90,  // RGBFB_R8G8B8
        0x90,  // RGBFB_B8G8R8
        0x50,  // RGBFB_R5G6B5PC
        0x30,  // RGBFB_R5G5B5PC
        0x70,  // RGBFB_A8R8G8B8
        0x70,  // RGBFB_A8B8G8R8
        0x70,  // RGBFB_R8G8B8A8
        0x70,  // RGBFB_B8G8R8A8
        0x50,  // RGBFB_R5G6B5
        0x30,  // RGBFB_R5G5B5
        0x50,  // RGBFB_B5G6R5PC
        0x30,  // RGBFB_B5G5R5PC
        0x00,  // RGBFB_YUV422CGX
        0x00,  // RGBFB_YUV411
        0x00,  // RGBFB_YUV411PC
        0x00,  // RGBFB_YUV422
        0x00,  // RGBFB_YUV422PC
        0x00,  // RGBFB_YUV422PA
        0x00,  // RGBFB_YUV422PAPC
    };

    static const UBYTE DAC_ColorModes[] = {
        0x00,  // RGBFB_NONE
        0x00,  // RGBFB_CLUT
        0x80,  // RGBFB_R8G8B8
        0x80,  // RGBFB_B8G8R8
        0x50,  // RGBFB_R5G6B5PC
        0x30,  // RGBFB_R5G5B5PC
        0x70,  // RGBFB_A8R8G8B8
        0x70,  // RGBFB_A8B8G8R8
        0x70,  // RGBFB_R8G8B8A8
        0x70,  // RGBFB_B8G8R8A8
        0x50,  // RGBFB_R5G6B5
        0x30,  // RGBFB_R5G5B5
        0x50,  // RGBFB_B5G6R5PC
        0x30,  // RGBFB_B5G5R5PC
        0x00,  // RGBFB_YUV422CGX
        0x00,  // RGBFB_YUV411
        0x00,  // RGBFB_YUV411PC
        0x00,  // RGBFB_YUV422
        0x00,  // RGBFB_YUV422PC
        0x00,  // RGBFB_YUV422PA
        0x00,  // RGBFB_YUV422PAPC
    };

    REGBASE();

    UBYTE sdacMode;
    UBYTE dacMode;
    if ((format == RGBFB_CLUT) && ((bi->ModeInfo->Flags & GMF_DOUBLECLOCK) != 0)) {
        D(INFO, "Setting 8bit multiplex SDAC mode\n");
        sdacMode = 0x10;
        dacMode  = 0x11;
    } else {
        sdacMode = SDAC_ColorModes[format];
        dacMode  = DAC_ColorModes[format];
    }
    DAC_ENABLE_RS2();
    W_REG(SDAC_COMMAND, sdacMode);
    DAC_DISABLE_RS2();
    W_CR_MASK(0x67, 0xF1, dacMode);
}

BOOL InitRAMDAC(struct BoardInfo *bi)
{
    REGBASE();

    ChipData_t *cd = getChipData(bi);

    W_REG(DAC_MASK, 0xff);

    cd->ramdacOps = NULL;

    switch (cd->chipFamily) {
    case VISION864:
        // Select clock 2 (see initialization of 0x3C2).
        // This should have no effect as the SDAC is set to ignore the clock select and
        // instead produce the clock programmed through its PLLs, but keep existing init behavior.
        W_CR(0x42, 0x02);
        W_CR(0x55, 0x00);  // RS2 = 0
        if (!CheckForSDAC(bi)) {
            DFUNC(ERROR, "Unsupported RAMDAC.\n");
            return FALSE;
        }
        InitSDAC(bi);
        cd->ramdacOps = &sdac_ops;
        return TRUE;

    case VISION968:
        // Prefer RGB524 if present
        if (CheckForRGB524(bi)) {
            InitRGB524(bi);

            // Configure VRAM memory for 64 bit SID (existing InitChip logic)
            // 00 = 64-bit serial (non-interleaved) SID bus operation
            // Bit6 : Tristate PA[0..7], because we're using SID to feed the RAMDAC
            W_CR_MASK(0x66, (3 << 5) | BIT(6), (0b00 << 5) | BIT(6));

            // Serial VRAM addressing
            W_CR_MASK(0x53, BIT(5), 0);

            // Serial Access Mode 256 Words Control
            W_CR_MASK(0x58, BIT(6), BIT(6));

            // Clock programming for RGB524 is not implemented separately yet; reuse SDAC-style programming for now.
            cd->ramdacOps = &sdac_ops;
            return TRUE;
        }

        // Fallback: SDAC/GENDAC
        if (CheckForSDAC(bi)) {
            InitSDAC(bi);
            cd->ramdacOps = &sdac_ops;
            return TRUE;
        }
        DFUNC(ERROR, "No supported RAMDAC found for Vision968.\n");
        return FALSE;

    case TRIO64:
    case TRIO64PLUS:
    case TRIO64UVPLUS:
    case TRIO64V2:
        cd->ramdacOps = &integrated_ops;
        return TRUE;

    case AURORA64PLUS:
        cd->ramdacOps = &aurora64_ops;
         W_SR_MASK(0x1A, BIT(7), BIT(0));  // 3.3V RAMDAC, so we can go up to 110Mhz(?)
         // W_SR_MASK(0x1A, BIT(7), BIT(7));  // 5V RAMDAC, so we can go up to 135Mhz(?)
        return TRUE;

    case UNKNOWN:
    case VIRGE3D:
    default:
        DFUNC(ERROR, "Unsupported chip family for InitRAMDAC.\n");
        return FALSE;
    }
}

static BOOL CheckForSDAC(struct BoardInfo *bi)
{
    REGBASE();

    /* probe for S3 GENDAC or SDAC */
    /*
     * S3 GENDAC and SDAC have two fixed read only PLL clocks
     *     CLK0 f0: 25.255MHz   M-byte 0x28  N-byte 0x61
     *     CLK0 f1: 28.311MHz   M-byte 0x3d  N-byte 0x62
     * which can be used to detect GENDAC and SDAC since there is no chip-id
     * for the GENDAC.
     *
     * NOTE: for the GENDAC on a MIRO 10SD (805+GENDAC) reading PLL values
     * for CLK0 f0 and f1 always returns 0x7f (but is documented "read only")
     */

    UBYTE saveCR55, saveCR45, saveCR43, savelut[6];
    ULONG clock01, clock23;
    BOOL found = FALSE;

    // Set OLD RS2 to 0
    saveCR43 = R_CR(0x43);
    W_CR_MASK(0x43, 0x20, 0);

    // ??
    //    saveCR45 = R_CR(0x45);
    //    W_CR_MASK(0x45, 0x20, 0);

    saveCR55 = R_CR(0x55);
    // Extended RAMDAC Control Register (EX_DAC_CT) (CR55)
    // Make RS2 take on the value o CR43 (see above)
    W_CR_MASK(0x55, 0x01, 0);

    // This is done to restore the state of a RAMDAC, in case its not an SDAC
    // Read Index
    W_REG(0x3c7, 0);
    for (int i = 0; i < 2 * 3; i++) { /* save first two LUT entries */
        savelut[i] = R_REG(0x3c9);
    }
    W_REG(0x3c8, 0);
    for (int i = 0; i < 2 * 3; i++) { /* set first two LUT entries to zero */
        W_REG(0x3c9, 0);
    }

    // Force RS2 = 1
    W_CR_MASK(0x55, 0x01, 0x01);

    // Read Index
    W_REG(0x3c7, 0);
    for (int i = clock01 = 0; i < 4; i++) {
        clock01 = (clock01 << 8) | R_REG(0x3c9);
    }
    for (int i = clock23 = 0; i < 4; i++) {
        clock23 = (clock23 << 8) | R_REG(0x3c9);
    }

    // Back to regular palette access?
    W_CR_MASK(0x55, 0x01, 0);

    W_REG(0x3c8, 0);
    for (int i = 0; i < 2 * 3; i++) /* restore first two LUT entries */
        W_REG(0x3c9, savelut[i]);

    W_CR(0x55, saveCR55);

    if (clock01 == 0x28613d62 || (clock01 == 0x7f7f7f7f && clock23 != 0x7f7f7f7f)) {
        W_REG(0x3c8, 0);
        // Reading DAC Mask returns chip ID?
        R_REG(0x3c6);
        R_REG(0x3c6);
        R_REG(0x3c6);

        /* the fourth read will show the SDAC chip ID and revision */
        if ((R_REG(0x3c6) & 0xf0) == 0x70) {
            DFUNC(0, "Found S3 SDAC\n");
            saveCR43 &= ~0x02;
            saveCR45 &= ~0x20;
        } else
            DFUNC(0, "Found S3 GENDAC\n");
        {
            saveCR43 &= ~0x02;
            saveCR45 &= ~0x20;
        }
        W_REG(0x3c8, 0);

        found = TRUE;
    } else {
        DFUNC(0, "Unknown RAMDAC\n");
    }

    //    W_CR(0x45, saveCR45);
    W_CR(0x43, saveCR43);

    return found;
}

static BOOL InitSDAC(struct BoardInfo *bi)
{
    REGBASE();

    DAC_ENABLE_RS2();

    W_REG(SDAC_WR_ADR, 0x0E);  // PLL Control register
    // In the ICS 5430, f0 and f1 are fixed clocks and cannot be programmed
    W_REG(SDAC_PLL_PARAM, BIT(5) | 0x02);  // Enable internal clock select. Select CLK0 = 2 and CLK1 = fA;
    W_REG(SDAC_PLL_PARAM, 0);              // Pseudo 2nd byte write to realize the change

    DAC_DISABLE_RS2();

    return TRUE;
}

#define DAC_IDX_LO   0x3C8
#define DAC_IDX_HI   0x3C9
#define DAC_IDX_DATA 0x3C6
#define DAC_IDX_CNTL 0x3C7

static BOOL CheckForRGB524(struct BoardInfo *bi)
{
    REGBASE();

    /* probe for IBM RGB524 RAMDAC */
    /*
     * The IBM RGB524 RAMDAC has a dedicated Product Identification Code register
     * at index 0x0001 that returns 0x02.
     * */
    BOOL found = FALSE;

    // Set up for RAMDAC direct register access
    // The RGB524 uses RS[2:0] for register selection and D[7:0] for data
    DAC_ENABLE_RS2();

    // Access the Product Identification Code register (index 0x0001)
    // According to the datasheet, this register should return 0x02 for RGB524
    W_REG(DAC_IDX_HI, 0x00);
    W_REG(DAC_IDX_LO, 0x00);                // Dac Register index 0x0000
    UBYTE revision = R_REG(DAC_IDX_DATA);   // Read the Revision
    W_REG(DAC_IDX_LO, 0x01);                // Dac register index 0x0001
    UBYTE productId = R_REG(DAC_IDX_DATA);  // Read the Product ID value

    if (productId == 0x02) {
        DFUNC(0, "Found IBM RGB524 RAMDAC (Product ID: 0x%02x)\n", productId);
        found = TRUE;
    } else {
        DFUNC(0, "RGB524 not detected, Product ID: 0x%02x (expected: 0x02)\n", productId);
    }

    DAC_DISABLE_RS2();

    return found;
}

static BOOL InitRGB524(struct BoardInfo *bi)
{
    REGBASE();

    DAC_ENABLE_RS2();

    W_REG(DAC_IDX_CNTL, 0x01);  // auto-increment register indices
    W_REG(DAC_IDX_HI, 0x00);

    W_REG(DAC_IDX_LO, 0x06);  // DAC operation
    W_REG(DAC_DATA, BIT(0));  // Blanking pedestal

    W_REG(DAC_IDX_LO, 0x0a);  // Pixel Format
    W_REG(DAC_DATA, 0b011);   // 8Bit

    W_REG(DAC_IDX_LO, 0x0b);  // 8 Bit Pixel Control
    W_REG(DAC_DATA, 0);       // Indirect through Palette enabled

    W_REG(DAC_IDX_LO, 0x0c);                         // 16 Bit Pixel Control
    W_REG(DAC_DATA, (0b11 << 6) | BIT(2) | BIT(1));  // Fill low order 0 bits with high order bits and 565 mode

    W_REG(DAC_IDX_LO, 0x0d);  // 24 Bit Pixel Control
    W_REG(DAC_DATA, 0x01);    // Direct, palette bypass

    W_REG(DAC_IDX_LO, 0x0e);           // 32 Bit Pixel Control
    W_REG(DAC_DATA, BIT(2) | (0b11));  // Direct, Palette bypass

    DAC_DISABLE_RS2();
}
