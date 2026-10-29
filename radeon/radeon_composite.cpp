#include "radeon_driver.hpp"

#include "radeon_composite.h"
#include "radeon_render.h"
#define __NOLIBBASE__
#include <proto/mathieeesingbas.h>

using namespace RadeonReg;

struct CompositeRect
{
    LONG x, y;
    ULONG width, height;
};

/* Order CPU access and 2D commands around the 3D rasterizer. Waiting for an
 * empty submission queue alone does not drain the rendering caches.
 */
static BOOL composite_sync(struct BoardInfo *bi)
{
    return radeon_cp_register(bi, RADEON_RB2D_DSTCACHE_CTLSTAT, RADEON_DSTCACHE_FLUSH_ALL) &&
           radeon_cp_register(bi, RADEON_WAIT_UNTIL,
                              RADEON_WAIT_2D_IDLECLEAN | RADEON_WAIT_3D_IDLECLEAN | RADEON_WAIT_HOST_IDLECLEAN) &&
           radeon_wait_idle(bi);
}

static const struct TagItem *composite_tag(const struct TagItem *tags, ULONG tag)
{
    while (tags) {
        switch (tags->ti_Tag) {
        case TAG_DONE:
            return NULL;
        case TAG_MORE:
            tags = (const struct TagItem *)tags->ti_Data;
            continue;
        case TAG_SKIP:
            tags += tags->ti_Data + 1;
            continue;
        case TAG_IGNORE:
            break;
        default:
            if (tags->ti_Tag == tag)
                return tags;
        }
        ++tags;
    }
    return NULL;
}

static ULONG composite_value(const struct TagItem *tags, ULONG tag, ULONG fallback)
{
    const struct TagItem *item = composite_tag(tags, tag);
    return item ? item->ti_Data : fallback;
}

static ULONG float_word(float value)
{
    union {
        float value;
        ULONG word;
    } bits;
    bits.value = value;
    return bits.word;
}

/* Amiga's conversion entry takes a signed integer. Split the unsigned range
 * before converting scale tags, which use unsigned 16.16 fixed point.
 */
static float float_unsigned(struct Library *MathIeeeSingBasBase, ULONG value)
{
    if (!(value & RC_INTEGER_SIGN))
        return IEEESPFlt(value);
    return IEEESPAdd(IEEESPMul(IEEESPFlt(value >> 1), 2.0f), IEEESPFlt(value & 1));
}

static ULONG alpha_color(ULONG alpha)
{
    alpha >>= RC_ALPHA_SHIFT;
    if (alpha > RC_ALPHA_BYTE_MAX)
        alpha = RC_ALPHA_BYTE_MAX;
    return alpha << RC_ALPHA_CHANNEL_SHIFT;
}

static BOOL has_alpha(const struct BitMapExtra *bitmap)
{
    return bitmap->RenderInfo.RGBFormat >= RGBFB_A8R8G8B8 && bitmap->RenderInfo.RGBFormat <= RGBFB_B8G8R8A8;
}

static void render_mask(struct RadeonRenderState *r, ULONG mask)
{
    r->planeMask = mask;
    if (mask == ~0UL)
        r->control &= ~RADEON_RENDER_PLANE_MASK_ENABLE;
    else
        r->control |= RADEON_RENDER_PLANE_MASK_ENABLE;
}

static void render_stages(struct RadeonRenderState *r, ULONG stages, ULONG textures)
{
    r->stages       = stages;
    r->pixelControl = (r->r100 ? R100_RENDER_SCISSOR_ENABLE : 0) |
                      (((1UL << stages) - 1) << R200_RENDER_STAGE_ENABLE_SHIFT) |
                      (((1UL << textures) - 1) << R200_RENDER_TEXTURE_ENABLE_SHIFT);
}

static void render_clip(struct RadeonRenderState *r, const struct CompositeRect *clip)
{
    r->topLeft     = ((ULONG)clip->y << RADEON_TEXTURE_DIMENSION_SHIFT) | (ULONG)clip->x;
    r->widthHeight = ((clip->y + clip->height - 1) << RADEON_TEXTURE_DIMENSION_SHIFT) | (clip->x + clip->width - 1);
}

