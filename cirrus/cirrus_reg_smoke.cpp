#include "cirrus_reg_apertures.hpp"
#include "cirrusconfig.h"
#include "common.h"

ULONG cirrusCxxRegSmokeReadSr8(volatile UBYTE *ioBase)
{
    return VgaIoQ(ioBase).readSR(0x08);
}
