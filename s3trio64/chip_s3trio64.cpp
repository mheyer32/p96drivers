#include "chip_s3trio64.h"
#include "edid_common.h"
#include "s3ramdac.h"

#define __NOLIBBASE__

#include <clib/debug_protos.h>
#include <debuglib.h>
#include <exec/types.h>
#include <graphics/rastport.h>
#include <hardware/cia.h>
#include <hardware/intbits.h>

#if OPENPCI
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/openpci.h>
#endif
#include <proto/exec.h>

#include <SDI_stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DBG
#ifdef CONFIG_CYBERVISION64
extern int debugLevel;
#else
int debugLevel = VERBOSE;
#endif
#endif

#if !MMIO_ONLY
#define SUBSYS_STAT  0x42E8  // Read
#define SUBSYS_CNTL  0x42E8  // Write
#define ADVFUNC_CNTL 0x4AE8
#else
// Offset from 'IO Register Base' (0x1008000) in MMIO addresses
#define SUBSYS_STAT  0x0504  // Read
#define SUBSYS_CNTL  0x0504  // Write
#define ADVFUNC_CNTL 0x050C
#endif

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

#define CMD_ALWAYS          0x0001
#define CMD_ACROSS_PLANE    0x0002
#define CMD_NO_LASTPIXEL    0x0004
#define CMD_RADIAL_DRAW_DIR 0x0008
#define CMD_DRAW_PIXELS     0x0010
#define CMD_DRAW_DIR_MASK   0x00e0
#define CMD_BYTE_SWAP       0x1000
#define CMD_WAIT_CPU        0x0100

#define CMD_BUS_SIZE_8BIT                     (0b00 << 9)
#define CMD_BUS_SIZE_16BIT                    (0b01 << 9)
#define CMD_BUS_SIZE_32BIT_MASK_32BIT_ALIGNED (0b10 << 9)
#if !BUILD_VISION864  // FIXME: supposed to apply to Vision964, too
#define CMD_BUS_SIZE_32BIT_MASK_8BIT_ALIGNED (0b11 << 9)
#endif

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
// #define PAT_Y         0xEAE8
// #define PAT_X         0xEAEA

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

#ifdef CONFIG_CYBERVISION64
#define HAS_ROXXLER 1
#else
#define HAS_ROXXLER 0
#endif

/******************************************************************************/
/*                                                                            */
/* library exports                                                                    */
/*                                                                            */
/******************************************************************************/

#if !defined(TESTEXE) && !defined(CONFIG_CYBERVISION64)

#if defined(CONFIG_S3TRIO64PLUS)
extern const char LibName[] = "S3Trio64Plus.chip";
#elif defined(CONFIG_VISION864)
extern const char LibName[] = "S3Vision864.chip";
#elif defined(CONFIG_S3TRIO3264)
extern const char LibName[] = "S3Trio3264.chip";
#elif defined(CONFIG_S3TRIO64V2)
extern const char LibName[] = "S3Trio64V2.chip";
#endif
extern const char LibIdString[] = "S3Vision864/Trio32/64/64Plus Picasso96 chip driver version 1.0";

#ifndef LIB_VERSION
#define LIB_VERSION 1
#endif
#ifndef LIB_REVISION
#define LIB_REVISION 0
#endif
extern const UWORD LibVersion  = LIB_VERSION;
extern const UWORD LibRevision = LIB_REVISION;
#endif

// Wait For just the blitter to finish. No wait for FIFO queue empty.
static void WaitForBlitter(__REGA0(struct BoardInfo *bi))
{
    asS3(bi)->waitForBlitter();
}

// Wait for blitter to finish AND FIFO queue empty.
static void WaitForIdle(__REGA0(struct BoardInfo *bi))
{
    asS3(bi)->waitForIdle();
}

void ASM S3Driver::waitBlitter()
{
    BoardInfo *bi = this;
    WaitForIdle(bi);
}

// Initialize PLL table for pixel clocks
void initPixelClockPLLTable(BoardInfo_t *bi)
{
    DFUNC(VERBOSE, "\n");

    LOCAL_SYSBASE();

    ChipData_t *cd             = getChipData(bi);
    const struct svga_pll *pll = NULL;
    UWORD maxFreqMhz           = 135;

    cd->ramdacOps->getPllParams(bi, &pll, &maxFreqMhz);

    UWORD minFreqMhz = 12;  // 12MHz min
    UWORD numEntries = (maxFreqMhz - minFreqMhz + 1) * 2;

    PLLValue_t *pllValues = (PLLValue_t *)AllocVec(sizeof(PLLValue_t) * numEntries, MEMF_PUBLIC);
    if (!pllValues) {
        DFUNC(ERROR, "Failed to allocate PLL table\n");
        return;
    }

    cd->pllValues    = pllValues;
    cd->numPllValues = 0;

    // For higher color depths, limit to lower frequencies for stability
    ULONG maxHiColorFreq   = 80000;  // 80MHz max for HiColor
    ULONG maxTrueColorFreq = 50000;  // 50MHz max for TrueColor

    bi->PixelClockCount[PLANAR]    = 0;
    bi->PixelClockCount[HICOLOR]   = 0;
    bi->PixelClockCount[TRUECOLOR] = 0;
    bi->PixelClockCount[TRUEALPHA] = 0;
    bi->PixelClockCount[CHUNKY]    = 0;

    // Generate PLL values for each frequency
    int lastValue = 0;
    for (UWORD i = 0; i < numEntries; ++i) {
        ULONG freqKhz = (minFreqMhz + i) * 500;  // kHz

        BOOL clockHalving = (freqKhz <= MIN_PLLCLOCK_KHZ);
        if (clockHalving) {
            freqKhz *= 2;  // PLL runs at 2x; SR1 bit 3 gives DCLK = VCLK/2
        }

        UWORD m, n, r;
        int currentKhz = svga_compute_pll(pll, freqKhz, &m, &n, &r);

        if (clockHalving) {
            currentKhz /= 2;  // Effective pixel clock
        }

        if (currentKhz >= 0 && currentKhz != lastValue) {
            lastValue                             = currentKhz;
            pllValues[cd->numPllValues].m         = m;
            pllValues[cd->numPllValues].n         = n;
            pllValues[cd->numPllValues].r         = r;
            pllValues[cd->numPllValues].freq10khz = (UWORD)((currentKhz + 5) / 10);  // store in 10 kHz units

            cd->numPllValues++;

            bi->PixelClockCount[CHUNKY]++;
            if (currentKhz <= maxHiColorFreq) {
                bi->PixelClockCount[HICOLOR]++;
                if (currentKhz <= maxTrueColorFreq) {
                    bi->PixelClockCount[TRUECOLOR]++;
                    bi->PixelClockCount[TRUEALPHA]++;
                }
            }

            DFUNC(CHATTY, "Pixelclock %03ld %09ldHz: m=%ld n=%ld r=%ld\n", (ULONG)cd->numPllValues - 1,
                  (ULONG)currentKhz * 1000, (ULONG)m, (ULONG)n, (ULONG)r);
        }
    }

    D(VERBOSE, "Initialized %ld PLL entries\n", cd->numPllValues);

    DFUNC(INFO, "PixelClockCount: Planar %ld, Chunky %ld, HiColor %ld, TrueColor %ld, TrueAlpha %ld\n",
          bi->PixelClockCount[PLANAR], bi->PixelClockCount[CHUNKY], bi->PixelClockCount[HICOLOR],
          bi->PixelClockCount[TRUECOLOR], bi->PixelClockCount[TRUEALPHA]);
}

ULONG SetMemoryClock(struct BoardInfo *bi, ULONG clockHz)
{
    DFUNC(INFO, "Hz: %ld\n", clockHz);

    const ChipData_t *cd = getChipData(bi);
    return cd->ramdacOps->setMemoryClock(bi, clockHz);
}

UWORD ASM S3Driver::calculateBytesPerRow(__REGD0(UWORD width), __REGD1(UWORD height), __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE_REG format))
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

void ASM S3Driver::setColorArray(__REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "startIndex %ld, count %ld\n", (ULONG)startIndex, (ULONG)count);

    VgaIo vga = this->vga();
    LOCAL_SYSBASE();

    // FIXME: this should be a constant for the Trio, no need to make it dynamic
    const UBYTE bppDiff = 2;  // 8 - bi->BitsPerCannon;

    // This may noty be interrupted, so DAC_WR_AD remains set throughout the
    // function
    Disable();

    vga.writeB(VgaReg::DAC_WR_INDEX, startIndex);

    struct CLUTEntry *entry = &bi->CLUT[startIndex];

    // Do not print these individual register writes as it takes ages
    for (UWORD c = 0; c < count; ++c) {
        vga.writeB(VgaReg::DAC_PEL_DATA, entry->Red >> bppDiff);
        vga.writeB(VgaReg::DAC_PEL_DATA, entry->Green >> bppDiff);
        vga.writeB(VgaReg::DAC_PEL_DATA, entry->Blue >> bppDiff);
        ++entry;
    }

    Enable();
    return;
}

void ASM S3Driver::setDAC(__REGD0(UWORD region), __REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    DFUNC(INFO, "\n");

    const RamdacOps_t *ops = chip()->ramdacOps;
	ops->setDac(bi, AS_RGBF(format));
}

static INLINE REGARGS UWORD toScanLines(UWORD y, UWORD modeFlags)
{
    if (modeFlags & GMF_DOUBLESCAN)
        y *= 2;
    if (modeFlags & GMF_INTERLACE)
        y /= 2;
    return y;
}

static INLINE REGARGS UWORD adjustBorder(UWORD x, BOOL borderEnabled, UWORD minBorder)
{
    if (!borderEnabled || x == 0)
        x = minBorder;
    return x;
}

void ASM S3Driver::setGC(__REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    BoardInfo *bi = this;
    VgaIo vga = this->vga();

    BOOL isInterlaced;
    UBYTE depth;
    UBYTE modeFlags;
    UWORD hTotal;
    UWORD screenWidth;

    DFUNC(INFO,
          "W %ld, H %ld, HTotal %ld, HBlankSize %ld, HSyncStart %ld, HSyncSize "
          "%ld, "
          "\nVTotal %ld, VBlankSize %ld,  VSyncStart %ld ,  VSyncSize %ld\n",
          (ULONG)mi->Width, (ULONG)mi->Height, (ULONG)mi->HorTotal, (ULONG)mi->HorBlankSize, (ULONG)mi->HorSyncStart,
          (ULONG)mi->HorSyncSize, (ULONG)mi->VerTotal, (ULONG)mi->VerBlankSize, (ULONG)mi->VerSyncStart,
          (ULONG)mi->VerSyncSize);

    bi->ModeInfo = mi;
    bi->Border   = border;

    this->waitBlitter();

    // Disable Clock Doubling
#if !BUILD_VISION864
    // W_SR_MASK(0x15, /*BIT(4) |*/ BIT(6), 0);
    vga.writeSRMask(0x18, BIT(7), 0);
#else
    vga.writeCRMask(0x43, BIT(0), 0x00);
#endif

    hTotal       = mi->HorTotal;
    screenWidth  = mi->Width;
    modeFlags    = mi->Flags;
    isInterlaced = (modeFlags & GMF_INTERLACE) != 0;

    depth = mi->Depth;
    if (depth <= 8) {
        if ((border == 0) || ((mi->HorBlankSize == 0 || (mi->VerBlankSize == 0)))) {
            D(INFO, "8-Bit Mode, NO Border\n");
            // Bit 5 BDR SEL - Blank/Border Select
            // 1 = BLANK is active during entire display inactive period (no border)
            vga.writeCRMask(0x33, 0x20, 0x20);
        } else {
            D(INFO, "8-Bit Mode, Border\n");
            // Bit 5 BDR SEL - Blank/Border Select
            // o = BLANK active time is defined by CR2 and CR3
            vga.writeCRMask(0x33, 0x20, 0x0);
        }

        // Disable horizontal counter double mode used for 16/32bit modes
        vga.writeCRMask(0x43, 0x80, 0x00);

#if BUILD_VISION864 && 0
        // FIXME: Do we ever overrun the max register size?
        if (hTotal > ((2 << 9) - 1 + 5)) {
            hTotal /= 2;
            ScreenWidth /= 2;
            vga.writeCRMask(0x43, BIT(7), BIT(7));
        }
#endif

        if (modeFlags & GMF_DOUBLECLOCK) {
            DFUNC(INFO, "Double-Clock Mode\n");
#if !BUILD_VISION864
            // CLKSYN Control 2 Register (SR15)
            // Bit 4 DCLK/2 - Divide DCLK by 2
            // Either this bit or bit 6 of this register must be set to 1 for clock
            // doubled RAMDAC operation (mode 0001).
            // This essentially makes DCLK = VCLK/2, which is then again doubled by the RAMDAC for the pixel clock
            // W_SR_MASK(0x15, BIT(6), BIT(6));

            // RAMDAC/CLKSYN Control Register (SR18)
            // Bit 7 CLKx2 - Enable clock doubled mode
            // 1 = RAMDAC clock doubled mode (0001) enabled
            // This bit must be set to 1 when mode 0001 is specified in bits 7-4
            // of CR67 or SRC. Either bit 4 or bit 6 of SR15 must also be set to 1.
            vga.writeSRMask(0x18, BIT(7), BIT(7));
#endif
        }
    } else if (depth <= 16) {
        D(INFO, "16-Bit Mode, No Border\n");
        // Bit 5 BDR SEL - Blank/Border Select
        // o = BLANK active time is defined by CR2 and CR3
        vga.writeCRMask(0x33, 0x20, 0x0);

        // Double all horizontal parameters.
        vga.writeCRMask(0x43, 0x80, 0x80);
        border = 0;
    } else {
        D(INFO, "24-Bit Mode, No Border\n");
        // Bit 5 BDR SEL - Blank/Border Select
        // 0 = BLANK active time is defined by CR2 and CR3
        vga.writeCRMask(0x33, 0x20, 0x0);

#if BUILD_VISION864
        // Double all horizontal parameters.
        vga.writeCRMask(0x43, 0x80, 0x80);
        // And double again. We need x4 "dot clocks"
        hTotal      = hTotal * 2;
        screenWidth = screenWidth * 2;
#else
        // Reset doubling all horizontal parameters.
        vga.writeCRMask(0x43, 0x80, 0x00);
#endif
        border = 0;
    }

#define ADJUST_HBORDER(x) adjustBorder(x, border, 8)
#define ADJUST_VBORDER(y) adjustBorder(y, border, 1);
#define TO_CLKS(x)        ((x) >> 3)
#define TO_SCANLINES(y)   toScanLines((y), modeFlags)

    {
        // Horizontal Total (CRO)
        UWORD hTotalClk = TO_CLKS(hTotal) - 5;
        D(INFO, "Horizontal Total %ld\n", (ULONG)hTotalClk);
        vga.writeCROverflow1(hTotalClk, 0x0, 0, 8, 0x5D, 0, 1);
        // Interlace Retrace Start Register (IL_RTSTART) (CR3C)
        vga.writeCR(0x3c, hTotalClk >> 1);
    }
    {
        // Horizontal Display End Register (H_D_END) (CR1)
        // One less than the total number of displayed characters
        // This register defines the number of character clocks for one line of the
        // active display. Bit 8 of this value is bit 1 of CR5D.
        UWORD hDisplayEnd = TO_CLKS(screenWidth) - 1;
        D(INFO, "Display End %ld\n", (ULONG)hDisplayEnd);
        vga.writeCROverflow1(hDisplayEnd, 0x1, 0, 8, 0x5d, 1, 1);
    }

    UWORD hBorderSize = ADJUST_HBORDER(mi->HorBlankSize);
    {
        // AR11 register defines the overscan or border color displayed on the CRT
        // screen. The overscan color is displayed when both BLANK and DE (Display
        // Enable) signals are inactive.
        UWORD hBlankStart = TO_CLKS(screenWidth + hBorderSize);
        // Start Horizontal Blank Register (S_H_BLNKI (CR2))
        D(INFO, "Horizontal Blank Start %ld\n", (ULONG)hBlankStart);
        vga.writeCROverflow1(hBlankStart, 0x2, 0, 8, 0x5d, 2, 1);
    }

    {
        // End Horizontal Blank Register (E_H_BLNKI (CR3)
        UWORD hBlankEnd = TO_CLKS(hTotal - hBorderSize) - 1;
        D(INFO, "Horizontal Blank End %ld\n", (ULONG)hBlankEnd);
        //    W_CR_OVERFLOW2(hBlankEnd, 0x3, 0, 5, 0x5, 7, 1, 0x5d, 3, 1);
        vga.writeCROverflow1(hBlankEnd, 0x3, 0, 5, 0x5, 7, 1);
    }

    UWORD hSyncStart = TO_CLKS(screenWidth + mi->HorSyncStart);
    {
        // Start Horizontal Sync Position Register (S_H_SV _PI (CR4)
        D(INFO, "HSync start %ld\n", (ULONG)hSyncStart);
        vga.writeCROverflow1(hSyncStart, 0x4, 0, 8, 0x5d, 4, 1);
    }

    UWORD hSyncEnd = TO_CLKS(screenWidth + mi->HorSyncStart + mi->HorSyncSize) - 1;
    {
        // End Horizontal Sync Position Register (E_H_SY_P) (CR5)
        D(INFO, "HSync End %ld\n", (ULONG)hSyncEnd);
        vga.writeCRMask(0x5, 0x1f, hSyncEnd);
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
        if (hSyncEnd > startDisplayFifo) {
            startDisplayFifo = hSyncEnd + 1;
        }
        D(INFO, "Start Display Fifo %ld\n", (ULONG)startDisplayFifo);
        vga.writeCROverflow1(startDisplayFifo, 0x3b, 0, 8, 0x5d, 6, 1);
    }

    {
        // Vertical Total (CR6)
        UWORD vTotal = TO_SCANLINES(mi->VerTotal) - 2;
        D(INFO, "VTotal %ld\n", (ULONG)vTotal);
        vga.writeCROverflow3(vTotal, 0x6, 0, 8, 0x7, 0, 1, 0x7, 5, 1, 0x5e, 0, 1);
    }

    {
        // Vertical Display End register (CR12)
        UWORD vDisplayEnd = TO_SCANLINES(mi->Height) - 1;
        D(INFO, "Vertical Display End %ld\n", (ULONG)vDisplayEnd);
        vga.writeCROverflow3(vDisplayEnd, 0x12, 0, 8, 0x7, 1, 1, 0x7, 6, 1, 0x5e, 1, 1);
    }

    UWORD vBlankSize = ADJUST_VBORDER(mi->VerBlankSize);
    {
        // Start Vertical Blank Register (SVB) (CR15)
        UWORD vBlankStart = TO_SCANLINES(mi->Height + vBlankSize);
        D(INFO, "VBlank Start %ld\n", (ULONG)vBlankStart);
        vga.writeCROverflow3(vBlankStart, 0x15, 0, 8, 0x7, 3, 1, 0x9, 5, 1, 0x5e, 2, 1);
    }

    {
        // End Vertical Blank Register (EVB) (CR16)
        UWORD vBlankEnd = TO_SCANLINES(mi->VerTotal - vBlankSize) - 1;
        D(6, "VBlank End %ld\n", (ULONG)vBlankEnd);
        vga.writeCR(0x16, vBlankEnd);
    }

    UWORD vRetraceStart = TO_SCANLINES(mi->Height + mi->VerSyncStart);
    {
        // Vertical Retrace Start Register (VRS) (CR10)
        D(INFO, "VRetrace Start %ld\n", (ULONG)vRetraceStart);
        vga.writeCROverflow3(vRetraceStart, 0x10, 0, 8, 0x7, 2, 1, 0x7, 7, 1, 0x5e, 4, 1);
    }

    {
        // Vertical Retrace End Register (VRE) (CR11) Bits 3-0 VERTICAL RETRACE END
        // Value = least significant 4 bits of the scan line counter value at which
        // VSYNC goes in active. To obtain this value, add the desired VSYNC pulse
        // width in scan line units to the CR10 value, also in scan line units. The
        // 4 1east significant bits of this sum are programmed into this field.
        // This allows a maximum VSYNC pulse width of 15 scan line units.
        UWORD vRetraceEnd = TO_SCANLINES(mi->Height + mi->VerSyncStart + mi->VerSyncSize) - 1;
        D(INFO, "VRetrace End %ld\n", (ULONG)vRetraceEnd);
        vga.writeCRMask(0x11, 0x0F, vRetraceEnd);
    }

    // Enable Interlace
    {
        UBYTE interlace = vga.readCR(0x42) & 0xdf;
        if (isInterlaced) {
            interlace = interlace | 0x20;
        }
        vga.writeCR(0x42, interlace);
    }

    // Enable Doublescan
    {
        UBYTE dblScan = vga.readCR(0x9) & 0x7f;
        if ((modeFlags & GMF_DOUBLESCAN) != 0) {
            dblScan = dblScan | 0x80;
        }
        vga.writeCR(0x9, dblScan);
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
        vga.writeMiscMask(0xC0, polarities);
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

    vga.writeCRMask(0x54, 0xFC, 0x18);

    {
        // Extended Memory Control 3 Register (EXT-MCTL-3) (CR60)
        // Bits 7-0 N(DISP-FETCH-PAGE) - N Parameter
        // Value = Number of MCLKs allocated to Streams Processor FIFO filling
        // before control of the memory bus is relinquished. See Section 7.5 for
        // more information.
        vga.writeCR(0x60, 0xff);
    }
    // Backward Compatibility 3 Register (BKWD_3) (CR34)
    // Bit 4 ENB SFF - Enable Start Display FIFO Fetch Register(CR3B)
    vga.writeCR(0x34, 0x10);

    {
        LOCAL_SYSBASE();
        Disable();

        // Atttribute Controller Index register to AR11 while preserving "Bit 5 ENB
        // PLT - Enable Video Display"

        vga.readB(VgaReg::INSTAT1);
        // write AR11 = 0 Border Color Register
        vga.writeAR(0x11, 0);

        // Re-enable video out
        vga.writeB(VgaReg::ATTR_AD, 0x20);
        vga.readB(VgaReg::INSTAT1);

        Enable();
    }
}

void ASM S3Driver::setPanning(__REGA1(UBYTE *memory), __REGD0(UWORD width), __REGD3(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset), __REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    VgaIo vga = this->vga();
    LOCAL_SYSBASE();

    DFUNC(INFO,
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

    this->waitBlitter();

    pitch /= 8;
    panOffset = (panOffset + memOffset) / 4;

    D(INFO, "panOffset 0x%lx, pitch %ld dwords\n", panOffset, (ULONG)pitch);
    // Start Address Low Register (STA(L)) (CRD)
    // Start Address High Register (STA(H)) (CRC)
    // Extended System Control 3 Register (EXT-SCTL-3)(CR69)
    vga.writeCROverflow2U(panOffset, 0xd, 0, 8, 0xc, 0, 8, 0x69, 0, 4);

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

    vga.writeCROverflow1(pitch, 0x13, 0, 8, 0x51, 4, 2);

    // Bits 5-4 of CR51 are extension bits 9-8 of this register. If these bits are
    // OOb, bit 2 of CR43 is extension bit 8 of this register.
    //  W_CR_MASK(0x43, 0x04, (pitch >> 6) & 0x04);

    Disable();

    vga.readB(VgaReg::INSTAT1);  // Reset AFF flip-flop // FIXME: why?

    Enable();
    return;
}

APTR ASM S3Driver::calculateMemory(__REGA1(APTR mem), __REGD0(struct RenderInfo *ri), __REGD7(RGBFTYPE_REG format))
{
#if HAS_ROXXLER
    return mem;
#else
#if !BUILD_VISION864
    if (chip()->chipFamily >= TRIO64PLUS) {
        switch (format) {
        case RGBFB_A8R8G8B8:
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
            // Redirect to Big Endian Linear Address Window.
            return (APTR)((UBYTE *)mem + 0x2000000);
            break;
        default:
            return mem;
            break;
        }
    }
#endif
#endif
    return mem;
}

ULONG ASM S3Driver::getCompatibleFormats(__REGD7(RGBFTYPE_REG format))
{
DFUNC(VERBOSE, "Format %ld\n", (ULONG)format);

    if (format == RGBFB_NONE)
        return (ULONG)0;

#if HAS_ROXXLER
    ULONG compatible = BIT(format);
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
    default:
        // all little-endian formats are compatible to each other
        compatible |= RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;
    }
#else
    // These formats can always reside in the Little Endian Window.
    // We never need to change any aperture setting for them
    ULONG compatible = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;

#if !BUILD_VISION864
    if (chip()->chipFamily >= TRIO64PLUS) {
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
#endif
    return compatible;
}

BOOL ASM S3Driver::setDisplay(__REGD0(BOOL state))
{
// Clocking Mode Register (ClK_MODE) (SR1)
    VgaIo vga = this->vga();

    DFUNC(VERBOSE, " state %ld\n", (ULONG)state);

    vga.writeSRMask(0x01, 0x20, (~(UBYTE)state & 1) << 5);



    //  R_REG(0x3DA);
    //  W_REG(ATR_AD, 0x20);
    //  R_REG(0x3DA);

    return TRUE;
}

LONG ASM S3Driver::resolvePixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG pixelClock), __REGD7(RGBFTYPE_REG RGBFormat))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "ModeInfo 0x%lx pixelclock %ld, format %ld\n", mi, pixelClock, (ULONG)RGBFormat);

    const ChipData_t *cd = chip();

    mi->Flags &= ~GMF_ALWAYSBORDER;
    if (0x3ff < mi->VerTotal) {
        mi->Flags |= GMF_ALWAYSBORDER;
    }
    // Figure out if we can/need to make use of double clocking
    mi->Flags &= ~GMF_DOUBLECLOCK;

    // Enable Double Clock for 8Bit modes when required pixelclock exceeds 80Mhz
    if (RGBFormat == RGBFB_CLUT || RGBFormat == RGBFB_NONE) {
        if (pixelClock > 67500000) {
            D(VERBOSE, "Applying pixel multiplex clocking\n")
            mi->Flags |= GMF_DOUBLECLOCK;
        }
    }
