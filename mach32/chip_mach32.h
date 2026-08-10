#ifndef CHIP_MACH32_H
#define CHIP_MACH32_H

#include "common.h"
#include "mach32_ramdac.h"

#define MACH32_SUPPORTED_RGBFF (RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_R8G8B8 | RGBFF_B8G8R8)
/*    | RGBFF_A8R8G8B8 |           \
     RGBFF_A8B8G8R8 | RGBFF_R8G8B8A8 | RGBFF_B8G8R8A8*/

/* GE_PITCH / CRT_PITCH: 8-bit field in units of 8 pixels (REG688000-15). */
#define MACH32_GE_PITCH_MAX     255
#define MACH32_MAX_PITCH_PIXELS (MACH32_GE_PITCH_MAX * 8) /* 2040 */

typedef struct ChipData
{
    ULONG GEfgPen;
    ULONG GEbgPen;
    UBYTE GEmask;
    UBYTE GEdrawMode;
    UBYTE GEOp;                                 /* BlitterOp_t */
    UBYTE GEfmt;                                /* (ULONG)RGBFTYPE; ~0 = unknown / invalidated */
    struct RenderInfo srcDstRenderInfoCache[2]; /* 0=dst, 1=src */
    union {
        UWORD linePatternCache; /* last LinePtrn written to PATT_DATA for patterned lines */
        UBYTE patternCache[8];
    };
    ULONG patternCacheKey;
    UBYTE lineMode; /* 0 = invalid, 1 = solid (DP replace + mask=1), 2 = patterned mono stipple */
    /* Cached EXT_FIFO_STATUS after accounting for pending GE writes. */
    UWORD fifoSlotsCached;
    const struct RamdacOps *ramdacOps;
} ChipData_t;

STATIC_ASSERT(sizeof(ChipData_t) < SIZEOF_MEMBER(BoardInfo_t, ChipData), chipdata_fits_boardinfo);

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

STATIC_ASSERT(sizeof(CardData_t) < SIZEOF_MEMBER(BoardInfo_t, CardData), carddata_fits_boardinfo);

#ifdef __cplusplus
#include "mach32_driver.hpp"
#endif

/*
 * Mach32 coprocessor I/O indices (16-bit word ports at RegisterBase unless noted).
 * ATI REG688000-15 (1993) §8–9, Appendix A.
 *
 * Trailing comment uses R / W / RW for host-visible access. Same index often denotes
 * different logical registers on read vs write — see both halves of the comment.
 *
 * Not listed as #defines: IBM 8514/A-style sub-registers written via the MEM_CNTL
 * multiplexer at 0xBEE8 (MIN_AXIS_PCNT, SCISSOR_*, PATTERN_*, PIXEL_CNTL, etc.; §8-43–8-50).
 */

