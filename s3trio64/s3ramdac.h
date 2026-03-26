#ifndef S3RAMDAC_H
#define S3RAMDAC_H

#include <boardinfo.h>
#include <exec/types.h>

// Minimum PLL frequency (kHz). Below this, SR1 bit 3 enables DCLK = VCLK/2 (clock halving).
#define MIN_PLLCLOCK_KHZ 24500
#define MIN_PLLCLOCK_HZ  (MIN_PLLCLOCK_KHZ * 1000)

#define DAC_ENABLE_RS2()  W_CR_MASK(0x55, 0x01, 0x01);  // Clear RS2 bit for direct register access
#define DAC_DISABLE_RS2() W_CR_MASK(0x55, 0x01, 0x00);  // Clear RS2 bit for direct register access

struct ModeInfo;
struct svga_pll;

typedef struct RamdacOps
{
    void (*getPllParams)(struct BoardInfo *bi, const struct svga_pll **pll, UWORD *maxFreqMhz);
    void (*packPllToModeInfo)(struct BoardInfo *bi, UWORD m, UWORD n, UWORD r, struct ModeInfo *mi);
    void (*setClock)(struct BoardInfo *bi);
    ULONG (*setMemoryClock)(struct BoardInfo *bi, ULONG clockHz);
    void (*setDac)(struct BoardInfo *bi, RGBFTYPE format);
} RamdacOps_t;

BOOL InitRAMDAC(struct BoardInfo *bi);
const RamdacOps_t *getRamdacOps(struct BoardInfo *bi);

#endif  // S3RAMDAC_H
