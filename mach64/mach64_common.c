#include "mach64_common.h"
#include "chip_mach64.h"

ChipFamily_t getChipFamily(UWORD deviceId)
{
    switch (deviceId) {
    case 0x5654:  // mach64 VT
        return MACH64VT;
    case 0x4758:  // mach64 GX
        return MACH64GX;
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
    case MACH64GT:
        return "Mach64 GT (Rage Pro)";
    case MACH64GM:
        return "Mach64 GR (Rage3 XL)";
    default:
        return "Unknown";
    }
}

UBYTE ReadPLL(BoardInfo_t *bi, UBYTE pllAddr)
{
    MMIOBASE();
    // FIXME: its possible older Mach chips want 8bit access here
    ULONG clockCntl = R_MMIO_AND_L(CLOCK_CNTL, ~(PLL_ADDR_MASK | PLL_DATA_MASK | PLL_WR_ENABLE_MASK));

    // Set PLL Adress
    clockCntl |= PLL_ADDR(pllAddr);
    W_MMIO_L(CLOCK_CNTL, clockCntl);
    // Read back data
    clockCntl = R_MMIO_L(CLOCK_CNTL);

    UBYTE pllValue = (clockCntl >> 16) & 0xFF;

    DFUNC(VERBOSE, "pllAddr: %ld, pllValue: 0x%02lX\n", (ULONG)pllAddr, (ULONG)pllValue);

    return pllValue;
}

void WritePLL(BoardInfo_t *bi, UBYTE pllAddr, UBYTE pllDataMask, UBYTE pllData)
{
    MMIOBASE();

    // // // FIXME: its possible older Mach chips want 8bit access here
    // ULONG oldClockCntl = R_MMIO_AND_L(CLOCK_CNTL, ~(PLL_ADDR_MASK | PLL_DATA_MASK | PLL_WR_ENABLE_MASK));

    // ULONG clockCntl = oldClockCntl;
    // // Set PLL Adress
    // clockCntl |= PLL_ADDR(pllAddr);
    // W_MMIO_L(CLOCK_CNTL, clockCntl);
    // // Read back old data
    // clockCntl = R_MMIO_L(CLOCK_CNTL);
    // // write new PLL_DATA
    // clockCntl &= ~PLL_DATA((ULONG)pllDataMask);
    // clockCntl |= PLL_DATA((ULONG)(pllData & pllDataMask));
    // clockCntl |= PLL_WR_ENABLE;
    // W_MMIO_L(CLOCK_CNTL, clockCntl);

    // // Read right back again
    // R_MMIO_L(CLOCK_CNTL);

    // // Disable PLL_WR_EN again
    // W_MMIO_L(CLOCK_CNTL, oldClockCntl);

    // DFUNC(VERBOSE, "pllAddr: %ld, pllDataMask: 0x%02lx & pllData: 0x%02lx --> pllValue: 0x%02lx\n", (ULONG)pllAddr,
    //       (ULONG)pllDataMask, (ULONG)pllData, (ULONG)(clockCntl >> 16) & 0xFF);

    // testing byte access
    W_MMIO_B(CLOCK_CNTL, CLOCK_CNTL_ADDR, pllAddr << 2);
    UBYTE oldValue = R_MMIO_B(CLOCK_CNTL, CLOCK_CNTL_DATA);
    UBYTE newValue = (oldValue & ~pllDataMask) | (pllData & pllDataMask);
    W_MMIO_B(CLOCK_CNTL, CLOCK_CNTL_ADDR, (pllAddr << 2) | 0x02);  // PLL_WR_EN
    W_MMIO_B(CLOCK_CNTL, CLOCK_CNTL_DATA, newValue);
    W_MMIO_B(CLOCK_CNTL, CLOCK_CNTL_ADDR, 0x00);

    // DFUNC(VERBOSE, "pllAddr: %ld, pllDataMask: 0x%02lx & pllData: 0x%02lx --> pllValue: 0x%02lx\n", (ULONG)pllAddr,
    //       (ULONG)pllDataMask, (ULONG)pllData, (ULONG)newValue);
}

ULONG computeFrequencyKhz10(UWORD RefFreq, UWORD FBDiv, UWORD RefDiv, UBYTE PostDiv)
{
    return ((ULONG)2 * RefFreq * FBDiv) / (RefDiv * PostDiv);
}

ULONG computeFrequencyKhz10FromPllValue(const BoardInfo_t *bi, const PLLValue_t *pllValues, const UBYTE *postDividers)
{
    const ChipSpecific_t *cs = getConstChipSpecific(bi);
    return computeFrequencyKhz10(cs->referenceFrequency, pllValues->N, cs->referenceDivider,
                                 postDividers[pllValues->Pidx]);
}

