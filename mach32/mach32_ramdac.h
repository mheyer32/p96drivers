#ifndef MACH32_RAMDAC_H
#define MACH32_RAMDAC_H

#include "chip_mach32.h"

#include <boardinfo.h>
#include <exec/types.h>

#define DAC_ENABLE_RS2()  asMach32(bi)->writeExtGeConfigMask(DAC_EXT_ADDR_MASK, DAC_EXT_ADDR(0b01));
#define DAC_DISABLE_RS2() asMach32(bi)->writeExtGeConfigMask(DAC_EXT_ADDR_MASK, DAC_EXT_ADDR(0b00));

#define PIXEL_CLOCK_INDEX_COUNT 30

typedef enum
{
    ATI68830 = 0,
    SC11483,
    AT68875,
    BT475,
    BT481,
    ATI68860
} DACType;

struct ModeInfo;
struct svga_pll;

#include "ramdac_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL InitRAMDAC(struct BoardInfo *bi, DACType dacType);

ULONG HzForClockIndex(ULONG index);
/* Logical dots/s for P96; HiColor indices encode ~2× synthesizer rate (Bt481 / REG688000-style). */
ULONG HzForClockIndexAsLogicalDotsPerSecond(ULONG index, RGBFTYPE format);
LONG ResolveModeInfoPixelClock(struct ModeInfo *mi, ULONG targetHz, RGBFTYPE format);

#ifdef __cplusplus
}
#endif

#endif  // MACH32_RAMDAC_H
