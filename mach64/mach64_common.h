#ifndef MACH64_COMMON_H
#define MACH64_COMMON_H

#include "common.h"
#include "chip_mach64.h"

struct BoardInfo;

typedef enum ChipFamily
{
    UNKNOWN,
    MACH64GX,
    MACH64VT,  // 8mb aperture only
    MACH64GT,
    MACH64GM  // Rage 3 XL
} ChipFamily_t;

#define CFG_VGA_DIS            BIT(19)
#define CFG_VGA_DIS_MASK       BIT(19)
#define CFG_MEM_VGA_AP_EN      BIT(2)
#define CFG_MEM_VGA_AP_EN_MASK BIT(2)
#define CFG_MEM_AP_LOC(x)      ((x) << 4)
#define CFG_MEM_AP_LOC_MASK    (0x3FF << 4)

#define VGA_128KAP_PAGING      BIT(20)
#define VGA_128KAP_PAGING_MASK BIT(20)
#define CRTC_EXT_DISP_EN       BIT(24)
#define CRTC_EXT_DISP_EN_MASK  BIT(24)
#define VGA_ATI_LINEAR         BIT(27)
#define VGA_ATI_LINEAR_MASK    BIT(27)

#define CFG_MEM_TYPE(x)          ((x) & 7)
#define CFG_MEM_TYPE_MASK        (7)
#define CFG_MEM_TYPE_DISABLE     0b000
#define CFG_MEM_TYPE_DRAM        0b001
#define CFG_MEM_TYPE_EDO         0b010
#define CFG_MEM_TYPE_PSEUDO_EDO  0b011
#define CFG_MEM_TYPE_SDRAM       0b100
#define CFG_MEM_TYPE_SGRAM       0b101
#define CFG_MEM_TYPE_SDRAM_32BIT 0b110

#define CFG_DUAL_CAS_EN      BIT(3)
#define CFG_DUAL_CAS_EN_MASK BIT(3)
#define CFG_VGA_EN           BIT(4)
#define CFG_VGA_EN_MASK      BIT(4)
#define CFG_CLOCK_EN         BIT(5)
#define CFG_CLOCK_EN_MASK    BIT(5)

#define GEN_CUR_ENABLE      BIT(7)
#define GEN_CUR_ENABLE_MASK BIT(7)
#define GEN_GUI_RESETB      BIT(8)
#define GEN_GUI_RESETB_MASK BIT(8)
#define GEN_SOFT_RESET      BIT(9)
#define GEN_SOFT_RESET_MASK BIT(9)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PLL Stuff
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define PLL_OVERRIDE      BIT(0)
#define PLL_OVERRIDE_MASK BIT(0)
#define PLL_MRESET        BIT(1)
#define PLL_MRESET_MASK   BIT(1)
#define OSC_EN            BIT(2)
#define OSC_EN_MASK       BIT(2)
#define MCLK_SRC_SEL(x)   (((x) & 7) << 4)
#define MCLK_SRC_SEL_MASK (7 << 4)

#define MFB_TIMES_4_2b      BIT(2)
#define MFB_TIMES_4_2b_MASK BIT(2)

#define XCLK_SRC_SEL(x)   (x)
#define XCLK_SRC_SEL_MASK (7)

#define CRTC_GEN_CNTL 0x07

#define CRTC_DBL_SCAN_EN        BIT(0)
#define CRTC_INTERLACE_EN       BIT(1)
#define CRTC_HSYNC_DIS          BIT(2)
#define CRTC_VSYNC_DIS          BIT(3)
#define CRTC_DISPLAY_DIS        BIT(6)
#define CRTC_DISPLAY_DIS_MASK   BIT(6)
#define CRTC_PIX_WIDTH(x)       ((x) << 8)
#define CRTC_PIX_WIDTH_MASK     (0x7 << 8)

