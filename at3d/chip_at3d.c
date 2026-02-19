#include "chip_at3d.h"
#include "at3d_i2c.h"
#include "edid_common.h"

#define __NOLIBBASE__

#include <clib/debug_protos.h>
#include <debuglib.h>
#include <exec/types.h>
#include <graphics/rastport.h>

#define OPENPCI_SWAP  // don't make it define its own SWAP macros
#include <exec/interrupts.h>
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/exec.h>
#include <proto/openpci.h>

#include <SDI_stdarg.h>

#ifdef DBG
#ifdef TESTEXE
int debugLevel = VERBOSE;
#else
int debugLevel = VERBOSE;
#endif
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

#define MIN_PLLCLOCK_KHZ 24000
#define MIN_PLLCLOCK_HZ  (MIN_PLLCLOCK_KHZ * 1000)

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
    DFUNC(INFO, "Returning size: %ld KB\n", maxSize / 1024);
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
static ULONG computeKhzFromPllValue(const AT3DPLLValue_t *pllValue)
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

static ULONG computePLLValues(ULONG targetFreqKhz, AT3DPLLValue_t *pllValues)
{
    UWORD _m, _n, _r;
    int freq = svga_compute_pll(&at3d_pll, targetFreqKhz, &_m, &_n, &_r);
    if (freq == -1) {
        return 0;  // No valid combination found
    }
    // the AT manual defines M and N in the opposite way to how S3 did it
    pllValues->n = _m;
    pllValues->m = _n;
    pllValues->l = _r;

    ULONG fref = 14318;                                 // Reference frequency in kHz
    ULONG fvco = (pllValues->n * fref) / pllValues->m;  // VCO frequency in kHz

    // Compute frequency range F based on VCO frequency (from apm.c formula)
    // Formula: f = (c + 500 - 34*fvco/1000)/1000, where c = 1000*(380*7)/(380-175)
    int c = 1000 * (380 * 7) / (380 - 175);  // ≈ 12976
    int f = (c + 500 - 34 * fvco / 1000) / 1000;
    if (f > 7)
        f = 7;  // Clamp to 3-bit field (0-7)
    if (f < 0)
        f = 0;

    pllValues->f = f;

    return freq;
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

    AT3DPLLValue_t *pllValues = AllocVec(sizeof(AT3DPLLValue_t) * numEntries, MEMF_PUBLIC);
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

        BOOL doubleClocking = (freq <= MIN_PLLCLOCK_KHZ);
        if (doubleClocking) {
            freq *= 2;  // Use DCLK = VCLK/2 for low frequencies
        }

        AT3DPLLValue_t *entry = &cd->pllValues[cd->numPllValues];
        ULONG currentKhz      = computePLLValues(freq, entry);

        if (doubleClocking) {
            currentKhz /= 2;  // Return to original frequency
        }

        if (currentKhz > 0 && currentKhz != lastValue) {
            lastValue        = currentKhz;
            entry->freq10khz = (UWORD)((currentKhz + 5) / 10);  // store in 10 kHz units
            cd->numPllValues++;

            bi->PixelClockCount[CHUNKY]++;
            if (currentKhz <= maxHiColorFreq) {
                bi->PixelClockCount[HICOLOR]++;
                if (currentKhz <= maxTrueColorFreq) {
                    bi->PixelClockCount[TRUECOLOR]++;
                    bi->PixelClockCount[TRUEALPHA]++;
                }
            }

            DFUNC(CHATTY, "Pixelclock %03ld %09ldHz: n=%ld m=%ld l=%ld\n", (ULONG)cd->numPllValues - 1,
                  (ULONG)currentKhz * 1000, (ULONG)entry->n, (ULONG)entry->m, (ULONG)entry->l);
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

    AT3DPLLValue_t pllValues;
    ULONG actualFreqKhz = computePLLValues(clockHz / 1000, &pllValues);

    if (actualFreqKhz == 0) {
        DFUNC(ERROR, "Failed to compute MCLK PLL values for %ld Hz\n", clockHz);
        return clockHz;  // Return requested frequency as fallback
    }

    DFUNC(INFO, "MCLK: N=%ld, M=%ld, L=%ld, actual=%ld kHz\n", (ULONG)pllValues.n, (ULONG)pllValues.m,
          (ULONG)pllValues.l, actualFreqKhz);

    MMIOBASE();

    W_MMIO_B(MCLK_CTRL, CLK_POSTSCALER(3));

    // Write denominator (M) - mask to ensure only valid bits are set
    W_MMIO_MASK_B(MCLK_DEN, CLK_DEN_MASK, pllValues.m - 1);

    // Write numerator (N) - mask to ensure only valid bits are set
    W_MMIO_MASK_B(MCLK_NUM, CLK_NUM_MASK, pllValues.n - 1);

    // Wait for PLL to stabilize
    delayMilliSeconds(5);

    // Set postscaler in control register and enable MCLK programming
    W_MMIO_B(MCLK_CTRL, CLK_FREQ_RANGE(0b100) | CLK_POSTSCALER(pllValues.l) | CLK_HIGH_SPEED);

    if (clockHz > 50000000) {
        W_MMIO_MASK_W(DISP_MEM_CFG, FAST_RAS_DISABLE_MASK, FAST_RAS_DISABLE);
    }

    return actualFreqKhz * 1000;  // convert to hz
}

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

    const ChipData_t *cd = getConstChipData(bi);
    if (!cd->pllValues || cd->numPllValues == 0) {
        DFUNC(ERROR, "PLL table not initialized (pllValues=%lx numPllValues=%ld)\n", (ULONG)cd->pllValues,
              (ULONG)cd->numPllValues);
        return desiredPixelClock;
    }

    UWORD targetFreq10khz = (UWORD)(desiredPixelClock / 10000);  // target in 10 kHz units

    // Find the best matching PLL entry using binary search
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

    mi->PixelClock = (ULONG)lowerFreq * 10000;  // 10 kHz -> Hz

    mi->Flags &= ~GMF_DOUBLECLOCK;
    if ((ULONG)lowerFreq * 10 <= MIN_PLLCLOCK_KHZ) {
        // FIXME: I'm still not sure if GMF_DOUBLECLOCK is meant "we're running at twice the desired pixel frequency"
        //  or "RAMDAC double indexed" mode
        mi->Flags |= GMF_DOUBLECLOCK;
    }

    const AT3DPLLValue_t *pllValues = &cd->pllValues[lower];

    // Store PLL values in the format expected by SetClock
    // AT3D registers store N-1 and M-1 (the chip uses register_value + 1 in calculations)
    // pll1.Numerator = N-1 (register value, 7-126 for N=8-127)
    // pll2.Denominator = (L << 6) | (F << 3) | (M-1)
    //   Bits [6:7] = L (postscaler, 0-3)
    //   Bits [3:5] = F (frequency range, 0-7)
    //   Bits [0:2] = M-1 (denominator, 0-4)
    mi->pll1.Numerator   = pllValues->n - 1;
    mi->pll2.Denominator = (pllValues->l << 6) | (pllValues->f << 3) | (pllValues->m - 1);

    DFUNC(CHATTY, "Reporting pixelclock Hz: %ld, index: %ld,  N:%ld M:%ld L:%ld \n\n", mi->PixelClock, (ULONG)lower,
          (ULONG)pllValues->n, (ULONG)pllValues->m, (ULONG)pllValues->l);

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

    {
        REGBASE();
        if (bi->ModeInfo->Flags & GMF_DOUBLECLOCK) {
            DFUNC(INFO, "SetClock: Clocking halving enabled\n");
            W_SR_MASK(0x01, BIT(3), BIT(3));  // Enable DCLK = VCLK/2
        } else {
            W_SR_MASK(0x01, BIT(3), BIT(0));  // Enable DCLK = VCLK/2
        }
    }

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

    return (ULONG)cd->pllValues[index].freq10khz * 10000;  // 10 kHz -> Hz
}

