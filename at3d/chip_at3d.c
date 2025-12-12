#include "chip_at3d.h"
#include "at3d_i2c.h"
#include "edid_common.h"

#define __NOLIBBASE__

#include <clib/debug_protos.h>
#include <debuglib.h>
#include <exec/types.h>

#define OPENPCI_SWAP  // don't make it define its own SWAP macros
#include <exec/interrupts.h>
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/exec.h>
#include <proto/openpci.h>

#include <SDI_stdarg.h>

#ifdef DBG
int debugLevel = VERBOSE;
#endif

/******************************************************************************/
/*                                                                            */
/* library exports                                                            */
/*                                                                            */
/******************************************************************************/

#ifndef TESTEXE
const char LibName[]     = "AT3D.chip";
const char LibIdString[] = "Alliance ProMotion AT3D Picasso96 chip driver version 1.0";

const UWORD LibVersion  = 1;
const UWORD LibRevision = 0;
#endif

// Helper function to probe framebuffer memory size
static ULONG probeFramebufferSize(BoardInfo_t *bi)
{
    LOCAL_SYSBASE();

    DFUNC(INFO, "Probing framebuffer memory size...\n");

    volatile UBYTE *memBase = (volatile UBYTE *)bi->MemoryBase;
    ULONG maxSize           = 4 * 1024 * 1024;

    // Test pattern for memory probing
    ULONG testOffset = 0;

    // Try to find memory boundary by testing at power-of-2 offsets
    for (ULONG size = 1 * 1024 * 1024; size <= maxSize; size *= 2) {
        testOffset = size - 32768 - 4;  // Test near the boundary

        // Save original values at test locations
        ULONG original       = *(volatile ULONG *)(memBase + testOffset);
        ULONG originalAtHalf = *(volatile ULONG *)(memBase + size / 2);
        ULONG prevSize       = size / 2;

        // Use a unique test pattern based on the test offset to detect wraparound
        // If memory wraps, writing at testOffset might appear at a different location
        ULONG uniquePattern = (ULONG)memBase + testOffset;  // Make pattern unique to this offset

        // Write unique pattern at test offset
        *(volatile ULONG *)(memBase + testOffset) = uniquePattern;
        // Write unique pattern at start of current segment
        *(volatile ULONG *)(memBase + size / 2) = uniquePattern;

        CacheClearU();

        // Read back from test offset
        ULONG readback  = *(volatile ULONG *)(memBase + testOffset);
        ULONG readback0 = *(volatile ULONG *)(memBase + size / 4);

        // Restore original values
        *(volatile ULONG *)(memBase + testOffset) = original;
        *(volatile ULONG *)(memBase + size / 2)   = originalAtHalf;
        CacheClearU();

        if (readback0 == uniquePattern) {
            // Wraparound detected - pattern written at testOffset or size/2 appeared at size/4
            DFUNC(INFO, "Memory wraparound detected, returning size: %ld KB\n", prevSize / 1024);
            return prevSize;  // Return previous size
        }
        if (readback != uniquePattern) {
            // Memory doesn't respond at this offset, we've found the boundary
            DFUNC(INFO, "Memory boundary detected returning size: %ld KB\n", prevSize / 1024);
            return prevSize;  // Return previous size
        }
    }

    // If we got here, use the maximum size we tested
    DFUNC(INFO, "Using maximum tested size: %ld KB\n", maxSize / 1024);
    return maxSize;
}

// Test register aperture (BAR1) access
static BOOL testRegisterAperture(BoardInfo_t *bi)
{
    DFUNC(INFO, "Testing register aperture access...\n");

    if (!bi->RegisterBase) {
        DFUNC(ERROR, "Register base is NULL\n");
        return FALSE;
    }

    REGBASE();

    // Test scratch pad register in sequencer registers (0x20-0x27)
    // Sequencer registers are accessed via SEQX (0x3C4) index and SEQ_DATA (0x3C5) data
    // Read current value from scratch pad register
    UBYTE original = R_SR(SR_SCRATCH_PAD);

    // Write test pattern to scratch pad register
    UBYTE testPattern = 0xAA;
    W_SR(SR_SCRATCH_PAD, testPattern);

    // Read back from scratch pad register
    UBYTE readback = R_SR(SR_SCRATCH_PAD);

    // Restore original value
    W_SR(SR_SCRATCH_PAD, original);

    DFUNC(INFO, "Scratch pad register (SR%02lx): wrote 0x%02lx, read 0x%02lx, original 0x%02lx\n",
          (ULONG)SR_SCRATCH_PAD, (ULONG)testPattern, (ULONG)readback, (ULONG)original);

    // Check if we can read back what we wrote
    if (readback == testPattern) {
        DFUNC(INFO, "Register test: PASSED - scratch pad register responds correctly\n");
        return TRUE;
    } else {
        DFUNC(ERROR, "Registertest: FAILED - scratch pad register readback mismatch\n");
        return FALSE;
    }
}

