#include "mach64_common.h"
#include "chip_mach64.h"


ChipFamily_t getChipFamily(UWORD deviceId)
{
    switch (deviceId) {
    case 0x5654:  // mach64 VT
        return MACH64VT;
    case 0x4758:  // mach64 GX
        return MACH64GX;
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
    REGBASE();
    // FIXME: its possible older Mach chips want 8bit access here
    ULONG clockCntl = R_BLKIO_AND_L(CLOCK_CNTL, ~(PLL_ADDR_MASK | PLL_DATA_MASK | PLL_WR_ENABLE_MASK));

    // Set PLL Adress
    clockCntl |= PLL_ADDR(pllAddr);
    W_BLKIO_L(CLOCK_CNTL, clockCntl);
    // Read back data
    clockCntl = R_BLKIO_L(CLOCK_CNTL);

    UBYTE pllValue = (clockCntl >> 16) & 0xFF;

    DFUNC(VERBOSE, "pllAddr: %ld, pllValue: 0x%02lX\n", (ULONG)pllAddr, (ULONG)pllValue);

    return pllValue;
}

void WritePLL(BoardInfo_t *bi, UBYTE pllAddr, UBYTE pllDataMask, UBYTE pllData)
{
    REGBASE();

    // // FIXME: its possible older Mach chips want 8bit access here
    // ULONG oldClockCntl = R_BLKIO_AND_L(CLOCK_CNTL, ~(PLL_ADDR_MASK | PLL_DATA_MASK | PLL_WR_ENABLE_MASK));

    // ULONG clockCntl = oldClockCntl;
    // // Set PLL Adress
    // clockCntl |= PLL_ADDR(pllAddr);
    // W_BLKIO_L(CLOCK_CNTL, clockCntl);
    // // Read back old data
    // clockCntl = R_BLKIO_L(CLOCK_CNTL);
    // // write new PLL_DATA
    // clockCntl &= ~PLL_DATA((ULONG)pllDataMask);
    // clockCntl |= PLL_DATA((ULONG)(pllData & pllDataMask));
    // clockCntl |= PLL_WR_ENABLE;
    // W_BLKIO_L(CLOCK_CNTL, clockCntl);

    // // Read right back again
    // R_BLKIO_L(CLOCK_CNTL);

    // // Disable PLL_WR_EN again
    // W_BLKIO_L(CLOCK_CNTL, oldClockCntl);

    // DFUNC(VERBOSE, "pllAddr: %ld, pllDataMask: 0x%02lx & pllData: 0x%02lx --> pllValue: 0x%02lx\n", (ULONG)pllAddr,
    //       (ULONG)pllDataMask, (ULONG)pllData, (ULONG)(clockCntl >> 16) & 0xFF);

    // testing byte access
    W_BLKIO_B(CLOCK_CNTL, CLOCK_CNTL_ADDR, pllAddr << 2);
    UBYTE oldValue = R_BLKIO_B(CLOCK_CNTL, CLOCK_CNTL_DATA);
    UBYTE newValue = (oldValue & ~pllDataMask) | (pllData & pllDataMask);
    W_BLKIO_B(CLOCK_CNTL, CLOCK_CNTL_ADDR, (pllAddr << 2) | 0x02); // PLL_WR_EN
    W_BLKIO_B(CLOCK_CNTL, CLOCK_CNTL_DATA, newValue);
    W_BLKIO_B(CLOCK_CNTL, CLOCK_CNTL_ADDR, 0x00);

     DFUNC(VERBOSE, "pllAddr: %ld, pllDataMask: 0x%02lx & pllData: 0x%02lx --> pllValue: 0x%02lx\n", (ULONG)pllAddr,
           (ULONG)pllDataMask, (ULONG)pllData, (ULONG)newValue);
}

ULONG computeFrequencyKhz10(UWORD RefFreq, UWORD FBDiv, UWORD RefDiv, UBYTE PostDiv)
{
    return ((ULONG)2 * RefFreq * FBDiv) / (RefDiv * PostDiv);
}

ULONG computeFrequencyKhz10FromPllValue(const BoardInfo_t *bi, const PLLValue_t *pllValues, const UBYTE *postDividers)
{
    const ChipData_t *cd = getConstChipData(bi);
    return computeFrequencyKhz10(cd->referenceFrequency, pllValues->N, cd->referenceDivider,
                                 postDividers[pllValues->Pidx]);
}

static BOOL inline isGoodVCOFrequency(ULONG freqKhz10)
{
    return freqKhz10 >= 11800 && freqKhz10 <= 23500;
}

ULONG computePLLValues(const BoardInfo_t *bi, ULONG freqKhz10, const UBYTE *postDividers, WORD numPostDividers,
                       PLLValue_t *pllValues)
{
    DFUNC(CHATTY, "targetFrequency: %ld0 KHz\n", freqKhz10);

    UBYTE postDivIdx = 0xff;
    if (freqKhz10 < 1475) {
        freqKhz10  = 1475;
        postDivIdx = numPostDividers - 1;
    }

    if (freqKhz10 > 23500) {
        freqKhz10 = 23500;
    }

    if (postDivIdx == 0xff) {
        for (BYTE i = numPostDividers - 1; i >= 0; --i) {
            if (isGoodVCOFrequency(freqKhz10 * postDividers[i])) {
                postDivIdx = i;
                break;
            }
        }
        if (postDivIdx == 0xff) {
            postDivIdx = 0;
            DFUNC(WARN, "Failed to find a suitable post divider for freq %ld0 KHz\n", freqKhz10);
        }
    }
    D(CHATTY, "chose postDivIdx %ld\n", postDivIdx);

    UWORD M = getConstChipData(bi)->referenceDivider;
    UWORD R = getConstChipData(bi)->referenceFrequency;

            // T = 2 * R * N / (M * P)
            // N = T * M * P / (2 * R)

    UWORD Ntimes2 = (freqKhz10 * M * postDividers[postDivIdx] + R - 1) / R;

    pllValues->N    = Ntimes2 / 2;
    pllValues->Pidx = postDivIdx;

    ULONG outputFreq = computeFrequencyKhz10(R, pllValues->N, M, postDividers[pllValues->Pidx]);

    D(CHATTY, "target: %ld0 KHz, Output: %ld0 KHz, R: %ld0 KHz, M: %ld, P: %ld, N: %ld\n", (ULONG)freqKhz10,
      (ULONG)outputFreq, (ULONG)R, (ULONG)M, (ULONG)postDividers[postDivIdx], (ULONG)pllValues->N);

    return outputFreq;
}

void InitVClockPLLTable(BoardInfo_t *bi, const BYTE *multipliers, BYTE numMultipliers)
{
    DFUNC(VERBOSE, "", bi);

    LOCAL_SYSBASE();

    ChipData_t *cd      = getChipData(bi);
    UWORD maxNumEntries = (cd->maxPClock + 99) / 100 - (cd->minPClock + 99) / 100;

    D(VERBOSE, "Number of Pixelclocks %ld\n", maxNumEntries);

            // FIXME: there's no free... is there ever a time a chip driver gets expunged?
    PLLValue_t *pllValues = AllocVec(sizeof(PLLValue_t) * maxNumEntries, MEMF_PUBLIC);
    cd->pllValues         = pllValues;

    UWORD minFreq = cd->minPClock;
    UWORD e       = 0;
    for (; e < maxNumEntries; ++e) {
        ULONG frequency = computePLLValues(bi, minFreq, multipliers, numMultipliers, &pllValues[e]);
        if (!frequency) {
            DFUNC(ERROR, "Unable to compute PLL values for %ld0 KHz\n", minFreq);
            break;
        } else {
            DFUNC(CHATTY, "Pixelclock %03ld %09ldHz: \n", e, frequency * 10000);
        }
        minFreq += 100;
    }
    // See if we can squeeze the max frequency still in there
    if (e < maxNumEntries - 1 && minFreq < cd->maxPClock) {
        ULONG frequency = computePLLValues(bi, cd->maxPClock, multipliers, numMultipliers, &pllValues[e]);
        if (frequency) {
            ++e;
        }
    }

    ULONG maxHiColorFreq = cd->chipFamily <= MACH64VT ? 8000 : cd->maxPClock;

    for (int i = 0; i < 5; i++) {
        bi->PixelClockCount[i] = 0;
    }

            // FIXME: Account for OVERCLOCK
    for (UWORD i = 0; i < e; ++i) {
        ULONG frequency = computeFrequencyKhz10FromPllValue(bi, &pllValues[i], multipliers);
        D(CHATTY, "Pixelclock %03ld %09ldHz: \n", i, frequency * 10000);

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
    REGBASE();

    for (int r = 0; r < numRegs; r += 2) {
        // if (!( r % 32)) // not necessary as all regs in default list are < 0x40 DWORD OFFSET
        //     waitFifo(bi, 16);
        D(10, "[%lX_%ldh] = 0x%04lx\n", (ULONG)defaultRegs[r] / 4, (ULONG)defaultRegs[r] % 4,
          (ULONG)defaultRegs[r + 1]);
        // Register offsets in the defaultRegs list are already BYTE offsets
        W_IO_W(defaultRegs[r], defaultRegs[r + 1]);
    }
}

void ResetEngine(const BoardInfo_t *bi)
{
    DFUNC(VERBOSE, "\n");
    REGBASE();

    // Reset engine. FIXME: older models use the same bit, but in a different way(?)
    ULONG genTestCntl = R_BLKIO_L(GEN_TEST_CNTL) & ~(GEN_GUI_RESETB_MASK | GEN_CUR_ENABLE_MASK);
    W_BLKIO_L(GEN_TEST_CNTL, genTestCntl | GEN_GUI_RESETB);
    delayMicroSeconds(10);
    W_BLKIO_L(GEN_TEST_CNTL, genTestCntl);
    delayMicroSeconds(10);
    // W_BLKIO_L(GEN_TEST_CNTL, genTestCntl | GEN_GUI_RESETB);

    if (getConstChipData(bi)->chipFamily < MACH64GT) {
        W_BLKIO_MASK_L(BUS_CNTL, BUS_FIFO_ERR_AK | BUS_HOST_ERR_AK, BUS_FIFO_ERR_AK | BUS_HOST_ERR_AK);
    }
}