static UWORD ASM CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                      __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE rgbFormat))
{
    if (mi) {
        // Bitmap supposed to show on screen.
        // We expect blits to and from on-screen subrectangles, so make the pitch Blitter-compatible
        // Use X/Y addressing for these
        if (width <= 320) {
            // We allow only small resolutions to have a non-Graphics Engine size.
            // These resolutions (notably 320xY) are often used in games and these games
            // assume a pitch of 320 bytes (not 640 which expansion to 640 would
            // require). Nevertheless, align to 8 bytes. We constrain all other
            // resolutions to Graphics Engine supported pitch.
            width = (width + 7) & ~7;
        } else if (width <= 512) {
            width = 512;
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
        } else {
            return 0;
        }
        // FIXME: extend getBPP for 24bit and YUV formats
        return (width * getBPP(rgbFormat) + 7) & ~7;
    } else {
        // Offscreen bitmaps can be stored in a tightly packed format to support "Linear Addressing"
        return width * getBPP(rgbFormat);
    }
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

    waitFifo(bi, 8);  // make sure FIFO is flushed
    while (TST_MMIO_L(EXT_DAC_STATUS, EXT_DAC_DRAWING_ENGINE_BUSY)) {
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
    if (cd->memFormat == format) {
        return;
    }
    cd->memFormat = format;

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

    UWORD hTotal      = mi->HorTotal;
    UWORD ScreenWidth = mi->Width;
    UBYTE modeFlags   = mi->Flags;
    BOOL isInterlaced = (modeFlags & GMF_INTERLACE) ? 1 : 0;
    UBYTE depth       = mi->Depth;

// 8 pixels default border size = 1 character clock
#define ADJUST_HBORDER(x) AdjustBorder(x, border, 8)
#define ADJUST_VBORDER(y) AdjustBorder(y, border, 1);
#define TO_CLKS(x)        ((x) >> 3)
#define TO_SCANLINES(y)   ToScanLines((y), modeFlags)

    REGBASE();

    // For some reason, the auto-reset-disable sometimes gets lost
    W_CR(CR_EXT_AUTORESET, CR_EXT_AUTORESET_DISABLE);

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
    // FIXME: currently GMF_DOUBLECLOCK is used to indicate halved pixel clock
    // if ((format == RGBFB_CLUT) && (bi->ModeInfo && (bi->ModeInfo->Flags & GMF_DOUBLECLOCK))) {
    //     regValue |= BIT(5);  // Enable double index
    // } else {
    //     regValue &= ~BIT(5);  // Disable double index
    // }

    // Write the register
    W_MMIO_B(SERIAL_CTRL, regValue);

    DFUNC(VERBOSE, "SetDAC: format=0x%lx, depth=0x%02lx, format=0x%02lx, reg=0x%02lx\n", (ULONG)format,
          (ULONG)pixelDepth, (ULONG)pixelFormat, (ULONG)regValue);
}

static void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    DFUNC(VERBOSE, "startIndex %ld, count %ld\n", (ULONG)startIndex, (ULONG)count);

    LOCAL_SYSBASE();

    // This may not be interrupted, so DAC_WR_AD remains set throughout the function
    Disable();

    REGBASE();
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

    if (startIndex == 0) {
        R_REG(0x3DA);  // Reset AFF
        // Background color 0 also sets the border color
        /* 3:3:2 RGB: R[7:5], G[4:2], B[1:0] */
        if (bi->ModeInfo->Depth <= 8) {
            W_AR(0x11, 0);
        } else {
            W_AR(0x11, (UBYTE)((bi->CLUT[0].Red & 0xE0) | ((bi->CLUT[0].Green >> 3) & 0x1C) | (bi->CLUT[0].Blue >> 6)));
        }
        W_REG(ATR_AD, 0x20);  // re-enable normal screen output
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

    // This has weird effects on the lines the cursor image shows
    // R_REG(0x3DA);  // Reset AFF to latch new start address
    // W_AR(0x13, xoffset & 7);  // Update border color to match new background color (in case it changed)

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

// FIXME: Make sure to coordinate with SetDPMSLevel, does the register signals still get produced?
static void WaitVerticalSync(__REGA0(struct BoardInfo *bi), __REGD0(BOOL waitForEnd))
{
    REGBASE();
    if (waitForEnd) {
        // wait for verticel retrace
        // Use readReg() so in debug mode slow serial output doesn't make us miss the signals
        while (!(readReg(RegBase, 0x3DA) & 0x08)) {
        };
        // For pixel display (should now be top of frame, i.e. end of retrace)
        while (!(readReg(RegBase, 0x3DA) & 0x01)) {
        };
    } else {  // For pixel display first
        while (!(readReg(RegBase, 0x3DA) & 0x01)) {
        };
        // wait for verticel retrace starting
        while (!(readReg(RegBase, 0x3DA) & 0x08)) {
        };
    }
}

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

/* Hardware cursor: 64x64 at 2 bpp, 16 bytes per row, stored at KB-aligned address.
 * Pattern base register is in kilobytes. Position/offset in pixels (12-bit X/Y, 6-bit offsets). */

static void ASM SetSpritePosition(__REGA0(struct BoardInfo *bi), __REGD0(WORD xpos), __REGD1(WORD ypos),
                                  __REGD7(RGBFTYPE fmt))
{
    (void)fmt;
    MMIOBASE();

    bi->MouseX = xpos;
    bi->MouseY = ypos;

    WORD spriteX = xpos - bi->XOffset;
    WORD spriteY = ypos - bi->YOffset + bi->YSplit;

    if (bi->ModeInfo->Flags & GMF_DOUBLESCAN) {
        spriteY *= 2;
    }

    WORD offsetX = 0;
    if (spriteX < 0) {
        offsetX = (spriteX > -64) ? -spriteX : 63;
        spriteX = 0;
    }
    WORD offsetY = 0;
    if (spriteY < 0) {
        offsetY = (spriteY > -64) ? -spriteY : 63;
        spriteY = 0;
    }

    W_MMIO_W(HW_CURSOR_X, spriteX & 0xFFF);
    W_MMIO_W(HW_CURSOR_Y, spriteY & 0xFFF);
    W_MMIO_B(HW_CURSOR_OFF_X, offsetX & 63);
    W_MMIO_B(HW_CURSOR_OFF_Y, offsetY & 63);
}

static void ASM SetSpriteImage(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE, "fmt=%ld\n", (ULONG)fmt);
    (void)fmt;
    /* AT3D: first 16 bytes = top row; 2 bpp, bit0=XOR bit1=AND per pixel; 16 bytes/row * 64 rows = 1024 bytes */
    const UWORD *image = bi->MouseImage + 2;
    ULONG *cursor      = (ULONG *)bi->MouseImageBuffer;
    for (UWORD y = 0; y < bi->MouseHeight; ++y) {
        // first 16 bit
        ULONG plane0 = *image++;
        ULONG plane1 = *image++;

        plane0 = ~plane0;

        *cursor++ = swapl((spreadBits(plane0) << 1) | spreadBits((plane1)));
        *cursor++ = 0xAAAAAAAA;  // encodes 0b10 per cursor pixel (transparent)
        *cursor++ = 0xAAAAAAAA;
        *cursor++ = 0xAAAAAAAA;
    }
    // Pad the rest of the cursor image
    for (UWORD y = bi->MouseHeight; y < 64; ++y) {
        for (UWORD p = 0; p < 4; ++p) {
            *cursor++ = 0xAAAAAAAA;
        }
    }

    LOCAL_SYSBASE();
    CacheClearU();
}

static void ASM SetSpriteColor(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE index), __REGD1(UBYTE red),
                               __REGD2(UBYTE green), __REGD3(UBYTE blue), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE, "index=%ld, R=%ld, G=%ld, B=%ld, fmt=%ld\n", (ULONG)index, (ULONG)red, (ULONG)green, (ULONG)blue,
          (ULONG)fmt);

    MMIOBASE();
    if (index > 2)
        return;
    // Luckily this bit of index rotation was enough to match the P96 sprite color indices to the AT3D ones
    LONG reg = HW_CURSOR_COL1 + (index + 1) % 3;

    switch (fmt) {
    case RGBFB_NONE:
    case RGBFB_CLUT: {
        UBYTE paletteEntry = (UBYTE)(17 + index);
        W_MMIO_B(reg, paletteEntry);
        // W_MMIO_MASK_B(HW_CURSOR_CTRL, BIT(2), 0x00);  // Disable "Full Color" mode
        break;
    }
    default:
        /* 3:3:2 RGB: R[7:5], G[4:2], B[1:0] */
        W_MMIO_B(reg, (UBYTE)((red & 0xE0) | ((green >> 3) & 0x1C) | (blue >> 6)));
        // W_MMIO_MASK_B(HW_CURSOR_CTRL, BIT(2), BIT(2));  // Enable "Full Color" mode
        break;
    }
}