// Test MMVGA window (BAR0) access
static BOOL testMMIO(BoardInfo_t *bi)
{
    DFUNC(INFO, "Testing MMIO access...\n");

    LOCAL_OPENPCIBASE();

    // Get device ID from PCI configuration space
    ULONG pciDeviceId = 0;
    CardData_t *card  = getCardData(bi);
    if (!GetBoardAttrs(card->board, PRM_Device, (Tag)&pciDeviceId, TAG_END)) {
        DFUNC(ERROR, "Could not retrieve device ID from PCI configuration\n");
        return FALSE;
    }

    MMIOBASE();

    // MMVGA window maps PCI configuration space
    // Read device ID from MMVGA window at DEVICE_ID (memory offset 182-183h per AT3D documentation)
    UWORD deviceId = R_MMIO_W(DEVICE_ID);

    DFUNC(INFO, "Device ID from PCI config: 0x%04lx, from MMIO window: 0x%04lx\n", (ULONG)pciDeviceId, (ULONG)deviceId);

    // Compare device IDs
    if (deviceId == (UWORD)pciDeviceId) {
        DFUNC(INFO, "MMIO test: PASSED - device ID matches\n");
        return TRUE;
    } else {
        DFUNC(ERROR, "MMIO test: FAILED - device ID mismatch\n");
        return FALSE;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Clock computation and programming functions

// AT3D clock formula: FOUT = (N+1)(FREF) / ((M+1)(2^L))
// Where:
//   FREF = 14.318 MHz (recommended reference frequency)
//   N = 8-127 (numerator)
//   M = 1-5 (denominator)
//   L = 0-3 (postscaler, 2^L = 1, 2, 4, 8)

static const struct svga_pll at3d_pll = {9, 128, 2, 6, 0, 3, 185000, 370000, 14318};

// Helper function to compute frequency from PLL values (in kHz)
static ULONG computeKhzFromPllValue(const PLLValue_t *pllValue)
{
    // FOUT = (N+1)(FREF) / ((M+1)(2^L))
    // FREF = 14.318 MHz = 14318 kHz
    // pllValue->n and pllValue->m are actual N and M values (not register values)
    // The registers store N-1 and M-1, but pllValue contains the actual values
    ULONG fref        = 14318;
    ULONG numerator   = (ULONG)(pllValue->n) * fref;
    ULONG denominator = (ULONG)(pllValue->m);
    return (numerator / denominator) >> pllValue->l;
}

/**
 * Compute PLL values for a target frequency (used for MCLK only)
 * @param targetFreqHz Target frequency in Hz
 * @param n Output: Numerator (N, 8-127)
 * @param m Output: Denominator (M, 1-5)
 * @param l Output: Postscaler index (L, 0-3)
 * @return Actual frequency in Hz, or 0 if no valid combination found
 */
static ULONG computePLLValues(ULONG targetFreqHz, UBYTE *n, UBYTE *m, UBYTE *l)
{
    UWORD _m, _n, _r;
    int freq = svga_compute_pll(&at3d_pll, targetFreqHz / 1000, &_m, &_n, &_r);
    if (freq == -1) {
        return 0;  // No valid combination found
    }
    // the AT manual defines M and N in the opposite way to how S3 did it
    *n = _m;
    *m = _n;
    *l = _r;
    return freq * 1000;  // Convert kHz to Hz
}

// Initialize PLL table for pixel clocks
void initPixelClockPLLTable(BoardInfo_t *bi)
{
    DFUNC(VERBOSE, "", bi);

    LOCAL_SYSBASE();

    ChipData_t *cd = getChipData(bi);

    // AT3D supports up to ~135MHz pixel clock
    UWORD maxFreq    = 175;  // 175MHz max
    UWORD minFreq    = 12;   // 12MHz min
    UWORD numEntries = (maxFreq - minFreq + 1) * 2;

    PLLValue_t *pllValues = AllocVec(sizeof(PLLValue_t) * numEntries, MEMF_PUBLIC);
    if (!pllValues) {
        DFUNC(ERROR, "Failed to allocate PLL table\n");
        return;
    }

    cd->pllValues    = pllValues;
    cd->numPllValues = 0;

    // For higher color depths, limit to frequencies per AT3D specifications (Table 2.6.1)
    // 15/16-bit modes: up to 144 MHz (1600×1200 @ 75Hz)
    // 24/32-bit modes: up to 75 MHz (1280×1024 @ 60Hz)
    ULONG maxHiColorFreq   = 144000;  // 144MHz max for HiColor (15/16-bit)
    ULONG maxTrueColorFreq = 75000;   // 75MHz max for TrueColor (24/32-bit)

    bi->PixelClockCount[PLANAR]    = 0;
    bi->PixelClockCount[HICOLOR]   = 0;
    bi->PixelClockCount[TRUECOLOR] = 0;
    bi->PixelClockCount[TRUEALPHA] = 0;
    bi->PixelClockCount[CHUNKY]    = 0;

    // Generate PLL values for each frequency
    int lastValue = 0;
    for (UWORD i = 0; i < numEntries; ++i) {
        ULONG freq = minFreq * 1000 + i * 500;  // Frequency in kHz
        UWORD m, n, r;
        int currentKhz = svga_compute_pll(&at3d_pll, freq, &m, &n, &r);

        if (currentKhz >= 0 && currentKhz != lastValue) {
            lastValue                     = currentKhz;
            pllValues[cd->numPllValues].n = m;
            pllValues[cd->numPllValues].m = n;
            pllValues[cd->numPllValues].l = r;

            DFUNC(CHATTY, "Pixelclock %03ld %09ldHz: n=%ld m=%ld l=%ld\n", (ULONG)cd->numPllValues - 1,
                  (ULONG)currentKhz * 1000, (ULONG)pllValues[cd->numPllValues].n, (ULONG)pllValues[cd->numPllValues].m,
                  (ULONG)pllValues[cd->numPllValues].l);

            cd->numPllValues++;

            bi->PixelClockCount[CHUNKY]++;
            if (currentKhz <= maxHiColorFreq) {
                bi->PixelClockCount[HICOLOR]++;
                if (currentKhz <= maxTrueColorFreq) {
                    bi->PixelClockCount[TRUECOLOR]++;
                    bi->PixelClockCount[TRUEALPHA]++;
                }
            }
        }
    }

    D(VERBOSE, "Initialized %ld PLL entries\n", cd->numPllValues);

    DFUNC(INFO, "PixelClockCount: Planar %ld, Chunky %ld, HiColor %ld, TrueColor %ld, TrueAlpha %ld\n",
          bi->PixelClockCount[PLANAR], bi->PixelClockCount[CHUNKY], bi->PixelClockCount[HICOLOR],
          bi->PixelClockCount[TRUECOLOR], bi->PixelClockCount[TRUEALPHA]);
}

/**
 * Program MCLK (Memory Clock)
 * @param bi BoardInfo structure
 * @param clockHz Target frequency in Hz
 * @return Actual frequency set in Hz
 */
ULONG SetMemoryClock(struct BoardInfo *bi, ULONG clockHz)
{
    DFUNC(INFO, "Setting MCLK to %ld Hz\n", clockHz);

    UBYTE n, m, l;
    ULONG actualFreq = computePLLValues(clockHz, &n, &m, &l);

    if (actualFreq == 0) {
        DFUNC(ERROR, "Failed to compute MCLK PLL values for %ld Hz\n", clockHz);
        return clockHz;  // Return requested frequency as fallback
    }

    DFUNC(INFO, "MCLK: N=%ld, M=%ld, L=%ld, actual=%ld Hz\n", (ULONG)n, (ULONG)m, (ULONG)l, actualFreq);

    MMIOBASE();

    W_MMIO_B(MCLK_CTRL, CLK_POSTSCALER(3));

    // Write denominator (M) - mask to ensure only valid bits are set
    W_MMIO_MASK_B(MCLK_DEN, CLK_DEN_MASK, m - 1);

    // Write numerator (N) - mask to ensure only valid bits are set
    W_MMIO_MASK_B(MCLK_NUM, CLK_NUM_MASK, n - 1);

    // Wait for PLL to stabilize
    delayMilliSeconds(5);

    // Set postscaler in control register and enable MCLK programming
    W_MMIO_B(MCLK_CTRL, CLK_FREQ_RANGE(0b100) | CLK_POSTSCALER(l) | CLK_HIGH_SPEED);

    if (clockHz > 50000000) {
        W_MMIO_MASK_W(DISP_MEM_CFG, FAST_RAS_DISABLE_MASK, FAST_RAS_DISABLE);
    }

    return actualFreq;
}

ULONG SetMemoryClock(struct BoardInfo *bi, ULONG clockHz);
void initPixelClockPLLTable(BoardInfo_t *bi);

/**
 * Get I2C operations structure for EDID support
 * @param bi BoardInfo structure
 * @return Pointer to I2COps_t structure, or NULL if not initialized
 */
I2COps_t *getI2COps(struct BoardInfo *bi)
{
    CardData_t *card = getCardData(bi);
    return &card->i2cOps;
}

// Stub implementations for required functions
// FIXME: BoardInfo defines this function as returning a BOOL, but what are we supposed to return?!
static BOOL ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    // Clocking Mode Register (ClK_MODE) (SR1)
    REGBASE();
    LOCAL_SYSBASE();

    DFUNC(VERBOSE, " state %ld\n", (ULONG)state);

    // SR1 bit 5: Screen Off (1 = screen off, 0 = screen on)
    W_SR_MASK(0x01, 0x20, (~(UBYTE)state & 1) << 5);

    return TRUE;
}

static BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL expected))
{
    REGBASE();
    return (R_REG(0x3DA) & 0x08) != 0;
}

