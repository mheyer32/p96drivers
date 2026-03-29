#ifndef CHIP_MACH32_H
#define CHIP_MACH32_H

#include "common.h"
#include "mach32_ramdac.h"

typedef struct ChipData
{
    const struct RamdacOps *ramdacOps;
    ULONG flags;
    ULONG reserved[13];
} ChipData_t;

STATIC_ASSERT(sizeof(ChipData_t) < SIZEOF_MEMBER(BoardInfo_t, ChipData), mach32_chipdata);

typedef struct CardData
{
    volatile UBYTE *legacyIOBase;
    struct Library *OpenPciBase;
    struct pci_dev *board;
    struct Node boardNode;
    char boardName[16];

} CardData_t;

STATIC_ASSERT(sizeof(CardData_t) < SIZEOF_MEMBER(BoardInfo_t, CardData), mach32_carddata);

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

#define ADVFUNC_CNTL        0x4AE8 /* W */
#define ALU_BG_FN           0xB6EE /* W — same index as R_H_SYNC_STRT (R) */
#define ALU_FG_FN           0xBAEE /* W — same index as R_H_SYNC_WID (R) */
#define BKGD_COLOR          0xA2E8 /* W */
#define BKGD_MIX            0xB6E8 /* W */
#define BOUNDS_BOTTOM       0x7EEE /* R — same index as MISC_CNTL (W) */
#define BOUNDS_LEFT         0x72EE /* R — same index as GE_OFFSET_HI (W) */
#define BOUNDS_RIGHT        0x7AEE /* R — same index as EXT_GE_CONFIG (W) */
#define BOUNDS_TOP          0x76EE /* R — same index as GE_PITCH (W) */
#define BRES_COUNT          0x96EE /* RW */
#define CHIP_ID             0xFAEE /* R */
#define CLOCK_SEL           0x4AEE /* RW */
#define CMD                 0x9AE8 /* W — same index as GE_STAT (R) */
#define CMP_COLOR           0xB2E8 /* W */
#define CONFIG_STATUS_1     0x12EE /* R — same index as HORZ_CURSOR_POSN (W) on Mach32 */
#define CONFIG_STATUS_2     0x16EE /* R — same index as VERT_CURSOR_POSN (W) on Mach32 */
#define CRT_OFFSET_HI       0x2EEE /* W */
#define CRT_OFFSET_LO       0x2AEE /* W */
#define CRT_PITCH           0x26EE /* W */
#define CURSOR_COLOR_0      0x1AEE /* W — same index as FIFO_TEST_DATA (R); low byte / word §9-80 */
#define CURSOR_COLOR_1      0x1AEF /* W — byte port §9-80 */
#define CURSOR_OFFSET_HI    0x0EEE /* W */
#define CURSOR_OFFSET_LO    0x0AEE /* W */
#define CUR_X               0x86E8 /* W */
#define CUR_Y               0x82E8 /* W */
#define DAC_DATA            0x02ED /* R/W VGA 0x3C9 */
#define DAC_MASK            0x02EA /* R/W VGA 0x3C6 */
#define DAC_R_INDEX         0x02EB /* R/W VGA 0x3C7 */
#define DAC_W_INDEX         0x02EC /* R/W VGA 0x3C8 */
#define DEST_CMP_FN         0xEEEE /* W */
#define DEST_COLOR_CMP_MASK 0xF2EE /* R/W */
#define DEST_X_END          0xAAEE /* W */
#define DEST_X_START        0xA6EE /* W */
#define DEST_Y_END          0xAEEE /* W */
#define DISP_CNTL           0x22E8 /* W */
#define DISP_STATUS         0x02E8 /* R — same index as H_TOTAL (W) */
#define DP_CONFIG           0xCEEE /* W — same index as VERT_LINE_CNTR (R) */
#define ERR_TERM            0x92E8 /* W */
#define EXT_CURSOR_COLOR_0  0x3AEE /* W — same index as FIFO_TEST_TAG (R) */
#define EXT_CURSOR_COLOR_1  0x3EEE /* W */
#define EXT_FIFO_STATUS     0x9AEE /* R — same index as LINEDRAW_INDEX (W) */
#define EXT_GE_CONFIG       0x7AEE /* W */
#define EXT_GE_STATUS       0x62EE /* R — same index as HORZ_OVERSCAN (W) */
#define EXT_SHORT_STROKE    0xC6EE /* W — same index as R_V_DISP (R) */
#define FIFO_TEST_DATA      0x1AEE /* R */
#define FIFO_TEST_TAG       0x3AEE /* R */
#define FRGD_COLOR          0xA6E8 /* W */
#define FRGD_MIX            0xBAE8 /* W */
#define GE_OFFSET_HI        0x72EE /* W — same index as BOUNDS_LEFT (R) */
#define GE_OFFSET_LO        0x6EEE /* W */
#define GE_PITCH            0x76EE /* W — same index as BOUNDS_TOP (R) */
#define GE_STAT             0x9AE8 /* R — same index as CMD (W) */
#define GENENA              0x46E8 /* W — add-on only (§Appendix A) */
#define H_DISP              0x06E8 /* W */
#define HORZ_CURSOR_OFFSET  0x1EEE /* W */
#define HORZ_CURSOR_POSN    0x12EE /* W — same index as CONFIG_STATUS_1 (R) */
#define HORZ_OVERSCAN       0x62EE /* W — same index as EXT_GE_STATUS (R) */
#define H_SYNC_STRT         0x0AE8 /* W */
#define H_SYNC_WID          0x0EE8 /* W */
#define H_TOTAL             0x02E8 /* W — same index as DISP_STATUS (R) */
#define LINEDRAW            0xFEEE /* W */
#define LINEDRAW_INDEX      0x9AEE /* W — same index as EXT_FIFO_STATUS (R) */
#define LINEDRAW_OPT        0xA2EE /* R/W */
#define LOCAL_CNTL          0x32EE /* R/W */
#define MAJ_AXIS_PCNT       0x96E8 /* W */
#define MAX_WAITSTATES      0x6AEE /* R/W — Mach32; PCI MISC_CONT / APERTURE_CNTL on some steppings */
#define MEM_BNDRY           0x42EE /* W */
#define MEM_CFG             0x5EEE /* R/W */
#define MISC_CNTL           0x7EEE /* W — same index as BOUNDS_BOTTOM (R) */
#define MISC_OPTIONS        0x36EE /* R/W — Mach32; mach8 FIFO_OPT (W) at same index */
#define PATT_DATA           0x8EEE /* W — same index as R_EXT_GE_CONFIG (R) */
#define PATT_DATA_INDEX     0x82EE /* R/W */
#define PATT_INDEX          0xD6EE /* W */
#define PATT_LENGTH         0xD2EE /* W — same index as R_V_SYNC_WID (R) */
#define PCI_CNTL            0x22EE /* R/W — “DAC_CONT (PCI)” in appendix */
#define PIX_TRANS           0xE2E8 /* R/W */
#define R_EXT_GE_CONFIG     0x8EEE /* R — same index as PATT_DATA (W) */
#define R_H_SYNC_STRT       0xB6EE /* R — same index as ALU_BG_FN (W) */
#define R_H_SYNC_WID        0xBAEE /* R — same index as ALU_FG_FN (W) */
#define R_H_TOTAL_DISP      0xB2EE /* R — same index as SRC_X_START (W) */
#define R_MISC_CNTL         0x92EE /* R */
#define R_SRC_X             0xDAEE /* R — same index as SCISSOR_LEFT (W) */
#define R_SRC_Y             0xDEEE /* R — same index as SCISSOR_TOP (W) */
#define R_V_DISP            0xC6EE /* R — same index as EXT_SHORT_STROKE (W) */
#define R_V_SYNC_STRT       0xCAEE /* R — same index as SCAN_TO_X (W) */
#define R_V_SYNC_WID        0xD2EE /* R — same index as PATT_LENGTH (W) */
#define R_V_TOTAL           0xC2EE /* R — same index as SRC_Y_DIR (W) */
#define RD_MASK             0xAEE8 /* W */
#define SCAN_TO_X           0xCAEE /* W — same index as R_V_SYNC_STRT (R) */
#define SCISSOR_BOTTOM      0xE6EE /* W */
#define SCISSOR_LEFT        0xDAEE /* W — same index as R_SRC_X (R) */
#define SCISSOR_RIGHT       0xE2EE /* W */
#define SCISSOR_TOP         0xDEEE /* W — same index as R_SRC_Y (R) */
#define SCRATCH_PAD0        0x52EE /* R/W */
#define SCRATCH_PAD1        0x56EE /* R/W */
#define SHADOW_CTL          0x46EE /* W */
#define SHADOW_SET          0x5AEE /* W */
#define SHORT_STROKE        0x9EE8 /* W */
#define SRC_X_DEST_X        0x8EE8 /* W — SRC_X / DEST_X / DIASTP §8-52 */
#define SRC_X_END           0xBEEE /* W */
#define SRC_X_START         0xB2EE /* W — same index as R_H_TOTAL_DISP (R) */
#define SRC_Y_DEST_Y        0x8AE8 /* W — SRC_Y / DEST_Y / AXSTP §8-53 */
#define SRC_Y_DIR           0xC2EE /* W — same index as R_V_TOTAL (R) */
#define SUBSYS_CNTL         0x42E8 /* W — same index as SUBSYS_STATUS (R) */
#define SUBSYS_STATUS       0x42E8 /* R — same index as SUBSYS_CNTL (W) */
#define V_DISP              0x16E8 /* W */
#define V_SYNC_STRT         0x1AE8 /* W */
#define V_SYNC_WID          0x1EE8 /* W */
#define V_TOTAL             0x12E8 /* W */
#define VERT_CURSOR_OFFSET  0x1EEF /* W */
#define VERT_CURSOR_POSN    0x16EE /* W — same index as CONFIG_STATUS_2 (R) */
#define VERT_LINE_CNTR      0xCEEE /* R — same index as DP_CONFIG (W) */
#define VERT_OVERSCAN       0x66EE /* W */
#define WRT_MASK            0xAAE8 /* W */