// These are CT/CT registers
#define CRTC_FIFO_OVERFILL(x)   ((x) << 14)
#define CRTC_FIFO_OVERFILL_MASK (0x3 << 14)
#define CRTC_FIFO_LWM(x)        ((x) << 16)
#define CRTC_FIFO_LWM_MASK      (0xF << 16)
#define CRTC_DISPREQ_ONLY       BIT(21)
#define CRTC_DISPREQ_ONLY_MASK  BIT(21)

#define CRTC_LOCK_REGS          BIT(22)
#define CRTC_LOCK_REGS_MASK     BIT(22)
#define CRTC_EXT_DISP_EN        BIT(24)
#define CRTC_EXT_DISP_EN_MASK   BIT(24)
#define CRTC_ENABLE             BIT(25)
#define CRTC_ENABLE_MASK        BIT(25)
#define CRTC_DISP_REQ_ENB       BIT(26)
#define CRTC_DISP_REQ_ENB_MASK  BIT(26)
#define VGA_ATI_LINEAR          BIT(27)
#define VGA_ATI_LINEAR_MASK     BIT(27)
#define VGA_XCRT_CNT_EN         BIT(30)
#define VGA_XCRT_CNT_EN_MASK    BIT(30)

#define BUS_ROM_DIS         BIT(12)
#define BUS_ROM_DIS_MASK    BIT(12)
#define BUS_FIFO_ERR_INT_EN BIT(20)
#define BUS_FIFO_ERR_INT    BIT(21)
#define BUS_FIFO_ERR_AK     BIT(21)  // INT and ACK are the same bit, distiguished by R/W operation
#define BUS_HOST_ERR_INT_EN BIT(22) // only in CT, not GT
#define BUS_HOST_ERR_INT    BIT(23) // only in CT, not GT
#define BUS_HOST_ERR_AK     BIT(23)  // only in CT, not GT.  INT and ACK are the same bit, distiguished by R/W operation

typedef struct PLL
{
    const BYTE *multipliers;
    BYTE numMultipliers;
    UWORD maxVCO; // in 10Khz
    UWORD minVCO; // in 10Khz
} PLL_t;

enum PLLType
{
    PLL_VCLK,
    PLL_SCLK,
    PLL_MCLK,
    PLL_XCLK,
    PLL_DLL1,
    PLL_DLL2,
    PLL_VFC,
    PLL_PM_DYN_CLK,
};

typedef struct PLLValue
{
    UBYTE Pidx;  // index into the postDividers table
    UBYTE N;      // feedback divider
} PLLValue_t;

enum PLL_REGS
{
    PLL_MPLL_CNTL     = 0,
    PLL_MACRO_CNTL    = 1,
    PLL_VPLL_CNTL     = 1, // GT
    PLL_REF_DIV       = 2,
    PLL_GEN_CNTL      = 3,
    PLL_MCLK_FB_DIV   = 4,
    PLL_VCLK_CNTL     = 5,
    PLL_VCLK_POST_DIV = 6,
    PLL_VCLK0_FB_DIV  = 7,
    PLL_VCLK1_FB_DIV  = 8,
    PLL_VCLK2_FB_DIV  = 9,
    PLL_VCLK3_FB_DIV  = 10,
    PLL_XCLK_CNTL     = 11,  // VT
    PLL_EXT_CNTL      = 11,  // GT
    PLL_FCP_CNTL      = 12,  // VT
    PLL_DLL_CNTL      = 12,     // GT
    PLL_DLL1_CNTL     = 12,
    PLL_VFC_CNTL      = 13,  // VT
    PLL_TEST_CNTL     = 14,
    PLL_TEST_COUNT    = 15,