static void setDefaultClocks(struct BoardInfo *bi)
{
    DFUNC(INFO, "\n");

    MMIOBASE();

    {
        USHORT vclk0 = 0x0F0;
        // Set postscaler in control register
        W_MMIO_B(vclk0, CLK_POSTSCALER_MASK | CLK_POSTSCALER(3));

        // Write denominator (M-1) - register stores M-1, chip uses (register_value + 1)
        W_MMIO_MASK_B(vclk0 + 1, CLK_DEN_MASK, 0x01);

        // Write numerator (N-1) - register stores N-1, chip uses (register_value + 1)
        W_MMIO_MASK_B(vclk0 + 2, CLK_NUM_MASK, 0x1b);

        // Use computed frequency range F from ResolvePixelClock
        W_MMIO_MASK_B(vclk0, CLK_POSTSCALER_MASK | CLK_FREQ_RANGE_MASK | CLK_POWER_OFF | CLK_BYPASS, 0x6c);

        delayMilliSeconds(1);  // Short delay

        UBYTE vclkCtrlVal = R_MMIO_B(vclk0);
        W_MMIO_B(vclk0, (vclkCtrlVal & 0xF9) | 0x4);
        W_MMIO_B(vclk0, (vclkCtrlVal & 0x79) | 0x4);
        W_MMIO_B(vclk0, (vclkCtrlVal & 0x79) | 0x84);
        W_MMIO_B(vclk0, vclkCtrlVal);
    }
    {
        USHORT vclk1 = 0x0F4;
        // Set postscaler in control register
        W_MMIO_B(vclk1, CLK_POSTSCALER_MASK | CLK_POSTSCALER(3));

        // Write denominator (M-1) - register stores M-1, chip uses (register_value + 1)
        W_MMIO_MASK_B(vclk1 + 1, CLK_DEN_MASK, 0x02);

        // Write numerator (N-1) - register stores N-1, chip uses (register_value + 1)
        W_MMIO_MASK_B(vclk1 + 2, CLK_NUM_MASK, 0x2e);

        // Use computed frequency range F from ResolvePixelClock
        W_MMIO_MASK_B(vclk1, CLK_POSTSCALER_MASK | CLK_FREQ_RANGE_MASK | CLK_POWER_OFF | CLK_BYPASS, 0x5c);

        delayMilliSeconds(1);  // Short delay

        UBYTE vclkCtrlVal = R_MMIO_B(vclk1);
        W_MMIO_B(vclk1, (vclkCtrlVal & 0xF9) | 0x4);
        W_MMIO_B(vclk1, (vclkCtrlVal & 0x79) | 0x4);
        W_MMIO_B(vclk1, (vclkCtrlVal & 0x79) | 0x84);
        W_MMIO_B(vclk1, vclkCtrlVal);
    }
}
static LONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi),
                                  __REGD0(ULONG desiredPixelClock), __REGD7(RGBFTYPE rgbFormat))
{
    DFUNC(VERBOSE, "desiredPixelClock=%ld Hz, format=%ld\n", desiredPixelClock, (ULONG)rgbFormat);

    if (!mi) {
        return desiredPixelClock;
    }

    const ChipData_t *cd = getChipData(bi);

    ULONG targetFreqKhz = desiredPixelClock / 1000;

    // Find the best matching PLL entry using binary search
    UWORD upper     = cd->numPllValues - 1;
    ULONG upperFreq = computeKhzFromPllValue(&cd->pllValues[upper]);
    UWORD lower     = 0;
    ULONG lowerFreq = computeKhzFromPllValue(&cd->pllValues[lower]);

    while (lower + 1 < upper) {
        UWORD middle     = (upper + lower) / 2;
        ULONG middleFreq = computeKhzFromPllValue(&cd->pllValues[middle]);

        if (targetFreqKhz > middleFreq) {
            lower     = middle;
            lowerFreq = middleFreq;
        } else {
            upper     = middle;
            upperFreq = middleFreq;
        }
    }

    // Return the best match between upper and lower
    if (targetFreqKhz - lowerFreq > upperFreq - targetFreqKhz) {
        lower     = upper;
        lowerFreq = upperFreq;
    }

    mi->PixelClock = lowerFreq * 1000;

    PLLValue_t pllValues = cd->pllValues[lower];

    // Calculate VCO frequency: FVCO = (N+1) * FREF / (M+1)
    // FREF = 14.318 MHz = 14318 kHz
    ULONG fref = 14318;                               // Reference frequency in kHz
    ULONG fvco = (pllValues.n * fref) / pllValues.m;  // VCO frequency in kHz

    // Compute frequency range F based on VCO frequency (from apm.c formula)
    // Formula: f = (c + 500 - 34*fvco/1000)/1000, where c = 1000*(380*7)/(380-175)
    int c = 1000 * (380 * 7) / (380 - 175);  // ≈ 12976
    int f = (c + 500 - 34 * fvco / 1000) / 1000;
    if (f > 7)
        f = 7;  // Clamp to 3-bit field (0-7)
    if (f < 0)
        f = 0;

    DFUNC(CHATTY, "VCO frequency: %ld kHz, Frequency range F: %ld\n", fvco, (ULONG)f);

    // Store PLL values in the format expected by SetClock
    // AT3D registers store N-1 and M-1 (the chip uses register_value + 1 in calculations)
    // pll1.Numerator = N-1 (register value, 7-126 for N=8-127)
    // pll2.Denominator = (L << 6) | (F << 3) | (M-1)
    //   Bits [6:7] = L (postscaler, 0-3)
    //   Bits [3:5] = F (frequency range, 0-7)
    //   Bits [0:2] = M-1 (denominator, 0-4)
    mi->pll1.Numerator   = pllValues.n - 1;
    mi->pll2.Denominator = (pllValues.l << 6) | (f << 3) | (pllValues.m - 1);

    DFUNC(CHATTY, "Reporting pixelclock Hz: %ld, index: %ld,  N:%ld M:%ld L:%ld \n\n", mi->PixelClock, (ULONG)lower,
          (ULONG)pllValues.n, (ULONG)pllValues.m, (ULONG)pllValues.l);

    return lower;  // Return the index into the PLL table
}

static INLINE void waitFifo(const BoardInfo_t *bi, UBYTE numSlots)
{
    MMIOBASE();
    while ((R_MMIO_B(EXT_DAC_STATUS) & 0x0F) < numSlots) {
        // Busy wait
    }
}

static void ASM SetClock(__REGA0(struct BoardInfo *bi))
{
    DFUNC(INFO, "\n");

    struct ModeInfo *mi = bi->ModeInfo;
    if (!mi) {
        DFUNC(ERROR, "ModeInfo is NULL\n");
        return;
    }

    D(INFO, "SetClock: PixelClock %ld Hz\n", mi->PixelClock);

    // Extract N, M, L, F from ModeInfo
    // ModeInfo stores register values: N-1 (7-126 for actual N=8-127) and M-1 (0-4 for actual M=1-5)
    // pll1.Numerator = N-1 (register value)
    // pll2.Denominator = (L << 6) | (F << 3) | (M-1)
    //   Bits [6:7] = L (postscaler, 0-3)
    //   Bits [3:5] = F (frequency range, 0-7)
    //   Bits [0:2] = M-1 (denominator, 0-4)
    UBYTE nReg = mi->pll1.Numerator;                  // Register value (N-1)
    UBYTE mReg = mi->pll2.Denominator & 0x07;         // Register value (M-1) in lower 3 bits
    UBYTE l    = (mi->pll2.Denominator >> 6) & 0x03;  // L in bits 6-7
    UBYTE f    = (mi->pll2.Denominator >> 3) & 0x07;  // F in bits 3-5

    // Calculate actual values for logging
    UBYTE nActual = nReg + 1;
    UBYTE mActual = mReg + 1;
    DFUNC(VERBOSE, "VCLK: N=%ld (reg=0x%lx), M=%ld (reg=0x%lx), L=%ld, F=%ld\n", (ULONG)nActual, (ULONG)nReg,
          (ULONG)mActual, (ULONG)mReg, (ULONG)l, (ULONG)f);

    MMIOBASE();
    REGBASE();

    // Set postscaler to 8x
    W_MMIO_MASK_B(VCLK_CTRL, CLK_POSTSCALER_MASK, CLK_POSTSCALER(3));

    // Write denominator (M-1) - register stores M-1, chip uses (register_value + 1)
    W_MMIO_B(VCLK_DEN, mReg);

    // Write numerator (N-1) - register stores N-1, chip uses (register_value + 1)
    W_MMIO_B(VCLK_NUM, nReg);

    // Use computed frequency range F from ResolvePixelClock
    W_MMIO_MASK_B(VCLK_CTRL, CLK_POSTSCALER_MASK | CLK_FREQ_RANGE_MASK | CLK_POWER_OFF | CLK_BYPASS,
                  CLK_POSTSCALER(l) | CLK_FREQ_RANGE(f));

    delayMicroSeconds(10);  // Short delay

    // PLL resync sequence: clear then set CLK_HIGH_SPEED bit
    W_MMIO_MASK_B(VCLK_CTRL, CLK_HIGH_SPEED, 0);
    W_MMIO_MASK_B(VCLK_CTRL, CLK_HIGH_SPEED, CLK_HIGH_SPEED);
}