static BOOL render_quad(struct Library *MathIeeeSingBasBase, struct BoardInfo *bi, struct RadeonRenderState *r,
                        const struct CompositeRect *rect, const float *uv, ULONG textures, BOOL sourcePass)
{
    ULONG packet[R100_VERTEX_PACKET_HEADER_WORDS +
                 RC_RECTANGLE_TRIANGLE_VERTICES *
                     (RC_VERTEX_COORDINATES + RADEON_RENDER_TEXTURES * RC_VERTEX_TEXTURE_COORDINATES)];
    ULONG stride      = RC_VERTEX_COORDINATES + textures * RC_VERTEX_TEXTURE_COORDINATES;
    ULONG vertices    = r->r100 && sourcePass ? RC_RECTANGLE_TRIANGLE_VERTICES : RC_QUAD_VERTICES;
    ULONG headerWords = r->r100 ? R100_VERTEX_PACKET_HEADER_WORDS : RC_VERTEX_PACKET_HEADER_WORDS;
    ULONG words       = vertices * stride;
    packet[0] =
        radeon_packet3(r->r100 ? R100_PACKET_DRAW_IMMEDIATE : RC_PACKET_DRAW_IMMEDIATE, words + headerWords - 1);
    if (r->r100)
        packet[1] =
            (textures ? R100_VERTEX_TEXTURE0 : 0) | (textures == RADEON_RENDER_TEXTURES ? R100_VERTEX_TEXTURE1 : 0);
    packet[headerWords - 1] = (vertices << RC_VERTEX_COUNT_SHIFT) |
                              (vertices == RC_QUAD_VERTICES ? RC_PRIMITIVE_TRIANGLE_FAN : RC_PRIMITIVE_TRIANGLE_LIST) |
                              (r->r100 ? R100_VERTEX_WALK_DATA : RC_VERTEX_WALK_DATA);
    float left = IEEESPFlt(rect->x), top = IEEESPFlt(rect->y);
    float right  = IEEESPAdd(left, IEEESPFlt(rect->width));
    float bottom = IEEESPAdd(top, IEEESPFlt(rect->height));
    for (ULONG vertex = 0; vertex < vertices; ++vertex) {
        static const UBYTE triangleCorners[RC_RECTANGLE_TRIANGLE_VERTICES] = {0, 1, 2, 1, 2, 3};
        BOOL east = vertex >= 2, south = vertex == 1 || vertex == 2;
        if (vertices != RC_QUAD_VERTICES) {
            ULONG corner = triangleCorners[vertex];
            east         = !!(corner & 1);
            south        = !!(corner & 2);
        }
        ULONG *out = packet + headerWords + vertex * stride;
        *out++     = float_word(east ? right : left);
        *out++     = float_word(south ? bottom : top);
        for (ULONG unit = 0; unit < textures; ++unit) {
            const float *coordinates = uv + unit * RC_QUAD_VERTICES;
            /* R100 places texture zero's reciprocal W before S and T. */
            if (r->r100 && unit == 0)
                *out++ = float_word(1.0f);
            *out++ = float_word(coordinates[east ? 2 : 0]);
            *out++ = float_word(coordinates[south ? 3 : 1]);
            if (!r->r100 || unit != 0)
                *out++ = float_word(1.0f);
        }
    }
    /* Each active texture contributes S, T and Q to the vertex stream. */
    r->vertexFormat[1] = textures == 2   ? RC_TEXTURE_VERTEX_FORMAT_TWO
                         : textures == 1 ? RC_TEXTURE_VERTEX_FORMAT_ONE
                                         : 0;
    return radeon_render_flush(bi, r) && radeon_cp_submit(bi, packet, words + headerWords);
}

struct CompositeTriangles
{
    const ULONG *vertices;
    const UWORD *indices;
    ULONG count, format;
};

/* Immediate packets contain whole triangles. The input format describes
 * position followed by optional S/T/Q triples; a mask can reuse the first
 * triple when no independent mask coordinates were supplied.
 */
