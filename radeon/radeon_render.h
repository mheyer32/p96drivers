#ifndef RADEON_RENDER_H
#define RADEON_RENDER_H

constexpr auto RADEON_RENDER_TEXTURES           = 2u;
constexpr auto RADEON_RENDER_STAGES             = 8u;
constexpr auto RADEON_RENDER_MAX_TEXTURE        = 2048u;
constexpr auto RADEON_RENDER_MAX_PITCH          = 4095u;
constexpr auto RADEON_TEXTURE_MAX_PITCH         = 1920u;
constexpr auto RADEON_RENDER_TARGET_ALIGNMENT   = 8u;
constexpr auto RADEON_TEXTURE_PITCH_BIAS        = 32u;
constexpr auto RADEON_TEXTURE_UNIT_SHIFT        = 24u;
constexpr auto RADEON_TEXTURE_DIMENSION_SHIFT   = 16u;
constexpr auto RADEON_TEXTURE_FILTER_NEAREST    = 0x11000000UL;
constexpr auto RADEON_TEXTURE_FILTER_LINEAR     = 0x11000003UL;
constexpr auto RADEON_TEXTURE_NON_POWER_OF_TWO  = 0xc0UL;
constexpr auto RADEON_TEXTURE_FORMAT_EXTENDED   = 0x00030000UL;
constexpr auto RADEON_TEXTURE_BYTE_SWAP         = 1u;
constexpr auto RADEON_RENDER_COLOR_SWAP         = (1UL << 19);
constexpr auto RADEON_RENDER_COLOR_FORMAT_MASK  = 0x3c00UL;
constexpr auto RADEON_RENDER_BLEND_ENABLE       = (1UL << 0);
constexpr auto RADEON_RENDER_PLANE_MASK_ENABLE  = (1UL << 1);
constexpr auto RADEON_RENDER_MISC_DEFAULT       = 0x00010700UL;
constexpr auto RADEON_RENDER_ZSTENCIL_DEFAULT   = 0x20007072UL;
constexpr auto RADEON_RENDER_SETUP_DEFAULT      = 0x1800801eUL;
constexpr auto R200_RENDER_COORDINATES_DEFAULT  = 0x0555000aUL;
constexpr auto R100_RENDER_SCISSOR_ENABLE       = (1UL << 1);
constexpr auto R100_RENDER_COORDINATES_DEFAULT  = 0x04280303UL;
constexpr auto R100_RENDER_COMBINER_CLAMP       = (1UL << 23);
constexpr auto R100_RENDER_COLOR_CONSTANT       = 0x00001000UL;
constexpr auto R100_RENDER_COLOR_TEXTURE        = 0x0000016aUL;
constexpr auto R100_RENDER_ALPHA_TEXTURE        = 0x00000500UL;
constexpr auto R100_RENDER_COLOR_GLOBAL_ALPHA   = 0x000000aaUL;
constexpr auto R100_RENDER_ALPHA_CONSTANT       = 0x00000200UL;
constexpr auto R100_RENDER_COLOR_TEXTURE_MASK   = 0x000001aaUL;
constexpr auto R100_RENDER_ALPHA_TEXTURE_MASK   = 0x00000600UL;
constexpr auto R100_RENDER_ALPHA_MASK_MODULATE  = 0x00000052UL;
constexpr auto R100_RENDER_COLOR_MODULATE       = 0x000000a2UL;
constexpr auto R100_RENDER_ALPHA_MODULATE       = 0x00000021UL;
constexpr auto R100_TEXTURE_REGISTER_STRIDE     = 24u;
constexpr auto R100_TEXTURE_SIZE_STRIDE         = 8u;
constexpr auto RADEON_RENDER_FOG_MASK           = 0x00ffffffUL;
constexpr auto RADEON_RENDER_STENCIL_MASK       = 0xffff0000UL;
constexpr auto RADEON_RENDER_LINE_PATTERN       = 0x0000ffffUL;
constexpr auto R200_RENDER_PIXEL_CONTROL        = 0x00001000UL;
constexpr auto R200_RENDER_VERTEX_CONTROL       = 0x00001100UL;
constexpr auto R200_RENDER_TEXTURE_ENABLE_SHIFT = 4u;
constexpr auto R200_RENDER_STAGE_ENABLE_SHIFT   = 12u;
constexpr auto R200_RENDER_COMBINER_CLAMP       = 0x00011000UL;
constexpr auto R200_RENDER_COLOR_TEXTURE        = 0x0000016aUL;
constexpr auto R200_RENDER_ALPHA_TEXTURE        = 0x00002800UL;
constexpr auto R200_RENDER_COLOR_GLOBAL_ALPHA   = 0x000000aaUL;
constexpr auto R200_RENDER_ALPHA_CONSTANT       = 0x00001000UL;
constexpr auto R200_RENDER_COLOR_TEXTURE_MASK   = 0x000001aaUL;
constexpr auto R200_RENDER_ALPHA_TEXTURE_MASK   = 0x00003000UL;
constexpr auto R200_RENDER_ALPHA_MASK_MODULATE  = 0x00000144UL;
constexpr auto R200_RENDER_ALPHA_MODULATE       = 0x0000008aUL;

struct RadeonTextureState
{
    ULONG filter, format, formatX, size, pitch, offset;
};
struct RadeonCombinerState
{
    ULONG color, colorControl, alpha, alphaControl;
};
struct RadeonRenderState
{
    ULONG misc, fog, solid, blend, zStencil;
    ULONG control, colorOffset, widthHeight, colorPitch, setup, coordinates;
    ULONG planeMask, vertexFormat[2], topLeft, pixelControl, vertexControl;
    ULONG alphaBlend, stencil, rop, linePattern;
    struct RadeonTextureState texture[RADEON_RENDER_TEXTURES];
    struct RadeonCombinerState combiner[RADEON_RENDER_STAGES];
    ULONG stages;
    BOOL r100;
};

struct BoardInfo;
struct BitMapExtra;
void radeon_render_init(struct RadeonRenderState *render);
BOOL radeon_render_target(struct BoardInfo *bi, struct RadeonRenderState *render, const struct BitMapExtra *bitmap);
BOOL radeon_render_texture(struct BoardInfo *bi, struct RadeonRenderState *render, const struct BitMapExtra *bitmap,
                           ULONG unit, BOOL filter);
BOOL radeon_render_flush(struct BoardInfo *bi, const struct RadeonRenderState *render);
#endif
