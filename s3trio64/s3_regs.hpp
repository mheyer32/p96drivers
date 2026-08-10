#ifndef S3_REGS_HPP
#define S3_REGS_HPP

#include <exec/types.h>

#ifdef DBG
#include "common.h"
#endif

/*
 * S3 GE / chip-specific absolute ports. Standard VGA ports live in vga_regs.hpp.
 * Many GE ports remain as #defines in chip_s3trio64.cpp (MMIO_ONLY remaps some);
 * cast those with S3_IO_ID / S3_MMIO_ID.
 */
#define S3_IO_REG_LIST(X) \
    X(GP_STAT, 0x9AE8)    \
    X(MULTI_FUNC_CNTL, 0xBEE8)

#define S3_MMIO_REG_LIST(X)    \
    X(GP_STAT, 0x9AE8)         \
    X(MULTI_FUNC_CNTL, 0xBEE8) \
    X(SERIAL_PORT, 0xFF20)

namespace IoReg {
enum Id : LONG
{
#define S3_IO_REG_ENUM(name, val) name = val,
    S3_IO_REG_LIST(S3_IO_REG_ENUM)
#undef S3_IO_REG_ENUM
};
#define S3_IO_REG_ID(name, val) static const Id id_##name = name;
S3_IO_REG_LIST(S3_IO_REG_ID)
#undef S3_IO_REG_ID
#ifdef DBG
static INLINE const char *regName(Id id)
{
    switch (id) {
#define S3_IO_REG_NAME(name, val) \
    case name:                    \
        return #name;
        S3_IO_REG_LIST(S3_IO_REG_NAME)
#undef S3_IO_REG_NAME
    default:
        return "?";
    }
}
#endif
}  // namespace IoReg

namespace MmioReg {
enum Id : LONG
{
#define S3_MMIO_REG_ENUM(name, val) name = val,
    S3_MMIO_REG_LIST(S3_MMIO_REG_ENUM)
#undef S3_MMIO_REG_ENUM
};
#define S3_MMIO_REG_ID(name, val) static const Id id_##name = name;
S3_MMIO_REG_LIST(S3_MMIO_REG_ID)
#undef S3_MMIO_REG_ID
#ifdef DBG
static INLINE const char *regName(Id id)
{
    switch (id) {
#define S3_MMIO_REG_NAME(name, val) \
    case name:                      \
        return #name;
        S3_MMIO_REG_LIST(S3_MMIO_REG_NAME)
#undef S3_MMIO_REG_NAME
    default:
        return "?";
    }
}
#endif
}  // namespace MmioReg

#endif