#define ADVFUNC_CNTL        0x4AE8  /* W */
#define ALU_BG_FN           0xB6EE  /* W — same index as R_H_SYNC_STRT (R) */
#define ALU_FG_FN           0xBAEE  /* W — same index as R_H_SYNC_WID (R) */
#define APERTURE_CNTL       0x6AEE  /* R/W — Mach32; MAX_WAITSTATES on some steppings */
#define BKGD_COLOR          0xA2E8  /* W */
#define BKGD_MIX            0xB6E8  /* W */
#define BOUNDS_BOTTOM       0x7EEE  /* R — same index as MISC_CNTL (W) */
#define BOUNDS_LEFT         0x72EE  /* R — same index as GE_OFFSET_HI (W) */
#define BOUNDS_RIGHT        0x7AEE  /* R — same index as EXT_GE_CONFIG (W) */
#define BOUNDS_TOP          0x76EE  /* R — same index as GE_PITCH (W) */
#define BRES_COUNT          0x96EE  /* RW */
#define CHIP_ID             0xFAEE  /* R */
#define CLOCK_SEL           0x4AEE  /* RW */
#define CMD                 0x9AE8  /* W — same index as GE_STAT (R) */
#define CMP_COLOR           0xB2E8  /* W */
#define CONFIG_STATUS_1     0x12EE  /* R — same index as HORZ_CURSOR_POSN (W) on Mach32 */
#define CONFIG_STATUS_2     0x16EE  /* R — same index as VERT_CURSOR_POSN (W) on Mach32 */
#define CRT_OFFSET_HI       0x2EEE  /* W */
#define CRT_OFFSET_LO       0x2AEE  /* W */
#define CRT_PITCH           0x26EE  /* W */
#define CURSOR_COLOR_0      0x1AEE  /* W — same index as FIFO_TEST_DATA (R); low byte / word §9-80 */
#define CURSOR_COLOR_1      0x1AEF  /* W — byte port §9-80 */
#define CURSOR_OFFSET_HI    0x0EEE  /* W — offset to cursor def in DWORDs from display base; see §9-78 */
#define CURSOR_OFFSET_LO    0x0AEE  /* W */
#define CURSOR_ENA          BIT(15) /* CURSOR_OFFSET_HI — REG688000-15 §9-78 */
#define CUR_X               0x86E8  /* W */
#define CUR_Y               0x82E8  /* W */
#define DAC_DATA            0x02ED  /* R/W VGA 0x3C9  RS<1:0> = 0b01 */
#define DAC_MASK            0x02EA  /* R/W VGA 0x3C6  RS<1:0> = 0b10 */
#define DAC_R_INDEX         0x02EB  /* R/W VGA 0x3C7  RS<1:0> = 0b11 */
#define DAC_W_INDEX         0x02EC  /* R/W VGA 0x3C8  RS<1:0> = 0b00 */
#define DEST_CMP_FN         0xEEEE  /* W */
#define DEST_COLOR_CMP_MASK 0xF2EE  /* R/W */
#define DEST_X_END          0xAAEE  /* W */
#define DEST_X_START        0xA6EE  /* W */
#define DEST_Y_END          0xAEEE  /* W */
#define DISP_CNTL           0x22E8  /* W */
#define DISP_STATUS         0x02E8  /* R — same index as H_TOTAL (W) */
/* DISP_STATUS (02E8 R) — REG688000-15 §8-8 */
#define DISP_STATUS_RGB_TEST  BIT(0)
#define DISP_STATUS_VERT_SYNC BIT(1) /* live; polarity mode-dependent */
#define DISP_STATUS_LINE_SYNC BIT(2) /* toggles each H retrace */
#define DP_CONFIG           0xCEEE  /* W — same index as VERT_LINE_CNTR (R) */
#define ERR_TERM            0x92E8  /* W */
#define EXT_CURSOR_COLOR_0  0x3AEE  /* W — same index as FIFO_TEST_TAG (R) */
#define EXT_CURSOR_COLOR_1  0x3EEE  /* W */
#define EXT_FIFO_STATUS     0x9AEE  /* R — same index as LINEDRAW_INDEX (W) */
#define EXT_GE_CONFIG       0x7AEE  /* W */
#define EXT_GE_STATUS       0x62EE  /* R — same index as HORZ_OVERSCAN (W) */
#define EXT_SHORT_STROKE    0xC6EE  /* W — same index as R_V_DISP (R) */
#define FIFO_TEST_DATA      0x1AEE  /* R */
#define FIFO_TEST_TAG       0x3AEE  /* R */
#define FRGD_COLOR          0xA6E8  /* W */
#define FRGD_MIX            0xBAE8  /* W */
#define GE_OFFSET_HI        0x72EE  /* W — same index as BOUNDS_LEFT (R) */
#define GE_OFFSET_LO        0x6EEE  /* W */
#define GE_PITCH            0x76EE  /* W — same index as BOUNDS_TOP (R) */
#define GE_STAT             0x9AE8  /* R — same index as CMD (W) */
#define GENENA              0x46E8  /* W — add-on only (§Appendix A) */
#define H_DISP              0x06E8  /* W */
#define H_SYNC_STRT         0x0AE8  /* W */
#define H_SYNC_WID          0x0EE8  /* W */
#define H_TOTAL             0x02E8  /* W — same index as DISP_STATUS (R) */
#define HORZ_CURSOR_OFFSET  0x1EEE  /* W */
#define HORZ_CURSOR_POSN    0x12EE  /* W — same index as CONFIG_STATUS_1 (R) */
#define HORZ_OVERSCAN       0x62EE  /* W — same index as EXT_GE_STATUS (R) */
#define LINEDRAW            0xFEEE  /* W */
#define LINEDRAW_INDEX      0x9AEE  /* W — same index as EXT_FIFO_STATUS (R) */
#define LINEDRAW_OPT        0xA2EE  /* R/W */
#define LOCAL_CNTL          0x32EE  /* R/W */
#define MAJ_AXIS_PCNT       0x96E8  /* W */
#define MAX_WAITSTATES      0x6AEE  /* R/W — Mach32; PCI MISC_CONT / APERTURE_CNTL on some steppings */
#define MEM_BNDRY           0x42EE  /* W */
#define MEM_CFG             0x5EEE  /* R/W — §9-75 */
#define MISC_CNTL           0x7EEE  /* W — same index as BOUNDS_BOTTOM (R) */
#define MISC_OPTIONS        0x36EE  /* R/W — Mach32; mach8 FIFO_OPT (W) at same index */
#define PATT_DATA           0x8EEE  /* W — same index as R_EXT_GE_CONFIG (R) */
#define PATT_DATA_INDEX     0x82EE  /* R/W */
#define PATT_INDEX          0xD6EE  /* W */
#define PATT_LENGTH         0xD2EE  /* W — same index as R_V_SYNC_WID (R) */
/* Linear mono/color pattern: PATT_LENGTH[4:0] = (pixel_length - 1); bits 7,15 clear — §9-60 */
#define PATT_LENGTH_MONO16 15u    /* 16-pixel line stipple */
#define PCI_CNTL           0x22EE /* R/W — “DAC_CONT (PCI)” in appendix */
#define PIX_TRANS          0xE2E8 /* R/W */
#define R_EXT_GE_CONFIG    0x8EEE /* R — same index as PATT_DATA (W) */
#define R_H_SYNC_STRT      0xB6EE /* R — same index as ALU_BG_FN (W) */
#define R_H_SYNC_WID       0xBAEE /* R — same index as ALU_FG_FN (W) */
#define R_H_TOTAL_DISP     0xB2EE /* R — same index as SRC_X_START (W) */
#define R_MISC_CNTL        0x92EE /* R */
#define R_SRC_X            0xDAEE /* R — same index as SCISSOR_LEFT (W) */
#define R_SRC_Y            0xDEEE /* R — same index as SCISSOR_TOP (W) */
#define R_V_DISP           0xC6EE /* R — same index as EXT_SHORT_STROKE (W) */
#define R_V_SYNC_STRT      0xCAEE /* R — same index as SCAN_TO_X (W) */
#define R_V_SYNC_WID       0xD2EE /* R — same index as PATT_LENGTH (W) */
#define R_V_TOTAL          0xC2EE /* R — same index as SRC_Y_DIR (W) */
#define RD_MASK            0xAEE8 /* W */
#define SCAN_TO_X          0xCAEE /* W — same index as R_V_SYNC_STRT (R) */
#define SCISSOR_BOTTOM     0xE6EE /* W */
#define SCISSOR_LEFT       0xDAEE /* W — same index as R_SRC_X (R) */
#define SCISSOR_RIGHT      0xE2EE /* W */
#define SCISSOR_TOP        0xDEEE /* W — same index as R_SRC_Y (R) */
#define SCRATCH_PAD0       0x52EE /* R/W */
#define SCRATCH_PAD1       0x56EE /* R/W */
#define SHADOW_CTL         0x46EE /* W */
#define SHADOW_SET         0x5AEE /* W */
#define SHORT_STROKE       0x9EE8 /* W */
#define SRC_X_DEST_X       0x8EE8 /* W — SRC_X / DEST_X / DIASTP §8-52 */
#define SRC_X_END          0xBEEE /* W */
#define SRC_X_START        0xB2EE /* W — same index as R_H_TOTAL_DISP (R) */
#define SRC_Y_DEST_Y       0x8AE8 /* W — SRC_Y / DEST_Y / AXSTP §8-53 */
#define SRC_Y_DIR          0xC2EE /* W — same index as R_V_TOTAL (R) */
#define SUBSYS_CNTL        0x42E8 /* W — same index as SUBSYS_STATUS (R) */
#define SUBSYS_STATUS      0x42E8 /* R — same index as SUBSYS_CNTL (W) */
/* SUBSYS_STATUS / SUBSYS_CNTL — REG688000-15 §8-17–8-20 */
#define SUBSYS_VBLANK_INT BIT(0)
#define SUBSYS_VBLANK_ACK BIT(0)
#define SUBSYS_VBLANK_ENA BIT(8)
#define V_DISP             0x16E8 /* W */
#define V_SYNC_STRT        0x1AE8 /* W */
#define V_SYNC_WID         0x1EE8 /* W */
#define V_TOTAL            0x12E8 /* W */
#define VERT_CURSOR_OFFSET 0x1EEF /* W */
#define VERT_CURSOR_POSN   0x16EE /* W — same index as CONFIG_STATUS_2 (R) */
#define VERT_LINE_CNTR     0xCEEE /* R — same index as DP_CONFIG (W) */
#define VERT_OVERSCAN      0x66EE /* W */
#define WRT_MASK           0xAAE8 /* W */

