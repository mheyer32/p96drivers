#include "mach64_common.h"
#include "chip_mach64.h"

#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/openpci.h>

using namespace MmioReg;
using namespace PllReg;

#ifdef __cplusplus
extern "C" {
#endif

ChipFamily_t getChipFamily(UWORD deviceId)
{
    switch (deviceId) {
    case 0x5654:  // mach64 VT
        return MACH64VT;
    case 0x4758:  // mach64 GX
        return MACH64GX;
    case 0x4354:  // mach64 CT (GX-class bus; integrated DAC/PLL)
        return MACH64CT;
    case 0x4749:  // mach64 Rage Pro
        return MACH64GT;
    case 0x4750:  // mach64 Rage Pro
        return MACH64GT;
    case 0x4752:  // mach64 Rage 3 XL
        return MACH64GM;
    default:
        return UNKNOWN;
    }
}

const char *getChipFamilyName(ChipFamily_t family)
{
    switch (family) {
    case MACH64VT:
        return "Mach64 VT";
    case MACH64GX:
        return "Mach64 GX";
    case MACH64CT:
        return "Mach64 CT";
    case MACH64GT:
        return "Mach64 GT (Rage Pro)";
    case MACH64GM:
        return "Mach64 GR (Rage3 XL)";
    default:
        return "Unknown";
    }
}

UBYTE ReadPLL(BoardInfo_t *bi, PllReg::Id pllAddr)
{
    DRIVER_LOCALS(bi);
    // FIXME: its possible older Mach chips want 8bit access here
    ULONG clockCntl = mmio.readL(CLOCK_CNTL) & ~(PLL_ADDR_MASK | PLL_DATA_MASK | PLL_WR_ENABLE_MASK);

    // Set PLL Adress
    clockCntl |= PLL_ADDR(pllAddr);
    mmio.writeL(CLOCK_CNTL, clockCntl);
    // Read back data
    clockCntl = mmio.readL(CLOCK_CNTL);

    UBYTE pllValue = (clockCntl >> 16) & 0xFF;

    DFUNC(VERBOSE, "pllAddr: %ld, pllValue: 0x%02lX\n", (ULONG)pllAddr, (ULONG)pllValue);

    return pllValue;
}

void WritePLL(BoardInfo_t *bi, PllReg::Id pllAddr, UBYTE pllDataMask, UBYTE pllData)
{
    DRIVER_LOCALS(bi);
    UBYTE addr = (UBYTE)pllAddr;

    // testing byte access
    mmio.writeB(CLOCK_CNTL, CLOCK_CNTL_ADDR, addr << 2);
    UBYTE oldValue = mmio.readB(CLOCK_CNTL, CLOCK_CNTL_DATA);
    UBYTE newValue = (oldValue & ~pllDataMask) | (pllData & pllDataMask);
    mmio.writeB(CLOCK_CNTL, CLOCK_CNTL_ADDR, (addr << 2) | 0x02);  // PLL_WR_EN
    mmio.writeB(CLOCK_CNTL, CLOCK_CNTL_DATA, newValue);
    mmio.writeB(CLOCK_CNTL, CLOCK_CNTL_ADDR, 0x00);
}

ULONG computeFrequencyKhz10(UWORD RefFreq, UWORD FBDiv, UWORD RefDiv, UBYTE PostDiv)
{
    if (!RefDiv || !PostDiv) {
        return 0;
    }
    return ((ULONG)2 * RefFreq * FBDiv) / (RefDiv * PostDiv);
}

ULONG computeFrequencyKhz10FromPllValue(const BoardInfo_t *bi, const PLLValue_t *pllValues, const UBYTE *postDividers)
{
    const ChipSpecific_t *cs = getConstChipSpecific(bi);
    UBYTE postDiv            = postDividers[pllValues->Pidx];
    if (!cs->referenceDivider || !postDiv) {
        return 0;
    }
    return computeFrequencyKhz10(cs->referenceFrequency, pllValues->N, cs->referenceDivider, postDiv);
}

static BOOL inline isGoodVCOFrequency(const BoardInfo_t *bi, ULONG freqKhz10)
{
    /* Appendix J: CT/ET VCO 68–135 MHz; later CT-family / GT use ~100–235 MHz. */
    if (getConstChipData(bi)->chipFamily == MACH64CT)
        return freqKhz10 >= 6800 && freqKhz10 <= 13500;
    return freqKhz10 >= 10000 && freqKhz10 <= 23500;
}

#ifndef MACH64_PCLK_MIN_FLOOR
/* 0 = ROM minPClock (allows ~12.6 MHz 320x200); non-zero = hard floor in 10kHz units. */
#define MACH64_PCLK_MIN_FLOOR 0
#endif

ULONG computePLLValues(const BoardInfo_t *bi, ULONG freqKhz10, const UBYTE *postDividers, WORD numPostDividers,
                       PLLValue_t *pllValues)
{
    DFUNC(CHATTY, "targetFrequency: %ld0 KHz\n", freqKhz10);

    const ChipSpecific_t *cs = getConstChipSpecific(bi);
    UWORD M                  = cs->referenceDivider;
    UWORD R                  = cs->referenceFrequency;

    /* 0 = use ROM minPClock (repro for low-floor regression); else hard floor in 10kHz units. */
    ULONG floor = MACH64_PCLK_MIN_FLOOR;
    if (floor == 0) {
        floor = cs->minPClock ? cs->minPClock : 1;
    }
    if (freqKhz10 < floor) {
        freqKhz10 = floor;
    }
    if (freqKhz10 > 23500) {
        freqKhz10 = 23500;
    }

    // T = 2 * R * N / (M * P)
    // N = T * M * P / (2 * R)

    UBYTE bestPostDivIdx = 0;
    ULONG bestError      = ~0UL;  // Maximum error
    UBYTE bestN          = 0;
    BOOL foundValid      = FALSE;

    // Try all post dividers to find the best match (largest index first)
    for (WORD i = numPostDividers - 1; i >= 0; --i) {
        UBYTE P = postDividers[i];

        // Calculate N = (T * M * P) / (2 * R)
        ULONG N = (freqKhz10 * M * P + R - 1) / (2 * R);

        // Check if N is in valid range (128-255)
        if (N < 128 || N > 255) {
            D(TELLALL, "Post divider %ld: N=%ld out of range\n", (ULONG)i, N);
            continue;  // Skip this post divider
        }

        // Check VCO frequency (before post divider)
        ULONG vcoFreq = freqKhz10 * P;
        if (!isGoodVCOFrequency(bi, vcoFreq)) {
            D(TELLALL, "Post divider %ld: VCO frequency %ld0 KHz out of range\n", (ULONG)i, vcoFreq);
            continue;  // Skip if VCO out of range
        }

        // Calculate actual output frequency
        ULONG actualFreq = computeFrequencyKhz10(R, (UBYTE)N, M, P);

        // Calculate error (absolute difference)
        ULONG error = (actualFreq > freqKhz10) ? (actualFreq - freqKhz10) : (freqKhz10 - actualFreq);

        D(TELLALL, "Post divider %ld (P=%ld): N=%ld, actual=%ld0 KHz, error=%ld0 KHz\n", (ULONG)i, (ULONG)P, N,
          actualFreq, error);

        // Keep track of best match
        if (error < bestError) {
            bestError      = error;
            bestPostDivIdx = i;
            bestN          = (UBYTE)N;
            foundValid     = TRUE;
        }
    }

    if (!foundValid) {
        DFUNC(ERROR, "No valid PLL combination found for %ld0 KHz\n", freqKhz10);
        return 0;
    }

    pllValues->N    = bestN;
    pllValues->Pidx = bestPostDivIdx;

    ULONG outputFreq = computeFrequencyKhz10(R, pllValues->N, M, postDividers[pllValues->Pidx]);

    D(CHATTY, "target: %ld0 KHz, Output: %ld0 KHz, R: %ld0 KHz, M: %ld, P: %ld, N: %ld, error: %ld0 KHz\n",
      (ULONG)freqKhz10, (ULONG)outputFreq, (ULONG)R, (ULONG)M, (ULONG)postDividers[pllValues->Pidx],
      (ULONG)pllValues->N, (ULONG)bestError);

    return outputFreq;
}

void InitVClockPLLTable(BoardInfo_t *bi, const BYTE *multipliers, BYTE numMultipliers)
{
    DFUNC(VERBOSE, "\n", bi);

    LOCAL_SYSBASE();

    const ChipData_t *cd = getConstChipData(bi);
    ChipSpecific_t *cs   = getChipSpecific(bi);

    if (cs->maxPClock <= cs->minPClock) {
        DFUNC(ERROR, "invalid PCLK range min=%ld max=%ld\n", (ULONG)cs->minPClock, (ULONG)cs->maxPClock);
        return;
    }

    /* One entry per minFreq step, plus room for the optional maxPClock squeeze. */
    UWORD maxNumEntries = (UWORD)((cs->maxPClock - cs->minPClock) / 100u + 2u);

    D(VERBOSE, "Number of Pixelclocks %ld (PCLK %ld..%ld)\n", (ULONG)maxNumEntries, (ULONG)cs->minPClock,
      (ULONG)cs->maxPClock);

    // FIXME: there's no free... is there ever a time a chip driver gets expunged?
    PLLValue_t *pllValues = static_cast<PLLValue_t *>(AllocVec(sizeof(PLLValue_t) * maxNumEntries, MEMF_PUBLIC));
    if (!pllValues) {
        DFUNC(ERROR, "AllocVec pllValues failed\n");
        return;
    }
    cs->vclkPllValues = pllValues;

    UWORD minFreq    = cs->minPClock;
    UWORD maxFreq    = cs->maxPClock;
    UWORD e          = 0;
    UWORD failStreak = 0;
    while (minFreq < maxFreq && e < maxNumEntries) {
        ULONG frequency =
            computePLLValues(bi, minFreq, reinterpret_cast<const UBYTE *>(multipliers), numMultipliers, &pllValues[e]);
        if (!frequency) {
            /* Gaps exist between post-div / VCO windows (esp. CT 68–135 MHz).
             * Skip holes; only stop after repeated failures near maxPClock. */
            D(CHATTY, "skip unachievable PCLK %ld0 KHz\n", (ULONG)minFreq);
            if (e > 0 && minFreq + 300u >= maxFreq && ++failStreak >= 3) {
                break;
            }
        } else {
            DFUNC(CHATTY, "Pixelclock %03ld %09ldHz --> %09ldHz: \n\n", (ULONG)e, (ULONG)minFreq * 10000,
                  frequency * 10000);
            failStreak = 0;
            ++e;
        }
        minFreq += 100;
    }
    if (e < maxNumEntries) {
        ULONG frequency = computePLLValues(bi, cs->maxPClock, reinterpret_cast<const UBYTE *>(multipliers),
                                           numMultipliers, &pllValues[e]);
        if (frequency) {
            ++e;
        }
    }

    if (e == 0) {
        DFUNC(ERROR, "PLL table empty (PCLK %ld..%ld, R=%ld M=%ld)\n", (ULONG)cs->minPClock, (ULONG)cs->maxPClock,
              (ULONG)cs->referenceFrequency, (ULONG)cs->referenceDivider);
        return;
    }

    ULONG maxHiColorFreq = cd->chipFamily <= MACH64VT ? 8000 : cs->maxPClock;

    for (int i = 0; i < 5; i++) {
        bi->PixelClockCount[i] = 0;
    }

    // FIXME: Account for OVERCLOCK
    for (UWORD i = 0; i < e; ++i) {
        ULONG frequency = cs->computeVCLKFrequency(bi, &pllValues[i]);
        D(CHATTY, "Pixelclock %03ld %09ldHz: \n\n", (ULONG)i, frequency * 10000);

        bi->PixelClockCount[CHUNKY]++;

        if (frequency <= maxHiColorFreq) {
            bi->PixelClockCount[HICOLOR]++;
            bi->PixelClockCount[TRUECOLOR]++;
            bi->PixelClockCount[TRUEALPHA]++;
        }
    }

    DFUNC(VERBOSE, "built %ld clocks, CHUNKY count %ld\n", (ULONG)e, bi->PixelClockCount[CHUNKY]);
}

void WriteDefaultRegList(const BoardInfo_t *bi, const UWORD *defaultRegs, int numRegs)
{
    /* Byte offsets (incl. hi/lo halves); AtiRegAperture is dword-index only. */
    RegAperture<MACH64_MMIO_ENDIAN, 0, RegLog::Verbose> mmio(asMach64(bi)->mmioBase());

    for (int r = 0; r < numRegs; r += 2) {
        // if (!( r % 32)) // not necessary as all regs in default list are < 0x40 DWORD OFFSET
        //     waitFifo(bi, 16);
        D(10, "[%lX_%ldh] = 0x%04lx\n", (ULONG)defaultRegs[r] / 4, (ULONG)defaultRegs[r] % 4,
          (ULONG)defaultRegs[r + 1]);
        mmio.writeOff<UWORD>(defaultRegs[r], defaultRegs[r + 1]
#ifdef DBG
                             ,
                             "defaultRegs"
#endif
        );
    }
}

void Mach64Driver::resetEngine()
{
    DFUNC(VERBOSE, "\n");
    Mach64MmioQ mmio = mmioQ();

    /* GEN_GUI_RESETB: 0 = hold reset, 1 = run (ATI PRG). */
    ULONG genTestCntl = mmio.readL(GEN_TEST_CNTL) & ~(GEN_GUI_RESETB_MASK | GEN_CUR_ENABLE_MASK);
    mmio.writeL(GEN_TEST_CNTL, genTestCntl);
    delayMicroSeconds(10);
    mmio.writeL(GEN_TEST_CNTL, genTestCntl | GEN_GUI_RESETB);
    delayMicroSeconds(10);

    if (chip()->chipFamily < MACH64GT) {
        mmio.writeMaskL(BUS_CNTL, BUS_FIFO_ERR_AK | BUS_HOST_ERR_AK, BUS_FIFO_ERR_AK | BUS_HOST_ERR_AK);
    }
}

void ResetEngine(const BoardInfo_t *bi)
{
    asMach64(const_cast<BoardInfo_t *>(bi))->resetEngine();
}

UBYTE getAsicVersion(const BoardInfo_t *bi)
{
    Mach64MmioQ mmio = asMach64(bi)->mmioQ();
    return (mmio.readB(CONFIG_CHIP_ID, 3) & 0x7);
}

BOOL isAsiclessThanV4(const BoardInfo_t *bi)
{
    return getAsicVersion(bi) < 4;
}

#ifdef __cplusplus
}
#endif