#if BUILD_VISION864
    if (getBPP(RGBFormat) >= 3) {
        // In 24/32bit modes, it takes 2 clock cycles to transfer one pixel to the RAMDAC,
        // and for the RAMDAC to output this pixel. Therefore, we need to double VCLK
        D(VERBOSE, "Applying 2x clocking\n");
        pixelClock *= 2;
    }
#endif

    UWORD targetFreq10khz = (UWORD)(pixelClock / 10000);  // 10 kHz units

    // Find the best matching PLL entry using binary search (on stored freq10khz)
    UWORD upper     = cd->numPllValues - 1;
    UWORD upperFreq = cd->pllValues[upper].freq10khz;
    UWORD lower     = 0;
    UWORD lowerFreq = cd->pllValues[lower].freq10khz;

    while (lower + 1 < upper) {
        UWORD middle     = (upper + lower) / 2;
        UWORD middleFreq = cd->pllValues[middle].freq10khz;

        if (targetFreq10khz > middleFreq) {
            lower     = middle;
            lowerFreq = middleFreq;
        } else {
            upper     = middle;
            upperFreq = middleFreq;
        }
    }

    // Return the best match between upper and lower
    if ((ULONG)targetFreq10khz - (ULONG)lowerFreq > (ULONG)upperFreq - (ULONG)targetFreq10khz) {
        lower     = upper;
        lowerFreq = upperFreq;
    }

    mi->PixelClock = (ULONG)cd->pllValues[lower].freq10khz * 10000;  // 10 kHz -> Hz
#if BUILD_VISION864
    if (getBPP(RGBFormat) >= 3) {
        mi->PixelClock /= 2;  // Compensate for the earlier doubling of pixel clock for 24/32bpp modes
    }
#endif

    PLLValue_t pllValues = cd->pllValues[lower];

    // FIXME: There's a note in the manual saying that fDCLK > fSCLK "to ensure proper PLL writes"
    //  I take SCLK as the 33Mhz PCI clock, thus DCLK must be greater than 16.5Mhz

    DFUNC(CHATTY, "Reporting pixelclock Hz: %ld, index: %ld,  M:%ld N:%ld R:%ld \n\n", mi->PixelClock, (ULONG)lower,
          (ULONG)pllValues.m, (ULONG)pllValues.n, (ULONG)pllValues.r);

    // Store PLL values in the format expected by SetClock via RAMDAC ops
    cd->ramdacOps->packPllToModeInfo(bi, pllValues.m, pllValues.n, pllValues.r, mi);

#if !BUILD_VISION864
    if ((mi->Flags & GMF_DOUBLECLOCK) && cd->chipFamily >= TRIO64PLUS) {
        // Bit 7 CLKx2 - Enable clock doubled mode
        // 0 = RAMDAC clock doubled mode (0001) disabled
        // 1 = RAMDAC clock doubled mode (0001) enabled
        // This bit must be set to 1 when mode 0001 is specified in bits 7-4 of CR67 or SRC.
        // Either bit 4 or bit 6 of SR15 must also be set to 1. This bit has the same function as
        // SR18_7. It allows enabling of clock doubling at the same time as the PLL parameters
        // are programmed, resulting in more controlled VCO operation.

        // FIXME: This confuses at least the S3TrioV264, so disable it for now
        // mi->pll2.Denominator |= 0x80;  // Set bit 7 to indicate double clocking;
    }
#else

#endif
    return lower;  // Return the index into the PLL table
}

ULONG ASM S3Driver::getPixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG index), __REGD7(RGBFTYPE_REG format))
{
DFUNC(INFO, "Index: %ld\n", index);

    const ChipData_t *cd = chip();

    if (index >= cd->numPllValues) {
        DFUNC(ERROR, "Invalid pixel clock index %ld (max %ld)\n", index, cd->numPllValues - 1);
        return 0;
    }

    ULONG pixelClock = (ULONG)cd->pllValues[index].freq10khz * 10000;  // 10 kHz -> Hz

#if BUILD_VISION864
    if (getBPP(format) >= 3) {
        pixelClock /= 2;  // Compensate for the earlier doubling of pixel clock for 24/32bpp modes
    }
#endif

    return pixelClock;  // 10 kHz -> Hz
}

void ASM S3Driver::setClock()
{
    BoardInfo *bi = this;
    DFUNC(INFO, "\n");

    const ChipData_t *cd = chip();
    struct ModeInfo *mi  = bi->ModeInfo;

    D(INFO, "SetClock: PixelClock %ldHz\n", mi->PixelClock);

    cd->ramdacOps->setClock(bi);
}

void S3Driver::setMemoryModeInternal(RGBFTYPE format)
{
    DFUNC(VERBOSE, "Format %ld\n", (LONG)format);
#if HAS_ROXXLER
    if (chip()->MemFormat == format) {
        return;
    }
    chip()->MemFormat = format;

    // Setup ROXXLER to either word-swap or doubleword-swap, depending on graphics format
    switch (format) {
    case RGBFB_A8R8G8B8:
        // swap all the bytes within a double word
        this->cv64().writeMask(CV64_SWAP32_BIT | CV64_SWAP16_BIT, CV64_SWAP32_BIT);
        break;
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        // Just swap the bytes within a word
        this->cv64().writeMask(CV64_SWAP32_BIT | CV64_SWAP16_BIT, CV64_SWAP16_BIT);
        break;
    default:
        this->cv64().writeMask(CV64_SWAP32_BIT | CV64_SWAP16_BIT, 0);
        break;
    }

#else

#if !BUILD_VISION864
    VgaIo vga = this->vga();

    if (chip()->chipFamily >= TRIO64PLUS)  // Trio64+?
    {
        if (chip()->MemFormat == format) {
            return;
        }
        chip()->MemFormat = format;

        this->waitBlitter();

        // Setup the linear window CPU access such that the below formats will be
        // converted to the actual framebuffer format on write/read
        switch (format) {
        case RGBFB_A8R8G8B8:
            // swap all the bytes within a double word
            vga.writeCRMask(0x53, 0x06, 0x04);
            break;
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
            // Just swap the bytes within a word
            vga.writeCRMask(0x53, 0x06, 0x02);
            break;
        default:
            vga.writeCRMask(0x53, 0x06, 0x00);
            break;
        }
    }
#endif
#endif
    return;
}

void ASM S3Driver::setMemoryMode(__REGD7(RGBFTYPE_REG format))
{
#if !BUILD_VISION864
this->setMemoryModeInternal(AS_RGBF(format));
#endif
}

void ASM S3Driver::setWriteMask(__REGD0(UBYTE mask))
{
    (void)mask;
}

void ASM S3Driver::setClearMask(__REGD0(UBYTE mask))
{
}

void ASM S3Driver::setReadPlane(__REGD0(UBYTE mask))
{
}

BOOL ASM S3Driver::getVSyncState(__REGD0(BOOL expected))
{
DFUNC(VERBOSE, "\n");
    VgaIo vga = this->vga();
    return (vga.readB(VgaReg::INSTAT1) & 0x08) != 0;
}

// FIXME: implement, but make sure to coordinate with SetDPMSLevel
void ASM S3Driver::waitVerticalSync(__REGD0(BOOL waitForEnd))
{
DFUNC(VERBOSE, "waitForEnd: %ld\n", (ULONG)waitForEnd);
    VgaIo vga = this->vga();

    if (vga.readSR(0x1) & BIT(5)) {
        // Don't attempt to time vertical sync if the display is off
        // Though SR1 promised to still generate the HSYNC/VSYNC signals
        return;
    }
    if (waitForEnd) {
        // wait for vertical retrace end
        // Quiet path / VgaIoQ if debug serial would miss the signals
        while (!(vga.readB(VgaReg::INSTAT1) & 0x08)) {
        };
        // For pixel display (should now be top of frame, i.e. end of retrace)
        while (!(vga.readB(VgaReg::INSTAT1) & 0x01)) {
        };
    } else {  // For pixel display first
        while (!(vga.readB(VgaReg::INSTAT1) & 0x01)) {
        };
        // wait for vertical retrace starting
        while (!(vga.readB(VgaReg::INSTAT1) & 0x08)) {
        };
    }
}

/* VGA CR11: bit4 = vert IRQ clear/arm, bit5 = 1 disables vert IRQ.
 * INPUTSTATUS0 (0x3C2) bit7 = this CRTC has a pending IRQ. */
BOOL ASM S3Driver::setInterrupt(__REGD0(BOOL state))
{
    BoardInfo *bi = this;
VgaIo vga = this->vga();
    LOCAL_SYSBASE();
    Disable();

    UBYTE idx = vga.readB(VgaReg::CRTC_INDEX);
    vga.writeB(VgaReg::CRTC_INDEX, 0x11);
    UBYTE cr11 = vga.readB(VgaReg::CRTC_VALUE);
    if (state)
        cr11 = (cr11 & ~BIT(5)) | BIT(4);
    else
        cr11 = (cr11 | BIT(5)) & ~BIT(4);
    vga.writeB(VgaReg::CRTC_VALUE, cr11);
    vga.writeB(VgaReg::CRTC_INDEX, idx);

    Enable();
    return TRUE;
}

/* Non-static method; free VBlankInterruptHandler trampoline + DEFINE_INTSERVER below. */
ULONG __attribute__((noinline)) S3Driver::interruptServer()
{
    BoardInfo *bi = this;
    VgaIoQ vga = this->vgaQ();

    if (!(vga.readB(VgaReg::MISC_OUT_W) & BIT(7)))
        return 0;

    UBYTE idx = vga.readB(VgaReg::CRTC_INDEX);
    vga.writeB(VgaReg::CRTC_INDEX, 0x11);
    UBYTE cr11 = vga.readB(VgaReg::CRTC_VALUE);
    vga.writeB(VgaReg::CRTC_VALUE, cr11 & ~BIT(4));
    vga.writeB(VgaReg::CRTC_VALUE, cr11 | BIT(4));
    vga.writeB(VgaReg::CRTC_INDEX, idx);

    {
        struct ExecBase *SysBase = bi->ExecBase;
        Cause(&bi->SoftInterrupt);
    }
    return 1;
}

