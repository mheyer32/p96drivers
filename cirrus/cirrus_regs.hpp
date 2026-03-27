#ifndef CIRRUS_REGS_HPP
#define CIRRUS_REGS_HPP

#include <exec/types.h>

#ifdef DBG
#include "common.h"
#endif

/*
 * Cirrus-specific I/O / MMIO offsets beyond standard VGA (vga_regs.hpp).
 * Blitter/MMIO GE registers can be added here when acceleration is wired.
 */
#define CIRRUS_IO_REG_LIST(X) \
    X(ADAPTER_SLEEP, 0x46E8)  \
    X(ADAPTER_SLEEP2, 0x4AE8) \
    X(POS102, 0x102)

namespace IoReg {
enum Id : LONG
{
#define CIRRUS_IO_REG_ENUM(name, val) name = val,
    CIRRUS_IO_REG_LIST(CIRRUS_IO_REG_ENUM)
#undef CIRRUS_IO_REG_ENUM
};
#define CIRRUS_IO_REG_ID(name, val) static const Id id_##name = name;
CIRRUS_IO_REG_LIST(CIRRUS_IO_REG_ID)
#undef CIRRUS_IO_REG_ID

#ifdef DBG
static INLINE const char *regName(Id id)
{
    switch (id) {
#define CIRRUS_IO_REG_NAME(name, val) \
    case name:                        \
        return #name;
        CIRRUS_IO_REG_LIST(CIRRUS_IO_REG_NAME)
#undef CIRRUS_IO_REG_NAME
    default:
        return "?";
    }
}
#endif
} /* namespace IoReg */

#endif /* CIRRUS_REGS_HPP */
