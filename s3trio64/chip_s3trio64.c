#include "chip_s3trio64.h"
#include "s3ramdac.h"

#define __NOLIBBASE__

#include <clib/debug_protos.h>
#include <debuglib.h>
#include <exec/types.h>
#include <graphics/rastport.h>
#include <hardware/cia.h>
#include <libraries/pcitags.h>
#include <proto/exec.h>
#include <proto/openpci.h>

#include <SDI_stdarg.h>

#ifdef DBG
int debugLevel = CHATTY;
#endif

#if !BUILD_VISION864
#define HAS_PACKED_MMIO 1
#else
#define HAS_PACKED_MMIO 0
#endif

#define SUBSYS_STAT  0x42E8  // Read
#define SUBSYS_CNTL  0x42E8  // Write
#define ADVFUNC_CNTL 0x4AE8

#define CUR_Y          0x82E8
#define CUR_Y2         0x82EA
#define CUR_X          0x86E8
#define CUR_X2         0x86EA
#define DESTY_AXSTP    0x8AE8
#define Y2_AXSTP2      0x8AEA
#define DESTX_DIASTP   0x8EE8
#define X2             0x8EEA
#define ERR_TERM       0x92E8
#define ERR_TERM2      0x92EA
#define MAJ_AXIS_PCNT  0x96E8
#define MAJ_AXIS_PCNT2 0x96EA
#define GP_STAT        0x9AE8  // Read-only

#define CMD 0x9AE8  // Write-only

#define CMD_ALWAYS                            0x0001
#define CMD_ACROSS_PLANE                      0x0002
#define CMD_NO_LASTPIXEL                      0x0004
#define CMD_RADIAL_DRAW_DIR                   0x0008
#define CMD_DRAW_PIXELS                       0x0010
#define CMD_DRAW_DIR_MASK                     0x00e0
#define CMD_BYTE_SWAP                         0x1000
#define CMD_WAIT_CPU                          0x0100
#define CMD_BUS_SIZE_8BIT                     (0b00 << 9)
#define CMD_BUS_SIZE_16BIT                    (0b01 << 9)
#define CMD_BUS_SIZE_32BIT_MASK_32BIT_ALIGNED (0b10 << 9)
#define CMD_BUS_SIZE_32BIT_MASK_8BIT_ALIGNED  (0b11 << 9)

#define CMD_COMMAND_TYPE_MASK  0xe000
#define CMD_COMMAND_TYPE_SHIFT 13

#define CMD_TYPE_NOP       (0b000 << CMD_COMMAND_TYPE_SHIFT)
#define CMD_TYPE_LINE      (0b001 << CMD_COMMAND_TYPE_SHIFT)
#define CMD_TYPE_RECT_FILL (0b010 << CMD_COMMAND_TYPE_SHIFT)
#define CMD_TYPE_BLIT      (0b110 << CMD_COMMAND_TYPE_SHIFT)
#define CMD_TYPE_PAT_BLIT  (0b111 << CMD_COMMAND_TYPE_SHIFT)

#define CMD2                     0x9AEA  // Write-only
#define CMD2_TRAPEZOID_DIR_MASK  0x00e0
#define CMD2_TRAPEZOID_DIR_SHIFT 5

#define SHORT_STROKE 0x9EE8
// These 5 are 32bit registers, which can be accessed either by two
// 16bit writes or 32bit writes when using IO programming (does not apply to
// MMIO)
#define BKGD_COLOR 0xA2E8
#define FRGD_COLOR 0xA6E8
#define WRT_MASK   0xAAE8
#define RD_MASK    0xAEE8
#define COLOR_CMP  0xB2E8

#define BKGD_MIX 0xB6E8
#define FRGD_MIX 0xBAE8

#define CLR_SRC_BKGD_COLOR (0b00 << 5)
#define CLR_SRC_FRGD_COLOR (0b01 << 5)
#define CLR_SRC_CPU        (0b10 << 5)
#define CLR_SRC_MEMORY     (0b11 << 5)

#define RD_REG_DT 0xBEE8
// The following are accessible via RD_REG_DT, the number indicates the index
#define MIN_AXIS_PCNT 0x0
#define SCISSORS_T    0x1
#define SCISSORS_L    0x2
#define SCISSORS_B    0x3
#define SCISSORS_R    0x4

#define PIX_CNTL            0xA
#define MASK_BIT_SRC_ONE    (0b00 << 6)
#define MASK_BIT_SRC_CPU    (0b10 << 6)
#define MASK_BIT_SRC_BITMAP (0b11 << 6)

#define MULT_MISC2 0xD
#define MULT_MISC  0xE
#define READ_SEL   0xF

#define PIX_TRANS     0xE2E8
#define PIX_TRANS_EXT 0xE2EA
#define PAT_Y         0xEAE8
#define PAT_X         0xEAEA

#if HAS_PACKED_MMIO
// Packed MMIO 32bit registers
#define ALT_CURXY  0x8100
#define ALT_CURXY2 0x8104
#define ALT_STEP   0x8108
#define ALT_STEP2  0x810C
#define ALT_ERR    0x8110
#define ALT_CMD    0x8118
#define ALT_MIX    0x8134
#define ALT_PCNT   0x8148
#define ALT_PAT    0x8168
#endif

/******************************************************************************/
/*                                                                            */
/* library exports                                                                    */
/*                                                                            */
/******************************************************************************/

#if BIGENDIAN_MMIO
const char LibName[] = "S3Trio64Plus.chip";
#elif BUILD_VISION864
const char LibName[] = "S3Vision864.chip";
#else
const char LibName[] = "S3Trio3264.chip";
#endif
const char LibIdString[] = "S3Vision864/Trio32/64/64Plus Picasso96 chip driver version 1.0";

const UWORD LibVersion  = 1;
const UWORD LibRevision = 0;

// Wait For just the blitter to finish. No wait for FIFO queue empty.
static void WaitForBlitter(__REGA0(struct BoardInfo *bi))
{
    D(20, "Waiting for blitter...");
    // FIXME: ideally you'd want this interrupt driven. I.e. sleep until the HW
    // interrupt indicates its done. Otherwise, whats the point of having the
    // blitter run asynchronous to the CPU?
#if BUILD_VISION864
    REGBASE();
    while (R_IO_W(GP_STAT) & BIT(9)) {
    };
#else
    MMIOBASE();
    while (R_MMIO_W(GP_STAT) & BIT(9)) {
    };
#endif
    D(20, "done\n");
}

// Wait for blitter to finish AND FIFO queue empty.
static void WaitForIdle(__REGA0(struct BoardInfo *bi))
{
    D(20, "Waiting for idle...");
    // Here we're waiting for the GE to finish AND all FIFO slots to clear
#if BUILD_VISION864
    REGBASE();
    while ((R_IO_W(GP_STAT) & (BIT(9) | BIT(10))) != BIT(10)) {
    };
#else
    MMIOBASE();
    while ((R_MMIO_W(GP_STAT) & (BIT(9) | BIT(10))) != BIT(10)) {
    };
#endif
    D(20, "done\n");
}

static void ASM WaitBlitter(__REGA0(struct BoardInfo *bi))
{
    WaitForIdle(bi);
}

static const struct svga_pll s3trio64_pll = {3, 129, 3, 33, 0, 3, 35000, 240000, 14318};
static const struct svga_pll s3sdac_pll   = {3, 129, 3, 33, 0, 3, 60000, 270000, 14318};

// Helper function to compute frequency from PLL values
static ULONG computeKhzFromPllValue(const struct svga_pll *pll, const PLLValue_t *pllValue)
{
    ULONG f_vco = (pll->f_base * pllValue->m) / pllValue->n;
    return f_vco >> pllValue->r;
}

// Initialize PLL table for pixel clocks
void InitPixelClockPLLTable(BoardInfo_t *bi)
{
    DFUNC(VERBOSE, "", bi);

    LOCAL_SYSBASE();

    ChipData_t *cd             = getChipData(bi);
    const struct svga_pll *pll = (cd->chipFamily >= TRIO64) ? &s3trio64_pll : &s3sdac_pll;

    UWORD maxFreq    = 135;  // 135MHz max
    UWORD minFreq    = 12;   // 12MHz min
    UWORD numEntries = maxFreq - minFreq + 1;

    PLLValue_t *pllValues = AllocVec(sizeof(PLLValue_t) * numEntries, MEMF_PUBLIC);
    if (!pllValues) {
        DFUNC(ERROR, "Failed to allocate PLL table\n");
        return;
    }

    cd->pllValues    = pllValues;
    cd->numPllValues = 0;

    int lastValue = 0;
    // Generate PLL values for each frequency
    for (UWORD freq = minFreq; freq <= maxFreq; freq++) {
        UWORD m, n, r;
        int currentKhz = svga_compute_pll(pll, freq * 1000, &m, &n, &r);

        if (currentKhz >= 0) {
            pllValues[cd->numPllValues].m = m;
            pllValues[cd->numPllValues].n = n;
            pllValues[cd->numPllValues].r = r;
            cd->numPllValues++;

            DFUNC(CHATTY, "Pixelclock %03ld %09ldHz: m=%ld n=%ld r=%ld\n", (ULONG)cd->numPllValues - 1,
                  (ULONG)currentKhz * 1000, (ULONG)m, (ULONG)n, (ULONG)r);
        }
    }

    D(VERBOSE, "Initialized %ld PLL entries\n", cd->numPllValues);
}

ULONG SetMemoryClock(struct BoardInfo *bi, ULONG clockHz)
{
    REGBASE();

    USHORT m, n, r;
    UBYTE regval;

    DFUNC(10, "original Hz: %ld\n", clockHz);

    const struct svga_pll *pll = (getChipData(bi)->chipFamily >= TRIO64) ? &s3trio64_pll : &s3sdac_pll;

    int currentKhz = svga_compute_pll(pll, clockHz / 1000, &m, &n, &r);
    if (currentKhz < 0) {
        DFUNC(0, "cannot set requested pixclock, keeping old value\n");
        return clockHz;
    }

    if (getChipData(bi)->chipFamily >= TRIO64) {
        /* Set S3 clock registers */
        W_SR(0x10, (r << 5) | (n - 2));
        W_SR(0x11, m - 2);

        delayMicroSeconds(10);

        /* Activate clock - write 0, 1, 0 to seq/15 bit 5 */
        regval = R_SR(0x15) & ~BIT(5); /* | 0x80; */
        W_SR(0x15, regval);
        W_SR(0x15, regval | BIT(5));
        W_SR(0x15, regval);

        // Setting this bit to 1 improves performance for systems using an MCLK less than 57
        // MHz. For MCLK frequencies between 55 and 57 MHz, bit 7 of SR15 should also be set
        // to 1 if linear addressing is being used.
        if (clockHz >= 55000000 && clockHz <= 57000000) {
            // 2 MCLK memory writes
            W_SR_MASK(0xA, 0x80, 0x8);
            W_SR_MASK(0x15, 0x80, 0x8);
        } else {
            // 3 MCLK memory writes
            W_SR_MASK(0xA, 0x80, 0x00);
            W_SR_MASK(0x15, 0x80, 0x00);
        }
    } else {
        /* set RS2 via CR55 - I believe this switches to a second "bank" of RAMDAC registers */
        W_CR_MASK(0x55, 0x01, 0x01);

        // Clock 10 is apparently the clock used for MCLK, weirdly, though the docs say that clock fA(0x09)
        // Is the one selected at power up
        W_REG(DAC_WR_AD, 0x0A);
        W_REG(DAC_DATA, m - 2);
        W_REG(DAC_DATA, (r << 5) | (n - 2));

        W_CR_MASK(0x55, 0x01, 0x00);
    }

    return currentKhz * 1000;
}

static UWORD CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                  __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE format))
{
    // Make the bytes per row compatible with the Graphics Engine's presets
    if (width <= 320) {
        // We allow only small resolutions to have a non-Graphics Engine size.
        // These resolutions (notably 320xY) are often used in games and these games
        // assume a pitch of 320 bytes (not 640 which expansion to 640 would
        // require). Nevertheless, align to 8 bytes. We constrain all other
        // resolutions to Graphics Engine supported pitch.
        width = (width + 7) & ~7;
    } else if (width <= 640) {
        width = 640;
    } else if (width <= 800) {
        width = 800;
    } else if (width <= 1024) {
        width = 1024;
    } else if (width <= 1152) {
        width = 1152;
    } else if (width <= 1280) {
        width = 1280;
    } else if (width <= 1600) {
        width = 1600;
    } else if (width <= 2048) {
        width = 2048;
    } else {
        return 0;
    }

    UBYTE bpp = getBPP(format);

    UWORD bytesPerRow = width * bpp;

    ULONG maxHeight = 2048;
    if (height > maxHeight) {
        return 0;
    }
    return bytesPerRow;
}

static void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    REGBASE();
    LOCAL_SYSBASE();

    DFUNC(VERBOSE, "startIndex %ld, count %ld\n", (ULONG)startIndex, (ULONG)count);

    // FIXME: this should be a constant for the Trio, no need to make it dynamic
    const UBYTE bppDiff = 2;  // 8 - bi->BitsPerCannon;

    // This may noty be interrupted, so DAC_WR_AD remains set throughout the
    // function
    Disable();

    W_REG(DAC_WR_AD, startIndex);

    struct CLUTEntry *entry = &bi->CLUT[startIndex];

    // Do not print these individual register writes as it takes ages
    for (UWORD c = 0; c < count; ++c) {
        writeReg(RegBase, DAC_DATA, entry->Red >> bppDiff);
        writeReg(RegBase, DAC_DATA, entry->Green >> bppDiff);
        writeReg(RegBase, DAC_DATA, entry->Blue >> bppDiff);
        ++entry;
    }

    Enable();

    return;
}

static void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    REGBASE();

    DFUNC(VERBOSE, "\n");
#if BUILD_VISION864
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
    if (format < RGBFB_MaxFormats) {
        UBYTE sdacMode;
        if ((format == RGBFB_CLUT) && ((bi->ModeInfo->Flags & GMF_DOUBLECLOCK) != 0)) {
            D(5, "Setting 8bit multiplex SDAC mode\n");
            sdacMode = 0x10;
        } else {
            sdacMode = SDAC_ColorModes[format];
        }
        W_CR_MASK(0x55, 0x01, 0x01);
        W_REG(0x3c6, sdacMode);
        W_CR_MASK(0x55, 0x01, 0x00);
    }
#endif
#if BUILD_VISION864
    static const UBYTE DAC_ColorModes[] = {
        0x00,  // RGBFB_NONE
        0x00,  // RGBFB_CLUT
        0x70,  // RGBFB_R8G8B8
        0x70,  // RGBFB_B8G8R8
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
#else
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
#endif

    if (format < RGBFB_MaxFormats) {
        UBYTE dacMode;

        if ((format == RGBFB_CLUT) && ((bi->ModeInfo->Flags & GMF_DOUBLECLOCK) != 0)) {
            D(5, "Setting 8bit multiplex DAC mode\n");
            // pixel multiplex and invert DCLK; This way it results in double-inversion and thus VCLK/PCLK on the RAMDAC
            // are in-phase with its internal double-clocked ICLK
            dacMode = 0x11;
        } else {
            dacMode = DAC_ColorModes[format];
        }
        W_CR_MASK(0x67, 0xF2, dacMode);

        // XFree86 does this... not sure if I need it?
        //        W_CR(0x6D, 0x02); // Blank Delay
    }

    return;
}

static INLINE REGARGS UWORD ToScanLines(UWORD y, UWORD modeFlags)
{
    if (modeFlags & GMF_DOUBLESCAN)
        y *= 2;
    if (modeFlags & GMF_INTERLACE)
        y /= 2;
    return y;
}

static INLINE REGARGS UWORD AdjustBorder(UWORD x, BOOL border, UWORD defaultX)
{
    if (!border || x == 0)
        x = defaultX;
    return x;
}

static void ASM SetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    REGBASE();

    BOOL isInterlaced;
    UBYTE depth;
    UBYTE modeFlags;
    UWORD hTotal;
    UWORD ScreenWidth;

    DFUNC(VERBOSE,
          "W %ld, H %ld, HTotal %ld, HBlankSize %ld, HSyncStart %ld, HSyncSize "
          "%ld, "
          "\nVTotal %ld, VBlankSize %ld,  VSyncStart %ld ,  VSyncSize %ld\n",
          (ULONG)mi->Width, (ULONG)mi->Height, (ULONG)mi->HorTotal, (ULONG)mi->HorBlankSize, (ULONG)mi->HorSyncStart,
          (ULONG)mi->HorSyncSize, (ULONG)mi->VerTotal, (ULONG)mi->VerBlankSize, (ULONG)mi->VerSyncStart,
          (ULONG)mi->VerSyncSize);

    bi->ModeInfo = mi;
    bi->Border   = border;

    WaitBlitter(bi);

    // Disable Clock Doubling
#if !BUILD_VISION864
    W_SR_MASK(0x15, 0x50, 0);
    W_SR_MASK(0x18, 0x80, 0);
#else
    W_SR_MASK(0x01, 0x04, 0x00);
