#ifndef MACH64_COMMON_H
#define MACH64_COMMON_H

#include "common.h"

struct BoardInfo;

typedef enum ChipFamily
{
    UNKNOWN,
    MACH64GX,
    MACH64VT,  // 8mb aperture only
    MACH64GT,
    MACH64GM  // Rage 3 XL
} ChipFamily_t;

#define DWORD_OFFSET(x) ((x) * 4)

#define SCRATCH_REG0    (0x20)
#define SCRATCH_REG1    (0x21)
#define BUS_CNTL        (0x28)
#define MEM_CNTL        (0x2C)
#define GEN_TEST_CNTL   (0x34)
#define CONFIG_CNTL     (0x37)
#define CONFIG_CHIP_ID  (0x38)
#define CONFIG_STAT0    (0x39)
#define CLOCK_CNTL      (0x24)
#define CLOCK_CNTL_ADDR (1)
#define CLOCK_CNTL_DATA (2)
#define DAC_REGS        (0x30)
#define DAC_CNTL        (0x31)
#define MEM_ADDR_CONFIG (0x0D)

#define CRTC_H_TOTAL_DISP     0x00
#define CRTC_H_SYNC_STRT_WID  0x01
#define CRTC_V_TOTAL_DISP     0x02
#define CRTC_V_SYNC_STRT_WID  0x03
#define CRTC_VLINE_CRNT_VLINE 0x04
#define CRTC_OFF_PITCH        0x05
#define CRTC_INT_CNTL         0x06

#define OVR_CLR            0x10
#define OVR_WID_LEFT_RIGHT 0x11
#define OVR_WID_TOP_BOTTOM 0x12
#define CUR_CLR0           0x18
#define CUR_CLR1           0x19
#define CUR_OFFSET         0x1A
#define CUR_HORZ_VERT_POSN 0x1B
#define CUR_HORZ_VERT_OFF  0x1C

#define FIFO_STAT  0xC4
#define GUI_STAT   0xCE
#define HOST_CNTL  0x90
#define HOST_DATA0 0x80  // 16 registers

#define DST_OFF_PITCH 0x40
// #define DST_X            0x41
// #define DST_Y            0x42
#define DST_Y_X 0x43
// #define DST_WIDTH        0x44
// #define DST_HEIGHT       0x45
#define DST_HEIGHT_WIDTH 0x46
#define DST_X_WIDTH      0x47
#define DST_BRES_LNTH    0x48
#define DST_BRES_ERR     0x49
#define DST_BRES_INC     0x4A
#define DST_BRES_DEC     0x4B
#define DST_CNTL         0x4C

#define SRC_OFF_PITCH 0x60
// #define SRC_X              0x61
// #define SRC_Y              0x62
#define SRC_Y_X 0x63
// #define SRC_WIDTH1         0x64
// #define SRC_HEIGHT1        0x65
#define SRC_HEIGHT1_WIDTH1 0x66
// #define SRC_X_START        0x67
// #define SRC_Y_START        0x68
#define SRC_Y_X_START 0x69
// #define SRC_WIDTH2         0x6A
// #define SRC_HEIGHT2        0x6B
#define SRC_HEIGHT2_WIDTH2 0x6C
#define SRC_CNTL           0x6D

#define PAT_REG0 0xA0
#define PAT_REG1 0xA1

// PAT_CNTL
#define PAT_CNTL 0xA2
// These values clash with the same in GUI_TRAJ_CNTL
// #define PAT_MONO_EN    0x01
// #define PAT_CLR_4x2_EN 0x02
// #define PAT_CLR_8x1_EN 0x04

// #define SC_LEFT       0xA8
// #define SC_RIGHT      0xA9
#define SC_LEFT_RIGHT 0xAA
// #define SC_TOP        0xAB
// #define SC_BOTTOM     0xAC
#define SC_TOP_BOTTOM 0xAD

#define DP_BKGD_CLR  0xB0
#define DP_FRGD_CLR  0xB1
#define DP_WRITE_MSK 0xB2
#define DP_CHAIN_MSK 0xB3
#define DP_PIX_WIDTH 0xB4
#define DP_MIX       0xB5
#define DP_SRC       0xB6

#define CLR_CMP_CLR  0xC0
#define CLR_CMP_MSK  0xC1
#define CLR_CMP_CNTL 0xC2

#define CONTEXT_MASK      0xC8
#define CONTEXT_LOAD_CNTL 0xCB

//
#define GUI_TRAJ_CNTL 0xCC

#define HW_DEBUG                   0x1F
#define AUTO_BLKWRT_COLOR_DIS      BIT(8)
#define AUTO_BLKWRT_COLOR_DIS_MASK BIT(8)
#define AUTO_FF_DIS                BIT(12)
#define AUTO_FF_DIS_MASK           BIT(12)
#define AUTO_BLKWRT_DIS            BIT(13)
#define AUTO_BLKWRT_DIS_MASK       BIT(13)

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

