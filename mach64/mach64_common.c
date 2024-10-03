#include "mach64_common.h"
#include "chip_mach64.h"


UBYTE ReadPLL(BoardInfo_t *bi, UBYTE pllAddr)
{
    REGBASE();
    DFUNC(VERBOSE, "ReadPLL: pllAddr: %d\n", (ULONG)pllAddr);

    // FIXME: its possible older Mach chips want 8bit access here
    ULONG clockCntl = R_BLKIO_AND_L(CLOCK_CNTL, ~(PLL_ADDR_MASK | PLL_DATA_MASK | PLL_WR_ENABLE_MASK));

    // Set PLL Adress
    clockCntl |= PLL_ADDR(pllAddr);
    W_BLKIO_L(CLOCK_CNTL, clockCntl);
    // Read back data
    clockCntl = R_BLKIO_L(CLOCK_CNTL);

    UBYTE pllValue = (clockCntl >> 16) & 0xFF;

    DFUNC(VERBOSE, "pllValue: %d\n", (ULONG)pllValue);

    return pllValue;
}

void WritePLL(BoardInfo_t *bi, UBYTE pllAddr, UBYTE pllDataMask, UBYTE pllData)
{
    REGBASE();

    DFUNC(VERBOSE, "pllAddr: %d, pllDataMask: 0x%02X, pllData: 0x%02X\n", (ULONG)pllAddr, (ULONG)pllDataMask,
          (ULONG)pllData);

    // FIXME: its possible older Mach chips want 8bit access here
    ULONG oldClockCntl = R_BLKIO_AND_L(CLOCK_CNTL, ~(PLL_ADDR_MASK | PLL_DATA_MASK | PLL_WR_ENABLE_MASK));

    ULONG clockCntl = oldClockCntl;
    // Set PLL Adress
    clockCntl |= PLL_ADDR(pllAddr);
    W_BLKIO_L(CLOCK_CNTL, clockCntl);
    // Read back old data
    clockCntl = R_BLKIO_L(CLOCK_CNTL);
    // write new PLL_DATA
    clockCntl &= ~PLL_DATA(pllDataMask);
    clockCntl |= PLL_DATA(pllData & pllDataMask);
    clockCntl |= PLL_WR_ENABLE;
    W_BLKIO_L(CLOCK_CNTL, clockCntl);
    // Read right back again
    R_BLKIO_L(CLOCK_CNTL);

    // Disable PLL_WR_EN again
    W_BLKIO_L(CLOCK_CNTL, oldClockCntl);
}


void WriteDefaultRegList(const BoardInfo_t *bi, const UWORD *defaultRegs, int numRegs)
{
    REGBASE();

    for (int r = 0; r < numRegs; r += 2) {
        D(10, "[%lX_%ldh] = 0x%04lx\n", (ULONG)defaultRegs[r] / 4, (ULONG)defaultRegs[r] % 4,
          (ULONG)defaultRegs[r + 1]);
        W_IO_W(defaultRegs[r], defaultRegs[r + 1]);
    }
}