static BOOL ASM SetSprite(__REGA0(struct BoardInfo *bi), __REGD0(BOOL activate), __REGD7(RGBFTYPE RGBFormat))
{
    DFUNC(VERBOSE, "activate=%ld, format=%ld\n", (ULONG)activate, (ULONG)RGBFormat);

    MMIOBASE();

    UBYTE cursorCtrl = BIT(1) | (activate ? BIT(0) : 0);  // 3-color + Enable

    W_MMIO_B(HW_CURSOR_CTRL, cursorCtrl);

    if (activate) {
        SetSpriteColor(bi, 0, bi->CLUT[17].Red, bi->CLUT[17].Green, bi->CLUT[17].Blue, bi->RGBFormat);
        SetSpriteColor(bi, 1, bi->CLUT[18].Red, bi->CLUT[18].Green, bi->CLUT[18].Blue, bi->RGBFormat);
        SetSpriteColor(bi, 2, bi->CLUT[19].Red, bi->CLUT[19].Green, bi->CLUT[19].Blue, bi->RGBFormat);
    }
    return TRUE;
}

static INLINE ULONG REGARGS getMemoryOffset(const struct BoardInfo *bi, APTR memory)
{
    ULONG offset = (ULONG)memory - (ULONG)bi->MemoryBase;
    offset &= ~0x800000;  // map addresses from (BE) linear window2 back to regular framebuffer address
    return offset;
}

static INLINE ULONG REGARGS getLinearPixelOffset(const struct BoardInfo *bi, const struct RenderInfo *ri, UWORD x,
                                                 UWORD y, UBYTE bppLog2)
{
    // Note: not suited for 3-bytes-per-pixel modes
    ULONG offset = getMemoryOffset(bi, ri->Memory);
    offset += y * ri->BytesPerRow;
    offset >>= bppLog2;  // convert line offset to units of pixels
    offset += x;         // final offset
    return offset;
}

static INLINE void getStartCoordinates(const struct BoardInfo *bi, const struct RenderInfo *ri, UBYTE bppLog2,
                                       UWORD *originX, UWORD *originY)
{
    // Memory offset is essentially pixel 0,0
    ULONG offset = getMemoryOffset(bi, ri->Memory);
    *originX     = (offset % ri->BytesPerRow) >> bppLog2;
    *originY     = (offset / ri->BytesPerRow);
}

static INLINE void setLocationRegister(const struct BoardInfo *bi, const struct RenderInfo *ri, UWORD x, UWORD y,
                                       UBYTE bppLog2, BOOL useLinearAddressing, UBYTE reg)
{
    ULONG location;
    if (useLinearAddressing) {
        ULONG pixelOffset = getLinearPixelOffset(bi, ri, x, y, bppLog2);
        DFUNC(VERBOSE, "linear pixel offset for (%ld,%ld): %ld (0x%lx)\n", (ULONG)x, (ULONG)y, pixelOffset,
              pixelOffset);
        location = makeDWORD(swapw(pixelOffset & 0xFFF), swapw(pixelOffset >> 12));
    } else {
        // Memory offset is essentially pixel 0,0
        ULONG offset = getMemoryOffset(bi, ri->Memory);
        UWORD originX, originY;
        getStartCoordinates(bi, ri, bppLog2, &originX, &originY);
        location = makeDWORD(swapw(x + originX), swapw(y + originY));
        DFUNC(VERBOSE, "base offset %ld (0x%lx), offX %ld, offY %ld, final location 0x%08lx\n", offset, offset,
              (ULONG)originX, (ULONG)originY, swapl(location));
    }
    MMIOBASE();
    W_MMIO_NOSWAP_L(reg, location);
}

static INLINE void setDstLocation(struct BoardInfo *bi, const struct RenderInfo *ri, UWORD x, UWORD y, UBYTE bppLog2,
                                  BOOL useLinearAddressing)
{
    setLocationRegister(bi, ri, x, y, bppLog2, useLinearAddressing, DST_LOCATION_X_LOW);
}

static INLINE void setSrcLocation(struct BoardInfo *bi, const struct RenderInfo *ri, UWORD x, UWORD y, UBYTE bppLog2,
                                  BOOL useLinearAddressing)
{
    setLocationRegister(bi, ri, x, y, bppLog2, useLinearAddressing, SRC_LOCATION_X_LOW);
}

static INLINE void setDstPitch(struct BoardInfo *bi, UWORD bytesPerRow)
{
    MMIOBASE();
    W_MMIO_W(DST_PITCH, bytesPerRow);
}

static INLINE void setDrawSize(struct BoardInfo *bi, UWORD width, UWORD height)
{
    MMIOBASE();
    W_MMIO_W(SRC_SIZE_Y, height);
    // write width last as it may start the drawing operation
    W_MMIO_W(SRC_SIZE_X, width);
}

ULONG REGARGS PenToColor(ULONG pen, RGBFTYPE fmt)
{
    switch (fmt) {
    case RGBFB_B8G8R8A8:
        pen = swapl(pen);
        // fallthrough
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
        pen = swapw(pen);
        // fallthrough
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        pen = copyToUpper(pen);
        break;
    case RGBFB_CLUT:
        pen = (pen << 8) | pen;
        pen = copyToUpper(pen);
    default:
        break;
    }
    return pen;
}

static INLINE void setForegroundPen(struct BoardInfo *bi, ULONG fgPen, RGBFTYPE fmt)
{
    ChipData_t *cd = getChipData(bi);
    if (cd->GEfgPen != fgPen) {
        cd->GEfgPen = fgPen;
        fgPen       = PenToColor(fgPen, fmt);
        MMIOBASE();
        W_MMIO_L(FRGD_COLOR, fgPen);
    }
}

static INLINE void setBackgroundPen(struct BoardInfo *bi, ULONG bgPen, RGBFTYPE fmt)
{
    ChipData_t *cd = getChipData(bi);
    if (cd->GEbgPen != bgPen) {
        cd->GEbgPen = bgPen;
        bgPen       = PenToColor(bgPen, fmt);
        MMIOBASE();
        W_MMIO_L(BKGD_COLOR, bgPen);
    }
}

