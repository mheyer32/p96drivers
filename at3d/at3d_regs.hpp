#ifndef AT3D_REGS_HPP
#define AT3D_REGS_HPP

#include <exec/types.h>

#ifdef DBG
#include "common.h"
#endif

/* Distinct MMIO byte offsets used by AbsRegAperture (REGISTER_OFFSET=0). */
#define AT3D_MMIO_REG_LIST(X) \
	X(DRAW_CMD, 0x040)     \
	X(CLIP_CTRL, 0x030)    \
	X(RASTEROP, 0x046)     \
	X(FRGD_COLOR, 0x060)   \
	X(BKGD_COLOR, 0x064)   \
	X(EXT_DAC_STATUS, 0x1FC)

namespace MmioReg {
enum Id : LONG {
#define AT3D_MMIO_REG_ENUM(name, val) name = val,
	AT3D_MMIO_REG_LIST(AT3D_MMIO_REG_ENUM)
#undef AT3D_MMIO_REG_ENUM
};
#ifdef DBG
static INLINE const char *regName(Id id)
{
	switch (id) {
#define AT3D_MMIO_REG_NAME(name, val) case name: return #name;
		AT3D_MMIO_REG_LIST(AT3D_MMIO_REG_NAME)
#undef AT3D_MMIO_REG_NAME
	default: return "?";
	}
}
#endif
}
#endif