static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD1(RGBFTYPE rgbFormat))
{
    DFUNC(INFO, "Index: %ld\n", index);

    const ChipData_t *cd = getChipData(bi);

    if (!cd->pllValues || index >= cd->numPllValues) {
        DFUNC(ERROR, "Invalid pixel clock index %ld (max %ld)\n", index, cd->numPllValues - 1);
        return 0;
    }

    ULONG frequency = computeKhzFromPllValue(&cd->pllValues[index]);
    return frequency * 1000;  // Convert kHz to Hz
}

static UWORD ASM CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                      __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE rgbFormat))
{
    // FIXME: extend getBPP for 24bit and YUV formats
    return (width * getBPP(rgbFormat) + 7) & ~7;
}

static APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi), __REGA1(APTR mem), __REGD0(struct RenderInfo *ri),
                                __REGD7(RGBFTYPE format))
{
    // AT24 and up: redirect non-packed formats to big-endian aperture
    if (getChipData(bi)->chipFamily >= AT24) {
        switch (format) {
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
        case RGBFB_A8R8G8B8:
        case RGBFB_R8G8B8A8:
        case RGBFB_A8B8G8R8:
        case RGBFB_R8G8B8:
            // Redirect to Big Endian Linear Address Window
            return mem + 0x800000;
        default:
            return mem;
        }
    }
    return mem;
}

static ULONG ASM GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    if (format == RGBFB_NONE)
        return (ULONG)0;

    // Base compatible formats that AT3D always supports (native/packed formats)
    // These formats can be used without special aperture configuration
    ULONG compatible = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8 | RGBFF_B8G8R8A8;

    // AT24 and up: non-packed formats available via big-endian aperture
    if (getChipData(bi)->chipFamily >= AT24) {
        switch (format) {
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
            // In Big Endian aperture, configured for byte swapping in words only
            compatible |= RGBFF_R5G6B5 | RGBFF_R5G5B5;
            break;
        case RGBFB_A8R8G8B8:
        case RGBFB_R8G8B8A8:
        case RGBFB_A8B8G8R8:
        case RGBFB_R8G8B8:
            // In Big Endian aperture, configured for byte swapping
            compatible |= RGBFF_A8R8G8B8 | RGBFF_R8G8B8A8 | RGBFF_A8B8G8R8 | RGBFF_R8G8B8;
            break;
        }
    }

    return compatible;
}

// Wait for blitter (drawing engine) to finish
static void ASM WaitBlitter(__REGA0(struct BoardInfo *bi))
{
    MMIOBASE();

    // Wait until drawing engine is not busy (bit 9 of EXT_DAC_STATUS)
    // Per AT3D documentation: 1FC[9] = drawing engine busy
    while (R_MMIO_L(EXT_DAC_STATUS) & EXT_DAC_DRAWING_ENGINE_BUSY) {
        // Busy wait - ideally this should be interrupt-driven
    }
}

static INLINE void ASM SetMemoryModeInternal(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    ChipData_t *cd = getChipData(bi);
    if (cd->chipFamily < AT24) {
        // No bi-endian support
        return;
    }

    MMIOBASE();

    // Check if format has changed
    if (cd->MemFormat == format) {
        return;
    }
    cd->MemFormat = format;

    WaitBlitter(bi);

    // Setup the bi-endian control register for aperture 1 (8-16MB)
    // This controls byte swapping for non-packed formats
    // Register is at offset 0xDC-0xDD in MMVGA window (BAR0)
    UWORD biendianCtrl       = R_MMIO_W(BIENDIAN_CTRL);
    UWORD aperture1Transform = BIENDIAN_NO_TRANSFORM;

    switch (format) {
    case RGBFB_A8R8G8B8:
        // 32-bit format: swap all bytes within a double word
        aperture1Transform = BIENDIAN_32BIT_TRANS;
        break;
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        // 16-bit formats: swap bytes within a word
        aperture1Transform = BIENDIAN_16BIT_TRANS;
        break;
    default:
        // Packed formats (RGBFB_R5G6B5PC, RGBFB_R5G5B5PC, etc.) and CLUT
        // No byte swapping needed
        aperture1Transform = BIENDIAN_NO_TRANSFORM;
        break;
    }

    // Update aperture 1 transform code (bits [3:2])
    biendianCtrl = (biendianCtrl & ~BIENDIAN_APERTURE1_MASK) | (aperture1Transform << 2);
    W_MMIO_W(BIENDIAN_CTRL, biendianCtrl);

    return;
}

static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    __asm __volatile("\t movem.l d0-d1/a0-a1,-(sp)\n"
                     : /* no result */
                     :
                     :);

    SetMemoryModeInternal(bi, format);

    __asm __volatile("\t movem.l (sp)+,d0-d1/a0-a1\n"
                     : /* no result */
                     :
                     : "d0", "d1", "a0", "a1");
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

    UWORD hTotal      = mi->HorTotal;
    UWORD ScreenWidth = mi->Width;
    UBYTE modeFlags   = mi->Flags;
    BOOL isInterlaced = (modeFlags & GMF_INTERLACE) ? 1 : 0;
    UBYTE depth       = mi->Depth;