/* MULTI_FUNC_CNTL (0xBEE8): minor-axis / height for rectangle fills — REG688000-15 §8-43–8-50 */
#define MULTI_FUNC_CNTL 0xBEE8

// The following are accessible via RD_REG_DT, the number indicates the index
#define MIN_AXIS_PCNT 0x0
#define SCISSORS_T    0x1
#define SCISSORS_L    0x2
#define SCISSORS_B    0x3
#define SCISSORS_R    0x4
#define MEM_CNTL      0x5  // BEE8 Index 5
#define PATTERN_H     0x9
#define PATTERN_L     0x8

#define PIXEL_CNTL          0xA
#define MASK_BIT_SRC_ONE    (0b00 << 6)
#define MASK_BIT_SRC_PATTEN (0b01 << 6)
#define MASK_BIT_SRC_CPU    (0b10 << 6)
#define MASK_BIT_SRC_BITMAP (0b11 << 6)

#define MULT_MISC2 0xD
#define MULT_MISC  0xE
#define READ_SEL   0xF

/* CMD (9AE8 W): rectangle fill opcode — REG688000-15 §8-47, Appendix A */
#define DRAW_RW_READ               0x0000
#define DRAW_RW_WRITE              0x0001
#define DRAW_PIXEL_MODE_SINGLE     0x0000
#define DRAW_PIXEL_MODE_NIBBLE     0x0002
#define DRAW_DIR_TYPE_XY           BIT(3) /* 0=DEGREE, 1=XY (REG688000-15 §8-31) */
#define DRAW_DRAW                  BIT(4)
#define DRAW_LAST_PEL_OFF          BIT(2)
#define DRAW_OCTANT_YPOS           BIT(7)
#define DRAW_OCTANT_YMAJOR         BIT(6)
#define DRAW_OCTANT_XPOS           BIT(5)
#define DRAW_CPU_WAIT              BIT(8)
#define DRAW_DATA_WIDTH_16BIT      BIT(9)
#define DRAW_LSB_FIRST             BIT(12)
#define DRAW_OPCODE_NOOP           0
#define DRAW_OPCODE_LINE           (1 << 13)
#define DRAW_OPCODE_FILL_RECT_HOR  (2 << 13)
#define DRAW_OPCODE_FILL_RECT_VPIX (3 << 13)
#define DRAW_OPCODE_FILL_RECT_VNIB (4 << 13)
#define DRAW_OPCODE_DRAW_POLY_LINE (5 << 13)
#define DRAW_OPCODE_BLIT           (5 << 13)

