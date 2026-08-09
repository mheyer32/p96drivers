#ifndef MACH64_COMMON_H
#define MACH64_COMMON_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BoardInfo;

typedef enum __attribute__((packed)) ChipFamily
{
    UNKNOWN,
    MACH64GX,
    MACH64CT,  // integrated DAC/PLL; dual 8MB LE+BE aperture; waitFifo
    MACH64VT,  // dual 8MB LE+BE (same aperture layout as CT)
    MACH64GT,
    MACH64GM  // Rage 3 XL
} ChipFamily_t;

STATIC_ASSERT(sizeof(ChipFamily_t) == 1, chipfamily_is_byte);

#define SCRATCH_REG0    (0x20)
#define SCRATCH_REG1    (0x21)
#define BUS_CNTL        (0x28)
#define MEM_CNTL        (0x2C)
#define MEM_VGA_WP_SEL  (0x2D) /* write pages for A000/A800 (32K each) */
#define MEM_VGA_RP_SEL  (0x2E) /* read pages for A000/A800 */
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

/* CONFIG_CNTL bit19: disable VGA decode for accelerator CRT.
 * On this GX (P/N 113-25517-100) it also blocks Expansion ROM reads at the
 * OpenPCI ROM BAR — clear before parseRomHeader on warm reinit, then set again. */
#define CFG_VGA_DIS            BIT(19)
#define CFG_VGA_DIS_MASK       BIT(19)
#define CFG_MEM_VGA_AP_EN      BIT(2)
#define CFG_MEM_VGA_AP_EN_MASK BIT(2)
#define CFG_MEM_AP_LOC(x)      ((x) << 4)
#define CFG_MEM_AP_LOC_MASK    (0x3FF << 4)
/* CFG_MEM_AP_SIZE bits 0–1 (RRG §3-9). Same numeric value, different meaning:
 * GX letter a: 0=off, 1=4M, 2=8M, 3=reserved
 * CT letter f: 0/1 reserved, 2=2×8M (LE @0 + BE @+8M), 3=reserved */
#define CFG_MEM_AP_SIZE(x)     ((x) & 3)
#define CFG_MEM_AP_SIZE_MASK   (0x3)
#define CFG_MEM_AP_SIZE_8M     2 /* GX: single 8M; CT/VT+: 2×8M dual */

/* CONFIG_STAT0 — GX/CX (ATI.TXT / RRG 3-10). Do not write on cold GX. */
#define CFG_BUS_TYPE_GX(x)         ((x) & 7)
#define CFG_BUS_TYPE_GX_MASK       (0x7)
#define CFG_BUS_TYPE_GX_ISA        0
#define CFG_BUS_TYPE_GX_EISA       1
#define CFG_BUS_TYPE_GX_VLB        6
#define CFG_BUS_TYPE_GX_PCI        7

#define CFG_MEM_TYPE_GX(x)         (((x) & 7) << 3)
#define CFG_MEM_TYPE_GX_MASK       (0x7 << 3)
#define CFG_MEM_TYPE_GX_DRAM4      0 /* DRAM 256Kx4 */
#define CFG_MEM_TYPE_GX_VRAM       1 /* VRAM 256Kx4/x8/x16 */
#define CFG_MEM_TYPE_GX_VRAM_SSR   2 /* VRAM short shift */
#define CFG_MEM_TYPE_GX_DRAM16     3
#define CFG_MEM_TYPE_GX_GDRAM      4
#define CFG_MEM_TYPE_GX_EVRAM      5
#define CFG_MEM_TYPE_GX_EVRAM_SSR  6

#define CFG_DUAL_CAS_EN_GX         BIT(6)
#define CFG_DUAL_CAS_EN_GX_MASK    BIT(6)
#define CFG_INIT_DAC_TYPE_GX(x)    (((x) & 7) << 9)
#define CFG_INIT_DAC_TYPE_GX_MASK  (0x7 << 9)
#define CFG_VGA_EN_GX              BIT(23)
#define CFG_VGA_EN_GX_MASK         BIT(23)

/* CONFIG_STAT0 — CT layout (RRG §3-12 letter z/aa+), not GX §3-10.
 * Low byte redefined vs GX: do not decode with CFG_*_GX.
 * CT mem_type (z): 0=disable, 1=DRAM, 2=EDO, 3–7 reserved on CT. */
#define CFG_MEM_TYPE_CT(x)         ((x) & 7)
#define CFG_MEM_TYPE_CT_MASK       (0x7)
#define CFG_MEM_TYPE_CT_DISABLE    0b000
#define CFG_MEM_TYPE_CT_DRAM       0b001
#define CFG_MEM_TYPE_CT_EDO        0b010
/* VT/GT+ encodings of the same field (CFG_MEM_TYPE_*) — invalid on CT. */
#define CFG_MEM_TYPE_VT_PSEUDO_EDO 0b011
#define CFG_MEM_TYPE_VT_SDRAM      0b100
#define CFG_MEM_TYPE_VT_SGRAM      0b101
#define CFG_MEM_TYPE_VT_SDRAM_32BIT 0b110

#define CFG_DUAL_CAS_EN_CT         BIT(3)
#define CFG_DUAL_CAS_EN_CT_MASK    BIT(3)
#define CFG_CLOCK_EN_CT            BIT(5)
#define CFG_CLOCK_EN_CT_MASK       BIT(5)

#define GEN_OVS_EN          BIT(5)
#define GEN_OVS_EN_MASK     BIT(5)
#define GEN_CUR_ENABLE      BIT(7)
#define GEN_CUR_ENABLE_MASK BIT(7)