static BOOL inline isGoodVCOFrequency(ULONG freqKhz10)
{
    // return freqKhz10 >= 11800 && freqKhz10 <= 23500;
    return freqKhz10 >= 10000 && freqKhz10 <= 20000;
}

ULONG computePLLValues(const BoardInfo_t *bi, ULONG freqKhz10, const UBYTE *postDividers, WORD numPostDividers,
                       PLLValue_t *pllValues)
{
    DFUNC(CHATTY, "targetFrequency: %ld0 KHz\n", freqKhz10);

    // Clamp frequency to valid range
    if (freqKhz10 < 1475) {
        freqKhz10 = 1475;
    }
    //FIXME: this is dependent on the PLL, thus chip dependent
    if (freqKhz10 > 23500) {
        freqKhz10 = 23500;
    }

    const ChipSpecific_t *cs = getConstChipSpecific(bi);
    UWORD M                  = cs->referenceDivider;
    UWORD R                  = cs->referenceFrequency;

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
        if (!isGoodVCOFrequency(vcoFreq)) {
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
    UWORD maxNumEntries  = (cs->maxPClock + 99) / 100 - (cs->minPClock + 99) / 100;

    D(VERBOSE, "Number of Pixelclocks %ld\n", maxNumEntries);

    // FIXME: there's no free... is there ever a time a chip driver gets expunged?
    PLLValue_t *pllValues = AllocVec(sizeof(PLLValue_t) * maxNumEntries, MEMF_PUBLIC);
    cs->vclkPllValues     = pllValues;

    UWORD minFreq = cs->minPClock;
    UWORD maxFreq = cs->maxPClock;
    UWORD e       = 0;
    while(minFreq < maxFreq){
        ULONG frequency = computePLLValues(bi, minFreq, multipliers, numMultipliers, &pllValues[e]);
        if (!frequency) {
            DFUNC(ERROR, "Unable to compute PLL values for %ld0 KHz\n", minFreq);
        } else {
            DFUNC(CHATTY, "Pixelclock %03ld %09ldHz --> %09ldHz: \n\n", (ULONG)e,  minFreq * 10000, frequency * 10000);
            ++e;
        }
        minFreq += 100;
    }
    // See if we can squeeze the max frequency still in there
    if (e < maxNumEntries - 1 && minFreq < cs->maxPClock) {
        ULONG frequency = computePLLValues(bi, cs->maxPClock, multipliers, numMultipliers, &pllValues[e]);
        if (frequency) {
            ++e;
        }
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
}

void WriteDefaultRegList(const BoardInfo_t *bi, const UWORD *defaultRegs, int numRegs)
{
    MMIOBASE();

    for (int r = 0; r < numRegs; r += 2) {
        // if (!( r % 32)) // not necessary as all regs in default list are < 0x40 DWORD OFFSET
        //     waitFifo(bi, 16);
        D(10, "[%lX_%ldh] = 0x%04lx\n", (ULONG)defaultRegs[r] / 4, (ULONG)defaultRegs[r] % 4,
          (ULONG)defaultRegs[r + 1]);
        // Register offsets in the defaultRegs list are already BYTE offsets
        W_MMIO_W(defaultRegs[r], defaultRegs[r + 1]);
    }
}

void ResetEngine(const BoardInfo_t *bi)
{
    DFUNC(VERBOSE, "\n");
    MMIOBASE();

    // Reset engine. FIXME: older models use the same bit, but in a different way(?)
    ULONG genTestCntl = R_MMIO_L(GEN_TEST_CNTL) & ~(GEN_GUI_RESETB_MASK | GEN_CUR_ENABLE_MASK);
    W_MMIO_L(GEN_TEST_CNTL, genTestCntl | GEN_GUI_RESETB);
    delayMicroSeconds(10);
    W_MMIO_L(GEN_TEST_CNTL, genTestCntl);
    delayMicroSeconds(10);
    // W_MMIO_L(GEN_TEST_CNTL, genTestCntl | GEN_GUI_RESETB);

    if (getConstChipData(bi)->chipFamily < MACH64GT) {
        W_MMIO_MASK_L(BUS_CNTL, BUS_FIFO_ERR_AK | BUS_HOST_ERR_AK, BUS_FIFO_ERR_AK | BUS_HOST_ERR_AK);
    }
}

UBYTE getAsicVersion(const BoardInfo_t *bi)
{
    MMIOBASE();
    return (R_MMIO_B(CONFIG_CHIP_ID, 3) & 0x7);
}

BOOL isAsiclessThanV4(const BoardInfo_t *bi)
{
    return getAsicVersion(bi) < 4;
}
