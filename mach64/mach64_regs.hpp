#ifndef MACH64_REGS_HPP
#define MACH64_REGS_HPP

/*
 * Mach64 dword-index register IDs (namespace + unscoped enum Id).
 * Names match ATI programming manuals (FIFO_STAT, CRTC_GEN_CNTL, …).
 *
 * In .cpp: using namespace MmioReg; then write FIFO_STAT instead of
 * MmioReg::FIFO_STAT (keep BlkIoReg:: / SparseIoReg:: when both apertures used).
 *
 * Under DBG, AtiRegAperture logs via ADL regName(id) (X-macro switch).
 */

#include <exec/types.h>

#define MACH64_MMIO_REG_LIST(X)    \
    X(CRTC_H_TOTAL_DISP, 0x00)     \
    X(CRTC_H_SYNC_STRT_WID, 0x01)  \
    X(CRTC_V_TOTAL_DISP, 0x02)     \
    X(CRTC_V_SYNC_STRT_WID, 0x03)  \
    X(CRTC_VLINE_CRNT_VLINE, 0x04) \
    X(CRTC_OFF_PITCH, 0x05)        \
    X(CRTC_INT_CNTL, 0x06)         \
    X(CRTC_GEN_CNTL, 0x07)         \
    X(MEM_ADDR_CONFIG, 0x0D)       \
    X(OVR_CLR, 0x10)               \
    X(OVR_WID_LEFT_RIGHT, 0x11)    \
    X(OVR_WID_TOP_BOTTOM, 0x12)    \
    X(CUR_CLR0, 0x18)              \
    X(CUR_CLR1, 0x19)              \
    X(CUR_OFFSET, 0x1A)            \
    X(CUR_HORZ_VERT_POSN, 0x1B)    \
    X(CUR_HORZ_VERT_OFF, 0x1C)     \
    X(HW_DEBUG, 0x1F)              \
    X(SCRATCH_REG0, 0x20)          \
    X(SCRATCH_REG1, 0x21)          \
    X(CLOCK_CNTL, 0x24)            \
    X(BUS_CNTL, 0x28)              \
    X(MEM_CNTL, 0x2C)              \
    X(MEM_VGA_WP_SEL, 0x2D)        \
    X(MEM_VGA_RP_SEL, 0x2E)        \
    X(DAC_REGS, 0x30)              \
    X(DAC_CNTL, 0x31)              \
    X(GEN_TEST_CNTL, 0x34)         \
    /* CONFIG_CNTL is I/O-only (RRG §1-3) — BlkIoReg / SparseIoReg, never MMIO */ \
    X(CONFIG_CHIP_ID, 0x38)        \
    X(CONFIG_STAT0, 0x39)          \
    X(CONFIG_STAT1, 0x3A)          \
    X(DST_OFF_PITCH, 0x40)         \
    X(DST_Y_X, 0x43)               \
    X(DST_HEIGHT_WIDTH, 0x46)      \
    X(DST_X_WIDTH, 0x47)           \
    X(DST_BRES_LNTH, 0x48)         \
    X(DST_BRES_ERR, 0x49)          \
    X(DST_BRES_INC, 0x4A)          \
    X(DST_BRES_DEC, 0x4B)          \
    X(DST_CNTL, 0x4C)              \
    X(SRC_OFF_PITCH, 0x60)         \
    X(SRC_Y_X, 0x63)               \
    X(SRC_HEIGHT1_WIDTH1, 0x66)    \
    X(SRC_Y_X_START, 0x69)         \
    X(SRC_HEIGHT2_WIDTH2, 0x6C)    \
    X(SRC_CNTL, 0x6D)              \
    X(HOST_DATA0, 0x80)            \
    X(HOST_DATA1, 0x81)            \
    X(HOST_DATA2, 0x82)            \
    X(HOST_DATA3, 0x83)            \
    X(HOST_DATA4, 0x84)            \
    X(HOST_DATA5, 0x85)            \
    X(HOST_DATA6, 0x86)            \
    X(HOST_DATA7, 0x87)            \
    X(HOST_DATA8, 0x88)            \
    X(HOST_DATA9, 0x89)            \
    X(HOST_DATA10, 0x8A)           \
    X(HOST_DATA11, 0x8B)           \
    X(HOST_DATA12, 0x8C)           \
    X(HOST_DATA13, 0x8D)           \
    X(HOST_DATA14, 0x8E)           \
    X(HOST_DATA15, 0x8F)           \
    X(HOST_CNTL, 0x90)             \
    X(PAT_REG0, 0xA0)              \
    X(PAT_REG1, 0xA1)              \
    X(PAT_CNTL, 0xA2)              \
    X(SC_LEFT_RIGHT, 0xAA)         \
    X(SC_TOP_BOTTOM, 0xAD)         \
    X(DP_BKGD_CLR, 0xB0)           \
    X(DP_FRGD_CLR, 0xB1)           \
    X(DP_WRITE_MSK, 0xB2)          \
    X(DP_CHAIN_MSK, 0xB3)          \
    X(DP_PIX_WIDTH, 0xB4)          \
    X(DP_MIX, 0xB5)                \
    X(DP_SRC, 0xB6)                \
    X(CLR_CMP_CLR, 0xC0)           \
    X(CLR_CMP_MSK, 0xC1)           \
    X(CLR_CMP_CNTL, 0xC2)          \
    X(FIFO_STAT, 0xC4)             \
    X(CONTEXT_MASK, 0xC8)          \
    X(CONTEXT_LOAD_CNTL, 0xCB)     \
    X(GUI_TRAJ_CNTL, 0xCC)         \
    X(GUI_STAT, 0xCE)