/*
 * Horizontal rectangle fill, +x/+y octants, draw enable — must match hardware (tested 0x50B3;
 * composed DRAW_* bits above do not yield the same value on all steppings).
 */
#define CMD_RECT_FILL 0x50B3u

/* LINEDRAW_OPT (A2EE R/W): REG688000-15 §9-21–9-22 */
#define LINEDRAW_OPT_POLY_MODE    BIT(0)
#define LINEDRAW_OPT_LAST_PEL_OFF BIT(2)
#define LINEDRAW_OPT_DIR_TYPE     BIT(3) /* 0=Bresenham/Octant, 1=Length/Degree */
#define LINEDRAW_OPT_OCTANT_XDIR  BIT(5)
#define LINEDRAW_OPT_OCTANT_YMAJ  BIT(6)
#define LINEDRAW_OPT_OCTANT_YDIR  BIT(7)

/* DP_CONFIG (CEEE W) — REG688000-15 §9-14..9-16 */
#define DP_CONFIG_RW_WRITE         BIT(0) /* 0=read trajectory, 1=write trajectory */
#define DP_CONFIG_POLY_FILL_MODE   BIT(1)
#define DP_CONFIG_READ_MODE_MONO   BIT(2) /* 0=color host data, 1=mono host data */
#define DP_CONFIG_DRAW             BIT(4) /* 0=disable draw, 1=enable draw */
#define DP_CONFIG_MONO_SRC_SHIFT   5u
#define DP_CONFIG_MONO_SRC_MASK    (3u << DP_CONFIG_MONO_SRC_SHIFT)
#define DP_CONFIG_MONO_SRC(x)      ((UWORD)(x) << DP_CONFIG_MONO_SRC_SHIFT)
#define DP_CONFIG_BG_SRC_SHIFT     7u
#define DP_CONFIG_BG_SRC_MASK      (3u << DP_CONFIG_BG_SRC_SHIFT)
#define DP_CONFIG_BG_SRC(x)        ((UWORD)(x) << DP_CONFIG_BG_SRC_SHIFT)
#define DP_CONFIG_DATA_WIDTH_16BIT BIT(9)  /* 0=8-bit, 1=16-bit */
#define DP_CONFIG_LSB_FIRST        BIT(12) /* 0=MSB first, 1=LSB first */
#define DP_CONFIG_FG_SRC_SHIFT     13u
#define DP_CONFIG_FG_SRC_MASK      (7u << DP_CONFIG_FG_SRC_SHIFT)
#define DP_CONFIG_FG_SRC(x)        ((UWORD)(x) << DP_CONFIG_FG_SRC_SHIFT)

