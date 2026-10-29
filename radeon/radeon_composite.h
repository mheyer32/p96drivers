#ifndef RADEON_COMPOSITE_H
#define RADEON_COMPOSITE_H

/* P96's compositing entry uses stack arguments, independently of the register
 * calling conventions used by its older drawing callbacks.
 */
enum RadeonCompositeTag
{
    RC_SRC_X = TAG_USER,
    RC_SRC_Y,
    RC_SRC_WIDTH,
    RC_SRC_HEIGHT,
    RC_CLIP_X,
    RC_CLIP_Y,
    RC_CLIP_WIDTH,
    RC_CLIP_HEIGHT,
    RC_SRC_ALPHA,
    RC_DST_ALPHA,
    RC_SCALE_X,
    RC_SCALE_Y,
    RC_SRC_ALPHA_MASK,
    RC_DST_ALPHA_MASK,
    RC_SRC_ALPHA_X,
    RC_SRC_ALPHA_Y,
    RC_DST_ALPHA_X,
    RC_DST_ALPHA_Y,
    RC_FLAGS,
    RC_OFFSET_X = TAG_USER + 20,
    RC_OFFSET_Y,
    RC_VERTEX_ARRAY = TAG_USER + 30,
    RC_INDEX_ARRAY,
    RC_VERTEX_FORMAT,
    RC_TRIANGLES
};
constexpr auto RC_SRC_ALPHA_OVERRIDE           = (1UL << 0);
constexpr auto RC_DST_ALPHA_OVERRIDE           = (1UL << 1);
constexpr auto RC_FILTER                       = (1UL << 2);
constexpr auto RC_IGNORE_DST_ALPHA             = (1UL << 4);
constexpr auto RC_FIXED_ONE                    = 65536UL;
constexpr auto RC_FIXED_RECIPROCAL             = (1.0f / 65536.0f);
constexpr auto RC_INTEGER_SIGN                 = (1UL << 31);
constexpr auto RC_ALPHA_SHIFT                  = 8u;
constexpr auto RC_ALPHA_BYTE_MAX               = 255u;
constexpr auto RC_ALPHA_CHANNEL_SHIFT          = 24u;
constexpr auto RC_ALPHA_MASK                   = 0xff000000UL;
constexpr auto RC_COLOR_MASK                   = 0x00ffffffUL;
constexpr auto RC_MAX_OPERATOR                 = 14u;
constexpr auto RC_LAST_PORTER_DUFF_OPERATOR    = 12u;
constexpr auto RC_SUCCESS                      = 0u;
constexpr auto RC_UNSUPPORTED                  = 3u;
constexpr auto RC_NO_MEMORY                    = 4u;
constexpr auto RC_BAD_OPERATOR                 = 6u;
constexpr auto RC_BAD_TRIANGLES                = 7u;
constexpr auto RC_MAX_X                        = 2047u;
constexpr auto RC_MAX_Y                        = 1535u;
constexpr auto RC_QUAD_VERTICES                = 4u;
constexpr auto RC_RECTANGLE_TRIANGLE_VERTICES  = 6u;
constexpr auto R100_VERTEX_PACKET_HEADER_WORDS = 3u;
constexpr auto R100_PACKET_DRAW_IMMEDIATE      = 0x29u;
constexpr auto R100_VERTEX_TEXTURE0            = 0x81u;
constexpr auto R100_VERTEX_TEXTURE1            = 0x300u;
constexpr auto R100_VERTEX_WALK_DATA           = 0x1f0u;
constexpr auto R100_TEXTURE_EDGE_BIAS          = 0.9f;
constexpr auto RC_TRIANGLE_VERTICES            = 3u;
constexpr auto RC_MAX_TRIANGLES                = (0xffffffffUL / RC_TRIANGLE_VERTICES);
constexpr auto RC_VERTEX_PACKET_WORDS          = 4080u;
constexpr auto RC_VERTEX_PACKET_HEADER_WORDS   = 2u;
constexpr auto RC_VERTEX_SOURCE_TEXTURE        = (1UL << 1);
constexpr auto RC_VERTEX_MASK_TEXTURE          = (1UL << 2);
constexpr auto RC_TEXTURE_VERTEX_UNIT_SHIFT    = 3u;
constexpr auto RC_VERTEX_COORDINATES           = 2u;
constexpr auto RC_VERTEX_TEXTURE_COORDINATES   = 3u;
constexpr auto RC_TEXTURE_EDGE_BIAS            = 0.8f;
constexpr auto RC_BLEND_ZERO                   = 32u;
constexpr auto RC_BLEND_ONE                    = 33u;
constexpr auto RC_BLEND_SRC_ALPHA              = 38u;
constexpr auto RC_BLEND_INV_SRC_ALPHA          = 39u;
constexpr auto RC_BLEND_DST_ALPHA              = 40u;
constexpr auto RC_BLEND_INV_DST_ALPHA          = 41u;
constexpr auto RC_BLEND_SOURCE_SHIFT           = 16u;
constexpr auto RC_BLEND_DEST_SHIFT             = 24u;
constexpr ULONG radeon_blend_factors(ULONG source, ULONG destination)
{
    return (source << RC_BLEND_SOURCE_SHIFT) | (destination << RC_BLEND_DEST_SHIFT);
}
constexpr auto RC_PACKET_SCISSORS           = 0x1eu;
constexpr auto RC_PACKET_DRAW_IMMEDIATE     = 0x35u;
constexpr auto RC_PRIMITIVE_TRIANGLE_FAN    = 5u;
constexpr auto RC_PRIMITIVE_TRIANGLE_LIST   = 4u;
constexpr auto RC_VERTEX_WALK_DATA          = (3u << 4);
constexpr auto RC_VERTEX_COUNT_SHIFT        = 16u;
constexpr auto RC_TEXTURE_VERTEX_FORMAT_ONE = 3u;
constexpr auto RC_TEXTURE_VERTEX_FORMAT_TWO = (3u | (3u << 3));
constexpr auto RC_SEPARATE_ALPHA_ENABLE     = (1UL << 16);

ULONG __stdargs radeon_composite_r200(struct BoardInfo *bi, ULONG operation, struct RenderInfo *source,
                                      struct RenderInfo *destination, struct BitMapExtra *sourceExtra,
                                      struct BitMapExtra *destinationExtra, struct BitMapExtra *sourceAlphaExtra,
                                      struct BitMapExtra *destinationAlphaExtra, const struct TagItem *tags);
ULONG __stdargs radeon_composite_r100(struct BoardInfo *bi, ULONG operation, struct RenderInfo *source,
                                      struct RenderInfo *destination, struct BitMapExtra *sourceExtra,
                                      struct BitMapExtra *destinationExtra, struct BitMapExtra *sourceAlphaExtra,
                                      struct BitMapExtra *destinationAlphaExtra, const struct TagItem *tags);
void radeon_install_composite(struct BoardInfo *bi);
#endif
