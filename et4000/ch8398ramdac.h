#ifndef CH8398RAMDAC_H
#define CH8398RAMDAC_H

#include <boardinfo.h>
#include <exec/types.h>

// CH8398 PLL formula: Fout = Fref * (m * k) / n
// where: m = M+2, n = N+8, k = 2^K
// Fref = 14.318 MHz

// CH8398 PLL value structure
// Note: CH8398 uses M, N, K where m=M+2, n=N+8, k=2^K
typedef struct CH8398_PLLValue
{
    ULONG freq10khz;  // Frequency in 10kHz units
    UBYTE M;          // M value (actual m = M+2)
    UBYTE N;          // N value (actual n = N+8)
    UBYTE K;          // K value (actual k = 2^K)
} CH8398_PLLValue_t;

extern BOOL CheckForCH8398(struct BoardInfo *bi);
extern BOOL InitCH8398(struct BoardInfo *bi);
extern ULONG SetMemoryClockCH8398(struct BoardInfo *bi, ULONG clockHz);

#endif  // CH8398RAMDAC_H