void ASM S3Driver::setDPMSLevel(__REGD0(ULONG level))
{
//  DPMS_ON,      /* Full operation                             */
    //  DPMS_STANDBY, /* Optional state of minimal power reduction  */
    //  DPMS_SUSPEND, /* Significant reduction of power consumption */
    //  DPMS_OFF      /* Lowest level of power consumption */

    static const UBYTE DPMSLevels[4] = {0x00, 0x90, 0x60, 0x50};

    VgaIo vga = this->vga();
    vga.writeSRMask(0xD, 0xF0, DPMSLevels[level]);
}

void ASM S3Driver::setSplitPosition(__REGD0(SHORT splitPos))
{
    BoardInfo *bi = this;
    VgaIo vga = this->vga();
    DFUNC(VERBOSE, "%ld\n", (ULONG)splitPos);

    bi->YSplit = splitPos;
    if (!splitPos) {
        splitPos = 0x7ff;
    } else {
        if (bi->ModeInfo->Flags & GMF_DOUBLESCAN) {
            splitPos *= 2;
        }
    }
    vga.writeCROverflow3((UWORD)splitPos, 0x18, 0, 8, 0x7, 4, 1, 0x9, 6, 1, 0x5e, 6, 1);
}

void ASM S3Driver::setSpritePosition(__REGD0(WORD xpos), __REGD1(WORD ypos), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "\n");
    VgaIo vga = this->vga();

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
    vga.writeCROverflow1(spriteX, 0x47, 0, 8, 0x46, 0, 8);
    vga.writeCROverflow1(spriteY, 0x49, 0, 8, 0x48, 0, 8);
    vga.writeCR(0x4e, offsetX & 63);
    vga.writeCR(0x4f, offsetY & 63);
}

void ASM S3Driver::setSpriteImage(__REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "\n");
#if HAS_ROXXLER
    // Temporarily switch to little endian
    Cv64Cached cv64 = this->cv64();
    UBYTE cv64Reg   = cv64.get();
    cv64.writeMask(CV64_SWAP32_BIT | CV64_SWAP16_BIT, 0);
#endif
#if BUILD_VISION864 && 0
    // FIXME: need to set temporary memory format?
    // No, MouseImage should be in little endian window and not affected

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
    UWORD *cursor = (UWORD *)bi->MouseImageBuffer;
    UWORD height  = bi->MouseHeight;
    if (height > 64)
        height = 64;

    if (bi->Flags & BIF_HIRESSPRITE) {
        const ULONG *image = (const ULONG *)bi->MouseImage + 2;
        for (UWORD y = 0; y < height; ++y) {
            ULONG plane0 = *image++;
            ULONG plane1 = *image++;

            ULONG andMask = ~plane0;
            ULONG xorMask = plane1;
            *cursor++     = (UWORD)(andMask >> 16);
            *cursor++     = (UWORD)(xorMask >> 16);
            *cursor++     = (UWORD)andMask;
            *cursor++     = (UWORD)xorMask;
            for (UWORD p = 0; p < 2; ++p) {
                *cursor++ = 0xFFFF;
                *cursor++ = 0x0000;
            }
        }
    } else if (bi->Flags & BIF_BIGSPRITE) {
        UWORD srcH = height >> 1;
        if (srcH > 32)
            srcH = 32;
        const UWORD *image = bi->MouseImage + 2;
        for (UWORD y = 0; y < srcH; ++y) {
            UWORD plane0  = *image++;
            UWORD plane1  = *image++;
            ULONG andMask = expandBits2x((UWORD)~plane0);
            ULONG xorMask = expandBits2x(plane1);
            UWORD row[8];
            row[0] = (UWORD)(andMask >> 16);
            row[1] = (UWORD)(xorMask >> 16);
            row[2] = (UWORD)andMask;
            row[3] = (UWORD)xorMask;
            row[4] = 0xFFFF;
            row[5] = 0x0000;
            row[6] = 0xFFFF;
            row[7] = 0x0000;
            for (UWORD r = 0; r < 2; ++r) {
                for (UWORD p = 0; p < 8; ++p)
                    *cursor++ = row[p];
            }
        }
        height = srcH * 2;
    } else {
        const UWORD *image = bi->MouseImage + 2;
        for (UWORD y = 0; y < height; ++y) {
            UWORD plane0 = *image++;
            UWORD plane1 = *image++;

            UWORD andMask = ~plane0;
            UWORD xorMask = plane1;
            *cursor++     = andMask;
            *cursor++     = xorMask;
            for (UWORD p = 0; p < 3; ++p) {
                *cursor++ = 0xFFFF;
                *cursor++ = 0x0000;
            }
        }
    }
    for (UWORD y = height; y < 64; ++y) {
        for (UWORD p = 0; p < 4; ++p) {
            *cursor++ = 0xFFFF;
            *cursor++ = 0x0000;
        }
    }
#endif

    LOCAL_SYSBASE();
    CacheClearU();

#if HAS_ROXXLER
    this->cv64().write(cv64Reg);
#endif
}

void ASM S3Driver::setSpriteColor(__REGD0(UBYTE index), __REGD1(UBYTE red), __REGD2(UBYTE green), __REGD3(UBYTE blue), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
DFUNC(VERBOSE, "Index %ld, Red %ld, Green %ld, Blue %ld\n", (ULONG)index, (ULONG)red, (ULONG)green, (ULONG)blue);
    VgaIo vga = this->vga();
    LOCAL_SYSBASE();

    if (index != 0 && index != 2)
        return;

    UBYTE reg = 0;

#if BUILD_VISION864
    if (fmt == RGBFB_CLUT) {
        // This seems to contradict the specs, but works
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

    vga.readCR(0x45);  // Reset "Graphics Cursor Stack"
    switch (fmt) {
    case RGBFB_NONE:
    case RGBFB_CLUT: {
        UBYTE paletteEntry;
        if (index == 0) {
            paletteEntry = 17;  // Cursor Palette entry is fixed
        } else {
            paletteEntry = 19;
        }
        vga.writeCR(reg, paletteEntry);
        vga.writeB(VgaReg::CRTC_VALUE, paletteEntry);
        // W_REG(CRTC_DATA, paletteEntry);
        // W_REG(CRTC_DATA, paletteEntry);
    } break;
    case RGBFB_B8G8R8A8:
    case RGBFB_A8R8G8B8: {
        // No Conversion needed for 24bit RGB
#if BUILD_VISION864
        vga.writeCR(reg, red);
        vga.writeB(VgaReg::CRTC_VALUE, 0);
        vga.writeB(VgaReg::CRTC_VALUE, blue);
        vga.writeB(VgaReg::CRTC_VALUE, green);
#else
        vga.writeCR(reg, blue);
        vga.writeB(VgaReg::CRTC_VALUE, green);
        vga.writeB(VgaReg::CRTC_VALUE, red);
#endif
    } break;
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G5B5: {
        UBYTE a = (blue >> 3) | ((green << 2) & 0xe);  // 16bit, just need to write the first two byte
        UBYTE b = (green >> 6) | ((red >> 1) & ~0x3);
        vga.writeCR(reg, a);
        vga.writeB(VgaReg::CRTC_VALUE, b);
    } break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G6B5: {
        UBYTE a = (blue >> 3) | ((green << 3) & 0xe);  // // 16bit, just need to write the first two byte
        UBYTE b = (green >> 5) | (red & 0xf8);
        vga.writeCR(reg, a);
        vga.writeB(VgaReg::CRTC_VALUE, b);
    } break;
    }
}

BOOL ASM S3Driver::setSprite(__REGD0(BOOL activate), __REGD7(RGBFTYPE_REG RGBFormat))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "\n");
    VgaIo vga = this->vga();

    vga.writeCRMask(0x45, 0x01, activate ? 0x01 : 0x00);

    if (activate) {
        this->setSpriteColor( 0, bi->CLUT[17].Red, bi->CLUT[17].Green, bi->CLUT[17].Blue, bi->RGBFormat);
        this->setSpriteColor( 1, bi->CLUT[18].Red, bi->CLUT[18].Green, bi->CLUT[18].Blue, bi->RGBFormat);
        this->setSpriteColor( 2, bi->CLUT[19].Red, bi->CLUT[19].Green, bi->CLUT[19].Blue, bi->RGBFormat);
    }

    return TRUE;
}


#define MByte(x) ((x) * (1024 * 1024))
static INLINE void REGARGS getGESegmentAndOffset(ULONG memOffset, WORD bytesPerRow, UBYTE bpp, UWORD *segment,
                                                 UWORD *xoffset, UWORD *yoffset)
{
    *segment = (memOffset >> 20) & 3;

    ULONG segOffset = memOffset & 0xFFFFF;
    *yoffset        = segOffset / bytesPerRow;
    *xoffset        = (segOffset % bytesPerRow) / bpp;

#ifdef DBG
    if (*segment > 0) {
        D(VERBOSE, "segment %ld, xoff %ld, yoff %ld, memoffset 0x%08lx\n", (ULONG)*segment, (ULONG)*xoffset,
          (ULONG)*yoffset, memOffset);
    }
#endif
}

ULONG S3Driver::getMemoryOffset(APTR memory)
{
    return (ULONG)memory - (ULONG)this->MemoryBase;
}

BOOL S3Driver::setGEFormat(UWORD bytesPerRow, UBYTE bpp)
{
    VgaIo vga = this->vga();

    ChipData_t *cd = chip();
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
        DFUNC(WARN, "pitch %ld bytes unsupported by GE\n", (ULONG)width);
        return FALSE;  // reserved
    }

    cd->patternCacheKey = ~0;  // Force pattern update because pitch may have changed
    cd->GEbytesPerRow   = bytesPerRow;
    cd->GEbpp           = bpp;

    getGESegmentAndOffset(this->getMemoryOffset(cd->patternVideoBuffer), bytesPerRow, bpp, &cd->pattSegment, &cd->pattX,
                          &cd->pattY);
    // Pattern Fill. Same as a BitBit except that an 8x8 patterned rectangle is
    // transferred repeatedly to the destination rectangle. The starting X coordinate of
    // the source rectangle should always be on an 8 pixel boundary.
    cd->pattX = (cd->pattX + 7) & ~7;  // Align to 8 pixel boundary

    cd->patternCacheKey = ~0;  // invalidate cache as  the pattern address may have moved
    D(CHATTY, "pattSeg %ld, pattX %ld, pattY %ld, bytesPerRow %ld\n", (ULONG)cd->pattSegment, (ULONG)cd->pattX,
      (ULONG)cd->pattY, (ULONG)cd->GEbytesPerRow);

    this->waitBlitter();

    vga.writeCRMask(0x50, 0xF1, CR50_76_0 | ((bpp - 1) << 4));
    vga.writeCRMask(0x31, (1 << 1), CR31_1);

    S3Mmio mmio = this->mmio();
    if (bpp >= 3) {
        this->writeBee8(MULT_MISC, BIT(9));
        mmio.writeL(S3_MMIO_ID(WRT_MASK), ~0);
        // Invalidate the mask and fg/bg color caches because we need to write them again.
        // Only in 32bpp mode, these registers are 32bit
        cd->GEmask  = 0xFF;
        cd->GEbgPen = cd->GEfgPen = 0x80000001;
    } else {
        this->writeBee8(MULT_MISC, 0);
        cd->GEmask  = 0x0;
        cd->GEbgPen = cd->GEfgPen = 0x80000001;
    }

    return TRUE;
}

static INLINE ULONG REGARGS penToColor(ULONG pen, RGBFTYPE fmt)
{
    switch (fmt) {
    case RGBFB_B8G8R8A8:
        pen = swapl(pen);
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
        pen = swapw(pen);
    default:
        break;
    }
    return pen;
}

static INLINE void REGARGS drawModeToMixMode(UBYTE drawMode, UWORD *frgdMix, UWORD *bkgdMix)
{
    UWORD f = CLR_SRC_FRGD_COLOR, g = CLR_SRC_BKGD_COLOR;
    switch (drawMode & (JAM1 | JAM2 | COMPLEMENT)) {
    case JAM1:
        f |= MIX_NEW;
        g |= MIX_CURRENT;
        break;
    case JAM2:
        f |= MIX_NEW;
        g |= MIX_NEW;
        break;
    case COMPLEMENT:
        f |= MIX_NOT_CURRENT;
        g |= MIX_CURRENT;
    }
    if (drawMode & INVERSVID) {
        // Swap the foreground and background
        UWORD t = f;
        f       = g;
        g       = t;
    }
    *frgdMix = f;
    *bkgdMix = g;
}

void S3Driver::setMix(UWORD frgdMix, UWORD bkgdMix)
{
    S3Mmio mmio = this->mmio();
#if HAS_PACKED_MMIO
    mmio.writeL(S3_MMIO_ID(ALT_MIX), makeDWORD(frgdMix, bkgdMix));
#else
    mmio.writeW(S3_MMIO_ID(FRGD_MIX), frgdMix);
    mmio.writeW(S3_MMIO_ID(BKGD_MIX), bkgdMix);
#endif
}

void S3Driver::setForegroundColor32(ULONG fgPen)
{
    S3Mmio mmio = this->mmio();
    mmio.writeL(S3_MMIO_ID(FRGD_COLOR), fgPen);
}

void S3Driver::setBackgroundColor32(ULONG bgPen)
{
    S3Mmio mmio = this->mmio();
    mmio.writeL(S3_MMIO_ID(BKGD_COLOR), bgPen);
}

void S3Driver::setForegroundColor(UWORD fgPen)
{
    S3Mmio mmio = this->mmio();
    mmio.writeW(S3_MMIO_ID(FRGD_COLOR), fgPen);
}

void S3Driver::setBackgroundColor(UWORD bgPen)
{
    S3Mmio mmio = this->mmio();
    mmio.writeW(S3_MMIO_ID(BKGD_COLOR), bgPen);
}

void S3Driver::setDrawMode(ULONG FgPen, ULONG BgPen, UBYTE DrawMode, RGBFTYPE format)
{
    ChipData_t *cd = chip();

    if (cd->GEfgPen != FgPen || cd->GEbgPen != BgPen || cd->GEdrawMode != DrawMode || cd->GEFormat != format) {
        cd->GEfgPen    = FgPen;
        cd->GEbgPen    = BgPen;
        cd->GEdrawMode = DrawMode;
        cd->GEFormat   = format;

        UWORD frgdMix, bkgdMix;
        drawModeToMixMode(DrawMode, &frgdMix, &bkgdMix);
        ULONG fgColor = penToColor(FgPen, format);
        ULONG bgColor = penToColor(BgPen, format);

        switch (format) {
        case RGBFB_B8G8R8A8:
        case RGBFB_A8R8G8B8:
            this->waitFifo(6);
            this->setForegroundColor32(fgColor);
            this->setBackgroundColor32(bgColor);
            break;
        default:
            this->waitFifo(4);
            this->setForegroundColor(fgColor);
            this->setBackgroundColor(bgColor);
        }
        this->setMix(frgdMix, bkgdMix);
    }
}

void S3Driver::setGEWriteMask(UBYTE mask, RGBFTYPE fmt, BYTE waitFifoSlots)
{
    ChipData_t *cd = chip();

    // 8bit modes use the mask
    if (fmt == RGBFB_CLUT && cd->GEmask != mask) {
        cd->GEmask = mask;

        this->waitFifo(waitFifoSlots + 1);
        S3Mmio mmio = this->mmio();
        mmio.writeB(S3_MMIO_ID(WRT_MASK), mask);
    } else {
        this->waitFifo(waitFifoSlots);
    }
}

void S3Driver::setBlitSrcPosAndSize(UWORD x, UWORD y, UWORD w, UWORD h)
{
    S3Mmio mmio = this->mmio();
#if HAS_PACKED_MMIO
    mmio.writeL(S3_MMIO_ID(ALT_CURXY), makeDWORD(x, y));
    mmio.writeL(S3_MMIO_ID(ALT_PCNT), makeDWORD(w - 1, h - 1));
#else
    mmio.writeW(S3_MMIO_ID(CUR_X), x);
    mmio.writeW(S3_MMIO_ID(CUR_Y), y);
    mmio.writeW(S3_MMIO_ID(MAJ_AXIS_PCNT), w - 1);
    this->writeBee8(MIN_AXIS_PCNT, h - 1);
#endif
}

void S3Driver::setBlitDestPos(UWORD dstX, UWORD dstY)
{
    S3Mmio mmio = this->mmio();
#if HAS_PACKED_MMIO
    mmio.writeL(S3_MMIO_ID(ALT_STEP), makeDWORD(dstX, dstY));
#else
    mmio.writeW(S3_MMIO_ID(DESTX_DIASTP), dstX);
    mmio.writeW(S3_MMIO_ID(DESTY_AXSTP), dstY);
#endif
}

#define TOP_LEFT     (0b101 << 5)
#define TOP_RIGHT    (0b100 << 5)
#define BOTTOM_LEFT  (0b001 << 5)
#define BOTTOM_RIGHT (0b000 << 5)
#define POSITIVE_X   (0b001 << 5)
#define POSITIVE_Y   (0b100 << 5)
#define Y_MAJOR      (0b010 << 5)

void ASM S3Driver::fillRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(ULONG pen), __REGD5(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\npen %08lx, mask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)pen, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    UBYTE bpp = getBPP(fmt);
    if (!bpp || !this->setGEFormat(ri->BytesPerRow, bpp)) {
        DFUNC(INFO, "Fallback to FillRectDefault\n");
        bi->FillRectDefault(bi, ri, x, y, width, height, pen, mask, AS_RGBF(fmt));
        return;
    }

    UWORD seg;
    UWORD xoffset;
    UWORD yoffset;
    getGESegmentAndOffset(this->getMemoryOffset(ri->Memory), ri->BytesPerRow, bpp, &seg, (UWORD *)&xoffset, (UWORD *)&yoffset);

    x += xoffset;
    y += yoffset;