static ULONG getAdressModelBits(struct RenderInfo *ri, UBYTE bppLog2)
{
    switch (ri->BytesPerRow >> bppLog2) {
    case 512:
        return DRAW_ADDRESS_MODEL(0b011);
    case 640:
        return DRAW_ADDRESS_MODEL(0b001);
    case 800:
        return DRAW_ADDRESS_MODEL(0b010);
    case 1024:
        return DRAW_ADDRESS_MODEL(0b100);
    case 1152:
        return DRAW_ADDRESS_MODEL(0b101);
    case 1280:
        return DRAW_ADDRESS_MODEL(0b110);
    case 1600:
        return DRAW_ADDRESS_MODEL(0b111);
    default:
        return 0;  // interpreted as "linear addressing mode"
    }
}

#define ROP_SOURCE              0xCC
#define ROP_PATTERN             0xF0
#define ROP_SRC_XOR_DST         0x66
#define ROP_SRC_AND_PAT_AND_DST 0x80
#define ROP_NOT_DST             0x55
#define ROP_JAM1                0xCA  // (P and S) or (not P and D): use S (fg) where P (pattern) is 1, else D
#define ROP_JAM2                ROP_SOURCE
#define ROP_COMPLEMENT          0x5A  // Flip destination where pattern is 1

/* BlitRectNoMaskComplete OpCode: P96 passes a 4-bit minterm only (B=source, C=destination); see
 * wiki.icomp.de P96_Driver_Development#BlitRectNoMaskComplete. Bits: 3=B∧C, 2=B∧¬C, 1=¬B∧C, 0=¬B∧¬C.
 * Copy source is 0x0C. Convert to ROP3 (P,S,D): replicate low nibble to high so result is independent
 * of Pattern and 0x0C -> 0xCC. No table needed. */
static INLINE UBYTE mintermToRop3(UBYTE minterm)
{
    UBYTE lo = minterm & 0x0FU;
    return (UBYTE)(lo | (lo << 4));
}

static void ASM FillRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                         __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(ULONG pen),
                         __REGD5(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    DFUNC(INFO,
          "\nx %ld, y %ld, w %ld, h %ld\npen %08lx, mask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)pen, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    // AT3D doesn't support bit masking for CLUT modes, so we require a full mask in that case.
    // True color modes can ignore the mask
    if (fmt <= RGBFB_CLUT && mask != 0xFF) {
        D(WARN, "FillRect fallback\n");
        bi->FillRectDefault(bi, ri, x, y, width, height, pen, mask, fmt);
        return;
    }

    MMIOBASE();

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != FILLRECT) {
        cd->GEOp          = FILLRECT;
        cd->GElinear      = 0x55;  // Force update of addressing mode and format
        cd->GEFormat      = ~0;
        cd->GEdrawCmd     = 0;
        cd->GEbytesPerRow = 0;
        W_MMIO_B(RASTEROP, ROP_SOURCE);
    }

    if (cd->GEFormat != fmt) {
        cd->GEbppLog2 = getBPPLog2(fmt);
    }

    UBYTE bppLog2 = cd->GEbppLog2;

    BOOL isLinear = ((width << bppLog2) == ri->BytesPerRow);

    if (isLinear != cd->GElinear || cd->GEbytesPerRow != ri->BytesPerRow || cd->GEFormat != fmt) {
        cd->GEbytesPerRow = ri->BytesPerRow;
        cd->GElinear      = isLinear;
        // Pixel depth:
        // 0b000 = determined by screen (6422 behavior)
        // 0bX01 = 8bpp
        // 0bX10 = 16bpp
        // 0bX11 = 32bpp
        // 0b100 = 24bpp
        UBYTE pixelDepth   = bppLog2 + 1;  // matches bit encoding for pixel depth, but doesn't cover 24 bits
        ULONG addressModel = isLinear ? (DRAW_DST_ADDR_LINEAR | DRAW_DST_CONTIGUOUS) : getAdressModelBits(ri, bppLog2);
        ULONG cmd = DRAW_CMD_OP(DRAW_CMD_RECT) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) | DRAW_PIXEL_DEPTH(pixelDepth) |
                    addressModel;

        if (cmd != cd->GEdrawCmd) {
            cd->GEdrawCmd = cmd;
            W_MMIO_L(DRAW_CMD, cmd);
        }

        setForegroundPen(bi, pen, fmt);
    } else {
        setForegroundPen(bi, pen, fmt);
    }

    setDstLocation(bi, ri, x, y, bppLog2, isLinear);

    // Kick off the fill by writing the size registers
    setDrawSize(bi, width, height);
}

static void ASM InvertRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                           __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                           __REGD7(RGBFTYPE fmt))
{
    DFUNC(INFO,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    if (!getBPP(fmt) || !width || !height) {
        bi->InvertRectDefault(bi, ri, x, y, width, height, mask, fmt);
        return;
    }
    if (fmt <= RGBFB_CLUT && mask != 0xFF) {
        D(WARN, "InvertRect fallback (mask)\n");
        bi->InvertRectDefault(bi, ri, x, y, width, height, mask, fmt);
        return;
    }

    MMIOBASE();

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != INVERTRECT) {
        cd->GEOp      = INVERTRECT;
        cd->GElinear  = 0x55;  // Force update of addressing mode and format
        cd->GEFormat  = ~0;
        cd->GEdrawCmd = 0;

        W_MMIO_B(RASTEROP, ROP_NOT_DST);
    }

    if (cd->GEFormat != fmt) {
        cd->GEbppLog2 = getBPPLog2(fmt);
    }

    UBYTE bppLog2 = cd->GEbppLog2;

    BOOL isLinear = ((width << bppLog2) == ri->BytesPerRow);
    D(INFO, "isLinear %ld\n", (ULONG)isLinear);

    if (isLinear != cd->GElinear || cd->GEbytesPerRow != ri->BytesPerRow || cd->GEFormat != fmt) {
        cd->GEbytesPerRow = ri->BytesPerRow;
        cd->GEFormat      = fmt;
        cd->GElinear      = isLinear;

        UBYTE pixelDepth   = bppLog2 + 1;
        ULONG addressModel = isLinear ? (DRAW_DST_ADDR_LINEAR | DRAW_DST_CONTIGUOUS) : getAdressModelBits(ri, bppLog2);
        ULONG cmd = DRAW_CMD_OP(DRAW_CMD_RECT) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) | DRAW_PIXEL_DEPTH(pixelDepth) |
                    addressModel;

        if (cmd != cd->GEdrawCmd) {
            cd->GEdrawCmd = cmd;
            W_MMIO_L(DRAW_CMD, cmd);
        }
    }

    setDstLocation(bi, ri, x, y, bppLog2, isLinear);

    setDrawSize(bi, width, height);
}