#endif

    hTotal       = mi->HorTotal;
    ScreenWidth  = mi->Width;
    modeFlags    = mi->Flags;
    isInterlaced = (modeFlags & GMF_INTERLACE) != 0;

    depth = mi->Depth;
    if (depth <= 8) {
        if ((border == 0) || ((mi->HorBlankSize == 0 || (mi->VerBlankSize == 0)))) {
            D(INFO, "8-Bit Mode, NO Border\n");
            // Bit 5 BDR SEL - Blank/Border Select
            // 1 = BLANK is active during entire display inactive period (no border)
            W_CR_MASK(0x33, 0x20, 0x20);
        } else {
            D(INFO, "8-Bit Mode, Border\n");
            // Bit 5 BDR SEL - Blank/Border Select
            // o = BLANK active time is defined by CR2 and CR3
            W_CR_MASK(0x33, 0x20, 0x0);
        }

        // Disable horizontal counter double mode used for 16/32bit modes
        W_CR_MASK(0x43, 0x80, 0x00);

        if (modeFlags & GMF_DOUBLECLOCK) {
            DFUNC(INFO, "Double-Clock Mode\n");
#if BUILD_VISION864
            hTotal      = hTotal / 2;
            ScreenWidth = ScreenWidth / 2;
//      W_SR_MASK(0x01, 0x04, 0x04);
#else
            // CLKSVN Control 2 Register (SR15)
            // Bit 4 DCLK/2 - Divide DCLK by 2
            // Either this bit or bit 6 of this register must be set to 1 for clock
            // doubled RAMDAC operation (mode 0001).
            W_SR_MASK(0x15, 0x10, 0x10);

            // RAMDAC/CLKSVN Control Register (SR18)
            // Bit 7 CLKx2 - Enable clock doubled mode
            // 1 = RAMDAC clock doubled mode (0001) enabled
            // This bit must be set to 1 when mode 0001 is specified in bits 7-4
            // of CR67 or SRC. Either bit 4 or bit 6 of SR15 must also be set to 1.
            W_SR_MASK(0x18, 0x80, 0x80);
#endif
        }
    } else if (depth <= 16) {
        D(INFO, "16-Bit Mode, No Border\n");
        // Bit 5 BDR SEL - Blank/Border Select
        // o = BLANK active time is defined by CR2 and CR3
        W_CR_MASK(0x33, 0x20, 0x0);

        // Double all horizontal parameters.
        W_CR_MASK(0x43, 0x80, 0x80);
        border = 0;
    } else {
        D(INFO, "24-Bit Mode, No Border\n");
        // Bit 5 BDR SEL - Blank/Border Select
        // 0 = BLANK active time is defined by CR2 and CR3
        W_CR_MASK(0x33, 0x20, 0x0);

#if BUILD_VISION864
        // Double all horizontal parameters.
        W_CR_MASK(0x43, 0x80, 0x80);
        // And double again. We need x4 "dot clocks"
        hTotal      = hTotal * 2;
        ScreenWidth = ScreenWidth * 2;
#else
        // Reset doubling all horizontal parameters.
        W_CR_MASK(0x43, 0x80, 0x00);
#endif
        border = 0;
    }

#define ADJUST_HBORDER(x) AdjustBorder(x, border, 8)
#define ADJUST_VBORDER(y) AdjustBorder(y, border, 1);
#define TO_CLKS(x)        ((x) >> 3)
#define TO_SCANLINES(y)   ToScanLines((y), modeFlags)

    {
        // Horizontal Total (CRO)
        UWORD hTotalClk = TO_CLKS(hTotal) - 5;
        D(INFO, "Horizontal Total %ld\n", (ULONG)hTotalClk);
        W_CR_OVERFLOW1(hTotalClk, 0x0, 0, 8, 0x5D, 0, 1);
        // FIXME: is this correct?
        {
            // Interlace Retrace Start Register (lL_RTSTART) (CR3C) ???
            W_CR(0x3c, hTotalClk >> 1);
        }
    }
    {
        // Horizontal Display End Register (H_D_END) (CR1)
        // One less than the total number of displayed characters
        // This register defines the number of character clocks for one line of the
        // active display. Bit 8 of this value is bit 1 of CR5D.
        UWORD hDisplayEnd = TO_CLKS(ScreenWidth) - 1;
        D(INFO, "Display End %ld\n", (ULONG)hDisplayEnd);
        W_CR_OVERFLOW1(hDisplayEnd, 0x1, 0, 8, 0x5D, 1, 1);
    }

    UWORD hBorderSize = ADJUST_HBORDER(mi->HorBlankSize);
    UWORD hBlankStart = TO_CLKS(ScreenWidth + hBorderSize) - 1;
    {
        // AR11 register defines the overscan or border color displayed on the CRT
        // screen. The overscan color is displayed when both BLANK and DE (Display
        // Enable) signals are inactive.

        // Start Horizontal Blank Register (S_H_BLNKI (CR2))
        D(INFO, "Horizontal Blank Start %ld\n", (ULONG)hBlankStart);
        W_CR_OVERFLOW1(hBlankStart, 0x2, 0, 8, 0x5d, 2, 1);
    }

    {
        // End Horizontal Blank Register (E_H_BLNKI (CR3)
        UWORD hBlankEnd = TO_CLKS(hTotal - hBorderSize) - 1;
        D(INFO, "Horizontal Blank End %ld\n", (ULONG)hBlankEnd);
        //    W_CR_OVERFLOW2(hBlankEnd, 0x3, 0, 5, 0x5, 7, 1, 0x5d, 3, 1);
        W_CR_OVERFLOW1(hBlankEnd, 0x3, 0, 5, 0x5, 7, 1);
    }

    UWORD hSyncStart = TO_CLKS(mi->HorSyncStart + ScreenWidth);
    {
        // Start Horizontal Sync Position Register (S_H_SV _PI (CR4)
        D(INFO, "HSync start %ld\n", (ULONG)hSyncStart);
        W_CR_OVERFLOW1(hSyncStart, 0x4, 0, 8, 0x5d, 4, 1);
    }

    UWORD endHSync = hSyncStart + TO_CLKS(mi->HorSyncSize);
    {
        // End Horizontal Sync Position Register (E_H_SY_P) (CR5)
        D(INFO, "HSync End %ld\n", (ULONG)endHSync);
        W_CR_MASK(0x5, 0x1f, endHSync);
        //    W_CR_OVERFLOW1(endHSync, 0x5, 0, 5, 0x5d, 5, 1);
    }

    // Start Display FIFO Register (DT _EX_POS) (CR3B)
    // FIFO filling cannot begin again
    // until the scan line position defined by the Start
    // Display FIFO register (CR3B), which is normally
    // programmed with a value 5 less than the value
    // programmed in CRO (horizontal total). This provides time during the
    // horizontal blanking period for RAM refresh and hardware cursor fetch.
    {
        UWORD startDisplayFifo = TO_CLKS(hTotal) - 5 - 5;
        if (endHSync > startDisplayFifo) {
            startDisplayFifo = endHSync + 1;
        }
        D(INFO, "Start Display Fifo %ld\n", (ULONG)startDisplayFifo);
        W_CR_OVERFLOW1(startDisplayFifo, 0x3b, 0, 8, 0x5d, 6, 1);
    }

    {
        // Vertical Total (CR6)
        UWORD vTotal = TO_SCANLINES(mi->VerTotal) - 2;
        D(INFO, "VTotal %ld\n", (ULONG)vTotal);
        W_CR_OVERFLOW3(vTotal, 0x6, 0, 8, 0x7, 0, 1, 0x7, 5, 1, 0x5e, 0, 1);
    }

    UWORD vBlankSize = ADJUST_VBORDER(mi->VerBlankSize);
    {
        // Vertical Display End register (CR12)
        UWORD vDisplayEnd = TO_SCANLINES(mi->Height) - 1;
        D(INFO, "Vertical Display End %ld\n", (ULONG)vDisplayEnd);
        W_CR_OVERFLOW3(vDisplayEnd, 0x12, 0, 8, 0x7, 1, 1, 0x7, 6, 1, 0x5e, 1, 1);
    }

    {
        // Start Vertical Blank Register (SVB) (CR15)
        UWORD vBlankStart = mi->Height;
        if ((modeFlags & GMF_DOUBLESCAN) != 0) {
            vBlankStart = vBlankStart * 2;
        }
        // FIXME: the blankSize is unaffected by double scan, but affected by
        // interlaced?
        vBlankStart = ((vBlankStart + vBlankSize) >> isInterlaced) - 1;
        D(INFO, "VBlank Start %ld\n", (ULONG)vBlankStart);
        W_CR_OVERFLOW3(vBlankStart, 0x15, 0, 8, 0x7, 3, 1, 0x9, 5, 1, 0x5e, 2, 1);
    }

    {
        // End Vertical Blank Register (EVB) (CR16)
        UWORD vBlankEnd = mi->VerTotal;
        if ((modeFlags & GMF_DOUBLESCAN) != 0) {
            vBlankEnd = vBlankEnd * 2;
        }
        vBlankEnd = ((vBlankEnd - vBlankSize) >> isInterlaced) - 1;
        D(6, "VBlank End %ld\n", (ULONG)vBlankEnd);
        // FIXME: the blankSize is unaffected by double scan, but affected by
        // interlaced?
        W_CR(0x16, vBlankEnd);
    }

    UWORD vRetraceStart = TO_SCANLINES(mi->Height + mi->VerSyncStart);
    {
        // Vertical Retrace Start Register (VRS) (CR10)
        // FIXME: here VsyncStart is in lines, not scanlines, while mi->VerBlankSize
        // is in scanlines?
        D(INFO, "VRetrace Start %ld\n", (ULONG)vRetraceStart);
        W_CR_OVERFLOW3(vRetraceStart, 0x10, 0, 8, 0x7, 2, 1, 0x7, 7, 1, 0x5e, 4, 1);
    }

    {
        // Vertical Retrace End Register (VRE) (CR11) Bits 3-0 VERTICAL RETRACE END
        // Value = least significant 4 bits of the scan line counter value at which
        // VSYNC goes in active. To obtain this value, add the desired VSYNC pulse
        // width in scan line units to the CR10 value, also in scan line units. The
        // 4 1east significant bits of this sum are programmed into this field.
        // This allows a maximum VSYNC pulse width of 15 scan line units.
        UWORD vRetraceEnd = vRetraceStart + TO_SCANLINES(mi->VerSyncSize);
        D(INFO, "VRetrace End %ld\n", (ULONG)vRetraceEnd);
        W_CR_MASK(0x11, 0x0F, vRetraceEnd);
    }

    // Enable Interlace
    {
        UBYTE interlace = R_CR(0x42) & 0xdf;
        if (isInterlaced) {
            interlace = interlace | 0x20;
        }
        W_CR(0x42, interlace);
    }

    // Enable Doublescan
    {
        UBYTE dblScan = R_CR(0x9) & 0x7f;
        if ((modeFlags & GMF_DOUBLESCAN) != 0) {
            dblScan = dblScan | 0x80;
        }
        W_CR(0x9, dblScan);
    }

    // Vsync/HSync polarity
    {
        UBYTE polarities = 0;
        if ((modeFlags & GMF_HPOLARITY) != 0) {
            polarities = polarities | 0x40;
        }
        if ((modeFlags & GMF_VPOLARITY) != 0) {
            polarities = polarities | 0x80;
        }
        W_MISC_MASK(0xC0, polarities);
    }

    //  {

    //    static const ULONG mcyclesPerEntry = 9;
    //    static const ULONG mcyclesPerPage = 2;
    //    static const ULONG fifoEntries = 16;
    //    static const ULONG fifoWidth = 64 /*Bits*/ /8; // Bytes // Trio in 1MB
    //    config has only 32bits width

    //    ULONG entries = 1;
    //    ULONG pageModeCycle = 10;

    //    // Find M parameter for MCLK (how many clocks are given back to CPU
    //    memory
    //    // access etc before handing it back to FIFO)
    //    ULONG memClock = bi->MemoryClock;
    //    UBYTE depth = mi->Depth;
    //    if (depth <= 4) {
    //      memClock /= 10;
    //    } else if (depth <= 8) {
    //      memClock /= 10;
    //    } else if (depth <= 16) {
    //      memClock /= 20;
    //    } else {
    //      memClock /= 41;
    //    }
    ////    mclkM = ((memClk / (mi->PixelClock / 10000)) / 10) -
    ////             bi->MemoryClock / 2400000) - 9 >> 1;

    //    // FIXME: check formula
    //    memClock = (((memClock ) / (mi->PixelClock * 1000))) - bi->MemoryClock /
    //    2400 000) - 9 >> 1;

    //    // FIXME: the resulting value is 6 bit, but here we're throwing away the
    //    // topmost bit, limiting us to just 32 cycles
    //    if (memClock > 0x1f) {
    //      memClock = 0x1f;
    //    }
    //    if (memClock < 0x3) {
    //      memClock = 0x3;
    //    }

    //    // M PARAMETER
    //    // 6-bit Value = maximum number of MCLKs that the LPB, CPU and Graphics
    //    // Engine can use to access memory before giving up control of the
    //    memory
    //    // bus. See Section 7.5 for more information. Bit 2 is the high order
    //    bit of
    //    // this value.
    //    // FIXME: on Trio64/32 this is a 5bit value, on Trio64+ 6bit
    //    W_CR(0x54, memClock << 3);
    //  }

    W_CR_MASK(0x54, 0xFC, 0x18);

    {
        // Extended Memory Control 3 Register (EXT-MCTL-3) (CR60)
        // Bits 7-0 N(DISP-FETCH-PAGE) - N Parameter
        // Value = Number of MCLKs allocated to Streams Processor FIFO filling
        // before control of the memory bus is relinquished. See Section 7.5 for
        // more information.
        W_CR(0x60, 0xff);
    }
    // Backward Compatibility 3 Register (BKWD_3) (CR34)
    // Bit 4 ENB SFF - Enable Start Display FIFO Fetch Register(CR3B)
    W_CR(0x34, 0x10);

    {
        LOCAL_SYSBASE();
        Disable();

        // Atttribute Controller Index register to AR11 while preserving "Bit 5 ENB
        // PLT - Enable Video Display"

        R_REG(0x3DA);
        // write AR11 = 0 Border Color Register
        W_AR(0x11, 0);

        // Re-enable video out
        W_REG(ATR_AD, 0x20);
        R_REG(0x3DA);

        Enable();
    }
}

static void ASM SetPanning(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD4(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset),
                           __REGD7(RGBFTYPE format))
{
    REGBASE();
    LOCAL_SYSBASE();

    DFUNC(VERBOSE,
          "mem 0x%lx, width %ld, height %ld, xoffset %ld, yoffset %ld, "
          "format %ld\n",
          memory, (ULONG)width, (ULONG)height, (LONG)xoffset, (LONG)yoffset, (ULONG)format);

    LONG panOffset;
    UWORD pitch;
    ULONG memOffset;

    bi->XOffset = xoffset;
    bi->YOffset = yoffset;
    memOffset   = (ULONG)memory - (ULONG)bi->MemoryBase;

    switch (format) {
    case RGBFB_NONE:
        pitch     = width >> 3;  // ?? can planar modes even be accessed?
        panOffset = (ULONG)yoffset * (width >> 3) + (xoffset >> 3);
        break;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
    case RGBFB_A8R8G8B8:
    case RGBFB_A8B8G8R8:
    case RGBFB_R8G8B8A8:
    case RGBFB_B8G8R8A8:
        pitch     = width * 4;
        panOffset = (yoffset * width + xoffset) * 4;
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
    case RGBFB_B5G6R5PC:
    case RGBFB_B5G5R5PC:
    case RGBFB_YUV422CGX:
    case RGBFB_YUV422:
    case RGBFB_YUV422PC:
    case RGBFB_YUV422PA:
    case RGBFB_YUV422PAPC:
        pitch     = width * 2;
        panOffset = (yoffset * width + xoffset) * 2;
        break;
    default:
        // RGBFB_CLUT:
        pitch     = width;
        panOffset = yoffset * width + xoffset;
        break;
    }

    WaitBlitter(bi);

    pitch /= 8;
    panOffset = (panOffset + memOffset) / 4;

    D(5, "panOffset 0x%lx, pitch %ld dwords\n", panOffset, (ULONG)pitch);
    // Start Address Low Register (STA(L)) (CRD)
    // Start Address High Register (STA(H)) (CRC)
    // Extended System Control 3 Register (EXT-SCTL-3)(CR69)
    W_CR_OVERFLOW2_ULONG(panOffset, 0xd, 0, 8, 0xc, 0, 8, 0x69, 0, 4);

    //  assert(pitchInDoublwWords < 0xFFFF);

    // Offset Register (SCREEN-OFFSET) (CR13)
    //  Bits 7-0 LOGICAL SCREEN WIDTH
    //      10-bit Value = quantity that is multiplied by 2 (word mode), 4
    //      (doubleword mode) or 8 (quadword mode) to specify the difference
    //      between the starting byte addresses of two consecutive scan lines.
    //      This register contains the least significant 8 bits of this value.
    //      The addressing mode is specified by bit 6 of CR14 and bit 3 of CR17.
    //      Setting bit 3 of CR31 to 1 forces doubleword mode.

    // This register specifies the amount to be added to the internal linear
    // counter when advancing from one screen row to the next. The addition is
    // performed whenever the internal row address counter advances past the
    // maximum row address value, indicating that all the scan lines in the
    // present row have been displayed. The Row Offset register is programmed in
    // terms of CPU-addressed words per scan line, counted as either words or
    // doublewords, depending on whether byte or word mode is in effect. If the
    // CRTC Mode register is set to select byte mode, the Row Offset register is
    // programmed with a word value. So for a 640-pixel (80-byte) wide graphics
    // display, a value of 80/2 = 40 (28 hex) would normally be programmed, where
    // 80 ts the number of bytes per scan line. If the CRTC Mode register is set
    // to select word mode, then the Row Offset register is programmed with a
    // doubleword, rather than a word, value. For instance, in 80-column text
    // mode, a value of 160/4=40 (28 hex) would be programmed, because from the
    // CPU-addressing side, each character requires 2 linear bytes (character code
    // byte and attribute byte), for a total of 160 (AO hex) bytes per row.

    W_CR_OVERFLOW1(pitch, 0x13, 0, 8, 0x51, 4, 2);

    // Bits 5-4 of CR51 are extension bits 9-8 of this register. If these bits are
    // OOb, bit 2 of CR43 is extension bit 8 of this register.
    //  W_CR_MASK(0x43, 0x04, (pitch >> 6) & 0x04);

    Disable();

    R_REG(0x3DA);  // Reset AFF flip-flop // FIXME: why?

    Enable();
    return;
}

static APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi), __REGA1(APTR mem), __REGD7(RGBFTYPE format))
{
#if !BUILD_VISION864
    if (getChipData(bi)->chipFamily >= TRIO64PLUS) {
        switch (format) {
        case RGBFB_A8R8G8B8:
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
            // Redirect to Big Endian Linear Address Window.
            return mem + 0x2000000;
            break;
        default:
            return mem;
            break;
        }
    }
#endif
    return mem;
}

static ULONG ASM GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    if (format == RGBFB_NONE)
        return (ULONG)0;

    // These formats can always reside in the Little Endian Window.
    // We never need to change any aperture setting for them
    ULONG compatible = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;

#if !BUILD_VISION864
    if (getChipData(bi)->chipFamily >= TRIO64PLUS) {
        switch (format) {
        case RGBFB_A8R8G8B8:
            // In Big Endian aperture, configured for byte swapping in long word
            compatible |= RGBFF_A8R8G8B8;
            break;
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
            // In Big Endian aperture, configured for byte swapping in words only
            compatible |= RGBFF_R5G6B5 | RGBFF_R5G5B5;
            break;
        }
    }
#endif
    return compatible;
}

static void ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    // Clocking Mode Register (ClK_MODE) (SR1)
    REGBASE();

    DFUNC(VERBOSE, " state %ld\n", (ULONG)state);

    W_SR_MASK(0x01, 0x20, (~(UBYTE)state & 1) << 5);
    //  R_REG(0x3DA);
    //  W_REG(ATR_AD, 0x20);
    //  R_REG(0x3DA);
}

static ULONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi),
                                   __REGD0(ULONG pixelClock), __REGD7(RGBFTYPE RGBFormat))
{
    DFUNC(VERBOSE, "ModeInfo 0x%lx pixelclock %ld, format %ld\n", mi, pixelClock, (ULONG)RGBFormat);

    const ChipData_t *cd       = getChipData(bi);
    const struct svga_pll *pll = (cd->chipFamily >= TRIO64) ? &s3trio64_pll : &s3sdac_pll;

    mi->Flags &= ~GMF_ALWAYSBORDER;
    if (0x3ff < mi->VerTotal) {
        mi->Flags |= GMF_ALWAYSBORDER;
    }

    mi->Flags &= ~GMF_DOUBLESCAN;
    if (mi->Height < 400) {
        mi->Flags |= GMF_DOUBLESCAN;
    }

    // Figure out if we can/need to make use of double clocking
    mi->Flags &= ~GMF_DOUBLECLOCK;
#if !BUILD_VISION864
    // Enable Double Clock for 8Bit modes when required pixelclock exceeds 80Mhz
    // I couldn't get this to work on the VISION864
    if (RGBFormat == RGBFB_CLUT || RGBFormat == RGBFB_NONE) {
        if (pixelClock > 67500000) {
            D(VERBOSE, "Applying pixel multiplex clocking\n")
            mi->Flags |= GMF_DOUBLECLOCK;
        }
    }
#endif
#if BUILD_VISION864
    if (getBPP(RGBFormat) >= 3) {
        // In 24/32bit modes, it takes 2 clock cycles to transfer one pixel to the RAMDAC,
        // and for the RAMDAC to output this pixel. Therefore, we need to double VCLK
        D(VERBOSE, "Applying 2x clocking\n");
        pixelClock *= 2;
    }
#endif

    ULONG targetFreqKhz = pixelClock / 1000;

    // Find the best matching PLL entry using binary search
    UWORD upper     = cd->numPllValues - 1;
    ULONG upperFreq = computeKhzFromPllValue(pll, &cd->pllValues[upper]);
    UWORD lower     = 0;
    ULONG lowerFreq = computeKhzFromPllValue(pll, &cd->pllValues[lower]);

    while (lower + 1 < upper) {
        UWORD middle     = (upper + lower) / 2;
        ULONG middleFreq = computeKhzFromPllValue(pll, &cd->pllValues[middle]);

        // DFUNC(INFO, "l %ld @ %ld, u %ld @ %ld, middle %ld @ %ld\n", (ULONG)lower, (ULONG)lowerFreq, (ULONG)upper,
        //       (ULONG)upperFreq, (ULONG)middle, (ULONG)middleFreq);

        if (targetFreqKhz > middleFreq) {
            lower     = middle;
            lowerFreq = middleFreq;
        } else {
            upper     = middle;
            upperFreq = middleFreq;
        }
    }

    // DFUNC(INFO, "Candidate fequencies: lower %ld @ %ld, upper %ld @ %ld, target %ld\n", (ULONG)lower,
    // (ULONG)lowerFreq,
    //       (ULONG)upper, (ULONG)upperFreq, (ULONG)targetFreqKhz);

    // Return the best match between upper and lower
    if (targetFreqKhz - lowerFreq > upperFreq - targetFreqKhz) {
        lower     = upper;
        lowerFreq = upperFreq;
    }

    mi->PixelClock = lowerFreq * 1000;

    PLLValue_t pllValues = cd->pllValues[lower];

    //     if (mi->Flags & GMF_DOUBLECLOCK) {
    //         pllValues.r += 1;
    // #if DBG
    //         ULONG halfFreq = computeKhzFromPllValue(pll, &pllValues);
    //         D(VERBOSE, "true DCLK Hz: %ld\n", halfFreq * 1000);
    // #endif
    //     }

    DFUNC(VERBOSE, "Reporting pixelclock Hz: %ld, index: %ld,  M:%ld N:%ld R:%ld \n\n", mi->PixelClock, (ULONG)lower,
          (ULONG)pllValues.m, (ULONG)pllValues.n, (ULONG)pllValues.r);

    // Store PLL values in the format expected by SetClock
    mi->pll1.Numerator   = pllValues.m - 2;
    mi->pll2.Denominator = (pllValues.r << 5) | (pllValues.n - 2);

#if !BUILD_VISION864
    if (mi->Flags & GMF_DOUBLECLOCK) {
        // Bit 7 CLKx2 - Enable clock doubled mode
        // 0 = RAMDAC clock doubled mode (0001) disabled
        // 1 = RAMDAC clock doubled mode (0001) enabled
        // This bit must be set to 1 when mode 0001 is specified in bits 7-4 of CR67 or SRC.
        // Either bit 4 or bit 6 of SR15 must also be set to 1. This bit has the same function as
        // SR18_7. It allows enabling of clock doubling at the same time as the PLL parameters
        // are programmed, resulting in more controlled VCO operation.
        mi->pll2.Denominator |= 0x80;  // Set bit 7 to indicate double clocking;
    }
#endif
    return lower;  // Return the index into the PLL table
}

static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD7(RGBFTYPE format))
{
    DFUNC(INFO, "Index: %ld\n", index);

    const ChipData_t *cd       = getChipData(bi);
    const struct svga_pll *pll = (cd->chipFamily >= TRIO64) ? &s3trio64_pll : &s3sdac_pll;

    if (index >= cd->numPllValues) {
        DFUNC(ERROR, "Invalid pixel clock index %ld (max %ld)\n", index, cd->numPllValues - 1);
        return 0;
    }

    ULONG frequency = computeKhzFromPllValue(pll, &cd->pllValues[index]);
    return frequency * 1000;  // Convert kHz to Hz
}

static void ASM SetClock(__REGA0(struct BoardInfo *bi))
{
    DFUNC(INFO, "\n");

    struct ModeInfo *mi = bi->ModeInfo;

    D(INFO, "SetClock: PixelClock %ldHz\n", mi->PixelClock);

#if DBG
    const ChipData_t *cd         = getChipData(bi);
    const struct svga_pll *pll   = (cd->chipFamily >= TRIO64) ? &s3trio64_pll : &s3sdac_pll;
    const PLLValue_t selectedPll = {
        .m = (mi->pll1.Numerator + 2), .n = (mi->pll2.Denominator & 0x1F) + 2, .r = (mi->pll2.Denominator >> 5) & 0x03};
    ULONG frequency = computeKhzFromPllValue(pll, &selectedPll);
    D(VERBOSE, "         Actual PixelClock %ldHz M: %ld N: %ld, R: %ld\n", frequency * 1000, (ULONG)selectedPll.m,
      (ULONG)selectedPll.n, (ULONG)selectedPll.r);
#endif

    REGBASE();

#if !BUILD_VISION864
    /* Set S3 DCLK clock registers */
    // Clock-Doubling will be enabled by SetGC
    W_SR(0x13, mi->pll1.Numerator);    // write M
    W_SR(0x12, mi->pll2.Denominator);  // write N and R

    delayMicroSeconds(100);

    /* Activate clock - write 0, 1, 0 to seq/15 bit 5 */
    UBYTE regval = R_SR(0x15) & ~BIT(5); /* | 0x80; */
    W_SR(0x15, regval);
    W_SR(0x15, regval | BIT(5));
    W_SR(0x15, regval);
#else
    W_CR_MASK(0x55, 0x01, 0x01);

    W_REG(DAC_WR_AD, 2);
    W_REG(DAC_DATA, mi->pll1.Numerator);
    W_REG(DAC_DATA, mi->pll2.Denominator);

    W_CR_MASK(0x55, 0x01, 0x00);
#endif
}

static INLINE void ASM SetMemoryModeInternal(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
#if !BUILD_VISION864
    REGBASE();

    if (getChipData(bi)->chipFamily >= TRIO64PLUS)  // Trio64+?
    {
        if (getChipData(bi)->MemFormat == format) {
            return;
        }
        getChipData(bi)->MemFormat = format;

        WaitBlitter(bi);

        // Setup the linear window CPU access such that the below formats will be
        // converted to the actual framebuffer format on write/read
        switch (format) {
        case RGBFB_A8R8G8B8:
            // swap all the bytes within a double word
            W_CR_MASK(0x53, 0x06, 0x04);
            break;
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
            // Just swap the bytes within a word
            W_CR_MASK(0x53, 0x06, 0x02);
            break;
        default:
            W_CR_MASK(0x53, 0x06, 0x00);
            break;
        }
    }
#endif
    return;
}

static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
#if !BUILD_VISION864
    __asm __volatile("\t movem.l d0-d1/a0-a1,-(sp)\n"
                     : /* no result */
                     :
                     :);

    SetMemoryModeInternal(bi, format);

    __asm __volatile("\t movem.l (sp)+,d0-d1/a0-a1\n"
                     : /* no result */
                     :
                     : "d0", "d1", "a0", "a1");
#endif
}

static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    __asm __volatile("\t movem.l d0-d1/a0-a1,-(sp)\n"
                     : /* no result */
                     :
                     :);

    //  SetWriteMaskInternal(bi, format);

    __asm __volatile("\t movem.l (sp)+,d0-d1/a0-a1\n"
                     : /* no result */
                     :
                     : "d0", "d1", "a0", "a1");
}

static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask)) {}

static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask)) {}

static BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL expected))
{
    REGBASE();
    return (R_REG(0x3DA) & 0x08) != 0;
}

static void WaitVerticalSync(__REGA0(struct BoardInfo *bi)) {}

static void ASM SetDPMSLevel(__REGA0(struct BoardInfo *bi), __REGD0(ULONG level))
{
    //  DPMS_ON,      /* Full operation                             */
    //  DPMS_STANDBY, /* Optional state of minimal power reduction  */
    //  DPMS_SUSPEND, /* Significant reduction of power consumption */
    //  DPMS_OFF      /* Lowest level of power consumption */

    static const UBYTE DPMSLevels[4] = {0x00, 0x90, 0x60, 0x50};

    REGBASE();
    W_SR_MASK(0xD, 0xF0, DPMSLevels[level]);
}

static void ASM SetSplitPosition(__REGA0(struct BoardInfo *bi), __REGD0(SHORT splitPos))
{
    REGBASE();
    DFUNC(VERBOSE, "%ld\n", (ULONG)splitPos);

    bi->YSplit = splitPos;
    if (!splitPos) {
        splitPos = 0x7ff;
    } else {
        if (bi->ModeInfo->Flags & GMF_DOUBLESCAN) {
            splitPos *= 2;
        }
    }
    W_CR_OVERFLOW3((UWORD)splitPos, 0x18, 0, 8, 0x7, 4, 1, 0x9, 6, 1, 0x5e, 6, 1);
}

static void ASM SetSpritePosition(__REGA0(struct BoardInfo *bi), __REGD0(WORD xpos), __REGD1(WORD ypos),
                                  __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE, "\n");
    REGBASE();

    bi->MouseX = xpos;
    bi->MouseY = ypos;

    WORD spriteX = xpos - bi->XOffset;
    WORD spriteY = ypos - bi->YOffset + bi->YSplit;
    if (bi->ModeInfo->Flags & GMF_DOUBLESCAN) {
        spriteY *= 2;
    }

#if BUILD_VISION864
    // It seems that the sprite coordinates are not pixel coordinates but
    // clock counts.
    // On Vision864 the 24/32 bit modes it take 2 DCLKs per pixel.
    if (getBPP(fmt) > 2) {
        spriteX *= 2;
    }
#endif

    WORD offsetX = 0;
    if (spriteX < 0) {
        if (spriteX > -64)
            offsetX = -spriteX;
        else
            offsetX = 64;
        spriteX = 0;
    }
    WORD offsetY = 0;
    if (spriteY < 0) {
        if (spriteY > -64)
            offsetY = -spriteY;
        else
            offsetY = 64;
        spriteY = 0;
    }

    D(VERBOSE, "SpritePos X: %ld 0x%lx, Y: %ld 0x%lx\n", (LONG)spriteX, (ULONG)spriteX, (LONG)spriteY, (ULONG)spriteY);
    // should we be able to handle negative values and use the offset registers
    // for that?
    W_CR_OVERFLOW1(spriteX, 0x47, 0, 8, 0x46, 0, 8);
    W_CR_OVERFLOW1(spriteY, 0x49, 0, 8, 0x48, 0, 8);
    W_CR(0x4e, offsetX & 63);
    W_CR(0x4f, offsetY & 63);
}

static void ASM SetSpriteImage(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE, "\n");

    // FIXME: need to set temporary memory format?
    // No, MouseImage should be in little endian window and not affected
#if BUILD_VISION864 && 0
    // Weird, the Vision864 docs describe the layout as:
    //  "The AND and the XOR cursor image bitmaps are 512 bytes each. These are stored in consecutive bytes
    //  of off-screen display memory, 512 AND bytes followed by 512 XOR bytes. "
    //  But that doesn't work. Instead the Trio32/64 shape programming (AND/XOR images are word-interleaved)
    //  works.
    const UWORD *image = bi->MouseImage + 2;
    UWORD *cursorAND   = (UWORD *)bi->MouseImageBuffer;
    UWORD *cursorXOR   = (UWORD *)(bi->MouseImageBuffer + 512);
    for (UWORD y = 0; y < bi->MouseHeight; ++y) {
        // first 16 bit
        UWORD plane0 = *image++;
        UWORD plane1 = *image++;

        UWORD andMask = ~plane0;  // AND mask
        UWORD xorMask = plane1;   // XOR mask
        *cursorAND++  = andMask;
        *cursorXOR++  = xorMask;
        // padding, should result in  screen color
        for (UWORD p = 0; p < 3; ++p) {
            *cursorAND++ = 0xFFFF;
            *cursorXOR++ = 0x0000;
        }
    }
    // Pad the rest of the cursor image
    for (UWORD y = bi->MouseHeight; y < 64; ++y) {
        for (UWORD p = 0; p < 4; ++p) {
            *cursorAND++ = 0xFFFF;
            *cursorXOR++ = 0x0000;
        }
    }
#else
    const UWORD *image = bi->MouseImage + 2;
    UWORD *cursor      = (UWORD *)bi->MouseImageBuffer;
    for (UWORD y = 0; y < bi->MouseHeight; ++y) {
        // first 16 bit
        UWORD plane0 = *image++;
        UWORD plane1 = *image++;

        UWORD andMask = ~plane0;  // AND mask
        UWORD xorMask = plane1;   // XOR mask
        *cursor++     = andMask;
        *cursor++     = xorMask;
        // padding, should result in  screen color
        for (UWORD p = 0; p < 3; ++p) {
            *cursor++ = 0xFFFF;
            *cursor++ = 0x0000;
        }
    }
    // Pad the rest of the cursor image
    for (UWORD y = bi->MouseHeight; y < 64; ++y) {
        for (UWORD p = 0; p < 4; ++p) {
            *cursor++ = 0xFFFF;
            *cursor++ = 0x0000;
        }
    }
#endif

    LOCAL_SYSBASE();
    CacheClearU();
}

static void ASM SetSpriteColor(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE index), __REGD1(UBYTE red),
                               __REGD2(UBYTE green), __REGD3(UBYTE blue), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE, "Index %ld, Red %ld, Green %ld, Blue %ld\n", (ULONG)index, (ULONG)red, (ULONG)green, (ULONG)blue);
    REGBASE();
    LOCAL_SYSBASE();

    if (index != 0 && index != 2)
        return;

    UBYTE reg = 0;

#if BUILD_VISION864
    if (fmt == RGBFB_CLUT) {
        if (index == 0) {
            reg = 0x0F;
        } else {
            reg = 0x0E;
        }
    } else