#define ADJUST_HBORDER(x) AdjustBorder(x, border, 8)
#define ADJUST_VBORDER(y) AdjustBorder(y, border, 1);
#define TO_CLKS(x)        ((x) >> 3)
#define TO_SCANLINES(y)   ToScanLines((y), modeFlags)

    REGBASE();

    // VGA CRTC registers
    // All VGA CRTC registers are supported. In addition, the horizontal and vertical timing, start, and
    // offset have been extended at 3D5.19–1D, which are described starting on page 181.
    // A bit exists to lock VGA CRTC registers. When the lock bit is set, writes to the VGA portion of
    // the CRTC registers (3D4 index 0–18) are ignored. When the lock bit is not set, writes to the
    // VGA portion of any CRTC register cause the extended CRTC bits of all registers to be reset, so in
    // order to load extended values into these registers, the VGA portions of all CRTC registers must
    // be loaded first. Writing any VGA CRTC register (assumed to be a mode-switch) also disables
    // cursor enable and motion video enable.

    {
        // Horizontal Total (CR0)
        // AT3D uses CR1B[0] for horizontal total overflow
        UWORD hTotalClk = TO_CLKS(hTotal) - 5;
        D(INFO, "Horizontal Total %ld\n", (ULONG)hTotalClk);
        W_CR_OVERFLOW1(hTotalClk, 0x00, 0, 8, 0x1B, 0, 1);
    }
    {
        // Horizontal Display End Register (CR1)
        // One less than the total number of displayed characters
        UWORD hDisplayEnd = TO_CLKS(ScreenWidth) - 1;
        D(INFO, "Display End %ld\n", (ULONG)hDisplayEnd);
        W_CR_OVERFLOW1(hDisplayEnd, 0x01, 0, 8, 0x1B, 1, 1);
    }

    UWORD hBorderSize = ADJUST_HBORDER(mi->HorBlankSize);
    UWORD hBlankStart = TO_CLKS(ScreenWidth + hBorderSize) - 1;
    {
        // Start Horizontal Blank Register (CR2)
        D(INFO, "Horizontal Blank Start %ld\n", (ULONG)hBlankStart);
        W_CR_OVERFLOW1(hBlankStart, 0x02, 0, 8, 0x1B, 2, 1);
    }

    {
        // End Horizontal Blank Register (CR3)
        UWORD hBlankEnd = TO_CLKS(hTotal - hBorderSize) - 1;
        D(INFO, "Horizontal Blank End %ld\n", (ULONG)hBlankEnd);
        W_CR_OVERFLOW1(hBlankEnd, 0x03, 0, 5, 0x05, 7, 1);
    }

    UWORD hSyncStart = TO_CLKS(mi->HorSyncStart + ScreenWidth);
    {
        // Start Horizontal Sync Position Register (CR4)
        D(INFO, "HSync start %ld\n", (ULONG)hSyncStart);
        W_CR_OVERFLOW1(hSyncStart, 0x04, 0, 8, 0x1B, 3, 1);
    }

    UWORD endHSync = TO_CLKS(mi->HorSyncStart + ScreenWidth + mi->HorSyncSize);
    {
        // End Horizontal Sync Position Register (CR5)
        D(INFO, "HSync End %ld\n", (ULONG)endHSync);
        W_CR_MASK(0x05, 0x1f, endHSync);
    }

    {
        // Vertical Total (CR6)
        UWORD vTotal = TO_SCANLINES(mi->VerTotal) - 2;
        D(INFO, "VTotal %ld\n", (ULONG)vTotal);
        W_CR_OVERFLOW3(vTotal, 0x06, 0, 8, 0x07, 0, 1, 0x07, 5, 1, 0x1A, 0, 1);
    }

    {
        // Vertical Display End register (CR12)
        UWORD vDisplayEnd = TO_SCANLINES(mi->Height) - 1;
        D(INFO, "Vertical Display End %ld\n", (ULONG)vDisplayEnd);
        // FIXME: check the overflow bit order. Counterintuitively, bit 1 nd 6 are
        // opposite to what they are on S3Trio.
        // W_CR_OVERFLOW3(vDisplayEnd, 0x12, 0, 8, 0x7, 6, 1, 0x7, 1, 1, 0x1A, 1, 1);

        // FIXME: I think the Alliance Specs are wrong here... why would they introduce
        // incompatibility with the standard VGA registers when all the other registers
        // follow the order for the overflow bits?

        // S3Trio64 order:
        W_CR_OVERFLOW3(vDisplayEnd, 0x12, 0, 8, 0x07, 1, 1, 0x07, 6, 1, 0x1A, 1, 1);
    }

    // Program vertical retrace registers (CR10-11) BEFORE vertical blank registers (CR15-16)
    // This matches the ROM's programming order and may be required for proper VSYNC initialization
    UWORD vRetraceStart = TO_SCANLINES(mi->Height + mi->VerSyncStart) - 1;
    {
        // Vertical Retrace Start Register (VRS) (CR10)
        D(INFO, "VRetrace Start %ld\n", (ULONG)vRetraceStart);
        W_CR_OVERFLOW3(vRetraceStart, 0x10, 0, 8, 0x07, 2, 1, 0x07, 7, 1, 0x1A, 3, 1);
    }

    {
        // Vertical Retrace End Register (VRE) (CR11) Bits 3-0 VERTICAL RETRACE END
        // Note: CR11 bit 5 controls vertical interrupt enable (should remain set to disable interrupt)
        // CR11 bit 7 is write protect (handled separately)
        UWORD vRetraceEnd     = TO_SCANLINES(mi->Height + mi->VerSyncStart + mi->VerSyncSize) - 1;
        UBYTE vRetraceEndLow4 = vRetraceEnd & 0x0F;
        D(INFO, "VRetrace End %ld (raw value), writing bits [3:0] = %ld\n", (ULONG)vRetraceEnd, (ULONG)vRetraceEndLow4);
        W_CR_MASK(0x11, 0x0F, vRetraceEndLow4);
    }

    UWORD vBlankSize = ADJUST_VBORDER(mi->VerBlankSize);
    {
        // Start Vertical Blank Register (SVB) (CR15)
        UWORD vBlankStart = mi->Height;
        if ((modeFlags & GMF_DOUBLESCAN) != 0) {
            vBlankStart = vBlankStart * 2;
        }
        vBlankStart = ((vBlankStart + vBlankSize) >> isInterlaced) - 1;
        D(INFO, "VBlank Start %ld\n", (ULONG)vBlankStart);
        W_CR_OVERFLOW3(vBlankStart, 0x15, 0, 8, 0x07, 3, 1, 0x09, 5, 1, 0x1A, 2, 1);
    }

    {
        // End Vertical Blank Register (EVB) (CR16)
        UWORD vBlankEnd = mi->VerTotal;
        if ((modeFlags & GMF_DOUBLESCAN) != 0) {
            vBlankEnd = vBlankEnd * 2;
        }
        vBlankEnd = ((vBlankEnd - vBlankSize) >> isInterlaced) - 1;
        D(6, "VBlank End %ld\n", (ULONG)vBlankEnd);
        W_CR(0x16, vBlankEnd);
    }

    // Enable Interlace
    // AT3D uses memory-mapped register 0D2h for interlace control
    {
        MMIOBASE();
        UBYTE interlaceCtrl = R_MMIO_B(MONITOR_INTERLACE_CTRL);
        if (isInterlaced) {
            interlaceCtrl |= BIT(0);  // Enable interlace (bit 0 of 0D2h)
        } else {
            interlaceCtrl &= ~BIT(0);  // Disable interlace
        }
        W_MMIO_B(MONITOR_INTERLACE_CTRL, interlaceCtrl);
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

    // Horizontal interlaced start
    W_CR(0x19, 0);
}

static void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD0(UWORD region), __REGD7(RGBFTYPE format))
{
    DFUNC(INFO, "format=%ld\n", (ULONG)format);

    MMIOBASE();

    if (format >= RGBFB_MaxFormats) {
        DFUNC(ERROR, "Invalid format %ld\n", (ULONG)format);
        return;
    }

    UBYTE pixelDepth  = 0;
    UBYTE pixelFormat = DESKTOP_FORMAT_DIRECT;  // Default to direct RGB
    UBYTE doubleIndex = 0;                      // Double index bit [5]

    switch (format) {
    case RGBFB_CLUT:
        pixelFormat = DESKTOP_FORMAT_INDEXED;
        pixelDepth  = DESKTOP_DEPTH_8BPP;
        break;

    case RGBFB_R5G5B5PC:
    case RGBFB_R5G5B5:
        pixelFormat = DESKTOP_FORMAT_DIRECT;
        pixelDepth  = DESKTOP_DEPTH_15BPP;
        break;

    case RGBFB_R5G6B5PC:
    case RGBFB_R5G6B5:
        pixelFormat = DESKTOP_FORMAT_DIRECT;
        pixelDepth  = DESKTOP_DEPTH_16BPP;
        break;

    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        // 24-bit: AT3D doesn't have a direct 24-bit mode, use 32-bit
        pixelFormat = DESKTOP_FORMAT_DIRECT;
        pixelDepth  = DESKTOP_DEPTH_32BPP;
        break;

    case RGBFB_A8R8G8B8:
    case RGBFB_B8G8R8A8:
    case RGBFB_R8G8B8A8:
    case RGBFB_A8B8G8R8:
        pixelFormat = DESKTOP_FORMAT_DIRECT;
        pixelDepth  = DESKTOP_DEPTH_32BPP;
        break;

    case RGBFB_NONE:
        // VGA mode
        pixelFormat = DESKTOP_FORMAT_INDEXED;
        pixelDepth  = DESKTOP_DEPTH_VGA;
        break;

    default:
        DFUNC(ERROR, "Unsupported format %ld\n", (ULONG)format);
        return;
    }

    // Read current register value
    UBYTE regValue = R_MMIO_B(SERIAL_CTRL);

    // Clear pixel depth and format bits
    regValue &= ~(DESKTOP_PIXEL_DEPTH_MASK | DESKTOP_PIXEL_FORMAT_MASK);

    // Set new pixel depth and format
    regValue |= pixelDepth | pixelFormat;

    // Handle double index for CLUT with double clock (if needed)
    // Note: Double index requires specific conditions per documentation
    if ((format == RGBFB_CLUT) && (bi->ModeInfo && (bi->ModeInfo->Flags & GMF_DOUBLECLOCK))) {
        regValue |= BIT(5);  // Enable double index
    } else {
        regValue &= ~BIT(5);  // Disable double index
    }

    // Write the register
    W_MMIO_B(SERIAL_CTRL, regValue);

    DFUNC(VERBOSE, "SetDAC: format=0x%lx, depth=0x%02lx, format=0x%02lx, reg=0x%02lx\n", (ULONG)format,
          (ULONG)pixelDepth, (ULONG)pixelFormat, (ULONG)regValue);
}