static void ASM BlitRectNoMaskComplete(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *sri),
                                       __REGA2(struct RenderInfo *dri), __REGD0(WORD srcX), __REGD1(WORD srcY),
                                       __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                                       __REGD5(WORD height), __REGD6(UBYTE opCode), __REGD7(RGBFTYPE format))
{
    DFUNC(INFO,
          "\nx1 %ld, y1 %ld, x2 %ld, y2 %ld, w %ld, h %ld\n"
          "minTerm 0x%lx fmt %ld\n"
          "sri->bytesPerRow %ld, sri->memory 0x%lx\n"
          "dri->bytesPerRow %ld, dri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)opCode, (ULONG)format,
          (ULONG)sri->BytesPerRow, (ULONG)sri->Memory, (ULONG)dri->BytesPerRow, (ULONG)dri->Memory);

    MMIOBASE();

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITRECTNOMASKCOMPLETE) {
        cd->GEOp      = BLITRECTNOMASKCOMPLETE;
        cd->GEdrawCmd = 0;
        cd->GEopCode  = 0;
        cd->GEFormat  = ~0;
    }

    if (opCode != cd->GEopCode) {
        cd->GEopCode = opCode;

        UBYTE rop3 = mintermToRop3(opCode);

        D(INFO, "minterm 0x%02lX ROP3 0x%02lX\n", (ULONG)opCode, (ULONG)rop3);

        W_MMIO_B(RASTEROP, rop3);
    }

    if (cd->GEFormat != format) {
        cd->GEFormat  = format;
        cd->GEbppLog2 = getBPPLog2(format);
    }

    UBYTE bppLog2 = cd->GEbppLog2;
    // FIXME: cache src and dst render info
    UWORD widthBytes = width << bppLog2;

    ULONG srcAddrModel = getAdressModelBits(sri, bppLog2);
    ULONG dstAddrModel = getAdressModelBits(dri, bppLog2);

    if (!srcAddrModel && !dstAddrModel) {
        D(WARN, "BlitRectNoMaskComplete fallback src and dst can't  both require linear addressing\n");
        bi->BlitRectNoMaskCompleteDefault(bi, sri, dri, srcX, srcY, dstX, dstY, width, height, opCode, format);
        return;
    }

    BOOL srcCanLinear = (widthBytes == sri->BytesPerRow);
    BOOL dstCanLinear = (widthBytes == dri->BytesPerRow);

    // Either one of src and dst could be rectangle, the other one linear
    if ((!srcAddrModel && !srcCanLinear) || (!dstAddrModel && !dstCanLinear)) {
        D(WARN, "BlitRectNoMaskComplete Fallback. src or dst needs linear but blitsize prevents it\n");
        bi->BlitRectNoMaskCompleteDefault(bi, sri, dri, srcX, srcY, dstX, dstY, width, height, opCode, format);
        return;
    }

    BOOL dstLinear  = FALSE;
    BOOL srcLinear  = FALSE;
    ULONG addrModel = srcAddrModel;
    if (srcAddrModel != dstAddrModel) {
        if (dstCanLinear || !dstAddrModel) {
            dstLinear = TRUE;
        } else if (srcCanLinear || !srcAddrModel) {
            srcLinear = TRUE;
            addrModel = dstAddrModel;
        } else {
            D(WARN, "BlitRectNoMaskComplete fallback src and dst are subrects of different pitch\n");
            bi->BlitRectNoMaskCompleteDefault(bi, sri, dri, srcX, srcY, dstX, dstY, width, height, opCode, format);
            return;
        }
    }
    D(INFO, "isSrcLinear %ld, isDstLinear %ld\n", (ULONG)srcLinear, (ULONG)dstLinear);

    // W_MMIO_B(SRC_PITCH, sri->BytesPerRow);
    // W_MMIO_B(DST_PITCH, dri->BytesPerRow);

    ULONG drawCmd = DRAW_CMD_OP(DRAW_CMD_BLT) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) | DRAW_PIXEL_DEPTH(bppLog2 + 1);
    drawCmd |= addrModel;

    if (srcLinear) {
        drawCmd |= DRAW_SRC_ADDR_LINEAR | DRAW_SRC_CONTIGUOUS;
    } else if (dstLinear) {
        drawCmd |= DRAW_DST_ADDR_LINEAR | DRAW_DST_CONTIGUOUS;
    } else {
        if (dstX > srcX) {
            drawCmd |= DRAW_DIR_X_NEGATIVE;
            srcX = srcX + width - 1;
            dstX = dstX + width - 1;
        }
        if (dstY > srcY) {
            drawCmd |= DRAW_DIR_Y_NEGATIVE;
            srcY = srcY + height - 1;
            dstY = dstY + height - 1;
        }
    }
    if (drawCmd != cd->GEdrawCmd) {
        cd->GEdrawCmd = drawCmd;
        D(VERBOSE, "drawCmd 0x%08lx\n", drawCmd);

        W_MMIO_L(DRAW_CMD, drawCmd);
    }
    setSrcLocation(bi, sri, srcX, srcY, bppLog2, srcLinear);
    setDstLocation(bi, dri, dstX, dstY, bppLog2, dstLinear);
    setDrawSize(bi, width, height);
}

static void ASM BlitRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *sri), __REGD0(WORD srcX),
                         __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                         __REGD5(WORD height), __REGD6(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    DFUNC(INFO,
          "\nx1 %ld, y1 %ld, x2 %ld, y2 %ld, w %ld, \n"
          "h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt,
          (ULONG)sri->BytesPerRow, (ULONG)sri->Memory);

    MMIOBASE();

    if (mask != 0xFF) {
        D(WARN, "BlitRect fallback (mask != 0xFF)\n");
        bi->BlitRectDefault(bi, sri, srcX, srcY, dstX, dstY, width, height, mask, fmt);
        return;
    };

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITRECT) {
        cd->GEOp      = BLITRECT;
        cd->GEdrawCmd = 0;
        cd->GEopCode  = 0;
        cd->GEFormat  = ~0;

        W_MMIO_B(RASTEROP, ROP_SOURCE);
    }

    if (cd->GEFormat != fmt) {
        cd->GEFormat  = fmt;
        cd->GEbppLog2 = getBPPLog2(fmt);
    }

    UBYTE bppLog2 = cd->GEbppLog2;
    // FIXME: cache src and dst render info
    UWORD widthBytes = width << bppLog2;

    ULONG addrModel = getAdressModelBits(sri, bppLog2);
    BOOL isLinear   = (widthBytes == sri->BytesPerRow);

    if (!addrModel && !isLinear) {
        D(WARN, "BlitRectNoMaskComplete Fallback. src needs linear but blitsize prevents it\n");
        bi->BlitRectDefault(bi, sri, srcX, srcY, dstX, dstY, width, height, mask, fmt);
        return;
    }

    ULONG drawCmd = DRAW_CMD_OP(DRAW_CMD_BLT) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) | DRAW_PIXEL_DEPTH(bppLog2 + 1);
    drawCmd |= addrModel;

    if (!addrModel) {
        drawCmd |= DRAW_SRC_ADDR_LINEAR | DRAW_SRC_CONTIGUOUS;
    } else {
        if (dstX > srcX) {
            drawCmd |= DRAW_DIR_X_NEGATIVE;
            srcX = srcX + width - 1;
            dstX = dstX + width - 1;
        }
        if (dstY > srcY) {
            drawCmd |= DRAW_DIR_Y_NEGATIVE;
            srcY = srcY + height - 1;
            dstY = dstY + height - 1;
        }
    }
    if (drawCmd != cd->GEdrawCmd) {
        cd->GEdrawCmd = drawCmd;
        D(VERBOSE, "drawCmd 0x%08lx\n", drawCmd);

        W_MMIO_L(DRAW_CMD, drawCmd);
    }
    // FIXME: this can be optimized into a single function
    setSrcLocation(bi, sri, srcX, srcY, bppLog2, isLinear);
    setDstLocation(bi, sri, dstX, dstY, bppLog2, isLinear);
    setDrawSize(bi, width, height);
}

// Host BLT port in flat memory (last 32K). Poll EXT_DAC_HOST_BLT_IN_PROGRESS until high before writing.
static INLINE volatile ULONG *getHostBltPort(struct BoardInfo *bi)
{
    return (volatile ULONG *)((UBYTE *)bi->MemoryBase + HOST_BLT_OFFSET);
}

