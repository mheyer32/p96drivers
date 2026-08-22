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

/* Absolute I/O port indices: IoReg in mach32_regs.hpp.
 * BEE8 indexed regs (MIN_AXIS_PCNT, PIXEL_CNTL, …) remain below as indices.
 */

#define CURSOR_ENA BIT(15) /* CURSOR_OFFSET_HI — REG688000-15 §9-78 */

#define DISP_STATUS_RGB_TEST  BIT(0)
#define DISP_STATUS_VERT_SYNC BIT(1) /* live; polarity mode-dependent */
#define DISP_STATUS_LINE_SYNC BIT(2) /* toggles each H retrace */

#define PATT_LENGTH_MONO16 15u /* 16-pixel line stipple */

/* SUBSYS_STATUS / SUBSYS_CNTL — REG688000-15 §8-17–8-20 */
#define SUBSYS_VBLANK_INT BIT(0)
#define SUBSYS_VBLANK_ACK BIT(0)
#define SUBSYS_VBLANK_ENA BIT(8)

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