#endif
    {
        if (index == 0) {
            reg = 0x4B;
        } else {
            reg = 0x4A;
        }
    }

    R_CR(0x45);  // Reset "Graphics Cursor Stack"
    switch (fmt) {
    case RGBFB_NONE:
    case RGBFB_CLUT: {
        UBYTE paletteEntry;
        if (index == 0) {
            paletteEntry = 17;  // Cursor Palette entry is fixed
        } else {
            paletteEntry = 19;
        }
        W_CR(reg, paletteEntry);
        W_REG(CRTC_DATA, paletteEntry);
        W_REG(CRTC_DATA, paletteEntry);
        W_REG(CRTC_DATA, paletteEntry);
    } break;
    case RGBFB_B8G8R8A8:
    case RGBFB_A8R8G8B8: {
        W_CR(reg, blue);  // No Conversion needed for 24bit RGB
        W_REG(CRTC_DATA, green);
        W_REG(CRTC_DATA, red);
    } break;
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G5B5: {
        UBYTE a = (blue >> 3) | ((green << 2) & 0xe);  // 16bit, just need to write the first two byte
        UBYTE b = (green >> 5) | ((red >> 1) & ~0x3);
        W_CR(reg, a);
        W_REG(CRTC_DATA, b);
    } break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G6B5: {
        UBYTE a = (blue >> 3) | ((green << 3) & 0xe);  // // 16bit, just need to write the first two byte
        UBYTE b = (green >> 5) | (red & 0xf8);
        W_CR(reg, a);
        W_REG(CRTC_DATA, b);
    } break;
    }
}

static BOOL ASM SetSprite(__REGA0(struct BoardInfo *bi), __REGD0(BOOL activate), __REGD7(RGBFTYPE RGBFormat))
{
    DFUNC(VERBOSE, "\n");
    REGBASE();

#if BUILD_VISION864
    if (getChipData(bi)->chipFamily <= VISION864) {
        UBYTE bpp = getBPP(RGBFormat);
        if (bpp < 3) {
            W_CR_MASK(0x45, 0x0C, 0b0000);
        } else {
            // Set Cursor pixel mode to 24/32bit
            W_CR_MASK(0x45, 0x0C, 0b0100);
        }
    }
#endif

    W_CR_MASK(0x45, 0x01, activate ? 0x01 : 0x00);

    if (activate) {
        SetSpriteColor(bi, 0, bi->CLUT[17].Red, bi->CLUT[17].Green, bi->CLUT[17].Blue, bi->RGBFormat);
        SetSpriteColor(bi, 1, bi->CLUT[18].Red, bi->CLUT[18].Green, bi->CLUT[18].Blue, bi->RGBFormat);
        SetSpriteColor(bi, 2, bi->CLUT[19].Red, bi->CLUT[19].Green, bi->CLUT[19].Blue, bi->RGBFormat);
    }

    return TRUE;
}

static INLINE void REGARGS WaitFifo(struct BoardInfo *bi, BYTE numSlots)
{
    if (!numSlots) {
        return;
    }

    DFUNC(20, "Waiting for %ld slots...", (ULONG)numSlots);
    //  assert(numSlots <= 13);

    // The FIFO bits are split into two groups, 7-0 and 15-11
    // Bit 7=0 (means 13 slots are available, bit 15 represents at least 5 slots
    // available)

    BYTE testBit = 7 - (numSlots - 1);
    testBit &= 0xF;  // handle wrap-around

    D(20, " testbit: %ld... ", (ULONG)testBit);

#if BUILD_VISION864
    // On Vision864 the MMIO registers are write-only, thus reading the status through IO
    REGBASE();
    while (R_IO_W(GP_STAT) & (1 << testBit)) {
    };
#else
    MMIOBASE();
    while (1) {
        UWORD gpStat = R_MMIO_W(GP_STAT);
        D(20, " gpstat: %lx,", (ULONG)gpStat);
        if (!(gpStat & (1 << testBit)))
            break;
    };
#endif

    D(20, "done\n");
}

#define MByte(x) ((x) * (1024 * 1024))
static INLINE void REGARGS getGESegmentAndOffset(ULONG memOffset, WORD bytesPerRow, UBYTE bpp, UWORD *segment,
                                                 UWORD *xoffset, UWORD *yoffset)
{
    *segment = (memOffset >> 20) & 7;

    ULONG segOffset = memOffset & 0xFFFFF;
    *yoffset        = segOffset / bytesPerRow;
    *xoffset        = (segOffset % bytesPerRow) / bpp;

#ifdef DBG
    if (*segment > 0) {
        D(10, "segment %ld, xoff %ld, yoff %ld, memoffset 0x%08lx\n", (ULONG)*segment, (ULONG)*xoffset, (ULONG)*yoffset,
          memOffset);
    }
#endif
}

static INLINE ULONG REGARGS getMemoryOffset(struct BoardInfo *bi, APTR memory)
{
    ULONG offset = (ULONG)memory - (ULONG)bi->MemoryBase;
    return offset;
}

static INLINE BOOL setCR50(struct BoardInfo *bi, UWORD bytesPerRow, UBYTE bpp)
{
    REGBASE();

    ChipData_t *cd = getChipData(bi);
    if (cd->GEbytesPerRow == bytesPerRow && cd->GEbpp == bpp) {
        return TRUE;
    }

    UWORD width     = bytesPerRow / bpp;
    UBYTE CR31_1    = 0;
    UBYTE CR50_76_0 = 0;
    // Make the bytes per row compatible with the Graphics Engine's presets
    if (width == 640) {
        CR50_76_0 = 0b01000000;
    } else if (width == 800) {
        CR50_76_0 = 0b10000000;
    } else if (width == 1024) {
        CR50_76_0 = 0b00000000;
    } else if (width == 1152) {
        CR50_76_0 = 0b00000001;
    } else if (width == 1280) {
        CR50_76_0 = 0b11000000;
    } else if (width == 1600) {
        CR50_76_0 = 0b10000001;
    } else if (width == 2048) {
        CR31_1    = (1 << 1);
        CR50_76_0 = 0b00000000;
    } else {
        DFUNC(0, "Width %ld unsupported by Graphics Engine, choosing unaccelerated  path\n", (ULONG)width);
        return FALSE;  // reserved
    }

    cd->patternCacheKey = ~0;  // Force pattern update because pitch may have changed
    cd->GEbytesPerRow   = bytesPerRow;
    cd->GEbpp           = bpp;

    getGESegmentAndOffset(getMemoryOffset(bi, cd->patternVideoBuffer), bytesPerRow, bpp, &cd->pattSegment, &cd->pattX,
                          &cd->pattY);
    cd->pattX = (cd->pattX + 7) & ~7;  // Align to 8 pixel boundary
    cd->pattY = (cd->pattY + 7) & ~7;
    D(CHATTY, "pattSeg %ld, pattX %ld, pattY %ld, bytesPerRow %ld\n", (ULONG)cd->pattSegment, (ULONG)cd->pattX,
      (ULONG)cd->pattY, (ULONG)cd->GEbytesPerRow);
#if DBG
    if (cd->pattX & 7 || cd->pattY & 7) {
        D(WARN, "pattern not on 8 pixel boundary %ld,%ld\n", (ULONG)cd->pattX, (ULONG)cd->pattY);
    }
#endif

    WaitBlitter(bi);

    W_CR_MASK(0x50, 0xF1, CR50_76_0 | ((bpp - 1) << 4));
    W_CR_MASK(0x31, (1 << 1), CR31_1);

    return TRUE;
}

static INLINE ULONG REGARGS PenToColor(ULONG pen, RGBFTYPE fmt)
{
    switch (fmt) {
    case RGBFB_B8G8R8A8:
        pen = swapl(pen);
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
        pen = swapw(pen);
        // Fallthrough
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        pen = makeDWORD(pen, pen);
        break;
    case RGBFB_CLUT:
        pen |= (pen << 8);
        pen = makeDWORD(pen, pen);
        break;
    default:
        break;
    }
    return pen;
}

static INLINE void REGARGS DrawModeToMixMode(UBYTE drawMode, UWORD *frgdMix, UWORD *bkgdMix)
{
    UWORD writeMode = (drawMode & COMPLEMENT) ? MIX_NOT_CURRENT : MIX_NEW;
    UWORD f, g;
    switch (drawMode & 1) {
    case JAM1:
        f = writeMode;
        g = MIX_CURRENT;
        break;
    case JAM2:
        f = writeMode;
        g = writeMode;
        break;
    }
    f |= CLR_SRC_FRGD_COLOR;
    g |= CLR_SRC_BKGD_COLOR;
    if (drawMode & INVERSVID) {
        // Swap the foreground and background
        UWORD t = f;
        f       = g;
        g       = t;
    }
    *frgdMix = f;
    *bkgdMix = g;
}

static INLINE void REGARGS setMix(struct BoardInfo *bi, UWORD frgdMix, UWORD bkgdMix)
{
    MMIOBASE();
#if HAS_PACKED_MMIO
    W_MMIO_L(ALT_MIX, makeDWORD(frgdMix, bkgdMix));
#else
    W_MMIO_W(FRGD_MIX, frgdMix);
    W_MMIO_W(BKGD_MIX, bkgdMix);
#endif
}

static INLINE void REGARGS SetDrawMode(struct BoardInfo *bi, ULONG FgPen, ULONG BgPen, UBYTE DrawMode, RGBFTYPE format)
{
    ChipData_t *cd = getChipData(bi);

    if (cd->GEfgPen != FgPen || cd->GEbgPen != BgPen || cd->GEdrawMode != DrawMode || cd->GEFormat != format) {
        cd->GEfgPen    = FgPen;
        cd->GEbgPen    = BgPen;
        cd->GEdrawMode = DrawMode;
        cd->GEFormat   = format;

        UWORD frgdMix, bkgdMix;
        DrawModeToMixMode(DrawMode, &frgdMix, &bkgdMix);
        ULONG fgPen = PenToColor(FgPen, format);
        ULONG bgPen = PenToColor(BgPen, format);

        WaitFifo(bi, 6);

        REGBASE();
        W_IO_L(FRGD_COLOR, fgPen);
        W_IO_L(BKGD_COLOR, bgPen);

        setMix(bi, frgdMix, bkgdMix);
    }
}

static INLINE void REGARGS SetGEWriteMask(struct BoardInfo *bi, UBYTE mask, RGBFTYPE fmt, BYTE waitFifoSlots)
{
    REGBASE();
    ChipData_t *cd = getChipData(bi);

    if (fmt != RGBFB_CLUT && cd->GEmask != 0xFF) {
        // 16/32 bit modes ignore the mask
        cd->GEmask = 0xFF;
        WaitFifo(bi, waitFifoSlots + 2);
        W_IO_L(WRT_MASK, 0xFFFFFFFF);
    } else {
        // 8bit modes use the mask
        if (cd->GEmask != mask) {
            cd->GEmask = mask;

            WaitFifo(bi, waitFifoSlots + 2);

            UWORD wmask = mask;
            wmask |= (wmask << 8);
            W_IO_L(WRT_MASK, makeDWORD(wmask, wmask));
        } else {
            WaitFifo(bi, waitFifoSlots);
        }
    }
}

static INLINE void REGARGS setBlitSrcPosAndSize(struct BoardInfo *bi, UWORD x, UWORD y, UWORD w, UWORD h)
{
    MMIOBASE();
#if HAS_PACKED_MMIO
    W_MMIO_L(ALT_CURXY, makeDWORD(x, y));
    W_MMIO_L(ALT_PCNT, makeDWORD(w - 1, h - 1));
#else
    W_MMIO_W(CUR_X, x);
    W_MMIO_W(CUR_Y, y);
    W_MMIO_W(MAJ_AXIS_PCNT, w - 1);
    W_BEE8(MIN_AXIS_PCNT, h - 1);
#endif
}

static INLINE void REGARGS setBlitDestPos(struct BoardInfo *bi, UWORD dstX, UWORD dstY)
{
    MMIOBASE();
#if HAS_PACKED_MMIO
    W_MMIO_L(ALT_STEP, makeDWORD(dstX, dstY));
#else
    W_MMIO_W(DESTX_DIASTP, dstX);
    W_MMIO_W(DESTY_AXSTP, dstY);
#endif
}

#define TOP_LEFT     (0b101 << 5)
#define TOP_RIGHT    (0b100 << 5)
#define BOTTOM_LEFT  (0b001 << 5)
#define BOTTOM_RIGHT (0b000 << 5)
#define POSITIVE_X   (0b001 << 5)
#define POSITIVE_Y   (0b100 << 5)
#define Y_MAJOR      (0b010 << 5)

static void ASM FillRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                         __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(ULONG pen),
                         __REGD5(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\npen %08lx, mask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)pen, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    MMIOBASE();

    UBYTE bpp = getBPP(fmt);
    if (!bpp || !setCR50(bi, ri->BytesPerRow, bpp)) {
        DFUNC(1, "Fallback to FillRectDefault\n")
        bi->FillRectDefault(bi, ri, x, y, width, height, pen, mask, fmt);
        return;
    }

    UWORD seg;
    UWORD xoffset;
    UWORD yoffset;
    getGESegmentAndOffset(getMemoryOffset(bi, ri->Memory), ri->BytesPerRow, bpp, &seg, &xoffset, &yoffset);

    x += xoffset;
    y += yoffset;

#ifdef DBG
    if ((x > (1 << 11)) || (y > (1 << 11))) {
        KPrintF("X %ld or Y %ld out of range\n", (ULONG)x, (ULONG)y);
    }
#endif

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != FILLRECT) {
        cd->GEOp = FILLRECT;

        WaitFifo(bi, 2);
        W_BEE8(PIX_CNTL, MASK_BIT_SRC_ONE);
        W_MMIO_W(FRGD_MIX, CLR_SRC_FRGD_COLOR | MIX_NEW);
    }

    SetGEWriteMask(bi, mask, fmt, 0);

    if (cd->GEfgPen != pen || cd->GEFormat != fmt) {
        cd->GEfgPen  = pen;
        cd->GEFormat = fmt;
        pen          = PenToColor(pen, fmt);

        WaitFifo(bi, 8);
        REGBASE();
        W_IO_L(FRGD_COLOR, pen);
    } else {
        WaitFifo(bi, 6);
    }

    // This could/should get chached as well
    W_BEE8(MULT_MISC2, seg << 4);

    setBlitSrcPosAndSize(bi, x, y, width, height);

    UWORD cmd = CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT;

    W_MMIO_W(CMD, cmd);
}

static void ASM InvertRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                           __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                           __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    MMIOBASE();

    UBYTE bpp = getBPP(fmt);
    if (!bpp || !setCR50(bi, ri->BytesPerRow, bpp)) {
        DFUNC(1, "Fallback to InvertRectDefault\n")
        bi->InvertRectDefault(bi, ri, x, y, width, height, mask, fmt);
        return;
    }

    UWORD seg;
    UWORD xoffset;
    UWORD yoffset;
    getGESegmentAndOffset(getMemoryOffset(bi, ri->Memory), ri->BytesPerRow, bpp, &seg, &xoffset, &yoffset);

    x += xoffset;
    y += yoffset;

#ifdef DBG
    if ((x > (1 << 11)) || (y > (1 << 11))) {
        KPrintF("X %ld or Y %ld out of range\n", (ULONG)x, (ULONG)y);
    }
#endif

    ChipData_t *cd = getChipData(bi);
    if (cd->GEOp != INVERTRECT) {
        cd->GEOp = INVERTRECT;

        WaitFifo(bi, 2);

        W_BEE8(PIX_CNTL, MASK_BIT_SRC_ONE);
        W_MMIO_W(FRGD_MIX, CLR_SRC_MEMORY | MIX_NOT_CURRENT);
    }

    SetGEWriteMask(bi, mask, fmt, 6);

    // This could/should get chached as well
    W_BEE8(MULT_MISC2, seg << 4);

    setBlitSrcPosAndSize(bi, x, y, width, height);

    W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT);
}

static void ASM BlitRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD srcX),
                         __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                         __REGD5(WORD height), __REGD6(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE,
          "\nx1 %ld, y1 %ld, x2 %ld, y2 %ld, w %ld, \n"
          "h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt,
          (ULONG)ri->BytesPerRow, (ULONG)ri->Memory);

    MMIOBASE();

    UBYTE bpp = getBPP(fmt);
    if (!bpp || !setCR50(bi, ri->BytesPerRow, bpp)) {
        DFUNC(1, "Fallback to BlitRectDefault\n")
        bi->BlitRectDefault(bi, ri, srcX, srcY, dstX, dstY, width, height, mask, fmt);
        return;
    }

    UWORD seg;
    WORD xoffset;
    WORD yoffset;
    getGESegmentAndOffset(getMemoryOffset(bi, ri->Memory), ri->BytesPerRow, bpp, &seg, &xoffset, &yoffset);

    srcX += xoffset;
    srcY += yoffset;
    dstX += xoffset;
    dstY += yoffset;

    WORD dx = dstX - srcX;
    WORD dy = dstY - srcY;

    UWORD dir = POSITIVE_X | POSITIVE_Y;

    // FIXME: do we really need to check for overlap?
    // Is it not equally fast to adjust the blit direction each time?
    //  BOOL overlapX = !(width <= dx || width <= -dx);
    //  BOOL overlapY = !(height <= dy || height <= -dy);
    //  if (overlapX && overlapY)
    {
        // rectangles overlap, figure out which direction to blit
        if (dstX > srcX) {
            dir &= ~POSITIVE_X;
            srcX = srcX + width - 1;
            dstX = dstX + width - 1;
        }
        if (dstY > srcY) {
            dir &= ~POSITIVE_Y;
            srcY = srcY + height - 1;
            dstY = dstY + height - 1;
        }
    }

    ChipData_t *cd = getChipData(bi);
    if (cd->GEOp != BLITRECT) {
        cd->GEOp = BLITRECT;

        WaitFifo(bi, 2);

        W_BEE8(PIX_CNTL, MASK_BIT_SRC_ONE);
        W_MMIO_W(FRGD_MIX, CLR_SRC_MEMORY | MIX_NEW);
    }

    SetGEWriteMask(bi, mask, fmt, 8);

    W_BEE8(MULT_MISC2, seg << 4 | seg);

    setBlitSrcPosAndSize(bi, srcX, srcY, width, height);
    setBlitDestPos(bi, dstX, dstY);

    W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_BLIT | CMD_DRAW_PIXELS | dir);
}

