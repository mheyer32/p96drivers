#include "at3d_reg_apertures.hpp"
#include "at3dconfig.h"
#include "common.h"
using namespace MmioReg;
ULONG at3dCxxRegSmokeReadStatus(volatile UBYTE *mmioBase)
{
    return At3dMmioQ(mmioBase).readL(EXT_DAC_STATUS);
}
