#ifndef S3_REGS_HPP
#define S3_REGS_HPP

#include <exec/types.h>

#ifdef DBG
#include "common.h"
#endif

/* Absolute GE/status ports used by AbsRegAperture (REGISTER_OFFSET / MMIOREGISTER_OFFSET). */
#define S3_IO_REG_LIST(X) \
	X(GP_STAT, 0x9AE8)

namespace IoReg {
enum Id : LONG {
#define S3_IO_REG_ENUM(name, val) name = val,
	S3_IO_REG_LIST(S3_IO_REG_ENUM)
#undef S3_IO_REG_ENUM
};
#ifdef DBG
static INLINE const char *regName(Id id)
{
	switch (id) {
#define S3_IO_REG_NAME(name, val) case name: return #name;
		S3_IO_REG_LIST(S3_IO_REG_NAME)
#undef S3_IO_REG_NAME
	default: return "?";
	}
}
#endif
}

namespace MmioReg {
enum Id : LONG {
#define S3_MMIO_REG_ENUM(name, val) name = val,
	S3_IO_REG_LIST(S3_MMIO_REG_ENUM)
#undef S3_MMIO_REG_ENUM
};
#ifdef DBG
static INLINE const char *regName(Id id)
{
	switch (id) {
#define S3_MMIO_REG_NAME(name, val) case name: return #name;
		S3_IO_REG_LIST(S3_MMIO_REG_NAME)
#undef S3_MMIO_REG_NAME
	default: return "?";
	}
}
#endif
}

#endif