const static UWORD minTermToMix[16] = {
    // MinTerm
    MIX_ZERO,                     // 0000
    MIX_NOT_CURRENT_AND_NOT_NEW,  // 0001  (!dst ^ !src)
    MIX_CURRENT_AND_NOT_NEW,      // 0010  (dst ^ !src)
    MIX_NOT_NEW,                  // 0011  (!dst ^ !src) v (dst ^ !src)
    MIX_NOT_CURRENT_AND_NEW,      // 0100  (!dst ^ src)
    MIX_NOT_CURRENT,              // 0101  (!dst ^ src) v (!dst ^ !src)
    MIX_CURRENT_XOR_NEW,          // 0110  (!dst ^ src) v (dst ^ !src)
    MIX_NOT_CURRENT_OR_NOT_NEW,   // 0111  (!dst ^ src) v (dst ^ !src) v (!dst ^ !src)
    MIX_CURRENT_AND_NEW,          // 1000  (dst ^ src)
    MIX_NOT_CURRENT_XOR_NEW,      // 1001  (!dst ^ !src) v (dst ^ src)
    MIX_CURRENT,                  // 1010  (dst ^ src) v (dst ^ !src)
    MIX_CURRENT_OR_NOT_NEW,       // 1011  (dst ^ src) v (dst ^ !src) v (!dst ^ !src)
    MIX_NEW,                      // 1100  (dst ^ src) v (!dst ^ src)
    MIX_NOT_CURRENT_OR_NEW,       // 1101  (dst ^ src) v (!dst ^ src) v (!dst ^ !src)
    MIX_CURRENT_OR_NEW,           // 1110  (dst ^ src) v (!dst ^ src) v (dst ^ !src)
    MIX_ONE,                      // 1111  (!dst ^ !src) v (dst ^ !src) v (!dst ^ src) v (dst ^ src)
};

static void ASM BlitRectNoMaskComplete(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *sri),
                                       __REGA2(struct RenderInfo *dri), __REGD0(WORD srcX), __REGD1(WORD srcY),
                                       __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                                       __REGD5(WORD height), __REGD6(UBYTE opCode), __REGD7(RGBFTYPE format))
{
    DFUNC(VERBOSE,
          "\nx1 %ld, y1 %ld, x2 %ld, y2 %ld, w %ld, \n"
          "h %ld\nminTerm 0x%lx fmt %ld\n"
          "sri->bytesPerRow %ld, sri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)opCode, (ULONG)format,
          (ULONG)sri->BytesPerRow, (ULONG)sri->Memory);

    MMIOBASE();

    UWORD bytesPerRow = dri->BytesPerRow > sri->BytesPerRow ? dri->BytesPerRow : sri->BytesPerRow;
    UBYTE bpp         = getBPP(format);
    if (!bpp || !setCR50(bi, bytesPerRow, bpp)) {
        DFUNC(1, "fallback to BlitRectNoMaskCompleteDefault\n");
        bi->BlitRectNoMaskCompleteDefault(bi, sri, dri, srcX, srcY, dstX, dstY, width, height, opCode, format);
        return;
    }

    ChipData_t *cd = getChipData(bi);
    if (cd->GEOp != BLITRECTNOMASKCOMPLETE) {
        cd->GEOp       = BLITRECTNOMASKCOMPLETE;
        cd->GEmask     = 0xFF;
        cd->GEdrawMode = 0xFF;  // invalidate minterm cache

        WaitFifo(bi, 3);
        W_BEE8(PIX_CNTL, MASK_BIT_SRC_ONE);

        REGBASE();
        W_IO_L(WRT_MASK, 0xFFFFFFFF);
    }

    if (cd->GEdrawMode != opCode) {
        cd->GEdrawMode = opCode;

        WaitFifo(bi, 1);
        W_MMIO_W(FRGD_MIX, CLR_SRC_MEMORY | minTermToMix[opCode]);
    }

    if (sri->BytesPerRow == dri->BytesPerRow) {
        WORD xoffset;
        WORD yoffset;
        UWORD segDst;
        getGESegmentAndOffset(getMemoryOffset(bi, dri->Memory), sri->BytesPerRow, bpp, &segDst, &xoffset, &yoffset);

        dstX += xoffset;
        dstY += yoffset;

        UWORD segSrc;
        getGESegmentAndOffset(getMemoryOffset(bi, sri->Memory), sri->BytesPerRow, bpp, &segSrc, &xoffset, &yoffset);

        srcX += xoffset;
        srcY += yoffset;

        WORD dx = dstX - srcX;
        WORD dy = dstY - srcY;

        UWORD dir = POSITIVE_X | POSITIVE_Y;

        // FIXME: do we really need to check for overlap?
        // Is it not equally fast to adjust the blit direction each time?
        //  BOOL overlapX = !(width <= dx || width <= -dx);
        //  BOOL overlapY = !(height <= dy || height <= -dy);
        //  if (segSrc == segDst && overlapX && overlapY)
        {
            // rectangles overlap, figure out which direction to blit
            if (dstX > srcX) {
                dir &= ~POSITIVE_X;
                srcX = srcX + width - 1;
                dstX = dstX + width - 1;
            }
            if (dstY > srcY) {
                dir &= ~POSITIVE_Y;
                srcY = srcY + height - 1;
                dstY = dstY + height - 1;
            }
        }

        WaitFifo(bi, 8);

        W_BEE8(MULT_MISC2, (segSrc << 4) | segDst);

        setBlitSrcPosAndSize(bi, srcX, srcY, width, height);
        setBlitDestPos(bi, dstX, dstY);

        W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_BLIT | CMD_DRAW_PIXELS | dir);
    } else if (sri->BytesPerRow < dri->BytesPerRow) {
        WORD xoffset;
        WORD yoffset;
        UWORD segDst;
        getGESegmentAndOffset(getMemoryOffset(bi, dri->Memory), dri->BytesPerRow, bpp, &segDst, &xoffset, &yoffset);

        dstX += xoffset;
        dstY += yoffset;

        UBYTE *srcMem = (UBYTE *)sri->Memory;
        srcMem += srcY * sri->BytesPerRow + srcX * bpp;
        ULONG memOffset = getMemoryOffset(bi, srcMem);

        WaitFifo(bi, 2);

        for (WORD h = 0; h < height; ++h) {
            WORD x;
            WORD y;
            UWORD segSrc;
            getGESegmentAndOffset(memOffset, dri->BytesPerRow, bpp, &segSrc, &x, &y);

            WaitFifo(bi, 8);
            W_BEE8(MULT_MISC2, (segSrc << 4) | segDst);

            setBlitSrcPosAndSize(bi, x, y, width, 1);
            setBlitDestPos(bi, dstX, dstY + h);

            W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_BLIT | CMD_DRAW_PIXELS | TOP_LEFT);

            memOffset += sri->BytesPerRow;
        }
    } else {
        WORD xoffset;
        WORD yoffset;
        UWORD segSrc;
        getGESegmentAndOffset(getMemoryOffset(bi, sri->Memory), sri->BytesPerRow, bpp, &segSrc, &xoffset, &yoffset);

        srcX += xoffset;
        srcY += yoffset;

        UBYTE *dstMem = (UBYTE *)dri->Memory;
        dstMem += dstY * dri->BytesPerRow + dstX * bpp;
        ULONG memOffset = getMemoryOffset(bi, dstMem);

        for (WORD h = 0; h < height; ++h) {
            WORD x;
            WORD y;
            UWORD segDst;
            getGESegmentAndOffset(memOffset, sri->BytesPerRow, bpp, &segDst, &x, &y);

            WaitFifo(bi, 8);
            W_BEE8(MULT_MISC2, (segSrc << 4) | segDst);

            setBlitSrcPosAndSize(bi, srcX, srcY + h, width, 1);
            setBlitDestPos(bi, x, y);

            W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_BLIT | CMD_DRAW_PIXELS | TOP_LEFT);

            memOffset += dri->BytesPerRow;
        }
    }
}

static void ASM BlitTemplate(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                             __REGA2(struct Template *template), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                             __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    UBYTE bpp = getBPP(fmt);
    if (!bpp || !setCR50(bi, ri->BytesPerRow, bpp)) {
        DFUNC(1, "fallback to BlitTemplateDefault\n");
        bi->BlitTemplateDefault(bi, ri, template, x, y, width, height, mask, fmt);
        return;
    }

    UWORD seg;
    UWORD xoffset;
    UWORD yoffset;
    getGESegmentAndOffset(getMemoryOffset(bi, ri->Memory), ri->BytesPerRow, bpp, &seg, &xoffset, &yoffset);

    x += xoffset;
    y += yoffset;

#ifdef DBG
    if ((x > (1 << 11)) || (y > (1 << 11))) {
        KPrintF("X %ld or Y %ld out of range\n", (ULONG)x, (ULONG)y);
    }
#endif

    MMIOBASE();

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITTEMPLATE) {
        cd->GEOp = BLITTEMPLATE;

        // Invalidate the pen and drawmode caches
        cd->GEdrawMode = 0xFF;

        WaitFifo(bi, 1);

        W_BEE8(PIX_CNTL, MASK_BIT_SRC_CPU);
    }

    SetDrawMode(bi, template->FgPen, template->BgPen, template->DrawMode, fmt);
    SetGEWriteMask(bi, mask, fmt, 6);

    // This could/should get chached as well
    W_BEE8(MULT_MISC2, seg << 4);

    setBlitSrcPosAndSize(bi, x, y, width, height);

    // Make sure, no blitter operation is still running before we start feeding PIX_TRANS
    WaitForBlitter(bi);

    W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT | CMD_ACROSS_PLANE | CMD_WAIT_CPU |
                      CMD_BUS_SIZE_32BIT_MASK_32BIT_ALIGNED);

    // FIXME: there's no promise that template->Memory and template->BytesPerRow
    // are 32bit aligned. This might either be slower than it could be on 030+ or
    // just crashing on 68k.
    const UBYTE *bitmap = (const UBYTE *)template->Memory;
    bitmap += (template->XOffset / 32) * 4;
    UWORD dwordsPerLine = (width + 31) / 32;
    UBYTE rol           = template->XOffset % 32;
    WORD bitmapPitch    = template->BytesPerRow;
    if (!rol) {
        for (UWORD y = 0; y < height; ++y) {
            for (UWORD x = 0; x < dwordsPerLine; ++x) {
                W_MMIO_L(PIX_TRANS, ((const ULONG *)bitmap)[x]);
            }
            bitmap += bitmapPitch;
        }
    } else {
        for (UWORD y = 0; y < height; ++y) {
            for (UWORD x = 0; x < dwordsPerLine; ++x) {
                ULONG left  = ((const ULONG *)bitmap)[x] << rol;
                ULONG right = ((const ULONG *)bitmap)[x + 1] >> (32 - rol);

                W_MMIO_L(PIX_TRANS, (left | right));
            }
            bitmap += bitmapPitch;
        }
    }
}

static void ASM BlitPattern(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                            __REGA2(struct Pattern *pattern), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                            __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    MMIOBASE();

    UBYTE bpp = getBPP(fmt);
    if (!bpp || !setCR50(bi, ri->BytesPerRow, bpp)) {
        DFUNC(1, "fallback to BlitPatternDefault\n");
        bi->BlitPatternDefault(bi, ri, pattern, x, y, width, height, mask, fmt);
        return;
    }

    UWORD seg;
    UWORD xoffset;
    UWORD yoffset;
    getGESegmentAndOffset(getMemoryOffset(bi, ri->Memory), ri->BytesPerRow, bpp, &seg, &xoffset, &yoffset);

    x += xoffset;
    y += yoffset;

#ifdef DBG
    if ((x > (1 << 11)) || (y > (1 << 11))) {
        KPrintF("X %ld or Y %ld out of range\n", (ULONG)x, (ULONG)y);
    }
#endif

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITPATTERN) {
        cd->GEOp = BLITPATTERN;

        // Invalidate the pen and drawmode caches
        cd->GEdrawMode = 0xFF;
        cd->patternCacheKey &= ~0x80000000;

        // Make sure, no blitter operation is still running before we start feeding PIX_TRANS
        WaitForBlitter(bi);
        W_BEE8(PIX_CNTL, MASK_BIT_SRC_CPU);
    }

    SetGEWriteMask(bi, mask, fmt, 6);

    // First, figure out if the new pattern would actually fit into an 8x8 mono pattern.
    // Then we can use the hardware pattern registers, which are much faster.
    // If not, upload the pattern to video memory and use that as mono blit source.
    // We cache the last pattern to avoid re-uploading it if it didn't change.
    UWORD patternHeight        = 1 << pattern->Size;
    const UWORD *sysMemPattern = (const UWORD *)pattern->Memory;
    UWORD *cachedPattern       = cd->patternCacheBuffer;

    // Try to avoid wait-for-idle by first checking if the pattern changed.
    // I'm not expecting huge patterns, so this will hopefully be fast
    BOOL patternChanged = FALSE;
    BOOL is8x8          = (patternHeight <= 8);
    for (UWORD i = 0; i < patternHeight; ++i) {
        UWORD row = sysMemPattern[i];
        // Compare new pattern with last one uploaded
        if (row != cachedPattern[i]) {
            cachedPattern[i] = row;
            patternChanged   = TRUE;
        }
        // Check if upper half and lower half of the 16bit pattern row are identical,
        // so the pattern width is essentially 8bit
        if ((UBYTE)(row >> 8) != (UBYTE)row) {
            is8x8 = FALSE;
        }
    }

    BOOL was8x8 = (cd->patternCacheKey & 0x80000000) != 0;

    if (is8x8) {
        // The Trio64 8x8 mono patttern cannot be offset directly.
        // Instead, its "destination aligned". So in order to offset the pattern, we
        // need to manually rotate it here.
        UBYTE pattOffX = (UBYTE)((x - pattern->XOffset) & 7);
        UBYTE pattOffY = (UBYTE)((y - pattern->YOffset) & 7);

        ULONG pattCacheKey = (pattOffX << 16) | (pattOffY << 8) | pattern->Size | 0x80000000;
        if (pattCacheKey != cd->patternCacheKey) {
            cd->patternCacheKey = pattCacheKey;
            patternChanged      = TRUE;
        }

        if (patternChanged) {
            // replicate the 8xN pattern to 8x8
            ULONG pat0;
            ULONG pat1;

            // Build the 8x8 pattern in the two registers
            // Source patterns that are smaller than 8 in height will be extended to height 8
            switch (pattern->Size) {
            case 0:
                // our pattern data is already 16bit, with the upper half and lower half determined to be identical
                pat0 = cachedPattern[0] | (cachedPattern[0] << 16);
                pat1 = pat0;
                break;
            case 1:
                pat0 = (cachedPattern[0] & 0xFF00) | ((cachedPattern[1] & 0xFF));
                pat0 |= (pat0 << 16);
                pat1 = pat0;
                break;
            case 2:
                pat0 = ((cachedPattern[0] & 0xFF00) << 16) | ((cachedPattern[1] & 0xFF00) << 8) |
                       (cachedPattern[2] & 0xFF00) | (cachedPattern[3] & 0xFF);
                pat1 = pat0;
                break;
            case 3:
                pat0 = ((cachedPattern[0] & 0xFF00) << 16) | ((cachedPattern[1] & 0xFF00) << 8) |
                       (cachedPattern[2] & 0xFF00) | (cachedPattern[3] & 0xFF);
                pat1 = ((cachedPattern[4] & 0xFF00) << 16) | ((cachedPattern[5] & 0xFF00) << 8) |
                       (cachedPattern[6] & 0xFF00) | (cachedPattern[7] & 0xFF);
                break;
            default:
                // fallthrough
                break;
            }

            // Since the Mach64 pattern is "destination aligned", emulate offsetting the pattern by uploading
            // a rotated pattern
            if (pattOffX) {
                // Rotate 'right' in X direction, we need to rotate within each byte
                ULONG maskLower = (1 << pattOffX) - 1;
                maskLower |= (maskLower << 8) | (maskLower << 16) | (maskLower << 24);
                ULONG maskUpper = ~maskLower;
                pat0            = ((pat0 & maskUpper) >> pattOffX) | ((pat0 & maskLower) << (8 - pattOffX));
                pat1            = ((pat1 & maskUpper) >> pattOffX) | ((pat1 & maskLower) << (8 - pattOffX));
            }

            if (pattOffY) {
                // Rotate 'down' in Y direction
                ULONG temp;
                if (pattOffY & 1) {
                    temp = pat0;
                    pat0 = (pat0 >> 8) | (pat1 << 24);
                    pat1 = (pat1 >> 8) | (temp << 24);
                }
                if (pattOffY & 2) {
                    temp = pat0;
                    pat0 = (pat0 >> 16) | (pat1 << 16);
                    pat1 = (pat1 >> 16) | (temp << 16);
                }
                if (pattOffY & 4) {
                    temp = pat0;
                    pat0 = pat1;
                    pat1 = temp;
                }
            }

            // First upload the pattern to the offscreen area.
            // I was hoping I could just place the 8x8 mono pattern into an offscreen area and
            // get the pattern blit monochrome-expand the bits to actual pixels.
            // But no. Instead, we need to blow up the 8x8 bit pattern into 8x8 actual black and white pixels.
            // Ontop, they need to be spread out by the actual pitch currently selected for the graphics engine.
            // As I understand the PATBLT text: the pattern pixel is read. Then all the '1' bits in the RD_MASK
            // are compared to the same bits in the fetched pixel. If they match, the the foreground mix path
            // is taken, otherwise the background path.
            // Therefore, instead of typical monochrome expansion, we need to produce the pattern first as "full blown"
            // pixels. We upload the monochrome pattern first via a fill blit under monochrome expansion. Then we point
            // the actual pattern blit at the produced "black and white pixels" image and let it be expanded to colored
            // pixels via above mentioned mechanism.
            cd->GEfgPen    = 0xFFFFFFFF;
            cd->GEbgPen    = 0;
            cd->GEdrawMode = 0xFF;

            WaitFifo(bi, 16);
            W_BEE8(PIX_CNTL, MASK_BIT_SRC_CPU);
            W_BEE8(MULT_MISC2, cd->pattSegment << 4);

            REGBASE();
            W_IO_L(FRGD_COLOR, 0xFFFFFFFF);
            W_IO_L(BKGD_COLOR, 0x0);

            setMix(bi, CLR_SRC_FRGD_COLOR | MIX_NEW, CLR_SRC_BKGD_COLOR | MIX_NEW);
            setBlitSrcPosAndSize(bi, cd->pattX, cd->pattY, 8, 8);

            W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT | CMD_ACROSS_PLANE |
                              CMD_WAIT_CPU | CMD_BUS_SIZE_32BIT_MASK_8BIT_ALIGNED);

            W_MMIO_L(PIX_TRANS, pat0);
            W_MMIO_L(PIX_TRANS, pat1);

            WaitFifo(bi, 1);
            W_BEE8(PIX_CNTL, MASK_BIT_SRC_BITMAP);
        } else {
            if (!was8x8) {
                WaitFifo(bi, 1);
                W_BEE8(PIX_CNTL, MASK_BIT_SRC_BITMAP);
            }
        }

        SetDrawMode(bi, pattern->FgPen, pattern->BgPen, pattern->DrawMode, fmt);

        WaitFifo(bi, 8);
        W_BEE8(MULT_MISC2, (cd->pattSegment << 4) | seg);
        setBlitDestPos(bi, x, y);
        setBlitSrcPosAndSize(bi, cd->pattX, cd->pattY, width, height);
        W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_PAT_BLIT | CMD_DRAW_PIXELS | TOP_LEFT | CMD_ACROSS_PLANE |
                          CMD_BUS_SIZE_32BIT_MASK_8BIT_ALIGNED);

    } else {
        cd->patternCacheKey &= ~0x80000000;

        SetDrawMode(bi, pattern->FgPen, pattern->BgPen, pattern->DrawMode, fmt);

        WaitFifo(bi, 7);

        if (was8x8) {
            W_BEE8(PIX_CNTL, MASK_BIT_SRC_CPU);
        }
        // This could/should get chached as well
        W_BEE8(MULT_MISC2, seg << 4);

        setBlitSrcPosAndSize(bi, x, y, width, height);

        W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT | CMD_ACROSS_PLANE | CMD_WAIT_CPU |
                          CMD_BUS_SIZE_32BIT_MASK_32BIT_ALIGNED);

        WORD dwordsPerLine      = (width + 31) / 32;
        UWORD *bitmap           = (UWORD *)pattern->Memory;
        UBYTE rol               = pattern->XOffset % 16;
        UWORD patternHeightMask = (1 << pattern->Size) - 1;

        if (!rol) {
            for (WORD y = 0; y < height; ++y) {
                UWORD bits  = bitmap[(y + pattern->YOffset) & patternHeightMask];
                ULONG bitsL = makeDWORD(bits, bits);
                for (WORD x = 0; x < dwordsPerLine; ++x) {
                    W_MMIO_L(PIX_TRANS, bitsL);
                }
            }
        } else {
            for (WORD y = 0; y < height; ++y) {
                UWORD bits  = bitmap[(y + pattern->YOffset) & patternHeightMask];
                bits        = (bits << rol) | (bits >> (16 - rol));
                ULONG bitsL = makeDWORD(bits, bits);
                for (WORD x = 0; x < dwordsPerLine; ++x) {
                    W_MMIO_L(PIX_TRANS, bitsL);
                }
            }
        }
    }
}

