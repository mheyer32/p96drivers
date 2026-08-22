#ifndef MACH64VT_H
#define MACH64VT_H

#include <boardinfo.h>

#ifdef __cplusplus
extern "C" {
#endif

BOOL InitMach64VT(struct BoardInfo *bi);
void AdjustCrtcFifo_VT(struct BoardInfo *bi);
void SetMemoryClock_VT(struct BoardInfo *bi, UWORD freqKhz10);

#ifdef __cplusplus
}
#endif

#endif  // MACH64VT_H
