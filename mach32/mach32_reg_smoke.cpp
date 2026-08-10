/*
 * Compile-time / link smoke for C++ Mach32 register apertures.
 * Include apertures before chip_mach32.h so IoReg::* is not clobbered by #defines.
 */
#include "common.h"
#include "mach32_reg_apertures.hpp"
#include "mach32config.h"

using namespace IoReg;

BOOL mach32CxxRegSmokeTestFifo(volatile UBYTE *ioBase, UWORD mask)
{
    Mach32IoQ io(ioBase);
    return io.testW(EXT_FIFO_STATUS, mask);
}

UWORD mach32CxxRegSmokeReadFifoRaw(volatile UBYTE *ioBase)
{
    Mach32IoNoSwapQ io(ioBase);
    return io.readWRaw(EXT_FIFO_STATUS);
}

void mach32CxxRegSmokeWriteMask(volatile UBYTE *ioBase, UWORD mask, UWORD val)
{
    Mach32Io io(ioBase);
    io.writeMaskW(EXT_GE_CONFIG, mask, val);
}
