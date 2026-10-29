#ifndef RADEON_REGS_HPP
#define RADEON_REGS_HPP

#include <exec/types.h>
#ifdef DBG
#include "common.h"
#endif

/* Register byte offsets in the Radeon MMIO aperture. */
#define RADEON_MMIO_REG_LIST(X)                  \
    X(RADEON_OVR_CLR, 0x0230)                    \
    X(RADEON_OVR_WID_LEFT_RIGHT, 0x0234)         \
    X(RADEON_OVR_WID_TOP_BOTTOM, 0x0238)         \
    X(RADEON_OV0_SCALE_CNTL, 0x0420)             \
    X(RADEON_MPP_TB_CONFIG, 0x01c0)              \
    X(RADEON_MPP_GP_CONFIG, 0x01c8)              \
    X(RADEON_RBBM_SOFT_RESET, 0x00f0)            \
    X(RADEON_SUBPIC_CNTL, 0x0540)                \
    X(RADEON_VIPH_CONTROL, 0x0c40)               \
    X(RADEON_I2C_CNTL_1, 0x0094)                 \
    X(RADEON_CAP0_TRIG_CNTL, 0x0950)             \
    X(RADEON_CAP1_TRIG_CNTL, 0x09c0)             \
    X(RADEON_CLOCK_INDEX, 0x0008)                \
    X(RADEON_CLOCK_DATA, 0x000c)                 \
    X(RADEON_GEN_INT_CNTL, 0x0040)               \
    X(RADEON_GEN_INT_STATUS, 0x0044)             \
    X(RADEON_CRTC_GEN_CNTL, 0x0050)              \
    X(RADEON_CRTC_EXT_CNTL, 0x0054)              \
    X(RADEON_DAC_CNTL, 0x0058)                   \
    X(RADEON_CRTC_STATUS, 0x005c)                \
    X(RADEON_PALETTE_INDEX, 0x00b0)              \
    X(RADEON_PALETTE_DATA, 0x00b4)               \
    X(RADEON_CONFIG_MEMSIZE, 0x00f8)             \
    X(RADEON_CONFIG_APER_0_BASE, 0x0100)         \
    X(RADEON_MC_STATUS, 0x0150)                  \
    X(RADEON_MEM_SDRAM_MODE_REG, 0x0158)         \
    X(RADEON_TMDS_TRANSMITTER_CNTL, 0x02a4)      \
    X(RADEON_TMDS_PLL_CNTL, 0x02a8)              \
    X(RADEON_MC_FB_LOCATION, 0x0148)             \
    X(RADEON_CRTC_H_TOTAL_DISP, 0x0200)          \
    X(RADEON_CRTC_H_SYNC_STRT_WID, 0x0204)       \
    X(RADEON_CRTC_V_TOTAL_DISP, 0x0208)          \
    X(RADEON_CRTC_V_SYNC_STRT_WID, 0x020c)       \
    X(RADEON_CRTC_VLINE, 0x0210)                 \
    X(RADEON_CRTC_OFFSET, 0x0224)                \
    X(RADEON_CRTC_OFFSET_CNTL, 0x0228)           \
    X(RADEON_CRTC_PITCH, 0x022c)                 \
    X(RADEON_DISPLAY_BASE_ADDR, 0x023c)          \
    X(RADEON_FP_H_TOTAL_DISP, 0x0250)            \
    X(RADEON_FP_V_TOTAL_DISP, 0x0254)            \
    X(RADEON_CUR_OFFSET, 0x0260)                 \
    X(RADEON_CUR_POSITION, 0x0264)               \
    X(RADEON_CUR_HOTSPOT, 0x0268)                \
    X(RADEON_DAC_EXT_CNTL, 0x0280)               \
    X(RADEON_FP_GEN_CNTL, 0x0284)                \
    X(RADEON_FP_HORZ_STRETCH, 0x028c)            \
    X(RADEON_FP_VERT_STRETCH, 0x0290)            \
    X(RADEON_FP_H_SYNC_STRT_WID, 0x02c4)         \
    X(RADEON_FP_V_SYNC_STRT_WID, 0x02c8)         \
    X(RADEON_DISPLAY2_BASE_ADDR, 0x033c)         \
    X(RADEON_OV0_BASE_ADDR, 0x043c)              \
    X(RADEON_SURFACE_CNTL, 0x0b00)               \
    X(RADEON_SURFACE7_LOWER_BOUND, 0x0b74)       \
    X(RADEON_SURFACE7_UPPER_BOUND, 0x0b78)       \
    X(RADEON_SURFACE7_INFO, 0x0b7c)              \
    X(RADEON_CRTC2_GEN_CNTL, 0x03f8)             \
    X(RADEON_FP2_GEN_CNTL, 0x0288)               \
    X(RADEON_TV_DAC_CNTL, 0x088c)                \
    X(RADEON_DISP_OUTPUT_CNTL, 0x0d64)           \
    X(RADEON_DISP_PWR_MAN, 0x0d08)               \
    X(RADEON_RBBM_STATUS, 0x0e40)                \
    X(RADEON_RB2D_DSTCACHE_CTLSTAT, 0x342c)      \
    X(RADEON_DST_OFFSET, 0x1404)                 \
    X(RADEON_DST_PITCH, 0x1408)                  \
    X(RADEON_DST_Y_X, 0x1438)                    \
    X(RADEON_DST_HEIGHT_WIDTH, 0x143c)           \
    X(RADEON_DP_GUI_MASTER_CNTL, 0x146c)         \
    X(RADEON_DP_BRUSH_BKGD_CLR, 0x1478)          \
    X(RADEON_DP_BRUSH_FRGD_CLR, 0x147c)          \
    X(RADEON_SRC_OFFSET, 0x15ac)                 \
    X(RADEON_SRC_PITCH, 0x15b0)                  \
    X(RADEON_SRC_Y_X, 0x1434)                    \
    X(RADEON_SRC_PITCH_OFFSET, 0x1428)           \
    X(RADEON_DST_PITCH_OFFSET, 0x142c)           \
    X(RADEON_DST_X_Y, 0x1594)                    \
    X(RADEON_DST_WIDTH_HEIGHT, 0x1598)           \
    X(RADEON_DP_CNTL_XDIR_YDIR, 0x16d0)          \
    X(RADEON_BRUSH_DATA0, 0x1480)                \
    X(RADEON_DST_LINE_START, 0x1600)             \
    X(RADEON_DST_LINE_END, 0x1604)               \
    X(RADEON_DP_DATATYPE, 0x16c4)                \
    X(RADEON_DP_MIX, 0x16c8)                     \
    X(RADEON_SC_TOP_LEFT, 0x16ec)                \
    X(RADEON_SC_BOTTOM_RIGHT, 0x16f0)            \
    X(RADEON_SRC_SC_BOTTOM_RIGHT, 0x16f4)        \
    X(RADEON_DP_SRC_FRGD_CLR, 0x15d8)            \
    X(RADEON_DP_SRC_BKGD_CLR, 0x15dc)            \
    X(RADEON_HOST_DATA0, 0x17c0)                 \
    X(RADEON_DISP_MISC_CNTL, 0x0d00)             \
    X(RADEON_DP_CNTL, 0x16c0)                    \
    X(RADEON_DP_WRITE_MASK, 0x16cc)              \
    X(RADEON_DEFAULT_OFFSET, 0x16e0)             \
    X(RADEON_DEFAULT_PITCH, 0x16e4)              \
    X(RADEON_DEFAULT_SC_BOTTOM_RIGHT, 0x16e8)    \
    X(RADEON_OV0_Y_X_START, 0x0400)              \
    X(RADEON_OV0_Y_X_END, 0x0404)                \
    X(RADEON_OV0_REG_LOAD_CNTL, 0x0410)          \
    X(RADEON_OV0_V_INC, 0x0424)                  \
    X(RADEON_OV0_P1_V_ACCUM_INIT, 0x0428)        \
    X(RADEON_OV0_P23_V_ACCUM_INIT, 0x042C)       \
    X(RADEON_OV0_P1_BLANK_LINES_AT_TOP, 0x0430)  \
    X(RADEON_OV0_P23_BLANK_LINES_AT_TOP, 0x0434) \
    X(RADEON_OV0_H_INC, 0x0480)                  \
    X(RADEON_OV0_STEP_BY, 0x0484)                \
    X(RADEON_OV0_P1_H_ACCUM_INIT, 0x0488)        \
    X(RADEON_OV0_P23_H_ACCUM_INIT, 0x048C)       \
    X(RADEON_OV0_P1_X_START_END, 0x0494)         \
    X(RADEON_OV0_P2_X_START_END, 0x0498)         \
    X(RADEON_OV0_P3_X_START_END, 0x049C)         \
    X(RADEON_OV0_VIDEO_KEY_CLR_LOW, 0x04E4)      \
    X(RADEON_OV0_VIDEO_KEY_CLR_HIGH, 0x04E8)     \
    X(RADEON_OV0_GRAPHICS_KEY_CLR_LOW, 0x04EC)   \
    X(RADEON_OV0_GRAPHICS_KEY_CLR_HIGH, 0x04F0)  \
    X(RADEON_OV0_KEY_CNTL, 0x04F4)               \
    X(RADEON_OV0_VID_BUF0_BASE_ADRS, 0x0440)     \
    X(RADEON_OV0_VID_BUF1_BASE_ADRS, 0x0444)     \
    X(RADEON_OV0_VID_BUF2_BASE_ADRS, 0x0448)     \
    X(RADEON_OV0_VID_BUF3_BASE_ADRS, 0x044C)     \
    X(RADEON_OV0_VID_BUF4_BASE_ADRS, 0x0450)     \
    X(RADEON_OV0_VID_BUF5_BASE_ADRS, 0x0454)     \
    X(RADEON_OV0_VID_BUF_PITCH0_VALUE, 0x0460)   \
    X(RADEON_OV0_VID_BUF_PITCH1_VALUE, 0x0464)   \
    X(RADEON_OV0_FILTER_CNTL, 0x04A0)            \
    X(RADEON_OV0_FOUR_TAP_COEF_0, 0x04B0)        \
    X(RADEON_OV0_FOUR_TAP_COEF_1, 0x04B4)        \
    X(RADEON_OV0_FOUR_TAP_COEF_2, 0x04B8)        \
    X(RADEON_OV0_FOUR_TAP_COEF_3, 0x04BC)        \
    X(RADEON_OV0_FOUR_TAP_COEF_4, 0x04C0)        \
    X(RADEON_OV0_TEST, 0x04F8)                   \
    X(RADEON_OV0_COLOUR_CNTL, 0x04E0)            \
    X(RADEON_OV0_LIN_TRANS_A, 0x0d20)            \
    X(RADEON_OV0_LIN_TRANS_B, 0x0d24)            \
    X(RADEON_OV0_LIN_TRANS_C, 0x0d28)            \
    X(RADEON_OV0_LIN_TRANS_D, 0x0d2c)            \
    X(RADEON_OV0_LIN_TRANS_E, 0x0d30)            \
    X(RADEON_OV0_LIN_TRANS_F, 0x0d34)            \
    X(RADEON_OV0_GAMMA_000_00F, 0x0d40)          \
    X(RADEON_OV0_GAMMA_010_01F, 0x0d44)          \
    X(RADEON_OV0_GAMMA_020_03F, 0x0d48)          \
    X(RADEON_OV0_GAMMA_040_07F, 0x0d4c)          \
    X(RADEON_OV0_GAMMA_080_0BF, 0x0e00)          \
    X(RADEON_OV0_GAMMA_0C0_0FF, 0x0e04)          \
    X(RADEON_OV0_GAMMA_100_13F, 0x0e08)          \
    X(RADEON_OV0_GAMMA_140_17F, 0x0e0c)          \
    X(RADEON_OV0_GAMMA_180_1BF, 0x0e10)          \
    X(RADEON_OV0_GAMMA_1C0_1FF, 0x0e14)          \
    X(RADEON_OV0_GAMMA_200_23F, 0x0e18)          \
    X(RADEON_OV0_GAMMA_240_27F, 0x0e1c)          \
    X(RADEON_OV0_GAMMA_280_2BF, 0x0e20)          \
    X(RADEON_OV0_GAMMA_2C0_2FF, 0x0e24)          \
    X(RADEON_OV0_GAMMA_300_33F, 0x0e28)          \
    X(RADEON_OV0_GAMMA_340_37F, 0x0e2c)          \
    X(RADEON_OV0_GAMMA_380_3BF, 0x0d50)          \
    X(RADEON_OV0_GAMMA_3C0_3FF, 0x0d54)          \
    X(RADEON_GPIO_VGA_DDC, 0x0060)               \
    X(RADEON_GPIO_DVI_DDC, 0x0064)               \
    X(RADEON_GPIO_MONID, 0x0068)                 \
    X(RADEON_GPIO_CRT2_DDC, 0x006c)              \
    X(RADEON_CP_RB_CNTL, 0x0704)                 \
    X(RADEON_CP_CSQ_CNTL, 0x0740)                \
    X(RADEON_CP_ME_CNTL, 0x07d0)                 \
    X(RADEON_CP_ME_RAM_ADDR, 0x07d4)             \
    X(RADEON_CP_ME_RAM_DATAH, 0x07dc)            \
    X(RADEON_CP_ME_RAM_DATAL, 0x07e0)            \
    X(RADEON_CP_CSQ_STAT, 0x07f8)                \
    X(RADEON_CP_CSQ_APER_PRIMARY, 0x1000)        \
    X(RADEON_SURFACE0_LOWER_BOUND, 0x0b04)       \
    X(RADEON_CP_BUS_CNTL, 0x0030)                \
    X(RADEON_MC_AGP_LOCATION, 0x014c)            \
    X(RADEON_AGP_BASE, 0x0170)                   \
    X(RADEON_AGP_COMMAND, 0x0f60)                \
    X(RADEON_AIC_CNTL, 0x01d0)                   \
    X(RADEON_CACHE_CNTL, 0x1724)                 \
    X(RADEON_WAIT_UNTIL, 0x1720)                 \
    X(RADEON_SE_CNTL_STATUS, 0x2140)             \
    X(R200_SE_VAP_CNTL, 0x2080)                  \
    X(R200_SE_VTE_CNTL, 0x20b0)                  \
    X(R200_SE_TCL_OUTPUT_VTX_FMT_0, 0x2090)      \
    X(R200_SE_TCL_OUTPUT_VTX_FMT_1, 0x2094)      \
    X(R200_RB3D_DEPTHXY_OFFSET, 0x1d60)          \
    X(RADEON_PP_FOG_COLOR, 0x1c18)               \
    X(RADEON_PP_MISC, 0x1c14)                    \
    X(RADEON_RB3D_BLENDCNTL, 0x1c20)             \
    X(RADEON_RB3D_CNTL, 0x1c3c)                  \
    X(RADEON_RB3D_COLOROFFSET, 0x1c40)           \
    X(RADEON_RB3D_COLORPITCH, 0x1c48)            \
    X(RADEON_RB3D_PLANEMASK, 0x1d84)             \
    X(RADEON_RB3D_ROPCNTL, 0x1d80)               \
    X(RADEON_RB3D_STENCILREFMASK, 0x1d7c)        \
    X(RADEON_RB3D_ZSTENCILCNTL, 0x1c2c)          \
    X(RADEON_RE_LINE_PATTERN, 0x1cd0)            \
    X(RADEON_RE_SOLID_COLOR, 0x1c1c)             \
    X(RADEON_RE_TOP_LEFT, 0x26c0)                \
    X(RADEON_RE_WIDTH_HEIGHT, 0x1c44)            \
    X(RADEON_SE_CNTL, 0x1c4c)                    \
    X(RADEON_SE_COORD_FMT, 0x1c50)               \
    X(RADEON_PP_CNTL, 0x1c38)                    \
    X(RADEON_RB3D_DEPTHOFFSET, 0x1c24)           \
    X(RADEON_RE_STIPPLE_ADDR, 0x1cc4)            \
    X(RADEON_RE_LINE_WIDTH, 0x1db8)              \
    X(RADEON_RE_AUX_SCISSOR_CNTL, 0x1d38)        \
    X(R200_PP_TXMULTI_CTL_0, 0x2cd0)             \
    X(R200_PP_TFACTOR_0, 0x2ee0)                 \
    X(RADEON_PP_TXFILTER_0, 0x1c54)              \
    X(RADEON_PP_TXFORMAT_0, 0x1c58)              \
    X(RADEON_PP_TXOFFSET_0, 0x1c5c)              \
    X(RADEON_PP_TXCBLEND_0, 0x1c60)              \
    X(RADEON_PP_TXABLEND_0, 0x1c64)              \
    X(RADEON_PP_TFACTOR_0, 0x1c68)               \
    X(RADEON_PP_TEX_SIZE_0, 0x1d04)              \
    X(RADEON_PP_TEX_PITCH_0, 0x1d08)             \
    X(R200_PP_TXFILTER_0, 0x2c00)                \
    X(R200_PP_TXFORMAT_0, 0x2c04)                \
    X(R200_PP_TXFORMAT_X_0, 0x2c08)              \
    X(R200_PP_TXSIZE_0, 0x2c0c)                  \
    X(R200_PP_TXPITCH_0, 0x2c10)                 \
    X(R200_PP_TXOFFSET_0, 0x2d00)                \
    X(R200_PP_TXCBLEND_0, 0x2f00)                \
    X(R200_PP_TXCBLEND2_0, 0x2f04)               \
    X(R200_PP_TXABLEND_0, 0x2f08)                \
    X(R200_PP_TXABLEND2_0, 0x2f0c)               \
    X(R200_SE_VTX_FMT_0, 0x2088)                 \
    X(R200_SE_VTX_FMT_1, 0x208c)                 \
    X(R200_PP_TEXTURE_CONTROL, 0x2cc0)           \
    X(R200_RB3D_ABLENDCNTL, 0x321c)

namespace RadeonReg {
enum Id : ULONG
{
#define RADEON_MMIO_ENUM(name, value) name = value,
    RADEON_MMIO_REG_LIST(RADEON_MMIO_ENUM)
#undef RADEON_MMIO_ENUM
};
#ifdef DBG
static INLINE const char *regName(Id id)
{
    switch (id) {
#define RADEON_MMIO_NAME(name, value) \
    case name:                        \
        return #name;
        RADEON_MMIO_REG_LIST(RADEON_MMIO_NAME)
#undef RADEON_MMIO_NAME
    default:
        return "?";
    }
}
#endif
}  // namespace RadeonReg

#undef RADEON_MMIO_REG_LIST

#endif
