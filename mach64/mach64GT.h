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

// CONFIG_STAT2
#define CONFIG_STAT2 0x26

#define PCI5VEN      BIT(25)
#define PCI5VEN_MASK BIT(25)

// BUS_CNTL
#define BUS_DBL_RESYNC        BIT(0)
#define BUS_DBL_RESYNC_MASK   BIT(0)
#define BUS_MSTR_RESET        BIT(1)
#define BUS_MSTR_RESET_MASK   BIT(1)
#define BUS_FLUSH_BUF         BIT(2)
#define BUS_FLUSH_BUF_MASK    BIT(2)
#define BUS_APER_REG_DIS      BIT(4)
#define BUS_APER_REG_DIS_MASK BIT(4)
#define BUS_MASTER_DIS        BIT(6)
#define BUS_MASTER_DIS_MASK   BIT(6)

// DSP_CONFIG
#define DSP_CONFIG  0x08
#define DSP_XCLKS_PER_QW(x)   (x)
#define DSP_XCLKS_PER_QW_MASK (0x3FFF)
#define DSP_LOOP_LATENCY(x)   ((x) << 16)
#define DSP_LOOP_LATENCY_MASK (0xF << 16)
#define DSP_PRECISION(x)      ((x) << 20)
#define DSP_PRECISION_MASK    (0x7 << 20)

// DSP_ON_OFF
#define DSP_ON_OFF   0x09
#define DSP_OFF(x)   (x)
#define DSP_OFF_MASK (0x7FF)
#define DSP_ON(x)    ((x) << 16)
#define DSP_ON_MASK  (0x7FF << 16)

extern const UBYTE g_VCLKPllMultiplier[];
extern const UBYTE g_VCLKPllMultiplierCode[];

void AdjustDSP(struct BoardInfo *bi, UBYTE fbDiv, UBYTE postDiv);

BOOL InitMach64GT(struct BoardInfo *bi);

#endif  // MACH64GT_H