    PLL_LVDS_CNTL0       = 16,  // GT
    PLL_DLL2_CNTL        = 20,  // GT
    PLL_SCLK_FB_DIV      = 21,  // GT
    PLL_SPLL_CNTL1       = 22,  // GT
    PLL_SPLL_CNTL2       = 23,  // GT
    PLL_APLL_STRAPS      = 24,  // GT
    PLL_EXT_VPLL_CNTL    = 25,  // GT
    PLL_EXT_VPLL_REF_DIV = 26,  // GT
    PLL_EXT_VPLL_FB_DIV  = 27,  // GT
    PLL_EXT_VPLL_MSB     = 28,  // GT
    PLL_HTOTAL_CNTL      = 29,  // GT
    PLL_BYTE_CLK_CNTL    = 30,  // GT
    PLL_PLL_YCLK_CNTL    = 41,  // GT
    PLL_PM_DYN_CLK_CNTL  = 42,  // GT
};

#define PLL_ADDR_MASK      (0x3F << 10) // 6 bits on GT, used to be less on older chips
#define PLL_ADDR(x)        ((x) << 10)
#define PLL_DATA_MASK      (0xFF << 16)
#define PLL_DATA(x)        ((x) << 16)
#define PLL_WR_ENABLE      BIT(9)
#define PLL_WR_ENABLE_MASK BIT(9)
#define CLOCK_SEL_MASK     (0x3)
#define CLOCK_SEL(x)       ((x) & CLOCK_SEL_MASK)

extern void WritePLL(struct BoardInfo *bi, UBYTE pllAddr, UBYTE pllDataMask, UBYTE pllData);
extern UBYTE ReadPLL(struct BoardInfo *bi, UBYTE pllAddr);

#define WRITE_PLL(pllAddr, data)            WritePLL(bi, (pllAddr), 0xFF, (data))
#define WRITE_PLL_MASK(pllAddr, mask, data) WritePLL(bi, (pllAddr), (mask), (data))
#define READ_PLL(pllAddr)                   ReadPLL(bi, (pllAddr))


extern void WriteDefaultRegList(const struct BoardInfo *bi, const UWORD *defaultRegs, int numRegs);
extern void InitVClockPLLTable(struct BoardInfo *bi, const BYTE *multipliers, BYTE numMultipliers);

extern ULONG ComputePLLValues(const BoardInfo_t *bi, ULONG targetFrequency, PLLValue_t *pllValues);
extern ULONG ComputeFrequencyKhz10(UWORD R, UWORD N, UWORD M, UBYTE Plog2);
extern ULONG ComputeFrequencyKhz10FromPllValue(const BoardInfo_t *bi, const PLLValue_t *pllValues);

extern ULONG computePLLValues(const BoardInfo_t *bi, ULONG freqKhz10, const UBYTE *multipliers, WORD numMultipliers,
                              PLLValue_t *pllValues);
extern ULONG computeFrequencyKhz10(UWORD RefFreq, UWORD FBDiv, UWORD RefDiv, UBYTE PostDiv);
extern ULONG computeFrequencyKhz10FromPllValue(const BoardInfo_t *bi, const PLLValue_t *pllValues, const UBYTE *multipliers);

extern void ResetEngine(const BoardInfo_t *bi);

extern ChipFamily_t getChipFamily(UWORD deviceId);
extern const char *getChipFamilyName(ChipFamily_t family);

static INLINE void waitFifo(const BoardInfo_t *bi, UBYTE entries)
{
    return;
    MMIOBASE();

    if (!entries)
        return;

    flushWrites();
    // ULONG busCntl = R_MMIO_L(BUS_CNTL);
    while ((R_MMIO_L(FIFO_STAT) & 0xffff) > ((ULONG)(0x8000 >> entries)))
        ;
}

static INLINE UBYTE getAsicVersion(const BoardInfo_t *bi)
{
    REGBASE();
    return (R_BLKIO_B(CONFIG_CHIP_ID, 3) & 0x7);
}

static INLINE BOOL isAsiclessThanV4(const BoardInfo_t *bi)
{
    return getAsicVersion(bi) < 4;
}

#endif  // MACH64_COMMON_H

