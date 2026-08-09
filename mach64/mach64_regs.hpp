#ifndef MACH64_REGS_HPP
#define MACH64_REGS_HPP

/*
 * Mach64 dword-index register IDs (namespace + unscoped enum Id).
 * Names match ATI programming manuals (FIFO_STAT, CRTC_GEN_CNTL, …).
 *
 * When this header is included, the matching #defines from mach64_common.h are
 * undefined so MmioReg::FIFO_STAT is not macro-expanded. Plain C TUs that never
 * include this header keep the #defines. In .cpp: using namespace MmioReg; then
 * write FIFO_STAT instead of MmioReg::FIFO_STAT (keep BlkIoReg:: when both used).
 */

#include <exec/types.h>

#ifdef CRTC_H_TOTAL_DISP
#undef CRTC_H_TOTAL_DISP
#undef CRTC_H_SYNC_STRT_WID
#undef CRTC_V_TOTAL_DISP
#undef CRTC_V_SYNC_STRT_WID
#undef CRTC_VLINE_CRNT_VLINE
#undef CRTC_OFF_PITCH
#undef CRTC_INT_CNTL
#undef CRTC_GEN_CNTL
#undef OVR_CLR
#undef OVR_WID_LEFT_RIGHT
#undef OVR_WID_TOP_BOTTOM
#undef CUR_CLR0
#undef CUR_CLR1
#undef CUR_OFFSET
#undef CUR_HORZ_VERT_POSN
#undef CUR_HORZ_VERT_OFF
#undef HW_DEBUG
#undef SCRATCH_REG0
#undef SCRATCH_REG1
#undef CLOCK_CNTL
#undef BUS_CNTL
#undef MEM_CNTL
#undef MEM_VGA_WP_SEL
#undef MEM_VGA_RP_SEL
#undef DAC_REGS
#undef DAC_CNTL
#undef GEN_TEST_CNTL
#undef CONFIG_CNTL
#undef CONFIG_CHIP_ID
#undef CONFIG_STAT0
#undef MEM_ADDR_CONFIG
#undef EXT_MEM_CNTL
#undef DSP_CONFIG
#undef DSP_ON_OFF
#undef DST_OFF_PITCH
#undef DST_Y_X
#undef DST_HEIGHT_WIDTH
#undef DST_X_WIDTH
#undef DST_BRES_LNTH
#undef DST_BRES_ERR
#undef DST_BRES_INC
#undef DST_BRES_DEC
#undef DST_CNTL
#undef SRC_OFF_PITCH
#undef SRC_Y_X
#undef SRC_HEIGHT1_WIDTH1
#undef SRC_Y_X_START
#undef SRC_HEIGHT2_WIDTH2
#undef SRC_CNTL
#undef HOST_DATA0
#undef HOST_CNTL
#undef PAT_REG0
#undef PAT_REG1
#undef PAT_CNTL
#undef SC_LEFT_RIGHT
#undef SC_TOP_BOTTOM
#undef DP_BKGD_CLR
#undef DP_FRGD_CLR
#undef DP_WRITE_MSK
#undef DP_CHAIN_MSK
#undef DP_PIX_WIDTH
#undef DP_MIX
#undef DP_SRC
#undef CLR_CMP_CLR
#undef CLR_CMP_MSK
#undef CLR_CMP_CNTL
#undef FIFO_STAT
#undef CONTEXT_MASK
#undef CONTEXT_LOAD_CNTL
#undef GUI_TRAJ_CNTL
#undef GUI_STAT
#endif

namespace MmioReg {
enum Id : LONG {
	CRTC_H_TOTAL_DISP     = 0x00,
	CRTC_H_SYNC_STRT_WID  = 0x01,
	CRTC_V_TOTAL_DISP     = 0x02,
	CRTC_V_SYNC_STRT_WID  = 0x03,
	CRTC_VLINE_CRNT_VLINE = 0x04,
	CRTC_OFF_PITCH        = 0x05,
	CRTC_INT_CNTL         = 0x06,
	CRTC_GEN_CNTL         = 0x07,

	OVR_CLR            = 0x10,
	OVR_WID_LEFT_RIGHT = 0x11,
	OVR_WID_TOP_BOTTOM = 0x12,
	CUR_CLR0           = 0x18,
	CUR_CLR1           = 0x19,
	CUR_OFFSET         = 0x1A,
	CUR_HORZ_VERT_POSN = 0x1B,
	CUR_HORZ_VERT_OFF  = 0x1C,