#define CRTC_DBL_SCAN_EN      BIT(0)
#define CRTC_INTERLACE_EN     BIT(1)
#define CRTC_HSYNC_DIS        BIT(2)
#define CRTC_VSYNC_DIS        BIT(3)
#define CRTC_DISPLAY_DIS      BIT(6)
#define CRTC_DISPLAY_DIS_MASK BIT(6)
#define CRTC_PIX_WIDTH(x)     ((x) << 8)
#define CRTC_PIX_WIDTH_MASK   (0x7 << 8)

// These are CT/CT registers
#define CRTC_FIFO_OVERFILL(x)   ((x) << 14)
#define CRTC_FIFO_OVERFILL_MASK (0x3 << 14)
#define CRTC_FIFO_LWM(x)        ((x) << 16)
#define CRTC_FIFO_LWM_MASK      (0xF << 16)
#define CRTC_DISPREQ_ONLY       BIT(21)
#define CRTC_DISPREQ_ONLY_MASK  BIT(21)

#define CRTC_LOCK_REGS         BIT(22)
#define CRTC_LOCK_REGS_MASK    BIT(22)
#define CRTC_EXT_DISP_EN       BIT(24)
#define CRTC_EXT_DISP_EN_MASK  BIT(24)
#define CRTC_ENABLE            BIT(25)
#define CRTC_ENABLE_MASK       BIT(25)
#define CRTC_DISP_REQ_ENB      BIT(26)
#define CRTC_DISP_REQ_ENB_MASK BIT(26)
#define VGA_ATI_LINEAR         BIT(27)
#define VGA_ATI_LINEAR_MASK    BIT(27)
#define VGA_XCRT_CNT_EN        BIT(30)
#define VGA_XCRT_CNT_EN_MASK   BIT(30)

#define BUS_ROM_DIS         BIT(12)
#define BUS_ROM_DIS_MASK    BIT(12)
#define BUS_FIFO_ERR_INT_EN BIT(20)
#define BUS_FIFO_ERR_INT    BIT(21)
#define BUS_FIFO_ERR_AK     BIT(21)  // INT and ACK are the same bit, distiguished by R/W operation
#define BUS_HOST_ERR_INT_EN BIT(22)  // only in CT, not GT
#define BUS_HOST_ERR_INT    BIT(23)  // only in CT, not GT
#define BUS_HOST_ERR_AK     BIT(23)  // only in CT, not GT.  INT and ACK are the same bit, distiguished by R/W operation

typedef struct CardData
{
    struct Library *OpenPciBase;
    struct pci_dev *board;
    struct Node boardNode;
    char boardName[16];

    APTR ASM (*AllocCardMemDefault)(__REGA0(struct BoardInfo *bi), __REGD0(ULONG size), __REGD1(BOOL force),
                                    __REGD2(BOOL system), __REGD3(ULONG bytesperrow), __REGA1(struct ModeInfo *mi),
                                    __REGD7(RGBFTYPE));
} CardData_t;

STATIC_ASSERT(sizeof(CardData_t) < SIZEOF_MEMBER(BoardInfo_t, CardData), check_carddata_size);

typedef struct PLL
{
    const BYTE *multipliers;
    BYTE numMultipliers;
    UWORD maxVCO;  // in 10Khz
    UWORD minVCO;  // in 10Khz
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
    UBYTE N;     // feedback divider
} PLLValue_t;