static BOOL render_triangles(struct BoardInfo *bi, struct RadeonRenderState *r,
                             const struct CompositeTriangles *triangles, ULONG textures, BOOL destinationPass)
{
    ULONG packet[RC_VERTEX_PACKET_WORDS];
    ULONG inputStride, outputStride;
    BOOL sourceCoordinates = !!(triangles->format & RC_VERTEX_SOURCE_TEXTURE);
    BOOL maskCoordinates   = !!(triangles->format & RC_VERTEX_MASK_TEXTURE);
    if (destinationPass) {
        inputStride = outputStride = RC_VERTEX_COORDINATES + (textures ? RC_VERTEX_TEXTURE_COORDINATES : 0);
        if (r->r100)
            inputStride = RC_VERTEX_COORDINATES + (sourceCoordinates + maskCoordinates) * RC_VERTEX_TEXTURE_COORDINATES;
        r->vertexFormat[1] = textures ? RC_TEXTURE_VERTEX_FORMAT_ONE : 0;
    } else {
        inputStride  = RC_VERTEX_COORDINATES + (sourceCoordinates + maskCoordinates) * RC_VERTEX_TEXTURE_COORDINATES;
        outputStride = RC_VERTEX_COORDINATES + (sourceCoordinates + (textures == 2)) * RC_VERTEX_TEXTURE_COORDINATES;
        r->vertexFormat[1] = (sourceCoordinates ? RC_TEXTURE_VERTEX_FORMAT_ONE : 0) |
                             (textures == 2 ? RC_TEXTURE_VERTEX_FORMAT_ONE << RC_TEXTURE_VERTEX_UNIT_SHIFT : 0);
    }
    if (!radeon_render_flush(bi, r))
        return FALSE;
    ULONG headerWords = r->r100 ? R100_VERTEX_PACKET_HEADER_WORDS : RC_VERTEX_PACKET_HEADER_WORDS;
    ULONG batchLimit =
        ((RC_VERTEX_PACKET_WORDS - headerWords) / (outputStride * RC_TRIANGLE_VERTICES)) * RC_TRIANGLE_VERTICES;
    ULONG remaining = triangles->count * RC_TRIANGLE_VERTICES, first = 0;
    while (remaining) {
        ULONG count = remaining < batchLimit ? remaining : batchLimit;
        ULONG words = count * outputStride;
        packet[0] =
            radeon_packet3(r->r100 ? R100_PACKET_DRAW_IMMEDIATE : RC_PACKET_DRAW_IMMEDIATE, words + headerWords - 1);
        if (r->r100)
            packet[1] = (r->vertexFormat[1] & RC_TEXTURE_VERTEX_FORMAT_ONE ? R100_VERTEX_TEXTURE0 : 0) |
                        (textures == RADEON_RENDER_TEXTURES ? R100_VERTEX_TEXTURE1 : 0);
        packet[headerWords - 1] = (count << RC_VERTEX_COUNT_SHIFT) | RC_PRIMITIVE_TRIANGLE_LIST |
                                  (r->r100 ? R100_VERTEX_WALK_DATA : RC_VERTEX_WALK_DATA);
        for (ULONG i = 0; i < count; ++i) {
            ULONG index     = triangles->indices ? triangles->indices[first + i] : first + i;
            const ULONG *in = triangles->vertices + index * inputStride;
            ULONG *out      = packet + headerWords + i * outputStride;
            out[0]          = in[0];
            out[1]          = in[1];
            if (destinationPass && textures) {
                out[2] = in[0];
                out[3] = in[1];
                out[4] = float_word(1.0f);
            } else if (!destinationPass) {
                if (sourceCoordinates)
                    for (ULONG j = 0; j < RC_VERTEX_TEXTURE_COORDINATES; ++j)
                        out[RC_VERTEX_COORDINATES + j] = in[RC_VERTEX_COORDINATES + j];
                if (textures == 2) {
                    ULONG output = RC_VERTEX_COORDINATES + sourceCoordinates * RC_VERTEX_TEXTURE_COORDINATES;
                    ULONG input =
                        RC_VERTEX_COORDINATES + (maskCoordinates && sourceCoordinates) * RC_VERTEX_TEXTURE_COORDINATES;
                    for (ULONG j = 0; j < RC_VERTEX_TEXTURE_COORDINATES; ++j)
                        out[output + j] = in[input + j];
                }
            }
            if (r->r100 && (r->vertexFormat[1] & RC_TEXTURE_VERTEX_FORMAT_ONE)) {
                ULONG q = out[4];
                out[4]  = out[3];
                out[3]  = out[2];
                out[2]  = q;
            }
        }
        if (!radeon_cp_submit(bi, packet, words + headerWords))
            return FALSE;
        first += count;
        remaining -= count;
    }
    return TRUE;
}

static BOOL render_destination(struct Library *MathIeeeSingBasBase, struct BoardInfo *bi, struct RadeonRenderState *r,
                               const struct CompositeRect *rect, const float *uv, ULONG textures,
                               const struct CompositeTriangles *triangles)
{
    if (triangles)
        return render_triangles(bi, r, triangles, textures, TRUE);
    return render_quad(MathIeeeSingBasBase, bi, r, rect, uv, textures, FALSE);
}

