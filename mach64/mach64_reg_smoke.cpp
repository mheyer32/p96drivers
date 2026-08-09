/*
 * Compile-time / link smoke for C++ Mach64 register apertures.
 * Ensures AtiRegAperture + MmioReg instantiate under Amiga g++.
 */
#include "chip_mach64.h"

#include "mach64_reg_apertures.hpp"


using namespace MmioReg;

/* Touch quiet NoSwap-style bit test (TST_MMIO_* / waitFifo FIFO_STAT poll). */
BOOL mach64CxxRegSmokeTestFifo(volatile UBYTE *mmioBase, UWORD mask)
{
	Mach64MmioQ mmio(mmioBase);
	return mmio.testW(FIFO_STAT, mask);
}

UWORD mach64CxxRegSmokeReadFifo(volatile UBYTE *mmioBase)
{
	Mach64MmioNoSwapQ mmio(mmioBase);
	return mmio.readW(FIFO_STAT);
}

ULONG mach64CxxRegSmokeReadGui(volatile UBYTE *mmioBase)
{
	Mach64MmioQ mmio(mmioBase);
	return mmio.readL(GUI_STAT);
}

void mach64CxxRegSmokeWriteMask(volatile UBYTE *mmioBase, ULONG mask, ULONG val)
{
	Mach64Mmio mmio(mmioBase);
	mmio.writeMaskL(CRTC_GEN_CNTL, mask, val);
}

/* Prove wrong-space misuse would be a type error if uncommented:
 *   mmio.read<ULONG>(BlkIoReg::CONFIG_CNTL);
 */