	HW_DEBUG        = 0x1F,
	SCRATCH_REG0    = 0x20,
	SCRATCH_REG1    = 0x21,
	CLOCK_CNTL      = 0x24,
	BUS_CNTL        = 0x28,
	MEM_CNTL        = 0x2C,
	MEM_VGA_WP_SEL  = 0x2D,
	MEM_VGA_RP_SEL  = 0x2E,
	DAC_REGS        = 0x30,
	DAC_CNTL        = 0x31,
	GEN_TEST_CNTL   = 0x34,
	CONFIG_CNTL     = 0x37,
	CONFIG_CHIP_ID  = 0x38,
	CONFIG_STAT0    = 0x39,
	MEM_ADDR_CONFIG = 0x0D,

	DST_OFF_PITCH    = 0x40,
	DST_Y_X          = 0x43,
	DST_HEIGHT_WIDTH = 0x46,
	DST_X_WIDTH      = 0x47,
	DST_BRES_LNTH    = 0x48,
	DST_BRES_ERR     = 0x49,
	DST_BRES_INC     = 0x4A,
	DST_BRES_DEC     = 0x4B,
	DST_CNTL         = 0x4C,

	SRC_OFF_PITCH      = 0x60,
	SRC_Y_X            = 0x63,
	SRC_HEIGHT1_WIDTH1 = 0x66,
	SRC_Y_X_START      = 0x69,
	SRC_HEIGHT2_WIDTH2 = 0x6C,
	SRC_CNTL           = 0x6D,

	HOST_DATA0 = 0x80,
	HOST_CNTL  = 0x90,

	PAT_REG0 = 0xA0,
	PAT_REG1 = 0xA1,
	PAT_CNTL = 0xA2,

	SC_LEFT_RIGHT = 0xAA,
	SC_TOP_BOTTOM = 0xAD,

	DP_BKGD_CLR  = 0xB0,
	DP_FRGD_CLR  = 0xB1,
	DP_WRITE_MSK = 0xB2,
	DP_CHAIN_MSK = 0xB3,
	DP_PIX_WIDTH = 0xB4,
	DP_MIX       = 0xB5,
	DP_SRC       = 0xB6,

	CLR_CMP_CLR  = 0xC0,
	CLR_CMP_MSK  = 0xC1,
	CLR_CMP_CNTL = 0xC2,

	FIFO_STAT         = 0xC4,
	CONTEXT_MASK      = 0xC8,
	CONTEXT_LOAD_CNTL = 0xCB,
	GUI_TRAJ_CNTL     = 0xCC,
	GUI_STAT          = 0xCE,
};
}

namespace BlkIoReg {
enum Id : LONG {
	CRTC_H_TOTAL_DISP = 0x00,
	CRTC_GEN_CNTL     = 0x07,
	DSP_CONFIG        = 0x08,
	DSP_ON_OFF        = 0x09,
	MEM_ADDR_CONFIG   = 0x0D,
	OVR_CLR           = 0x10,
	HW_DEBUG          = 0x1F,
	SCRATCH_REG0      = 0x20,
	SCRATCH_REG1      = 0x21,
	CLOCK_CNTL        = 0x24,
	LT_GIO            = 0x2A,
	BUS_CNTL          = 0x28,
	EXT_MEM_CNTL      = 0x2B,
	MEM_CNTL          = 0x2C,
	DAC_REGS          = 0x30,
	DAC_CNTL          = 0x31,
	GEN_TEST_CNTL     = 0x34,
	CONFIG_CNTL       = 0x37,
	CONFIG_CHIP_ID    = 0x38,
	CONFIG_STAT0      = 0x39,
};
}

namespace SparseIoReg {
enum Id : LONG {
	CONFIG_CNTL = 0x1A,
};
}


#ifdef DBG
inline const char *regName(MmioReg::Id r)
{
	switch (r) {
	case MmioReg::FIFO_STAT: return "FIFO_STAT";
	case MmioReg::GUI_STAT: return "GUI_STAT";
	case MmioReg::CRTC_GEN_CNTL: return "CRTC_GEN_CNTL";
	case MmioReg::DP_PIX_WIDTH: return "DP_PIX_WIDTH";
	case MmioReg::DP_WRITE_MSK: return "DP_WRITE_MSK";
	case MmioReg::CONFIG_CNTL: return "CONFIG_CNTL";
	case MmioReg::BUS_CNTL: return "BUS_CNTL";
	case MmioReg::MEM_CNTL: return "MEM_CNTL";
	case MmioReg::GEN_TEST_CNTL: return "GEN_TEST_CNTL";
	case MmioReg::HOST_DATA0: return "HOST_DATA0";
	default: return "MmioReg::Id";
	}
}
#endif

#endif /* MACH64_REGS_HPP */