static ULONG composite_blend(ULONG operation, BOOL ignoreAlpha)
{
    static const UBYTE source[]      = {RC_BLEND_ZERO,      RC_BLEND_ONE,           RC_BLEND_ZERO,
                                        RC_BLEND_ONE,       RC_BLEND_INV_DST_ALPHA, RC_BLEND_DST_ALPHA,
                                        RC_BLEND_ZERO,      RC_BLEND_INV_DST_ALPHA, RC_BLEND_ZERO,
                                        RC_BLEND_DST_ALPHA, RC_BLEND_INV_DST_ALPHA, RC_BLEND_INV_DST_ALPHA,
                                        RC_BLEND_ONE,       RC_BLEND_SRC_ALPHA,     RC_BLEND_SRC_ALPHA};
    static const UBYTE destination[] = {
        RC_BLEND_ZERO,      RC_BLEND_ZERO,          RC_BLEND_ONE,  RC_BLEND_INV_SRC_ALPHA, RC_BLEND_ONE,
        RC_BLEND_ZERO,      RC_BLEND_SRC_ALPHA,     RC_BLEND_ZERO, RC_BLEND_INV_SRC_ALPHA, RC_BLEND_INV_SRC_ALPHA,
        RC_BLEND_SRC_ALPHA, RC_BLEND_INV_SRC_ALPHA, RC_BLEND_ONE,  RC_BLEND_INV_SRC_ALPHA, RC_BLEND_INV_SRC_ALPHA};
    ULONG factor = source[operation];
    if (ignoreAlpha && factor == RC_BLEND_DST_ALPHA)
        factor = RC_BLEND_ONE;
    if (ignoreAlpha && factor == RC_BLEND_INV_DST_ALPHA)
        factor = RC_BLEND_ZERO;
    return radeon_blend_factors(factor, destination[operation]);
}

static BOOL intersect(struct CompositeRect *result, const struct CompositeRect *a, const struct CompositeRect *b)
{
    /* Destination origins may lie outside the bitmap. Keep the comparisons
     * signed so alpha preparation still covers the visible part of a draw.
     */
    LONG rightA = a->x + a->width, rightB = b->x + b->width;
    LONG bottomA = a->y + a->height, bottomB = b->y + b->height;
    LONG right  = rightA < rightB ? rightA : rightB;
    LONG bottom = bottomA < bottomB ? bottomA : bottomB;
    result->x   = a->x > b->x ? a->x : b->x;
    result->y   = a->y > b->y ? a->y : b->y;
    if (right <= result->x || bottom <= result->y)
        return FALSE;
    result->width  = right - result->x;
    result->height = bottom - result->y;
    return TRUE;
}

/* R100 shares its RGB and alpha blend factors. Replace destination alpha
 * in a second, masked pass after premultiplying RGB; the R200 path can do
 * both operations at once with independent alpha blending.
 */
static BOOL prepare_destination_r100(struct Library *MathIeeeSingBasBase, struct BoardInfo *bi,
                                     struct RadeonRenderState *r, const struct CompositeRect *rect, ULONG flags,
                                     const struct TagItem *tags, const struct BitMapExtra *alphaBitmap,
                                     const struct CompositeTriangles *triangles)
{
    const struct TagItem *alpha = composite_tag(tags, RC_DST_ALPHA);
    ULONG value                 = alpha ? alpha->ti_Data : RC_FIXED_ONE;
    if ((flags & RC_IGNORE_DST_ALPHA) || ((flags & RC_DST_ALPHA_OVERRIDE) && value == RC_FIXED_ONE))
        return TRUE;
    r->solid = alpha_color(value);
    r->control |= RADEON_RENDER_BLEND_ENABLE;
    render_stages(r, 1, 0);
    r->combiner[0].color = R100_RENDER_COMBINER_CLAMP | R100_RENDER_COLOR_CONSTANT;
    r->combiner[0].alpha = R100_RENDER_COMBINER_CLAMP | R100_RENDER_ALPHA_CONSTANT;
    if ((flags & RC_DST_ALPHA_OVERRIDE) || composite_value(tags, RC_DST_ALPHA_MASK, 0)) {
        ULONG textures = 0;
        float uv[RC_QUAD_VERTICES];
        struct CompositeRect maskRect = {rect->x, rect->y, rect->width, rect->height};
        if (!(flags & RC_DST_ALPHA_OVERRIDE)) {
            if (!radeon_render_texture(bi, r, alphaBitmap, 0, FALSE))
                return FALSE;
            textures = 1;
            render_stages(r, 1, textures);
            r->combiner[0].alpha = R100_RENDER_COMBINER_CLAMP | R100_RENDER_ALPHA_MASK_MODULATE;
            uv[0]                = IEEESPFlt(rect->x);
            uv[1]                = IEEESPFlt(rect->y);
            uv[2]                = IEEESPFlt(rect->x + maskRect.width - 1);
            uv[3]                = IEEESPFlt(rect->y + maskRect.height - 1);
        }
        r->blend = radeon_blend_factors(RC_BLEND_ZERO, RC_BLEND_SRC_ALPHA);
        render_mask(r, textures ? ~0UL : RC_COLOR_MASK);
        if (!render_destination(MathIeeeSingBasBase, bi, r, &maskRect, uv, textures, triangles))
            return FALSE;
        r->control &= ~RADEON_RENDER_BLEND_ENABLE;
        render_mask(r, RC_ALPHA_MASK);
        return render_destination(MathIeeeSingBasBase, bi, r, &maskRect, uv, textures, triangles);
    }
    if (alpha) {
        r->blend = radeon_blend_factors(RC_BLEND_ZERO, RC_BLEND_SRC_ALPHA);
        render_mask(r, RC_ALPHA_MASK);
        if (!render_destination(MathIeeeSingBasBase, bi, r, rect, NULL, 0, triangles))
            return FALSE;
    }
    r->blend = radeon_blend_factors(RC_BLEND_ZERO, RC_BLEND_DST_ALPHA);
    render_mask(r, RC_COLOR_MASK);
    return render_destination(MathIeeeSingBasBase, bi, r, rect, NULL, 0, triangles);
}

