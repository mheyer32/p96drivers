#ifndef MACH32_REGS_HPP
#define MACH32_REGS_HPP

/*
 * Mach32 absolute I/O port indices (namespace IoReg + unscoped enum Id).
 * One enumerator per distinct address; R/W aliases share the value.
 */

#include <exec/types.h>

#ifdef DBG
#include "common.h"
#endif

/* Prefer the write-side / primary name when R and W share an index. */
#define MACH32_IO_REG_LIST(X)             \
	X(H_TOTAL, 0x02E8)                \
	X(H_DISP, 0x06E8)                 \
	X(H_SYNC_STRT, 0x0AE8)            \
	X(H_SYNC_WID, 0x0EE8)             \
	X(CURSOR_OFFSET_LO, 0x0AEE)       \
	X(CURSOR_OFFSET_HI, 0x0EEE)       \
	X(V_TOTAL, 0x12E8)                \
	X(HORZ_CURSOR_POSN, 0x12EE)       \
	X(V_DISP, 0x16E8)                 \
	X(VERT_CURSOR_POSN, 0x16EE)       \
	X(V_SYNC_STRT, 0x1AE8)            \
	X(CURSOR_COLOR_0, 0x1AEE)         \
	X(CURSOR_COLOR_1, 0x1AEF)         \
	X(V_SYNC_WID, 0x1EE8)             \
	X(HORZ_CURSOR_OFFSET, 0x1EEE)     \
	X(VERT_CURSOR_OFFSET, 0x1EEF)     \
	X(DISP_CNTL, 0x22E8)              \
	X(PCI_CNTL, 0x22EE)               \
	X(CRT_PITCH, 0x26EE)              \
	X(CRT_OFFSET_LO, 0x2AEE)          \
	X(CRT_OFFSET_HI, 0x2EEE)          \
	X(LOCAL_CNTL, 0x32EE)             \
	X(MISC_OPTIONS, 0x36EE)           \
	X(EXT_CURSOR_COLOR_0, 0x3AEE)     \
	X(EXT_CURSOR_COLOR_1, 0x3EEE)     \
	X(SUBSYS_CNTL, 0x42E8)            \
	X(MEM_BNDRY, 0x42EE)              \
	X(ADVFUNC_CNTL, 0x4AE8)           \
	X(CLOCK_SEL, 0x4AEE)              \
	X(SHADOW_CTL, 0x46EE)             \
	X(SCRATCH_PAD0, 0x52EE)           \
	X(SCRATCH_PAD1, 0x56EE)           \
	X(SHADOW_SET, 0x5AEE)             \
	X(MEM_CFG, 0x5EEE)                \
	X(EXT_GE_STATUS, 0x62EE)          \
	X(VERT_OVERSCAN, 0x66EE)          \
	X(APERTURE_CNTL, 0x6AEE)          \
	X(GE_OFFSET_LO, 0x6EEE)           \
	X(GE_OFFSET_HI, 0x72EE)           \
	X(GE_PITCH, 0x76EE)               \
	X(EXT_GE_CONFIG, 0x7AEE)          \
	X(MISC_CNTL, 0x7EEE)              \
	X(CUR_Y, 0x82E8)                  \
	X(PATT_DATA_INDEX, 0x82EE)        \
	X(CUR_X, 0x86E8)                  \
	X(SRC_Y_DEST_Y, 0x8AE8)           \
	X(SRC_X_DEST_X, 0x8EE8)           \
	X(PATT_DATA, 0x8EEE)              \
	X(ERR_TERM, 0x92E8)               \
	X(BRES_COUNT, 0x96EE)             \
	X(GE_STAT, 0x9AE8)                \
	X(EXT_FIFO_STATUS, 0x9AEE)        \
	X(SHORT_STROKE, 0x9EE8)           \
	X(BKGD_COLOR, 0xA2E8)             \
	X(LINEDRAW_OPT, 0xA2EE)           \
	X(FRGD_COLOR, 0xA6E8)             \
	X(DEST_X_START, 0xA6EE)           \
	X(WRT_MASK, 0xAAE8)               \
	X(DEST_X_END, 0xAAEE)             \
	X(RD_MASK, 0xAEE8)                \
	X(DEST_Y_END, 0xAEEE)             \
	X(CMP_COLOR, 0xB2E8)              \
	X(SRC_X_START, 0xB2EE)            \
	X(BKGD_MIX, 0xB6E8)               \
	X(ALU_BG_FN, 0xB6EE)              \
	X(FRGD_MIX, 0xBAE8)               \
	X(ALU_FG_FN, 0xBAEE)              \
	X(MULTI_FUNC_CNTL, 0xBEE8)        \
	X(SRC_X_END, 0xBEEE)              \
	X(SRC_Y_DIR, 0xC2EE)              \
	X(EXT_SHORT_STROKE, 0xC6EE)       \
	X(SCAN_TO_X, 0xCAEE)              \
	X(DP_CONFIG, 0xCEEE)              \
	X(PATT_LENGTH, 0xD2EE)            \
	X(PATT_INDEX, 0xD6EE)             \
	X(SCISSOR_LEFT, 0xDAEE)           \
	X(SCISSOR_TOP, 0xDEEE)            \
	X(PIX_TRANS, 0xE2E8)              \
	X(SCISSOR_RIGHT, 0xE2EE)          \
	X(SCISSOR_BOTTOM, 0xE6EE)         \
	X(DEST_CMP_FN, 0xEEEE)            \
	X(DEST_COLOR_CMP_MASK, 0xF2EE)    \
	X(CHIP_ID, 0xFAEE)                \
	X(LINEDRAW, 0xFEEE)               \
	X(DAC_MASK, 0x02EA)               \
	X(DAC_R_INDEX, 0x02EB)            \
	X(DAC_W_INDEX, 0x02EC)            \
	X(DAC_DATA, 0x02ED)