static BOOL setDrawMode(BoardInfo_t *bi, UBYTE drawMode, ULONG fgPen, ULONG bgPen, RGBFTYPE fmt)
{
    UBYTE rop = 0;
    switch (drawMode & (JAM1 | JAM2 | COMPLEMENT)) {
    case JAM1:
        rop = ROP_JAM2;  // ROP_JAM1;
        break;
    case JAM2:
        rop = ROP_JAM2;
        break;
    case COMPLEMENT:
    // case COMPLEMENT | JAM1:
    // fallthrough
    case COMPLEMENT | JAM2:
        rop = ROP_NOT_DST;  // ROP_NOT_DST; //ROP_SRC_XOR_DST; //ROP_COMPLEMENT;
        break;
    default:
        rop = ROP_SOURCE;
        break;
    }
    setForegroundPen(bi, fgPen, fmt);
    setBackgroundPen(bi, bgPen, fmt);

    {
        MMIOBASE();
        // Documentation says that PCI burst writes break writing to ROP right after DRAW_CMD.
        // Thus, make sure there's enough other writes between writing to ROP and DRAW_CMD
        // Thus, make sure there's enough other writes between writing to ROP and DRAW_CMD
        // W_MMIO_B(RASTEROP, 0);
        W_MMIO_B(RASTEROP, rop);
    }
}

static void ASM BlitTemplate(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                             __REGA2(struct Template *template), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                             __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    DFUNC(INFO, "x %ld, y %ld, w %ld, h %ld mask 0x%02lx fmt %ld\n", (LONG)x, (LONG)y, (LONG)width, (LONG)height,
          (ULONG)mask, (ULONG)fmt);

    if (fmt <= RGBFB_CLUT && mask != 0xFF) {
        D(WARN, "BlitTemplate fallback (CLUT and mask)\n");
        bi->BlitTemplateDefault(bi, ri, template, x, y, width, height, mask, fmt);
        return;
    }

    MMIOBASE();
    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITTEMPLATE) {
        cd->GEOp      = BLITTEMPLATE;
        cd->GEdrawCmd = 0;
        cd->GEFormat  = ~0;

        //    setDstPitch(bi, ri->BytesPerRow);
        /* 11.7.6: Source Location X must be 0 for mono-to-color. Monochrome source must be 64-bit aligned. */
        W_MMIO_W(SRC_LOCATION_X_LOW, 0);
    }

    setDrawMode(bi, template->DrawMode, template->FgPen, template->BgPen, fmt);


    UBYTE bppLog2      = getBPPLog2(fmt);
    UWORD widthBytes   = width << bppLog2;
    BOOL isLinear      = (widthBytes == ri->BytesPerRow);
    ULONG addressModel = isLinear ? (DRAW_DST_ADDR_LINEAR | DRAW_DST_CONTIGUOUS) : getAdressModelBits(ri, bppLog2);
    ULONG drawCmd      = DRAW_CMD_OP(DRAW_CMD_HOST_BLT_WRITE) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) |
                    DRAW_SRC_MONOCHROME | DRAW_SRC_ADDR_LINEAR | DRAW_SRC_CONTIGUOUS | DRAW_PIXEL_DEPTH(bppLog2 + 1) |
                    addressModel;

    if (!(template->DrawMode & JAM2)) {
        drawCmd |= DRAW_SRC_TRANSPARENT;
    }

    // if (drawCmd != cd->GEdrawCmd)
    {
        cd->GEdrawCmd = drawCmd;
        W_MMIO_L(DRAW_CMD, drawCmd);
    }

    setDstLocation(bi, ri, (UWORD)x, (UWORD)y, bppLog2, isLinear);

    ULONG invert = (template->DrawMode & INVERSVID) ? ~0 : 0;

    /* 11.7.6: Host BLT mono data is a byte stream: width rounded up to next 8 bits (bytesPerLine bytes per row).
     * Bytes are packed into 32-bit words; words are written at 8-byte offsets (0, 8, 16, ...).
     * So one 32-bit word can span a row boundary (e.g. last bytes of line 0 + first bytes of line 1). */
    UWORD byteWidth = (width + 7) / 8;

    volatile ULONG *hostBlt = getHostBltPort(bi);

    BOOL srcLinear = (byteWidth == template->BytesPerRow);
    if (srcLinear) {
        setDrawSize(bi, width, height);

        // Template data is already tightly 8-bit packed
        ASSERT(template->XOffset == 0);  // if the width is the same as the pitch, we can't really have an X offset
        D(INFO, "Template data is already in suitable format for direct host BLT\n");
        const ULONG *src = (const ULONG *)template->Memory;

        while (!TST_MMIO_L(EXT_DAC_STATUS, EXT_DAC_HOST_BLT_IN_PROGRESS)) {
        };

        for (ULONG i = 0; i < ((width * height + 31) / 32); i++) {
            ULONG pattern = src[i];
            hostBlt[i]    = pattern ^ invert;
        }
    } else {
        // // more generic functions
        // const UBYTE *bitmap = (const UBYTE *)template->Memory;
        // UWORD bitmapPitch   = (UWORD) template->BytesPerRow;

        // UBYTE rol = (UBYTE) template->XOffset;
        // if (template->XOffset >= 8) {
        //     bitmap++;
        //     rol -= 8;
        // }

        // ULONG dwords          = ((byteWidth * height) + 3) / 4;
        // UWORD currentLineByte = 0;

        // D(INFO, "byteWidth %ld, bitmapPitch %ld, rol %ld, dwords %ld\n", (ULONG)byteWidth, (ULONG)bitmapPitch,
        //   (ULONG)rol, dwords);
        // while (!TST_MMIO_L(EXT_DAC_STATUS, EXT_DAC_HOST_BLT_IN_PROGRESS)) {
        // };
        // for (ULONG k = 0; k < dwords; k++) {
        //     ULONG w = 0;
        //     for (unsigned j = 0; j < 4; j++) {
        //         UWORD t = *(UWORD *)(bitmap + currentLineByte);
        //         t <<= rol;
        //         t = swapw(t);
        //         w <<= 8;
        //         w = moveb(t, w);
        //         // w |= t & 0xFF;

        //         ++currentLineByte;
        //         if (currentLineByte >= byteWidth) {
        //             currentLineByte = 0;
        //             bitmap += bitmapPitch;
        //         }
        //     }
        //     *hostBlt = w;
        // }

        const UBYTE *bitmap = (const UBYTE *)template->Memory;
        UWORD bitmapPitch   = (UWORD) template->BytesPerRow;
        UWORD originX, originY;
        getStartCoordinates(bi, ri, bppLog2, &originX, &originY);
        W_MMIO_W(CLIP_RIGHT, originX + x + width - 1);
        width = (width + 31) & ~31;

        setDrawSize(bi, width, height);
        UWORD dwordsPerLine = (width + 31) / 32;
        UBYTE rol           = template->XOffset;
        while (!TST_MMIO_L(EXT_DAC_STATUS, EXT_DAC_HOST_BLT_IN_PROGRESS)) {
        };
        if (!rol) {
            for (UWORD y = 0; y < height; ++y) {
                for (UWORD x = 0; x < dwordsPerLine; ++x) {
                    *hostBlt = invert ^ (((const ULONG *)bitmap)[x]);
                }
                bitmap += bitmapPitch;
            }
        } else {
            for (UWORD y = 0; y < height; ++y) {
                for (UWORD x = 0; x < dwordsPerLine; ++x) {
                    ULONG left  = ((const ULONG *)bitmap)[x] << rol;
                    ULONG right = ((const ULONG *)bitmap)[x + 1] >> (32 - rol);

                    *hostBlt = invert ^ (left | right);
                }
                bitmap += bitmapPitch;
            }
        }
    }
    int count = 100;

    while (TST_MMIO_L(EXT_DAC_STATUS, EXT_DAC_HOST_BLT_IN_PROGRESS) && --count) {
        *hostBlt = 0xFF00AACC;
    };

    /* 11.7.6: write to M040 (DRAW_CMD) with arbitrary data to complete the write to last line(s). */
    // W_MMIO_L(DRAW_CMD, DRAW_CMD_OP(DRAW_CMD_NOP));

    if (count < 100) {
        if (!count)
            W_MMIO_B(0x1FF, 0x00);  // Byte counting gone wrong, abort host write blit
        D(WARN, "Host BLT completion wait loop iterated %d times\n", 100 - count);
    }

    W_MMIO_W(CLIP_RIGHT, 0xFFF);
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

        // Remap Control
        // Map HOST BLT port to last 32K of flat space less final 2K
        // Map ProMotion registers to  last 2K of flat space.
        W_SR_MASK(0x1b, 0x3F, (0b100 << 3) | (0b100));

        // Flat Model Control
        // Enable flat memory access, set aperture to 4MB and disable VGA memory (A000:0–BFFF:F) access
        W_SR_MASK(0x1c, 0x3F, 0b00111101);
        R_SR(0x1c);  // Dummy read to force flush of FIFO( is this the right way?)
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
        UBYTE extSigTiming = R_MMIO_B(EXTSIG_TIMING) & 0xC0;
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
        W_CR(CR_EXT_AUTORESET, 0x00);
        W_CR(0x00, 0x00);
        W_CR(CR_EXT_AUTORESET, CR_EXT_AUTORESET_DISABLE);
        // FIXME: something is off with this register. The readback sometimes returns 0x01, sometimes 0xff
        // and sometimes it seems to get reset to 0 later.
        while ((R_CR(CR_EXT_AUTORESET) & CR_EXT_AUTORESET_DISABLE) != CR_EXT_AUTORESET_DISABLE) {
            W_CR(CR_EXT_AUTORESET, CR_EXT_AUTORESET_DISABLE);
        }
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
    W_MMIO_B(COLOR_CORRECTION, BIT(4));  // 8bit per gun palette write (Does not seem to work?!)
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

    // Force 8 Dot, force Graphics mode, force VCLK PLL, disable VGA IO
    W_MMIO_W(VGA_OVERRIDE, BIT(5) | BIT(6) | BIT(7) | BIT(9) | BIT(12));

    // Enable extended VGA Modes
    W_MMIO_B(SERIAL_CTRL, BIT(6) | DESKTOP_DEPTH_8BPP | DESKTOP_FORMAT_INDEXED);  //

    W_MMIO_MASK_B(APERTURE_CTRL, PALETTE_ACCESS_MASK, PALETTE_ACCESS(0b01));  // Disable RAMDAC snooping
    W_MMIO_B(DPMS_SYNC_CTRL, 0x00);  // Clear bits [1:0] to enable both HSYNC and VSYNC
    W_MMIO_B(PIXEL_FIFO_REQ_POINT, 0x16);
    W_MMIO_B(PIXEL_FIFO_REQ_POINT + 1, 0x16);
    W_MMIO_B(PIXEL_FIFO_REQ_POINT + 2, 0x16);

    // W_MMIO_B(0xDA, 0x00);  // This used to be an "Internal Register" on older 6210 cards(?)

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

        W_AR(0x10, 0x61);  // 256color mode, separate pixel panning, graphics mode

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

    R_CR(CR_EXT_AUTORESET);

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

    R_CR(CR_EXT_AUTORESET);

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
    bi->Flags =
        bi->Flags | BIF_GRANTDIRECTACCESS | BIF_VGASCREENSPLIT | BIF_HASSPRITEBUFFER | BIF_HARDWARESPRITE | BIF_BLITTER;

    // AT3D supports CLUT (8-bit palette), hicolor (15/16-bit), and truecolor (24/32-bit)
    // Per AT3D specifications: "Optimized 24- and 32-bit truecolor", "hi-color, and 256-color GUI"
    bi->RGBFormats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;

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

    bi->SetSprite         = SetSprite;
    bi->SetSpritePosition = SetSpritePosition;
    bi->SetSpriteImage    = SetSpriteImage;
    bi->SetSpriteColor    = SetSpriteColor;

    bi->WaitBlitter            = WaitBlitter;
    bi->FillRect               = FillRect;
    bi->InvertRect             = InvertRect;
    bi->BlitRectNoMaskComplete = BlitRectNoMaskComplete;
    bi->BlitRect               = BlitRect;
    bi->BlitTemplate           = BlitTemplate;


    cd->GEfgPen = 0x12345678;
    cd->GEbgPen = 0x12345678;

    SetSplitPosition(bi, 0);

    /* Hardware cursor: take cursor image data off the top of the memory; cursor at 1 KB segment boundary */
    {
        const ULONG maxCursorBufferSize = (64 * 64 * 2 / 8); /* 64x64 at 2 bpp = 1024 bytes */
        MMIOBASE();

        bi->MemorySize       = (bi->MemorySize - maxCursorBufferSize) & ~(1024 - 1);
        bi->MouseImageBuffer = bi->MemoryBase + bi->MemorySize;

        // DFUNC(INFO, "Cursor offset %ld\n", bi->MemorySize);

        W_MMIO_B(HW_CURSOR_CTRL, 0);
        W_MMIO_W(HW_CURSOR_BASE, (UWORD)(bi->MemorySize >> 10));
        W_MMIO_W(HW_CURSOR_X, 0);
        W_MMIO_W(HW_CURSOR_Y, 0);
        W_MMIO_B(HW_CURSOR_OFF_X, 0);
        W_MMIO_B(HW_CURSOR_OFF_Y, 0);
    }

    W_MMIO_B(BYTE_MASK, 0xFF);

    W_MMIO_W(CLIP_CTRL, BIT(0));
    W_MMIO_W(CLIP_LEFT, 0);
    W_MMIO_W(CLIP_TOP, 0);
    W_MMIO_W(CLIP_RIGHT, 0xFFF);
    W_MMIO_W(CLIP_BOTTOM, 0xFFF);

    SetMemoryClock(bi, 50000000);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef TESTEXE

