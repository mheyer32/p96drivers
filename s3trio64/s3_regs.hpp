#ifndef S3_REGS_HPP
#define S3_REGS_HPP

/*
 * S3 GE absolute port / MMIO indices (namespaces IoReg / MmioReg + unscoped enum Id).
 * Standard VGA ports live in vga_regs.hpp. BEE8 indexed regs stay as numeric indices
 * (MIN_AXIS_PCNT, PIX_CNTL, …) in the chip source.
 */

#include <exec/types.h>

#ifdef DBG
#include "common.h"
#endif

/* Absolute I/O port indices: IoReg / MmioReg. Chip builds -include s3config.h;
 * the card TU gets defaults via s3_reg_apertures.hpp → s3config.h. */
#ifndef MMIO_ONLY
#error s3_regs.hpp requires MMIO_ONLY (include s3config.h first)
#endif

/* Shared GE addresses (same index in I/O and MMIO apertures). */
#define S3_GE_REG_LIST(X)      \
    X(CUR_Y, 0x82E8)           \
    X(CUR_Y2, 0x82EA)          \
    X(CUR_X, 0x86E8)           \
    X(CUR_X2, 0x86EA)          \
    X(DESTY_AXSTP, 0x8AE8)     \
    X(Y2_AXSTP2, 0x8AEA)       \
    X(DESTX_DIASTP, 0x8EE8)    \
    X(X2, 0x8EEA)              \
    X(ERR_TERM, 0x92E8)        \
    X(ERR_TERM2, 0x92EA)       \
    X(MAJ_AXIS_PCNT, 0x96E8)   \
    X(MAJ_AXIS_PCNT2, 0x96EA)  \
    X(GP_STAT, 0x9AE8)         \
    X(CMD2, 0x9AEA)            \
    X(SHORT_STROKE, 0x9EE8)    \
    X(BKGD_COLOR, 0xA2E8)      \
    X(FRGD_COLOR, 0xA6E8)      \
    X(WRT_MASK, 0xAAE8)        \
    X(RD_MASK, 0xAEE8)         \
    X(COLOR_CMP, 0xB2E8)       \
    X(BKGD_MIX, 0xB6E8)        \
    X(FRGD_MIX, 0xBAE8)        \
    X(MULTI_FUNC_CNTL, 0xBEE8) \
    X(PIX_TRANS, 0xE2E8)       \
    X(PIX_TRANS_EXT, 0xE2EA)

/* Packed 32-bit MMIO aliases (HAS_PACKED_MMIO); always enumerated for naming. */
#define S3_MMIO_PACKED_REG_LIST(X) \
    X(ALT_CURXY, 0x8100)           \
    X(ALT_CURXY2, 0x8104)          \
    X(ALT_STEP, 0x8108)            \
    X(ALT_STEP2, 0x810C)           \
    X(ALT_ERR, 0x8110)             \
    X(ALT_CMD, 0x8118)             \
    X(ALT_MIX, 0x8134)             \
    X(ALT_PCNT, 0x8148)            \
    X(ALT_PAT, 0x8168)

#define S3_MMIO_EXTRA_REG_LIST(X) \
    X(SERIAL_PORT, 0xFF20)        \
    X(I2C_PAD, 0xFF08)

#if MMIO_ONLY
/* New-style MMIO: these live at fixed offsets in the MMIO window (accessed via io aperture). */
#define S3_IO_EXTRA_REG_LIST(X) \
    X(SUBSYS_CNTL, 0x0504)      \
    X(ADVFUNC_CNTL, 0x050C)
#else
#define S3_IO_EXTRA_REG_LIST(X) \
    X(SUBSYS_CNTL, 0x42E8)      \
    X(ADVFUNC_CNTL, 0x4AE8)
#endif

namespace IoReg {
enum Id : LONG
{
#define S3_IO_REG_ENUM(name, val) name = val,
    S3_GE_REG_LIST(S3_IO_REG_ENUM)
    S3_IO_EXTRA_REG_LIST(S3_IO_REG_ENUM)
#undef S3_IO_REG_ENUM
    /* Same-index aliases. */
    CMD         = GP_STAT,
    RD_REG_DT   = MULTI_FUNC_CNTL,
    SUBSYS_STAT = SUBSYS_CNTL,
};

#ifdef DBG
static INLINE const char *regName(Id id)
{
    switch (id) {
#define S3_IO_REG_NAME(name, val) \
    case name:                    \
        return #name;
        S3_GE_REG_LIST(S3_IO_REG_NAME)
        S3_IO_EXTRA_REG_LIST(S3_IO_REG_NAME)
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
    S3_GE_REG_LIST(S3_MMIO_REG_ENUM)
    S3_MMIO_PACKED_REG_LIST(S3_MMIO_REG_ENUM)
    S3_MMIO_EXTRA_REG_LIST(S3_MMIO_REG_ENUM)
#undef S3_MMIO_REG_ENUM
    CMD       = GP_STAT,
    RD_REG_DT = MULTI_FUNC_CNTL,
};

#ifdef DBG
static INLINE const char *regName(Id id)
{
    switch (id) {
#define S3_MMIO_REG_NAME(name, val) \
    case name:                      \
        return #name;
        S3_GE_REG_LIST(S3_MMIO_REG_NAME)
        S3_MMIO_PACKED_REG_LIST(S3_MMIO_REG_NAME)
        S3_MMIO_EXTRA_REG_LIST(S3_MMIO_REG_NAME)
#undef S3_MMIO_REG_NAME
    default:
        return "?";
    }
}
#endif
}  // namespace MmioReg

#endif
