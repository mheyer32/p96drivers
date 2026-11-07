#ifndef CH8398RAMDAC_H
#define CH8398RAMDAC_H

#include <boardinfo.h>
#include <exec/types.h>

typedef struct CH8398_PLLValue
{
    ULONG freq10khz;
    UBYTE M;
    UBYTE N;
    UBYTE K;
} CH8398_PLLValue_t;

#ifdef __cplusplus
extern "C" {
#endif
BOOL CheckForCH8398(struct BoardInfo *bi);
BOOL InitCH8398(struct BoardInfo *bi);
ULONG SetMemoryClockCH8398(struct BoardInfo *bi, ULONG clockHz);
#ifdef __cplusplus
}
#endif

#endif
