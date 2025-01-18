#ifndef MACH64GT_H
#define MACH64GT_H

#include <boardinfo.h>

// new for GT
#define EXT_MEM_CNTL 0x2B

#define MEM_SDRAM_RESET_MASK BIT(1)
#define MEM_SDRAM_RESET      BIT(1)
// #define MEM_MA_YCLK           BIT(4)
// #define MEM_MA_YCLK_MASK      BIT(4)
#define MEM_CYC_TEST_MASK     (0x3 << 2)
#define MEM_CYC_TEST(x)       ((x) << 2)
#define MEM_ALL_PAGE_DIS_MASK BIT(30)
#define MEM_ALL_PAGE_DIS      BIT(30)
#define MEM_TILE_SELECT(x)    ((x) << 4)
#define MEM_TILE_SELECT_MASK  (0xF << 4)

extern const UBYTE g_VCLKPllMultiplier[];
extern const UBYTE g_VCLKPllMultiplierCode[];

BOOL InitMach64GT(struct BoardInfo *bi);

#endif  // MACH64GT_H