#define MACH64_BLKIO_REG_LIST(X) \
    X(CRTC_H_TOTAL_DISP, 0x00)   \
    X(CRTC_GEN_CNTL, 0x07)       \
    X(DSP_CONFIG, 0x08)          \
    X(DSP_ON_OFF, 0x09)          \
    X(MEM_ADDR_CONFIG, 0x0D)     \
    X(OVR_CLR, 0x10)             \
    X(HW_DEBUG, 0x1F)            \
    X(SCRATCH_REG0, 0x20)        \
    X(SCRATCH_REG1, 0x21)        \
    X(CLOCK_CNTL, 0x24)          \
    X(BUS_CNTL, 0x28)            \
    X(LT_GIO, 0x2A)              \
    X(EXT_MEM_CNTL, 0x2B)        \
    X(MEM_CNTL, 0x2C)            \
    X(DAC_REGS, 0x30)            \
    X(DAC_CNTL, 0x31)            \
    X(GEN_TEST_CNTL, 0x34)       \
    X(CONFIG_CNTL, 0x37)         \
    X(CONFIG_CHIP_ID, 0x38)      \
    X(CONFIG_STAT0, 0x39)

#define MACH64_SPARSEIO_REG_LIST(X) X(CONFIG_CNTL, 0x1A)

#ifdef DBG
#define MACH64_DEFINE_REG_NAME(LIST)   \
    inline const char *regName(Id r)   \
    {                                  \
        switch (r) {                   \
            LIST(MACH64_REG_NAME_CASE) \
        default:                       \
            return "?";                \
        }                              \
    }
#define MACH64_REG_NAME_CASE(n, v) \
    case n:                        \
        return #n;
#endif

namespace MmioReg {
enum Id : LONG
{
#define MACH64_REG_ENUM(n, v) n = v,
    MACH64_MMIO_REG_LIST(MACH64_REG_ENUM)
#undef MACH64_REG_ENUM
};
#ifdef DBG
MACH64_DEFINE_REG_NAME(MACH64_MMIO_REG_LIST)
#endif
}  // namespace MmioReg

namespace BlkIoReg {
enum Id : LONG
{
#define MACH64_REG_ENUM(n, v) n = v,
    MACH64_BLKIO_REG_LIST(MACH64_REG_ENUM)
#undef MACH64_REG_ENUM
};
#ifdef DBG
MACH64_DEFINE_REG_NAME(MACH64_BLKIO_REG_LIST)
#endif
}  // namespace BlkIoReg

namespace SparseIoReg {
enum Id : LONG
{
#define MACH64_REG_ENUM(n, v) n = v,
    MACH64_SPARSEIO_REG_LIST(MACH64_REG_ENUM)
#undef MACH64_REG_ENUM
};
#ifdef DBG
MACH64_DEFINE_REG_NAME(MACH64_SPARSEIO_REG_LIST)
#endif
}  // namespace SparseIoReg

#ifdef DBG
#undef MACH64_DEFINE_REG_NAME
#undef MACH64_REG_NAME_CASE
#endif

#endif /* MACH64_REGS_HPP */
