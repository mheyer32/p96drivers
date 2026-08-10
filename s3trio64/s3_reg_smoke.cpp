#include "s3config.h"
#include "common.h"
#include "s3_reg_apertures.hpp"
ULONG s3CxxRegSmokeReadGpStat(volatile UBYTE *ioBase)
{
	return S3IoQ(ioBase).readW(static_cast<IoReg::Id>(0x9AE8));
}
