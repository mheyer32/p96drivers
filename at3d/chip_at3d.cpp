#include "chip_at3d.h"
#include "at3d_i2c.h"
#include "edid_common.h"

#define __NOLIBBASE__

#include <clib/debug_protos.h>
#include <debuglib.h>
#include <exec/types.h>
#include <graphics/rastport.h>

#include <exec/interrupts.h>
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/exec.h>
#include <proto/openpci.h>

#include <SDI_stdarg.h>

#if defined(DBG) && !defined(AT3D_EMBEDDED_CHIP)
#if defined(TESTEXE) && !defined(AT3D_EMBEDDED_CHIP)
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

#if !defined(TESTEXE) && !defined(AT3D_EMBEDDED_CHIP)
extern const char LibName[]     = "AT3D.chip";
extern const char LibIdString[] = "Alliance ProMotion AT3D Picasso96 chip driver version 1.0";

#ifndef LIB_VERSION
#define LIB_VERSION 1
#endif
#ifndef LIB_REVISION
#define LIB_REVISION 0
#endif
extern const UWORD LibVersion  = LIB_VERSION;
extern const UWORD LibRevision = LIB_REVISION;
#endif

#define MIN_PLLCLOCK_KHZ 24000
#define MIN_PLLCLOCK_HZ  (MIN_PLLCLOCK_KHZ * 1000)

using namespace AT3DMmioReg;

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

    VgaIo vga = asAt3d(bi)->vga();

    // Test scratch pad register in sequencer registers (0x20-0x27)
    // Sequencer registers are accessed via SEQX (0x3C4) index and SEQ_DATA (0x3C5) data
    // Read current value from scratch pad register
    UBYTE original = vga.readSR(SR_SCRATCH_PAD);

    // Write test pattern to scratch pad register
    UBYTE testPattern = 0xAA;
    vga.writeSR(SR_SCRATCH_PAD, testPattern);

    // Read back from scratch pad register
    UBYTE readback = vga.readSR(SR_SCRATCH_PAD);

    // Restore original value
    vga.writeSR(SR_SCRATCH_PAD, original);

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

    At3dMmio mmio = asAt3d(bi)->mmio();

    // MMVGA window maps PCI configuration space
    // Read device ID from MMVGA window at DEVICE_ID (memory offset 182-183h per AT3D documentation)
    UWORD deviceId = mmio.readW(DEVICE_ID);

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
    DFUNC(VERBOSE, "\n");

    LOCAL_SYSBASE();

    ChipData_t *cd = getChipData(bi);

    // AT3D supports up to ~135MHz pixel clock; 6422 limited to 135MHz
    UWORD maxFreq    = (cd->chipFamily < AT24) ? 135 : 175;
    UWORD minFreq    = 12;  // 12MHz min
    UWORD numEntries = (maxFreq - minFreq + 1) * 2;

    AT3DPLLValue_t *pllValues = (AT3DPLLValue_t *)AllocVec(sizeof(AT3DPLLValue_t) * numEntries, MEMF_PUBLIC);
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

ULONG setMemoryClock(struct BoardInfo *bi, ULONG clockHz)
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

    At3dMmio mmio = asAt3d(bi)->mmio();

    mmio.writeB(MCLK_CTRL, CLK_POSTSCALER(3));

    // Write denominator (M) - mask to ensure only valid bits are set
    mmio.writeMaskB(MCLK_DEN, CLK_DEN_MASK, pllValues.m - 1);

    // Write numerator (N) - mask to ensure only valid bits are set
    mmio.writeMaskB(MCLK_NUM, CLK_NUM_MASK, pllValues.n - 1);

    // Wait for PLL to stabilize
    delayMilliSeconds(5);

    // Set postscaler in control register and enable MCLK programming
    mmio.writeB(MCLK_CTRL, CLK_FREQ_RANGE(0b100) | CLK_POSTSCALER(pllValues.l) | CLK_HIGH_SPEED);

    if (clockHz > 50000000) {
        mmio.writeMaskW(DISP_MEM_CFG, FAST_RAS_DISABLE_MASK, FAST_RAS_DISABLE);
    }

    return actualFreqKhz * 1000;  // convert to hz
}

// Stub implementations for required functions
// FIXME: BoardInfo defines this function as returning a BOOL, but what are we supposed to return?!
BOOL ASM At3dDriver::setDisplay(__REGD0(BOOL state))
{
    BoardInfo *bi = this;
    // Clocking Mode Register (ClK_MODE) (SR1)
    VgaIo vga = this->vga();
    LOCAL_SYSBASE();

    DFUNC(VERBOSE, " state %ld\n", (ULONG)state);

    // SR1 bit 5: Screen Off (1 = screen off, 0 = screen on)
    vga.writeSRMask(0x01, 0x20, (~(UBYTE)state & 1) << 5);

    ChipFlags = (ChipFlags & ~1) | (state & 1);

    return TRUE;
}

BOOL ASM At3dDriver::getVSyncState(__REGD0(BOOL expected))
{
    VgaIo vga = this->vga();
    return (vga.readB(VgaReg::INPUT_STATUS1) & 0x08) != 0;
}

ULONG ASM At3dDriver::getVBeamPos()
{
    At3dMmio mmio = this->mmio();
    return mmio.readW(VERTICAL_CURRENT_POS) & 0x7FF;
}