#define MEM_CNTL 5  // BEE8 Index 5

// CLOCK_SEL register
#define PASS_THROUGH_MASK BIT(0)
#define PASS_THROUGH      BIT(0)
#define CLK_SEL_MASK      (0xF << 2)
#define CLK_SEL(x)        ((x) <)
#define CLK_DIV_MASK      BIT(6)
#define CLK_DIV           BIT(6)
#define VFIFO_DEPTH_MASK  (0xF << 8)
#define VFIFO_DEPTH(x)    ((x) << 8)

// EXT_GE_CONFIG register
#define PIXEL_WIDTH_MASK        (0x3 << 4)
#define PIXEL_WIDTH(x)          ((x) << 4)
#define _16_BIT_COLOR_MODE_MASK (0x3 << 6)
#define _16_BIT_COLOR_MODE(x)   ((x) << 6)
#define _24_COLOR_ORDER_MASK    BIT(9)
#define _24_COLOR_ORDER         BIT(9)
#define DAC_EXT_ADDR_MASK       (0x3 << 12)
#define DAC_EXT_ADDR(x)         ((x) << 12)
#define DAC_8BIT_EN_MASK        BIT(14)
#define DAC_8BIT_EN             BIT(14)

#define W_BEE8(idx, value) W_IO_W(0xBEE8, ((idx << 12) | (value)))

static inline readModifyWrite(const BoardInfo_t *bi, LONG readReg, LONG writeReg, UWORD mask, UWORD value, const char *writeRegName)
{
    REGBASE();
    UWORD regValue = readRegWNoSwap(RegBase, readReg, writeRegName);
    regValue = (regValue & ~SWAPW_IO(mask)) | SWAPW(value & mask);
    writeRegWNoSwap(RegBase, writeReg, regValue, writeRegName);
}
#define W_EXT_GE_CONFIG_MASK(mask, value) readModifyWrite(bi, R_EXT_GE_CONFIG, EXT_GE_CONFIG, mask, value, "[R_]EXT_GE_CONFIG")

#endif  // CHIP_MACH32_H
