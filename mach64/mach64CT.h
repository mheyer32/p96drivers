#ifndef MACH64CT_H
#define MACH64CT_H

#include <boardinfo.h>

BOOL InitMach64CT(struct BoardInfo *bi);
void AdjustCrtcFifo_CT(struct BoardInfo *bi);
void SetMemoryClock_CT(struct BoardInfo *bi, UWORD freqKhz10);

#endif  // MACH64CT_H
