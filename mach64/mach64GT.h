#ifndef MACH64GT_H
#define MACH64GT_H

#include <boardinfo.h>

class Mach64Driver;

extern const UBYTE g_VPLLPostDivider[7];
extern const UBYTE g_VPLLPostDividerCodes[];

void AdjustDSP(Mach64Driver &bi, UBYTE vclkFBDiv, UBYTE vclkPostDiv);

BOOL InitMach64GT(Mach64Driver &bi);
void SetMemoryClock_GT(Mach64Driver &bi, UWORD freqKhz10);
void ASM SetClock_GT(__REGA0(struct BoardInfo *bi));

#endif
