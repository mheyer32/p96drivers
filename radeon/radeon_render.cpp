#include "radeon_driver.hpp"

#include "radeon_render.h"

using namespace RadeonReg;

/* The rasterizer and texture units use distinct format encodings and endian
 * controls. An ARGB texture is not necessarily a legal render destination.
 */
static BOOL texture_format(RGBFTYPE format, ULONG *hardware, BOOL *swap)
{
    *swap = FALSE;
    switch (format) {
    case RGBFB_CLUT:
        *hardware = 0;
        *swap     = TRUE;
        return TRUE;
    case RGBFB_R5G6B5PC:
        *swap = TRUE;
        /* fall through */
    case RGBFB_R5G6B5:
        *hardware = RADEON_TEXTURE_RGB565;
        return TRUE;
    case RGBFB_R5G5B5PC:
        *swap = TRUE;
        /* fall through */
    case RGBFB_R5G5B5:
        *hardware = RADEON_TEXTURE_ARGB1555;
        return TRUE;
    case RGBFB_B8G8R8A8:
        *swap = TRUE;
        /* fall through */
    case RGBFB_A8R8G8B8:
        *hardware = RADEON_TEXTURE_ARGB8888;
        return TRUE;
    case RGBFB_A8B8G8R8:
        *swap = TRUE;
        /* fall through */
    case RGBFB_R8G8B8A8:
        *hardware = RADEON_TEXTURE_RGBA8888;
        return TRUE;
    default:
        return FALSE;
    }
}

static BOOL target_format(RGBFTYPE format, ULONG *hardware, BOOL *swap)
{
    if (!texture_format(format, hardware, swap) || format == RGBFB_CLUT || format == RGBFB_R8G8B8A8)
        return FALSE;
    if (*hardware == RADEON_TEXTURE_RGBA8888)
        *hardware = RADEON_TEXTURE_ARGB8888;
    *hardware <<= RADEON_COLOR_FORMAT_SHIFT;
    return TRUE;
}

void radeon_render_init(struct RadeonRenderState *render)
{
    render->r100            = FALSE;
    render->misc            = RADEON_RENDER_MISC_DEFAULT;
    render->fog             = RADEON_RENDER_FOG_MASK;
    render->solid           = ~0UL;
    render->blend           = RADEON_BLEND_SOURCE_ALPHA | RADEON_BLEND_DEST_INV_SOURCE_ALPHA;
    render->zStencil        = RADEON_RENDER_ZSTENCIL_DEFAULT;
    render->control         = RADEON_COLOR_ARGB8888;
    render->colorOffset     = 0;
    render->widthHeight     = 0;
    render->colorPitch      = 0;
    render->setup           = RADEON_RENDER_SETUP_DEFAULT;
    render->coordinates     = R200_RENDER_COORDINATES_DEFAULT;
    render->planeMask       = ~0UL;
    render->vertexFormat[0] = 0;
    render->vertexFormat[1] = 0;
    render->topLeft         = 0;
    render->pixelControl    = R200_RENDER_PIXEL_CONTROL;
    render->vertexControl   = R200_RENDER_VERTEX_CONTROL;
    render->alphaBlend      = 0;
    render->stencil         = RADEON_RENDER_STENCIL_MASK;
    render->rop             = 0;
    render->linePattern     = RADEON_RENDER_LINE_PATTERN;
    render->stages          = 1;
    for (ULONG i = 0; i < RADEON_RENDER_TEXTURES; ++i) {
        struct RadeonTextureState *texture = &render->texture[i];
        texture->filter = texture->format = texture->formatX = 0;
        texture->size = texture->pitch = texture->offset = 0;
    }
    for (ULONG i = 0; i < RADEON_RENDER_STAGES; ++i) {
        struct RadeonCombinerState *stage = &render->combiner[i];
        stage->color = stage->alpha = 0;
        stage->colorControl = stage->alphaControl = R200_RENDER_COMBINER_CLAMP;
    }
}