/* Convert the destination into the premultiplied representation required by
 * the blend unit. A supplied constant can replace its alpha, or multiply the
 * existing alpha before RGB is premultiplied in a separate pass.
 */
static BOOL prepare_destination(struct Library *MathIeeeSingBasBase, struct BoardInfo *bi, struct RadeonRenderState *r,
                                const struct CompositeRect *rect, ULONG flags, const struct TagItem *tags,
                                const struct BitMapExtra *alphaBitmap, const struct CompositeTriangles *triangles)
{
    if (r->r100)
        return prepare_destination_r100(MathIeeeSingBasBase, bi, r, rect, flags, tags, alphaBitmap, triangles);
    const struct TagItem *alpha = composite_tag(tags, RC_DST_ALPHA);
    ULONG value                 = alpha ? alpha->ti_Data : RC_FIXED_ONE;
    if (flags & RC_IGNORE_DST_ALPHA || ((flags & RC_DST_ALPHA_OVERRIDE) && value == RC_FIXED_ONE))
        return TRUE;
    r->solid = alpha_color(value);
    r->control |= RADEON_RENDER_BLEND_ENABLE;
    render_stages(r, 1, 0);
    r->combiner[0].color = R200_RENDER_ALPHA_CONSTANT;
    r->combiner[0].alpha = R200_RENDER_ALPHA_CONSTANT;
    if (flags & RC_DST_ALPHA_OVERRIDE) {
        r->control |= RC_SEPARATE_ALPHA_ENABLE;
        r->alphaBlend = radeon_blend_factors(RC_BLEND_ONE, RC_BLEND_ZERO);
        r->blend      = radeon_blend_factors(RC_BLEND_ZERO, RC_BLEND_SRC_ALPHA);
        render_mask(r, ~0UL);
        if (!render_destination(MathIeeeSingBasBase, bi, r, rect, NULL, 0, triangles))
            return FALSE;
        r->control &= ~RC_SEPARATE_ALPHA_ENABLE;
        return TRUE;
    }
    if (composite_value(tags, RC_DST_ALPHA_MASK, 0)) {
        if (!radeon_render_texture(bi, r, alphaBitmap, 0, FALSE))
            return FALSE;
        r->control |= RC_SEPARATE_ALPHA_ENABLE;
        r->alphaBlend        = radeon_blend_factors(RC_BLEND_ONE, RC_BLEND_ZERO);
        r->blend             = radeon_blend_factors(RC_BLEND_ZERO, RC_BLEND_SRC_ALPHA);
        r->combiner[0].alpha = R200_RENDER_ALPHA_MASK_MODULATE;
        render_stages(r, 1, 1);
        render_mask(r, ~0UL);
        /* This pass samples the mask in destination coordinates, with an
         * inclusive final pixel instead of the source pass's edge bias.
         */
        struct CompositeRect maskRect = {rect->x, rect->y, rect->width - 1, rect->height - 1};
        float uv[]                    = {IEEESPFlt(rect->x), IEEESPFlt(rect->y), IEEESPFlt(rect->x + maskRect.width),
                                         IEEESPFlt(rect->y + maskRect.height)};
        BOOL ok                       = render_destination(MathIeeeSingBasBase, bi, r, &maskRect, uv, 1, triangles);
        r->control &= ~RC_SEPARATE_ALPHA_ENABLE;
        return ok;
    }
    if (alpha) {
        r->blend = radeon_blend_factors(RC_BLEND_ZERO, RC_BLEND_SRC_ALPHA);
        render_mask(r, RC_ALPHA_MASK);
        if (!render_destination(MathIeeeSingBasBase, bi, r, rect, NULL, 0, triangles))
            return FALSE;
    }
    r->blend = radeon_blend_factors(RC_BLEND_ZERO, RC_BLEND_DST_ALPHA);
    render_mask(r, RC_COLOR_MASK);
    return render_destination(MathIeeeSingBasBase, bi, r, rect, NULL, 0, triangles);
}

