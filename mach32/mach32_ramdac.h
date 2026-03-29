#ifndef MACH32_RAMDAC_H
#define MACH32_RAMDAC_H

#include "chip_mach32.h"

#include <boardinfo.h>
#include <exec/types.h>

#define DAC_ENABLE_RS2()  W_EXT_GE_CONFIG_MASK(DAC_EXT_ADDR_MASK, DAC_EXT_ADDR(0b01));  // Set RS2
#define DAC_DISABLE_RS2() W_EXT_GE_CONFIG_MASK(DAC_EXT_ADDR_MASK, DAC_EXT_ADDR(0b00));  // Clear RS2

typedef enum
{
    ATI68830 = 0,
    SC11483,
    AT68875,
    BT475,
    BT481,
    ATI68860
}  DACType;

struct ModeInfo;
struct svga_pll;

/* Same layout as s3trio64/s3ramdac.h RamdacOps_t for future shared refactoring. */
typedef struct RamdacOps
{
    void (*getPllParams)(struct BoardInfo *bi, const struct svga_pll **pll, UWORD *maxFreqMhz);
    void (*packPllToModeInfo)(struct BoardInfo *bi, UWORD m, UWORD n, UWORD r, struct ModeInfo *mi);
    void (*setClock)(struct BoardInfo *bi);
    ULONG (*setMemoryClock)(struct BoardInfo *bi, ULONG clockHz);
    void (*setDac)(struct BoardInfo *bi, RGBFTYPE format);
} RamdacOps_t;

BOOL InitRAMDAC(struct BoardInfo *bi, DACType dacType);

#endif  // MACH32_RAMDAC_H
