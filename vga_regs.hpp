#ifndef VGA_REGS_HPP
#define VGA_REGS_HPP

#include <exec/types.h>

#ifdef DBG
#include "common.h"
#endif

/*
 * Standard VGA I/O ports (byte addresses). Names avoid common.h macros
 * (CRTC_IDX, SEQ_DATA, MISC_W, …) so this header stays safe after common.h.
 */
#define VGA_REG_LIST(X)                 \
	X(ATTR_AD, 0x3C0)               \
	X(ATTR_DATA_R, 0x3C1)           \
	X(MISC_OUT_W, 0x3C2)            \
	X(VGA_ENABLE, 0x3C3)            \
	X(SEQ_INDEX, 0x3C4)             \
	X(SEQ_VALUE, 0x3C5)             \
	X(DAC_PEL_MASK, 0x3C6)          \
	X(DAC_RD_INDEX, 0x3C7)          \
	X(DAC_WR_INDEX, 0x3C8)          \
	X(DAC_PEL_DATA, 0x3C9)          \
	X(FEATURE_CTL_R, 0x3CA)         \
	X(MISC_OUT_R, 0x3CC)            \
	X(GRC_INDEX, 0x3CE)             \
	X(GRC_VALUE, 0x3CF)             \
	X(CRTC_INDEX, 0x3D4)            \
	X(CRTC_VALUE, 0x3D5)            \
	X(INSTAT1, 0x3DA)

namespace VgaReg {
enum Id : LONG {
#define VGA_REG_ENUM(name, val) name = val,
	VGA_REG_LIST(VGA_REG_ENUM)
#undef VGA_REG_ENUM
	/* Attribute controller write data shares 0x3C0 with ATTR_AD. */
	ATTR_DATA_W = ATTR_AD,
};
#define VGA_REG_ID(name, val) static const Id id_##name = name;
VGA_REG_LIST(VGA_REG_ID)
#undef VGA_REG_ID
static const Id id_ATTR_DATA_W = ATTR_DATA_W;

#ifdef DBG
static INLINE const char *regName(Id id)
{
	switch (id) {
#define VGA_REG_NAME(name, val) \
	case name:                  \
		return #name;
		VGA_REG_LIST(VGA_REG_NAME)
#undef VGA_REG_NAME
	default:
		return "?";
	}
}
#endif
} /* namespace VgaReg */

#endif /* VGA_REGS_HPP */