/* DP_CONFIG.MONO_SRC values */
#define DP_MONO_SRC_ONE      0u /* Always “1” */
#define DP_MONO_SRC_PATTERN  1u /* Mono pattern register */
#define DP_MONO_SRC_PIXTRANS 2u /* Pixel transfer register (PIX_TRANS) */
#define DP_MONO_SRC_BLIT_SRC 3u /* VRAM blit source */

/* DP_CONFIG.{FG,BG}_COLOR_SRC values (REG688000-15 §9-15) */
#define DP_COLOR_SRC_BKGD     0u /* Background color register */
#define DP_COLOR_SRC_FRGD     1u /* Foreground color register */
#define DP_COLOR_SRC_PIXTRANS 2u /* Pixel transfer register */
#define DP_COLOR_SRC_BLIT_SRC 3u /* VRAM blit source */

/* Common composed modes used by this driver */
#define DP_CONFIG_REPLACE                                                                     \
    (DP_CONFIG_RW_WRITE | DP_CONFIG_DRAW | DP_CONFIG_DATA_WIDTH_16BIT | DP_CONFIG_LSB_FIRST | \
     DP_CONFIG_BG_SRC(DP_COLOR_SRC_FRGD) | DP_CONFIG_FG_SRC(DP_COLOR_SRC_FRGD) | DP_CONFIG_MONO_SRC(DP_MONO_SRC_ONE))