#ifdef DBG
    if ((x > (1 << 11)) || (y > (1 << 11))) {
        D(ERROR, "X %ld or Y %ld out of range\n", (ULONG)x, (ULONG)y);
    }
#endif

    ChipData_t *cd = chip();
    S3Mmio mmio = this->mmio();

    if (cd->GEOp != FILLRECT) {
        cd->GEOp = FILLRECT;

        this->waitFifo(2);
        this->writeBee8(PIX_CNTL, MASK_BIT_SRC_ONE);
        mmio.writeW(S3_MMIO_ID(FRGD_MIX), CLR_SRC_FRGD_COLOR | MIX_NEW);
    }

    this->setGEWriteMask(mask, AS_RGBF(fmt), 0);

    if (cd->GEfgPen != pen || cd->GEFormat != fmt) {
        cd->GEfgPen  = pen;
        cd->GEFormat = fmt;

        pen = penToColor(pen, AS_RGBF(fmt));

        if (bpp < 3) {
            this->waitFifo(7);
            this->setForegroundColor(pen);
        } else {
            this->waitFifo(8);
            this->setForegroundColor32(pen);
        }
    } else {
        this->waitFifo(6);
    }

    // This could/should get chached as well
    this->writeBee8(MULT_MISC2, seg << 4);

    this->setBlitSrcPosAndSize(x, y, width, height);

    UWORD cmd = CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT;

    mmio.writeW(S3_MMIO_ID(CMD), cmd);
}

void ASM S3Driver::invertRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    S3Mmio mmio = this->mmio();

    UBYTE bpp = getBPP(fmt);
    if (!bpp || !this->setGEFormat(ri->BytesPerRow, bpp)) {
        DFUNC(INFO, "Fallback to InvertRectDefault\n");
        bi->InvertRectDefault(bi, ri, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    UWORD seg;
    UWORD xoffset;
    UWORD yoffset;
    getGESegmentAndOffset(this->getMemoryOffset(ri->Memory), ri->BytesPerRow, bpp, &seg, (UWORD *)&xoffset, (UWORD *)&yoffset);

    x += xoffset;
    y += yoffset;

#ifdef DBG
    if ((x > (1 << 11)) || (y > (1 << 11))) {
        DFUNC(ERROR, "X %ld or Y %ld out of range\n", (ULONG)x, (ULONG)y);
    }
#endif

    ChipData_t *cd = chip();
    if (cd->GEOp != INVERTRECT) {
        cd->GEOp = INVERTRECT;

        this->waitFifo(2);

        this->writeBee8(PIX_CNTL, MASK_BIT_SRC_ONE);
        mmio.writeW(S3_MMIO_ID(FRGD_MIX), CLR_SRC_MEMORY | MIX_NOT_CURRENT);
    }

    this->setGEWriteMask(mask, AS_RGBF(fmt), 6);

    // This could/should get chached as well
    this->writeBee8(MULT_MISC2, seg << 4);

    this->setBlitSrcPosAndSize(x, y, width, height);

    mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT);
}

void ASM S3Driver::blitRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD srcX), __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width), __REGD5(WORD height), __REGD6(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nx1 %ld, y1 %ld, x2 %ld, y2 %ld, w %ld, \n"
          "h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt,
          (ULONG)ri->BytesPerRow, (ULONG)ri->Memory);

    S3Mmio mmio = this->mmio();

    UBYTE bpp = getBPP(fmt);
    if (!bpp || !this->setGEFormat(ri->BytesPerRow, bpp)) {
        DFUNC(INFO, "Fallback to BlitRectDefault\n");
        bi->BlitRectDefault(bi, ri, srcX, srcY, dstX, dstY, width, height, mask, AS_RGBF(fmt));
        return;
    }

    UWORD seg;
    WORD xoffset;
    WORD yoffset;
    getGESegmentAndOffset(this->getMemoryOffset(ri->Memory), ri->BytesPerRow, bpp, &seg, (UWORD *)&xoffset, (UWORD *)&yoffset);

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

    ChipData_t *cd = chip();
    if (cd->GEOp != BLITRECT) {
        cd->GEOp = BLITRECT;

        this->waitFifo(2);

        this->writeBee8(PIX_CNTL, MASK_BIT_SRC_ONE);
        mmio.writeW(S3_MMIO_ID(FRGD_MIX), CLR_SRC_MEMORY | MIX_NEW);
    }

    this->setGEWriteMask(mask, AS_RGBF(fmt), 8);

    this->writeBee8(MULT_MISC2, seg << 4 | seg);

    this->setBlitSrcPosAndSize(srcX, srcY, width, height);
    this->setBlitDestPos(dstX, dstY);

    mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_BLIT | CMD_DRAW_PIXELS | dir);
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

UWORD mintermToMixMode(UBYTE minterm)
{
    // Unfortunately, the mix modes don't seem to be classic ROP codes that would be
    // the same as minterms, so we have to translate here.
    return minTermToMix[minterm];
    //   return minterm | (minterm << 4);
}

void ASM S3Driver::blitRectNoMaskComplete(__REGA1(struct RenderInfo *sri), __REGA2(struct RenderInfo *dri), __REGD0(WORD srcX), __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width), __REGD5(WORD height), __REGD6(UBYTE minTerm), __REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nx1 %ld, y1 %ld, x2 %ld, y2 %ld, w %ld, \n"
          "h %ld\nminTerm 0x%lx fmt %ld\n"
          "sri->bytesPerRow %ld, sri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)minTerm,
          (ULONG)format, (ULONG)sri->BytesPerRow, (ULONG)sri->Memory);

    S3Mmio mmio = this->mmio();

    UWORD bytesPerRow = dri->BytesPerRow > sri->BytesPerRow ? dri->BytesPerRow : sri->BytesPerRow;
    UBYTE bpp         = getBPP(format);
    if (!bpp || !this->setGEFormat(bytesPerRow, bpp)) {
        DFUNC(INFO, "fallback to BlitRectNoMaskCompleteDefault\n");
        bi->BlitRectNoMaskCompleteDefault(bi, sri, dri, srcX, srcY, dstX, dstY, width, height, minTerm, AS_RGBF(format));
        return;
    }

    ChipData_t *cd = chip();
    if (cd->GEOp != BLITRECTNOMASKCOMPLETE) {
        cd->GEOp       = BLITRECTNOMASKCOMPLETE;
        cd->GEdrawMode = 0xFF;  // invalidate minterm cache

        this->setGEWriteMask(~0, AS_RGBF(format), 1);
        this->writeBee8(PIX_CNTL, MASK_BIT_SRC_ONE);
    }

    if (cd->GEdrawMode != minTerm) {
        cd->GEdrawMode = minTerm;

        this->waitFifo(1);
        mmio.writeW(S3_MMIO_ID(FRGD_MIX), CLR_SRC_MEMORY | mintermToMixMode(minTerm));
    }

    if (sri->BytesPerRow == dri->BytesPerRow) {
        WORD xoffset;
        WORD yoffset;
        UWORD segDst;
        getGESegmentAndOffset(this->getMemoryOffset(dri->Memory), sri->BytesPerRow, bpp, &segDst, (UWORD *)&xoffset, (UWORD *)&yoffset);

        dstX += xoffset;
        dstY += yoffset;

        UWORD segSrc;
        getGESegmentAndOffset(this->getMemoryOffset(sri->Memory), sri->BytesPerRow, bpp, &segSrc, (UWORD *)&xoffset, (UWORD *)&yoffset);

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

        this->waitFifo(8);

        this->writeBee8(MULT_MISC2, (segSrc << 4) | segDst);

        this->setBlitSrcPosAndSize(srcX, srcY, width, height);
        this->setBlitDestPos(dstX, dstY);

        mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_BLIT | CMD_DRAW_PIXELS | dir);
    } else if (sri->BytesPerRow < dri->BytesPerRow) {
        WORD xoffset;
        WORD yoffset;
        UWORD segDst;
        getGESegmentAndOffset(this->getMemoryOffset(dri->Memory), dri->BytesPerRow, bpp, &segDst, (UWORD *)&xoffset, (UWORD *)&yoffset);

        dstX += xoffset;
        dstY += yoffset;

        UBYTE *srcMem = (UBYTE *)sri->Memory;
        srcMem += srcY * sri->BytesPerRow + srcX * bpp;
        ULONG memOffset = this->getMemoryOffset(srcMem);

        this->waitFifo(2);

        for (WORD h = 0; h < height; ++h) {
            WORD x;
            WORD y;
            UWORD segSrc;
            getGESegmentAndOffset(memOffset, dri->BytesPerRow, bpp, &segSrc, (UWORD *)&x, (UWORD *)&y);

            this->waitFifo(8);
            this->writeBee8(MULT_MISC2, (segSrc << 4) | segDst);

            this->setBlitSrcPosAndSize(x, y, width, 1);
            this->setBlitDestPos(dstX, dstY + h);

            mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_BLIT | CMD_DRAW_PIXELS | TOP_LEFT);

            memOffset += sri->BytesPerRow;
        }
    } else {
        WORD xoffset;
        WORD yoffset;
        UWORD segSrc;
        getGESegmentAndOffset(this->getMemoryOffset(sri->Memory), sri->BytesPerRow, bpp, &segSrc, (UWORD *)&xoffset, (UWORD *)&yoffset);

        srcX += xoffset;
        srcY += yoffset;

        UBYTE *dstMem = (UBYTE *)dri->Memory;
        dstMem += dstY * dri->BytesPerRow + dstX * bpp;
        ULONG memOffset = this->getMemoryOffset(dstMem);

        for (WORD h = 0; h < height; ++h) {
            WORD x;
            WORD y;
            UWORD segDst;
            getGESegmentAndOffset(memOffset, sri->BytesPerRow, bpp, &segDst, (UWORD *)&x, (UWORD *)&y);

            this->waitFifo(8);
            this->writeBee8(MULT_MISC2, (segSrc << 4) | segDst);

            this->setBlitSrcPosAndSize(srcX, srcY + h, width, 1);
            this->setBlitDestPos(x, y);

            mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_BLIT | CMD_DRAW_PIXELS | TOP_LEFT);

            memOffset += dri->BytesPerRow;
        }
    }
}

void S3Driver::writePIX_TRANS(ULONG value)
{
    S3Mmio mmio = this->mmio();
#if HAS_ROXXLER
    // in a twist of luck, we need to write the swapped value here
    mmio.writeL(S3_MMIO_ID(PIX_TRANS), swapl(value));
#else
    mmio.writeL(S3_MMIO_ID(PIX_TRANS), value);
#endif
}

void ASM S3Driver::blitTemplate(__REGA1(struct RenderInfo *ri), __REGA2(struct Template *tmpl), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    UBYTE bpp = getBPP(fmt);
    if (!bpp || !this->setGEFormat(ri->BytesPerRow, bpp)) {
        DFUNC(INFO, "fallback to BlitTemplateDefault\n");
        bi->BlitTemplateDefault(bi, ri, tmpl, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    UWORD seg;
    UWORD xoffset;
    UWORD yoffset;
    getGESegmentAndOffset(this->getMemoryOffset(ri->Memory), ri->BytesPerRow, bpp, &seg, (UWORD *)&xoffset, (UWORD *)&yoffset);

    x += xoffset;
    y += yoffset;

#ifdef DBG
    if ((x > (1 << 11)) || (y > (1 << 11))) {
        DFUNC(ERROR, "X %ld or Y %ld out of range\n", (ULONG)x, (ULONG)y);
    }
#endif

    S3Mmio mmio = this->mmio();

    ChipData_t *cd = chip();

    if (cd->GEOp != BLITTEMPLATE) {
        cd->GEOp = BLITTEMPLATE;

        // Invalidate the pen and drawmode caches
        cd->GEdrawMode = 0xFF;

        this->waitFifo(1);

        this->writeBee8(PIX_CNTL, MASK_BIT_SRC_CPU);
    }

    this->setDrawMode(tmpl->FgPen, tmpl->BgPen, tmpl->DrawMode, AS_RGBF(fmt));
    this->setGEWriteMask(mask, AS_RGBF(fmt), 6);

    // This could/should get chached as well
    this->writeBee8(MULT_MISC2, seg << 4);

    this->setBlitSrcPosAndSize(x, y, width, height);

    // Make sure, no blitter operation is still running before we start feeding PIX_TRANS
    WaitForBlitter(bi);

    mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT | CMD_ACROSS_PLANE | CMD_WAIT_CPU |
                      CMD_BUS_SIZE_32BIT_MASK_32BIT_ALIGNED);

    // FIXME: there's no promise that tmpl->Memory and tmpl->BytesPerRow
    // are 32bit aligned. This might either be slower than it could be on 030+ or
    // just crashing on 68k.
    const UBYTE *bitmap = (const UBYTE *)tmpl->Memory;
    bitmap += (tmpl->XOffset / 32) * 4;
    UWORD dwordsPerLine = (width + 31) / 32;
    UBYTE rol           = tmpl->XOffset % 32;
    WORD bitmapPitch    = tmpl->BytesPerRow;
    if (!rol) {
        for (UWORD y = 0; y < height; ++y) {
            for (UWORD x = 0; x < dwordsPerLine; ++x) {
                this->writePIX_TRANS(((const ULONG *)bitmap)[x]);
            }
            bitmap += bitmapPitch;
        }
    } else {
        for (UWORD y = 0; y < height; ++y) {
            for (UWORD x = 0; x < dwordsPerLine; ++x) {
                ULONG left  = ((const ULONG *)bitmap)[x] << rol;
                ULONG right = ((const ULONG *)bitmap)[x + 1] >> (32 - rol);

                this->writePIX_TRANS(left | right);
            }
            bitmap += bitmapPitch;
        }
    }
}

void ASM S3Driver::blitPattern(__REGA1(struct RenderInfo *ri), __REGA2(struct Pattern *pattern), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    S3Mmio mmio = this->mmio();

    UBYTE bpp = getBPP(fmt);
    if (!bpp || !this->setGEFormat(ri->BytesPerRow, bpp)) {
        DFUNC(INFO, "fallback to BlitPatternDefault\n");
        bi->BlitPatternDefault(bi, ri, pattern, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    UWORD seg;
    UWORD xoffset;
    UWORD yoffset;
    getGESegmentAndOffset(this->getMemoryOffset(ri->Memory), ri->BytesPerRow, bpp, &seg, (UWORD *)&xoffset, (UWORD *)&yoffset);

    x += xoffset;
    y += yoffset;

#ifdef DBG
    if ((x > (1 << 11)) || (y > (1 << 11))) {
        DFUNC(ERROR, "X %ld or Y %ld out of range\n", (ULONG)x, (ULONG)y);
    }
#endif

    ChipData_t *cd = chip();

    if (cd->GEOp != BLITPATTERN) {
        cd->GEOp = BLITPATTERN;

        // Invalidate the pen and drawmode caches
        cd->GEdrawMode = 0xFF;
        cd->patternCacheKey &= ~0x80000000;

        this->writeBee8(PIX_CNTL, MASK_BIT_SRC_CPU);
    }

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

            this->setGEWriteMask(~0, AS_RGBF(fmt), 16);
            if (bpp > 2) {
                this->setForegroundColor32(~0);
                this->setBackgroundColor32(0);
            } else {
                this->setForegroundColor(~0);
                this->setBackgroundColor(0);
            }

            this->writeBee8(PIX_CNTL, MASK_BIT_SRC_CPU);
            this->writeBee8(MULT_MISC2, cd->pattSegment << 4);

            this->setMix(CLR_SRC_FRGD_COLOR | MIX_NEW, CLR_SRC_BKGD_COLOR | MIX_NEW);
            this->setBlitSrcPosAndSize(cd->pattX, cd->pattY, 8, 8);

            WaitForBlitter(bi);
            // FIXME: I should get away from checking aginst the family and instead have "feature bits"
            if (cd->chipFamily == VISION864 || cd->chipFamily == VISION968) {
                // The vision 864 doesn't have CMD_BUS_SIZE_32BIT_MASK_8BIT_ALIGNED, so we have to transfer the pattern
                // in 8bit chunks
                mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT | CMD_ACROSS_PLANE |
                                  CMD_WAIT_CPU | CMD_BUS_SIZE_8BIT);
                // FIXME: at this point I wonder if it would be faster to place the 8x8 pattern via CPU writes instead
                // of blitting it
                pat0 = swapl(pat0);
                pat1 = swapl(pat1);
                for (int i = 0; i < 4; ++i) {
                    mmio.writeB(S3_MMIO_ID(PIX_TRANS), pat0);
                    pat0 >>= 8;
                }
                for (int i = 0; i < 4; ++i) {
                    mmio.writeB(S3_MMIO_ID(PIX_TRANS), pat1);
                    pat1 >>= 8;
                }
            }
#if !BUILD_VISION864
            else {
                mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT | CMD_ACROSS_PLANE |
                                  CMD_WAIT_CPU | CMD_BUS_SIZE_32BIT_MASK_8BIT_ALIGNED);
                this->writePIX_TRANS(pat0);
                this->writePIX_TRANS(pat1);
            }
#endif

            this->waitFifo(1);
            this->writeBee8(PIX_CNTL, MASK_BIT_SRC_BITMAP);
        } else {
            if (!was8x8) {
                this->waitFifo(1);
                this->writeBee8(PIX_CNTL, MASK_BIT_SRC_BITMAP);
            }
        }

        // Now that the pattern is in place, we can do the actual pattern blit
        this->setGEWriteMask(mask, AS_RGBF(fmt), 6);
        this->setDrawMode(pattern->FgPen, pattern->BgPen, pattern->DrawMode, AS_RGBF(fmt));

        this->waitFifo(8);
        this->writeBee8(MULT_MISC2, (cd->pattSegment << 4) | seg);
        this->setBlitDestPos(x, y);
        this->setBlitSrcPosAndSize(cd->pattX, cd->pattY, width, height);

        mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_PAT_BLIT | CMD_DRAW_PIXELS | TOP_LEFT);
    } else {
        cd->patternCacheKey &= ~0x80000000;

        this->setDrawMode(pattern->FgPen, pattern->BgPen, pattern->DrawMode, AS_RGBF(fmt));

        this->setGEWriteMask(mask, AS_RGBF(fmt), 7);

        if (was8x8) {
            this->writeBee8(PIX_CNTL, MASK_BIT_SRC_CPU);
        }
        // This could/should get chached as well
        this->writeBee8(MULT_MISC2, seg << 4);

        this->setBlitSrcPosAndSize(x, y, width, height);

        mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT | CMD_ACROSS_PLANE | CMD_WAIT_CPU |
                          CMD_BUS_SIZE_32BIT_MASK_32BIT_ALIGNED);

        WORD dwordsPerLine      = (width + 31) / 32;
        UWORD *bitmap           = (UWORD *)pattern->Memory;
        UBYTE rol               = pattern->XOffset % 16;
        UWORD patternHeightMask = (1 << pattern->Size) - 1;

        if (!rol) {
            for (WORD y = 0; y < height; ++y) {
                UWORD bits  = bitmap[(y + pattern->YOffset) & patternHeightMask];
                ULONG bitsL = copyToUpper(bits);
                for (WORD x = 0; x < dwordsPerLine; ++x) {
                    this->writePIX_TRANS(bitsL);
                }
            }
        } else {
            for (WORD y = 0; y < height; ++y) {
                UWORD bits  = bitmap[(y + pattern->YOffset) & patternHeightMask];
                bits        = (bits << rol) | (bits >> (16 - rol));
                ULONG bitsL = copyToUpper(bits);
                for (WORD x = 0; x < dwordsPerLine; ++x) {
                    this->writePIX_TRANS(bitsL);
                }
            }
        }
    }
}