static void setDefaultClocks(struct BoardInfo *bi)
{
    DFUNC(INFO, "\n");

    At3dMmio mmio = asAt3d(bi)->mmio();

    {
        // Set postscaler in control register
        mmio.writeB(VCLK_DEFAULT0_CTRL, CLK_POSTSCALER_MASK | CLK_POSTSCALER(3));

        // Write denominator (M-1) - register stores M-1, chip uses (register_value + 1)
        mmio.writeMaskB(VCLK_DEFAULT0_DEN, CLK_DEN_MASK, 0x01);

        // Write numerator (N-1) - register stores N-1, chip uses (register_value + 1)
        mmio.writeMaskB(VCLK_DEFAULT0_NUM, CLK_NUM_MASK, 0x1b);

        // Use computed frequency range F from ResolvePixelClock
        mmio.writeMaskB(VCLK_DEFAULT0_CTRL, CLK_POSTSCALER_MASK | CLK_FREQ_RANGE_MASK | CLK_POWER_OFF | CLK_BYPASS,
                        0x6c);

        delayMilliSeconds(1);  // Short delay

        UBYTE vclkCtrlVal = mmio.readB(VCLK_DEFAULT0_CTRL);
        mmio.writeB(VCLK_DEFAULT0_CTRL, (vclkCtrlVal & 0xF9) | 0x4);
        mmio.writeB(VCLK_DEFAULT0_CTRL, (vclkCtrlVal & 0x79) | 0x4);
        mmio.writeB(VCLK_DEFAULT0_CTRL, (vclkCtrlVal & 0x79) | 0x84);
        mmio.writeB(VCLK_DEFAULT0_CTRL, vclkCtrlVal);
    }
    {
        // Set postscaler in control register
        mmio.writeB(VCLK_DEFAULT1_CTRL, CLK_POSTSCALER_MASK | CLK_POSTSCALER(3));

        // Write denominator (M-1) - register stores M-1, chip uses (register_value + 1)
        mmio.writeMaskB(VCLK_DEFAULT1_DEN, CLK_DEN_MASK, 0x02);

        // Write numerator (N-1) - register stores N-1, chip uses (register_value + 1)
        mmio.writeMaskB(VCLK_DEFAULT1_NUM, CLK_NUM_MASK, 0x2e);

        // Use computed frequency range F from ResolvePixelClock
        mmio.writeMaskB(VCLK_DEFAULT1_CTRL, CLK_POSTSCALER_MASK | CLK_FREQ_RANGE_MASK | CLK_POWER_OFF | CLK_BYPASS,
                        0x5c);

        delayMilliSeconds(1);  // Short delay

        UBYTE vclkCtrlVal = mmio.readB(VCLK_DEFAULT1_CTRL);
        mmio.writeB(VCLK_DEFAULT1_CTRL, (vclkCtrlVal & 0xF9) | 0x4);
        mmio.writeB(VCLK_DEFAULT1_CTRL, (vclkCtrlVal & 0x79) | 0x4);
        mmio.writeB(VCLK_DEFAULT1_CTRL, (vclkCtrlVal & 0x79) | 0x84);
        mmio.writeB(VCLK_DEFAULT1_CTRL, vclkCtrlVal);
    }
}
LONG ASM At3dDriver::resolvePixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG desiredPixelClock),
                                       __REGD7(RGBFTYPE_REG rgbFormat))
{
    DFUNC(VERBOSE, "desiredPixelClock=%ld Hz, format=%ld\n", desiredPixelClock, (ULONG)rgbFormat);

    if (!mi) {
        return desiredPixelClock;
    }

    const ChipData_t *cd = chip();
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

void ASM At3dDriver::setClock()
{
    DFUNC(INFO, "\n");

    struct ModeInfo *mi = ModeInfo;
    if (!mi) {
        DFUNC(ERROR, "ModeInfo is NULL\n");
        return;
    }

    D(INFO, "SetClock: PixelClock %ld Hz\n", mi->PixelClock);

    {
        VgaIo vga = this->vga();
        if (ModeInfo->Flags & GMF_DOUBLECLOCK) {
            DFUNC(INFO, "SetClock: Clocking halving enabled\n");
            vga.writeSRMask(0x01, BIT(3), BIT(3));  // Enable DCLK = VCLK/2
        } else {
            vga.writeSRMask(0x01, BIT(3), BIT(0));  // Enable DCLK = VCLK/2
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

    At3dMmio mmio = this->mmio();

    // Set postscaler to 8x
    mmio.writeMaskB(VCLK_CTRL, CLK_POSTSCALER_MASK, CLK_POSTSCALER(3));

    // Write denominator (M-1) - register stores M-1, chip uses (register_value + 1)
    mmio.writeB(VCLK_DEN, mReg);

    // Write numerator (N-1) - register stores N-1, chip uses (register_value + 1)
    mmio.writeB(VCLK_NUM, nReg);

    // Use computed frequency range F from ResolvePixelClock
    mmio.writeMaskB(VCLK_CTRL, CLK_POSTSCALER_MASK | CLK_FREQ_RANGE_MASK | CLK_POWER_OFF | CLK_BYPASS,
                    CLK_POSTSCALER(l) | CLK_FREQ_RANGE(f));

    delayMicroSeconds(10);  // Short delay

    // PLL resync sequence: clear then set CLK_HIGH_SPEED bit
    mmio.writeMaskB(VCLK_CTRL, CLK_HIGH_SPEED, 0);
    mmio.writeMaskB(VCLK_CTRL, CLK_HIGH_SPEED, CLK_HIGH_SPEED);
}

ULONG ASM At3dDriver::getPixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG index), __REGD7(RGBFTYPE_REG rgbFormat))
{
    DFUNC(INFO, "Index: %ld\n", index);

    const ChipData_t *cd = chip();

    if (!cd->pllValues || index >= cd->numPllValues) {
        DFUNC(ERROR, "Invalid pixel clock index %ld (max %ld)\n", index, cd->numPllValues - 1);
        return 0;
    }

    return (ULONG)cd->pllValues[index].freq10khz * 10000;  // 10 kHz -> Hz
}

UWORD ASM At3dDriver::calculateBytesPerRow(__REGD0(UWORD width), __REGD1(UWORD height), __REGA1(struct ModeInfo *mi),
                                           __REGD7(RGBFTYPE_REG rgbFormat))
{
    if (mi) {
        // Bitmap supposed to show on screen.
        // We expect blits to and from on-screen subrectangles, so make the pitch Blitter-compatible
        // Use X/Y addressing for these
        if (width <= 512) {
            // We allow only small resolutions to have a non-Graphics Engine size.
            // These resolutions (notably 320xY) are often used in games and these games
            // assume a pitch of 320 bytes (not 640 which expansion to 640 would
            // require). Nevertheless, align to 8 bytes. We constrain all other
            // resolutions to Graphics Engine supported pitch.
            width = (width + 7) & ~7;
            // width == 512 uses width as-is and is blitter-supported
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

APTR ASM At3dDriver::calculateMemory(__REGA1(APTR mem), __REGD0(struct RenderInfo *ri), __REGD7(RGBFTYPE_REG format))
{
    // AT24 and up: redirect non-packed formats to big-endian aperture
    if (chip()->chipFamily >= AT24) {
        switch (format) {
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
        case RGBFB_A8R8G8B8:
        case RGBFB_R8G8B8A8:
        case RGBFB_A8B8G8R8:
        case RGBFB_R8G8B8:
            // Redirect to Big Endian Linear Address Window
            return (APTR)((UBYTE *)mem + 0x800000);
        default:
            return mem;
        }
    }
    return mem;
}

ULONG ASM At3dDriver::getCompatibleFormats(__REGD7(RGBFTYPE_REG format))
{
    if (format == RGBFB_NONE)
        return (ULONG)0;

    // Base compatible formats that AT3D always supports (native/packed formats)
    // These formats can be used without special aperture configuration
    ULONG compatible = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8 | RGBFF_B8G8R8A8;

    // AT24 and up: non-packed formats available via big-endian aperture
    if (chip()->chipFamily >= AT24) {
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
void ASM At3dDriver::waitBlitter()
{
    DFUNC(CHATTY, "...\n");
    At3dMmio mmio = this->mmio();

    const ChipData_t *cd = chip();
    UBYTE numSlots       = cd->chipFamily < AT24 ? 4 : 8;  // AT24+ has a deeper FIFO

    ULONG status = waitFifo(numSlots);  // make sure FIFO is flushed
    // Wait for FiFo idle and
    while (status & EXT_DAC_DRAWING_ENGINE_BUSY) {
        status = mmio.readL(EXT_DAC_STATUS);
    }
    DFUNC(CHATTY, "done.\n");
}

void At3dDriver::setMemoryModeInternal(RGBFTYPE format)
{
    BoardInfo *bi  = this;
    ChipData_t *cd = chip();
    if (cd->chipFamily < AT24) {
        // No bi-endian support
        return;
    }

    At3dMmio mmio = this->mmio();

    // Check if format has changed
    if (cd->memFormat == format) {
        return;
    }
    cd->memFormat = format;

    // Setup the bi-endian control register for aperture 1 (8-16MB)
    // This controls byte swapping for non-packed formats
    // Register is at offset 0xDC-0xDD in MMVGA window (BAR0)
    UWORD biendianCtrl       = mmio.readW(BIENDIAN_CTRL);
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
    mmio.writeW(BIENDIAN_CTRL, biendianCtrl);

    return;
}

void ASM At3dDriver::setMemoryMode(__REGD7(RGBFTYPE_REG format))
{
    setMemoryModeInternal((RGBFTYPE)format);
}

static INLINE REGARGS UWORD toScanLines(UWORD y, UWORD modeFlags)
{
    if (modeFlags & GMF_DOUBLESCAN)
        y *= 2;
    if (modeFlags & GMF_INTERLACE)
        y /= 2;
    return y;
}

static INLINE REGARGS UWORD adjustBorder(UWORD x, BOOL border, UWORD defaultX)
{
    if (!border || x == 0)
        x = defaultX;
    return x;
}

void ASM At3dDriver::setGC(__REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    DFUNC(VERBOSE,
          "W %ld, H %ld, HTotal %ld, HBlankSize %ld, HSyncStart %ld, HSyncSize "
          "%ld, "
          "\nVTotal %ld, VBlankSize %ld,  VSyncStart %ld ,  VSyncSize %ld\n",
          (ULONG)mi->Width, (ULONG)mi->Height, (ULONG)mi->HorTotal, (ULONG)mi->HorBlankSize, (ULONG)mi->HorSyncStart,
          (ULONG)mi->HorSyncSize, (ULONG)mi->VerTotal, (ULONG)mi->VerBlankSize, (ULONG)mi->VerSyncStart,
          (ULONG)mi->VerSyncSize);

    ModeInfo = mi;
    Border   = border;

    UWORD hTotal      = mi->HorTotal;
    UWORD screenWidth = mi->Width;
    UBYTE modeFlags   = mi->Flags;
    BOOL isInterlaced = (modeFlags & GMF_INTERLACE) ? 1 : 0;
    UBYTE depth       = mi->Depth;

// 8 pixels default border size = 1 character clock
#define ADJUST_HBORDER(x) adjustBorder(x, border, 8)
#define ADJUST_VBORDER(y) adjustBorder(y, border, 1);
#define TO_CLKS(x)        ((x) >> 3)
#define TO_SCANLINES(y)   toScanLines((y), modeFlags)

    VgaIo vga = this->vga();

    // For some reason, the auto-reset-disable sometimes gets lost
    vga.writeCR(CR_EXT_AUTORESET, CR_EXT_AUTORESET_DISABLE);

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
        vga.writeCROverflow1(hTotalClk, 0x00, 0, 8, 0x1B, 0, 1);

        // Horizontal interlaced start
        vga.writeCROverflow1(hTotalClk >> 1, 0x19, 0, 8, 0x1B, 4, 1);
    }

    {
        // Horizontal Display End Register (CR1)
        // One less than the total number of displayed characters
        UWORD hDisplayEnd = TO_CLKS(screenWidth) - 1;
        D(INFO, "Display End %ld\n", (ULONG)hDisplayEnd);
        vga.writeCROverflow1(hDisplayEnd, 0x01, 0, 8, 0x1B, 1, 1);
    }

    UWORD hBorderSize = ADJUST_HBORDER(mi->HorBlankSize);
    {
        // Start Horizontal Blank Register (CR2)
        UWORD hBlankStart = TO_CLKS(screenWidth + hBorderSize);
        D(INFO, "Horizontal Blank Start %ld\n", (ULONG)hBlankStart);
        vga.writeCROverflow1(hBlankStart, 0x02, 0, 8, 0x1B, 2, 1);
    }

    {
        // End Horizontal Blank Register (CR3)
        UWORD hBlankEnd = TO_CLKS(hTotal - hBorderSize) - 1;
        D(INFO, "Horizontal Blank End %ld\n", (ULONG)hBlankEnd);
        vga.writeCROverflow1(hBlankEnd, 0x03, 0, 5, 0x05, 7, 1);
    }

    {
        // Start Horizontal Sync Position Register (CR4)
        UWORD hSyncStart = TO_CLKS(screenWidth + mi->HorSyncStart);
        D(INFO, "HSync start %ld\n", (ULONG)hSyncStart);
        vga.writeCROverflow1(hSyncStart, 0x04, 0, 8, 0x1B, 3, 1);
    }

    {
        // End Horizontal Sync Position Register (CR5)
        UWORD endHSync = TO_CLKS(screenWidth + mi->HorSyncStart + mi->HorSyncSize) - 1;
        D(INFO, "HSync End %ld\n", (ULONG)endHSync);
        vga.writeCRMask(0x05, 0x1f, endHSync);
    }

    {
        // Vertical Total (CR6)
        UWORD vTotal = TO_SCANLINES(mi->VerTotal) - 2;
        D(INFO, "VTotal %ld\n", (ULONG)vTotal);
        vga.writeCROverflow3(vTotal, 0x06, 0, 8, 0x07, 0, 1, 0x07, 5, 1, 0x1A, 0, 1);
    }

    {
        // Vertical Display End register (CR12)
        UWORD vDisplayEnd = TO_SCANLINES(mi->Height) - 1;
        D(INFO, "Vertical Display End %ld\n", (ULONG)vDisplayEnd);
        vga.writeCROverflow3(vDisplayEnd, 0x12, 0, 8, 0x07, 1, 1, 0x07, 6, 1, 0x1A, 1, 1);
    }

    {
        // Vertical Retrace Start Register (VRS) (CR10)
        UWORD vRetraceStart = TO_SCANLINES(mi->Height + mi->VerSyncStart);
        D(INFO, "VRetrace Start %ld\n", (ULONG)vRetraceStart);
        vga.writeCROverflow3(vRetraceStart, 0x10, 0, 8, 0x07, 2, 1, 0x07, 7, 1, 0x1A, 3, 1);
    }

    {
        // Vertical Retrace End Register (VRE) (CR11) Bits 3-0 VERTICAL RETRACE END
        // Note: CR11 bit 5 controls vertical interrupt enable (should remain set to disable interrupt)
        // CR11 bit 7 is write protect (handled separately)
        UWORD vRetraceEnd = TO_SCANLINES(mi->Height + mi->VerSyncStart + mi->VerSyncSize) - 1;
        D(INFO, "VRetrace End %ld, writing low 4 bits 0x%lx", (ULONG)vRetraceEnd, (ULONG)vRetraceEnd & 0xF);
        vga.writeCRMask(0x11, 0x0F, vRetraceEnd);
    }

    UWORD vBlankSize = ADJUST_VBORDER(mi->VerBlankSize);
    {
        // Start Vertical Blank Register (SVB) (CR15)
        UWORD vBlankStart = TO_SCANLINES(mi->Height + vBlankSize);
        D(INFO, "VBlank Start %ld\n", (ULONG)vBlankStart);
        vga.writeCROverflow3(vBlankStart, 0x15, 0, 8, 0x07, 3, 1, 0x09, 5, 1, 0x1A, 2, 1);
    }

    {
        // End Vertical Blank Register (EVB) (CR16)
        UWORD vBlankEnd = TO_SCANLINES(mi->VerTotal - vBlankSize) - 1;
        D(6, "VBlank End %ld\n", (ULONG)vBlankEnd);
        vga.writeCR(0x16, vBlankEnd);
    }

    // Interlace
    {
        At3dMmio mmio       = this->mmio();
        UBYTE interlaceCtrl = mmio.readB(MONITOR_INTERLACE_CTRL);
        if (isInterlaced) {
            interlaceCtrl |= BIT(0);  // Enable interlace (bit 0 of 0D2h)
        } else {
            interlaceCtrl &= ~BIT(0);  // Disable interlace
        }
        mmio.writeB(MONITOR_INTERLACE_CTRL, interlaceCtrl);
    }

    // Doublescan
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
}

void ASM At3dDriver::setDAC(__REGD0(UWORD region), __REGD7(RGBFTYPE_REG format))
{
    DFUNC(INFO, "format=%ld\n", (ULONG)format);

    At3dMmio mmio = this->mmio();

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

    ChipData_t *cd = chip();
    cd->GEFormat   = format;
    cd->GEbppLog2  = getBPPLog2((RGBFTYPE)format);

    // Read current register value
    UBYTE regValue = mmio.readB(SERIAL_CTRL);

    // Clear pixel depth and format bits
    regValue &= ~(DESKTOP_PIXEL_DEPTH_MASK | DESKTOP_PIXEL_FORMAT_MASK);

    // Set new pixel depth and format
    regValue |= pixelDepth | pixelFormat;

    // Handle double index for CLUT with double clock (if needed)
    // Note: Double index requires specific conditions per documentation
    // FIXME: currently GMF_DOUBLECLOCK is used to indicate halved pixel clock
    // if ((format == RGBFB_CLUT) && (ModeInfo && (ModeInfo->Flags & GMF_DOUBLECLOCK))) {
    //     regValue |= BIT(5);  // Enable double index
    // } else {
    //     regValue &= ~BIT(5);  // Disable double index
    // }

    // Write the register
    mmio.writeB(SERIAL_CTRL, regValue);

    DFUNC(VERBOSE, "SetDAC: format=0x%lx, depth=0x%02lx, format=0x%02lx, reg=0x%02lx\n", (ULONG)format,
          (ULONG)pixelDepth, (ULONG)pixelFormat, (ULONG)regValue);
}

void ASM At3dDriver::setColorArray(__REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "startIndex %ld, count %ld\n", (ULONG)startIndex, (ULONG)count);

    LOCAL_SYSBASE();

    // This may not be interrupted, so DAC_WR_AD remains set throughout the function
    Disable();

    VgaIo vga = this->vga();
    vga.writeB(VgaReg::DAC_WR_INDEX, startIndex);

    struct CLUTEntry *entry = &CLUT[startIndex];

    // Write color data for each palette entry
    // Do not print these individual register writes as it takes ages
    for (UWORD c = 0; c < count; ++c) {
        vga.writeB(VgaReg::DAC_PEL_DATA, entry->Red);
        vga.writeB(VgaReg::DAC_PEL_DATA, entry->Green);
        vga.writeB(VgaReg::DAC_PEL_DATA, entry->Blue);
        ++entry;
    }

    if (startIndex == 0) {
        vga.readB(VgaReg::INPUT_STATUS1);  // Reset AFF
        // Background color 0 also sets the border color
        /* 3:3:2 RGB: R[7:5], G[4:2], B[1:0] */
        if (ModeInfo->Depth <= 8) {
            vga.writeAR(0x11, 0);
        } else {
            vga.writeAR(0x11, (UBYTE)((CLUT[0].Red & 0xE0) | ((CLUT[0].Green >> 3) & 0x1C) | (CLUT[0].Blue >> 6)));
        }
        vga.writeB(VgaReg::ATTR_AD, 0x20);  // re-enable normal screen output
    }

    Enable();

    DFUNC(VERBOSE, "done.\n", (ULONG)startIndex, (ULONG)count);
    return;
}

ULONG At3dDriver::getMemoryOffset(APTR memory)
{
    ULONG offset = (ULONG)memory - (ULONG)MemoryBase;
    offset &= ~0x800000;  // map addresses from (BE) linear window2 back to regular framebuffer address
    return offset;
}

void ASM At3dDriver::setPanning(__REGA1(UBYTE *memory), __REGD0(UWORD width), __REGD3(UWORD height),
                                __REGD1(WORD xoffset), __REGD2(WORD yoffset), __REGD7(RGBFTYPE_REG format))
{
    VgaIo vga = this->vga();

    DFUNC(INFO,
          "mem 0x%lx, width %ld, height %ld, xoffset %ld, yoffset %ld, "
          "format %ld\n",
          (ULONG)memory, (ULONG)width, (ULONG)height, (LONG)xoffset, (LONG)yoffset, (ULONG)format);

    LONG panOffset;
    UWORD pitch;
    ULONG memOffset;

    XOffset   = xoffset;
    YOffset   = yoffset;
    memOffset = getMemoryOffset(memory);

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
    vga.writeCROverflow2U(panOffset, 0x0d, 0, 8, 0x0c, 0, 8, 0x1c, 0, 4);

    // Set Serial offset: CR13 (low 8 bits), CR1C[7:4] (bits 11:8)
    // Offset is in units of 8 bytes
    // Thus max pitch is 4095*8 =  32760 bytes
    vga.writeCROverflow1(pitch, 0x13, 0, 8, 0x1c, 4, 4);

    // This has weird effects on the lines the cursor image shows
    // vga.readB(VgaReg::INPUT_STATUS1);  // Reset AFF to latch new start address
    // vga.writeAR(0x13, xoffset & 7);  // Update border color to match new background color (in case it changed)

    return;
}

/**
 * Set DPMS (Display Power Management Signaling) level
 * @param bi BoardInfo structure
 * @param level DPMS level: DPMS_ON (0), DPMS_STANDBY (1), DPMS_SUSPEND (2), DPMS_OFF (3)
 */
void ASM At3dDriver::setDPMSLevel(__REGD0(ULONG level))
{
    DFUNC(VERBOSE, "level=%ld\n", level);
    // Mapping:
    //  DPMS_ON:      Both bits clear (0x00) - HSYNC and VSYNC enabled
    //  DPMS_STANDBY: VSYNC disabled (0x02) - bit [1] set
    //  DPMS_SUSPEND: HSYNC disabled (0x01) - bit [0] set
    //  DPMS_OFF:     Both bits set (0x03) - both HSYNC and VSYNC disabled

    static const UBYTE DPMSLevels[4] = {0x00, 0x01, 0x02, 0x03};

    if (level > 3) {
        level = 3;
    }

    At3dMmio mmio = this->mmio();

    // Set DPMS level in bits [1:0], preserving other bits
    mmio.writeMaskB(DPMS_SYNC_CTRL, 0x03, DPMSLevels[level]);
}

// FIXME: Make sure to coordinate with SetDPMSLevel, does the register signals still get produced?
void ASM At3dDriver::waitVerticalSync(__REGD0(BOOL waitForEnd))
{
    DFUNC(VERBOSE, "waitForEnd: %ld, displayState: 0x%lx\n", (ULONG)waitForEnd, (ULONG)ChipFlags);

    // Don't wait for VSYNC if display is off.
    if (!(ChipFlags & 1)) {
        return;
    }

    VgaIo vga = this->vga();
    if (waitForEnd) {
        // wait for verticel retrace
        // Quiet path / VgaIoQ if debug serial would miss the signals
        while (!(vga.readB(VgaReg::INPUT_STATUS1) & 0x08)) {
        };
        // For pixel display (should now be top of frame, i.e. end of retrace)
        while (!(vga.readB(VgaReg::INPUT_STATUS1) & 0x01)) {
        };
    } else {  // For pixel display first
        while (!(vga.readB(VgaReg::INPUT_STATUS1) & 0x01)) {
        };
        // wait for verticel retrace starting
        while (!(vga.readB(VgaReg::INPUT_STATUS1) & 0x08)) {
        };
    }
}

/* VGA CR11: bit4 = vert IRQ clear/arm, bit5 = 1 disables vert IRQ.
 * INPUTSTATUS0 (0x3C2) bit7 = this CRTC has a pending IRQ. */
BOOL ASM At3dDriver::setInterrupt(__REGD0(BOOL state))
{
    VgaIo vga = this->vga();

    BoardInfo *bi = this;  // FIXME: we need this just for the LOCAL_SYSBASE
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

/* Non-static: DEFINE_INTSERVER asm must jsr the C symbol. */
ULONG ASM At3dDriver::interruptServer()
{
    VgaIoQ vga = vgaQ();

    if (!(vga.readB(VgaReg::MISC_OUT_W) & BIT(7)))
        return 0;

    UBYTE idx = vga.readB(VgaReg::CRTC_INDEX);
    vga.writeB(VgaReg::CRTC_INDEX, 0x11);
    UBYTE cr11 = vga.readB(VgaReg::CRTC_VALUE);
    vga.writeB(VgaReg::CRTC_VALUE, cr11 & ~BIT(4));
    vga.writeB(VgaReg::CRTC_VALUE, cr11 | BIT(4));
    vga.writeB(VgaReg::CRTC_INDEX, idx);

    {
        struct ExecBase *SysBase = ExecBase;
        Cause(&SoftInterrupt);
    }
    return 1;
}
DEFINE_INTSERVER(interruptServerTrampoline, interruptServer);

void ASM At3dDriver::setWriteMask(__REGD0(UBYTE mask)) {}

void ASM At3dDriver::setClearMask(__REGD0(UBYTE mask)) {}

void ASM At3dDriver::setReadPlane(__REGD0(UBYTE mask)) {}

void ASM At3dDriver::setSplitPosition(__REGD0(SHORT splitPos))
{
    VgaIo vga = this->vga();
    DFUNC(VERBOSE, "%ld\n", (ULONG)splitPos);

    YSplit = splitPos;
    if (!splitPos) {
        splitPos = 0x7ff;
    } else {
        if (ModeInfo->Flags & GMF_DOUBLESCAN) {
            splitPos *= 2;
        }
    }
    splitPos -= 1;

    vga.writeCROverflow3((UWORD)splitPos, 0x18, 0, 8, 0x7, 4, 1, 0x9, 6, 1, 0x1a, 4, 1);
}

/* Hardware cursor: 64x64 at 2 bpp, 16 bytes per row, stored at KB-aligned address.
 * Pattern base register is in kilobytes. Position/offset in pixels (12-bit X/Y, 6-bit offsets). */

void ASM At3dDriver::setSpritePosition(__REGD0(WORD xpos), __REGD1(WORD ypos), __REGD7(RGBFTYPE_REG fmt))
{
    (void)fmt;
    At3dMmio mmio = this->mmio();

    MouseX = xpos;
    MouseY = ypos;

    WORD spriteX = xpos - XOffset + 24;
    WORD spriteY = ypos - YOffset + YSplit;

    if (ModeInfo->Flags & GMF_DOUBLESCAN) {
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

    mmio.writeW(HW_CURSOR_X, spriteX & 0xFFF);
    mmio.writeW(HW_CURSOR_Y, spriteY & 0xFFF);
    mmio.writeB(HW_CURSOR_OFF_X, offsetX & 63);
    mmio.writeB(HW_CURSOR_OFF_Y, offsetY & 63);
}

void ASM At3dDriver::setSpriteImage(__REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(VERBOSE, "fmt=%ld\n", (ULONG)fmt);
    (void)fmt;
    packAtiHwCursorImage(this);
}

void ASM At3dDriver::setSpriteColor(__REGD0(UBYTE index), __REGD1(UBYTE red), __REGD2(UBYTE green), __REGD3(UBYTE blue),
                                    __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(VERBOSE, "index=%ld, R=%ld, G=%ld, B=%ld, fmt=%ld\n", (ULONG)index, (ULONG)red, (ULONG)green, (ULONG)blue,
          (ULONG)fmt);

    At3dMmio mmio = this->mmio();
    if (index > 2)
        return;
    // Luckily this bit of index rotation was enough to match the P96 sprite color indices to the AT3D ones
    auto reg = AT3D_MMIO_ID(HW_CURSOR_COL1 + (index + 1) % 3);

    switch (fmt) {
    case RGBFB_NONE:
    case RGBFB_CLUT: {
        UBYTE paletteEntry = index + 17;
        mmio.writeB(reg, paletteEntry);
        // mmio.writeMaskB(HW_CURSOR_CTRL, BIT(2), 0x00);  // Disable "Full Color" mode
        break;
    }
    default:
        /* 3:3:2 RGB: R[7:5], G[4:2], B[1:0] */
        mmio.writeB(reg, (UBYTE)((red & 0xE0) | ((green >> 3) & 0x1C) | (blue >> 6)));
        // mmio.writeMaskB(HW_CURSOR_CTRL, BIT(2), BIT(2));  // Enable "Full Color" mode
        break;
    }
}

BOOL ASM At3dDriver::setSprite(__REGD0(BOOL activate), __REGD7(RGBFTYPE_REG RGBFormat))
{
    DFUNC(VERBOSE, "activate=%ld, format=%ld\n", (ULONG)activate, (ULONG)RGBFormat);

    At3dMmio mmio = this->mmio();

    UBYTE cursorCtrl = BIT(1) | (activate ? BIT(0) : 0);  // 3-color + Enable

    mmio.writeB(HW_CURSOR_CTRL, cursorCtrl);

    if (activate) {
        setSpriteColor(0, CLUT[17].Red, CLUT[17].Green, CLUT[17].Blue, RGBFormat);
        setSpriteColor(1, CLUT[18].Red, CLUT[18].Green, CLUT[18].Blue, RGBFormat);
        setSpriteColor(2, CLUT[19].Red, CLUT[19].Green, CLUT[19].Blue, RGBFormat);
    }
    return TRUE;
}

ULONG At3dDriver::getLinearPixelOffset(const struct RenderInfo *ri, UWORD x, UWORD y, UBYTE bppLog2)
{
    // Note: not suited for 3-bytes-per-pixel modes
    ULONG offset = getMemoryOffset(ri->Memory);
    offset += y * ri->BytesPerRow;
    offset >>= bppLog2;  // convert line offset to units of pixels
    offset += x;         // final offset
    return offset;
}

BOOL At3dDriver::getStartCoordinates(const struct RenderInfo *ri, UBYTE bppLog2, UWORD *originX, UWORD *originY)
{
    // Memory offset is essentially pixel 0,0
    ULONG offset = getMemoryOffset(ri->Memory);
    UWORD y      = (offset / ri->BytesPerRow);
    UWORD x      = (offset % ri->BytesPerRow) >> bppLog2;
    // P96 might store a bitmap relatively far up in memory.  Since the blitter is using
    // coordinates relative to offset 0x0 in memory, we might end up with addresses outside
    // the reach of the blitter. Try to shift the surplus bits into the start X coordinate and
    // hope for the best.
    if (y > BLIT_MAX_SIZE) {
        return FALSE;
    }

    *originX = x;
    *originY = y;

    return TRUE;
}

ULONG At3dDriver::getLocationRegisterValue(const struct RenderInfo *ri, UWORD x, UWORD y, UBYTE bppLog2,
                                           BOOL useLinearAddressing)
{
    ULONG location;
    if (useLinearAddressing) {
        ULONG pixelOffset = getLinearPixelOffset(ri, x, y, bppLog2);
        DFUNC(INFO, "linear pixel offset for (%ld,%ld): %ld (0x%lx)\n", (ULONG)x, (ULONG)y, pixelOffset, pixelOffset);
        location = makeDWORD(swapw(pixelOffset & 0xFFF), swapw(pixelOffset >> 12));
    } else {
        // Memory offset is essentially pixel 0,0
        UWORD originX, originY;
        BOOL reachable = getStartCoordinates(ri, bppLog2, &originX, &originY);
        if (!reachable)
            return ~0;
        location = makeDWORD(swapw(x + originX), swapw(y + originY));
#ifdef DBG
        ULONG offset = getMemoryOffset(ri->Memory);
        DFUNC(INFO, "rect offset: base %ld (0x%lx) -> offX %ld, offY %ld, final register value 0x%08lx\n", offset,
              offset, (ULONG)originX, (ULONG)originY, swapl(location));
#endif
    }

    return location;
}

BOOL At3dDriver::setLocationRegister(const struct RenderInfo *ri, UWORD x, UWORD y, UBYTE bppLog2,
                                     BOOL useLinearAddressing, AT3DMmioReg::Id reg)
{
    ULONG location = getLocationRegisterValue(ri, x, y, bppLog2, useLinearAddressing);
    if (location == ~0)
        return FALSE;

    At3dMmio mmio = this->mmio();
    mmio.writeLRaw(reg, location);
    return TRUE;
}

BOOL At3dDriver::setDstLocation(const struct RenderInfo *ri, UWORD x, UWORD y, UBYTE bppLog2, BOOL useLinearAddressing)
{
    return setLocationRegister(ri, x, y, bppLog2, useLinearAddressing, DST_LOCATION_X_LOW);
}

BOOL At3dDriver::setSrcLocation(const struct RenderInfo *ri, UWORD x, UWORD y, UBYTE bppLog2, BOOL useLinearAddressing)
{
    return setLocationRegister(ri, x, y, bppLog2, useLinearAddressing, SRC_LOCATION_X_LOW);
}

void At3dDriver::setDstPitch(UWORD bytesPerRow)
{
    At3dMmio mmio = this->mmio();
    mmio.writeW(DST_PITCH, bytesPerRow);
}

void At3dDriver::setDrawSize(UWORD width, UWORD height)
{
    At3dMmio mmio = this->mmio();
    mmio.writeW(SRC_SIZE_Y, height);
    // write width last as it may start the drawing operation
    mmio.writeW(SRC_SIZE_X, width);
}

void At3dDriver::setFormat(RGBFTYPE fmt)
{
    ChipData_t *cd = chip();
    if (cd->GEFormat != fmt) {
        cd->GEFormat  = fmt;
        cd->GEbppLog2 = getBPPLog2((RGBFTYPE)fmt);
    }
}

static ULONG penToColor(ULONG pen, RGBFTYPE fmt)
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
    default:
        break;
    }
    return pen;
}

void At3dDriver::setForegroundPen(ULONG fgPen, RGBFTYPE fmt)
{
    ChipData_t *cd = chip();
    if (cd->GEfgPen != fgPen) {
        cd->GEfgPen   = fgPen;
        fgPen         = penToColor(fgPen, fmt);
        At3dMmio mmio = this->mmio();
        mmio.writeL(FRGD_COLOR, fgPen);
    }
}

void At3dDriver::setBackgroundPen(ULONG bgPen, RGBFTYPE fmt)
{
    ChipData_t *cd = chip();
    if (cd->GEbgPen != bgPen) {
        cd->GEbgPen   = bgPen;
        bgPen         = penToColor(bgPen, fmt);
        At3dMmio mmio = this->mmio();
        mmio.writeL(BKGD_COLOR, bgPen);
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

#define ROP_SOURCE                 0xCC
#define ROP_PATTERN                0xF0
#define ROP_SRC_XOR_DST            0x66
#define ROP_SRC_AND_PAT_AND_DST    0x80
#define ROP_NOT_DST                0x55
#define ROP_SRC_OR_DST             0xEE  // ROP3: result = S | D (for BlitPlanar2Chunky OR-accumulate)
#define ROP_PATTERN_AND_SRC_OR_DST 0xE2  //
#define ROP_JAM1                   0xCA  // (P and S) or (not P and D): use S (fg) where P (pattern) is 1, else D
#define ROP_JAM2                   ROP_SOURCE
#define ROP_COMPLEMENT             0x5A  // Flip destination where pattern is 1

/* BlitRectNoMaskComplete OpCode: P96 passes a 4-bit minterm only (B=source, C=destination); see
 * wiki.icomp.de P96_Driver_Development#BlitRectNoMaskComplete. Bits: 3=B∧C, 2=B∧¬C, 1=¬B∧C, 0=¬B∧¬C.
 * Copy source is 0x0C. Convert to ROP3 (P,S,D): replicate low nibble to high so result is independent
 * of Pattern and 0x0C -> 0xCC. No table needed. */
static INLINE UBYTE mintermToRop3(UBYTE minterm)
{
    return (UBYTE)(minterm | (minterm << 4));
}

void At3dDriver::setDrawCmd(ULONG drawCmd)
{
    ChipData_t *cd = chip();
    if (drawCmd != cd->GEdrawCmd) {
        cd->GEdrawCmd = drawCmd;
        At3dMmio mmio = this->mmio();
        mmio.writeL(DRAW_CMD, drawCmd);
    }
}

void ASM At3dDriver::fillRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                              __REGD3(WORD height), __REGD4(ULONG pen), __REGD5(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(INFO,
          "\nx %ld, y %ld, w %ld, h %ld\npen %08lx, mask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)pen, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    // AT3D doesn't support bit masking for CLUT modes, so we require a full mask in that case.
    // True color modes can ignore the mask
    // FIXME: can we use a ROP of "SRC_AND_DST" to emulate the mask?
    if (fmt <= RGBFB_CLUT && mask != 0xFF) {
        D(WARN, "FillRect fallback\n");
        waitBlitter();
        FillRectDefault(this, ri, x, y, width, height, pen, mask, AS_RGBF(fmt));
        return;
    }

    ChipData_t *cd = chip();

    if (cd->chipFamily < AT24 && (UBYTE)fmt != cd->GEFormat) {
        waitBlitter();
        FillRectDefault(this, ri, x, y, width, height, pen, mask, AS_RGBF(fmt));
        return;
    } else {
        setFormat((RGBFTYPE)fmt);
    }

    UBYTE bppLog2 = cd->GEbppLog2;
    BOOL isLinear = ((width << bppLog2) == ri->BytesPerRow);

    ULONG addressModel = isLinear ? (DRAW_DST_ADDR_LINEAR | DRAW_DST_CONTIGUOUS) : getAdressModelBits(ri, bppLog2);
    if (!addressModel) {
        // Pitch can't be expressed in addressing mode bits, fallback to CPU fill
        waitBlitter();
        FillRectDefault(this, ri, x, y, width, height, pen, mask, AS_RGBF(fmt));
        return;
    }
    if (!setDstLocation(ri, x, y, bppLog2, isLinear)) {
        waitBlitter();
        FillRectDefault(this, ri, x, y, width, height, pen, mask, AS_RGBF(fmt));
        return;
    }

    At3dMmio mmio = this->mmio();

    if (cd->GEOp != FILLRECT) {
        cd->GEOp          = FILLRECT;
        cd->GElinear      = 0x55;  // Force update of addressing mode and format
        cd->GEdrawCmd     = 0;
        cd->GEbytesPerRow = 0;
        cd->GEopCode      = 0x81;
        mmio.writeB(RASTEROP, ROP_SOURCE);
    }

    if (isLinear != cd->GElinear || cd->GEbytesPerRow != ri->BytesPerRow || cd->GEFormat != fmt) {
        cd->GEbytesPerRow = ri->BytesPerRow;
        cd->GElinear      = isLinear;
        // Pixel depth:
        // 0b000 = determined by screen (6422 behavior)
        // 0bX01 = 8bpp
        // 0bX10 = 16bpp
        // 0bX11 = 32bpp
        // 0b100 = 24bpp
        UBYTE pixelDepth = bppLog2 + 1;  // matches bit encoding for pixel depth, but doesn't cover 24 bits
        ULONG cmd = DRAW_CMD_OP(DRAW_CMD_RECT) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) | DRAW_PIXEL_DEPTH(pixelDepth) |
                    addressModel;

        setForegroundPen(pen, (RGBFTYPE)fmt);

        setDrawCmd(cmd);

    } else {
        setForegroundPen(pen, (RGBFTYPE)fmt);
    }

    // Kick off the fill by writing the size registers
    setDrawSize(width, height);
    return;
}

void ASM At3dDriver::invertRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                                __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(INFO,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    if (fmt <= RGBFB_CLUT && mask != 0xFF) {
        D(WARN, "InvertRect fallback (mask)\n");
        waitBlitter();
        InvertRectDefault(this, ri, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    ChipData_t *cd = chip();

    if (cd->chipFamily < AT24 && (UBYTE)fmt != cd->GEFormat) {
        waitBlitter();
        InvertRectDefault(this, ri, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    } else {
        setFormat((RGBFTYPE)fmt);
    }

    UBYTE bppLog2 = cd->GEbppLog2;
    BOOL isLinear = ((width << bppLog2) == ri->BytesPerRow);

    ULONG addressModel = isLinear ? (DRAW_DST_ADDR_LINEAR | DRAW_DST_CONTIGUOUS) : getAdressModelBits(ri, bppLog2);
    if (!addressModel) {
        // Pitch can't be expressed in addressing mode bits, fallback to CPU fill
        waitBlitter();
        InvertRectDefault(this, ri, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }
    if (!setDstLocation(ri, x, y, bppLog2, isLinear)) {
        waitBlitter();
        InvertRectDefault(this, ri, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    At3dMmio mmio = this->mmio();

    if (cd->GEOp != INVERTRECT) {
        cd->GEOp      = INVERTRECT;
        cd->GElinear  = 0x55;  // Force update of addressing mode and format
        cd->GEdrawCmd = 0;
        cd->GEopCode  = 0x81;

        mmio.writeB(RASTEROP, ROP_NOT_DST);
    }

    if (isLinear != cd->GElinear || cd->GEbytesPerRow != ri->BytesPerRow || cd->GEFormat != fmt) {
        cd->GEbytesPerRow = ri->BytesPerRow;
        cd->GEFormat      = fmt;
        cd->GElinear      = isLinear;

        UBYTE pixelDepth = bppLog2 + 1;
        ULONG cmd = DRAW_CMD_OP(DRAW_CMD_RECT) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) | DRAW_PIXEL_DEPTH(pixelDepth) |
                    addressModel;

        setDrawCmd(cmd);
    }

    setDrawSize(width, height);
    return;
}

void ASM At3dDriver::blitRectNoMaskComplete(__REGA1(struct RenderInfo *sri), __REGA2(struct RenderInfo *dri),
                                            __REGD0(WORD srcX), __REGD1(WORD srcY), __REGD2(WORD dstX),
                                            __REGD3(WORD dstY), __REGD4(WORD width), __REGD5(WORD height),
                                            __REGD6(UBYTE opCode), __REGD7(RGBFTYPE_REG format))
{
    DFUNC(INFO,
          "\nx1 %ld, y1 %ld, x2 %ld, y2 %ld, w %ld, h %ld\n"
          "minTerm 0x%lx fmt %ld\n"
          "sri->bytesPerRow %ld, sri->memory 0x%lx\n"
          "dri->bytesPerRow %ld, dri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)opCode, (ULONG)format,
          (ULONG)sri->BytesPerRow, (ULONG)sri->Memory, (ULONG)dri->BytesPerRow, (ULONG)dri->Memory);

    At3dMmio mmio = this->mmio();

    ChipData_t *cd = chip();

    // On older chips the format is tied to the current screen format
    if (cd->chipFamily < AT24 && (UBYTE)format != cd->GEFormat) {
        waitBlitter();
        BlitRectNoMaskCompleteDefault(this, sri, dri, srcX, srcY, dstX, dstY, width, height, opCode, AS_RGBF(format));
        return;
    } else {
        setFormat((RGBFTYPE)format);
    }

    if (cd->GEOp != BLITRECTNOMASKCOMPLETE) {
        cd->GEOp      = BLITRECTNOMASKCOMPLETE;
        cd->GEdrawCmd = 0;
        cd->GEopCode  = 0x81;
    }

    if (opCode != cd->GEopCode) {
        cd->GEopCode = opCode;

        UBYTE rop3 = mintermToRop3(opCode);

        D(INFO, "minterm 0x%02lX ROP3 0x%02lX\n", (ULONG)opCode, (ULONG)rop3);

        mmio.writeB(RASTEROP, rop3);
    }

    UBYTE bppLog2 = cd->GEbppLog2;
    // FIXME: cache src and dst render info
    UWORD widthBytes = width << bppLog2;

    ULONG srcAddrModel = getAdressModelBits(sri, bppLog2);
    ULONG dstAddrModel = getAdressModelBits(dri, bppLog2);

    if (!srcAddrModel && !dstAddrModel) {
        D(WARN, "BlitRectNoMaskComplete fallback src and dst can't  both require linear addressing\n");
        waitBlitter();
        BlitRectNoMaskCompleteDefault(this, sri, dri, srcX, srcY, dstX, dstY, width, height, opCode, AS_RGBF(format));
        return;
    }

    BOOL srcCanLinear = (widthBytes == sri->BytesPerRow);
    BOOL dstCanLinear = (widthBytes == dri->BytesPerRow);

    // Either one of src and dst could be rectangle, the other one linear
    if ((!srcAddrModel && !srcCanLinear) || (!dstAddrModel && !dstCanLinear)) {
        D(WARN, "BlitRectNoMaskComplete Fallback. src or dst needs linear but blitsize prevents it\n");
        waitBlitter();
        BlitRectNoMaskCompleteDefault(this, sri, dri, srcX, srcY, dstX, dstY, width, height, opCode, AS_RGBF(format));
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
            waitBlitter();
            BlitRectNoMaskCompleteDefault(this, sri, dri, srcX, srcY, dstX, dstY, width, height, opCode,
                                          AS_RGBF(format));
            return;
        }
    }
    D(INFO, "isSrcLinear %ld, isDstLinear %ld\n", (ULONG)srcLinear, (ULONG)dstLinear);

    // mmio.writeB(SRC_PITCH, sri->BytesPerRow);
    // mmio.writeB(DST_PITCH, dri->BytesPerRow);

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
    setDrawCmd(drawCmd);

    setSrcLocation(sri, srcX, srcY, bppLog2, srcLinear);
    setDstLocation(dri, dstX, dstY, bppLog2, dstLinear);

    setDrawSize(width, height);
    return;
}

void ASM At3dDriver::blitRect(__REGA1(struct RenderInfo *sri), __REGD0(WORD srcX), __REGD1(WORD srcY),
                              __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width), __REGD5(WORD height),
                              __REGD6(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(INFO,
          "\nx1 %ld, y1 %ld, x2 %ld, y2 %ld, w %ld, \n"
          "h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt,
          (ULONG)sri->BytesPerRow, (ULONG)sri->Memory);

    At3dMmio mmio = this->mmio();

    if (mask != 0xFF) {
        D(WARN, "BlitRect fallback (mask != 0xFF)\n");
        waitBlitter();
        BlitRectDefault(this, sri, srcX, srcY, dstX, dstY, width, height, mask, AS_RGBF(fmt));
        return;
    }

    ChipData_t *cd = chip();

    if (cd->chipFamily < AT24 && (UBYTE)fmt != cd->GEFormat) {
        waitBlitter();
        BlitRectDefault(this, sri, srcX, srcY, dstX, dstY, width, height, mask, AS_RGBF(fmt));
        return;
    } else {
        setFormat((RGBFTYPE)fmt);
    }

    if (cd->GEOp != BLITRECT) {
        cd->GEOp      = BLITRECT;
        cd->GEdrawCmd = 0;
        cd->GEopCode  = 0x81;

        mmio.writeB(RASTEROP, ROP_SOURCE);
    }

    UBYTE bppLog2    = cd->GEbppLog2;
    UWORD widthBytes = width << bppLog2;

    ULONG addrModel = getAdressModelBits(sri, bppLog2);
    BOOL isLinear   = (widthBytes == sri->BytesPerRow);

    if (!addrModel) {
        D(WARN, "BlitRectNoMaskComplete Fallback. src needs linear but blitsize prevents it\n");
        waitBlitter();
        BlitRectDefault(this, sri, srcX, srcY, dstX, dstY, width, height, mask, AS_RGBF(fmt));
        return;
    }

    ULONG drawCmd = DRAW_CMD_OP(DRAW_CMD_BLT) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) | DRAW_PIXEL_DEPTH(bppLog2 + 1);
    drawCmd |= addrModel;

    if (!addrModel) {
        drawCmd |= DRAW_SRC_ADDR_LINEAR | DRAW_SRC_CONTIGUOUS;
    }
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
    setDrawCmd(drawCmd);
    // FIXME: this can be optimized into a single function
    setSrcLocation(sri, srcX, srcY, bppLog2, isLinear);
    setDstLocation(sri, dstX, dstY, bppLog2, isLinear);

    setDrawSize(width, height);
    return;
}

// Host BLT port in flat memory (last 32K). Poll EXT_DAC_HOST_BLT_IN_PROGRESS until high before writing.
volatile ULONG *At3dDriver::getHostBltPort()
{
    return (volatile ULONG *)((UBYTE *)MemoryBase + HOST_BLT_OFFSET);
}

void At3dDriver::setDrawMode(UBYTE drawMode, ULONG fgPen, ULONG bgPen, RGBFTYPE fmt)
{
    setForegroundPen(fgPen, (RGBFTYPE)fmt);
    setBackgroundPen(bgPen, (RGBFTYPE)fmt);

    ChipData_t *cd = chip();
    if (cd->GEopCode != drawMode) {
        cd->GEopCode = drawMode;
        UBYTE rop    = 0;
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
        {
            At3dMmio mmio = this->mmio();
            // Documentation says that PCI burst writes break writing to ROP right after DRAW_CMD.
            mmio.writeB(RASTEROP, rop);
        }
    }
}

/* AT24+: mono-to-color via HOST-BLT (getHostBltPort, write template data). */
void ASM At3dDriver::blitTemplate(__REGA1(struct RenderInfo *ri), __REGA2(struct Template *tmpl), __REGD0(WORD x),
                                  __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                                  __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(INFO, "x %ld, y %ld, w %ld, h %ld mask 0x%02lx fmt %ld\n", (LONG)x, (LONG)y, (LONG)width, (LONG)height,
          (ULONG)mask, (ULONG)fmt);

    if (fmt <= RGBFB_CLUT && mask != 0xFF) {
        D(WARN, "BlitTemplate fallback (CLUT and mask)\n");
        waitBlitter();
        BlitTemplateDefault(this, ri, tmpl, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    At3dMmio mmio  = this->mmio();
    ChipData_t *cd = chip();

    setFormat((RGBFTYPE)fmt);
    UBYTE bppLog2    = cd->GEbppLog2;
    UWORD widthBytes = width << bppLog2;

    BOOL isLinear      = (widthBytes == ri->BytesPerRow);
    ULONG addressModel = isLinear ? (DRAW_DST_ADDR_LINEAR | DRAW_DST_CONTIGUOUS) : getAdressModelBits(ri, bppLog2);
    if (!addressModel) {
        waitBlitter();
        BlitTemplateDefault(this, ri, tmpl, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    if (cd->GEOp != BLITTEMPLATE) {
        cd->GEOp      = BLITTEMPLATE;
        cd->GEdrawCmd = 0;
        cd->GEopCode  = 0x81;

        //    setDstPitch(ri->BytesPerRow);
        /* 11.7.6: Source Location X must be 0 for mono-to-color. Monochrome source must be 64-bit aligned. */
        mmio.writeL(SRC_LOCATION_X_LOW, 0);
    }
    setDstLocation(ri, (UWORD)x, (UWORD)y, bppLog2, isLinear);

    ULONG drawCmd = DRAW_CMD_OP(DRAW_CMD_HOST_BLT_WRITE) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) |
                    DRAW_SRC_MONOCHROME | DRAW_SRC_ADDR_LINEAR | DRAW_SRC_CONTIGUOUS | DRAW_PIXEL_DEPTH(bppLog2 + 1) |
                    addressModel;

    ULONG bgPen = tmpl->BgPen;
    if (!(tmpl->DrawMode & JAM2)) {
        drawCmd |= DRAW_SRC_TRANSPARENT;

        // "Color/monochrome - Monochrome regions are expanded to depth of
        // display memory by replacing source 0s with background color and 1s
        // with foreground color. To expand a monochrome region to
        // foreground/transparent, the host should set both the source
        // monochrome and source transparent bits, and set the background color
        // to a different color than the foreground color register."

        // I think this means that the expanded color will be compared to the
        // background/transparency color for determining whether to write the pixel or not.
        // So if we want the foreground color to survive the transparency test,
        // set the background to its complement.
        bgPen = ~tmpl->FgPen;
    }
    setDrawMode(tmpl->DrawMode, tmpl->FgPen, bgPen, (RGBFTYPE)fmt);
    setDrawCmd(drawCmd);

    ULONG invert = (tmpl->DrawMode & INVERSVID) ? ~(ULONG)0 : 0;

    /* 11.7.6: Host BLT mono data is a byte stream: width rounded up to next 8 bits (bytesPerLine bytes per row).
     * Bytes are packed into 32-bit words; words are written at 8-byte offsets (0, 8, 16, ...).
     * So one 32-bit word can span a row boundary (e.g. last bytes of line 0 + first bytes of line 1). */
    UWORD byteWidth = (width + 7) / 8;
    BOOL srcLinear  = (byteWidth == tmpl->BytesPerRow);
    if (srcLinear) {
        setDrawSize(width, height);

        // Template data is already tightly 8-bit packed
        ASSERT(tmpl->XOffset == 0);  // if the width is the same as the pitch, we can't really have an X offset
        D(INFO, "Template data is already in suitable format for direct host BLT\n");
        const ULONG *src = (const ULONG *)tmpl->Memory;

        while (!mmio.testL(EXT_DAC_STATUS, EXT_DAC_HOST_BLT_IN_PROGRESS)) {
        };

        // Round up each line to byte boundary, then to dword boundary since we write in dwords
        volatile ULONG *hostBlt = getHostBltPort();
        UWORD linearDWords      = ((byteWidth * height) + 3) / 4;
        for (ULONG i = 0; i < linearDWords; i++) {
            ULONG pattern = src[i];
            *hostBlt      = pattern ^ invert;
        }
    } else {
        // // more generic functions
        // const UBYTE *bitmap = (const UBYTE *)tmpl->Memory;
        // UWORD bitmapPitch   = (UWORD) tmpl->BytesPerRow;

        // UBYTE rol = (UBYTE) tmpl->XOffset;
        // if (tmpl->XOffset >= 8) {
        //     bitmap++;
        //     rol -= 8;
        // }

        // ULONG dwords          = ((byteWidth * height) + 3) / 4;
        // UWORD currentLineByte = 0;

        // D(INFO, "byteWidth %ld, bitmapPitch %ld, rol %ld, dwords %ld\n", (ULONG)byteWidth, (ULONG)bitmapPitch,
        //   (ULONG)rol, dwords);
        // while (!mmio.testL(EXT_DAC_STATUS, EXT_DAC_HOST_BLT_IN_PROGRESS)) {
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

        if (!isLinear) {
            UWORD originX, originY;
            getStartCoordinates(ri, bppLog2, &originX, &originY);
            UWORD maxWidth = originX + ri->BytesPerRow >> bppLog2;
            UWORD clipR    = originX + x + width - 1;
            if (clipR >= maxWidth) {
                clipR = maxWidth - 1;
            }
            mmio.writeW(CLIP_RIGHT, clipR);
        }

        // Now Round up to the next multiple of 32
        width = (width + 31) & ~31;

        setDrawSize(width, height);

        const UBYTE *bitmap = (const UBYTE *)tmpl->Memory;
        UWORD bitmapPitch   = (UWORD)tmpl->BytesPerRow;
        UWORD dwordsPerLine = width / 32;
        UBYTE rol           = tmpl->XOffset;
        while (!mmio.testL(EXT_DAC_STATUS, EXT_DAC_HOST_BLT_IN_PROGRESS)) {
        };
        volatile ULONG *hostBlt = getHostBltPort();
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
    /* 11.7.6: write to M040 (DRAW_CMD) with arbitrary data to complete the write to last line(s). */
    // setDrawCmd(DRAW_CMD_OP(DRAW_CMD_NOP)| DRAW_ENGINE_START);
    // flushWrites();

#if 1
    // FIXME: this should not be needed if everything went right...
    {
        int count               = 100;
        volatile ULONG *hostBlt = getHostBltPort();
        while (mmio.testL(EXT_DAC_STATUS, EXT_DAC_HOST_BLT_IN_PROGRESS) && --count) {
            *hostBlt = 0;
        };

        if (count < 99) {
            if (!count)
                mmio.writeB(ABORT, 0x00);  // Byte counting gone wrong, abort host write blit
            D(WARN, "Host BLT completion wait loop iterated %d times\n", 100 - count);
        }
    }
#endif

    if (!isLinear) {
        mmio.writeW(CLIP_RIGHT, 0xFFF);
    }
    return;
}

/* 6422: CPU upload template to reserved 1KB staging, then screen-to-screen mono BLT (no HOST-Write). */
void ASM At3dDriver::blitTemplate6422(__REGA1(struct RenderInfo *ri), __REGA2(struct Template *tmpl), __REGD0(WORD x),
                                      __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                                      __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(INFO, "x %ld, y %ld, w %ld, h %ld mask 0x%02lx fmt %ld\n", (LONG)x, (LONG)y, (LONG)width, (LONG)height,
          (ULONG)mask, (ULONG)fmt);

    if (fmt <= RGBFB_CLUT && mask != 0xFF) {
        D(WARN, "BlitTemplate6422 fallback (CLUT and mask)\n");
        waitBlitter();
        BlitTemplateDefault(this, ri, tmpl, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    ChipData_t *cd = chip();

    if (cd->chipFamily < AT24 && (UBYTE)fmt != cd->GEFormat) {
        waitBlitter();
        BlitTemplateDefault(this, ri, tmpl, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    } else {
        setFormat((RGBFTYPE)fmt);
    }

    UBYTE bppLog2      = cd->GEbppLog2;
    UWORD widthBytes   = width << bppLog2;
    BOOL isLinear      = (widthBytes == ri->BytesPerRow);
    ULONG addressModel = isLinear ? (DRAW_DST_ADDR_LINEAR | DRAW_DST_CONTIGUOUS) : getAdressModelBits(ri, bppLog2);
    if (!addressModel) {
        waitBlitter();
        BlitTemplateDefault(this, ri, tmpl, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    UWORD byteWidth     = (width + 7) / 8;
    UWORD rowBytesDword = (byteWidth + 3) & ~3;
    UWORD maxRows       = 1024 / rowBytesDword;
    if (height > maxRows) {
        D(WARN, "BlitTemplate6422 fallback (height %ld > maxRows %ld)\n", (ULONG)height, (ULONG)maxRows);
        waitBlitter();
        BlitTemplateDefault(this, ri, tmpl, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    if (cd->GEOp != BLITTEMPLATE) {
        cd->GEOp = BLITTEMPLATE;
    }

    UWORD blitWidth = (width + 31) & ~31;

    {
        waitBlitter();  // Wait for previous blits to finish accessing the staging area

        volatile ULONG *staging = (volatile ULONG *)(MemoryBase + cd->templateStagingOffset);
        const UBYTE *bitmap     = (const UBYTE *)tmpl->Memory;
        UWORD bitmapPitch       = (UWORD)tmpl->BytesPerRow;
        UWORD dwordsPerLine     = blitWidth / 32;
        ULONG invert            = (tmpl->DrawMode & INVERSVID) ? ~0 : 0;
        UBYTE rol               = (UBYTE)tmpl->XOffset;

        if (!rol) {
            for (UWORD row = 0; row < height; ++row) {
                for (UWORD col = 0; col < dwordsPerLine; ++col) {
                    *staging++ = invert ^ ((const ULONG *)bitmap)[col];
                }
                bitmap += bitmapPitch;
            }
        } else {
            for (UWORD row = 0; row < height; ++row) {
                for (UWORD col = 0; col < dwordsPerLine; ++col) {
                    ULONG left  = ((const ULONG *)bitmap)[col] << rol;
                    ULONG right = ((const ULONG *)bitmap)[col + 1] >> (32 - rol);
                    *staging++  = invert ^ (left | right);
                }
                bitmap += bitmapPitch;
            }
        }
    }

    At3dMmio mmio = this->mmio();

    if (!isLinear) {
        UWORD originX, originY;
        if (!getStartCoordinates(ri, bppLog2, &originX, &originY)) {
            waitBlitter();
            BlitTemplateDefault(this, ri, tmpl, x, y, width, height, mask, AS_RGBF(fmt));
            return;
        }
        UWORD clipR    = originX + x + width - 1;
        UWORD maxWidth = originX + ri->BytesPerRow >> bppLog2;
        if (clipR >= maxWidth) {
            clipR = maxWidth - 1;
        }

        mmio.writeW(CLIP_RIGHT, clipR);
    }

    ULONG drawCmd = DRAW_CMD_OP(DRAW_CMD_BLT) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) | DRAW_SRC_MONOCHROME |
                    DRAW_SRC_ADDR_LINEAR | DRAW_SRC_CONTIGUOUS | addressModel;

    ULONG bgPen = tmpl->BgPen;
    if (!(tmpl->DrawMode & JAM2)) {
        drawCmd |= DRAW_SRC_TRANSPARENT;
        // make forground color always survive the transparency test
        bgPen = ~tmpl->FgPen;
    }

    setDrawCmd(drawCmd);
    setDrawMode(tmpl->DrawMode, tmpl->FgPen, bgPen, (RGBFTYPE)fmt);

    {
        ULONG location = cd->templateStagingOffset >> bppLog2;
        location       = makeDWORD(swapw(location & 0xFFF), swapw(location >> 12));
        mmio.writeLRaw(SRC_LOCATION_X_LOW, location);
    }

    setDstLocation(ri, (UWORD)x, (UWORD)y, bppLog2, isLinear);
    setDrawSize(blitWidth, height);

    if (!isLinear) {
        mmio.writeW(CLIP_RIGHT, 0xFFF);
    }
    return;
}

/* One plane of BlitPlanar2Chunky: mono Host BLT with FgPen=(1<<p), BgPen=0, ROP Src OR Dst. */
void At3dDriver::performPlanarPlaneBlit(UWORD width, UWORD height, UBYTE *bitmap, UWORD dwordsPerLine, WORD bmPitch,
                                        UBYTE rol, UBYTE planeIndex)
{
    if (!bitmap) {
        D(INFO, "skip plane\n");
        // no need to fill in 0s
        return;
    }

    DFUNC(INFO, "BlitPlanar2Chunky plane %ld,  w %ld (%ld dwords), h %ld rol %ld, 0x%08lx \n", (ULONG)planeIndex,
          (ULONG)width, (ULONG)dwordsPerLine, (ULONG)height, (ULONG)rol, bitmap);
    At3dMmio mmio = this->mmio();

    setForegroundPen(1 << planeIndex, RGBFB_CLUT);
    // setBackgroundPen(1 << planeIndex, RGBFB_CLUT);
    setDrawSize(width, height);

    volatile ULONG *hostBlt = getHostBltPort();
    while (!mmio.testL(EXT_DAC_STATUS, EXT_DAC_HOST_BLT_IN_PROGRESS)) {
    }

    if ((ULONG)bitmap == 0xFFFFFFFFUL) {
        // FIXME: use FillRect to cover these planes
        ULONG fill = ~0;
        for (UWORD y = 0; y < height; ++y) {
            for (UWORD x = 0; x < dwordsPerLine; ++x) {
                *hostBlt = fill;
            }
        }
    } else {
        if (!rol) {
            for (UWORD y = 0; y < height; ++y) {
                for (UWORD x = 0; x < dwordsPerLine; ++x) {
                    *hostBlt = ((const ULONG *)bitmap)[x];
                }
                bitmap += bmPitch;
            }
        } else {
            for (UWORD y = 0; y < height; ++y) {
                for (UWORD x = 0; x < dwordsPerLine; ++x) {
                    ULONG left  = ((const ULONG *)bitmap)[x] << rol;
                    ULONG right = ((const ULONG *)bitmap)[x + 1] >> (32 - rol);
                    *hostBlt    = left | right;
                }
                bitmap += bmPitch;
            }
        }
    }

    //    mmio.writeL(DRAW_CMD, DRAW_CMD_OP(DRAW_CMD_NOP) | DRAW_ENGINE_START);

    {
        ChipData_t *cd = chip();
        int count      = 100;
        while (mmio.testL(EXT_DAC_STATUS, EXT_DAC_HOST_BLT_IN_PROGRESS) && --count) {
            *hostBlt = 0xFF00AACC;
        }
        if (!count) {
            D(WARN, "Host BLT completion wait loop iterated too many times, aborting\n");
            mmio.writeB(ABORT, 0x01);
        }
    }
}

/* Planar to chunky: clear destination to 0, then for each plane OR (1<<p) expansion. No per-bit write mask on AT3D. */
void ASM At3dDriver::blitPlanar2Chunky(__REGA1(struct BitMap *bm), __REGA2(struct RenderInfo *ri), __REGD0(SHORT srcX),
                                       __REGD1(SHORT srcY), __REGD2(SHORT dstX), __REGD3(SHORT dstY),
                                       __REGD4(SHORT width), __REGD5(SHORT height), __REGD6(UBYTE minTerm),
                                       __REGD7(UBYTE mask))
{
    DFUNC(INFO, "src %ld,%ld dst %ld,%ld w %ld h %ld mask 0x%02lx minTerm 0x%02lx\n", (LONG)srcX, (LONG)srcY,
          (LONG)dstX, (LONG)dstY, (LONG)width, (LONG)height, (ULONG)mask, (ULONG)minTerm);

    // if (mask != 0xFF) {
    //     DFUNC(WARN, "BlitPlanar2Chunky fallback (mask != 0xFF)\n");
    //     // Though we could easily incorporate the mask into into the host blit for the conversion, the initial
    //     // clearing of the destination via FillRect can't support the mask, so just fallback to CPU blit for
    //     simplicity.
    //     // FIXME: have a FillRect function that supports ROP and can use and SRC_AND_DST function to clear
    //     // with mask
    //     goto fallback;
    // }
    // if (minTerm != 0x0C) {
    //     DFUNC(WARN, "fallback (minTerm != 0x0C)\n");
    //     goto fallback;
    // }

    ASSERT(ri->RGBFormat == RGBFB_CLUT);

    fillRect(ri, dstX, dstY, width, height, 0, mask, RGBFB_CLUT);

    DFUNC(INFO, "post Fillrect\n");

    At3dMmio mmio  = this->mmio();
    ChipData_t *cd = chip();

    if (cd->GEOp != BLITPLANAR2CHUNKY) {
        cd->GEOp      = BLITPLANAR2CHUNKY;
        cd->GEdrawCmd = 0;
        cd->GEopCode  = 0x81;
        // ROP3 only available during pattern blits?
        // mmio.writeB(RASTEROP, ROP_PATTERN_AND_SOURCE_OR_DST);
        mmio.writeB(RASTEROP, ROP_SRC_OR_DST | (mintermToRop3(minTerm) & 0xF0));
        mmio.writeL(SRC_LOCATION_X_LOW, 0);
        setBackgroundPen(0, RGBFB_CLUT);
    }

    struct RenderInfo dstRi = *ri;

    // We can do linear, if there's effectively no pitch and we only need to transfer full dwords
    const BOOL isLinear = FALSE;  // s(width == dstRi.BytesPerRow) && !(width & 31);
    // If this is a linear blit, we can handle all width, otherwise either the blitter can support the pitch
    // directly or as a last resort, we can emulate 320 width.
    const BOOL emulate320 = !isLinear && (dstRi.BytesPerRow == 320);

    if (emulate320) {
        DFUNC(WARN, "emulating 320\n");
        dstRi.BytesPerRow = 640;
    }

    ULONG addressModel = isLinear ? (DRAW_DST_ADDR_LINEAR | DRAW_DST_CONTIGUOUS) : getAdressModelBits(&dstRi, 0);

    D(INFO, "isLinear %ld, emulate320 %ld, addressModel 0x%08lx\n", (ULONG)isLinear, (ULONG)emulate320, addressModel);

    if (!addressModel) {
        waitBlitter();
        BlitPlanar2ChunkyDefault(this, bm, ri, srcX, srcY, dstX, dstY, width, height, minTerm, mask);
        return;
    }

    UWORD clipR;
    if (!isLinear) {
        UWORD originX, originY;
        getStartCoordinates(&dstRi, 0, &originX, &originY);

        UWORD maxWidth = originX + dstRi.BytesPerRow;
        clipR          = originX + dstX + width - 1;
        if (clipR >= maxWidth) {
            clipR = maxWidth - 1;
        }
        mmio.writeW(CLIP_RIGHT, clipR);
    }

    // Round up to 32pixels, so we don't have too much hassle with the HOST Blit being byte-aligned.
    // Compensate with the clipping setup
    width = (width + 31) & ~31;

    ULONG drawCmd = DRAW_CMD_OP(DRAW_CMD_HOST_BLT_WRITE) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) |
                    DRAW_SRC_MONOCHROME | DRAW_SRC_ADDR_LINEAR | DRAW_SRC_CONTIGUOUS | DRAW_PIXEL_DEPTH(1) |
                    addressModel;
    setDrawCmd(drawCmd);

    WORD bmPitch        = bm->BytesPerRow;
    ULONG bmStartOffset = (ULONG)(srcY * bmPitch) + (srcX / 32) * 4;  // should this be rather / 8?

    UWORD dwordsPerLine = width / 32;
    UBYTE rol           = (UBYTE)(srcX % 32);

    D(INFO, "bmPitch %ld, bmStartOffset %ld, rol %ld, dwordsPerLine %ld\n", (ULONG)bmPitch, (ULONG)bmStartOffset,
      (ULONG)rol, (ULONG)dwordsPerLine);

    setDstLocation(&dstRi, dstX, dstY, 0, isLinear);

    for (short p = 0; p < 8; ++p) {
        if (!(mask & (1 << p))) {
            continue;
        }
        UBYTE *planeBitmap  = (UBYTE *)bm->Planes[p];
        UBYTE *planeBitmap2 = (UBYTE *)bm->Planes[p];
        if (planeBitmap != (UBYTE *)0 && (ULONG)planeBitmap != 0xFFFFFFFFUL) {
            planeBitmap  = (UBYTE *)((ULONG)planeBitmap + bmStartOffset);
            planeBitmap2 = (UBYTE *)((ULONG)planeBitmap + bmStartOffset + bmPitch);
        }

        if (!emulate320) {
            performPlanarPlaneBlit(width, height, planeBitmap, dwordsPerLine, bmPitch, rol, p);
        } else {
            UWORD halfHeight1 = (height + 1) / 2;
            UWORD halfHeight2 = height / 2;

            setDstLocation(&dstRi, dstX, dstY, 0, isLinear);
            mmio.writeW(CLIP_RIGHT, clipR);
            performPlanarPlaneBlit(width, halfHeight1, planeBitmap, dwordsPerLine, bmPitch * 2, rol, p);

            if (halfHeight2) {
                setDstLocation(&dstRi, dstX + 320, dstY, 0, isLinear);
                mmio.writeW(CLIP_RIGHT, clipR + 320);
                performPlanarPlaneBlit(width, halfHeight2, planeBitmap2, dwordsPerLine, bmPitch * 2, rol, p);
            }
        }
    }

    if (!isLinear) {
        mmio.writeW(CLIP_RIGHT, 0xFFF);
    }

    return;
}

void ASM At3dDriver::blitPattern(__REGA1(struct RenderInfo *ri), __REGA2(struct Pattern *pattern), __REGD0(WORD x),
                                 __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                                 __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(INFO, "x %ld, y %ld, w %ld, h %ld mask 0x%02lx fmt %ld\n", (LONG)x, (LONG)y, (LONG)width, (LONG)height,
          (ULONG)mask, (ULONG)fmt);

    if (fmt <= RGBFB_CLUT && mask != 0xFF) {
        D(WARN, "BlitPattern fallback (CLUT mask)\n");
        waitBlitter();
        BlitPatternDefault(this, ri, pattern, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    ChipData_t *cd = chip();

    if (cd->chipFamily < AT24 && (UBYTE)fmt != cd->GEFormat) {
        waitBlitter();
        BlitPatternDefault(this, ri, pattern, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    } else {
        setFormat((RGBFTYPE)fmt);
    }

    UBYTE bppLog2 = cd->GEbppLog2;

    BOOL isLinear      = FALSE;  // ((width << bppLog2) == ri->BytesPerRow);
    ULONG addressModel = isLinear ? (DRAW_DST_ADDR_LINEAR | DRAW_DST_CONTIGUOUS) : getAdressModelBits(ri, bppLog2);
    if (!addressModel) {
        waitBlitter();
        BlitPatternDefault(this, ri, pattern, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    if (cd->GEOp != BLITPATTERN) {
        cd->GEOp      = BLITPATTERN;
        cd->GEdrawCmd = 0;
        cd->patternCacheKey &= ~0x80000000;
    }

    UWORD invert = (pattern->DrawMode & INVERSVID) ? ~0 : 0;

    UWORD patternHeight        = 1 << pattern->Size;
    const UWORD *sysMemPattern = (const UWORD *)pattern->Memory;
    UWORD *cachedPattern       = cd->patternCacheBuffer;

    BOOL patternChanged = FALSE;
    BOOL is8x8          = (patternHeight <= 8);

    if (is8x8) {
        for (UWORD i = 0; i < patternHeight; ++i) {
            UWORD row = sysMemPattern[i] ^ invert;
            if (row != cachedPattern[i]) {
                cachedPattern[i] = row;
                patternChanged   = TRUE;
            }
            if ((UBYTE)(row >> 8) != (UBYTE)row) {
                is8x8 = FALSE;
            }
        }
    }

    if (!is8x8) {
        // FIXME: implement fallback using repeating HOST blit mono pattern
        waitBlitter();
        BlitPatternDefault(this, ri, pattern, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    UWORD originX, originY;
    if (!getStartCoordinates(ri, bppLog2, &originX, &originY)) {
        waitBlitter();
        BlitPatternDefault(this, ri, pattern, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    /* Screen-space aligned 8x8: offset pattern by (x - XOffset) & 7 and (y - YOffset) & 7 via pre-rotation. */
    UBYTE pattOffX = (UBYTE)((originX + x - pattern->XOffset) & 7);
    UBYTE pattOffY = (UBYTE)((originY + y - pattern->YOffset) & 7);

    D(INFO, "pattern offset in screen space: (%ld, %ld)\n", (ULONG)pattOffX, (ULONG)pattOffY);

    ULONG pattCacheKey = (pattOffX << 16) | (pattOffY << 8) | pattern->Size | 0x80000000;
    if (patternChanged || pattCacheKey != cd->patternCacheKey) {
        cd->patternCacheKey = pattCacheKey;

        // Duplicate pattern vertically if needed
        ULONG pat0, pat1;
        switch (pattern->Size) {
        case 0:
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
            pat0 = pat1 = 0;
            break;
        }

        // Pre-rotate horizontally
        if (pattOffX) {
            ULONG maskLower = (1 << pattOffX) - 1;
            maskLower |= (maskLower << 8) | (maskLower << 16) | (maskLower << 24);
            ULONG maskUpper = ~maskLower;
            pat0            = ((pat0 & maskUpper) >> pattOffX) | ((pat0 & maskLower) << (8 - pattOffX));
            pat1            = ((pat1 & maskUpper) >> pattOffX) | ((pat1 & maskLower) << (8 - pattOffX));
        }

        // Pre-rotate vertically
        if (pattOffY) {
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

        cd->pat0 = pat0;
        cd->pat1 = pat1;
    }

    At3dMmio mmio = this->mmio();

    /* Upload pre-rotated 8x8 pattern to PATTERN register (0x048: two DWORDs).
     * Documentation says: "Write to this register prior to each use.
     * The contents of M048–04F are not sustained across all operations.".
     * So no caching.
     */
    mmio.writeLRaw(PATTERN0, cd->pat0);
    mmio.writeLRaw(PATTERN1, cd->pat1);

    ULONG drawCmd = DRAW_CMD_OP(DRAW_CMD_RECT) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) |
                    DRAW_PIXEL_DEPTH(bppLog2 + 1) | addressModel;
    drawCmd |= cd->chipFamily >= AT24 ? DRAW_PATTERN_FORMAT(0b10) : DRAW_6422_PATTERN;

    ULONG bgPen = pattern->BgPen;
    if (!(pattern->DrawMode & JAM2)) {
        drawCmd |= DRAW_SRC_TRANSPARENT;
        // Make the forground color always survive the transparency test
        bgPen = ~pattern->FgPen;
    }
    setDrawMode(pattern->DrawMode, pattern->FgPen, bgPen, (RGBFTYPE)fmt);

    setDrawCmd(drawCmd);

    setDstLocation(ri, (UWORD)x, (UWORD)y, bppLog2, isLinear);
    setDrawSize((UWORD)width, (UWORD)height);
    return;
}

/* DrawLine: horizontal/vertical via FillRect or strip; diagonal via AT3D vector DDA.
 * Patterned lines (LinePtrn != 0xFFFF) fall back to DrawLineDefault for now. */
void ASM At3dDriver::drawLine(__REGA1(struct RenderInfo *ri), __REGA2(struct Line *line), __REGD0(UBYTE mask),
                              __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(VERBOSE,
          "X %ld Y %ld Length %lu dX %ld dY %ld sDelta %ld lDelta %ld twoSDminusLD %ld "
          "LinePtrn 0x%04lx PatternShift %lu FgPen %lu BgPen %lu Horizontal %ld DrawMode 0x%02lx "
          "Xorigin %lu Yorigin %lu mask 0x%02lx fmt %ld\n",
          (LONG)line->X, (LONG)line->Y, (ULONG)line->Length, (LONG)line->dX, (LONG)line->dY, (LONG)line->sDelta,
          (LONG)line->lDelta, (LONG)line->twoSDminusLD, (ULONG)line->LinePtrn, (ULONG)line->PatternShift,
          (ULONG)line->FgPen, (ULONG)line->BgPen, (ULONG)line->Horizontal, (ULONG)line->DrawMode, (ULONG)line->Xorigin,
          (ULONG)line->Yorigin, (ULONG)mask, (ULONG)fmt);

    if (fmt <= RGBFB_CLUT && mask != 0xFF) {
        D(WARN, "DrawLine fallback (CLUT mask)\n");
        waitBlitter();
        DrawLineDefault(this, ri, line, mask, AS_RGBF(fmt));
        return;
    }

    /* Patterned lines: fallback until PATTERN setup for 16-bit line pattern is implemented */
    if (line->LinePtrn != 0xFFFF) {
        D(WARN, "DrawLine fallback (patterned)\n");
        waitBlitter();
        DrawLineDefault(this, ri, line, mask, AS_RGBF(fmt));
        return;
    }

    ChipData_t *cd = chip();

    if (cd->chipFamily < AT24 && (UBYTE)fmt != cd->GEFormat) {
        waitBlitter();
        DrawLineDefault(this, ri, line, mask, AS_RGBF(fmt));
        return;
    }

    if (cd->GEFormat != fmt) {
        cd->GEbppLog2 = getBPPLog2((RGBFTYPE)fmt);
    }
    UBYTE bppLog2      = cd->GEbppLog2;
    ULONG addressModel = getAdressModelBits(ri, bppLog2);
    if (!addressModel) {
        waitBlitter();
        DrawLineDefault(this, ri, line, mask, AS_RGBF(fmt));
        return;
    }

    /* Horizontal and vertical: use FillRect. sDelta==0 means axis-aligned. */
    // RTG Library already makes that decision
    // if (line->sDelta == 0) {
    //     if (line->Horizontal) {
    //         fillRect( ri, line->X, line->Y, (WORD)line->Length, 1, line->FgPen, mask, fmt);
    //     } else {
    //         fillRect( ri, line->X, line->Y, 1, (WORD)line->Length, line->FgPen, mask, fmt);
    //     }
    //     return;
    // }

    /* Diagonal: use AT3D vector drawing with DDA */
    At3dMmio mmio = this->mmio();

    if (cd->GEOp != LINE) {
        cd->GEOp      = LINE;
        cd->GEdrawCmd = 0;
    }

    setDrawMode(line->DrawMode, line->FgPen, line->BgPen, (RGBFTYPE)fmt);

    /* DST_PITCH has no bearing in non-linear (XY) addressing model; omit. */
    // setDstPitch(ri->BytesPerRow);

    if (!setDstLocation(ri, (UWORD)line->X, (UWORD)line->Y, bppLog2, FALSE)) {
        waitBlitter();
        DrawLineDefault(this, ri, line, mask, AS_RGBF(fmt));
        return;
    }

    /* AT3D spec Table 11.4.1.2a: dmin=min(dx,dy), dmax=max(dx,dy).
     * P96: lDelta= major extent, sDelta= minor -> |lDelta|=dmax, |sDelta|=dmin. */
    WORD absMAX         = myabs(line->lDelta);
    WORD twoTimesAbsMIN = myabs(2 * line->sDelta);

    // mmio.writeW(DDA_AXIAL_STEP, (UWORD)(2 * absMIN));               /* 2*dmin */
    // mmio.writeW(DDA_DIAGONAL_STEP, ); /* (2*dmin)-(2*dmax) */
    mmio.writeL(DDA_AXIAL_STEP, makeDWORD(twoTimesAbsMIN - 2 * absMAX, twoTimesAbsMIN)); /* 2*dmin */
    mmio.writeW(DDA_ERROR_TERM, line->twoSDminusLD); /* P96 provides correct initial value */

    ULONG drawCmd = DRAW_CMD_OP(DRAW_CMD_VECTOR_ENDPOINT) | DRAW_QUICK_START(QUICKSTART_DIM_WIDTH) |
                    DRAW_PIXEL_DEPTH(bppLog2 + 1) | addressModel;

    /* M040[8] Major axis: set if dy>dx (Y-major), clear if X-major */
    if (!line->Horizontal) {
        drawCmd |= DRAW_MAJOR_AXIS_Y;
    }
    if (line->dX < 0) {
        drawCmd |= DRAW_DIR_X_NEGATIVE;
    }
    if (line->dY < 0) {
        drawCmd |= DRAW_DIR_Y_NEGATIVE;
    }

    // FIXME: if this is ever changed to a different order (no quickstart), make sure to at least
    //  clear DRAW_CMD as it may have a "quick start" still enabled from prior operations
    setDrawCmd(drawCmd);

    /* SRC_SIZE_X: Spec Dimension X = dmax + 1; P96 Length = dmax = max(|dx|,|dy|). */
    mmio.writeW(SRC_SIZE_X, line->Length + 1);
    return;
}

/* P96 BoardInfo entry stubs */

static BOOL ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    return asAt3d(bi)->setDisplay(state);
}
static BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL expected))
{
    return asAt3d(bi)->getVSyncState(expected);
}
static ULONG ASM GetVBeamPos(__REGA0(struct BoardInfo *bi))
{
    return asAt3d(bi)->getVBeamPos();
}
static LONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi),
                                  __REGD0(ULONG desiredPixelClock), __REGD7(RGBFTYPE rgbFormat))
{
    return asAt3d(bi)->resolvePixelClock(mi, desiredPixelClock, rgbFormat);
}
static void ASM SetClock(__REGA0(struct BoardInfo *bi))
{
    asAt3d(bi)->setClock();
}
static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD7(RGBFTYPE rgbFormat))
{
    return asAt3d(bi)->getPixelClock(mi, index, rgbFormat);
}
static UWORD ASM CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                      __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE rgbFormat))
{
    return asAt3d(bi)->calculateBytesPerRow(width, height, mi, rgbFormat);
}
static APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi), __REGA1(APTR mem), __REGD0(struct RenderInfo *ri),
                                __REGD7(RGBFTYPE format))
{
    return asAt3d(bi)->calculateMemory(mem, ri, format);
}
static ULONG ASM GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    return asAt3d(bi)->getCompatibleFormats(format);
}
static void ASM WaitBlitter(__REGA0(struct BoardInfo *bi))
{
    asAt3d(bi)->waitBlitter();
}
static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    __asm __volatile("\t movem.l d0-d1/a0-a1,-(sp)\n" : : :);
    asAt3d(bi)->setMemoryMode(format);
    __asm __volatile("\t movem.l (sp)+,d0-d1/a0-a1\n" : : : "d0", "d1", "a0", "a1");
}
static void ASM SetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    asAt3d(bi)->setGC(mi, border);
}
static void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD0(UWORD region), __REGD7(RGBFTYPE format))
{
    asAt3d(bi)->setDAC(region, format);
}
static void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    asAt3d(bi)->setColorArray(startIndex, count);
}
static void ASM SetPanning(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD3(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset),
                           __REGD7(RGBFTYPE format))
{
    asAt3d(bi)->setPanning(memory, width, height, xoffset, yoffset, format);
}
static void ASM SetDPMSLevel(__REGA0(struct BoardInfo *bi), __REGD0(ULONG level))
{
    asAt3d(bi)->setDPMSLevel(level);
}
static void ASM WaitVerticalSync(__REGA0(struct BoardInfo *bi), __REGD0(BOOL waitForEnd))
{
    asAt3d(bi)->waitVerticalSync(waitForEnd);
}
static BOOL ASM SetInterrupt(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    return asAt3d(bi)->setInterrupt(state);
}
extern "C" ULONG ASM interruptServer(__REGA1(struct BoardInfo *bi))
{
    return asAt3d(bi)->interruptServer();
}
static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    asAt3d(bi)->setWriteMask(mask);
}
static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    asAt3d(bi)->setClearMask(mask);
}
static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    asAt3d(bi)->setReadPlane(mask);
}
static void ASM SetSplitPosition(__REGA0(struct BoardInfo *bi), __REGD0(SHORT splitPos))
{
    asAt3d(bi)->setSplitPosition(splitPos);
}
static void ASM SetSpritePosition(__REGA0(struct BoardInfo *bi), __REGD0(WORD xpos), __REGD1(WORD ypos),
                                  __REGD7(RGBFTYPE fmt))
{
    asAt3d(bi)->setSpritePosition(xpos, ypos, fmt);
}
static void ASM SetSpriteImage(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE fmt))
{
    asAt3d(bi)->setSpriteImage(fmt);
}
static void ASM SetSpriteColor(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE index), __REGD1(UBYTE red),
                               __REGD2(UBYTE green), __REGD3(UBYTE blue), __REGD7(RGBFTYPE fmt))
{
    asAt3d(bi)->setSpriteColor(index, red, green, blue, fmt);
}
static BOOL ASM SetSprite(__REGA0(struct BoardInfo *bi), __REGD0(BOOL activate), __REGD7(RGBFTYPE RGBFormat))
{
    return asAt3d(bi)->setSprite(activate, RGBFormat);
}
static void ASM FillRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                         __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(ULONG pen),
                         __REGD5(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asAt3d(bi)->fillRect(ri, x, y, width, height, pen, mask, fmt);
}
static void ASM InvertRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                           __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                           __REGD7(RGBFTYPE fmt))
{
    asAt3d(bi)->invertRect(ri, x, y, width, height, mask, fmt);
}
static void ASM BlitRectNoMaskComplete(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *sri),
                                       __REGA2(struct RenderInfo *dri), __REGD0(WORD srcX), __REGD1(WORD srcY),
                                       __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                                       __REGD5(WORD height), __REGD6(UBYTE opCode), __REGD7(RGBFTYPE format))
{
    asAt3d(bi)->blitRectNoMaskComplete(sri, dri, srcX, srcY, dstX, dstY, width, height, opCode, format);
}
static void ASM BlitRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *sri), __REGD0(WORD srcX),
                         __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                         __REGD5(WORD height), __REGD6(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asAt3d(bi)->blitRect(sri, srcX, srcY, dstX, dstY, width, height, mask, fmt);
}
static void ASM BlitTemplate(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                             __REGA2(struct Template *tmpl), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                             __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asAt3d(bi)->blitTemplate(ri, tmpl, x, y, width, height, mask, fmt);
}
static void ASM BlitTemplate6422(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                                 __REGA2(struct Template *tmpl), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                                 __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asAt3d(bi)->blitTemplate6422(ri, tmpl, x, y, width, height, mask, fmt);
}
static void ASM BlitPlanar2Chunky(__REGA0(struct BoardInfo *bi), __REGA1(struct BitMap *bm),
                                  __REGA2(struct RenderInfo *ri), __REGD0(SHORT srcX), __REGD1(SHORT srcY),
                                  __REGD2(SHORT dstX), __REGD3(SHORT dstY), __REGD4(SHORT width), __REGD5(SHORT height),
                                  __REGD6(UBYTE minTerm), __REGD7(UBYTE mask))
{
    asAt3d(bi)->blitPlanar2Chunky(bm, ri, srcX, srcY, dstX, dstY, width, height, minTerm, mask);
}
static void ASM BlitPattern(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                            __REGA2(struct Pattern *pattern), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                            __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asAt3d(bi)->blitPattern(ri, pattern, x, y, width, height, mask, fmt);
}
static void ASM DrawLine(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGA2(struct Line *line),
                         __REGD0(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asAt3d(bi)->drawLine(ri, line, mask, fmt);
}

extern "C" BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    DFUNC(ALWAYS, "AT3D InitChip - Testing hardware access\n");

    LOCAL_SYSBASE();

    ChipData_t *cd = getChipData(bi);

    {
        VgaIo vga = asAt3d(bi)->legacyVga();

        // The older AT 6410 cards had the wakeup registers
        if (cd->chipFamily < AT24) {
            vga.writeB(VGA_ID(0x46E8), 0x10);
            vga.writeB(VGA_ID(0x102), 0x01);
            vga.writeB(VGA_ID(0x46E8), 0x08);
        }

        // Unlock extended registers
        vga.writeSR(0x10, 0x12);
        // vga.readSR(0x10);  // Debug read

        // Remap Control
        // Map HOST BLT port to last 32K of flat space less final 2K
        // Map ProMotion registers to  last 2K of flat space.
        vga.writeSRMask(0x1b, 0x3F, (0b100 << 3) | (0b100));

        // Flat Model Control
        // Enable flat memory access, set aperture to 4MB and disable VGA memory (A000:0–BFFF:F) access
        vga.writeSRMask(0x1c, 0x3F, 0b00111101);
        vga.readSR(0x1c);  // Dummy read to force flush of FIFO (is this the right way?)
    }

    At3dMmio mmio = asAt3d(bi)->mmio();

    if (getChipData(bi)->chipFamily >= AT24) {
#define LDEV_MASK (0x3 << 4)
#define LDEV(x)   ((x) << 4)

        // Enable extended registers, disable classic VGA IO range,
        // enable coprocessor aperture, enable second linear aperture
        mmio.writeB(ENABLE_EXT_REGS, 0x0F);
        // Doc say about the MMVGA address space:
        // LDEV wait states register field (0xD9[5:4]) must be programmed to value 2 in order to access this space.
        UBYTE extSigTiming = mmio.readB(EXTSIG_TIMING) & 0xC0;
        extSigTiming |= LDEV(2);  //  I have seen 0x59  used in the ROM
        mmio.writeB(EXTSIG_TIMING, extSigTiming);
    }

    if (!testMMIO(bi)) {
        D(ERROR, "MMIO test failed - cannot access MMIO window\n");
        return FALSE;
    }

    // From here on we can access the MMVGA window
    VgaIo vga = asAt3d(bi)->vga();

    char chipId[10] = {0};
    for (int c = 0; c < 9; ++c) {
        chipId[c] = vga.readSR(0x11 + c);
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
        vga.writeCR(CR_EXT_AUTORESET, 0x00);
        vga.writeCR(0x00, 0x00);
        vga.writeCR(CR_EXT_AUTORESET, CR_EXT_AUTORESET_DISABLE);
        // FIXME: something is off with this register. The readback sometimes returns 0x01, sometimes 0xff
        // and sometimes it seems to get reset to 0 later.
        while ((vga.readCR(CR_EXT_AUTORESET) & CR_EXT_AUTORESET_DISABLE) != CR_EXT_AUTORESET_DISABLE) {
            vga.writeCR(CR_EXT_AUTORESET, CR_EXT_AUTORESET_DISABLE);
        }
    }

    setDefaultClocks(bi);
    vga.writeMiscMask(0x0C, 0);  // Disable programmable VCLK

    mmio.writeMaskW(DISP_MEM_CFG, BIT(5),
                    BIT(5));  // 128bit  graphics engine access. Manual says this MUST be set.

    if (bi->MemorySize >= 2 * 1024 * 1024) {
        mmio.writeMaskW(DISP_MEM_CFG, BIT(8), BIT(8));  // 64bit memory bus
    }

    // Scratchpad registers (6422: 0x21–0x23 only; AT24+: 0x21–0x27)
    vga.readSR(0x20);
    int scratchEnd = (cd->chipFamily < AT24) ? 0x23 : 0x27;
    for (int i = 0x21; i <= scratchEnd; ++i) {
        vga.writeSR(i, 0x00);
    }
    mmio.writeB(ABORT, 0);
    mmio.writeB(COLOR_CORRECTION, BIT(4));  // 8bit per gun palette write (Does not seem to work?!)
    /* DAC_CTRL 0E4h bit0 = blanking pedestal; BLACKLEVEL=Black clears it. */
    mmio.writeB(DAC_CTRL, (bi->CardFlags & CFF_BLACKLEVEL_BLACK) ? 0 : BIT(0));
    if (cd->chipFamily >= AT24) {
        mmio.writeB(SIGANALYSER_CTRL, 0);  // Disable signal analyser
        mmio.writeB(FEATURE_CTRL, 0);
        mmio.writeL(VMI_PORT_CTRL, 0);
        mmio.writeB(THP_CTRL, 0);
        mmio.writeB(GPIO_CTRL, 0);
        mmio.writeB(OVERCURRENT_RED, 0);
        mmio.writeB(OVERCURRENT_GREEN, 0);
        mmio.writeB(OVERCURRENT_BLUE, 0);
    }

    mmio.writeB(MONITOR_INTERLACE_CTRL, 0x00);

    // Force 8 Dot, force Graphics mode, force VCLK PLL,
    UWORD vgaOverride = BIT(5) | BIT(6) | BIT(7) | BIT(9);
    if (cd->chipFamily >= AT24) {
        vgaOverride |= BIT(12);  //  disable VGA IO; 6422 doesn't have the MMVGA regions, thus we still need VGA IO
    }
    mmio.writeW(VGA_OVERRIDE, vgaOverride);

    // Enable extended VGA Modes
    mmio.writeB(SERIAL_CTRL, BIT(6) | DESKTOP_DEPTH_8BPP | DESKTOP_FORMAT_INDEXED);  //

    mmio.writeMaskB(APERTURE_CTRL, PALETTE_ACCESS_MASK, PALETTE_ACCESS(0b01));  // Disable RAMDAC snooping
    mmio.writeB(DPMS_SYNC_CTRL, 0x00);  // Clear bits [1:0] to enable both HSYNC and VSYNC
    mmio.writeL(PIXEL_FIFO_REQ_POINT, (0x16 << 16) | (16 << 8) | 16);

    // mmio.writeB(0xDA, 0x00);  // This used to be an "Internal Register" on older 6210 cards(?)

    mmio.writeW(DISP_MEM_CFG, 0x0520);  // Single Cycle Page Mode, mem64, 128bit gfx access

    mmio.writeB(VCLK_CTRL, 0x5C);
    mmio.writeB(VCLK_DEN, 0x01);
    mmio.writeB(VCLK_NUM, 0x1b);

    vga.writeMiscMask(0xF,
                      0b1111);  // enable Host Memory Access, programmable VCLK, Color Mode (3DA, 3D4 and 3D5 enabled))

    {
        // Enable writing attribute palette registers, disable video
        vga.readB(VgaReg::INPUT_STATUS1);
        vga.writeB(VgaReg::ATTR_AD, 0x0);

        // Reset AFF to index register selection
        vga.readB(VgaReg::INPUT_STATUS1);

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

        vga.writeAR(0x10, 0x61);  // 256color mode, separate pixel panning, graphics mode

        // Enable video
        vga.readB(VgaReg::INPUT_STATUS1);   // reset AFF
        vga.writeB(VgaReg::ATTR_AD, 0x20);  // enable video
    }

    // Setup 8 pixels per DCLK, screen off
    vga.writeSR(0x01, BIT(5) | SR_CHAR_CLOCK_8_DOT);
    vga.writeSR(0x03, 0x00);
    vga.writeSR(0x04, 0x06);  // >64k present, unchained mode

    vga.writeGR(0x00, 0x00);
    vga.writeGR(0x01, 0x00);
    vga.writeGR(0x02, 0x00);
    vga.writeGR(0x03, 0x00);
    vga.writeGR(0x04, 0x00);
    vga.writeGR(0x05, 0x00);
    vga.writeGR(0x06, 0x01);
    vga.writeGR(0x07, 0x0F);
    vga.writeGR(0x08, 0xFF);

    vga.readCR(CR_EXT_AUTORESET);

    vga.writeCR(0x08, 0x00);
    vga.writeCR(0x09, 0x00);
    vga.writeCR(0x0A, 0x00);
    vga.writeCR(0x0B, 0x00);
    vga.writeCR(0x0C, 0x00);  // "Serial Start" = Start address of screen to 0
    vga.writeCR(0x0D, 0x00);
    vga.writeCR(0x0E, 0x00);
    vga.writeCR(0x0F, 0x00);

    vga.writeCR(0x11, 0x20);  // Disable Vertical Interrupt, ack. Interrupt
    vga.writeCR(0x11, 0x30);  // Normal retrace

    vga.writeCR(0x1C, 0x00);
    vga.writeCR(0x13, 0x50);    // "Serial Offset" = pitch 80 * 8 = 640 byte
    vga.writeCR(0x14, BIT(6));  // Enable "Double Word mode"  CRTC display memory addresses incremented by 4
    vga.writeCR(0x17, BIT(7) | BIT(6) | BIT(5) | BIT(0));  // CRTC  Mode Control Register, enable HSYNC and VSYNC
    vga.writeCR(0x18, 0xff);                               // Line compare register

    vga.readCR(CR_EXT_AUTORESET);

    vga.writeB(VgaReg::DAC_PEL_MASK, 0xFF);

    UBYTE miscOut = vga.readB(VgaReg::MISC_OUT_R);
    D(INFO, "Monitor is %s present (may be inaccurate)\n", (miscOut & 0x10) ? "" : "not");

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
    bi->Flags = bi->Flags | BIF_GRANTDIRECTACCESS | BIF_VGASCREENSPLIT | BIF_HARDWARESPRITE | BIF_BLITTER |
                BIF_DBLSCANDBLSPRITEY;

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
    // All modes: 4096 pixels max in "Horizontal Total" register (511 * 8 dclks)
    bi->MaxHorValue[PLANAR]    = 4093;  // 511 * 8 + 5 dclks
    bi->MaxHorValue[CHUNKY]    = 4093;
    bi->MaxHorValue[HICOLOR]   = 4093;
    bi->MaxHorValue[TRUECOLOR] = 4093;
    bi->MaxHorValue[TRUEALPHA] = 4093;

    // Maximum vertical values (11 Bit "Vertical Total", 2047 scanlines)
    // This _could_ be stretched to * 2 via vertical doubling, but its not implemented
    bi->MaxVerValue[PLANAR]    = 2047;
    bi->MaxVerValue[CHUNKY]    = 2047;
    bi->MaxVerValue[HICOLOR]   = 2047;
    bi->MaxVerValue[TRUECOLOR] = 2047;
    bi->MaxVerValue[TRUEALPHA] = 2047;

    // Maximum resolution per format type
    // 12 Bit * 8 Serial Offset = max 32760byte pitch
    // 12 Bit Blitter coordinates and Clip coordinates: 4095
    bi->MaxHorResolution[PLANAR] = 4096;
    bi->MaxVerResolution[PLANAR] = 4096;

    bi->MaxHorResolution[CHUNKY] = 4096;
    bi->MaxVerResolution[CHUNKY] = 4096;

    bi->MaxHorResolution[HICOLOR] = 4096;
    bi->MaxVerResolution[HICOLOR] = 4096;

    bi->MaxHorResolution[TRUECOLOR] = 4096;
    bi->MaxVerResolution[TRUECOLOR] = 4096;

    bi->MaxHorResolution[TRUEALPHA] = 4096;
    bi->MaxVerResolution[TRUEALPHA] = 4096;

    // Set function pointers
    P96_HOOK(bi->SetGC, SetGC);
    P96_HOOK(bi->SetPanning, SetPanning);
    P96_HOOK(bi->CalculateBytesPerRow, CalculateBytesPerRow);
    P96_HOOK(bi->CalculateMemory, CalculateMemory);
    P96_HOOK(bi->GetCompatibleFormats, GetCompatibleFormats);
    P96_HOOK(bi->SetDAC, SetDAC);
    P96_HOOK(bi->SetColorArray, SetColorArray);
    P96_HOOK(bi->SetDisplay, SetDisplay);
    P96_HOOK(bi->SetMemoryMode, SetMemoryMode);
    P96_HOOK(bi->ResolvePixelClock, ResolvePixelClock);
    P96_HOOK(bi->GetPixelClock, GetPixelClock);
    P96_HOOK(bi->SetClock, SetClock);
    P96_HOOK(bi->SetWriteMask, SetWriteMask);
    P96_HOOK(bi->SetReadPlane, SetReadPlane);
    P96_HOOK(bi->SetClearMask, SetClearMask);
    P96_HOOK(bi->GetVSyncState, GetVSyncState);
    P96_HOOK(bi->GetVBeamPos, GetVBeamPos);
    P96_HOOK(bi->WaitVerticalSync, WaitVerticalSync);
    P96_HOOK(bi->SetInterrupt, SetInterrupt);
    bi->HardInterrupt.is_Code = (void (*)())interruptServerTrampoline;

    P96_HOOK(bi->SetDPMSLevel, SetDPMSLevel);
    P96_HOOK(bi->SetSplitPosition, SetSplitPosition);
    P96_HOOK(bi->SetSprite, SetSprite);
    P96_HOOK(bi->SetSpritePosition, SetSpritePosition);
    P96_HOOK(bi->SetSpriteImage, SetSpriteImage);
    P96_HOOK(bi->SetSpriteColor, SetSpriteColor);
    P96_HOOK(bi->WaitBlitter, WaitBlitter);
    P96_HOOK(bi->FillRect, FillRect);
    P96_HOOK(bi->InvertRect, InvertRect);
    P96_HOOK(bi->BlitRectNoMaskComplete, BlitRectNoMaskComplete);
    P96_HOOK(bi->BlitRect, BlitRect);
    if (cd->chipFamily >= AT24) {
        P96_HOOK(bi->BlitTemplate, BlitTemplate);
        P96_HOOK(bi->BlitPlanar2Chunky, BlitPlanar2Chunky);
    } else {
        P96_HOOK(bi->BlitTemplate, BlitTemplate6422);
    }
    P96_HOOK(bi->DrawLine, DrawLine);
    /* 8x8 pattern cache for BlitPattern (screen-space aligned, pre-rotated upload).
     * Allocate 8 lines of 16bit (Amiga patterns are 16bit wide). At runtime we compare the
     * incoming pattern to the one we
     */
    cd->patternCacheBuffer = (UWORD *)AllocVec(8 * sizeof(UWORD), MEMF_PUBLIC);
    if (cd->patternCacheBuffer) {
        cd->patternCacheKey = ~0UL;

        P96_HOOK(bi->BlitPattern, BlitPattern);
    }

    cd->GEfgPen = 0x12345678;
    cd->GEbgPen = 0x12345678;

    SetSplitPosition(bi, 0);

    /* Hardware cursor: take cursor image data off the top of the memory; cursor at 1 KB segment boundary.
     * 6422: also reserve 1KB for BlitTemplate6422 mono staging (below cursor). */
    {
        ULONG maxCursorBufferSize = (64 * 64 * 2 / 8); /* 64x64 at 2 bpp = 1024 bytes */
        At3dMmio mmio             = asAt3d(bi)->mmio();

        bi->MemorySize       = (bi->MemorySize - maxCursorBufferSize) & ~(1024 - 1);
        bi->MouseImageBuffer = bi->MemoryBase + bi->MemorySize;

        // DFUNC(INFO, "Cursor offset %ld\n", bi->MemorySize);

        mmio.writeB(HW_CURSOR_CTRL, 0);
        mmio.writeW(HW_CURSOR_BASE, (UWORD)(bi->MemorySize >> 10));
        mmio.writeW(HW_CURSOR_X, 0);
        mmio.writeW(HW_CURSOR_Y, 0);
        mmio.writeB(HW_CURSOR_OFF_X, 0);
        mmio.writeB(HW_CURSOR_OFF_Y, 0);
    }

    if (cd->chipFamily < AT24) {
        ULONG stagingSize         = 1024; /* +1KB template staging for 6422 */
        cd->templateStagingOffset = (bi->MemorySize - stagingSize) & ~7;
        bi->MemorySize            = cd->templateStagingOffset;
    }

    mmio.writeB(BYTE_MASK, 0xFF);

    mmio.writeW(CLIP_CTRL, BIT(0));
    mmio.writeW(CLIP_LEFT, 0);
    mmio.writeW(CLIP_TOP, 0);
    mmio.writeW(CLIP_RIGHT, 0xFFF);
    mmio.writeW(CLIP_BOTTOM, 0xFFF);

    ULONG memClk = bi->MemoryClock;
    if (memClk < 45000000) {
        memClk = 45000000;
    } else if (memClk > 75000000) {
        memClk = 75000000;
    }
    setMemoryClock(bi, memClk);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(TESTEXE) && !defined(AT3D_EMBEDDED_CHIP)

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
    bi->UtilBase = (struct Library *)UtilityBase;

    D(INFO, "AT3D Test Executable\n");
    D(INFO, "UtilityBase 0x%lx\n", bi->UtilBase);

    struct pci_dev *board = NULL;
    CardData_t *card      = getCardData(bi);

    // Open openpci library
    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        DFUNC(ERROR, "Cannot open openpci.library v%ld+\n", MIN_OPENPCI_VERSION);
        goto exit;
    }

    card->OpenPciBase = OpenPciBase;

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
        bi->SetClock(bi);

        DFUNC(ALWAYS, "SetGC\n");
        bi->SetGC(bi, &mi, FALSE);

        DFUNC(ALWAYS, "SetDAC\n");
        bi->SetDAC(bi, 0, RGBFB_CLUT);

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
        bi->SetColorArray(bi, 0, 256);

        DFUNC(ALWAYS, "SetPanning\n");
        bi->SetPanning(bi, bi->MemoryBase, 640, 480, 0, 0, RGBFB_CLUT);

        DFUNC(ALWAYS, "SetDisplay ON\n");
        bi->SetDisplay(bi, TRUE);

        DFUNC(ALWAYS, "SetDPMSLevel  ON\n");
        bi->SetDPMSLevel(bi, DPMS_ON);
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
            bi->FillRect(bi, &ri, 100, 100, 640 - 200, 480 - 200, 0xFF, 0xFF, RGBFB_CLUT);
        }

        {
            bi->InvertRect(bi, &ri, 0, 20, 640, 40, 0xFF, RGBFB_CLUT);
        }

        {
            bi->FillRect(bi, &ri, 64, 64, 128, 128, 0xAA, 0xFF, RGBFB_CLUT);
        }

        {
            bi->FillRect(bi, &ri, 256, 10, 128, 128, 0x33, 0xFF, RGBFB_CLUT);
        }

        {
            bi->InvertRect(bi, &ri, 100, 100, 640 - 200, 480 - 200, 0xFF, RGBFB_CLUT);
        }

        /* BlitRectNoMaskComplete: fill a source rect, then copy it to another position (same buffer). */
        {
            bi->FillRect(bi, &ri, 50, 200, 80, 80, 0x55, 0xFF, RGBFB_CLUT); /* source rect */
            bi->BlitRectNoMaskComplete(bi, &ri, &ri, 50, 200, 300, 200, 80, 80, 0xc,
                                       RGBFB_CLUT); /* copy: minterm 0xC0 */
        }

        /* BlitRectNoMaskComplete with overlapping source and destination (copy 120x40 from 50,350 to 100,350). */
        {
            bi->FillRect(bi, &ri, 50, 350, 120, 40, 0x77, 0xFF, RGBFB_CLUT); /* source rect */
            bi->BlitRectNoMaskComplete(bi, &ri, &ri, 50, 350, 100, 350, 120, 40, 0xc,
                                       RGBFB_CLUT); /* overlapping copy */
        }

        bi->BlitRectNoMaskComplete(bi, &ri, &ri, 0, 0, 0, 240, 640, 100, 0xc, RGBFB_CLUT);
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

        if (bi->BlitTemplate) {
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
                    bi->FillRect(bi, &ri, i * 32 + i, 0, 32, 32, 50, 0xFF, RGBFB_CLUT);
                    bi->BlitTemplate(bi, &ri, &tmpl, i * 32 + i, 0, 32, 32, 0xFF, RGBFB_CLUT);
                }
            }
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
                    tmpl.BgPen       = BLIT_TMPL_PEN_FG;  // force the driver to work around the transparency test
                    bi->FillRect(bi, &ri, 32 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                    bi->BlitTemplate(bi, &ri, &tmpl, 32 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
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
                    bi->FillRect(bi, &ri, 96 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                    bi->BlitTemplate(bi, &ri, &tmpl, 96 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
                }

                /* BlitTemplate COMPLEMENT: flip destination where pattern is 1 (blue<->red on checker). */
                {
                    struct Template tmpl;
                    tmpl.Memory      = template32;
                    tmpl.BytesPerRow = 4;
                    tmpl.XOffset     = i * 2;
                    tmpl.DrawMode    = COMPLEMENT;
                    tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                    tmpl.BgPen       = BLIT_TMPL_PEN_FG;  // force the driver to work around the transparency test
                    bi->FillRect(bi, &ri, 144 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                    bi->BlitTemplate(bi, &ri, &tmpl, 144 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
                }
                /* BlitTemplate JAM1 | INVERSVID: pens swapped -> pattern 1 -> BgPen (blue), 0 -> D. */
                {
                    struct Template tmpl;
                    tmpl.Memory      = template32;
                    tmpl.BytesPerRow = 4;
                    tmpl.XOffset     = i * 2;
                    tmpl.DrawMode    = JAM1 | INVERSVID;
                    tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                    tmpl.BgPen       = BLIT_TMPL_PEN_FG;  // force the driver to work around the transparency test
                    bi->FillRect(bi, &ri, 192 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                    bi->BlitTemplate(bi, &ri, &tmpl, 192 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
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
                    bi->FillRect(bi, &ri, 240 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                    bi->BlitTemplate(bi, &ri, &tmpl, 240 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
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
                    bi->FillRect(bi, &ri, 288 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                    bi->BlitTemplate(bi, &ri, &tmpl, 288 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
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
                bi->FillRect(bi, &ri, 422, 262, 32, 32, 0xFF, 0xFF, RGBFB_CLUT);
                bi->BlitTemplate(bi, &ri, &tmpl, 422, 262, 32, 32, 0xFF, RGBFB_CLUT);

                // Edge case: while the blit itself is inside the640x480 rectangle, artificially increasing the
                // blit width to 32 pixels (617+32 = 649).
                bi->FillRect(bi, &ri, 627, 262, 20, 32, 0xFF, 0xFF, RGBFB_CLUT);
                bi->BlitTemplate(bi, &ri, &tmpl, 627, 262, 20, 32, 0xFF, RGBFB_CLUT);
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
                bi->FillRect(bi, &ri, 94, 57, 46, 11, 0xFF, 0xFF, RGBFB_CLUT);
                bi->BlitTemplate(bi, &ri, &tmpl46, 94, 57, 46, 11, 0xFF, RGBFB_CLUT);
            }
        }

#if 1
        /* BlitPattern: 8x8 mono pattern (PATTERN register). Red/blue checkerboard; one test per DrawMode. */
        if (bi->BlitPattern) {
            /* 8x8 pattern: checkerboard. Each row 8 bits repeated in low and high byte (0xXXXX) for 8x8 path. */
            static const UWORD pattern8x8[8] = {
                (UWORD)0x3333, (UWORD)0x5555, (UWORD)0xAAAA, (UWORD)0xCCCC,
                (UWORD)0xAAAA, (UWORD)0x5555, (UWORD)0xAAAA, (UWORD)0x5555,
            };
            struct Pattern pat;
            pat.Memory  = (APTR)pattern8x8;
            pat.XOffset = 0;
            pat.YOffset = 0;
            pat.FgPen   = 1; /* red */
            pat.BgPen   = 0; /* blue */
            pat.Size    = 3; /* 2^3 = 8 rows */
/* Base position for DrawMode row: (20, 400), each cell 48x48, gap 4 -> next at +52 */
#define PAT_CELL_W 48
#define PAT_CELL_H 48
#define PAT_STRIDE 52
#define PAT_LEFT   20
#define PAT_TOP    400

            /* JAM1: pattern 1 -> FgPen (red), pattern 0 -> keep D (blue). */
            pat.DrawMode = JAM1;
            pat.FgPen    = 1;
            pat.BgPen    = 0;

            bi->FillRect(bi, &ri, PAT_LEFT, PAT_TOP, PAT_CELL_W * 5, PAT_CELL_H, 0, 0xFF, RGBFB_CLUT);
            bi->BlitPattern(bi, &ri, &pat, PAT_LEFT, PAT_TOP, PAT_CELL_W, PAT_CELL_H, 0xFF, RGBFB_CLUT);

            /* JAM2: pattern 1 -> FgPen (red), pattern 0 -> BgPen (blue). */
            pat.DrawMode = JAM2;
            bi->BlitPattern(bi, &ri, &pat, PAT_LEFT + PAT_STRIDE, PAT_TOP, PAT_CELL_W, PAT_CELL_H, 0xFF, RGBFB_CLUT);

            /* COMPLEMENT: flip destination where pattern is 1 (blue<->red). */
            pat.DrawMode = COMPLEMENT;
            bi->BlitPattern(bi, &ri, &pat, PAT_LEFT + PAT_STRIDE * 2, PAT_TOP, PAT_CELL_W, PAT_CELL_H, 0xFF,
                            RGBFB_CLUT);

            /* JAM1 | INVERSVID: pens swapped -> pattern 1 -> BgPen (blue), 0 -> keep D. */
            pat.DrawMode = JAM1 | INVERSVID;
            bi->BlitPattern(bi, &ri, &pat, PAT_LEFT + PAT_STRIDE * 3, PAT_TOP, PAT_CELL_W, PAT_CELL_H, 0xFF,
                            RGBFB_CLUT);

            /* JAM2 | INVERSVID: pattern 1 -> BgPen (blue), pattern 0 -> FgPen (red). */
            pat.DrawMode = JAM2 | INVERSVID;
            bi->BlitPattern(bi, &ri, &pat, PAT_LEFT + PAT_STRIDE * 4, PAT_TOP, PAT_CELL_W, PAT_CELL_H, 0xFF,
                            RGBFB_CLUT);

            /* COMPLEMENT | INVERSVID: invert where pattern is 1 (same as COMPLEMENT on blue bg). */
            pat.DrawMode = COMPLEMENT | INVERSVID;
            bi->BlitPattern(bi, &ri, &pat, PAT_LEFT + PAT_STRIDE * 5, PAT_TOP, PAT_CELL_W, PAT_CELL_H, 0xFF,
                            RGBFB_CLUT);

#undef PAT_CELL_W
#undef PAT_CELL_H
#undef PAT_STRIDE
#undef PAT_LEFT
#undef PAT_TOP

            /* Offset test: pattern at (123, 403) so pattOffX=3, pattOffY=3. */
            bi->FillRect(bi, &ri, 120, 455, 48, 48, 0, 0xFF, RGBFB_CLUT);
            pat.DrawMode = JAM2;
            pat.XOffset  = 0;
            pat.YOffset  = 0;
            bi->BlitPattern(bi, &ri, &pat, 123, 458, 48, 48, 0xFF, RGBFB_CLUT);
        }
#endif

#if 1
        /* DrawLine: horizontal, vertical, and diagonal (X-major, Y-major, both directions). */
        if (bi->DrawLine) {
            struct Line line;
            line.LinePtrn     = 0xFFFF; /* solid */
            line.PatternShift = 0;
            line.FgPen        = 2; /* green */
            line.BgPen        = 0;
            line.DrawMode     = JAM2;
            line.Xorigin      = 0;
            line.Yorigin      = 0;

            /* Clear a region for lines and draw a light background */
            bi->FillRect(bi, &ri, 10, 400, 300, 70, 50, 0xFF, RGBFB_CLUT);

            /* Horizontal line (100, 420) -> (250, 420), 150 pixels */
            line.X          = 100;
            line.Y          = 420;
            line.Length     = 150;
            line.dX         = 1;
            line.dY         = 0;
            line.sDelta     = 0;
            line.lDelta     = 150;
            line.Horizontal = TRUE;
            bi->DrawLine(bi, &ri, &line, 0xFF, RGBFB_CLUT);

            /* Vertical line (170, 410) -> (170, 460), 50 pixels */
            line.X          = 170;
            line.Y          = 410;
            line.Length     = 50;
            line.dX         = 0;
            line.dY         = 1;
            line.sDelta     = 0;
            line.lDelta     = 50;
            line.Horizontal = FALSE;
            bi->DrawLine(bi, &ri, &line, 0xFF, RGBFB_CLUT);

            /* X-major diagonal: (20, 450) down-right, 60 pixels in X, 30 in Y */
            line.X          = 20;
            line.Y          = 450;
            line.Length     = 60;
            line.dX         = 1;
            line.dY         = 1;
            line.sDelta     = 30;
            line.lDelta     = 60;
            line.Horizontal = TRUE;
            bi->DrawLine(bi, &ri, &line, 0xFF, RGBFB_CLUT);

            /* Y-major diagonal: (110, 410) down-right, 30 pixels in X, 60 in Y */
            line.X          = 110;
            line.Y          = 410;
            line.Length     = 60;
            line.dX         = 1;
            line.dY         = 1;
            line.sDelta     = 30;
            line.lDelta     = 60;
            line.Horizontal = FALSE;
            bi->DrawLine(bi, &ri, &line, 0xFF, RGBFB_CLUT);

            /* Diagonal with negative X: (250, 415) down-left */
            line.X          = 250;
            line.Y          = 415;
            line.Length     = 40;
            line.dX         = -1;
            line.dY         = 1;
            line.sDelta     = 20;
            line.lDelta     = 40;
            line.Horizontal = TRUE;
            bi->DrawLine(bi, &ri, &line, 0xFF, RGBFB_CLUT);

            /* Diagonal with negative Y: (290, 455) up-right */
            line.X          = 290;
            line.Y          = 455;
            line.Length     = 40;
            line.dX         = 1;
            line.dY         = -1;
            line.sDelta     = 20;
            line.lDelta     = 40;
            line.Horizontal = TRUE;
            bi->DrawLine(bi, &ri, &line, 0xFF, RGBFB_CLUT);

            /* Full-screen diagonal (0,11) -> (639,479): Length 639, dX 639, dY 468, twoSDminusLD 297 */
            line.X            = 0;
            line.Y            = 11;
            line.Length       = 639;
            line.dX           = 639;
            line.dY           = 468;
            line.sDelta       = 468;
            line.lDelta       = 639;
            line.twoSDminusLD = 297;
            line.LinePtrn     = 0xFFFF;
            line.PatternShift = 15;
            line.FgPen        = 0;
            line.BgPen        = 0;
            line.Horizontal   = TRUE;
            line.DrawMode     = 0x81;
            line.Xorigin      = 0;
            line.Yorigin      = 11;
            bi->DrawLine(bi, &ri, &line, 0xFF, RGBFB_CLUT);
        }

        bi->WaitBlitter(bi);
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

