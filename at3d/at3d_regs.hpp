#ifndef AT3D_REGS_HPP
#define AT3D_REGS_HPP

#include <exec/types.h>

#ifdef DBG
#include "common.h"
#endif

/*
 * AT3D MMIO byte offsets. Values match chip_at3d.h #defines.
 * After that header, prefer MmioReg::id_* or AT3D_MMIO_ID(SYMBOL).
 * Standard VGA ports: vga_regs.hpp / VgaIo.
 */
#define AT3D_MMIO_REG_LIST(X)        \
    X(CLIP_CTRL, 0x030)              \
    X(CLIP_LEFT, 0x038)              \
    X(CLIP_TOP, 0x03A)               \
    X(CLIP_RIGHT, 0x03C)             \
    X(CLIP_BOTTOM, 0x03E)            \
    X(DRAW_CMD, 0x040)               \
    X(RASTEROP, 0x046)               \
    X(BYTE_MASK, 0x047)              \
    X(PATTERN0, 0x048)               \
    X(PATTERN1, 0x04C)               \
    X(SRC_LOCATION_X_LOW, 0x050)     \
    X(SRC_LOCATION_Y_HIGH, 0x052)    \
    X(DST_LOCATION_X_LOW, 0x054)     \
    X(DST_LOCATION_Y_HIGH, 0x056)    \
    X(SRC_SIZE_X, 0x058)             \
    X(SRC_SIZE_Y, 0x05A)             \
    X(DST_PITCH, 0x05C)              \
    X(SRC_PITCH, 0x05E)              \
    X(FRGD_COLOR, 0x060)             \
    X(BKGD_COLOR, 0x064)             \
    X(DST_TRANSPARENCY_COLOR, 0x06C) \
    X(DST_TRANSPARENCY_MASK, 0x06F)  \
    X(DDA_AXIAL_STEP, 0x070)         \
    X(DDA_DIAGONAL_STEP, 0x072)      \
    X(DDA_ERROR_TERM, 0x074)         \
    X(SERIAL_CTRL, 0x080)            \
    X(SIGANALYSER_CTRL, 0x0B4)       \
    X(APERTURE_CTRL, 0x0C2)          \
    X(DISP_MEM_CFG, 0x0C4)           \
    X(VGA_OVERRIDE, 0x0C8)           \
    X(FEATURE_CTRL, 0x0CC)           \
    X(DPMS_SYNC_CTRL, 0x0D0)         \
    X(MONITOR_INTERLACE_CTRL, 0x0D2) \
    X(PIXEL_FIFO_REQ_POINT, 0x0D4)   \
    X(FIFO_UNDERFLOW, 0x0D8)         \
    X(EXTSIG_TIMING, 0x0D9)          \
    X(ENABLE_EXT_REGS, 0x0DB)        \
    X(BIENDIAN_CTRL, 0x0DC)          \
    X(COLOR_CORRECTION, 0x0E0)       \
    X(DAC_CTRL, 0x0E4)               \
    X(OVERCURRENT_RED, 0x0E5)        \
    X(OVERCURRENT_GREEN, 0x0E6)      \
    X(OVERCURRENT_BLUE, 0x0E7)       \
    X(MCLK_CTRL, 0x0E8)              \
    X(MCLK_DEN, 0x0E9)               \
    X(MCLK_NUM, 0x0EA)               \
    X(VCLK_CTRL, 0x0EC)              \
    X(VCLK_DEN, 0x0ED)               \
    X(VCLK_NUM, 0x0EE)               \
    X(VCLK_DEFAULT0_CTRL, 0x0F0)     \
    X(VCLK_DEFAULT0_DEN, 0x0F1)      \
    X(VCLK_DEFAULT0_NUM, 0x0F2)      \
    X(VCLK_DEFAULT1_CTRL, 0x0F4)     \
    X(VCLK_DEFAULT1_DEN, 0x0F5)      \
    X(VCLK_DEFAULT1_NUM, 0x0F6)      \
    X(VMI_PORT0_CTRL, 0x100)         \
    X(VMI_PORT0_TIMING, 0x101)       \
    X(VMI_PORT0_INDEX_OFFSET, 0x102) \
    X(VMI_PORT1_CTRL, 0x104)         \
    X(VMI_PORT1_TIMING, 0x105)       \
    X(VMI_PORT1_INDEX_OFFSET, 0x106) \
    X(THP_CTRL, 0x110)               \
    X(VMI_PORT_CTRL, 0x120)          \
    X(HW_CURSOR_CTRL, 0x140)         \
    X(HW_CURSOR_COL1, 0x141)         \
    X(HW_CURSOR_COL2, 0x142)         \
    X(HW_CURSOR_COL3, 0x143)         \
    X(HW_CURSOR_BASE, 0x144)         \
    X(HW_CURSOR_X, 0x148)            \
    X(HW_CURSOR_Y, 0x14A)            \
    X(HW_CURSOR_OFF_X, 0x14C)        \
    X(HW_CURSOR_OFF_Y, 0x14D)        \
    X(DEVICE_ID, 0x182)              \
    X(GPIO_CTRL, 0x1F0)              \
    X(VERTICAL_CURRENT_POS, 0x1FA)   \
    X(EXT_DAC_STATUS, 0x1FC)         \
    X(ABORT, 0x1FF)

namespace AT3DMmioReg {
enum Id : WORD
{
#define AT3D_MMIO_REG_ENUM(name, val) name = val,
    AT3D_MMIO_REG_LIST(AT3D_MMIO_REG_ENUM)
#undef AT3D_MMIO_REG_ENUM
};
#define AT3D_MMIO_REG_ID(name, val) static const Id id_##name = name;
AT3D_MMIO_REG_LIST(AT3D_MMIO_REG_ID)
#undef AT3D_MMIO_REG_ID
#ifdef DBG
static INLINE const char *regName(Id id)
{
    switch (id) {
#define AT3D_MMIO_REG_NAME(name, val) \
    case name:                        \
        return #name;
        AT3D_MMIO_REG_LIST(AT3D_MMIO_REG_NAME)
#undef AT3D_MMIO_REG_NAME
    default:
        return "?";
    }
}
#endif
}  // namespace AT3DMmioReg

#endif