static ULONG composite(struct Library *MathIeeeSingBasBase, BOOL r100, struct BoardInfo *bi, ULONG operation,
                       struct RenderInfo *source, struct RenderInfo *destination, struct BitMapExtra *sourceExtra,
                       struct BitMapExtra *destinationExtra, struct BitMapExtra *sourceAlphaExtra,
                       struct BitMapExtra *destinationAlphaExtra, const struct TagItem *tags)
{
    (void)source;
    (void)destination;
    if (!sourceExtra || !destinationExtra || !getCardData(bi)->state->cp.active)
        return RC_UNSUPPORTED;
    if (operation > RC_MAX_OPERATOR)
        return RC_BAD_OPERATOR;
    const struct TagItem *vertexTag     = composite_tag(tags, RC_VERTEX_ARRAY);
    struct CompositeTriangles triangles = {
        vertexTag ? (const ULONG *)vertexTag->ti_Data : NULL, (const UWORD *)composite_value(tags, RC_INDEX_ARRAY, 0),
        composite_value(tags, RC_TRIANGLES, 0),
        composite_value(tags, RC_VERTEX_FORMAT, source ? RC_VERTEX_SOURCE_TEXTURE : 0)};
    if (vertexTag && !composite_tag(tags, RC_TRIANGLES))
        return RC_BAD_TRIANGLES;
    /* An empty vertex array has no primitives. Do not interpret it as a
     * rectangle, which would draw over the destination unexpectedly.
     */
    if (vertexTag && !triangles.count)
        return RC_SUCCESS;
    if (vertexTag && (!triangles.vertices || triangles.count > RC_MAX_TRIANGLES))
        return RC_UNSUPPORTED;
    struct CompositeRect src  = {static_cast<LONG>(composite_value(tags, RC_SRC_X, 0)),
                                 static_cast<LONG>(composite_value(tags, RC_SRC_Y, 0)),
                                 composite_value(tags, RC_SRC_WIDTH, sourceExtra->Width),
                                 composite_value(tags, RC_SRC_HEIGHT, sourceExtra->Height)};
    struct CompositeRect clip = {static_cast<LONG>(composite_value(tags, RC_CLIP_X, 0)),
                                 static_cast<LONG>(composite_value(tags, RC_CLIP_Y, 0)),
                                 composite_value(tags, RC_CLIP_WIDTH, destinationExtra->Width),
                                 composite_value(tags, RC_CLIP_HEIGHT, destinationExtra->Height)};
    if (!vertexTag && ((ULONG)clip.x >= destinationExtra->Width || (ULONG)clip.y >= destinationExtra->Height))
        return RC_SUCCESS;
    if (!vertexTag && (src.x > sourceExtra->Width || src.y > sourceExtra->Height))
        return RC_UNSUPPORTED;
    if (src.width > sourceExtra->Width - src.x)
        src.width = sourceExtra->Width - src.x;
    if (src.height > sourceExtra->Height - src.y)
        src.height = sourceExtra->Height - src.y;
    if (clip.width > destinationExtra->Width - clip.x)
        clip.width = destinationExtra->Width - clip.x;
    if (clip.height > destinationExtra->Height - clip.y)
        clip.height = destinationExtra->Height - clip.y;
    float scaleX = IEEESPMul(float_unsigned(MathIeeeSingBasBase, composite_value(tags, RC_SCALE_X, RC_FIXED_ONE)),
                             RC_FIXED_RECIPROCAL);
    float scaleY = IEEESPMul(float_unsigned(MathIeeeSingBasBase, composite_value(tags, RC_SCALE_Y, RC_FIXED_ONE)),
                             RC_FIXED_RECIPROCAL);
    struct CompositeRect dst = {static_cast<LONG>(composite_value(tags, RC_OFFSET_X, 0)),
                                static_cast<LONG>(composite_value(tags, RC_OFFSET_Y, 0)),
                                IEEESPFix(IEEESPMul(IEEESPFlt(src.width), scaleX)),
                                IEEESPFix(IEEESPMul(IEEESPFlt(src.height), scaleY))},
                         visible;
    if (!vertexTag && (!src.width || !src.height || !dst.width || !dst.height || !clip.width || !clip.height ||
                       !intersect(&visible, &dst, &clip)))
        return RC_SUCCESS;
    if (!vertexTag && (dst.x > (LONG)RC_MAX_X || dst.y > (LONG)RC_MAX_Y || (ULONG)dst.x + dst.width > RC_MAX_X ||
                       (ULONG)dst.y + dst.height > RC_MAX_Y))
        return RC_UNSUPPORTED;
    LOCAL_SYSBASE();
    struct RadeonRenderState *r = static_cast<RadeonRenderState *>(AllocVec(sizeof(*r), MEMF_PUBLIC));
    if (!r)
        return RC_NO_MEMORY;
    ULONG result = RC_UNSUPPORTED;
    {
        radeon_render_init(r);
        r->r100 = r100;
        if (r100) {
            r->coordinates = R100_RENDER_COORDINATES_DEFAULT;
            r->stencil     = 0;
        }
        if (!radeon_render_target(bi, r, destinationExtra))
            goto out;
        if (r100)
            render_clip(r, &clip);
        ULONG flags     = composite_value(tags, RC_FLAGS, 0);
        ULONG scissor[] = {radeon_packet3(RC_PACKET_SCISSORS, 2), 0, RADEON_SCISSOR_MAX};
        if (!composite_sync(bi) || !radeon_cp_submit(bi, scissor, ARRAY_SIZE(scissor)))
            goto out;
        if ((has_alpha(destinationExtra) || composite_value(tags, RC_DST_ALPHA_MASK, 0)) &&
            !prepare_destination(MathIeeeSingBasBase, bi, r, vertexTag ? &clip : &visible, flags, tags,
                                 destinationAlphaExtra, vertexTag ? &triangles : NULL))
            goto out;
        if (!radeon_render_texture(bi, r, sourceExtra, 0, !!(flags & RC_FILTER)))
            goto out;
        BOOL sourceMask = !!composite_value(tags, RC_SRC_ALPHA_MASK, 0);
        if (sourceMask && !radeon_render_texture(bi, r, sourceAlphaExtra, 1, !!(flags & RC_FILTER)))
            goto out;
        ULONG textures              = sourceMask ? 2 : 1;
        const struct TagItem *alpha = composite_tag(tags, RC_SRC_ALPHA);
        if (alpha)
            r->solid = alpha_color(alpha->ti_Data);
        if (r100) {
            r->combiner[0].color =
                R100_RENDER_COMBINER_CLAMP | (sourceMask ? R100_RENDER_COLOR_TEXTURE_MASK : R100_RENDER_COLOR_TEXTURE);
            r->combiner[0].alpha =
                R100_RENDER_COMBINER_CLAMP | (sourceMask ? R100_RENDER_ALPHA_TEXTURE_MASK : R100_RENDER_ALPHA_TEXTURE);
        } else {
            r->combiner[0].color = sourceMask ? R200_RENDER_COLOR_TEXTURE_MASK : R200_RENDER_COLOR_TEXTURE;
            r->combiner[0].alpha = sourceMask ? R200_RENDER_ALPHA_TEXTURE_MASK : R200_RENDER_ALPHA_TEXTURE;
        }
        render_stages(r, 1, textures);
        if ((flags & RC_SRC_ALPHA_OVERRIDE) || (alpha && !sourceMask && !has_alpha(sourceExtra))) {
            r->combiner[0].color =
                r100 ? R100_RENDER_COMBINER_CLAMP | R100_RENDER_COLOR_GLOBAL_ALPHA : R200_RENDER_COLOR_GLOBAL_ALPHA;
            r->combiner[0].alpha =
                r100 ? R100_RENDER_COMBINER_CLAMP | R100_RENDER_ALPHA_CONSTANT : R200_RENDER_ALPHA_CONSTANT;
        } else if (alpha) {
            r->combiner[1].color =
                r100 ? R100_RENDER_COMBINER_CLAMP | R100_RENDER_COLOR_MODULATE : R200_RENDER_COLOR_GLOBAL_ALPHA;
            r->combiner[1].alpha =
                r100 ? R100_RENDER_COMBINER_CLAMP | R100_RENDER_ALPHA_MODULATE : R200_RENDER_ALPHA_MODULATE;
            render_stages(r, 2, textures);
        }
        r->control &= ~RC_SEPARATE_ALPHA_ENABLE;
        r->control |= RADEON_RENDER_BLEND_ENABLE;
        render_mask(r, !vertexTag && (flags & RC_IGNORE_DST_ALPHA) ? RC_COLOR_MASK : ~0UL);
        /* The final two accepted operator values retain the existing factors. */
        if (operation <= RC_LAST_PORTER_DUFF_OPERATOR)
            r->blend = composite_blend(operation, !!(flags & RC_IGNORE_DST_ALPHA));
        render_clip(r, &clip);
        if (vertexTag) {
            /* Triangle submissions start with an opaque constant even when a
             * preceding destination pass used a different alpha value.
             */
            if (!alpha)
                r->solid = ~0UL;
            if (render_triangles(bi, r, &triangles, textures, FALSE))
                result = RC_SUCCESS;
            goto out;
        }
        float edgeBias = r100 ? R100_TEXTURE_EDGE_BIAS : RC_TEXTURE_EDGE_BIAS;
        float uv[RADEON_RENDER_TEXTURES * RC_QUAD_VERTICES];
        uv[0] = IEEESPFlt(src.x);
        uv[1] = IEEESPFlt(src.y);
        uv[2] = IEEESPAdd(IEEESPSub(IEEESPAdd(IEEESPFlt(src.width), uv[0]), 1.0f), edgeBias);
        uv[3] = IEEESPAdd(IEEESPSub(IEEESPAdd(IEEESPFlt(src.height), uv[1]), 1.0f), edgeBias);
        if (sourceMask) {
            LONG maskX = r100 ? src.x : composite_value(tags, RC_SRC_ALPHA_X, src.x);
            LONG maskY = r100 ? src.y : composite_value(tags, RC_SRC_ALPHA_Y, src.y);
            uv[4]      = IEEESPFlt(maskX);
            uv[5]      = IEEESPFlt(maskY);
            uv[6]      = IEEESPAdd(IEEESPSub(IEEESPAdd(IEEESPFlt(src.width), uv[4]), 1.0f), edgeBias);
            uv[7]      = IEEESPAdd(IEEESPSub(IEEESPAdd(IEEESPFlt(src.height), uv[5]), 1.0f), edgeBias);
        }
        if (render_quad(MathIeeeSingBasBase, bi, r, &dst, uv, textures, TRUE))
            result = RC_SUCCESS;
    }
out:
    if (!composite_sync(bi))
        result = RC_UNSUPPORTED;
    FreeVec(r);
    return result;
}

