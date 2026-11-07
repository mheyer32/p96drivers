#include "ch8398ramdac.h"
#include "et4000_common.h"

using namespace VgaReg;

#define CH8398_ID_VALUE 0xC0

static const CH8398_PLLValue_t ch8398_pll_table[] = {
    {1431, 8, 72, 1},    {2506, 2, 27, 1},    {2520, 23, 168, 1}, {2595, 6, 50, 1},   {2744, 7, 61, 1},
    {2828, 8, 71, 1},    {2833, 21, 174, 1},  {2864, 2, 24, 2},   {3007, 3, 34, 2},   {3150, 8, 80, 2},
    {3257, 8, 83, 2},    {3375, 5, 58, 2},    {3589, 3, 17, 1},   {3759, 6, 34, 1},   {3848, 6, 35, 1},
    {4009, 8, 48, 1},    {4398, 5, 35, 1},    {4474, 6, 42, 1},   {4725, 3, 25, 1},   {5011, 3, 27, 1},
    {5190, 6, 50, 1},    {5489, 7, 61, 1},    {5656, 8, 71, 1},   {5727, 2, 24, 1},   {6014, 3, 34, 2},
    {6300, 8, 80, 1},    {6515, 8, 83, 1},    {6750, 5, 58, 1},   {7159, 3, 17, 0},   {7517, 6, 34, 0},
    {7696, 6, 35, 0},    {8018, 8, 48, 0},    {8795, 5, 35, 0},   {8949, 6, 42, 0},   {9450, 3, 25, 0},
    {10023, 4, 34, 0},   {10500, 7, 58, 0},   {10818, 7, 60, 0},  {10977, 7, 61, 0},  {11812, 2, 25, 0},
    {12027, 3, 34, 0},   {13091, 5, 56, 0},   {13500, 7, 58, 0},
};

#define CH8398_PLL_TABLE_SIZE (sizeof(ch8398_pll_table) / sizeof(ch8398_pll_table[0]))

BOOL CheckForCH8398(struct BoardInfo *bi)
{
    DRIVER_LOCALS(bi);

    UBYTE saveCR31;
    BOOL found = FALSE;

    saveCR31 = vga.readCR(0x31);
    vga.writeCRMask(0x31, 0x40, 0x00);

    vga.readB(DAC_PEL_MASK);
    vga.readB(DAC_PEL_MASK);
    vga.readB(DAC_PEL_MASK);
    UBYTE idValue = vga.readB(DAC_PEL_MASK);

    vga.writeCR(0x31, saveCR31);

    if (idValue == CH8398_ID_VALUE) {
        DFUNC(0, "Found Chrontel CH8398 RAMDAC (ID: 0x%02x)\n", idValue);
        found = TRUE;
    } else {
        DFUNC(0, "CH8398 not detected, ID: 0x%02x (expected: 0x%02x)\n", idValue, CH8398_ID_VALUE);
    }

    return found;
}

BOOL InitCH8398(struct BoardInfo *bi)
{
    DRIVER_LOCALS(bi);

    DFUNC(0, "CH8398 RAMDAC initialized\n");
    vga.writeB(DAC_PEL_MASK, 0xFF);
    return TRUE;
}

ULONG SetMemoryClockCH8398(struct BoardInfo *bi, ULONG clockHz)
{
    DRIVER_LOCALS(bi);

    ULONG targetFreq10khz = clockHz / 10000;

    DFUNC(INFO, "SetMemoryClock: requested %ld Hz (%ld * 10kHz)\n", clockHz, targetFreq10khz);

    UWORD left     = 0;
    UWORD right    = CH8398_PLL_TABLE_SIZE - 1;
    UWORD best     = 0;
    ULONG bestDiff = 0xFFFFFFFF;

    while (left <= right) {
        UWORD mid     = (left + right) / 2;
        ULONG midFreq = ch8398_pll_table[mid].freq10khz;

        ULONG diff = (midFreq > targetFreq10khz) ? (midFreq - targetFreq10khz) : (targetFreq10khz - midFreq);

        if (diff < bestDiff) {
            bestDiff = diff;
            best     = mid;
        }

        if (midFreq == targetFreq10khz) {
            best = mid;
            break;
        } else if (midFreq < targetFreq10khz) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    const CH8398_PLLValue_t *pllValue = &ch8398_pll_table[best];
    ULONG actualFreqHz                = pllValue->freq10khz * 10000;

    DFUNC(INFO, "SetMemoryClock: selected entry %ld: %ld Hz (M=%ld, N=%ld, K=%ld)\n", (ULONG)best, actualFreqHz,
          (ULONG)pllValue->M, (ULONG)pllValue->N, (ULONG)pllValue->K);

    UBYTE saveCR31 = vga.readCR(0x31);
    vga.writeCRMask(0x31, 0x40, 0x00);

    vga.writeB(DAC_WR_INDEX, 0x10);

    UBYTE pllLSB = pllValue->N;
    UBYTE pllMSB = (pllValue->K << 6) | (pllValue->M & 0x3F);

    vga.writeB(DAC_PEL_DATA, pllLSB);
    vga.writeB(DAC_PEL_DATA, pllMSB);

    vga.writeCR(0x31, saveCR31);

    DFUNC(INFO, "SetMemoryClock: programmed MPLL to %ld Hz\n", actualFreqHz);

    return actualFreqHz;
}
