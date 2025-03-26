#include "mach64_common.h"
#include "chip_mach64.h"

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

    // FIXME: its possible older Mach chips want 8bit access here
    ULONG oldClockCntl = R_BLKIO_AND_L(CLOCK_CNTL, ~(PLL_ADDR_MASK | PLL_DATA_MASK | PLL_WR_ENABLE_MASK));

    ULONG clockCntl = oldClockCntl;
    // Set PLL Adress
    clockCntl |= PLL_ADDR(pllAddr);
    W_BLKIO_L(CLOCK_CNTL, clockCntl);
    // Read back old data
    clockCntl = R_BLKIO_L(CLOCK_CNTL);
    // write new PLL_DATA
    clockCntl &= ~PLL_DATA((ULONG)pllDataMask);
    clockCntl |= PLL_DATA((ULONG)(pllData & pllDataMask));
    clockCntl |= PLL_WR_ENABLE;
    W_BLKIO_L(CLOCK_CNTL, clockCntl);

    // Read right back again
    R_BLKIO_L(CLOCK_CNTL);

    // Disable PLL_WR_EN again
    W_BLKIO_L(CLOCK_CNTL, oldClockCntl);

    DFUNC(VERBOSE, "pllAddr: %ld, pllDataMask: 0x%02lx & pllData: 0x%02lx --> pllValue: 0x%02lx\n", (ULONG)pllAddr,
          (ULONG)pllDataMask, (ULONG)pllData, (ULONG)(clockCntl >> 16) & 0xFF);
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
