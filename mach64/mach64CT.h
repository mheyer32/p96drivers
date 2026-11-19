#ifndef MACH64CT_H
#define MACH64CT_H

#include <boardinfo.h>

class Mach64Driver;

BOOL InitMach64CT(Mach64Driver &bi);
void AdjustCrtcFifo_CT(Mach64Driver &bi);
void SetMemoryClock_CT(Mach64Driver &bi, UWORD freqKhz10);

#endif