static void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    DFUNC(VERBOSE, "startIndex %ld, count %ld\n", (ULONG)startIndex, (ULONG)count);

    REGBASE();
    LOCAL_SYSBASE();

    // This may not be interrupted, so DAC_WR_AD remains set throughout the function
    Disable();

    W_REG(DAC_WR_AD, startIndex);

    struct CLUTEntry *entry = &bi->CLUT[startIndex];

    // Write color data for each palette entry
    // Do not print these individual register writes as it takes ages
    for (UWORD c = 0; c < count; ++c) {
        writeReg(RegBase, DAC_DATA, entry->Red);
        writeReg(RegBase, DAC_DATA, entry->Green);
        writeReg(RegBase, DAC_DATA, entry->Blue);
        ++entry;
    }

    Enable();

    DFUNC(VERBOSE, "done.\n", (ULONG)startIndex, (ULONG)count);
    return;
}

static void ASM SetPanning(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD4(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset),
                           __REGD7(RGBFTYPE format))
{
    REGBASE();
    LOCAL_SYSBASE();

    DFUNC(INFO,
          "mem 0x%lx, width %ld, height %ld, xoffset %ld, yoffset %ld, "
          "format %ld\n",
          (ULONG)memory, (ULONG)width, (ULONG)height, (LONG)xoffset, (LONG)yoffset, (ULONG)format);

    LONG panOffset;
    UWORD pitch;
    ULONG memOffset;

    bi->XOffset = xoffset;
    bi->YOffset = yoffset;
    memOffset   = (ULONG)memory - (ULONG)bi->MemoryBase;

    // Calculate pitch and panning offset based on format
    switch (format) {
    case RGBFB_NONE:
        pitch     = width >> 3;  // Planar modes: bytes per row
        panOffset = (ULONG)yoffset * (width >> 3) + (xoffset >> 3);
        break;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        // 24-bit modes: 3 bytes per pixel
        pitch     = width * 3;
        panOffset = (yoffset * width + xoffset) * 3;
        break;
    case RGBFB_A8R8G8B8:
    case RGBFB_A8B8G8R8:
    case RGBFB_R8G8B8A8:
    case RGBFB_B8G8R8A8:
        // 32-bit modes: 4 bytes per pixel
        pitch     = width * 4;
        panOffset = (yoffset * width + xoffset) * 4;
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
    case RGBFB_B5G6R5PC:
    case RGBFB_B5G5R5PC:
        // 16-bit modes: 2 bytes per pixel
        pitch     = width * 2;
        panOffset = (yoffset * width + xoffset) * 2;
        break;
    default:
        // RGBFB_CLUT and others: 1 byte per pixel
        pitch     = width;
        panOffset = yoffset * width + xoffset;
        break;
    }

    WaitBlitter(bi);

    // AT3D Serial offset register (CR13) is in units of 8 bytes
    // Serial offset [7:0] in CR13, [11:8] in CR1C[7:4]
    pitch = (pitch + 7) / 8;  // Convert to units of 8 bytes

    // AT3D Serial start address (CR0C-0D) is in doublewords (4 bytes)
    // Start address [15:0] in CR0C-0D, [19:16] in CR1C[3:0]
    panOffset = (panOffset + memOffset) / 4;  // Convert to doublewords

    D(INFO, "panOffset 0x%lx (dwords), pitch %ld (8-byte units)\n", (ULONG)panOffset, (ULONG)pitch);

    // Set Serial start address: CR0C-0D (low 16 bits), CR1C[3:0] (bits 19:16)
    // Start address is in doublewords (4 bytes)
    // FIXME: 0x0c and 0x0d are again swapped from what the S3Trio does
    // W_CR_OVERFLOW2_ULONG(panOffset, 0x0c, 0, 8, 0x0d, 0, 8, 0x1c, 0, 4);

    // test s3trio order
    W_CR_OVERFLOW2_ULONG(panOffset, 0x0d, 0, 8, 0x0c, 0, 8, 0x1c, 0, 4);

    // Set Serial offset: CR13 (low 8 bits), CR1C[7:4] (bits 11:8)
    // Offset is in units of 8 bytes
    W_CR_OVERFLOW1(pitch, 0x13, 0, 8, 0x1c, 4, 4);

    return;
}

/**
 * Set DPMS (Display Power Management Signaling) level
 * @param bi BoardInfo structure
 * @param level DPMS level: DPMS_ON (0), DPMS_STANDBY (1), DPMS_SUSPEND (2), DPMS_OFF (3)
 */
static void ASM SetDPMSLevel(__REGA0(struct BoardInfo *bi), __REGD0(ULONG level))
{
    DFUNC(VERBOSE, "SetDPMSLevel: level=%ld\n", level);
    // Mapping:
    //  DPMS_ON:      Both bits clear (0x00) - HSYNC and VSYNC enabled
    //  DPMS_STANDBY: VSYNC disabled (0x02) - bit [1] set
    //  DPMS_SUSPEND: HSYNC disabled (0x01) - bit [0] set
    //  DPMS_OFF:     Both bits set (0x03) - both HSYNC and VSYNC disabled

    static const UBYTE DPMSLevels[4] = {0x00, 0x01, 0x02, 0x03};

    if (level > 3) {
        level = 3;
    }

    MMIOBASE();

    // Set DPMS level in bits [1:0], preserving other bits
    W_MMIO_MASK_B(DPMS_SYNC_CTRL, 0x03, DPMSLevels[level]);
}

// FIXME: implement, but make sure to coordinate with SetDPMSLevel
static void WaitVerticalSync(__REGA0(struct BoardInfo *bi), __REGD0(BOOL waitForEnd)) {}

static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask)) {}

static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask)) {}

static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask)) {}

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
    splitPos -= 1;

    W_CR_OVERFLOW3((UWORD)splitPos, 0x18, 0, 8, 0x7, 4, 1, 0x9, 6, 1, 0x1a, 4, 1);
}

BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    DFUNC(ALWAYS, "AT3D InitChip - Testing hardware access\n");

    LOCAL_SYSBASE();

    ChipData_t *cd = getChipData(bi);

    {
        LEGACYIOBASE();

        // The older AT 6410 cards had the wakup registers
        if (cd->chipFamily < AT24) {
            W_REG(0x46E8, 0x10);
            W_REG(0x102, 0x01);
            W_REG(0x46E8, 0x08);
        }
        // Unlock extended registers
        W_SR(0x10, 0x12);
        // R_SR(0x10);  // Debug read

        W_SR(0x1B, 0x00);
        W_SR(0x1C, 0x00);

        // Map HOST BLT port to last 32K of flat space less final 2K
        // Map ProMotion registers to  last 2K of flat space.
        W_SR_MASK(0x1b, 0x3F, (0b100 << 3) | (0b100));
        // Enable flat memory access, set aperture to 4MB and disable VGA memory (A000:0–BFFF:F) access
        W_SR_MASK(0x1c, 0x3F, 0b101101);
    }

    MMIOBASE();

    if (getChipData(bi)->chipFamily >= AT24) {
#define LDEV_MASK (0x3 << 4)
#define LDEV(x)   ((x) << 4)

        // Enable extended registers, disable classic VGA IO range,
        // enable coprocessor aperture, enable second linear aperture
        W_MMIO_B(ENABLE_EXT_REGS, 0x0F);
        // Doc say about the MMVGA address space:
        // LDEV wait states register field (0xD9[5:4]) must be programmed to value 2 in order to access this space.
        UBYTE extSigTiming = R_MMIO_B(EXTSIG_TIMING);
        extSigTiming |= LDEV(2);  //  I have seen 0x59  used in the ROM
        W_MMIO_B(EXTSIG_TIMING, extSigTiming);
    }

    if (!testMMIO(bi)) {
        D(ERROR, "MMIO test failed - cannot access MMIO window\n");
        return FALSE;
    }

    // From here on we can access the MMVGA window
    REGBASE();

    char chipId[10] = {0};
    for (int c = 0; c < 9; ++c) {
        chipId[c] = R_SR(0x11 + c);
    }

    D(INFO, "Chip ID: %s\n", chipId);

    // Test register aperture (BAR1)
    if (!testRegisterAperture(bi)) {
        D(ERROR, "Register aperture test failed - cannot access registers\n");
        return FALSE;
    }

    // Probe framebuffer memory size
    if (!(bi->MemorySize = probeFramebufferSize(bi))) {
        D(ERROR, "Failed to probe framebuffer memory size\n");
        return FALSE;
    }

    // invoke auto-reset of many non-VGA registers
    {
        W_CR_MASK(CR_EXT_AUTORESET, CR_EXT_AUTORESET_DISABLE, 0);
        W_CR(0x00, 0x00);
        W_CR_MASK(CR_EXT_AUTORESET, CR_EXT_AUTORESET_DISABLE, CR_EXT_AUTORESET_DISABLE);
    }

    setDefaultClocks(bi);
    W_MISC_MASK(0x0C, 0);  // Disable programmable VCLK

    W_MMIO_MASK_W(DISP_MEM_CFG, BIT(5), BIT(5));  // 128bit  graphics engine access. Manual says this MUST be set.

    if (bi->MemorySize >= 2 * 1024 * 1024) {
        W_MMIO_MASK_W(DISP_MEM_CFG, BIT(8), BIT(8));  // 64bit memory bus
    }

    // Scratchpad registers
    R_SR(0x20);
    for (int i = 0x21; i <= 0x27; ++i) {
        W_SR(i, 0x00);
    }
    W_MMIO_B(ABORT, 0);
    W_MMIO_B(COLOR_CORRECTION, BIT(4));  // 8bit per gun palette write
    W_MMIO_B(DAC_CTRL, BIT(0));          // DAC Powered, Blanking pedesta, no overcurrent boost
    W_MMIO_B(SIGANALYSER_CTRL, 0);       // Disable signal analyser
    W_MMIO_B(FEATURE_CTRL, 0);
    W_MMIO_L(VMI_PORT_CTRL, 0);
    W_MMIO_B(THP_CTRL, 0);
    W_MMIO_B(GPIO_CTRL, 0);
    W_MMIO_B(OVERCURRENT_RED, 0);
    W_MMIO_B(OVERCURRENT_GREEN, 0);
    W_MMIO_B(OVERCURRENT_BLUE, 0);

    W_MMIO_B(MONITOR_INTERLACE_CTRL, 0x00);

    W_MMIO_W(VGA_OVERRIDE, BIT(5) | BIT(6) | BIT(7) | BIT(9) | BIT(12));
    W_MMIO_B(SERIAL_CTRL, BIT(6) | DESKTOP_DEPTH_8BPP | DESKTOP_FORMAT_INDEXED);  //

    W_MMIO_MASK_B(APERTURE_CTRL, PALETTE_ACCESS_MASK, PALETTE_ACCESS(0b01));  // Disable RAMDAC snooping
    W_MMIO_B(DPMS_SYNC_CTRL, 0x00);  // Clear bits [1:0] to enable both HSYNC and VSYNC
    W_MMIO_B(PIXEL_FIFO_REQ_POINT, 0x16);
    W_MMIO_B(PIXEL_FIFO_REQ_POINT + 1, 0x16);
    W_MMIO_B(PIXEL_FIFO_REQ_POINT + 2, 0x16);

    W_MMIO_B(0xDA, 0x00);  // This used to be an "Internal Register" on older 6210 cards(?)

    W_MMIO_W(DISP_MEM_CFG, 0x0520);  // Single Cycle Page Mode, mem64, 128bit gfx access

    W_MMIO_B(VCLK_CTRL, 0x5C);
    W_MMIO_B(VCLK_DEN, 0x01);
    W_MMIO_B(VCLK_NUM, 0x1b);

    W_MISC_MASK(0xF, 0b1111);  // enable Host Memory Access, programmable VCLK, Color Mode (3DA, 3D4 and 3D5 enabled))

    {
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

        W_AR(0x10, 0x41);  // 256color mode, graphics

        // Enable video
        R_REG(0x3DA);         // reset AFF
        W_REG(ATR_AD, 0x20);  // enable video
    }

    // Setup 8 pixels per DCLK, screen off
    W_SR(0x01, BIT(5) | SR_CHAR_CLOCK_8_DOT);
    W_SR(0x03, 0x00);
    W_SR(0x04, 0x06);  // >64k present, unchained mode

    W_GR(0x00, 0x00);
    W_GR(0x01, 0x00);
    W_GR(0x02, 0x00);
    W_GR(0x03, 0x00);
    W_GR(0x04, 0x00);
    W_GR(0x05, 0x00);
    W_GR(0x06, 0x01);
    W_GR(0x07, 0x0F);
    W_GR(0x08, 0xFF);

    W_CR(0x08, 0x00);
    W_CR(0x09, 0x00);
    W_CR(0x0A, 0x00);
    W_CR(0x0B, 0x00);
    W_CR(0x0C, 0x00);  // "Serial Start" = Start address of screen to 0
    W_CR(0x0D, 0x00);
    W_CR(0x0E, 0x00);
    W_CR(0x0F, 0x00);

    W_CR(0x11, 0x20);  // Disable Vertical Interrupt, ack. Interrupt
    W_CR(0x11, 0x30);  // Normal retrace

    W_CR(0x1C, 0x00);
    W_CR(0x13, 0x50);    // "Serial Offset" = pitch 80 * 8 = 640 byte
    W_CR(0x14, BIT(6));  // Enable "Double Word mode"  CRTC display memory addresses incremented by 4
    W_CR(0x17, BIT(7) | BIT(6) | BIT(5) | BIT(0));  // CRTC  Mode Control Register, enable HSYNC and VSYNC
    W_CR(0x18, 0xff);                               // Line compare register

    W_REG(DAC_MASK, 0xFF);

    UBYTE miscOut = R_REG(MISC_R);
    D(INFO, "Monitor is %s present (may be inaccurate)\n", (miscOut & 0x10) ? "" : "not");

    // Initialize I2C operations for EDID support
    CardData_t *card     = getCardData(bi);
    card->i2cOps.init    = at3dI2cInit;
    card->i2cOps.setScl  = at3dI2cSetScl;
    card->i2cOps.setSda  = at3dI2cSetSda;
    card->i2cOps.readScl = at3dI2cReadScl;
    card->i2cOps.readSda = at3dI2cReadSda;

    D(INFO, "Attempting EDID readout of monitor\n");
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

    // Set up basic BoardInfo structure
    bi->GraphicsControllerType = GCT_APM;  // Will need to define AT3D type
    bi->PaletteChipType        = PCT_APM;  // Will need to define AT3D type
    bi->Flags                  = 0;        // Minimal flags for now

    // AT3D supports CLUT (8-bit palette), hicolor (15/16-bit), and truecolor (24/32-bit)
    // Per AT3D specifications: "Optimized 24- and 32-bit truecolor", "hi-color, and 256-color GUI"
    bi->RGBFormats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8 | RGBFF_B8G8R8A8;

    // AT24 and up can support non-packed formats via big-endian aperture
    if (getChipData(bi)->chipFamily >= AT24) {
        // With big-endian aperture, we can also support:
        bi->RGBFormats |= RGBFF_R5G6B5 | RGBFF_R5G5B5 | RGBFF_A8R8G8B8;
    }

    // Initialize PLL table for pixel clocks
    initPixelClockPLLTable(bi);

    // Set maximum bitmap dimensions
    // AT3D supports coordinate addressing with up to 4096 pixels per line
    // Maximum bitmap size is limited by available memory and coordinate range
    bi->MaxBMWidth  = 2048;
    bi->MaxBMHeight = 2048;

    // Bits per cannon (used for coordinate calculations and palette loading)
    // AT3D can be configured to use 8 bits per gun for full 24-bit color
    bi->BitsPerCannon = 8;

    // Maximum horizontal/vertical values per format type
    // These are based on the coordinate addressing limits
    // For 8-bit modes: 4096 pixels max (511 * 8 dclks)
    bi->MaxHorValue[PLANAR] = 4088;  // 511 * 8 dclks
    bi->MaxHorValue[CHUNKY] = 4088;
    // For 16-bit modes: 8192 pixels max (511 * 8 * 2)
    bi->MaxHorValue[HICOLOR] = 8176;  // 511 * 8 * 2
    // For 32-bit modes: 16384 pixels max (511 * 8 * 4)
    bi->MaxHorValue[TRUECOLOR] = 16352;  // 511 * 8 * 4
    bi->MaxHorValue[TRUEALPHA] = 16352;

    // Maximum vertical values (2047 scanlines, 11-bit coordinate)
    // FIXME: since the CTC "serial offset" register is in units of 8 bytes, this should be dependent on the screen mode
    bi->MaxVerValue[PLANAR]    = 2047;
    bi->MaxVerValue[CHUNKY]    = 2047;
    bi->MaxVerValue[HICOLOR]   = 2047;
    bi->MaxVerValue[TRUECOLOR] = 2047;
    bi->MaxVerValue[TRUEALPHA] = 2047;

    // Maximum resolution per format type
    // Determined by 10-bit value divided by bpp and practical limits
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

    // Set function pointers
    bi->SetGC                = SetGC;
    bi->SetPanning           = SetPanning;
    bi->CalculateBytesPerRow = CalculateBytesPerRow;
    bi->CalculateMemory      = CalculateMemory;
    bi->GetCompatibleFormats = GetCompatibleFormats;
    bi->SetDAC               = SetDAC;
    bi->SetColorArray        = SetColorArray;
    bi->SetDisplay           = SetDisplay;
    bi->SetMemoryMode        = SetMemoryMode;
    bi->ResolvePixelClock    = ResolvePixelClock;
    bi->GetPixelClock        = GetPixelClock;
    bi->SetClock             = SetClock;
    bi->SetWriteMask         = SetWriteMask;
    bi->SetReadPlane         = SetReadPlane;
    bi->SetClearMask         = SetClearMask;
    bi->GetVSyncState        = GetVSyncState;
    bi->WaitVerticalSync     = WaitVerticalSync;

    bi->SetDPMSLevel     = SetDPMSLevel;
    bi->SetSplitPosition = SetSplitPosition;

    SetSplitPosition(bi, 0);

    SetMemoryClock(bi, 50000000);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef TESTEXE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct UtilityBase *UtilityBase;