static void REGARGS performBlitPlanar2ChunkyBlits(struct BoardInfo *bi, SHORT dstX, SHORT dstY, SHORT width,
                                                  SHORT height, UWORD mixMode, UBYTE *bitmap, UWORD dwordsPerLine,
                                                  WORD bmPitch, UBYTE rol)
{
    MMIOBASE();

    setBlitSrcPosAndSize(bi, dstX, dstY, width, height);

    if ((ULONG)bitmap == 0x00000000) {
        W_BEE8(PIX_CNTL, MASK_BIT_SRC_ONE);
        W_MMIO_W(FRGD_MIX, (CLR_SRC_BKGD_COLOR | mixMode));
        W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT);

    } else if ((ULONG)bitmap == 0xFFFFFFFF) {
        W_BEE8(PIX_CNTL, MASK_BIT_SRC_ONE);
        W_MMIO_W(FRGD_MIX, (CLR_SRC_FRGD_COLOR | mixMode));
        W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT);

    } else {
        // FIXME: Should I have a path for 16bit aligned width?
        // The only argument for not doing it is unaligned 32bit reads from CPU
        // memory. PCI transfers are 32bit anyways, so wasting bus cycles by
        // transferring in chunks of 16bit seems wasteful
        W_BEE8(PIX_CNTL, MASK_BIT_SRC_CPU);
        setMix(bi, (CLR_SRC_FRGD_COLOR | mixMode), (CLR_SRC_BKGD_COLOR | mixMode));

        // Make sure, no blitter operation is still running before we start feeding PIX_TRANS
        WaitForBlitter(bi);

        W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT | CMD_ACROSS_PLANE | CMD_WAIT_CPU |
                          CMD_BUS_SIZE_32BIT_MASK_32BIT_ALIGNED);

        if (!rol) {
            for (UWORD y = 0; y < height; ++y) {
                for (UWORD x = 0; x < dwordsPerLine; ++x) {
                    W_MMIO_L(PIX_TRANS, ((ULONG *)bitmap)[x]);
                }
                bitmap += bmPitch;
            }
        } else {
            for (UWORD y = 0; y < height; ++y) {
                for (UWORD x = 0; x < dwordsPerLine; ++x) {
                    ULONG left  = ((ULONG *)bitmap)[x] << rol;
                    ULONG right = ((ULONG *)bitmap)[x + 1] >> (32 - rol);
                    W_MMIO_L(PIX_TRANS, (left | right));
                }
                bitmap += bmPitch;
            }
        }
    }
}

static void ASM BlitPlanar2Chunky(__REGA0(struct BoardInfo *bi), __REGA1(struct BitMap *bm),
                                  __REGA2(struct RenderInfo *ri), __REGD0(SHORT srcX), __REGD1(SHORT srcY),
                                  __REGD2(SHORT dstX), __REGD3(SHORT dstY), __REGD4(SHORT width), __REGD5(SHORT height),
                                  __REGD6(UBYTE minTerm), __REGD7(UBYTE mask))
{
    DFUNC(VERBOSE,
          "\nsrcX %ld, srcY %ld, dstX %ld, dstY %ld, w %ld, h %ld"
          "\nmask 0x%lx minTerm %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)minTerm,
          (ULONG)ri->BytesPerRow, (ULONG)ri->Memory);

    MMIOBASE();

    BOOL emulate320  = (ri->BytesPerRow == 320);
    WORD bytesPerRow = emulate320 ? 640 : ri->BytesPerRow;
    // how many dwords per line in the source plane
    UWORD numPlanarBytes              = width / 8 * height * bm->Depth;
    UWORD projectedRegisterWriteBytes = (9 + 8 * 8) * 2;

    if ((projectedRegisterWriteBytes > numPlanarBytes) || !setCR50(bi, bytesPerRow, 1)) {
        DFUNC(1, "fallback to BlitPlanar2ChunkyDefault\n");
        bi->BlitPlanar2ChunkyDefault(bi, bm, ri, srcX, srcY, dstX, dstY, width, height, minTerm, mask);
        return;
    }

    UWORD seg;
    UWORD xoffset;
    UWORD yoffset;
    getGESegmentAndOffset(getMemoryOffset(bi, ri->Memory), bytesPerRow, 1, &seg, &xoffset, &yoffset);

    dstX += xoffset;
    dstY += yoffset;

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITPLANAR2CHUNKY) {
        cd->GEOp = BLITPLANAR2CHUNKY;

        // Invalidate the pen and drawmode caches
        cd->GEdrawMode = 0xFF;

        REGBASE();
        W_IO_L(FRGD_COLOR, 0xFFFFFFFF);
        W_IO_L(BKGD_COLOR, 0x00000000);
    }

    UWORD mixMode = minTermToMix[minTerm];

    if (cd->GEfgPen != 0xFFFFFFFF || cd->GEdrawMode != minTerm || cd->GEFormat != RGBFB_CLUT) {
        cd->GEfgPen    = 0xFFFFFFFF;
        cd->GEbgPen    = 0x00000000;
        cd->GEdrawMode = minTerm;
        cd->GEFormat   = RGBFB_CLUT;

        WaitFifo(bi, 10);
        setMix(bi, (CLR_SRC_FRGD_COLOR | mixMode), (CLR_SRC_BKGD_COLOR | mixMode));
    }

    // This could/should get chached as well
    W_BEE8(MULT_MISC2, seg << 4);

    WORD bmPitch        = bm->BytesPerRow;
    ULONG bmStartOffset = (srcY * bmPitch) + (srcX / 32) * 4;
    UWORD dwordsPerLine = (width + 31) / 32;
    UBYTE rol           = srcX % 32;

    for (short p = 0; p < 8; ++p) {
        UBYTE writeMask = 1 << p;

        if (!(mask & writeMask)) {
            continue;
        }

        SetGEWriteMask(bi, writeMask, RGBFB_CLUT, 8);

        UBYTE *bitmap = (UBYTE *)bm->Planes[p];
        if (bitmap != 0x0 && (ULONG)bitmap != 0xffffffff) {
            bitmap += bmStartOffset;
        }

        if (!emulate320) {
            performBlitPlanar2ChunkyBlits(bi, dstX, dstY, width, height, mixMode, bitmap, dwordsPerLine, bmPitch, rol);
        } else {
            SHORT halfHeight1 = (height + 1) / 2;
            SHORT halfHeight2 = height / 2;

            performBlitPlanar2ChunkyBlits(bi, dstX, dstY, width, halfHeight1, mixMode, bitmap, dwordsPerLine,
                                          bmPitch * 2, rol);
            if (halfHeight2) {
                performBlitPlanar2ChunkyBlits(bi, dstX + 320, dstY, width, halfHeight2, mixMode, bitmap + bmPitch,
                                              dwordsPerLine, bmPitch * 2, rol);
            }
        }
    }
}

void ASM DrawLine(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGA2(struct Line *line),
                  __REGD0(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE, "\n");

    MMIOBASE();

    UBYTE bpp = getBPP(fmt);
    if (!bpp || !setCR50(bi, ri->BytesPerRow, bpp) || !line->Length) {
        DFUNC(1, "Fallback to DrawLineDefault\n")
        bi->DrawLineDefault(bi, ri, line, mask, fmt);
        return;
    }

    UWORD x, y;

    UWORD seg;
    UWORD xoffset;
    UWORD yoffset;
    getGESegmentAndOffset(getMemoryOffset(bi, ri->Memory), ri->BytesPerRow, bpp, &seg, &x, &y);

    x += line->X;
    y += line->Y;

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != LINE) {
        // Make sure, no blitter operation is still running before we start feeding PIX_TRANS
        WaitForBlitter(bi);

        cd->GEOp       = LINE;
        cd->GEdrawMode = 0xFF;
    }

    WaitFifo(bi, 1);

    // This could/should get chached as well
    W_BEE8(MULT_MISC2, seg << 4);

    SetDrawMode(bi, line->FgPen, line->BgPen, line->DrawMode, fmt);
    SetGEWriteMask(bi, mask, fmt, 0);

    UWORD direction = 0;

    WORD absMAX = myabs(line->lDelta);
    WORD absMIN = myabs(line->sDelta);

    WORD errTerm = 2 * absMIN - absMAX;
    if (line->dX > 0) {
        direction |= POSITIVE_X;
    } else {
        errTerm -= 1;
    }
    if (line->dY > 0)
        direction |= POSITIVE_Y;

    if (!line->Horizontal)
        direction |= Y_MAJOR;

    WaitFifo(bi, 8);

#if HAS_PACKED_MMIO
    W_MMIO_L(ALT_CURXY, makeDWORD(x, y));
    W_MMIO_L(ALT_STEP, makeDWORD(2 * (absMIN - absMAX), (2 * absMIN)));
#else
    W_MMIO_W(CUR_X, x);
    W_MMIO_W(CUR_Y, y);
    W_MMIO_W(DESTX_DIASTP, 2 * (absMIN - absMAX));
    W_MMIO_W(DESTY_AXSTP, (2 * absMIN));
#endif

    W_MMIO_W(MAJ_AXIS_PCNT, line->Length - 1);
    W_MMIO_W(ERR_TERM, errTerm);

    BOOL isSolid = (line->LinePtrn == 0xFFFF);
    if (isSolid) {
        W_BEE8(PIX_CNTL, MASK_BIT_SRC_ONE);
        W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_LINE | CMD_DRAW_PIXELS | direction);
    } else {
        W_BEE8(PIX_CNTL, MASK_BIT_SRC_CPU);

        W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_LINE | CMD_DRAW_PIXELS | CMD_ACROSS_PLANE | CMD_WAIT_CPU |
                          CMD_BUS_SIZE_32BIT_MASK_32BIT_ALIGNED | direction);

        // Line->PatternShift selects which bit of the pattern is to be used for the
        // origin of the line and thus shifts the pattern to the indicated number of
        // bits to the left. It is the pattern shift value at the start of the line
        // segment to be drawn.
        UWORD rol      = line->PatternShift;
        UWORD pattern  = (line->LinePtrn << rol) | (line->LinePtrn >> (16u - rol));
        ULONG patternL = makeDWORD(pattern, pattern);
        WORD numDWords = (line->Length + 31) / 32;
        for (WORD i = 0; i < numDWords; ++i) {
            W_MMIO_L(PIX_TRANS, patternL);
        }
    }
}

BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    REGBASE();
    MMIOBASE();
    LOCAL_SYSBASE();

    DFUNC(ALWAYS, "\n");

    //  getChipData(bi)->DOSBase = (ULONG)OpenLibrary(DOSNAME, 0);
    //  if (!getChipData(bi)->DOSBase) {
    //    return FALSE;
    //  }

    bi->GraphicsControllerType = GCT_S3Trio64;
    bi->PaletteChipType        = PCT_S3Trio64;
    bi->Flags = bi->Flags | BIF_NOMEMORYMODEMIX | BIF_BORDERBLANK | BIF_BLITTER | BIF_GRANTDIRECTACCESS |
                BIF_VGASCREENSPLIT | BIF_HASSPRITEBUFFER | BIF_HARDWARESPRITE;
    // Trio64 supports BGR_8_8_8_X 24bit, R5G5B5 and R5G6B5 modes.
    // Prometheus does byte-swapping for writes to memory, so if we're writing a
    // 32bit register filled with XRGB, the written memory order will be BGRX
    bi->RGBFormats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;

    // We don't support these modes, but if we did, they would not allow for a HW
    // sprite
    bi->SoftSpriteFlags = RGBFF_B8G8R8 | RGBFF_R8G8B8;

    bi->SetGC                = SetGC;
    bi->SetPanning           = SetPanning;
    bi->CalculateBytesPerRow = CalculateBytesPerRow;
    bi->CalculateMemory      = CalculateMemory;
    bi->GetCompatibleFormats = GetCompatibleFormats;
    bi->SetDAC               = SetDAC;
    bi->SetColorArray        = SetColorArray;
    bi->SetDisplay           = SetDisplay;
    bi->SetMemoryMode        = SetMemoryMode;
    bi->SetWriteMask         = SetWriteMask;
    bi->SetReadPlane         = SetReadPlane;
    bi->SetClearMask         = SetClearMask;
    bi->ResolvePixelClock    = ResolvePixelClock;
    bi->GetPixelClock        = GetPixelClock;
    bi->SetClock             = SetClock;

    // VSYNC
    bi->WaitVerticalSync = WaitVerticalSync;
    bi->GetVSyncState    = GetVSyncState;

    // DPMS
    bi->SetDPMSLevel = SetDPMSLevel;

    // VGA Splitscreen
    bi->SetSplitPosition = SetSplitPosition;

    // Mouse Sprite
    bi->SetSprite         = SetSprite;
    bi->SetSpritePosition = SetSpritePosition;
    bi->SetSpriteImage    = SetSpriteImage;
    bi->SetSpriteColor    = SetSpriteColor;

    // Blitter acceleration
    bi->WaitBlitter            = WaitBlitter;
    bi->BlitRect               = BlitRect;
    bi->InvertRect             = InvertRect;
    bi->FillRect               = FillRect;
    bi->BlitTemplate           = BlitTemplate;
    bi->BlitPlanar2Chunky      = BlitPlanar2Chunky;
    bi->BlitRectNoMaskComplete = BlitRectNoMaskComplete;
    bi->DrawLine               = DrawLine;
    bi->BlitPattern            = BlitPattern;

    DFUNC(CHATTY,
          "WaitBlitter 0x%08lx\nBlitRect 0x%08lx\nInvertRect 0x%08lx\nFillRect "
          "0x%08lx\n"
          "BlitTemplate 0x%08lx\n BlitPlanar2Chunky 0x%08lx\n"
          "BlitRectNoMaskComplete 0x%08lx\n DrawLine 0x%08lx\n",
          bi->WaitBlitter, bi->BlitRect, bi->InvertRect, bi->FillRect, bi->BlitTemplate, bi->BlitPlanar2Chunky,
          bi->BlitRectNoMaskComplete, bi->DrawLine);

    ChipData_t *cd = getChipData(bi);

    ULONG revision;
    ULONG deviceId;
    LOCAL_OPENPCIBASE();
    GetBoardAttrs(getCardData(bi)->board, PRM_Device, (ULONG)&deviceId, PRM_Revision, (ULONG)&revision, TAG_END);

    if ((cd->chipFamily = getChipFamily(deviceId, revision)) == UNKNOWN) {
        return FALSE;
    }

    // For higher color depths, limit to lower frequencies for stability
    ULONG maxHiColorFreq   = 80000;  // 80MHz max for HiColor
    ULONG maxTrueColorFreq = 50000;  // 50MHz max for TrueColor

    bi->PixelClockCount[HICOLOR]   = 0;
    bi->PixelClockCount[TRUECOLOR] = 0;
    bi->PixelClockCount[TRUEALPHA] = 0;

    // Informed by the largest X/Y coordinates the blitter can talk to
    bi->MaxBMWidth  = 2048;
    bi->MaxBMHeight = 2048;

    bi->BitsPerCannon          = 6;
    bi->MaxHorValue[PLANAR]    = 4088;  // 511 * 8dclks
    bi->MaxHorValue[CHUNKY]    = 4088;
    bi->MaxHorValue[HICOLOR]   = 8176;   // 511 * 8 * 2
    bi->MaxHorValue[TRUECOLOR] = 16352;  // 511 * 8 * 4
    bi->MaxHorValue[TRUEALPHA] = 16352;

    bi->MaxVerValue[PLANAR]    = 2047;
    bi->MaxVerValue[CHUNKY]    = 2047;
    bi->MaxVerValue[HICOLOR]   = 2047;
    bi->MaxVerValue[TRUECOLOR] = 2047;
    bi->MaxVerValue[TRUEALPHA] = 2047;

    // Determined by 10bit value divided by bpp
    bi->MaxHorResolution[PLANAR] = 1600;
    bi->MaxVerResolution[PLANAR] = 1600;

    bi->MaxHorResolution[CHUNKY] = 1600;
    bi->MaxVerResolution[CHUNKY] = 1600;

    bi->MaxHorResolution[HICOLOR] = 1280;
    bi->MaxVerResolution[HICOLOR] = 1280;

    bi->MaxHorResolution[TRUECOLOR] = 1280;
    bi->MaxVerResolution[TRUECOLOR] = 1280;

    bi->MaxHorResolution[TRUEALPHA] = 1280;
    bi->MaxVerResolution[TRUEALPHA] = 1280;

    DFUNC(VERBOSE, "S3 chip wakeup\n");
    if (getChipData(bi)->chipFamily >= TRIO64PLUS) {
        /* Chip wakeup Trio64+ */
        W_REG(0x3C3, 0x01);
    } else {
        /* Chip wakeup Trio64/32 */
        W_REG(0x3C3, 0x10);
        W_REG(0x102, 0x01);
        W_REG(0x3C3, 0x08);

        // FIXME: The Vision864 BIOS seems to lookup a ROM adress to decide which register to use for chip wakeup
        /* Also try 0x46E8 */
        W_REG(0x46E8, 0x10);
        W_REG(0x102, 0x01);
        W_REG(0x46E8, 0x08);
    }

    W_REG(0x3C2, 0x0F);  // Enable clock via clock select CR42; Color-Emulation, 0x3D4/5 for CR_IDX/DATA

    // Unlock S3 registers
    W_CR(0x38, 0x48);
    W_CR(0x39, 0xa5);

    DFUNC(VERBOSE, "check register response\n");
    UBYTE deviceIdHi = R_CR(0x2D);
    UBYTE deviceIdLo = R_CR(0x2E);

    if ((deviceIdHi << 8 | deviceIdLo) != deviceId) {
        DFUNC(ERROR, "Chipset ID mismatch: expected 0x%04lx, got 0x%02lx%02lx\n", deviceId, deviceIdHi, deviceIdLo);
        return FALSE;
    }

    // Initialize PLL table for pixel clocks
    InitPixelClockPLLTable(bi);

    // Set pixel clock counts based on initialized PLL table
    bi->PixelClockCount[PLANAR] = 0;
    bi->PixelClockCount[CHUNKY] = cd->numPllValues;
    // Count how many PLL entries are suitable for each color depth
    const struct svga_pll *pll = (cd->chipFamily >= TRIO64) ? &s3trio64_pll : &s3sdac_pll;
    for (UWORD i = 0; i < cd->numPllValues; i++) {
        ULONG freqKhz = computeKhzFromPllValue(pll, &cd->pllValues[i]);

        if (freqKhz <= maxHiColorFreq) {
            bi->PixelClockCount[HICOLOR]++;
            if (freqKhz <= maxTrueColorFreq) {
                bi->PixelClockCount[TRUECOLOR]++;
                bi->PixelClockCount[TRUEALPHA]++;
            }
        }
    }

    DFUNC(INFO, "PixelClockCount: Planar %ld, Chunky %ld, HiColor %ld, TrueColor %ld, TrueAlpha %ld\n",
          bi->PixelClockCount[PLANAR], bi->PixelClockCount[CHUNKY], bi->PixelClockCount[HICOLOR],
          bi->PixelClockCount[TRUECOLOR], bi->PixelClockCount[TRUEALPHA]);

    UBYTE chipRevision = R_CR(0x2F);
    if (getChipData(bi)->chipFamily >= TRIO64PLUS) {
        BOOL LPBMode            = (R_CR(0x6F) & 0x01) == 0;
        CONST_STRPTR modeString = (LPBMode ? "Local Peripheral Bus (LPB)" : "Compatibility");
        D(INFO, "Chip is Trio64+/V2 (Rev %ld) in %s mode\n", (ULONG)chipRevision & 0x0f, modeString);

        // We can support byte-swapped formats on this chip via the Big Linear
        // Adressing Window
        bi->RGBFormats |= RGBFF_A8R8G8B8 | RGBFF_R5G6B5 | RGBFF_R5G5B5;
    } else {
        D(INFO, "Chip is Visiona864/Trio64/32 (Rev %ld)\n", (ULONG)chipRevision);
#if BUILD_VISION864
        if (!CheckForSDAC(bi)) {
            DFUNC(ERROR, "Unsupported RAMDAC.\n");
            return FALSE;
        }
#endif
    }

    /* The Enhanced Graphics Command register group is unlocked
       by setting bit 0 of the System Configuration register (CR40) to 1.
       After that, bitO of4AE8H must be setto 1 to enable Enhanced mode functions.
        */
    W_CR_MASK(0x40, 0x1, 0x1);

    /* Now that we enabled enhanced mode register access;
     * Enable enhanced mode functions,  write lower byte of 0x4AE8
     * WARNING: DO NOT ENABLE MMIO WITH BIT 5 HERE.
     * This bit will be OR'd into CR53 and thus makes into impossible to setup
     * "new MMIO only" mode on Trio64+. This is despite the docs claiming Bit 5 is
     * "reserved" on there.
     */
    W_REG(ADVFUNC_CNTL, 0x01);

#if BIGENDIAN_MMIO
    if (getChipData(bi)->chipFamily >= TRIO64PLUS) {
        // Enable BYTE-Swapping for MMIO register reads/writes
        // This allows us to write to WORD/DWORD MMIO registers without swapping
        W_CR_MASK(0x54, 0x03, 0b11);
    }
#endif

    /* This field contains the upper 6 bits (19-14) of the CPU base address,
     allowing accessing of up to 4 MBytes of display memory via 64K pages.
     When a non-zero value is programmed in this field, bits 3-0 of CR35
     and 3-2 of CR51 (the old CPU base address bits) are ignored.
     Bit 0 of CR31 must be set to 1 to enable this field.  */
    W_CR(0x6a, 0x0);

    W_SR(0x08, 0x06);  // Unlock Extended Sequencer Registers SR9-SR1C
    W_SR(0x00, 0x00);
    W_SR(0x01, 0x21);  // 8 DCLK per character clock, Display off
    W_SR(0x02, 0x0f);
    W_SR(0x03, 0x00);
    W_SR(0x04, 0x02);
    W_SR(0x0D, 0x00);
    W_SR(0x14, 0x00);
#if !BUILD_VISION864
    W_SR(0x15, 0x00);
    W_SR(0x18, 0x00);
#endif

    // FIXME: this has memory setting implications potentially only valid for the
    // Cybervision
    //  W_SR(0xa, 0xc0);
    //  W_SR(0x18, 0xc0);

    // Init RAMDAC
#if BUILD_VISION864
    W_CR(0x42, 0x02);  // Select clock 2 (see initialization of 0x3C2)
    W_CR(0x55, 0x00);  // RS2 = 0
#endif

    // RAMDAC Mask register
    W_REG(0x3C6, 0xff);

    ULONG clock = bi->MemoryClock;
#if BUILD_VISION864
    if (clock < 40000000) {
        clock = 40000000;
    }
    if (60000000 < clock) {
        clock = 60000000;
    }
#else
    if (clock < 54000000) {
        clock = 54000000;
    }
    if (65000000 < clock) {
        clock = 65000000;
    }
#endif

    clock           = SetMemoryClock(bi, clock);
    bi->MemoryClock = clock;

    W_CR(0x0, 0x5f);
    W_CR(0x1, 0x4f);
    W_CR(0x2, 0x50);
    W_CR(0x3, 0x82);
    W_CR(0x4, 0x54);
    W_CR(0x5, 0x80);
    W_CR(0x6, 0xbf);
    W_CR(0x7, 0x1f);
    W_CR(0x8, 0x0);
    W_CR(0x9, 0x40);
    W_CR(0xa, 0x0);
    W_CR(0xb, 0x0);
    W_CR(0xc, 0x0);
    W_CR(0xd, 0x0);
    W_CR(0xe, 0x0);
    W_CR(0xf, 0x0);
    W_CR(0x10, 0x9c);

    // 5 DRAM refresh cycles, unlock CR0/CR7, disable Vertical Interrupt
    W_CR(0x11, 0xe);

    W_CR(0x12, 0x8f);

    // Offset Register (SCREEN-OFFSET) (CR13)
    // Specifies the logical screen width (pitch). Bits 5-4 of CR51 are extension
    // bits 9-8 for this value. If these bits are OOb, bit 2 of
    // CR43 is extension bit 8 of this register.
    // 10-bit Value = quantity that is multiplied by 2 (word mode), 4 (doubleword
    // mode) or 8 (quadword mode) to specify the difference between the starting
    // byte addresses of two consecutive scan lines. This register contains the
    // least significant 8 bits of this

    W_CR(0x13, 0x50);  // == 160, meaning 640byte in double word mode

    // Underline Location Register (ULL) (CR14) (affects address
    // counting)
    //  Bit 5 CNT BY4 - Select Count by 4 Mode
    //      0= The memory address counter depends on bit 3 of CR17 (count by 2)
    //      1 = The memory address counter is incremented every four character
    //      clocks
    //              The CNT BY4 bit is used when double word addresses are used.
    //  Bit 6 DBLWD MODE - Select Doubleword Mode
    //      0 = The memory addresses are byte or word addresses
    //      1 = The memory addresses are doubleword addresses
    //
    W_CR(0x14, 0x40);

    W_CR(0x15, 0x96);  // Start Vertical Blank Register (SVB) (CR15)
    W_CR(0x16, 0xb9);  // End Vertical Blank Register (EVB) (CR16)

    //  CRTC Mode Control Register (CRT _MO) (CR17
    //  Bit 3 CNT BY2 - Select Word Mode
    //  0= Memory address counter is clocked with the character clock input, and
    //  byte mode addressing for the video memory is selected 1 = Memory address
    //  counter is clocked by the character clock input divided by 2, and word
    //  mode addressing for the video memory is selected Bit 6 BYTE MODE - Select
    //  Byte Addressing Mode
    //      0 = Word mode shifts all memory address counter bits down one bit, and
    //      the most
    //          significant bit of the counter appears on the least significant
    //          bit of the memory address output
    //      1 = Byte address mode
    W_CR(0x17, 0xC3);  // Byte Adressing mode, V/HSync pulses enabled

    W_CR(0x18, 0xff);  // Line compare register

    // Memory Configuration Register (MEM_CNFG) (eR31)
    // Bit 3 ENH MAP - Use Enhanced Mode Memory Mapping
    // 0= Force IBM VGA mapping for memory accesses
    // 1 = Force Enhanced Mode mappings
    // Setting this bit to 1 overrides the settings of bit 6 of CR14 and bit 3 of
    // CR17 and causes the use of doubleword memory addressing mode. Also, the
    // function of bits 3- 2 of GR6 is overridden with a fixed 64K map at AOOOOH.
    W_CR(0x31, 0x08);  // Enhanced memory mapping, Doubleword mode

    W_CR(0x32, 0x10);

    // BackWard Compatibility 2 Register (BKWD_2) (CR33)
    // Bit 1 DIS VDE - Disable Vertical Display End Extension Bits Write
    // Protection
    //  0 = VDE protection enabled
    //  1 = Disables the write protect setting of the bit 7 of CR11 on bits 1
    //      and 6 of CR7
    W_CR(0x33, 0x02);
    /* Miscellaneous 1 (CR3A)
     * Bits 1-0 REF-CNT - Alternate Refresh Count Control
          01 = Refresh Count 1
       Bit 2 ENS RFC - Enable Alternate Refresh Count Control
         1 = Alternate refresh count control (bits 1-0) is enabled
       Bit 4 ENH 256 - Enable 8 Bits/Pixel or Greater Color Enhanced Mode
       Bit 5 HST DFW - Enable High Speed Text Font Writing */
    W_CR(0x3a, 0x35);
    // Start Display FIFO Fetch (CR3B)
    W_CR(0x3b, 0x5a);

    // Extended Mode Register (EXT_MODE) (CR43)
    W_CR(0x43, 0x00);

    // Extended System Conttrol 1 Register (EX_SCTL_1)  (CR50)
    W_CR(0x50, 0x00);

    // Extended System Control 2 Register (EX_SCTL_2) (CR51)
    W_CR(0x51, 0x00);

    // Enable 4MB Linear Address Window (LAW)
    W_CR(0x58, 0x13);
    if (getChipData(bi)->chipFamily >= TRIO64PLUS) {
        // Enable Trio64+ "New MMIO" only and byte swapping in the Big Endian window
        D(5, "setup newstyle MMIO\n");
        W_CR_MASK(0x53, 0x3E, 0x0c);
    } else {
        D(5, "setup compatible MMIO\n");
        // Enable Trio64 old style MMIO. This hardcodes the MMIO range to 0xA8000
        // physical address. Need to make sure, nothing else sits there
        W_CR_MASK(0x53, 0x10, 0x10);

        // Test, also enable MMIO and Linear addressing via the other register
        // W_REG_MASK(ADVFUNC_CNTL, 0x30, 0x30);

#if DBG || 1
        {
            LOCAL_OPENPCIBASE();
            // LAW start address
            ULONG physAddress = (ULONG)pci_logic_to_physic_addr(bi->MemoryBase, getCardData(bi)->board);
            if (physAddress & 0x3FFFFF) {
                D(WARN, "WARNING: card's base address is not 4MB aligned!\n");
            }
        }
#endif
        // Setup the Linear Address Window (LAW)  position
        // Beware: while bi->MemoryBase is a 'virtual' address, the register wants a physical address
        // We basically achieve this translation by chopping off the topmost bits.
        W_CR_MASK(0x5a, 0x40, (ULONG)bi->MemoryBase >> 16);
        D(0, "CR59: 0x%lx CR5A: 0x%lx\n", (ULONG)R_CR(0x59), (ULONG)R_CR(0x5a));
        // Upper address bits may  not be touched as they would result in shifting
        // the PCI window
        //    W_CR_MASK(0x59, physAddress >> 24);
    }
    D(0, "MMIO base address: 0x%lx\n", (ULONG)getMMIOBase(bi));

    // MCLK M Parameter
    W_CR_MASK(0x54, 0xFC, 0x70);

    W_CR(0x60, 0xff);

    W_CR(0x5d, 0x0);
    W_CR(0x5e, 0x40);
    W_GR(0x0, 0x0);
    W_GR(0x1, 0x0);
    W_GR(0x2, 0x0);
    W_GR(0x3, 0x0);
    W_GR(0x4, 0x0);
    W_GR(0x5, 0x40);
    W_GR(0x6, 0x1);
    W_GR(0x7, 0xf);
    W_GR(0x8, 0xff);

    // Enable writing attribute palette registers, disable video
    R_REG(0x3DA);
    W_REG(ATR_AD, 0x0);

    // Reset AFF to index register selection
    R_REG(0x3DA);

    for (int p = 0; p < 16; ++p) {
        /* The attribute controller registers are located atthe same byte I/O
           address for writing address and data. An internal address flip-flop (AFF)
           controls the selection of either the attribute index or data registers.
           To initialize the address flip-flop (AFF), an I/O read is issued at
           address 3BAH or 3DAH. This presets the address flip-flop to select the
           index register. After the index register has been loaded by an I/O write
           to address 3COH, AFF toggles and the next 1/0 write loads the data
           register. Every I/O write to address 3COH toggles this address flip-flop.
           However, it does not toggle for I/O reads at address 3COH or 3C1 H. The
           Attribute Controller Index register is read at 3COH, and the Attribute
           Controller Data register is read
           at address 3C1 H.  */
        W_AR(p, p);
    }
    W_AR(0x30, 0x61);
    W_AR(0x31, 0x0);
    W_AR(0x32, 0xf);
    W_AR(0x33, 0x0);
    W_AR(0x34, 0x0);

    // Enable video
    R_REG(0x3DA);  // reset AFF
    W_REG(ATR_AD, 0x20);

#if !BUILD_VISION864
    /* Enable PLL load */
    W_MISC_MASK(0x0c, 0x0c);
#endif

    // Just some diagnostics; FIXME: this is different between various series of Vision/Trio chips
#ifdef DBG
    UBYTE memType = (R_CR(0x36) >> 2) & 3;
    switch (memType) {
    case 0b00:
        D(1, "1-cycle EDO\n");
        break;
    case 0b10:
        D(1, "2-cycle EDO\n");
        break;
    case 0b11:
        D(1, "FPM\n");
        break;
    default:
        D(0, "unknown memory type\n");
    }