void S3Driver::performBlitPlanar2ChunkyBlits(SHORT dstX, SHORT dstY, SHORT width,
                                                  SHORT height, UWORD mixMode, UBYTE *bitmap, UWORD dwordsPerLine,
                                                  WORD bmPitch, UBYTE rol)
{
    BoardInfo *bi = this;
    S3Mmio mmio = this->mmio();

    this->setBlitSrcPosAndSize(dstX, dstY, width, height);

    // FIXME: we can optimize this by building a mask of 0 and 1 planes and then do a single fill
    // first to establish that pattern. IDK how much this feature is used, though. I guess its meant
    // to support planar images with less than 8 bitplanes.
    if ((ULONG)bitmap == 0x00000000) {
        this->writeBee8(PIX_CNTL, MASK_BIT_SRC_ONE);
        mmio.writeW(S3_MMIO_ID(FRGD_MIX), (CLR_SRC_BKGD_COLOR | mixMode));
        mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT);

    } else if ((ULONG)bitmap == 0xFFFFFFFF) {
        this->writeBee8(PIX_CNTL, MASK_BIT_SRC_ONE);
        mmio.writeW(S3_MMIO_ID(FRGD_MIX), (CLR_SRC_FRGD_COLOR | mixMode));
        mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT);

    } else {
        // FIXME: Should I have a path for 16bit aligned width?
        // The only argument for not doing it is unaligned 32bit reads from CPU
        // memory. PCI transfers are 32bit anyways, so wasting bus cycles by
        // transferring in chunks of 16bit seems wasteful
        this->writeBee8(PIX_CNTL, MASK_BIT_SRC_CPU);
        this->setMix((CLR_SRC_FRGD_COLOR | mixMode), (CLR_SRC_BKGD_COLOR | mixMode));

        // Make sure, no blitter operation is still running before we start feeding PIX_TRANS
        WaitForBlitter(bi);

        mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT | CMD_ACROSS_PLANE | CMD_WAIT_CPU |
                          CMD_BUS_SIZE_32BIT_MASK_32BIT_ALIGNED);

        if (!rol) {
            for (UWORD y = 0; y < height; ++y) {
                for (UWORD x = 0; x < dwordsPerLine; ++x) {
                    this->writePIX_TRANS(((ULONG *)bitmap)[x]);
                }
                bitmap += bmPitch;
            }
        } else {
            for (UWORD y = 0; y < height; ++y) {
                for (UWORD x = 0; x < dwordsPerLine; ++x) {
                    ULONG left  = ((ULONG *)bitmap)[x] << rol;
                    ULONG right = ((ULONG *)bitmap)[x + 1] >> (32 - rol);
                    this->writePIX_TRANS((left | right));
                }
                bitmap += bmPitch;
            }
        }
    }
}

void ASM S3Driver::blitPlanar2Chunky(__REGA1(struct BitMap *bm), __REGA2(struct RenderInfo *ri), __REGD0(SHORT srcX), __REGD1(SHORT srcY), __REGD2(SHORT dstX), __REGD3(SHORT dstY), __REGD4(SHORT width), __REGD5(SHORT height), __REGD6(UBYTE minTerm), __REGD7(UBYTE mask))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nsrcX %ld, srcY %ld, dstX %ld, dstY %ld, w %ld, h %ld"
          "\nmask 0x%lx minTerm %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)minTerm,
          (ULONG)ri->BytesPerRow, (ULONG)ri->Memory);

    BOOL emulate320  = (ri->BytesPerRow == 320);
    WORD bytesPerRow = emulate320 ? 640 : ri->BytesPerRow;
    // how many dwords per line in the source plane
    UWORD numPlanarBytes              = width / 8 * height * bm->Depth;
    UWORD projectedRegisterWriteBytes = (9 + 8 * 8) * 2;

    BOOL swFallback = (projectedRegisterWriteBytes > numPlanarBytes);

    if (swFallback || !this->setGEFormat(bytesPerRow, 1)) {
        DFUNC(1, "fallback to BlitPlanar2ChunkyDefault\n");
        bi->BlitPlanar2ChunkyDefault(bi, bm, ri, srcX, srcY, dstX, dstY, width, height, minTerm, mask);
        return;
    }

    UWORD seg;
    UWORD xoffset;
    UWORD yoffset;
    getGESegmentAndOffset(this->getMemoryOffset(ri->Memory), bytesPerRow, 1, &seg, (UWORD *)&xoffset, (UWORD *)&yoffset);

    dstX += xoffset;
    dstY += yoffset;

    ChipData_t *cd = chip();

    if (cd->GEOp != BLITPLANAR2CHUNKY) {
        cd->GEOp = BLITPLANAR2CHUNKY;

        // Invalidate the pen and drawmode caches
        cd->GEdrawMode = 0xFF;

        cd->GEfgPen = 0xFF;
        cd->GEbgPen = 0x00;

        this->setForegroundColor(0xFF);
        this->setBackgroundColor(0x00);
    }

    UWORD mixMode  = mintermToMixMode(minTerm);
    cd->GEdrawMode = minTerm;
    cd->GEFormat   = RGBFB_CLUT;

    S3Mmio mmio = this->mmio();

    // This could/should get chached as well
    this->writeBee8(MULT_MISC2, seg << 4);

    WORD bmPitch        = bm->BytesPerRow;
    ULONG bmStartOffset = (srcY * bmPitch) + (srcX / 32) * 4;
    UWORD dwordsPerLine = (width + 31) / 32;
    UBYTE rol           = srcX % 32;

    for (short p = 0; p < 8; ++p) {
        UBYTE writeMask = 1 << p;

        if (!(mask & writeMask)) {
            continue;
        }

        this->setGEWriteMask(writeMask, RGBFB_CLUT, 8);

        UBYTE *bitmap = (UBYTE *)bm->Planes[p];
        if (bitmap != 0x0 && (ULONG)bitmap != 0xffffffff) {
            bitmap += bmStartOffset;
        }

        if (!emulate320) {
            this->performBlitPlanar2ChunkyBlits(dstX, dstY, width, height, mixMode, bitmap, dwordsPerLine, bmPitch, rol);
        } else {
            SHORT halfHeight1 = (height + 1) / 2;
            SHORT halfHeight2 = height / 2;

            this->performBlitPlanar2ChunkyBlits(dstX, dstY, width, halfHeight1, mixMode, bitmap, dwordsPerLine,
                                          bmPitch * 2, rol);
            if (halfHeight2) {
                this->performBlitPlanar2ChunkyBlits(dstX + 320, dstY, width, halfHeight2, mixMode, bitmap + bmPitch,
                                              dwordsPerLine, bmPitch * 2, rol);
            }
        }
    }
}

void ASM S3Driver::drawLine(__REGA1(struct RenderInfo *ri), __REGA2(struct Line *line), __REGD0(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "\n");

    UBYTE bpp = getBPP(fmt);
    if (!bpp || !this->setGEFormat(ri->BytesPerRow, bpp) || !line->Length) {
        DFUNC(1, "Fallback to DrawLineDefault\n");
        bi->DrawLineDefault(bi, ri, line, mask, AS_RGBF(fmt));
        return;
    }

    UWORD x, y;

    UWORD seg;
    UWORD xoffset;
    UWORD yoffset;
    getGESegmentAndOffset(this->getMemoryOffset(ri->Memory), ri->BytesPerRow, bpp, &seg, (UWORD *)&x, (UWORD *)&y);

    x += line->X;
    y += line->Y;

    ChipData_t *cd = chip();

    if (cd->GEOp != LINE) {
        // Make sure, no blitter operation is still running before we start feeding PIX_TRANS
        WaitForBlitter(bi);

        cd->GEOp       = LINE;
        cd->GEdrawMode = 0xFF;
    }

    this->waitFifo(1);

    S3Mmio mmio = this->mmio();

    // This could/should get chached as well
    this->writeBee8(MULT_MISC2, seg << 4);

    this->setDrawMode(line->FgPen, line->BgPen, line->DrawMode, AS_RGBF(fmt));
    this->setGEWriteMask(mask, AS_RGBF(fmt), 0);

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

    this->waitFifo(8);

#if HAS_PACKED_MMIO
    mmio.writeL(S3_MMIO_ID(ALT_CURXY), makeDWORD(x, y));
    mmio.writeL(S3_MMIO_ID(ALT_STEP), makeDWORD(2 * (absMIN - absMAX), (2 * absMIN)));
#else
    mmio.writeW(S3_MMIO_ID(CUR_X), x);
    mmio.writeW(S3_MMIO_ID(CUR_Y), y);
    mmio.writeW(S3_MMIO_ID(DESTX_DIASTP), 2 * (absMIN - absMAX));
    mmio.writeW(S3_MMIO_ID(DESTY_AXSTP), (2 * absMIN));
#endif

    mmio.writeW(S3_MMIO_ID(MAJ_AXIS_PCNT), line->Length - 1);
    mmio.writeW(S3_MMIO_ID(ERR_TERM), errTerm);

    BOOL isSolid = (line->LinePtrn == 0xFFFF);
    if (isSolid) {
        this->writeBee8(PIX_CNTL, MASK_BIT_SRC_ONE);
        mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_LINE | CMD_DRAW_PIXELS | direction);
    } else {
        this->writeBee8(PIX_CNTL, MASK_BIT_SRC_CPU);

        mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_LINE | CMD_DRAW_PIXELS | CMD_ACROSS_PLANE | CMD_WAIT_CPU |
                          CMD_BUS_SIZE_32BIT_MASK_32BIT_ALIGNED | direction);

        // Line->PatternShift selects which bit of the pattern is to be used for the
        // origin of the line and thus shifts the pattern to the indicated number of
        // bits to the left. It is the pattern shift value at the start of the line
        // segment to be drawn.
        UWORD rol      = line->PatternShift;
        UWORD pattern  = (line->LinePtrn << rol) | (line->LinePtrn >> (16u - rol));
        ULONG patternL = copyToUpper(pattern);
        WORD numDWords = (line->Length + 31) / 32;
        for (WORD i = 0; i < numDWords; ++i) {
            this->writePIX_TRANS(patternL);
        }
    }
}

/** Read CR36 memory type, log it, and return (CR36>>2)&3 for use by callers (e.g. Trio64 SR0x0A). */
static UBYTE getMemoryType(struct BoardInfo *bi)
{
    VgaIo vga = asS3(bi)->vga();
    const UBYTE memType = (vga.readCR(0x36) >> 2) & 3;
    switch (memType) {
    case 0b00:
        D(INFO, "1-cycle EDO/VRAM\n");
        break;
    case 0b10:
        D(INFO, "2-cycle EDO\n");
        break;
    case 0b11:
        D(INFO, "FPM\n");
        break;
    default:
        D(WARN, "unknown memory type\n");
    }
    return memType;
}

/** Probe framebuffer at current bi->MemorySize: write/read high, low, zero; return TRUE if all match. */
static BOOL testFramebufferReadWrite(struct BoardInfo *bi)
{
    LOCAL_SYSBASE();
    volatile ULONG *framebuffer = (volatile ULONG *)bi->MemoryBase;
    CacheClearU();
    volatile ULONG *highOffset = framebuffer + (bi->MemorySize >> 2) - 1;
    volatile ULONG *lowOffset  = framebuffer + (bi->MemorySize >> 3);
    *framebuffer               = 0;
    *highOffset                = (ULONG)highOffset;
    *lowOffset                 = (ULONG)lowOffset;
    CacheClearU();
    ULONG readbackHigh = *highOffset;
    ULONG readbackLow  = *lowOffset;
    ULONG readbackZero = *framebuffer;
    D(10, "Probing memory at 0x%lx ?= 0x%lx; 0x%lx ?= 0x%lx, 0x0 ?= 0x%lx\n", highOffset, readbackHigh, lowOffset,
      readbackLow, readbackZero);
    return readbackHigh == (ULONG)highOffset && readbackLow == (ULONG)lowOffset && readbackZero == 0;
}

static BOOL probeFramebufferVision(struct BoardInfo *bi)
{
    VgaIo vga = asS3(bi)->vga();
    getMemoryType(bi);

    ChipFamily_t chipFamily = (ChipFamily_t)getChipData(bi)->chipFamily;

    bi->MemorySize = (chipFamily == VISION968) ? 0x800000 : 0x400000;
    while (bi->MemorySize) {
        D(VERBOSE, "\nProbing memory size %ld\n", bi->MemorySize);
        {
            UBYTE LAWSize = 0;
            UBYTE MemSize = 0;
            if (bi->MemorySize >= 0x800000) {
                LAWSize = 0b11;
                MemSize = 0b011;
            } else if (bi->MemorySize >= 0x600000) {
                LAWSize = 0b11;
                MemSize = 0b101;
            } else if (bi->MemorySize >= 0x400000) {
                LAWSize = 0b11;
                MemSize = 0b000;
            } else if (bi->MemorySize >= 0x200000) {
                LAWSize = 0b10;
                MemSize = 0b100;
            } else {
                LAWSize = 0b01;
                MemSize = 0b110;
            }
            vga.writeCRMask(0x36, 0xE0, MemSize << 5);
            vga.writeCRMask(0x58, 0x13, LAWSize | BIT(4));
        }
        if (testFramebufferReadWrite(bi)) {
            break;
        }
        bi->MemorySize >>= 1;
        if (bi->MemorySize < 1024 * 1024) {
            D(ERROR, "Memory detection failed, aborting\n");
            return FALSE;
        }
    }
    D(INFO, "MemorySize: %ldmb\n", bi->MemorySize / (1024 * 1024));
    return TRUE;
}

static BOOL probeFramebufferTrio64(struct BoardInfo *bi)
{
    VgaIo vga = asS3(bi)->vga();
    const UBYTE memType = getMemoryType(bi);

    ChipFamily_t chipFamily = (ChipFamily_t)getChipData(bi)->chipFamily;

    if (chipFamily == TRIO64 || chipFamily == TRIO64PLUS) {
        vga.writeCRMask(0x68, BIT(7), BIT(7));
    }

    bi->MemorySize = 0x400000;
    while (bi->MemorySize) {
        D(VERBOSE, "\nProbing memory size %ld\n", bi->MemorySize);
        {
            UBYTE LAWSize = 0;
            UBYTE MemSize = 0;
            if (bi->MemorySize >= 0x800000) {
                LAWSize = 0b11;
                MemSize = 0b011;
            } else if (bi->MemorySize >= 0x600000) {
                LAWSize = 0b11;
                MemSize = 0b101;
            } else if (bi->MemorySize >= 0x400000) {
                LAWSize = 0b11;
                MemSize = 0b000;
                if ((chipFamily == TRIO64 || chipFamily == TRIO64PLUS) && memType == 0b11) {
                    vga.writeSRMask(0x0A, BIT(6), BIT(6));
                }
            } else if (bi->MemorySize >= 0x200000) {
                LAWSize = 0b10;
                MemSize = 0b100;
                if (chipFamily == TRIO64 || chipFamily == TRIO64PLUS) {
                    vga.writeSRMask(0x0A, BIT(6), 0);
                }
            } else {
                LAWSize = 0b01;
                MemSize = 0b110;
                if (chipFamily == TRIO64 || chipFamily == TRIO64PLUS) {
                    vga.writeCRMask(0x68, BIT(7), 0);
                }
            }
            vga.writeCRMask(0x36, 0xE0, MemSize << 5);
            vga.writeCRMask(0x58, 0x13, LAWSize | BIT(4));
        }
        if (testFramebufferReadWrite(bi)) {
            break;
        }
        bi->MemorySize >>= 1;
        if (bi->MemorySize < 1024 * 1024) {
            D(ERROR, "Memory detection failed, aborting\n");
            return FALSE;
        }
    }
    D(INFO, "MemorySize: %ldmb\n", bi->MemorySize / (1024 * 1024));
    return TRUE;
}