#include <hardware/blit.h>
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
        /* Two colors for BlitTemplate DrawMode tests: index 0 = blue, index 1 = red */
        bi->CLUT[0].Red   = 0;
        bi->CLUT[0].Green = 0;
        bi->CLUT[0].Blue  = 255;
        bi->CLUT[1].Red   = 255;
        bi->CLUT[1].Green = 0;
        bi->CLUT[1].Blue  = 0;
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

        struct RenderInfo ri;
        ri.Memory      = bi->MemoryBase;
        ri.BytesPerRow = 640;
        ri.RGBFormat   = RGBFB_CLUT;

        {
            FillRect(bi, &ri, 100, 100, 640 - 200, 480 - 200, 0xFF, 0xFF, RGBFB_CLUT);
        }

        {
            InvertRect(bi, &ri, 0, 20, 640, 40, 0xFF, RGBFB_CLUT);
        }

        {
            FillRect(bi, &ri, 64, 64, 128, 128, 0xAA, 0xFF, RGBFB_CLUT);
        }

        {
            FillRect(bi, &ri, 256, 10, 128, 128, 0x33, 0xFF, RGBFB_CLUT);
        }

        {
            InvertRect(bi, &ri, 100, 100, 640 - 200, 480 - 200, 0xFF, RGBFB_CLUT);
        }

        /* BlitRectNoMaskComplete: fill a source rect, then copy it to another position (same buffer). */
        {
            FillRect(bi, &ri, 50, 200, 80, 80, 0x55, 0xFF, RGBFB_CLUT);                       /* source rect */
            BlitRectNoMaskComplete(bi, &ri, &ri, 50, 200, 300, 200, 80, 80, 0xc, RGBFB_CLUT); /* copy: minterm 0xC0 */
        }

        /* BlitRectNoMaskComplete with overlapping source and destination (copy 120x40 from 50,350 to 100,350). */
        {
            FillRect(bi, &ri, 50, 350, 120, 40, 0x77, 0xFF, RGBFB_CLUT);                       /* source rect */
            BlitRectNoMaskComplete(bi, &ri, &ri, 50, 350, 100, 350, 120, 40, 0xc, RGBFB_CLUT); /* overlapping copy */
        }

        BlitRectNoMaskComplete(bi, &ri, &ri, 0, 0, 0, 240, 640, 100, 0xc, RGBFB_CLUT);
        // clang-format off
        /* BlitTemplate: 32x32 pattern = circle (center 15.5, radius 12) in 0b notation. MSB = left. */
        static ULONG template32[32] = {
            (ULONG)0b00000000000000100000000000000000,
            (ULONG)0b00000000000000100000000000000000,
            (ULONG)0b00000000000000100000000000000000,
            (ULONG)0b00000000000000100000000000000000,
            (ULONG)0b00000000001111011000000000000000,
            (ULONG)0b00000011111111111110000000000000,
            (ULONG)0b00001111111111111111110000000000,
            (ULONG)0b00011111111111111111111100000000,
            (ULONG)0b00111111111111111111111111000000,
            (ULONG)0b01111110000111111100001111110000,
            (ULONG)0b01111110000111111100001111110000,
            (ULONG)0b00011111111111111111111111100000,
            (ULONG)0b00001111111111111111111111100000,
            (ULONG)0b00000111111111111111111111100000,
            (ULONG)0b00000111111111111111111111100000,
            (ULONG)0b00000111111111111111111111100000,
            (ULONG)0b00000111110011111111100111100000,
            (ULONG)0b00000111111100000000011111100000,
            (ULONG)0b00000111111111111111111111100000,
            (ULONG)0b00000111111111111111111111100000,
            (ULONG)0b00111111111111111111111111000000,
            (ULONG)0b00111111111111111111111111000000,
            (ULONG)0b00000001111111111111111110000000,
            (ULONG)0b00000000111111111111111100000000,
            (ULONG)0b00000011111111111100000000000000,
            (ULONG)0b10101010101010101010101010101010,
            (ULONG)0b01010101010101010101010101010101,
            (ULONG)0b11111111111111111111111111111111,
            (ULONG)0b00000000000000000000000000000000,
            (ULONG)0b11001100110011001100110011001100,
            (ULONG)0b00110011001100110011001100110011,
        };
