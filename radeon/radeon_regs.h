#ifndef RADEON_REGS_H
#define RADEON_REGS_H

#include <exec/types.h>

namespace RadeonReg {

/* Register bitfields and hardware limits. MMIO offsets are in radeon_regs.hpp. */
constexpr auto RADEON_DAC_RANGE_MASK           = 7UL;
constexpr auto RADEON_DAC_8BIT_ENABLE          = (1UL << 8);
constexpr auto RADEON_TMDS_PLL_ENABLE          = (1UL << 0);
constexpr auto RADEON_TMDS_PLL_RESET           = (1UL << 1);
constexpr auto RADEON_MC_IDLE                  = (1UL << 2);
constexpr auto RADEON_PLL_PWRMGT_CNTL          = 0x14;
constexpr auto RADEON_MC_BUSY                  = (1UL << 16);
constexpr auto RADEON_DLL_READY                = (1UL << 19);
constexpr auto RADEON_MC_RESET                 = (1UL << 24);
constexpr auto RADEON_LINE_LEFT_TO_RIGHT       = (1UL << 31);
constexpr auto RADEON_LINE_TOP_TO_BOTTOM       = (1UL << 15);
constexpr auto RADEON_DATATYPE_BRUSH_LINE_BG   = (6UL << 8);
constexpr auto RADEON_DATATYPE_BRUSH_LINE_FG   = (7UL << 8);
constexpr auto RADEON_SOFT_RESET_GRPH_PP       = (1UL << 0);
constexpr auto RADEON_MCLK_CNTL                = 0x12;
constexpr auto RADEON_FORCE_ENGINE_CLOCKS      = (0x3fUL << 16);
constexpr auto RADEON_DATATYPE_BRUSH_NONE      = (15UL << 8);
constexpr auto RADEON_DATATYPE_BRUSH_SOLID     = (13UL << 8);
constexpr auto RADEON_DATATYPE_SRC_COLOR       = (3UL << 16);
constexpr auto RADEON_DATATYPE_SRC_MONO_FG     = (1UL << 16);
constexpr auto RADEON_DATATYPE_BYTE_LSB_TO_MSB = (1UL << 30);
constexpr auto RADEON_MIX_SOURCE_MEMORY        = (2UL << 8);
constexpr auto RADEON_MIX_SOURCE_HOST          = (4UL << 8);
constexpr auto RADEON_ROP_SRCXOR               = 0x66UL;
constexpr auto RADEON_PITCH_BYTE_SHIFT         = 6;
constexpr auto RADEON_PITCH_FIELD_SHIFT        = 22;
constexpr auto RADEON_OFFSET_BYTE_SHIFT        = 10;
constexpr auto RADEON_OFFSET_BYTE_MASK         = ((1UL << RADEON_OFFSET_BYTE_SHIFT) - 1);

constexpr auto RADEON_VBLANK_INTERRUPT        = (1UL << 0);
constexpr auto RADEON_VSYNC_INTERRUPT         = (1UL << 2);
constexpr auto RADEON_CRTC_SCAN_MASK          = 0x00000003UL;
constexpr auto RADEON_CRTC_PIXEL_FORMAT_MASK  = 0x00000f00UL;
constexpr auto RADEON_CRTC_PIXEL_FORMAT_SHIFT = 8;
constexpr auto RADEON_CRTC_CURSOR_ENABLE      = (1UL << 16);
constexpr auto RADEON_CRTC_CURSOR_ARGB        = (3UL << 20);
constexpr auto RADEON_CRTC_DISPLAY_ENABLE     = (3UL << 24);
constexpr auto RADEON_CRTC_BLANK_MASK         = (7UL << 8);
constexpr auto RADEON_CRTC_CRT_ENABLE         = ((1UL << 15) | (1UL << 6) | (1UL << 3));
constexpr auto RADEON_DAC_MASK_ALL            = 0xff000000UL;
constexpr auto RADEON_DAC_FORCE_BLANK         = (1UL << 5);
constexpr auto RADEON_FP_DIRECT_COLOR         = (1UL << 3);
constexpr auto RADEON_FP_PANEL_ENABLE         = ((1UL << 0) | (1UL << 2));
constexpr auto RADEON_FP_POWER_MASK           = (3UL << 8);
constexpr auto RADEON_FP_POWER_SHIFT          = 8;
constexpr auto RADEON_SYNC_POLARITY           = (1UL << 23);
constexpr auto RADEON_H_TOTAL_MASK            = 0x3ffUL;
constexpr auto RADEON_H_DISPLAY_MASK          = 0x1ffUL;
constexpr auto RADEON_V_SYNC_START_MASK       = 0xfffUL;
constexpr auto RADEON_FIELD_HIGH_SHIFT        = 16;
constexpr auto RADEON_H_SYNC_WIDTH_MAX        = 63u;
constexpr auto RADEON_H_CHAR_PIXELS           = 8;
constexpr auto RADEON_FP_V_SIZE_SHIFT         = 12;
constexpr auto RADEON_CURSOR_LOCK             = (1UL << 31);
constexpr auto RADEON_CURSOR_WIDTH            = 64;
constexpr auto RADEON_CURSOR_HEIGHT           = 64;
constexpr auto RADEON_CURSOR_PIXEL_BYTES      = 4;
constexpr auto RADEON_CURSOR_BYTES           = (RADEON_CURSOR_WIDTH * RADEON_CURSOR_HEIGHT * RADEON_CURSOR_PIXEL_BYTES);
constexpr auto RADEON_SURFACE_SWAP_16        = 0x00500000UL;
constexpr auto RADEON_SURFACE_SWAP_32        = 0x00a00000UL;
constexpr auto RADEON_PLL_WRITE_ENABLE       = (1UL << 7);
constexpr auto RADEON_PLL_INDEX_MASK         = 0xffUL;
constexpr auto RADEON_PLL_DIV_SEL            = (3UL << 8);
constexpr auto RADEON_PPLL_CNTL              = 2;
constexpr auto RADEON_PPLL_REF_DIV           = 3;
constexpr auto RADEON_PPLL_DIV_3             = 7;
constexpr auto RADEON_VCLK_ECP_CNTL          = 8;
constexpr auto RADEON_HTOTAL_CNTL            = 9;
constexpr auto RADEON_PPLL_SLEEP             = (1UL << 1);
constexpr auto RADEON_PPLL_SETTLE_US         = 5000;
constexpr auto RADEON_PPLL_RESET             = (1UL << 0);
constexpr auto RADEON_PPLL_ATOMIC_ENABLE     = (3UL << 16);
constexpr auto RADEON_PPLL_ATOMIC_UPDATE     = (1UL << 15);
constexpr auto RADEON_PPLL_REF_MASK          = 0x3ffUL;
constexpr auto RADEON_PPLL_FEEDBACK_MASK     = 0x7ffUL;
constexpr auto RADEON_PPLL_POST_MASK         = (7UL << 16);
constexpr auto RADEON_PLL_SOURCE_MASK        = 3UL;
constexpr auto RADEON_PLL_SOURCE_PPLL        = 3UL;
constexpr auto RADEON_POLL_LIMIT             = 1000000UL;
constexpr auto RADEON_COLOR_RED_SHIFT        = 16;
constexpr auto RADEON_COLOR_GREEN_SHIFT      = 8;
constexpr auto RADEON_COLOR_ALPHA_OPAQUE     = 0xff000000UL;
constexpr auto RADEON_PALETTE_ENTRIES        = 256;
constexpr auto RADEON_FIFO_COUNT_MASK        = 0x7fUL;
constexpr auto RADEON_ENGINE_ACTIVE          = (1UL << 31);
constexpr auto RADEON_DSTCACHE_FLUSH_ALL     = 0x0fUL;
constexpr auto RADEON_DSTCACHE_BUSY          = (1UL << 31);
constexpr auto RADEON_DP_LEFT_TO_RIGHT       = (1UL << 0);
constexpr auto RADEON_DP_TOP_TO_BOTTOM       = (1UL << 1);
constexpr auto RADEON_GMC_EXPLICIT_PITCH     = (3UL << 0);
constexpr auto RADEON_GMC_WRITE_MASK_DISABLE = (1UL << 30);
constexpr auto RADEON_HOST_WORD_BITS         = 32u;
constexpr auto RADEON_SCISSOR_LIMIT          = 0x1fffu;
constexpr auto RADEON_GMC_BRUSH_SOLID        = (13UL << 4);
constexpr auto RADEON_GMC_BRUSH_NONE         = (15UL << 4);
constexpr auto RADEON_GMC_DST_FORMAT_SHIFT   = 8;
constexpr auto RADEON_GMC_SRC_COLOR          = (3UL << 12);
constexpr auto RADEON_GMC_ROP_SHIFT          = 16;
constexpr auto RADEON_GMC_SRC_MEMORY         = (2UL << 24);
constexpr auto RADEON_GMC_CLR_CMP_DISABLE    = (1UL << 28);
constexpr auto RADEON_GMC_AUX_CLIP_DISABLE   = (1UL << 29);
constexpr auto RADEON_ROP_PATCOPY            = 0xf0UL;
constexpr auto RADEON_ROP_SRCCOPY            = 0xccUL;
constexpr auto RADEON_ROP_DSTINVERT          = 0x55UL;
constexpr auto RADEON_SCISSOR_MAX            = 0x1fff1fffUL;

/* Overlay scaler register bank. */
constexpr auto RADEON_OVERLAY_LOAD_LOCK       = (1UL << 0);
constexpr auto RADEON_OVERLAY_LOAD_READY      = (1UL << 3);
constexpr auto RADEON_OVERLAY_ENABLE          = (1UL << 30);
constexpr auto RADEON_OVERLAY_BURST_PER_PLANE = (0x7fUL << 16);
constexpr auto RADEON_OVERLAY_RGB_CONVERSION  = (1UL << 28);
constexpr auto RADEON_OVERLAY_FORMAT_SHIFT    = 8u;
constexpr auto RADEON_GMC_SOURCE_CLIP         = (1UL << 2);
constexpr auto RADEON_GMC_DESTINATION_CLIP    = (1UL << 3);

constexpr auto RADEON_OVERLAY_GAMMA_UNITY = (1UL << 24);
constexpr auto RADEON_GPIO_DATA_OUTPUT    = (1UL << 0);
constexpr auto RADEON_GPIO_CLOCK_OUTPUT   = (1UL << 1);
constexpr auto RADEON_GPIO_DATA_INPUT     = (1UL << 8);
constexpr auto RADEON_GPIO_CLOCK_INPUT    = (1UL << 9);
constexpr auto RADEON_GPIO_DATA_DRIVE     = (1UL << 16);
constexpr auto RADEON_GPIO_CLOCK_DRIVE    = (1UL << 17);
/* Command processor and host command aperture. */
constexpr auto RADEON_CP_ME_RUN                = (1UL << 30);
constexpr auto RADEON_CP_PRIMARY_PIO           = (1UL << 28);
constexpr auto RADEON_CP_NO_RPTR_WRITEBACK     = (1UL << 27);
constexpr auto RADEON_CP_R200_RESET_MASK       = 0x1fffUL;
constexpr auto RADEON_SURFACE_REGISTER_STRIDE  = 0x10u;
constexpr auto RADEON_SURFACE_DESCRIPTOR_WORDS = 3u;
constexpr auto RADEON_CP_SURFACE_COUNT         = 7u;

constexpr auto RADEON_CP_BUS_ACCESS_MASK   = 0x310UL;
constexpr auto RADEON_CP_BUS_ACCESS_ENABLE = 0x100UL;
constexpr auto RADEON_WAIT_2D_IDLECLEAN    = (1UL << 16);
constexpr auto RADEON_WAIT_3D_IDLECLEAN    = (1UL << 17);
constexpr auto RADEON_WAIT_HOST_IDLECLEAN  = (1UL << 18);
constexpr auto RADEON_CACHE_R100_MODE      = 0x33UL;
constexpr auto RADEON_CACHE_R200_MODE      = 0x3fUL;
constexpr auto RADEON_TCL_BYPASS           = (1UL << 8);
constexpr ULONG r200MaxVertices(ULONG count)
{
    return count << 18;
}
constexpr auto R200_VTX_XY_FMT = (1UL << 8);
constexpr auto R200_VTX_Z_FMT  = (1UL << 9);

/* R100/R200 rasterizer and texture state. */
constexpr auto RADEON_TEXTURE_RGB565              = 4u;
constexpr auto RADEON_TEXTURE_ARGB1555            = 3u;
constexpr auto RADEON_TEXTURE_ARGB8888            = 6u;
constexpr auto RADEON_TEXTURE_RGBA8888            = 7u;
constexpr auto RADEON_COLOR_FORMAT_SHIFT          = 10u;
constexpr auto RADEON_COLOR_ARGB8888              = (RADEON_TEXTURE_ARGB8888 << RADEON_COLOR_FORMAT_SHIFT);
constexpr auto RADEON_BLEND_SOURCE_ALPHA          = (38UL << 16);
constexpr auto RADEON_BLEND_DEST_INV_SOURCE_ALPHA = (39UL << 24);
constexpr auto R200_TEXTURE_REGISTER_STRIDE       = 0x20u;
constexpr auto R200_TEXTURE_OFFSET_STRIDE         = 0x18u;
constexpr auto R200_COMBINER_REGISTER_STRIDE      = 0x10u;

}  // namespace RadeonReg

#endif