enum PLL_REGS
{
    PLL_MPLL_CNTL     = 0,
    PLL_MACRO_CNTL    = 1,
    PLL_VPLL_CNTL     = 1,  // GT
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
    PLL_DLL_CNTL      = 12,  // GT
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


#define PLL_ADDR_MASK      (0x3F << 10)  // 6 bits on GT, used to be less on older chips
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

extern ULONG computePLLValues(const BoardInfo_t *bi, ULONG freqKhz10, const UBYTE *multipliers, WORD numMultipliers,
                              PLLValue_t *pllValues);
extern ULONG computeFrequencyKhz10(UWORD RefFreq, UWORD FBDiv, UWORD RefDiv, UBYTE PostDiv);
extern ULONG computeFrequencyKhz10FromPllValue(const BoardInfo_t *bi, const PLLValue_t *pllValues,
                                               const UBYTE *postDivMultipliers);

extern void ResetEngine(const BoardInfo_t *bi);

extern ChipFamily_t getChipFamily(UWORD deviceId);
extern const char *getChipFamilyName(ChipFamily_t family);

static INLINE UBYTE REGARGS readATIRegisterB(volatile UBYTE *regbase, LONG regIndex, WORD byteIndex,
                                             const char *regName)
{
    UBYTE value = readReg(regbase, DWORD_OFFSET(regIndex) + byteIndex);
    D(VERBOSE, "R %s_%ld -> 0x%02lx\n", regName, (LONG)byteIndex, (LONG)value);

    return value;
}

static INLINE ULONG REGARGS readATIRegisterL(volatile UBYTE *regbase, LONG regIndex, const char *regName)
{
    ULONG value = readRegL(regbase, DWORD_OFFSET(regIndex), regName);
    return value;
}

static INLINE ULONG REGARGS readATIMMIOL(volatile UBYTE *regbase, LONG regIndex, const char *regName)
{
    flushWrites();
    return readATIRegisterL(regbase, regIndex, regName);
}

static INLINE ULONG REGARGS readATIRegisterAndMaskL(volatile UBYTE *regbase, LONG regIndex, ULONG mask,
                                                    const char *regName)
{
    ULONG value       = readRegL(regbase, DWORD_OFFSET(regIndex), regName);
    ULONG valueMasked = value & mask;
    D(VERBOSE, "R %s -> 0x%08lx & 0x%08lx = 0x%08lx\n", regName, value, mask, valueMasked);

    return valueMasked;
}

static INLINE void REGARGS writeATIRegisterMaskB(volatile UBYTE *regbase, LONG regIndex, BYTE byteIndex, UBYTE mask,
                                                 UBYTE value, const char *regName)
{
    UBYTE regValue = readReg(regbase, DWORD_OFFSET(regIndex) + byteIndex);
    regValue &= ~mask;
    regValue |= value & mask;
    D(VERBOSE, "W %s_%ld <- 0x%02lx\n", regName, (LONG)byteIndex, (LONG)value);
    writeReg(regbase, DWORD_OFFSET(regIndex) + byteIndex, regValue);
}

static INLINE void REGARGS writeATIRegisterB(volatile UBYTE *regbase, LONG regIndex, BYTE byteIndex, UBYTE value,
                                             const char *regName)
{
    D(VERBOSE, "W %s_%ld <- 0x%02lx\n", regName, (LONG)byteIndex, (LONG)value);
    writeReg(regbase, DWORD_OFFSET(regIndex) + byteIndex, value);
}

static INLINE void REGARGS writeATIRegisterL(volatile UBYTE *regbase, LONG regIndex, ULONG value, const char *regName)
{
    writeRegL(regbase, DWORD_OFFSET(regIndex), value, regName);
}

static INLINE void REGARGS writeATIRegisterMaskL(volatile UBYTE *regbase, LONG regIndex, ULONG mask, ULONG value,
                                                 const char *regName)
{
    ULONG regValue = readRegLNoSwap(regbase, DWORD_OFFSET(regIndex), regName);

    mask     = swapl(mask);
    value    = swapl(value);
    regValue = (regValue & ~mask) | (mask & value);

    writeRegLNoSwap(regbase, DWORD_OFFSET(regIndex), regValue, regName);
}

static INLINE void REGARGS writeATIRegisterNoSwapL(volatile UBYTE *regbase, LONG regIndex, ULONG value,
                                                   const char *regName)
{
    writeRegLNoSwap(regbase, DWORD_OFFSET(regIndex), value, regName);
}

// FIXME reusing the same function for IO and MMIO shouldn't work because MMIOREGISTER_OFFSET and REGISTER_OFFSET might
// be different, but in practise they aren't. Refactor the code.
#define CHECK_BLKIO(regIndex, X)                                        \
do {                                                                \
        _Static_assert(regIndex < 0x40, "BLKIO only for regs < 0x040"); \
        X;                                                              \
} while (0)
#define R_BLKIO_B(regIndex, byteIndex)        readATIRegisterB(RegBase, regIndex, byteIndex, #regIndex)
#define R_BLKIO_L(regIndex)                   readATIRegisterL(RegBase, regIndex, #regIndex)
#define R_BLKIO_AND_L(regIndex, mask)         readATIRegisterAndMaskL(RegBase, regIndex, mask, #regIndex)
#define W_BLKIO_B(regIndex, byteIndex, value) writeATIRegisterB(RegBase, regIndex, byteIndex, value, #regIndex)
#define W_BLKIO_MASK_B(regIndex, byteIndex, mask, value) \
    writeATIRegisterMaskB(RegBase, regIndex, byteIndex, mask, value, #regIndex)
#define W_BLKIO_L(regIndex, value)                                      \
    do {                                                                \
        _Static_assert(regIndex < 0x40, "BLKIO only for regs < 0x040"); \
        writeATIRegisterL(RegBase, regIndex, value, #regIndex);         \
} while (0)
#define W_BLKIO_MASK_L(regIndex, mask, value) writeATIRegisterMaskL(RegBase, regIndex, mask, value, #regIndex)

#undef R_MMIO_L
#undef W_MMIO_L
#undef W_MMIO_MASK_L
#define R_MMIO_L(regIndex)                   readATIMMIOL(MMIOBase, regIndex, #regIndex)
#define W_MMIO_L(regIndex, value)            writeATIRegisterL(MMIOBase, regIndex, value, #regIndex)
#define W_MMIO_NOSWAP_L(regIndex, value)     writeATIRegisterNoSwapL(MMIOBase, regIndex, value, #regIndex)
#define W_MMIO_MASK_L(regIndex, mask, value) writeATIRegisterMaskL(MMIOBase, regIndex, mask, value, #regIndex)

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