static BOOL probeFramebufferAurora64(struct BoardInfo *bi)
{
    VgaIo vga = asS3(bi)->vga();
    getMemoryType(bi);

    static const UBYTE memSizes[]     = {4, 2, 2, 1};
    static const UBYTE lawSizeCodes[] = {0b11, 0b10, 0b10, 0b01};
    // There's a weird discrepancy in the Aurora manual/ Early in the text the below
    // bitmasks are presribed, while the 'classic' bitmasks are used elsewhere
    // UBYTE MemSize[] = {0b111, 0b011, 0b010, 0b001};

    // OTOH the manual states:
    // "If EDO memory is used, the 86CM65 must be configured for 1-cycle operation (CR36_3-2 = 11b).
    // IDK if one can trust the strapping of CR36 memory type, though.
    static const UBYTE memSizeCodes[] = {0b000, 0b011, 0b010, 0b110};

    WORD m = 0;
    for (; m < ARRAY_SIZE(memSizes); ++m) {
        bi->MemorySize = memSizes[m] << 20;
        D(VERBOSE, "\nProbing memory size %ld\n", bi->MemorySize);
        vga.writeCRMask(0x36, 0xE0, memSizeCodes[m] << 5);
        vga.writeCRMask(0x58, 0x13, lawSizeCodes[m] | BIT(4));
        if (testFramebufferReadWrite(bi)) {
            break;
        }
    }

    if (m == ARRAY_SIZE(memSizes)) {
        D(ERROR, "Memory detection failed, aborting\n");
        return FALSE;
    }
    D(INFO, "MemorySize: %ldmb\n", bi->MemorySize / (1024 * 1024));
    return TRUE;
}

/* P96 BoardInfo entry stubs */

static void ASM WaitBlitter(__REGA0(struct BoardInfo *bi))
{
    asS3(bi)->waitBlitter();
}
static UWORD ASM CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                  __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE format))
{
    return asS3(bi)->calculateBytesPerRow(width, height, mi, format);
}
static void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    asS3(bi)->setColorArray(startIndex, count);
}
static void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD0(UWORD region), __REGD7(RGBFTYPE format))
{
    asS3(bi)->setDAC(region, format);
}
static void ASM SetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    asS3(bi)->setGC(mi, border);
}
static void ASM SetPanning(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD3(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset),
                           __REGD7(RGBFTYPE format))
{
    asS3(bi)->setPanning(memory, width, height, xoffset, yoffset, format);
}
static APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi), __REGA1(APTR mem), __REGD0(struct RenderInfo *ri),
                                __REGD7(RGBFTYPE format))
{
    return asS3(bi)->calculateMemory(mem, ri, format);
}
static ULONG ASM GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    return asS3(bi)->getCompatibleFormats(format);
}
static BOOL ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    return asS3(bi)->setDisplay(state);
}
static LONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi),
                                  __REGD0(ULONG pixelClock), __REGD7(RGBFTYPE RGBFormat))
{
    return asS3(bi)->resolvePixelClock(mi, pixelClock, RGBFormat);
}
static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD7(RGBFTYPE format))
{
    return asS3(bi)->getPixelClock(mi, index, format);
}
static void ASM SetClock(__REGA0(struct BoardInfo *bi))
{
    asS3(bi)->setClock();
}
static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
#if !BUILD_VISION864
    __asm __volatile("\t movem.l d0-d1/a0-a1,-(sp)\n" : : :);
    asS3(bi)->setMemoryMode(format);
    __asm __volatile("\t movem.l (sp)+,d0-d1/a0-a1\n" : : : "d0", "d1", "a0", "a1");
#else
    (void)bi;
    (void)format;