/* Keep the math base local to this invocation. No process startup or global
 * C runtime is available in a card library, and concurrent calls must not
 * share a temporarily opened library base.
 */
ULONG RadeonDriver::composite(BOOL r100, ULONG operation, RenderInfo *source, RenderInfo *destination,
                              struct BitMapExtra *sourceExtra, struct BitMapExtra *destinationExtra,
                              struct BitMapExtra *sourceAlphaExtra, struct BitMapExtra *destinationAlphaExtra,
                              const TagItem *tags)
{
    BoardInfo *bi = this;
    LOCAL_SYSBASE();
    Library *math = OpenLibrary("mathieeesingbas.library", 0);
    if (!math)
        return RC_UNSUPPORTED;
    ULONG result;
    {
        RadeonResourceLock lock(bi);
        result = ::composite(math, r100, bi, operation, source, destination, sourceExtra, destinationExtra,
                             sourceAlphaExtra, destinationAlphaExtra, tags);
    }
    CloseLibrary(math);
    return result;
}

/* The P96 compositor uses stack arguments, unlike the older drawing hooks. */
ULONG __stdargs radeon_composite_r200(BoardInfo *bi, ULONG operation, RenderInfo *source, RenderInfo *destination,
                                      struct BitMapExtra *sourceExtra, struct BitMapExtra *destinationExtra,
                                      struct BitMapExtra *sourceAlphaExtra, struct BitMapExtra *destinationAlphaExtra,
                                      const TagItem *tags)
{
    return asRadeon(bi)->composite(FALSE, operation, source, destination, sourceExtra, destinationExtra,
                                   sourceAlphaExtra, destinationAlphaExtra, tags);
}