#define DP_CONFIG_BLIT                                                                        \
    (DP_CONFIG_RW_WRITE | DP_CONFIG_DRAW | DP_CONFIG_DATA_WIDTH_16BIT | DP_CONFIG_LSB_FIRST | \
     DP_CONFIG_FG_SRC(DP_COLOR_SRC_BLIT_SRC) | DP_CONFIG_MONO_SRC(DP_MONO_SRC_ONE))

/* Template blit: like REPLACE, but DP_CONFIG.MONO_SRC=PIX_TRANS. */
#define DP_CONFIG_TEMPLATE                                                                    \
    (DP_CONFIG_RW_WRITE | DP_CONFIG_DRAW | DP_CONFIG_DATA_WIDTH_16BIT | DP_CONFIG_LSB_FIRST | \
     DP_CONFIG_BG_SRC(DP_COLOR_SRC_BKGD) | DP_CONFIG_FG_SRC(DP_COLOR_SRC_FRGD) |              \
     DP_CONFIG_MONO_SRC(DP_MONO_SRC_PIXTRANS))

/* 8x8 mono pattern blit: fg/bg from color regs, mono from pattern regs. */
#define DP_CONFIG_MONO_PATTERN                                                                \
    (DP_CONFIG_RW_WRITE | DP_CONFIG_DRAW | DP_CONFIG_DATA_WIDTH_16BIT | DP_CONFIG_LSB_FIRST | \
     DP_CONFIG_BG_SRC(DP_COLOR_SRC_BKGD) | DP_CONFIG_FG_SRC(DP_COLOR_SRC_FRGD) |              \
     DP_CONFIG_MONO_SRC(DP_MONO_SRC_PATTERN))

/* MEM_CFG (5EEE) — REG688000-15 §9-75 (PCI: bits 1:0 and 15:8 only) */
#define MEM_APERT_SEL_MASK  0x0003u
#define MEM_APERT_SEL_OFF   0u
#define MEM_APERT_SEL_1MB   1u
#define MEM_APERT_SEL_4MB   2u
#define MEM_APERT_PAGE_MASK 0x000Cu /* reserved on PCI */
#define MEM_APERT_LOC_MASK  0xFF00u
#define MEM_APERT_LOC_SHIFT 8u

/* SUBSYS_STATUS (42E8 R) bit 7 — §8-20 (strap / boundary hint, not full VRAM size) */
#define SUBSYS_MEMSIZE_BIT BIT(7)

// CLOCK_SEL register (4AEE)
#define PASS_THROUGH_DISABLE_MASK BIT(0)
#define PASS_THROUGH_DISABLE      BIT(0)
#define CLK_SEL_MASK              (0xF << 2)
#define CLK_SEL(x)                ((x) << 2)
#define CLK_DIV_MASK              BIT(6)
#define CLK_DIV                   BIT(6)
#define VFIFO_DEPTH_MASK          (0xF << 8)
#define VFIFO_DEPTH(x)            ((x) << 8)

// EXT_GE_CONFIG register
#define PIXEL_WIDTH_MASK (0x3 << 4)
#define PIXEL_WIDTH(x)   ((x) << 4)