#endif
}
static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    __asm __volatile("\t movem.l d0-d1/a0-a1,-(sp)\n" : : :);
    asS3(bi)->setWriteMask(mask);
    __asm __volatile("\t movem.l (sp)+,d0-d1/a0-a1\n" : : : "d0", "d1", "a0", "a1");
}
static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    asS3(bi)->setClearMask(mask);
}
static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    asS3(bi)->setReadPlane(mask);
}
static BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL expected))
{
    return asS3(bi)->getVSyncState(expected);
}
static void ASM WaitVerticalSync(__REGA0(struct BoardInfo *bi), __REGD0(BOOL waitForEnd))
{
    asS3(bi)->waitVerticalSync(waitForEnd);
}
static BOOL ASM SetInterrupt(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    return asS3(bi)->setInterrupt(state);
}
ULONG ASM interruptServer(__REGA1(struct BoardInfo *bi))
{
    return asS3(bi)->interruptServer();
}
DEFINE_INTSERVER(interruptServerTrampoline, interruptServer);
static void ASM SetDPMSLevel(__REGA0(struct BoardInfo *bi), __REGD0(ULONG level))
{
    asS3(bi)->setDPMSLevel(level);
}
static void ASM SetSplitPosition(__REGA0(struct BoardInfo *bi), __REGD0(SHORT splitPos))
{
    asS3(bi)->setSplitPosition(splitPos);
}
static void ASM SetSpritePosition(__REGA0(struct BoardInfo *bi), __REGD0(WORD xpos), __REGD1(WORD ypos),
                                  __REGD7(RGBFTYPE fmt))
{
    asS3(bi)->setSpritePosition(xpos, ypos, fmt);
}
static void ASM SetSpriteImage(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE fmt))
{
    asS3(bi)->setSpriteImage(fmt);
}
static void ASM SetSpriteColor(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE index), __REGD1(UBYTE red),
                               __REGD2(UBYTE green), __REGD3(UBYTE blue), __REGD7(RGBFTYPE fmt))
{
    asS3(bi)->setSpriteColor(index, red, green, blue, fmt);
}
static BOOL ASM SetSprite(__REGA0(struct BoardInfo *bi), __REGD0(BOOL activate), __REGD7(RGBFTYPE RGBFormat))
{
    return asS3(bi)->setSprite(activate, RGBFormat);
}
static void ASM FillRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                         __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(ULONG pen),
                         __REGD5(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asS3(bi)->fillRect(ri, x, y, width, height, pen, mask, fmt);
}
static void ASM InvertRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                           __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                           __REGD7(RGBFTYPE fmt))
{
    asS3(bi)->invertRect(ri, x, y, width, height, mask, fmt);
}
static void ASM BlitRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD srcX),
                         __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                         __REGD5(WORD height), __REGD6(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asS3(bi)->blitRect(ri, srcX, srcY, dstX, dstY, width, height, mask, fmt);
}
static void ASM BlitRectNoMaskComplete(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *sri),
                                       __REGA2(struct RenderInfo *dri), __REGD0(WORD srcX), __REGD1(WORD srcY),
                                       __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                                       __REGD5(WORD height), __REGD6(UBYTE minTerm), __REGD7(RGBFTYPE format))
{
    asS3(bi)->blitRectNoMaskComplete(sri, dri, srcX, srcY, dstX, dstY, width, height, minTerm, format);
}
static void ASM BlitTemplate(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                             __REGA2(struct Template *tmpl), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                             __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asS3(bi)->blitTemplate(ri, tmpl, x, y, width, height, mask, fmt);
}
static void ASM BlitPattern(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                            __REGA2(struct Pattern *pattern), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                            __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asS3(bi)->blitPattern(ri, pattern, x, y, width, height, mask, fmt);
}
static void ASM BlitPlanar2Chunky(__REGA0(struct BoardInfo *bi), __REGA1(struct BitMap *bm),
                                  __REGA2(struct RenderInfo *ri), __REGD0(SHORT srcX), __REGD1(SHORT srcY),
                                  __REGD2(SHORT dstX), __REGD3(SHORT dstY), __REGD4(SHORT width), __REGD5(SHORT height),
                                  __REGD6(UBYTE minTerm), __REGD7(UBYTE mask))
{
    asS3(bi)->blitPlanar2Chunky(bm, ri, srcX, srcY, dstX, dstY, width, height, minTerm, mask);
}
static void ASM DrawLine(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGA2(struct Line *line),
                  __REGD0(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asS3(bi)->drawLine(ri, line, mask, fmt);
}
BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    DFUNC(ALWAYS, "\n");

    //  getChipData(bi)->DOSBase = (ULONG)OpenLibrary(DOSNAME, 0);
    //  if (!getChipData(bi)->DOSBase) {
    //    return FALSE;
    //  }

    bi->GraphicsControllerType = GCT_S3Trio64;
    bi->PaletteChipType        = PCT_S3Trio64;
    bi->Flags = bi->Flags | BIF_NOMEMORYMODEMIX | BIF_BORDERBLANK | BIF_BLITTER | BIF_GRANTDIRECTACCESS |
                BIF_VGASCREENSPLIT | BIF_HARDWARESPRITE;
    // Trio64 supports BGR_8_8_8_X 24bit, R5G5B5 and R5G6B5 modes.
    // From the perspective of our big endian machine, the following formats map to that:
    // 32bit register filled with XRGB, the written memory order will be BGRX
    bi->RGBFormats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;

#if HAS_ROXXLER
    // Cybervision has ROXXLER chip
    bi->RGBFormats |= RGBFF_A8R8G8B8 | RGBFF_R5G6B5 | RGBFF_R5G5B5;
#endif

    // We don't support these modes, but if we did, they would not allow for a HW
    // sprite
    bi->SoftSpriteFlags = RGBFF_B8G8R8 | RGBFF_R8G8B8;

    P96_HOOK(bi->SetGC, SetGC);
    P96_HOOK(bi->SetPanning, SetPanning);
    P96_HOOK(bi->CalculateBytesPerRow, CalculateBytesPerRow);
    P96_HOOK(bi->CalculateMemory, CalculateMemory);
    P96_HOOK(bi->GetCompatibleFormats, GetCompatibleFormats);
    P96_HOOK(bi->SetDAC, SetDAC);
    P96_HOOK(bi->SetColorArray, SetColorArray);
    P96_HOOK(bi->SetDisplay, SetDisplay);
    P96_HOOK(bi->SetMemoryMode, SetMemoryMode);
    P96_HOOK(bi->SetWriteMask, SetWriteMask);
    P96_HOOK(bi->SetReadPlane, SetReadPlane);
    P96_HOOK(bi->SetClearMask, SetClearMask);
    P96_HOOK(bi->ResolvePixelClock, ResolvePixelClock);
    P96_HOOK(bi->GetPixelClock, GetPixelClock);
    P96_HOOK(bi->SetClock, SetClock);
    // VSYNC / VBlank IRQ (card: pci_add_intserver or AddIntServer INTB_PORTS)
    P96_HOOK(bi->WaitVerticalSync, WaitVerticalSync);
    P96_HOOK(bi->GetVSyncState, GetVSyncState);
    P96_HOOK(bi->SetInterrupt, SetInterrupt);
    bi->HardInterrupt.is_Node.ln_Type = NT_INTERRUPT;
    bi->HardInterrupt.is_Node.ln_Pri  = 0;
    bi->HardInterrupt.is_Node.ln_Name = (char *)"S3VBlank";
    bi->HardInterrupt.is_Data         = bi;
    bi->HardInterrupt.is_Code         = (void (*)())interruptServerTrampoline;

    // DPMS
    P96_HOOK(bi->SetDPMSLevel, SetDPMSLevel);
    // VGA Splitscreen
    P96_HOOK(bi->SetSplitPosition, SetSplitPosition);
    // Mouse Sprite
    P96_HOOK(bi->SetSprite, SetSprite);
    P96_HOOK(bi->SetSpritePosition, SetSpritePosition);
    P96_HOOK(bi->SetSpriteImage, SetSpriteImage);
    P96_HOOK(bi->SetSpriteColor, SetSpriteColor);
    // Blitter acceleration
    P96_HOOK(bi->WaitBlitter, WaitBlitter);
    P96_HOOK(bi->BlitRect, BlitRect);
    P96_HOOK(bi->InvertRect, InvertRect);
    P96_HOOK(bi->FillRect, FillRect);
    P96_HOOK(bi->BlitTemplate, BlitTemplate);
    P96_HOOK(bi->BlitPlanar2Chunky, BlitPlanar2Chunky);
    P96_HOOK(bi->BlitRectNoMaskComplete, BlitRectNoMaskComplete);
    P96_HOOK(bi->DrawLine, DrawLine);
    P96_HOOK(bi->BlitPattern, BlitPattern);
    DFUNC(CHATTY,
          "WaitBlitter 0x%08lx\nBlitRect 0x%08lx\nInvertRect 0x%08lx\nFillRect "
          "0x%08lx\n"
          "BlitTemplate 0x%08lx\n BlitPlanar2Chunky 0x%08lx\n"
          "BlitRectNoMaskComplete 0x%08lx\n DrawLine 0x%08lx\n",
          bi->WaitBlitter, bi->BlitRect, bi->InvertRect, bi->FillRect, bi->BlitTemplate, bi->BlitPlanar2Chunky,
          bi->BlitRectNoMaskComplete, bi->DrawLine);

    ChipData_t *cd = getChipData(bi);

#if OPENPCI
    ULONG revision;
    ULONG deviceId;
    LOCAL_OPENPCIBASE();
    GetBoardAttrs(getCardData(bi)->board, PRM_Device, (ULONG)&deviceId, PRM_Revision, (ULONG)&revision, TAG_END);

    if ((cd->chipFamily = getChipFamily(deviceId, revision)) == UNKNOWN) {
        return FALSE;
    }
#else
    // For non-PCI cards (e.g., Zorro), chipFamily should already be set by the card driver
    if (cd->chipFamily == UNKNOWN) {
        return FALSE;
    }
    ULONG deviceId = 0x8811;  // Trio64 chip on Cybervision64 card

#endif

    ChipFamily_t chipFamily = (ChipFamily_t)cd->chipFamily;

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

#if OPENPCI
    pci_write_config_word(PCI_COMMAND, PCI_COMMAND_MEMORY | PCI_COMMAND_IO, getCardData(bi)->board);
#endif

    {
        VgaIo vga = asS3(bi)->vga();
        BOOL used3c3 = FALSE;
        if (chipFamily >= TRIO64PLUS) {
            /* Chip wakeup Trio64+ and up*/
            vga.writeB(VGA_ID(0x3C3), 0x01);
            used3c3 = TRUE;
        } else {
            /* Chip wakeup Trio64/32 */
            vga.writeB(VGA_ID(0x46E8), 0x10);
            vga.writeB(VGA_ID(0x102), 0x01);
            vga.writeB(VGA_ID(0x46E8), 0x08);
        }

    retry:
        vga.writeMiscMask(0x0F, 0x0F);  // Enable RAM access and Color-Emulation, 0x3D4/5 for CR_IDX/DATA

        vga.writeCR(0x38, 0x48);  // provide access to extended CRTC registers CR2D-CR3F
        vga.writeCR(0x39, 0xa5);  // provide access to extended CRTC registers CR40-CRFF
        vga.writeSR(0x08, 0x06);  // unlock extended sequencer registers SR9-SR1C

        if (chipFamily >= TRIO64) {
            DFUNC(INFO, "Checking register response...\n");
            UBYTE chipId = vga.readCR(0x30) >> 4;
            if (chipId != 0xE) {
                DFUNC(ERROR, "CR30 Chip ID: expected 0xE, got 0x%1lX\n", (ULONG)chipId);

                if (used3c3 == FALSE) {
                    vga.readB(VGA_ID(0x65));
                    vga.writeB(VGA_ID(0x3C3), 0x10);
                    vga.writeB(VGA_ID(0x102), 0x01);
                    vga.writeB(VGA_ID(0x3C3), 0x08);

                    used3c3 = TRUE;
                    goto retry;
                }

                return FALSE;
            }
            UBYTE deviceIdHi = vga.readCR(0x2D);
            UBYTE deviceIdLo = vga.readCR(0x2E);
            if (chipFamily == TRIO64) {
                deviceIdLo |= 0x01;  // TRIO32 reports 0x10 in CR2E instead  of 0x11, so make it 0x11
            }
            if ((deviceIdHi << 8 | deviceIdLo) != deviceId) {
                DFUNC(ERROR, "Chipset ID mismatch: expected 0x%04lX, got 0x%02lX%02lX\n", (ULONG)deviceId,
                      (ULONG)deviceIdHi, (ULONG)deviceIdLo);
                return FALSE;
            }
            D(INFO, "register response Good.\n");
            if (chipFamily <= TRIO64) {
                if (used3c3) {
                    vga.writeMaskB(VGA_ID(0x65), BIT(2), 0);
                } else {
                    vga.writeMaskB(VGA_ID(0x65), BIT(2), BIT(2));
                }
            }
        } else {
            // Test CR30 for the right id, which is probably not the PCI device ID
        }
    }

#if BIGENDIAN_MMIO && !defined(CONFIG_CYBERVISION64)
    if (chipFamily >= VISION968) {
        bi->MemoryIOBase += 0x2000000;
    } else {
        DFUNC(ERROR, "Big Endian MMIO requested, but not supported on this chipset.\n");
        return FALSE;
    }
#endif

#if MMIO_ONLY
    {
        // FIXME: PCI cards should startup with New MMIO enabled. Thus, we should be able to just use MMIO directly.
        D(INFO, "Setting up MMIO only driver\n");
        bi->RegisterBase = bi->MemoryIOBase + 0x8000;  // bi->MemoryIObase has MMIOREGISTER_OFFSET added already
#if OPENPCI
                                                       // Disable IO Response

        pci_write_config_word(PCI_COMMAND, PCI_COMMAND_MEMORY /*| PCI_COMMAND_IO*/, getCardData(bi)->board);
#endif
    }
#endif

    VgaIo vga = asS3(bi)->vga();
    S3Io io = asS3(bi)->io();

    if (chipFamily == VISION968) {
        vga.writeCRMask(0x36, 0x1e, 0x1e);
        vga.writeCR(0x37, 0xff);
        vga.writeCR(0x68, 0xec);
    } else if (chipFamily == AURORA64PLUS) {
        vga.readSR(0x1A);  // This register allows to override the 3.3v/5v autoselection found in SR1B
        vga.readSR(0x1B);
        vga.readSR(0x0B);
        // Try to power down Controller2 as much as possible
        vga.writeSR(0x21, BIT(4) | BIT(5));
        vga.writeSR(0x1E, BIT(1));  // Power down Controller 2 DCLK when Controller 2 is  not enabled
    }

#if !BUILD_VISION864
    vga.writeSR(0x15, 0x00);
    vga.writeSR(0x18, 0x00);

    // The power-on default value for SR13 in conjunction with the power-on default value for SR12
    // generate a DCLK value of 25.175 MHz. The default value is automatically placed in this register
    //  when bits 3-2 of 3C2H are programmed to OOb.
    // FIXME: This doesn't seem do load SR11/SR12?!
    vga.writeMiscMask(0x0C, 0x00);  //

    // Init PLL to a known state
    vga.writeMiscMask(0x0C, 0x0C);  // Enables loading of DCLK PLL parameters in SR12 and SR13

    // When new DCLK PLL values are programmed, this bit can be set to 1 to load these
    // values in the PLL. Bits 3-2 of 3C2H must also be set to 11b ifthey are not already at
    // this value. The loading may be delayed a small but variable amount of time. This bit
    // should be programmed to 1 at power-up to allow loading of the VGA DCLK value and
    // then left at this setting. Use bit 5 of this register to produce an immediate load.
    // W_SR_MASK(0x15, 0x02, 0x02);  // Enable new DCLK frequency load
    // // W_SR_MASK(0x15, 0x02, 0x00);  // Enable new DCLK frequency load

    // // When new MCLK PLL values are programmed, this bit can be set to 1 to load these
    // // values in the PLL. The loading may be delayed a small but variable amount of time.
    // // This bit should be cleared to 0 after loading to prevent repeated
    // // loading. Alternately, use bit 5 of this register to produce an immediate load.
    // W_SR_MASK(0x15, 0x01, 0x01);  // Enable new MCLK frequency load
    // W_SR_MASK(0x15, 0x01, 0x00);  // Clear MCLK load bit

    // testS3PLLClock(bi, FALSE);
#endif

#if BIGENDIAN_MMIO && !defined(CONFIG_CYBERVISION64)
    if (chipFamily >= VISION968) {
        // Enable BYTE-Swapping for MMIO register reads/writes
        // This allows us to write to WORD/DWORD MMIO registers without swapping
        vga.writeCRMask(0x54, 0x03, 0b11);
    } else {
        D(ERROR, "architecture doesn't support bigendian MMIO\n");
        return FALSE;
    }
#endif
    // Enable 4MB Linear Address Window (LAW)
    vga.writeCR(0x58, 0x13);
    if (chipFamily >= VISION968) {
        // Enable Trio64+ "New MMIO". This should be on by default on PCI cards.
        D(INFO, "setup newstyle MMIO\n");
        vga.writeCRMask(0x53, 0x18, 0x08);
    } else {
        // FIXME: this code should not exist with MMIO_ONLY or BIGENDIAN_MMIO
        D(INFO, "setup compatible MMIO\n");
        // Enable Trio64 old style MMIO. This hardcodes the MMIO range to 0xA8000
        // physical address. Need to make sure, nothing else sits there
        vga.writeCRMask(0x53, 0x10, 0x10);

        // Test, also enable MMIO and Linear addressing via the other register
        // W_REG_MASK(ADVFUNC_CNTL, 0x30, 0x30);

#ifdef DBG
#if OPENPCI
        {
            LOCAL_OPENPCIBASE();
            // LAW start address
            ULONG physAddress = (ULONG)pci_logic_to_physic_addr(bi->MemoryBase, getCardData(bi)->board);
            if (physAddress & 0x3FFFFF) {
                D(WARN, "WARNING: card's base address is not 4MB aligned!\n");
            }
        }
#endif
#endif
#ifndef CONFIG_CYBERVISION64
        // Setup the Linear Address Window (LAW)  position
        // Beware: while bi->MemoryBase is a 'virtual' address, the register wants a physical address
        // We basically achieve this translation by chopping off the topmost bits.
        vga.writeCRMask(0x5a, 0x40, (ULONG)bi->MemoryBase >> 16);
        D(INFO, "CR59: 0x%lx CR5A: 0x%lx\n", (ULONG)vga.readCR(0x59), (ULONG)vga.readCR(0x5a));
        // Upper address bits may  not be touched as they would result in shifting
        // the PCI window
        //    W_CR_MASK(0x59, physAddress >> 24);
#else
        // On the Cybervision the VRAM is mapped at a specific fixed address from the Trio's point of view.
        // I guess, this is done such the LAW window is not at 0x0 (and thus would overlap withe legacy VGA Window at
        // 0xA0000)
        vga.writeCR(0x59, 0x00);
        vga.writeCR(0x5a, 0x40);
#endif
    }

#if defined(CONFIG_S3TRIO64PLUS) || defined(CONFIG_STRIO64V2)
    vga.writeCRMask(0x66, BIT(7) | BIT(3), BIT(7) | BIT(3));  // enable PCI Disconnect on FIFO full
#endif
    D(INFO, "MMIO base address: 0x%08lx, IO base address: 0x%08lx \n", (ULONG)getMMIOBase(bi), (ULONG)getIOBase(bi));

    S3Mmio mmio = asS3(bi)->mmio();

    if (!InitRAMDAC(bi)) {
        DFUNC(ERROR, "InitRAMDAC() failed\n");
        return FALSE;
    }

    // Initialize PLL table for pixel clocks (RAMDAC ops provide PLL limits)
    initPixelClockPLLTable(bi);

    UBYTE chipRevision = vga.readCR(0x2F);
    if (chipFamily >= VISION968) {
        D(INFO, "Chip supports BigEndian aperture, enabling more formats\n");

        // We can support byte-swapped formats on this chip via the Big Linear
        // Adressing Window
        bi->RGBFormats |= RGBFF_A8R8G8B8 | RGBFF_R5G6B5 | RGBFF_R5G5B5;

        if (chipFamily == TRIO64PLUS) {
            UBYTE cr6F = vga.readCR(0x6F);
            D(INFO, "Trio64+ card is in %s mode \n", TESTBIT(cr6F, 0) ? "Trio64 compatibility" : "LPB");
        }
    } else {
        D(INFO, "Chip does not support Big Endian aperture\n");
    }

    /* The Enhanced Graphics Command register group is unlocked
       by setting bit 0 of the System Configuration register (CR40) to 1.
       After that, bitO of4AE8H must be setto 1 to enable Enhanced mode functions.
    */
    vga.writeCRMask(0x40, 0x01, 0x01);

    /* Now that we enabled enhanced mode register access;
     * Enable enhanced mode functions,  write lower byte of 0x4AE8
     * WARNING: DO NOT ENABLE MMIO WITH BIT 5 HERE.
     * This bit will be OR'd into CR53 and thus makes impossible to setup
     * "new MMIO only" mode on Trio64+. This is despite the docs claiming Bit 5 is
     * "reserved" on there.
     * BEWARE: CR50 docs claim "00 = 1 byte. Bit 2 of 4AE8H selects between 4 (=0) and 8 (=1) bits/pixel "
     * This is wrong. 4AE8H, Bit2 = 0 is 8bits, 1  is 4bits.
     */
#if MMIO_ONLY
    // In "new MMIO only" mode, don't enable enhanced functions via MMIO write to ADVFUNC_CNTL.
    // Writing to ADVFUNC_CNTL just hung on Trio64+ cards.
    // Instead, chose CR66 to enable it. CR66 Bit0 has different meanings on pre-Trio64Plus chips
    vga.writeCRMask(0x66, BIT(1) | BIT(0), BIT(1) | BIT(0));
    vga.writeCRMask(0x66, BIT(1), 0);
#else
    // Trio64+ and Trio64M writing to ADVFUNC_CNTL via MMIO hangs the machine
    // USHORT sysCntl = R_IO_W(SUBSYS_STAT);
    USHORT advCntl = io.readW(S3_IO_ID(ADVFUNC_CNTL));
    advCntl |= BIT(0);  // | BIT(4) | BIT(7);
    io.writeW(S3_IO_ID(ADVFUNC_CNTL), advCntl);
    // sysCntl = R_IO_W(SUBSYS_STAT);
#endif

    /* This field contains the upper 6 bits (19-14) of the CPU base address,
     allowing accessing of up to 4 MBytes of display memory via 64K pages.
     When a non-zero value is programmed in this field, bits 3-0 of CR35
     and 3-2 of CR51 (the old CPU base address bits) are ignored.
     Bit 0 of CR31 must be set to 1 to enable this field.  */
    vga.writeCR(0x6a, 0x0);

    vga.writeSR(0x00, 0x00);
    vga.writeSR(0x01, 0x21);  // 8 DCLK per character clock, Display off
    vga.writeSR(0x02, 0x0f);
    vga.writeSR(0x03, 0x00);
    vga.writeSR(0x04, 0x02);
#if MMIO_ONLY
    // Bit 7 MMIO-ONLY - Memory-mapped I/O register access only
    // 0 = When MMIO is enabled, both programmed I/O and memory-mapped I/O register accesses are allowed
    // 1 = When MMIO is enabled, only memory-mapped I/O register accesses are allowed
    vga.writeSR(0x09, 0x80);
#endif
    vga.readSR(0x09);
    vga.writeSR(0x0D, 0x00);
    vga.writeSR(0x14, 0x00);

    // FIXME: this has memory setting implications potentially only valid for the
#ifdef CONFIG_CYBERVISION64
    vga.writeSR(0x0A, BIT(6));  // Support 4MB FPM RAM and 2MCLK memory writes
    vga.writeSR(0x18, BIT(6));  // 1 DCLK LUT Write Cycle
#endif

    ULONG clock = bi->MemoryClock;
    if (clock < 40000000) {
        clock = 40000000;
    }

#if BUILD_VISION864
    if (60000000 < clock) {
        clock = 60000000;
    }
#else
    if (chipFamily == AURORA64PLUS) {
        if (clock > 60000000) {
            clock = 60000000;
        }
    } else if (chipFamily >= TRIO64V2) {
        if (clock > 70000000) {
            clock = 70000000;
        }
    } else {
        if (clock > 65000000) {
            clock = 65000000;
        }
    }
#endif

    clock           = SetMemoryClock(bi, clock);
    bi->MemoryClock = clock;

    vga.writeCR(0x0, 0x5f);
    vga.writeCR(0x1, 0x4f);
    vga.writeCR(0x2, 0x50);
    vga.writeCR(0x3, 0x82);
    vga.writeCR(0x4, 0x54);
    vga.writeCR(0x5, 0x80);
    vga.writeCR(0x6, 0xbf);
    vga.writeCR(0x7, 0x1f);
    vga.writeCR(0x8, 0x0);
    vga.writeCR(0x9, 0x40);
    vga.writeCR(0xa, 0x0);
    vga.writeCR(0xb, 0x0);
    vga.writeCR(0xc, 0x0);
    vga.writeCR(0xd, 0x0);
    vga.writeCR(0xe, 0x0);
    vga.writeCR(0xf, 0x0);
    vga.writeCR(0x10, 0x9c);

    // 5 DRAM refresh cycles, unlock CR0/CR7, disable Vertical Interrupt
    vga.writeCR(0x11, 0x70);

    vga.writeCR(0x12, 0x8f);

    // Offset Register (SCREEN-OFFSET) (CR13)
    // Specifies the logical screen width (pitch). Bits 5-4 of CR51 are extension
    // bits 9-8 for this value. If these bits are OOb, bit 2 of
    // CR43 is extension bit 8 of this register.
    // 10-bit Value = quantity that is multiplied by 2 (word mode), 4 (doubleword
    // mode) or 8 (quadword mode) to specify the difference between the starting
    // byte addresses of two consecutive scan lines. This register contains the
    // least significant 8 bits of this

    vga.writeCR(0x13, 0x50);  // == 160, meaning 640byte in double word mode

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
    vga.writeCR(0x14, 0x40);

    vga.writeCR(0x15, 0x96);  // Start Vertical Blank Register (SVB) (CR15)
    vga.writeCR(0x16, 0xb9);  // End Vertical Blank Register (EVB) (CR16)

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
    vga.writeCR(0x17, 0xC3);  // Byte Adressing mode, V/HSync pulses enabled

    vga.writeCR(0x18, 0xff);  // Line compare register

    // Memory Configuration Register (MEM_CNFG) (eR31)
    // Bit 3 ENH MAP - Use Enhanced Mode Memory Mapping
    // 0= Force IBM VGA mapping for memory accesses
    // 1 = Force Enhanced Mode mappings
    // Setting this bit to 1 overrides the settings of bit 6 of CR14 and bit 3 of
    // CR17 and causes the use of doubleword memory addressing mode. Also, the
    // function of bits 3- 2 of GR6 is overridden with a fixed 64K map at AOOOOH.
    vga.writeCR(0x31, 0x08);  // Enhanced memory mapping, Doubleword mode

    vga.writeCR(0x32, 0x10);

    // BackWard Compatibility 2 Register (BKWD_2) (CR33)
    // Bit 1 DIS VDE - Disable Vertical Display End Extension Bits Write
    // Protection
    //  0 = VDE protection enabled
    //  1 = Disables the write protect setting of the bit 7 of CR11 on bits 1
    //      and 6 of CR7
    vga.writeCR(0x33, 0x02);

    // Backward Compatibility 3 Register (BKWD_3) (CR34)
    // Bit 4 ENB SFF - Enable Start Display FIFO Fetch Register(CR3B)
    vga.writeCRMask(0x34, BIT(4), BIT(4));

    /* Miscellaneous 1 (CR3A)
     * Bits 1-0 REF-CNT - Alternate Refresh Count Control
          01 = Refresh Count 1
       Bit 2 ENS RFC - Enable Alternate Refresh Count Control
         1 = Alternate refresh count control (bits 1-0) is enabled
       Bit 4 ENH 256 - Enable 8 Bits/Pixel or Greater Color Enhanced Mode
       Bit 5 HST DFW - Enable High Speed Text Font Writing */
    vga.writeCR(0x3a, 0x35);
    // Start Display FIFO Fetch (CR3B)
    vga.writeCR(0x3b, 0x5a);

    // Extended Mode Register (EXT_MODE) (CR43)
    vga.writeCR(0x43, 0x00);

    // Extended System Conttrol 1 Register (EX_SCTL_1)  (CR50)
    vga.writeCR(0x50, 0x00);

    // Extended System Control 2 Register (EX_SCTL_2) (CR51)
    vga.writeCR(0x51, 0x00);

    // MCLK M Parameter
    // 6-bit Value = maximum number of MCLKs that the LPB, CPU and Graphics Engine can
    // use to access memory before giving up control of the memory bus.
    // Bit 2 is the high order bit of this value.
    // W_CR_MASK(0x54, 0xF8, 0x70);

    vga.writeCR(0x60, 0xff);

    vga.writeCR(0x5d, 0x0);
    vga.writeCR(0x5e, 0x40);
    vga.writeGR(0x0, 0x0);
    vga.writeGR(0x1, 0x0);
    vga.writeGR(0x2, 0x0);
    vga.writeGR(0x3, 0x0);
    vga.writeGR(0x4, 0x0);
    vga.writeGR(0x5, 0x40);
    vga.writeGR(0x6, 0x1);
    vga.writeGR(0x7, 0xf);
    vga.writeGR(0x8, 0xff);

    // Enable writing attribute palette registers, disable video
    vga.readB(VgaReg::INSTAT1);
    vga.writeB(VgaReg::ATTR_AD, 0x0);

    // Reset AFF to index register selection
    vga.readB(VgaReg::INSTAT1);

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
        vga.writeAR(p, p);
    }
    vga.writeAR(0x30, 0x61);
    vga.writeAR(0x31, 0x0);
    vga.writeAR(0x32, 0xf);
    vga.writeAR(0x33, 0x0);
    vga.writeAR(0x34, 0x0);

    // Enable video
    vga.readB(VgaReg::INSTAT1);  // reset AFF
    vga.writeB(VgaReg::ATTR_AD, 0x20);

    switch (chipFamily) {
    case VISION864:
    case VISION968:
        if (!probeFramebufferVision(bi)) {
            return FALSE;
        }
        break;
    case TRIO64:
    case TRIO64PLUS:
    case TRIO64UVPLUS:
    case TRIO64V2:
        if (!probeFramebufferTrio64(bi)) {
            return FALSE;
        }
        break;
    case AURORA64PLUS:
        if (!probeFramebufferAurora64(bi)) {
            return FALSE;
        }
        break;
    default:
        D(0, "Unknown chip family for framebuffer probe\n");
        return FALSE;
    }

    setCacheMode(bi, bi->MemoryBase, bi->MemorySize & ~4095, MAPP_NONSERIALIZED | MAPP_IMPRECISE | MAPP_CACHEINHIBIT,
                 CACHEFLAGS);

    // Input Status ? Register (STATUS_O)
    D(1, "Monitor is %s present\n", ((vga.readB(VgaReg::MISC_OUT_W) & 0x10) ? "" : "NOT"));

    // FIXME VISION968:
    // The hardware graphics cursor requires the use of the pixel address bus. However this
    // bus is not used for Enhanced mode operation since pixel data is transferred via the
    // VRAM SID fines. Therefore, the hardware graphics cursor will normally not be used
    // with the Vision964. FIXME: have to use the RAMDAC's cursor

    // Two sprite images, each 64x64*2 bits
    const ULONG maxSpriteBuffersSize = (64 * 64 * 2 / 8);

    // take sprite image data off the top of the memory
    // sprites can be placed at segment boundaries of 1kb
    bi->MemorySize       = (bi->MemorySize - maxSpriteBuffersSize) & ~(1024 - 1);
    bi->MouseImageBuffer = bi->MemoryBase + bi->MemorySize;

    // Start Address in terms of 1024byte segments
    vga.writeCR(0x4c, 0);  // init to 0
    vga.writeCR(0x4d, 0);
    vga.writeCROverflow1((bi->MemorySize >> 10), 0x4d, 0, 8, 0x4c, 0, 4);
    // Sprite image offsets
    vga.writeCR(0x4e, 0);
    vga.writeCR(0x4f, 0);
    // Reset cursor position
    vga.writeCR(0x46, 0);
    vga.writeCR(0x47, 0);
    vga.writeCR(0x48, 0);
    vga.writeCR(0x49, 0);

    WaitForIdle(bi);

    // Write conservative scissors
    asS3(bi)->writeBee8(SCISSORS_T, 0x0000);
    asS3(bi)->writeBee8(SCISSORS_L, 0x0000);
    asS3(bi)->writeBee8(SCISSORS_B, 0x0fff);
    asS3(bi)->writeBee8(SCISSORS_R, 0x0fff);

    asS3(bi)->writeBee8(MULT_MISC2, 0);

    {
        const UBYTE memType    = (vga.readCR(0x36) >> 2) & 3;
        const BOOL oneCycleEDO = (memType == 0b00);
        UWORD multMisc         = oneCycleEDO ? 0 : BIT(10);
        (void)multMisc;
    }

    // Init GE write/read masks.
    asS3(bi)->writeBee8(MULT_MISC, (1 << 9));

    // Flush FIFO
    mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_NOP);

    waitFifo(bi, 16);
    mmio.writeL(S3_MMIO_ID(WRT_MASK), ~0);
    mmio.writeL(S3_MMIO_ID(RD_MASK), ~0);
    mmio.writeL(S3_MMIO_ID(COLOR_CMP), 0);

    mmio.writeW(S3_MMIO_ID(FRGD_MIX), CLR_SRC_FRGD_COLOR | MIX_NEW);
    mmio.writeW(S3_MMIO_ID(BKGD_MIX), CLR_SRC_BKGD_COLOR | MIX_NEW);

    // Flush FIFO
    mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_NOP);

    {
        LOCAL_SYSBASE();
        // reserve memory for a 8x8 monochrome pattern
        // Unfortunately we're overallocating here for the largest pitch.
        ULONG patternSize      = 8 * 3200;  // 800 * 4 * 8
        ULONG patternOffset    = (bi->MemorySize - patternSize);
        patternOffset          = (patternOffset / 3200) * 3200;  // align to 800pixel@32bit boundary
        bi->MemorySize         = patternOffset;
        cd->patternVideoBuffer = (ULONG *)(bi->MemoryBase + bi->MemorySize);
        cd->patternCacheBuffer = (UWORD *)AllocVec(patternSize, MEMF_PUBLIC);
    }

    // Read EDID after I2C initialization (for TRIO64PLUS and higher)
    if (chipFamily >= TRIO64PLUS) {
        UBYTE edid_data[EDID_BLOCK_SIZE];
        if (readEDID(bi, edid_data)) {
            char manufacturer[4];
            char product_name[14];

            getEDIDManufacturer(edid_data, manufacturer);
            D(INFO, "EDID: Manufacturer: %s\n", manufacturer);

            if (getEDIDProductName(edid_data, product_name)) {
                D(INFO, "EDID: Product Name: %s\n", product_name);
            }

            D(INFO, "EDID: Version %d.%d, Year: %d, Week: %d\n", edid_data[18], edid_data[19], edid_data[17] + 1990,
              edid_data[16]);
        } else {
            D(INFO, "EDID: Not available or read failed (monitor may not support EDID)\n");
        }
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef TESTEXE

#include <stdio.h>

static void testDumpModeInfo(const struct ModeInfo *mi, const char *label)
{
    D(ALWAYS,
      "\n=== %s ===\n"
      "W=%ld H=%ld Depth=%ld Flags=0x%02lx\n"
      "HTotal=%ld HBlank=%ld HSyncStart=%ld HSyncSize=%ld\n"
      "VTotal=%ld VBlank=%ld VSyncStart=%ld VSyncSize=%ld\n"
      "PixelClock=%ld Hz\n",
      label ? label : "(null)", (ULONG)mi->Width, (ULONG)mi->Height, (ULONG)mi->Depth, (ULONG)mi->Flags,
      (ULONG)mi->HorTotal, (ULONG)mi->HorBlankSize, (ULONG)mi->HorSyncStart, (ULONG)mi->HorSyncSize,
      (ULONG)mi->VerTotal, (ULONG)mi->VerBlankSize, (ULONG)mi->VerSyncStart, (ULONG)mi->VerSyncSize,
      (ULONG)mi->PixelClock);
}

static void testWaitForEnter(void)
{
    D(ALWAYS, "Press Enter for next mode...\n");
    int ch;
    do {
        ch = getchar();
    } while (ch != '\n' && ch != EOF);
}

static void testFillPattern8bpp(BoardInfo_t *bi, UWORD width, UWORD height)
{
    volatile UBYTE *mem = (volatile UBYTE *)bi->MemoryBase;
    for (UWORD y = 0; y < height; y++) {
        for (UWORD x = 0; x < width; x++) {
            mem[y * width + x] = (UBYTE)(x ^ y);
        }
    }
}

static void testApplyMode(BoardInfo_t *bi, struct ModeInfo *mi, const char *label)
{
    testDumpModeInfo(mi, label);

    bi->ModeInfo = mi;

    (void)ResolvePixelClock(bi, mi, mi->PixelClock, RGBFB_CLUT);

    DFUNC(ALWAYS, "SetClock\n");
    SetClock(bi);

    DFUNC(ALWAYS, "SetGC\n");
    SetGC(bi, mi, TRUE);

    DFUNC(ALWAYS, "SetDAC\n");
    SetDAC(bi, 0, RGBFB_CLUT);

    SetSprite(bi, FALSE, RGBFB_CLUT);

    SetPanning(bi, bi->MemoryBase, mi->Width, mi->Height, 0, 0, RGBFB_CLUT);

    DFUNC(ALWAYS, "SetDisplay ON\n");
    SetDisplay(bi, TRUE);

    testFillPattern8bpp(bi, mi->Width, mi->Height);

    /* GE pitch limits — FillRect falls back to FillRectDefault (needs P96). */
    if (mi->Width >= 640) {
        struct RenderInfo ri;
        ri.Memory      = bi->MemoryBase;
        ri.BytesPerRow = mi->Width;
        ri.RGBFormat   = RGBFB_CLUT;

        FillRect(bi, &ri, 0, 0, mi->Width, mi->Height, 0x10, 0xFF, RGBFB_CLUT);
        FillRect(bi, &ri, 4, 4, mi->Width - 8, mi->Height - 8, 0x7F, 0xFF, RGBFB_CLUT);
        FillRect(bi, &ri, 8, 8, mi->Width - 16, mi->Height - 16, 0xE0, 0xFF, RGBFB_CLUT);
    } else {
        D(ALWAYS, "Skip FillRect (width %ld < 640; no P96 FillRectDefault)\n", (ULONG)mi->Width);
    }
}

static volatile ULONG softVBlankCount;

static void ASM SoftVBlankCount(__REGA1(ULONG *count))
{
    (*count)++;
}

/* Enable CRTC IRQ, count SoftInterrupts for 2s.
 * PCI: pci_add_intserver; Zorro CV64: AddIntServer(INTB_PORTS). */
static void testVBlankInterrupt(BoardInfo_t *bi)
{
    LOCAL_SYSBASE();
    softVBlankCount = 0;
    BOOL addedHere  = FALSE;

    /* Server may run immediately — chip must not assert IRQ yet. */
    Disable();
    bi->SetInterrupt(bi, FALSE);
    Enable();

    bi->SoftInterrupt.is_Node.ln_Type = NT_INTERRUPT;
    bi->SoftInterrupt.is_Node.ln_Pri  = 0;
    bi->SoftInterrupt.is_Node.ln_Name = (char *)"TestS3SoftVBlank";
    bi->SoftInterrupt.is_Data         = (APTR)&softVBlankCount;
    bi->SoftInterrupt.is_Code         = (void (*)())SoftVBlankCount;

    bi->HardInterrupt.is_Node.ln_Type = NT_INTERRUPT;
    bi->HardInterrupt.is_Node.ln_Pri  = 0;
    bi->HardInterrupt.is_Node.ln_Name = (char *)"TestS3VBlank";
    bi->HardInterrupt.is_Data         = bi;
    /* is_Code set by InitChip */

    if (!(bi->CardFlags & CFF_VBLANK_INTSERVER)) {
#if OPENPCI
        LOCAL_OPENPCIBASE();
        if (!pci_add_intserver(&bi->HardInterrupt, getCardData(bi)->board)) {
            D(ERROR, "VBlank IRQ test: pci_add_intserver failed\n");
            return;
        }
#else
        AddIntServer(INTB_PORTS, &bi->HardInterrupt);
#endif
        bi->CardFlags |= CFF_VBLANK_INTSERVER;
        addedHere = TRUE;
    }

    bi->Flags |= BIF_VBLANKINTERRUPT;
    bi->SetInterrupt(bi, TRUE);

    {
        VgaIo vga = asS3(bi)->vga();
        UBYTE idx = vga.readB(VgaReg::CRTC_INDEX);
        vga.writeB(VgaReg::CRTC_INDEX, 0x11);
        UBYTE cr11 = vga.readB(VgaReg::CRTC_VALUE);
        vga.writeB(VgaReg::CRTC_INDEX, idx);
        D(ALWAYS, "VBlank IRQ test: CR11=0x%02lx INPUTSTATUS0=0x%02lx — counting 2s...\n", (ULONG)cr11,
          (ULONG)vga.readB(VgaReg::MISC_OUT_W));
    }

    delayMilliSeconds(2000);

    ULONG count = softVBlankCount;
    bi->SetInterrupt(bi, FALSE);

    if (addedHere) {
#if OPENPCI
        LOCAL_OPENPCIBASE();
        pci_rem_intserver(&bi->HardInterrupt, getCardData(bi)->board);
#else
        RemIntServer(INTB_PORTS, &bi->HardInterrupt);
#endif
        bi->CardFlags &= ~CFF_VBLANK_INTSERVER;
        bi->Flags &= ~BIF_VBLANKINTERRUPT;
    }

    D(ALWAYS, "VBlank IRQ test: %lu softints in 2s (~%lu Hz)\n", count, count / 2);
    if (count < 50)
        D(ERROR, "VBlank IRQ test: too few interrupts (expected ~120 @60Hz)\n");
}

BOOL TestCard(BoardInfo_t *bi, BOOL vblankTest)
{
    struct ChipBase *ChipBase = NULL;

    D(ALWAYS, "Trio64 has %ldkb usable memory\n", bi->MemorySize / 1024);

    {
        DFUNC(ALWAYS, "SetDisplay OFF\n");
        SetDisplay(bi, FALSE);
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
        DFUNC(ALWAYS, "SetDisplay OFF\n");
        SetDisplay(bi, FALSE);
    }

    static struct ModeInfo modes[6];
    static BOOL modesInit;
    if (!modesInit) {
        struct {
            UWORD w, h, depth, flags, htot, hblank, hss, hsz, vtot, vblank, vss, vsz;
            ULONG pix;
        } init[] = {
            {640, 480, 8, (UWORD)(GMF_HPOLARITY | GMF_VPOLARITY), 800, 0, 16, 96, 525, 0, 10, 2, 25175000UL},
            {320, 200, 8, (UWORD)(GMF_DOUBLESCAN | GMF_HPOLARITY | GMF_VPOLARITY), 400, 0, 8, 48, 225, 0, 6, 1,
             12587500UL},
            {320, 200, 8, (UWORD)(GMF_DOUBLESCAN | GMF_HPOLARITY | GMF_VPOLARITY), 400, 0, 8, 48, 224, 0, 6, 1,
             12587500UL},
            {320, 240, 8, (UWORD)(GMF_DOUBLESCAN | GMF_HPOLARITY | GMF_VPOLARITY), 400, 0, 8, 48, 263, 0, 5, 1,
             12587500UL},
            {320, 240, 8, (UWORD)(GMF_DOUBLESCAN | GMF_HPOLARITY | GMF_VPOLARITY), 400, 0, 8, 48, 262, 0, 5, 1,
             12587500UL},
            {1280, 1024, 8, 0, 1688, 0, 48, 128, 1066, 0, 3, 3, 108000000UL},
        };
        for (UWORD i = 0; i < 6; i++) {
            memset(&modes[i], 0, sizeof(modes[i]));
            modes[i].Width        = init[i].w;
            modes[i].Height       = init[i].h;
            modes[i].Depth        = init[i].depth;
            modes[i].Flags        = init[i].flags;
            modes[i].HorTotal     = init[i].htot;
            modes[i].HorBlankSize = init[i].hblank;
            modes[i].HorSyncStart = init[i].hss;
            modes[i].HorSyncSize  = init[i].hsz;
            modes[i].VerTotal     = init[i].vtot;
            modes[i].VerBlankSize = init[i].vblank;
            modes[i].VerSyncStart = init[i].vss;
            modes[i].VerSyncSize  = init[i].vsz;
            modes[i].PixelClock   = init[i].pix;
        }
        modesInit = TRUE;
    }

    static const char *modeNames[] = {
        "640x480@60 baseline",
        "320x200 doublescan vtotal=225",
        "320x200 doublescan vtotal=224",
        "320x240 doublescan vtotal=263",
        "320x240 doublescan vtotal=262",
        "1280x1024@60 8bpp",
    };

    if (vblankTest) {
        testApplyMode(bi, &modes[0], modeNames[0]);
        testVBlankInterrupt(bi);
        SetDisplay(bi, FALSE);
        return TRUE;
    }

    for (UWORD i = 0; i < (sizeof(modes) / sizeof(modes[0])); i++) {
        testApplyMode(bi, &modes[i], modeNames[i]);
        testWaitForEnter();
        DFUNC(ALWAYS, "SetDisplay OFF\n");
        SetDisplay(bi, FALSE);
    }

    return TRUE;

#if 0
#if 1
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

    VgaIo vga = asS3(bi)->vga();
    S3Io io = asS3(bi)->io();
    S3Mmio mmio = asS3(bi)->mmio();

    asS3(bi)->writeBee8(MULT_MISC, (1 << 9));
    // Flush FIFO
    mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_NOP);

    WaitBlitter(bi);
    flushWrites();
    WaitForIdle(bi);

    asS3(bi)->readBee8(MULT_MISC);
    io.readL(S3_IO_ID(WRT_MASK));

    asS3(bi)->setGEFormat(640 * 4, 4);
    asS3(bi)->writeBee8(MULT_MISC, (1 << 9));

    // Flush FIFO
    mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_NOP);
    flushWrites();
    WaitForIdle(bi);
    asS3(bi)->readBee8(MULT_MISC);
    // R_IO_L(WRT_MASK);

    // W_IO_L(WRT_MASK, 0xaabbccdd);

    flushWrites();

    // Flush FIFO
    D(INFO, "1");
    mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_NOP);
    flushWrites();
    WaitForIdle(bi);

    D(INFO, "2");
    asS3(bi)->readBee8(MULT_MISC);
    // R_MMIO_W(WRT_MASK);
    // R_IO_L(WRT_MASK);

    // W_IO_L(WRT_MASK, 0xbaadf00d);
    // Flush FIFO
    D(INFO, "3");
    mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_NOP);
    flushWrites();
    WaitForIdle(bi);

    D(INFO, "4");
    asS3(bi)->readBee8(MULT_MISC);
    // R_MMIO_W(WRT_MASK);
    // R_IO_L(WRT_MASK);

    mmio.writeL(S3_MMIO_ID(WRT_MASK), 0xcafebabe);
    // Flush FIFO
    D(INFO, "3");
    mmio.writeW(S3_MMIO_ID(CMD), CMD_ALWAYS | CMD_TYPE_NOP);
    flushWrites();
    WaitForIdle(bi);

    D(INFO, "4");
    asS3(bi)->readBee8(MULT_MISC);
    // R_MMIO_W(WRT_MASK);
    mmio.readL(S3_MMIO_ID(WRT_MASK));

    return TRUE;
