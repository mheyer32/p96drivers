#ifndef MACH64GT_H
#define MACH64GT_H

#include <boardinfo.h>

#define BLOCK1(register) ((register) - 0x100)

// new for GT
#define EXT_MEM_CNTL 0x2B

// Additional register definitions
#define GP_IO        0x1E
#define TIMER_CONFIG 0x0A

// MEM_BUF_CNTL
#define MEM_BUF_CNTL             0x0B
#define INVALIDATE_RB_CACHE_MASK BIT(23)
#define INVALIDATE_RB_CACHE      BIT(23)

#define CUSTOM_MACRO_CNTL 0x35
#define MPP_CONFIG        0x3B
#define TVO_CNTL          0x3F
#define VGA_DSP_CONFIG    0x13
#define VGA_DSP_ON_OFF    0x14
#define Z_CNTL            0x53 /* GT */
#define ALPHA_TST_CNTL    0x54 /* GTPro */
#define SCALE_3D_CNTL     0x7f /* GT */

#define SETUP_CNTL BLOCK1(0xc1)

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
#define DSP_CONFIG            0x08
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

// DP_SET_GUI_ENGINE
#define DP_SET_GUI_ENGINE2 0xBE
#define DP_SET_GUI_ENGINE  0xBF

extern const UBYTE g_VPLLPostDivider[];
extern const UBYTE g_VPLLPostDividerCodes[];

void AdjustDSP(struct BoardInfo *bi, UBYTE vclkFBDiv, UBYTE vclkPostDiv);

BOOL InitMach64GT(struct BoardInfo *bi);

#endif  // MACH64GT_H