namespace IoReg {
enum Id : LONG {
#define MACH32_IO_REG_ENUM(name, val) name = val,
	MACH32_IO_REG_LIST(MACH32_IO_REG_ENUM)
#undef MACH32_IO_REG_ENUM
	/* Same-index aliases (read vs write / alternate names). */
	DISP_STATUS        = H_TOTAL,
	HORZ_OVERSCAN      = EXT_GE_STATUS,
	SUBSYS_STATUS      = SUBSYS_CNTL,
	CONFIG_STATUS_1    = HORZ_CURSOR_POSN,
	CONFIG_STATUS_2    = VERT_CURSOR_POSN,
	MAX_WAITSTATES     = APERTURE_CNTL,
	CMD                = GE_STAT,
	LINEDRAW_INDEX     = EXT_FIFO_STATUS,
	R_EXT_GE_CONFIG    = PATT_DATA,
	R_V_DISP           = EXT_SHORT_STROKE,
	VERT_LINE_CNTR     = DP_CONFIG,
	R_V_TOTAL          = SRC_Y_DIR,
	R_V_SYNC_WID       = PATT_LENGTH,
	BOUNDS_LEFT        = GE_OFFSET_HI,
	BOUNDS_TOP         = GE_PITCH,
	BOUNDS_RIGHT       = EXT_GE_CONFIG,
	BOUNDS_BOTTOM      = MISC_CNTL,
	FIFO_TEST_DATA     = CURSOR_COLOR_0,
	FIFO_TEST_TAG      = EXT_CURSOR_COLOR_0,
};

#define MACH32_IO_REG_ID(name, val) static const Id id_##name = static_cast<Id>(val);
MACH32_IO_REG_LIST(MACH32_IO_REG_ID)
#undef MACH32_IO_REG_ID
static const Id id_DISP_STATUS     = DISP_STATUS;
static const Id id_HORZ_OVERSCAN   = HORZ_OVERSCAN;
static const Id id_SUBSYS_STATUS   = SUBSYS_STATUS;
static const Id id_CONFIG_STATUS_1 = CONFIG_STATUS_1;
static const Id id_CONFIG_STATUS_2 = CONFIG_STATUS_2;
static const Id id_MAX_WAITSTATES  = MAX_WAITSTATES;
static const Id id_CMD             = CMD;
static const Id id_R_EXT_GE_CONFIG = R_EXT_GE_CONFIG;
static const Id id_R_V_DISP        = R_V_DISP;
static const Id id_VERT_LINE_CNTR  = VERT_LINE_CNTR;
static const Id id_R_V_TOTAL       = R_V_TOTAL;

#ifdef DBG
static INLINE const char *regName(Id id)
{
	switch (id) {
#define MACH32_IO_REG_NAME(name, val) \
	case name:                    \
		return #name;
		MACH32_IO_REG_LIST(MACH32_IO_REG_NAME)
#undef MACH32_IO_REG_NAME
	default:
		return "?";
	}
}
#endif
} /* namespace IoReg */

#endif /* MACH32_REGS_HPP */