#endif

    // Determine memory size of the card (typically 1-2MB, but can be up to 4MB)
    bi->MemorySize              = 0x400000;
    volatile ULONG *framebuffer = (volatile ULONG *)bi->MemoryBase;
    framebuffer[0]              = 0;
    while (bi->MemorySize) {
        D(1, "Probing memory size %ld\n", bi->MemorySize);

        // Enable Linear Addressing Window LAW
        {
            UBYTE LAWSize = 0;
            UBYTE MemSize = 0;
            if (bi->MemorySize >= 0x400000) {
                LAWSize = 0b11;
                MemSize = 0b000;
            } else if (bi->MemorySize >= 0x200000) {
                LAWSize = 0b10;  // 2MB
                MemSize = 0b100;
            } else {
                LAWSize = 0b01;  // 1MB
                MemSize = 0b110;
            }
            W_CR_MASK(0x36, 0xE0, MemSize << 5);
            W_CR_MASK(0x58, 0x13, LAWSize | 0x10);
        }

        CacheClearU();

        // Probe the last and the first longword for the current segment,
        // as well as offset 0 to check for wrap arounds
        volatile ULONG *highOffset = framebuffer + (bi->MemorySize >> 2) - 1;
        volatile ULONG *lowOffset  = framebuffer + (bi->MemorySize >> 3);
        // Probe  memory
        *framebuffer = 0;
        *highOffset  = (ULONG)highOffset;
        *lowOffset   = (ULONG)lowOffset;

        CacheClearU();

        ULONG readbackHigh = *highOffset;
        ULONG readbackLow  = *lowOffset;
        ULONG readbackZero = *framebuffer;

        D(10, "Probing memory at 0x%lx ?= 0x%lx; 0x%lx ?= 0x%lx, 0x0 ?= 0x%lx\n", highOffset, readbackHigh, lowOffset,
          readbackLow, readbackZero);

        if (readbackHigh == (ULONG)highOffset && readbackLow == (ULONG)lowOffset && readbackZero == 0) {
            break;
        }
        // reduce available memory size
        bi->MemorySize >>= 1;
        if (bi->MemorySize < 1024 * 1024) {
            D(0, "Memory detection failed, aborting\n");
            return FALSE;
        }
    }

    D(1, "MemorySize: %ldmb\n", bi->MemorySize / (1024 * 1024));

    // Input Status ? Register (STATUS_O)
    D(1, "Monitor is %s present\n", (!(R_REG(0x3C2) & 0x10) ? "" : "NOT"));

    // Two sprite images, each 64x64*2 bits
    const ULONG maxSpriteBuffersSize = (64 * 64 * 2 / 8) * 2;

    // take sprite image data off the top of the memory
    // sprites can be placed at segment boundaries of 1kb
    bi->MemorySize       = (bi->MemorySize - maxSpriteBuffersSize) & ~(1024 - 1);
    bi->MouseImageBuffer = bi->MemoryBase + bi->MemorySize;
    bi->MouseSaveBuffer  = bi->MemoryBase + bi->MemorySize + maxSpriteBuffersSize / 2;

    // Start Address in terms of 1024byte segments
    W_CR(0x4c, 0);  // init to 0
    W_CR(0x4d, 0);
    W_CR_OVERFLOW1((bi->MemorySize >> 10), 0x4d, 0, 8, 0x4c, 0, 4);
    // Sprite image offsets
    W_CR(0x4e, 0);
    W_CR(0x4f, 0);
    // Reset cursor position
    W_CR(0x46, 0);
    W_CR(0x47, 0);
    W_CR(0x48, 0);
    W_CR(0x49, 0);

    WaitForIdle(bi);

    // Write conservative scissors
    W_BEE8(SCISSORS_T, 0x0000);
    W_BEE8(SCISSORS_L, 0x0000);
    W_BEE8(SCISSORS_B, 0x0fff);
    W_BEE8(SCISSORS_R, 0x0fff);

    // Set MULT_MISC first so that
    // "Bit 4 RSF - Select Upper Word in 32 Bits/Pixel Mode" is set to 0 and
    // Bit 9 CMR 32B - Select 32-Bit Command Registers
    W_BEE8(MULT_MISC, (1 << 9));

    W_BEE8(MULT_MISC2, 0);
    // Init GE write/read masks. The use of IO instead of MMIO is deliberate.
    // Apparently when we switch these registers to 32bit via MULT_MISC above,
    // Only the registers in the IO space become 32bit, but not in MMIO!
    W_IO_L(WRT_MASK, 0xFFFFFFFF);
    W_IO_L(RD_MASK, 0xFFFFFFFF);
    W_IO_L(COLOR_CMP, 0x0);

    W_MMIO_W(FRGD_MIX, CLR_SRC_FRGD_COLOR | MIX_NEW);
    W_MMIO_W(BKGD_MIX, CLR_SRC_BKGD_COLOR | MIX_NEW);

    W_MMIO_W(PAT_Y, 0);
    W_MMIO_W(PAT_X, 0);

    // Flush FIFO
    W_MMIO_W(CMD, CMD_ALWAYS | CMD_TYPE_NOP);

    // reserve memory for a 8x8 monochrome pattern
    // Unfortunately we're overallocating here for the largest pitch.
    ULONG patternSize      = 8 * 3200;  // 800 * 4 * 8
    ULONG patternOffset    = (bi->MemorySize - patternSize);
    patternOffset          = (patternOffset / 3200) * 3200;  // align to 800pixel@32bit boundary
    bi->MemorySize         = patternOffset;
    cd->patternVideoBuffer = (ULONG *)(bi->MemoryBase + bi->MemorySize);
    cd->patternCacheBuffer = AllocVec(patternSize, MEMF_PUBLIC);

    // setCacheMode(bi, bi->MemoryBase, bi->MemorySize & ~4095, MAPP_NONSERIALIZED | MAPP_IMPRECISE, CACHEFLAGS);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef TESTEXE

#include <boardinfo.h>
#include <libraries/openpci.h>
#include <proto/dos.h>
#include <proto/expansion.h>
#include <proto/openpci.h>
#include <proto/timer.h>
#include <proto/utility.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VENDOR_E3B        0xE3B
#define VENDOR_MATAY      0xAD47
#define DEVICE_FIRESTORM  200
#define DEVICE_PROMETHEUS 1

struct Library *OpenPciBase = NULL;
struct IORequest ioRequest;
struct Device *TimerBase = NULL;
struct UtilityBase *UtilityBase;

void sigIntHandler(int dummy)
{
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
    }
    abort();
}

int main()
{
    signal(SIGINT, sigIntHandler);

    int rval = EXIT_FAILURE;

    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        D(0, "Unable to open openpci.library\n");
    }

    // if (!(DOSBase = OpenLibrary(DOSNAME, 0))) {
    //     D(0, "Unable to open dos.library\n");
    //     goto exit;
    // }

    // if (OpenDevice(TIMERNAME, 0, &ioRequest, 0)) {
    //     D(0, "Unable to open " TIMERNAME "\n");
    //     goto exit;
    // }
    // TimerBase = ioRequest.io_Device;

    // struct EClockVal startTime, endTime;
    // ULONG eFreq = ReadEClock(&startTime);
    // delayMicroSeconds(555);
    // ReadEClock(&endTime);
    // ULONG delta = *(uint64_t *)&endTime - *(uint64_t *)&startTime;
    // delta       = (delta * 1000) / (eFreq / 1000);

    // D(INFO, "Delay: %ld ms\n", delta);

    ULONG dmaSize = 128 * 1024;

    struct pci_dev *board = NULL;

    D(0, "Looking for S3 Trio64 card\n");

    while ((board = FindBoard(board, PRM_Vendor, VENDOR_ID_S3, TAG_END)) != NULL) {
        ULONG Device, Revision, Memory0Size = 0;
        APTR Memory0      = 0;
        APTR legacyIOBase = 0;

        ULONG count = GetBoardAttrs(board, PRM_Device, (ULONG)&Device, PRM_Revision, (ULONG)&Revision, PRM_MemoryAddr0,
                                    (ULONG)&Memory0, PRM_MemorySize0, (ULONG)&Memory0Size, PRM_LegacyIOSpace,
                                    (ULONG)&legacyIOBase, TAG_END);

        D(0, "device %x revision %x\n", Device, Revision);

        ChipFamily_t chipFamily = getChipFamily(Device, Revision);

        if (chipFamily != UNKNOWN) {
            D(ALWAYS, "S3: %s found\n", getChipFamilyName(chipFamily));

            // Write PCI COMMAND register to enable IO and Memory access
            //            pci_write_config_word(0x04, 0x0003, board);

            D(ALWAYS, "MemoryBase 0x%x, MemorySize %u, IOBase 0x%x\n", Memory0, Memory0Size, legacyIOBase);

            APTR physicalAddress = pci_logic_to_physic_addr(Memory0, board);
            D(ALWAYS, "physicalAdress 0x%08lx\n", physicalAddress);

            struct ChipBase *ChipBase = NULL;

            struct BoardInfo boardInfo;
            memset(&boardInfo, 0, sizeof(boardInfo));
            struct BoardInfo *bi = &boardInfo;

            CardData_t *card = getCardData(bi);
            bi->ExecBase      = SysBase;
            bi->UtilBase      = UtilityBase;
            bi->ChipBase      = ChipBase;
            card->OpenPciBase = OpenPciBase;
            card->board       = board;

            if (chipFamily >= TRIO64PLUS) {
                // The Trio64
                // S3Trio64.chip expects register base adress to be offset by 0x8000
                // to be able to address all registers with just regular signed 16bit
                // offsets
                bi->RegisterBase = ((UBYTE *)legacyIOBase) + REGISTER_OFFSET;
                // Use the Trio64+ MMIO range in the BE Address Window at BaseAddress +
                // 0x3000000
                bi->MemoryIOBase = Memory0 + 0x3000000 + MMIOREGISTER_OFFSET;
                // No need to fudge with the base address here
                bi->MemoryBase = Memory0;
            } else {
                bi->RegisterBase = ((UBYTE *)legacyIOBase) + REGISTER_OFFSET;
                // This is how I understand Trio64/32 MMIO approach: 0xA0000 is
                // hardcoded as the base of the enhanced registers I need to make
                // sure, the first 1 MB of address space don't overlap with anything.
                bi->MemoryIOBase = pci_physic_to_logic_addr((APTR)0xA0000, board);

                if (bi->MemoryIOBase == NULL) {
                    D(ALWAYS, "VGA memory window at 0xA0000-BFFFF is not available. Aborting.\n");
                    goto exit;
                }

                D(ALWAYS, "MMIO Base at physical address 0xA0000 virtual: 0x%lx.\n", bi->MemoryIOBase);

                bi->MemoryIOBase += MMIOREGISTER_OFFSET;

                // I have to push out the card's Linear Address Window memory base
                // address to not overlap with its own MMIO address range at
                // 0xA8000-0xAFFFF On Trio64+ this is way easier with the "new MMIO"
                // approach. Here we move the Linear Address Window up by 4MB. This
                // gives us 4MB alignment and moves the LAW while not moving the PCI
                // BAR of teh card. The assumption is that the gfx card is the first
                // one to be initialized and thus sit at 0x00000000 in PCI address
                // space. This way 0xA8000 is in the card's BAR and the LAW should be
                // at 0x400000
                bi->MemoryBase = Memory0;
                if (pci_logic_to_physic_addr(bi->MemoryBase, board) <= (APTR)0xB0000) {
                    // This shifts the memory base address by 4MB, which should be ok
                    // since the S3Trio asks for 8MB PCI address space, typically only
                    // utilizing the first 4MB
                    bi->MemoryBase += 0x400000;

                    D(ALWAYS,
                      "WARNING: Trio64/32 memory base overlaps with MMIO address at "
                      "0xA8000-0xAFFFF.\n"
                      "Moving FB adress window out by 4mb to 0x%lx\n",
                      bi->MemoryBase);
                }
            }

            D(ALWAYS, "Trio64 init chip\n");
            if (!InitChip(bi)) {
                D(ERROR, "InitChip failed. Exit");
                goto exit;
            }
            D(ALWAYS, "Trio64 has %ldkb usable memory\n", bi->MemorySize / 1024);

            {
                DFUNC(ALWAYS, "SetDisplay OFF\n");
                SetDisplay(bi, FALSE);
            }

            {
                // test 640x480 screen
                struct ModeInfo mi;

                mi.Depth            = 8;
                mi.Flags            = 0;
                mi.Height           = 480;
                mi.Width            = 680;
                mi.HorBlankSize     = 0;
                mi.HorEnableSkew    = 0;
                mi.HorSyncSize      = 96;
                mi.HorSyncStart     = 16;
                mi.HorTotal         = 800;
                mi.PixelClock       = 25175000;
                mi.pll1.Numerator   = 0;
                mi.pll2.Denominator = 0;
                mi.VerBlankSize     = 0;
                mi.VerSyncSize      = 2;
                mi.VerSyncStart     = 10;
                mi.VerTotal         = 525;

                bi->ModeInfo = &mi;

                // ResolvePixelClock(bi, &mi, 67000000, RGBFB_CLUT);

                ULONG index = ResolvePixelClock(bi, &mi, mi.PixelClock, RGBFB_CLUT);

                DFUNC(ALWAYS, "SetClock\n");

                SetClock(bi);

                DFUNC(ALWAYS, "SetGC\n");

                SetGC(bi, &mi, TRUE);
            }
            {
                DFUNC(ALWAYS, "SetDAC\n");
                SetDAC(bi, RGBFB_CLUT);
            }
            {
                SetSprite(bi, FALSE, RGBFB_CLUT);
            }

            {
                DFUNC(0, "SetColorArray\n");
                UBYTE colors[256 * 3];
                for (int c = 0; c < 256; c++) {
                    bi->CLUT[c].Red   = c;
                    bi->CLUT[c].Green = c;
                    bi->CLUT[c].Blue  = c;
                }
                SetColorArray(bi, 0, 256);
            }
            {
                SetPanning(bi, bi->MemoryBase, 640, 480, 0, 0, RGBFB_CLUT);
            }
            {
                DFUNC(ALWAYS, "SetDisplay ON\n");
                SetDisplay(bi, TRUE);
            }

            for (int y = 0; y < 480; y++) {
                for (int x = 0; x < 640; x++) {
                    *(volatile UBYTE *)(bi->MemoryBase + y * 640 + x) = x;
                }
            }

            struct RenderInfo ri;
            ri.Memory      = bi->MemoryBase;
            ri.BytesPerRow = 640;
            ri.RGBFormat   = RGBFB_CLUT;

            {
                FillRect(bi, &ri, 100, 100, 640 - 200, 480 - 200, 0xFF, 0xFF, RGBFB_CLUT);
            }

            {
                FillRect(bi, &ri, 64, 64, 128, 128, 0xAA, 0xFF, RGBFB_CLUT);
            }

            {
                FillRect(bi, &ri, 256, 10, 128, 128, 0x33, 0xFF, RGBFB_CLUT);
            }

#if 0
            for (int i = 0; i < 8; ++i) {
                {
                    UWORD patternData[] = {0x0101, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
                    struct Pattern pattern;
                    pattern.BgPen    = 127;
                    pattern.FgPen    = 255;
                    pattern.DrawMode = JAM2;
                    pattern.Size     = 2;
                    pattern.Memory   = patternData;
                    pattern.XOffset  = i;
                    pattern.YOffset  = i;

                    BlitPattern(bi, &ri, &pattern, 100 + i * 32, 150 + i * 32, 24, 24, 0xFF, RGBFB_CLUT);
                }
            }

            for (int i = 0; i < 8; ++i) {
                {
                    UWORD patternData[] = {0x0101, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
                    struct Pattern pattern;
                    pattern.BgPen    = 127;
                    pattern.FgPen    = 255;
                    pattern.DrawMode = JAM2;
                    pattern.Size     = 2;
                    pattern.Memory   = patternData;
                    pattern.XOffset  = 0;
                    pattern.YOffset  = 0;

                    BlitPattern(bi, &ri, &pattern, 150 + i * 32 + i, 150 + i * 32 + i, 24, 24, 0xFF, RGBFB_CLUT);
                }
            }

            for (int i = 0; i < 8; ++i) {
                {
                    UWORD patternData[] = {
                        0xF0F0, 0xF0F0, 0x0F0F, 0x0F0F, 0x0, 0x0, 0x0,
                    };
                    struct Pattern pattern;
                    pattern.BgPen    = 127;
                    pattern.FgPen    = 255;
                    pattern.DrawMode = JAM2;
                    pattern.Size     = 2;
                    pattern.Memory   = patternData;
                    pattern.XOffset  = i;
                    pattern.YOffset  = i;

                    BlitPattern(bi, &ri, &pattern, 200 + i * 32 + i, 150 + i * 32 + i, 24, 24, 0xFF, RGBFB_CLUT);
                }
            }
#endif
            WaitBlitter(bi);
            // RegisterOwner(cb, board, (struct Node *)ChipBase);

            // if ((dmaSize > 0) && (dmaSize <= bi->MemorySize)) {
            //     // Place DMA window at end of memory window 0 and page-align it
            //     ULONG dmaOffset = (bi->MemorySize - dmaSize) & ~(4096 - 1);
            //     InitDMAMemory(cb, bi->MemoryBase + dmaOffset, dmaSize);
            //     bi->MemorySize = dmaOffset;
            //     cb->cb_DMAMemGranted = TRUE;
            // }
            // no need to continue - we have found a match
            rval = EXIT_SUCCESS;
            goto exit;
        }
    }  // while

    D(ERROR, "no Trio64 found.\n");

exit:
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
    }

    return rval;
}
#endif  // TESTEXE