#define EXT_GE_16BIT_555          0u /* (5,5,5) R14:10 G9:5 B4:0 */
#define EXT_GE_16BIT_565          1u /* (5,6,5) R15:11 G10:5 B4:0 */
#define _16_BIT_COLOR_MODE_MASK   (0x3 << 6)
#define _16_BIT_COLOR_MODE(x)     ((x) << 6)
#define _24_BIT_COLOR_CONFIG_MASK (1 << 9)
#define _24_BIT_COLOR_CONFIG(x)   ((x) << 9)
#define _24_BIT_COLOR_ORDER_MASK  (1 << 10)
#define _24_BIT_COLOR_ORDER(x)    ((x) << 10)
#define DAC_EXT_ADDR_MASK         (0x3 << 12)
#define DAC_EXT_ADDR(x)           ((x) << 12)
#define DAC_8BIT_EN_MASK          BIT(14)
#define DAC_8BIT_EN               BIT(14)
#define DISPLAY_PIXEL_SIZE_MASK   BIT(11)
#define DISPLAY_PIXEL_SIZE        BIT(11)
#define DRAW_PIXEL_SIZE_MASK      BIT(15)
#define DRAW_PIXEL_SIZE           BIT(15)
/* EXT_GE_CONFIG[8]: display-path mux; keep cleared for Brooktree Bt481 (no parallel pixel mux). */
#define MULTIPLEX_PIXELS_MASK BIT(8)
#define MULTIPLEX_PIXELS      BIT(8)

// MISC_OPTIONS register
#define MEM_SIZE_ALIAS_MASK (0x3 << 2)
#define MEM_SIZE_ALIAS(x)   ((x) << 2)

#ifdef __cplusplus
/* Prefer Mach32Driver::writeBee8 / writeExtGeConfigMask in .cpp. */
#else
#define W_BEE8(idx, value) W_IO_W(0xBEE8, ((idx << 12) | (value)))
static inline void readModifyWrite(const BoardInfo_t *bi, LONG readReg, LONG writeReg, UWORD mask, UWORD value,
                                   const char *writeRegName)
{
    REGBASE();
    UWORD regValue = readRegWNoSwap(RegBase, readReg, writeRegName);
    regValue       = (regValue & ~SWAPW_IO(mask)) | SWAPW_IO(value & mask);
    writeRegWNoSwap(RegBase, writeReg, regValue, writeRegName);
}
#define W_EXT_GE_CONFIG_MASK(mask, value) \
    readModifyWrite(bi, R_EXT_GE_CONFIG, EXT_GE_CONFIG, mask, value, "[R_]EXT_GE_CONFIG")
#endif

/* DISP_CNTL (22E8) — REG688000-15 §8-7 */
#define Y_CONTROL_SHIFT  1u
#define Y_CONTROL_MASK   (3u << Y_CONTROL_SHIFT)
#define Y_CONTROL_NORMAL (1u << Y_CONTROL_SHIFT) /* bits 2:1 = 01: “bit 2 skipped” / normal line counter */
#define DOUBLE_SCAN_BIT  BIT(3)
#define INTERLACE_BIT    BIT(4)
#define ENA_DISPLAY_MASK 0x0060u
#define CRT_RESET        (2u << 5)
#define CRT_ENABLED      (1u << 5)

/* HORZ_OVERSCAN (62EE) — REG688000-15 §9-71 */
#define SYN_CONT_SEL BIT(13)
#define HSYN_CONT    BIT(14)
#define VSYN_CONT    BIT(15)

#ifdef __cplusplus
extern "C" {
#endif
void dumpMach32Eeprom(BoardInfo_t *bi);
BOOL InitChip(__REGA0(struct BoardInfo *bi));
#ifdef __cplusplus
}
#endif

#endif  // CHIP_MACH32_H
