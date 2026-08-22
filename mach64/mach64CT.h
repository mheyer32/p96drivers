#ifndef MACH64CT_H
#define MACH64CT_H

#include <boardinfo.h>

#ifdef __cplusplus
extern "C" {
#endif

BOOL InitMach64CT(struct BoardInfo *bi);
void AdjustCrtcFifo_CT(struct BoardInfo *bi);
void SetMemoryClock_CT(struct BoardInfo *bi, UWORD freqKhz10);

#ifdef __cplusplus
}
#endif

#endif  // MACH64CT_H