// clang-format on
/* FgPen = 1 (red), BgPen = 0 (blue) for all DrawMode tests. */
#define BLIT_TMPL_PEN_FG 1
#define BLIT_TMPL_PEN_BG 0

        for (int i = 7; i < 16; i++) {
            WORD y = i * 33;
            /* BlitTemplate JAM1: pattern 1 -> FgPen (red), pattern 0 -> keep D (blue). */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = 0;
                tmpl.DrawMode    = JAM2;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_BG;
                FillRect(bi, &ri, i * 32 + i, 0, 32, 32, 50, 0xFF, RGBFB_CLUT);
                BlitTemplate(bi, &ri, &tmpl, i * 32 + i, 0, 32, 32, 0xFF, RGBFB_CLUT);
            }
        }
#if 1
        for (int i = 0; i < 4; i++) {
            WORD y    = 200 + i * 40;
            WORD xoff = 5;

            /* BlitTemplate JAM1: pattern 1 -> FgPen (red), pattern 0 -> keep D (blue). */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = i * 2;
                tmpl.DrawMode    = JAM1;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_BG;
                FillRect(bi, &ri, 32 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                BlitTemplate(bi, &ri, &tmpl, 32 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
            }
            /* BlitTemplate JAM2: pattern 1 -> FgPen (red), pattern 0 -> BgPen (blue). */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = i * 2;
                tmpl.DrawMode    = JAM2;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_BG;
                FillRect(bi, &ri, 96 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                BlitTemplate(bi, &ri, &tmpl, 96 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
            }
            /* BlitTemplate COMPLEMENT: flip destination where pattern is 1 (blue<->red on checker). */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = i * 2;
                tmpl.DrawMode    = COMPLEMENT;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_BG;
                FillRect(bi, &ri, 144 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                BlitTemplate(bi, &ri, &tmpl, 144 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
            }
            /* BlitTemplate JAM1 | INVERSVID: pens swapped -> pattern 1 -> BgPen (blue), 0 -> D. */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = i * 2;
                tmpl.DrawMode    = JAM1 | INVERSVID;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_BG;
                FillRect(bi, &ri, 192 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                BlitTemplate(bi, &ri, &tmpl, 192 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
            }
            /* BlitTemplate JAM2 | INVERSVID: pattern 1 -> BgPen (blue), pattern 0 -> FgPen (red). */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = i * 2;
                tmpl.DrawMode    = JAM2 | INVERSVID;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_BG;
                FillRect(bi, &ri, 240 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                BlitTemplate(bi, &ri, &tmpl, 240 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
            }
            /* BlitTemplate COMPLEMENT: flip destination where pattern is 1 (blue<->red on checker). */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = i * 2;
                tmpl.DrawMode    = COMPLEMENT | INVERSVID;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_BG;
                FillRect(bi, &ri, 288 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                BlitTemplate(bi, &ri, &tmpl, 288 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
            }
        }

#undef BLIT_TMPL_PEN_FG
#undef BLIT_TMPL_PEN_BG

        /* BlitTemplate: original small frame (JAM2) at (422,262) for regression. */
        {
            static ULONG templateFrame[32];
            for (int row = 0; row < 32; row++) {
                if (row == 0 || row == 31)
                    templateFrame[row] = 0xFFFFFFFFUL;
                else
                    templateFrame[row] = 0x80000001UL;
            }
            struct Template tmpl;
            tmpl.Memory      = templateFrame;
            tmpl.BytesPerRow = 4;
            tmpl.XOffset     = 0;
            tmpl.DrawMode    = JAM2;
            tmpl.FgPen       = 0x00;
            tmpl.BgPen       = 0xFF;
            FillRect(bi, &ri, 422, 262, 32, 32, 0xFF, 0xFF, RGBFB_CLUT);
            BlitTemplate(bi, &ri, &tmpl, 422, 262, 32, 32, 0xFF, RGBFB_CLUT);
        }

        /* BlitTemplate 46x11: exercise non-32 width (byteWidth 6, totalBytes 66, dwords 18). */
        {
            static UBYTE template46x11[11][8]; /* 6 bytes per row used, 8 for alignment */
            for (int row = 0; row < 11; row++) {
                if (row == 0 || row == 10) {
                    template46x11[row][0] = 0xFF;
                    template46x11[row][1] = 0xFF;
                    template46x11[row][2] = 0xFF;
                    template46x11[row][3] = 0xFF;
                    template46x11[row][4] = 0xFF;
                    template46x11[row][5] = 0x3F; /* 46 bits: low 6 bits of byte 5 */
                } else {
                    template46x11[row][0] = 0x01;
                    template46x11[row][1] = 0x00;
                    template46x11[row][2] = 0x00;
                    template46x11[row][3] = 0x00;
                    template46x11[row][4] = 0x00;
                    template46x11[row][5] = 0x20; /* bit 45 = last column */
                }
            }
            struct Template tmpl46;
            tmpl46.Memory      = template46x11;
            tmpl46.BytesPerRow = 6;
            tmpl46.XOffset     = 0;
            tmpl46.DrawMode    = JAM2;
            tmpl46.FgPen       = 0x00;
            tmpl46.BgPen       = 0xFF;
            FillRect(bi, &ri, 94, 57, 46, 11, 0xFF, 0xFF, RGBFB_CLUT);
            BlitTemplate(bi, &ri, &tmpl46, 94, 57, 46, 11, 0xFF, RGBFB_CLUT);
        }
#endif

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
