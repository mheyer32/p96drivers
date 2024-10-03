#ifndef MACH64GT_H
#define MACH64GT_H

#include <boardinfo.h>

extern const UBYTE g_VCLKPllMultiplier[];
extern const UBYTE g_VCLKPllMultiplierCode[];

BOOL InitMach64GT(struct BoardInfo *bi);



#endif  // MACH64GT_H