/* DAC_CNTL (RRG) — shared by GX external DAC SetDAC and integrated-DAC InitChip. */
#define DAC_BLANKING        BIT(2) /* 1 = 7.5 IRE pedestal */
#define DAC_BLANKING_MASK   BIT(2)
#define DAC_8BIT_EN         BIT(8)
#define DAC_8BIT_EN_MASK    BIT(8)
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
#define CRTC_CSYNC_EN         BIT(4) /* composite sync on HSYNC — breaks separate H/V monitors */
#define CRTC_PIC_BY_2_EN      BIT(5)
#define CRTC_DISPLAY_DIS      BIT(6)
#define CRTC_DISPLAY_DIS_MASK BIT(6)
#define CRTC_PIX_WIDTH(x)     ((x) << 8)
#define CRTC_PIX_WIDTH_MASK   (0x7 << 8)
#define CRTC_BYTE_PIX_ORDER   BIT(11)
#define CRTC_BYTE_PIX_ORDER_MASK BIT(11)

/* Shared (GX + CT): display FIFO LWM — DRAM configs only (ATI.TXT / RRG). */
#define CRTC_FIFO_LWM(x)        ((x) << 16)
#define CRTC_FIFO_LWM_MASK      (0xF << 16)

/* CT CRTC_GEN_CNTL bits 20–22 (RRG-S00700-05 §3-18). Not present on GX.
 * Do not confuse with later VT/GT names (128KAP / DISPREQ_ONLY / LOCK_REGS). */
#define CRTC_EXTRA_PIPE_DELAY_CT      BIT(20)
#define CRTC_EXTRA_PIPE_DELAY_CT_MASK BIT(20)
#define CRTC_EXTRA_FIFO_READ_CT       BIT(21)
#define CRTC_EXTRA_FIFO_READ_CT_MASK  BIT(21)
#define CRTC_VSTATUS_VSYNC_CT         BIT(22) /* 0=VSTATUS, 1=VSYNC */
#define CRTC_VSTATUS_VSYNC_CT_MASK    BIT(22)

/* VT/GT+ (not CT RRG): names at the same bit numbers. */
#define VGA_128KAP_PAGING_VT       BIT(20)
#define VGA_128KAP_PAGING_VT_MASK  BIT(20)
#define CRTC_DISPREQ_ONLY_VT       BIT(21)
#define CRTC_DISPREQ_ONLY_VT_MASK  BIT(21)
#define CRTC_FIFO_OVERFILL_VT(x)   ((x) << 14)
#define CRTC_FIFO_OVERFILL_VT_MASK (0x3 << 14)
#define CRTC_LOCK_REGS             BIT(22)
#define CRTC_LOCK_REGS_MASK        BIT(22)

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
    volatile UBYTE *legacyIOBase;
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

#ifdef __cplusplus
namespace PllReg {
enum Id : UBYTE {
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
}
#endif

#define PLL_ADDR_MASK      (0x3F << 10)  // 6 bits on GT, used to be less on older chips
#define PLL_ADDR(x)        ((ULONG)(x) << 10)
#define PLL_DATA_MASK      (0xFF << 16)
#define PLL_DATA(x)        ((x) << 16)
#define PLL_WR_ENABLE      BIT(9)
#define PLL_WR_ENABLE_MASK BIT(9)
#define CLOCK_SEL_MASK     (0x3)
#define CLOCK_SEL(x)       ((x) & CLOCK_SEL_MASK)
#define CLOCK_STROBE       BIT(6)
#define CLOCK_STROBE_MASK  BIT(6)

#ifdef __cplusplus
extern void WritePLL(struct BoardInfo *bi, PllReg::Id pllAddr, UBYTE pllDataMask, UBYTE pllData);
extern UBYTE ReadPLL(struct BoardInfo *bi, PllReg::Id pllAddr);
#else
extern void WritePLL(struct BoardInfo *bi, UBYTE pllAddr, UBYTE pllDataMask, UBYTE pllData);
extern UBYTE ReadPLL(struct BoardInfo *bi, UBYTE pllAddr);
#endif

#define WRITE_PLL(pllAddr, data)            WritePLL(drv, (pllAddr), 0xFF, (data))
#define WRITE_PLL_MASK(pllAddr, mask, data) WritePLL(drv, (pllAddr), (mask), (data))
#define READ_PLL(pllAddr)                   ReadPLL(drv, (pllAddr))

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

static INLINE BOOL mach64ChipFamilySupported(ChipFamily_t family)
{
#if MACH64_PCI_RETRY
    return family == MACH64VT || family == MACH64GT || family == MACH64GM;
#else
    return family == MACH64GX || family == MACH64CT;
#endif
}

/* MMIO sits at top of the LE 8MB window (base+0x7FFC00), not at end of a 16MB BAR.
 * CT VRAM ≤4MB → no overlap with that MMIO hole. */
static INLINE ULONG mach64MmioOffsetInBar0(ULONG bar0Size)
{
    ULONG aper = (bar0Size >= 0x800000UL) ? 0x800000UL : bar0Size;
    return aper - 1024UL;
}

/* waitFifo is Mach64Driver::waitFifo (mach64_driver.hpp). */

UBYTE getAsicVersion(const BoardInfo_t *bi);
BOOL isAsiclessThanV4(const BoardInfo_t *bi);

#ifdef __cplusplus
}
#endif

#endif  // MACH64_COMMON_H