BOOL radeon_render_target(struct BoardInfo *bi, struct RadeonRenderState *render, const struct BitMapExtra *bitmap)
{
    ULONG format;
    BOOL swap;
    if (!bitmap || !target_format(bitmap->RenderInfo.RGBFormat, &format, &swap) || !bitmap->Width || !bitmap->Height ||
        (bitmap->Width & (RADEON_RENDER_TARGET_ALIGNMENT - 1)))
        return FALSE;
    ULONG bytes = radeon_bytes_per_pixel(bitmap->RenderInfo.RGBFormat);
    ULONG pitch = (UWORD)bitmap->RenderInfo.BytesPerRow / bytes;
    if (pitch > RADEON_RENDER_MAX_PITCH)
        return FALSE;
    render->colorOffset = (ULONG)bitmap->RenderInfo.Memory - (ULONG)bi->MemorySpaceBase;
    render->colorPitch  = pitch | (swap ? RADEON_RENDER_COLOR_SWAP : 0);
    render->widthHeight = ((ULONG)(bitmap->Height - 1) << RADEON_TEXTURE_DIMENSION_SHIFT) | (bitmap->Width - 1);
    render->control     = (render->control & ~RADEON_RENDER_COLOR_FORMAT_MASK) | format;
    return TRUE;
}

BOOL radeon_render_texture(struct BoardInfo *bi, struct RadeonRenderState *render, const struct BitMapExtra *bitmap,
                           ULONG unit, BOOL filter)
{
    ULONG format;
    BOOL swap;
    if (!bitmap || unit >= RADEON_RENDER_TEXTURES || !bitmap->Width || !bitmap->Height ||
        bitmap->Width > RADEON_RENDER_MAX_TEXTURE || bitmap->Height > RADEON_RENDER_MAX_TEXTURE ||
        !texture_format(bitmap->RenderInfo.RGBFormat, &format, &swap))
        return FALSE;
    ULONG pitch = (UWORD)bitmap->RenderInfo.BytesPerRow;
    if (pitch < RADEON_TEXTURE_PITCH_BIAS ||
        pitch / radeon_bytes_per_pixel(bitmap->RenderInfo.RGBFormat) > RADEON_TEXTURE_MAX_PITCH)
        return FALSE;
    struct RadeonTextureState *texture = &render->texture[unit];
    texture->filter                    = filter ? RADEON_TEXTURE_FILTER_LINEAR : RADEON_TEXTURE_FILTER_NEAREST;
    texture->format                    = format | RADEON_TEXTURE_NON_POWER_OF_TWO | (unit << RADEON_TEXTURE_UNIT_SHIFT);
    texture->formatX                   = RADEON_TEXTURE_FORMAT_EXTENDED;
    texture->size  = ((ULONG)(bitmap->Height - 1) << RADEON_TEXTURE_DIMENSION_SHIFT) | (bitmap->Width - 1);
    texture->pitch = pitch - RADEON_TEXTURE_PITCH_BIAS;
    texture->offset =
        ((ULONG)bitmap->RenderInfo.Memory - (ULONG)bi->MemorySpaceBase) | (swap ? RADEON_TEXTURE_BYTE_SWAP : 0);
    return TRUE;
}

/* Submit complete state at an operation boundary. This also restores state
 * after the 2D engine has been used, without depending on another client's
 * cached copy of the GPU registers.
 */