#endif /* disabled legacy TestCard code */
}

#if !defined(CONFIG_CYBERVISION64)

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
extern struct Library *DOSBase;
extern struct Library *UtilityBase;
struct IORequest ioRequest;
struct Device *TimerBase;

void sigIntHandler(int dummy)
{
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
    }
    abort();
}

/* VBLANK/S — args buffer must be long-aligned (static); stack LONGs broke ReadArgs. */
static const char testArgsTemplate[] = "VBLANK/S";
static LONG testArgs[1];

int main(void)
{
    signal(SIGINT, sigIntHandler);

    int rval              = EXIT_FAILURE;
    BOOL vblankTest       = FALSE;
    struct RDArgs *rdargs = NULL;
    struct pci_dev *board = NULL;
    struct BoardInfo *bi  = NULL;
    /* BoardInfo is ~2KB — keep off the default 4KB stack. */
    static struct BoardInfo boardInfo;

    testArgs[0] = 0;
    rdargs      = ReadArgs((STRPTR)testArgsTemplate, testArgs, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR) "TestS3");
        goto exit;
    }
    vblankTest = testArgs[0] ? TRUE : FALSE;
    FreeArgs(rdargs);
    rdargs = NULL;

    D(ALWAYS, "Args: VBLANK=%ld\n", (LONG)vblankTest);

#if OPENPCI
    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        D(0, "Unable to open openpci.library\n");
        goto exit;
    }
#endif

    D(0, "Looking for S3 Trio64 card\n");

    while ((board = FindBoard(board, PRM_Vendor, VENDOR_ID_S3, TAG_END)) != NULL) {
        memset(&boardInfo, 0, sizeof(boardInfo));
        bi = &boardInfo;

        CardData_t *card  = getCardData(bi);
        P96_HOOK(bi->ExecBase, SysBase);
        P96_HOOK(bi->UtilBase, UtilityBase);
        bi->ChipBase      = NULL;
        card->OpenPciBase = OpenPciBase;
        card->board       = board;
        bi->MemoryClock   = 54000000;

        if (!initRegisterAndMemoryBases(bi)) {
            continue;
        }

        D(ALWAYS, "S3: %s found\n", getChipFamilyName((ChipFamily_t)getChipData(bi)->chipFamily));

        D(ALWAYS, "Trio64 init chip\n");
        if (!InitChip(bi)) {
            D(ERROR, "InitChip failed. Exit");
            goto exit;
        }
        D(ALWAYS, "Trio64 has %ldkb usable memory\n", bi->MemorySize / 1024);

        TestCard(bi, vblankTest);

        WaitBlitter(bi);
        rval = EXIT_SUCCESS;
        goto exit;
    }

    D(ERROR, "no Trio64 found.\n");

exit:
    if (rdargs) {
        FreeArgs(rdargs);
    }
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
        OpenPciBase = NULL;
    }
    return rval;
}
#endif  // !defined(CONFIG_CYBERVISION64)
#endif  // TESTEXE

#ifdef __cplusplus
}
#endif
