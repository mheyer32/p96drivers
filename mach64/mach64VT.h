#ifndef MACH64VT_H
#define MACH64VT_H

#include <boardinfo.h>

class Mach64Driver;

BOOL InitMach64VT(Mach64Driver &bi);
void AdjustCrtcFifo_VT(Mach64Driver &bi);
void SetMemoryClock_VT(Mach64Driver &bi, UWORD freqKhz10);

#endif