static struct BoardInfo boardInfo  = {0};
static struct Library *OpenPciBase = NULL;

void sigIntHandler(int dummy)
{
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
        OpenPciBase = NULL;
    }
    abort();
}

int main()
{
    signal(SIGINT, sigIntHandler);

    int rval = EXIT_FAILURE;

    memset(&boardInfo, 0, sizeof(boardInfo));

    struct BoardInfo *bi = &boardInfo;

    bi->ExecBase = SysBase;
    bi->UtilBase = UtilityBase;

    D(INFO, "AT3D Test Executable\n");
    D(INFO, "UtilityBase 0x%lx\n", bi->UtilBase);

    // Open openpci library
    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        DFUNC(ERROR, "Cannot open openpci.library v%ld+\n", MIN_OPENPCI_VERSION);
        goto exit;
    }

    // Find AT3D card
    struct pci_dev *board = NULL;
    CardData_t *card      = getCardData(bi);
    card->OpenPciBase     = OpenPciBase;

    D(INFO, "Looking for Alliance Promotion card, Vendor ID " STRINGIFY(VENDOR_ID_ALLIANCE) "\n");

    board = FindBoard(board, PRM_Vendor, VENDOR_ID_ALLIANCE, TAG_END);

    if (!board) {
        DFUNC(ERROR, "No card found\n");
        goto exit;
    }

    D(INFO, "Alliance Promotion card found\n");
    card->board = board;

    // Initialize register and memory bases
    if (!initRegisterAndMemoryBases(bi)) {
        DFUNC(ERROR, "Failed to initialize register and memory bases\n");
        goto exit;
    }

    // Test chip initialization
    D(INFO, "Calling InitChip...\n");
    if (!InitChip(bi)) {
        DFUNC(ERROR, "InitChip failed\n");
        goto exit;
    }

    // Set up a test screen mode (640x480 8-bit CLUT)
    {
        struct ModeInfo mi;

        mi.Depth            = 8;
        mi.Flags            = GMF_HPOLARITY | GMF_VPOLARITY;
        mi.Height           = 480;
        mi.Width            = 640;
        mi.HorBlankSize     = 0;
        mi.HorEnableSkew    = 0;
        mi.HorSyncSize      = 96;
        mi.HorSyncStart     = 16;
        mi.HorTotal         = 800;
        mi.PixelClock       = 25175000;  // 25.175 MHz for 640x480@60Hz
        mi.pll1.Numerator   = 0;
        mi.pll2.Denominator = 0;
        mi.VerBlankSize     = 0;
        mi.VerSyncSize      = 2;
        mi.VerSyncStart     = 10;
        mi.VerTotal         = 525;

        bi->ModeInfo = &mi;

        DFUNC(ALWAYS, "ResolvePixelClock\n");
        ULONG index = ResolvePixelClock(bi, &mi, mi.PixelClock, RGBFB_CLUT);

        DFUNC(ALWAYS, "SetClock\n");
        SetClock(bi);

        DFUNC(ALWAYS, "SetGC\n");
        SetGC(bi, &mi, FALSE);

        DFUNC(ALWAYS, "SetDAC\n");
        SetDAC(bi, 0, RGBFB_CLUT);

        DFUNC(ALWAYS, "SetColorArray\n");
        // Set up a grayscale palette
        for (int c = 0; c < 256; c++) {
            bi->CLUT[c].Red   = c;
            bi->CLUT[c].Green = c;
            bi->CLUT[c].Blue  = c;
        }
        SetColorArray(bi, 0, 256);

        DFUNC(ALWAYS, "SetPanning\n");
        SetPanning(bi, bi->MemoryBase, 640, 480, 0, 0, RGBFB_CLUT);

        DFUNC(ALWAYS, "SetDisplay ON\n");
        SetDisplay(bi, TRUE);

        DFUNC(ALWAYS, "SetDPMSLevel  ON\n");
        SetDPMSLevel(bi, DPMS_ON);
        // }

        // Write a test pattern to the framebuffer
        DFUNC(ALWAYS, "Writing test pattern\n");
        for (int y = 0; y < 480; y++) {
            for (int x = 0; x < 640; x++) {
                *(volatile UBYTE *)(bi->MemoryBase + y * 640 + x) = x;
            }
        }
        D(INFO, "Alliance Promotion test completed\n");
        D(INFO, "Screen should now be displaying a test pattern\n");
        rval = EXIT_SUCCESS;
    }

exit:
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
    }
    return rval;
}

#endif  // TESTEXE