BOOL radeon_render_flush(struct BoardInfo *bi, const struct RadeonRenderState *r)
{
    const struct
    {
        ULONG reg, value;
    } registers[] = {{RADEON_PP_MISC, r->misc},
                     {RADEON_PP_FOG_COLOR, r->fog},
                     {RADEON_RE_SOLID_COLOR, r->solid},
                     {RADEON_RB3D_BLENDCNTL, r->blend},
                     {RADEON_RB3D_ZSTENCILCNTL, r->zStencil},
                     {RADEON_RB3D_CNTL, r->control},
                     {RADEON_RB3D_COLOROFFSET, r->colorOffset},
                     {RADEON_RE_WIDTH_HEIGHT, r->widthHeight},
                     {RADEON_RB3D_COLORPITCH, r->colorPitch},
                     {RADEON_SE_CNTL, r->setup},
                     {RADEON_SE_COORD_FMT, r->coordinates},
                     {RADEON_RB3D_PLANEMASK, r->planeMask},
                     {RADEON_RE_TOP_LEFT, r->topLeft},
                     {RADEON_RB3D_STENCILREFMASK, r->stencil},
                     {RADEON_RB3D_ROPCNTL, r->rop}};
    for (ULONG i = 0; i < ARRAY_SIZE(registers); ++i)
        if (!radeon_cp_register(bi, registers[i].reg, registers[i].value))
            return FALSE;
    if (r->r100) {
        if (!radeon_cp_register(bi, RADEON_PP_CNTL, r->pixelControl))
            return FALSE;
        for (ULONG i = 0; i < RADEON_RENDER_TEXTURES; ++i) {
            const struct RadeonTextureState *texture = &r->texture[i];
            const struct RadeonCombinerState *stage  = &r->combiner[i];
            ULONG base                               = i * R100_TEXTURE_REGISTER_STRIDE;
            if (!radeon_cp_register(bi, RADEON_PP_TXFILTER_0 + base, texture->filter) ||
                !radeon_cp_register(bi, RADEON_PP_TXFORMAT_0 + base, texture->format) ||
                !radeon_cp_register(bi, RADEON_PP_TXOFFSET_0 + base, texture->offset) ||
                !radeon_cp_register(bi, RADEON_PP_TXCBLEND_0 + base, stage->color) ||
                !radeon_cp_register(bi, RADEON_PP_TXABLEND_0 + base, stage->alpha) ||
                !radeon_cp_register(bi, RADEON_PP_TFACTOR_0 + base, 0) ||
                !radeon_cp_register(bi, RADEON_PP_TEX_SIZE_0 + i * R100_TEXTURE_SIZE_STRIDE, texture->size) ||
                !radeon_cp_register(bi, RADEON_PP_TEX_PITCH_0 + i * R100_TEXTURE_SIZE_STRIDE, texture->pitch))
                return FALSE;
        }
        return TRUE;
    }
    if (!radeon_cp_register(bi, R200_SE_VTX_FMT_0, r->vertexFormat[0]) ||
        !radeon_cp_register(bi, R200_SE_VTX_FMT_1, r->vertexFormat[1]) ||
        !radeon_cp_register(bi, R200_PP_TEXTURE_CONTROL, r->pixelControl) ||
        !radeon_cp_register(bi, R200_SE_VTE_CNTL, r->vertexControl) ||
        !radeon_cp_register(bi, R200_RB3D_ABLENDCNTL, r->alphaBlend) ||
        !radeon_cp_register(bi, RADEON_RE_LINE_PATTERN, r->linePattern))
        return FALSE;
    for (ULONG i = 0; i < RADEON_RENDER_TEXTURES; ++i) {
        const struct RadeonTextureState *texture = &r->texture[i];
        ULONG base                               = i * R200_TEXTURE_REGISTER_STRIDE;
        if (!radeon_cp_register(bi, R200_PP_TXFILTER_0 + base, texture->filter) ||
            !radeon_cp_register(bi, R200_PP_TXFORMAT_0 + base, texture->format) ||
            !radeon_cp_register(bi, R200_PP_TXFORMAT_X_0 + base, texture->formatX) ||
            !radeon_cp_register(bi, R200_PP_TXSIZE_0 + base, texture->size) ||
            !radeon_cp_register(bi, R200_PP_TXPITCH_0 + base, texture->pitch) ||
            !radeon_cp_register(bi, R200_PP_TXOFFSET_0 + i * R200_TEXTURE_OFFSET_STRIDE, texture->offset))
            return FALSE;
    }
    for (ULONG i = 0; i < r->stages; ++i) {
        const struct RadeonCombinerState *stage = &r->combiner[i];
        ULONG base                              = i * R200_COMBINER_REGISTER_STRIDE;
        if (!radeon_cp_register(bi, R200_PP_TXCBLEND_0 + base, stage->color) ||
            !radeon_cp_register(bi, R200_PP_TXCBLEND2_0 + base, stage->colorControl) ||
            !radeon_cp_register(bi, R200_PP_TXABLEND_0 + base, stage->alpha) ||
            !radeon_cp_register(bi, R200_PP_TXABLEND2_0 + base, stage->alphaControl))
            return FALSE;
    }
    return TRUE;
}