ULONG __stdargs radeon_composite_r100(BoardInfo *bi, ULONG operation, RenderInfo *source, RenderInfo *destination,
                                      struct BitMapExtra *sourceExtra, struct BitMapExtra *destinationExtra,
                                      struct BitMapExtra *sourceAlphaExtra, struct BitMapExtra *destinationAlphaExtra,
                                      const TagItem *tags)
{
    return asRadeon(bi)->composite(TRUE, operation, source, destination, sourceExtra, destinationExtra,
                                   sourceAlphaExtra, destinationAlphaExtra, tags);
}

STATIC_ASSERT(offsetof(struct BoardInfo, Reserved3Default) == RADEON_P96_COMPOSITE_OFFSET,
              radeon_composite_callback_offset);

void radeon_install_composite(struct BoardInfo *bi)
{
    const struct RadeonState *state = getCardData(bi)->state;
    if (state->cp.active &&
        (state->device->engine == RADEON_ENGINE_R100 || state->device->engine == RADEON_ENGINE_R200)) {
        /* Modern P96 assigns this formerly reserved slot to its stack-ABI
         * compositor. The bundled older SDK still declares the legacy type.
         */
        P96_HOOK(bi->Reserved3Default,
                 state->device->engine == RADEON_ENGINE_R100 ? radeon_composite_r100 : radeon_composite_r200);
    }
}
