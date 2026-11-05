#ifndef RAMDAC_OPS_H
#define RAMDAC_OPS_H

#include <boardinfo.h>
#include <exec/types.h>

struct ModeInfo;
struct svga_pll;

/*
 * Shared external/integrated RAMDAC ops table (Mach32 / S3).
 * Chip-specific InitRAMDAC fills ChipData::ramdacOps.
 */
typedef struct RamdacOps
{
    void (*getPllParams)(struct BoardInfo *bi, const struct svga_pll **pll, UWORD *maxFreqMhz);
    void (*packPllToModeInfo)(struct BoardInfo *bi, UWORD m, UWORD n, UWORD r, struct ModeInfo *mi);
    void (*setClock)(struct BoardInfo *bi);
    ULONG (*setMemoryClock)(struct BoardInfo *bi, ULONG clockHz);
    void (*setDac)(struct BoardInfo *bi, RGBFTYPE format);
} RamdacOps_t;

#endif /* RAMDAC_OPS_H */
