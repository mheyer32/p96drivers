#include "radeon_driver.hpp"

#include <stddef.h>

using namespace RadeonReg;

constexpr auto OV_RGB15                = 3u;
constexpr auto OV_RGB16                = 4u;
constexpr auto OV_RGB32                = 6u;
constexpr auto OV_YUV9                 = 9u;
constexpr auto OV_YUV12                = 10u;
constexpr auto OV_YUV422               = 11u;
constexpr auto OV_FIXED_ONE            = 4096UL;
constexpr auto OV_PHASE_STEPS          = 32u;
constexpr auto OV_PHASE_SHIFT          = 15u;
constexpr auto OV_H_INTEGER_SHIFT      = 28u;
constexpr auto OV_H_FRACTION_MASK      = 0x000f8000UL;
constexpr auto OV_V_PHASE_MASK         = 0x03ff8000UL;
constexpr auto OV_CHROMA_PHASE_MASK    = 0x01ff8000UL;
constexpr auto OV_MIN_SCALE_DIVISOR    = 8u;
constexpr auto OV_MAX_SCALE_FACTOR     = 16u;
constexpr auto OV_MAX_SOURCE_DIMENSION = 2048u;
constexpr auto OV_BLANK_LUMA           = 0xfffu;
constexpr auto OV_BLANK_CHROMA         = 0x7ffu;
constexpr auto OV_FILTER_ALL_PLANES    = 0x0fu;
constexpr auto OV_VIDEO_KEY_TRUE       = 1u;
constexpr auto OV_GRAPHICS_KEY_TRUE    = (1u << 4);
constexpr auto OV_GRAPHICS_KEY_EQUAL   = (2u << 4);
constexpr auto OV_KEY_AND              = (1u << 8);
constexpr auto OV_KEY_RGB16            = 0x00100010UL;
constexpr auto OV_KEY_RGB32            = 0x00110011UL;
constexpr auto OV_KEY_ALPHA_MASK       = 0xff000000UL;
constexpr auto OV_CHROMA_PITCH_SELECT  = 1u;
constexpr auto OV_LUMA_ALIGNMENT       = 32u;
constexpr auto OV_CHROMA_ALIGNMENT     = 16u;
constexpr ULONG overlayCoefficients(LONG a, LONG b, LONG c, LONG d)
{
    return (a & 15UL) | ((b & 127UL) << 8) | ((c & 127UL) << 16) | ((d & 15UL) << 24);
}

static const struct
{
    UBYTE first;
    ULONG taps[5];
} filters[] = {
#include "radeon_filter.h"
};
static const ULONG enlargementFilter[5] = {overlayCoefficients(0, 32, 0, 0), overlayCoefficients(-2, 29, 5, 0),
                                           overlayCoefficients(-3, 27, 9, -1), overlayCoefficients(-4, 24, 14, -2),
                                           overlayCoefficients(-3, 19, 19, -3)};

static ULONG overlay_format(RGBFTYPE format)
{
    switch (format) {
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G6B5:
        return OV_RGB16;
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G5B5:
        return OV_RGB15;
    case RGBFB_YUV422CGX:
    case RGBFB_YUV422:
    case RGBFB_YUV422PC:
        return OV_YUV422;
    case RGBFB_YUV422PA:
        return OV_YUV12;
    case RGBFB_YUV422PAPC:
        return OV_YUV9;
    default:
        return OV_RGB32;
    }
}

static ULONG rounded_divide(ULONG numerator, ULONG denominator)
{
    return (numerator + denominator / 2) / denominator;
}

static ULONG horizontal_phase(ULONG increment)
{
    /* Start the four-tap filter 2.5 pixels into its input, rounded to the
     * scaler's 1/32-pixel phase resolution.
     */
    ULONG phase = 5 * OV_PHASE_STEPS / 2 + rounded_divide(increment, 2 * OV_FIXED_ONE / OV_PHASE_STEPS);
    return ((phase / OV_PHASE_STEPS) << OV_H_INTEGER_SHIFT) | ((phase << OV_PHASE_SHIFT) & OV_H_FRACTION_MASK);
}

void radeon_overlay_scale(struct RadeonOverlay *image, const struct RadeonWindow *window)
{
    ULONG format = overlay_format(window->format);
    ULONG source = window->sourceWidth, dest = window->width;
    ULONG group1 = 1, group23 = 1;
    /* The prescaler fetches groups of 1, 2, 4 or 8 source pixels. RGB32
     * consumes twice the bandwidth of RGB16; planar chroma has its own
     * sampling rate. Select groups before computing the 12-bit increments.
     */
    if (format == OV_RGB15 || format == OV_RGB16) {
        group1  = dest * 2 >= source ? 1 : dest * 4 >= source ? 2 : 4;
        group23 = group1;
    } else if (format == OV_RGB32) {
        group1  = dest * 4 >= source * 3 ? 1 : dest * 4 >= source ? 2 : 4;
        group23 = dest * 4 >= source * 3 ? 1 : dest * 8 >= source * 3 ? 2 : dest * 16 >= source * 3 ? 4 : 8;
    } else if (format == OV_YUV9) {
        group1  = rounded_divide(source * OV_FIXED_ONE, dest) <= 3 * OV_FIXED_ONE ? 1 : dest * 16 >= source * 3 ? 2 : 4;
        group23 = dest * 8 >= source * 3 ? 1 : dest * 16 >= source * 3 ? 2 : 4;
    } else {
        group1  = dest * 8 >= source * 3 ? 1 : dest * 16 >= source * 3 ? 2 : 4;
        group23 = dest * 2 >= source ? 1 : dest * 4 >= source ? 2 : 4;
    }
    ULONG chromaDivisor = format == OV_YUV9 ? 4 : format >= OV_YUV12 ? 2 : 1;
    ULONG inc1          = rounded_divide(source * OV_FIXED_ONE, dest * group1);
    ULONG inc23         = rounded_divide(source * OV_FIXED_ONE, dest * group23 * chromaDivisor);
    ULONG step1 = 1, step23 = 1;
    for (ULONG group = group1; group > 1; group >>= 1)
        ++step1;
    for (ULONG group = group23; group > 1; group >>= 1)
        ++step23;
    image->hInc  = (inc23 << RADEON_FIELD_HIGH_SHIFT) | inc1;
    image->step  = (step23 << 8) | step1;
    image->xEnd1 = ((source - 1) / group1) * group1;
    image->xEnd2 = image->xEnd3 = ((source / 2 - 1) / group23) * group23;
    image->hAccum1              = horizontal_phase(inc1);
    image->hAccum23             = horizontal_phase(inc23);
    ULONG vInc                  = window->sourceHeight * OV_FIXED_ONE / window->height;
    image->vInc                 = vInc << 8;
    image->blank1               = ((ULONG)(window->sourceHeight - 1) << RADEON_FIELD_HIGH_SHIFT) | OV_BLANK_LUMA;
    image->blank23              = ((ULONG)(window->sourceHeight - 1) << RADEON_FIELD_HIGH_SHIFT) | OV_BLANK_CHROMA;
    ULONG phase =
        minu(5 * OV_PHASE_STEPS / 2, 3 * OV_PHASE_STEPS / 2 + rounded_divide(vInc, 2 * OV_FIXED_ONE / OV_PHASE_STEPS));
    image->vAccum1  = ((phase << OV_PHASE_SHIFT) & OV_V_PHASE_MASK) | 1;
    image->vAccum23 = 1;
    if (format == OV_YUV9 || format == OV_YUV12) {
        phase = minu(5 * OV_PHASE_STEPS / 2,
                     3 * OV_PHASE_STEPS / 2 + rounded_divide(vInc, chromaDivisor * 2 * OV_FIXED_ONE / OV_PHASE_STEPS));
        image->vAccum23 |= (phase << OV_PHASE_SHIFT) & OV_CHROMA_PHASE_MASK;
    }
    /* Filter selection uses the effective horizontal scale after grouping.
     * Truncate the scale to hundredths before selecting a coefficient row.
     */
    const ULONG *coefficients = enlargementFilter;
    if (inc1 >= OV_FIXED_ONE) {
        LONG index = (LONG)(100 * OV_FIXED_ONE / inc1) - 25;
        ULONG row  = 0;
        while (row + 1 < sizeof(filters) / sizeof(filters[0]) && index >= filters[row + 1].first)
            ++row;
        coefficients = filters[row].taps;
    }
    for (ULONG i = 0; i < 5; ++i)
        image->coefficient[i] = coefficients[i];
    image->scale  = RADEON_OVERLAY_ENABLE | RADEON_OVERLAY_BURST_PER_PLANE | (format << RADEON_OVERLAY_FORMAT_SHIFT);
    image->filter = 0;
    if (format <= OV_RGB32) {
        image->scale |= RADEON_OVERLAY_RGB_CONVERSION;
        image->filter = OV_FILTER_ALL_PLANES;
    }
}

static const struct TagItem *next_tag(const struct TagItem **cursor)
{
    while (*cursor) {
        const struct TagItem *tag = (*cursor)++;
        switch (tag->ti_Tag) {
        case TAG_END:
            *cursor = NULL;
            return NULL;
        case TAG_IGNORE:
            break;
        case TAG_SKIP:
            *cursor += tag->ti_Data;
            break;
        case TAG_MORE:
            *cursor = (const struct TagItem *)tag->ti_Data;
            break;
        default:
            return tag;
        }
    }
    return NULL;
}

static BOOL valid_window(const struct RadeonWindow *window)
{
    return window->format >= RGBFB_R5G6B5PC && radeon_bytes_per_pixel(window->format) && window->sourceWidth >= 2 &&
           window->sourceWidth <= OV_MAX_SOURCE_DIMENSION && window->sourceHeight &&
           window->sourceHeight <= OV_MAX_SOURCE_DIMENSION && window->width && window->height &&
           (ULONG)window->width * OV_MIN_SCALE_DIVISOR >= window->sourceWidth &&
           (ULONG)window->height * OV_MIN_SCALE_DIVISOR >= window->sourceHeight &&
           window->width <= (ULONG)window->sourceWidth * OV_MAX_SCALE_FACTOR &&
           window->height <= (ULONG)window->sourceHeight * OV_MAX_SCALE_FACTOR;
}

static void free_buffers(struct BoardInfo *bi, struct RadeonWindow *window)
{
    for (ULONG i = 0; i < window->numBuffers; ++i)
        if (window->buffers[i]) {
            bi->FreeBitMap(bi, window->buffers[i], NULL);
            window->buffers[i] = NULL;
        }
}

static BOOL allocate_buffers(struct BoardInfo *bi, struct RadeonWindow *window)
{
    window->allocationTags[0].ti_Data = window->format;
    for (ULONG i = 0; i < window->numBuffers; ++i) {
        window->buffers[i] = bi->AllocBitMap(bi, window->sourceWidth, window->sourceHeight, window->allocationTags);
        if (!window->buffers[i]) {
            free_buffers(bi, window);
            return FALSE;
        }
    }
    return TRUE;
}

static void disable_overlay(struct BoardInfo *bi)
{
    radeon_update(bi, RADEON_OV0_SCALE_CNTL, RADEON_OVERLAY_ENABLE, 0);
    getCardData(bi)->state->visibleWindow = NULL;
}

static BOOL commit_overlay(struct BoardInfo *bi, const struct RadeonOverlay *image)
{
#define OV_REGISTER(field, reg)                                 \
    {                                                           \
        offsetof(struct RadeonOverlay, field), RADEON_OV0_##reg \
    }
    static const struct
    {
        UWORD offset;
        RadeonReg::Id reg;
    } registers[] = {OV_REGISTER(start, Y_X_START),
                     OV_REGISTER(end, Y_X_END),
                     OV_REGISTER(scale, SCALE_CNTL),
                     OV_REGISTER(vInc, V_INC),
                     OV_REGISTER(vAccum1, P1_V_ACCUM_INIT),
                     OV_REGISTER(vAccum23, P23_V_ACCUM_INIT),
                     OV_REGISTER(blank1, P1_BLANK_LINES_AT_TOP),
                     OV_REGISTER(blank23, P23_BLANK_LINES_AT_TOP),
                     OV_REGISTER(hInc, H_INC),
                     OV_REGISTER(step, STEP_BY),
                     OV_REGISTER(hAccum1, P1_H_ACCUM_INIT),
                     OV_REGISTER(hAccum23, P23_H_ACCUM_INIT),
                     OV_REGISTER(xEnd1, P1_X_START_END),
                     OV_REGISTER(xEnd2, P2_X_START_END),
                     OV_REGISTER(xEnd3, P3_X_START_END),
                     OV_REGISTER(videoKeyLow, VIDEO_KEY_CLR_LOW),
                     OV_REGISTER(videoKeyHigh, VIDEO_KEY_CLR_HIGH),
                     OV_REGISTER(graphicsKeyLow, GRAPHICS_KEY_CLR_LOW),
                     OV_REGISTER(graphicsKeyHigh, GRAPHICS_KEY_CLR_HIGH),
                     OV_REGISTER(key, KEY_CNTL),
                     OV_REGISTER(base[0], VID_BUF0_BASE_ADRS),
                     OV_REGISTER(base[1], VID_BUF1_BASE_ADRS),
                     OV_REGISTER(base[2], VID_BUF2_BASE_ADRS),
                     OV_REGISTER(base[3], VID_BUF3_BASE_ADRS),
                     OV_REGISTER(base[4], VID_BUF4_BASE_ADRS),
                     OV_REGISTER(base[5], VID_BUF5_BASE_ADRS),
                     OV_REGISTER(pitch0, VID_BUF_PITCH0_VALUE),
                     OV_REGISTER(pitch1, VID_BUF_PITCH1_VALUE),
                     OV_REGISTER(filter, FILTER_CNTL),
                     OV_REGISTER(coefficient[0], FOUR_TAP_COEF_0),
                     OV_REGISTER(coefficient[1], FOUR_TAP_COEF_1),
                     OV_REGISTER(coefficient[2], FOUR_TAP_COEF_2),
                     OV_REGISTER(coefficient[3], FOUR_TAP_COEF_3),
                     OV_REGISTER(coefficient[4], FOUR_TAP_COEF_4)};
#undef OV_REGISTER
    radeon_write(bi, RADEON_OV0_REG_LOAD_CNTL, RADEON_OVERLAY_LOAD_LOCK);
    for (ULONG poll = 0; poll < RADEON_POLL_LIMIT; ++poll) {
        if (!(radeon_read(bi, RADEON_OV0_REG_LOAD_CNTL) & RADEON_OVERLAY_LOAD_READY))
            continue;
        for (ULONG i = 0; i < sizeof(registers) / sizeof(registers[0]); ++i)
            radeon_write(bi, registers[i].reg, *(const ULONG *)((const UBYTE *)image + registers[i].offset));
        radeon_write(bi, RADEON_OV0_REG_LOAD_CNTL, 0);
        return TRUE;
    }
    radeon_write(bi, RADEON_OV0_REG_LOAD_CNTL, 0);
    disable_overlay(bi);
    return FALSE;
}

static void show_window(struct BoardInfo *bi, struct RadeonWindow *window)
{
    if ((window->flags & RADEON_WINDOW_VISIBLE) != RADEON_WINDOW_VISIBLE || !bi->ModeInfo) {
        if (getCardData(bi)->state->visibleWindow == window)
            disable_overlay(bi);
        return;
    }
    LONG left = (WORD)(window->left - bi->XOffset), top = (WORD)(window->top - bi->YOffset);
    LONG right       = min(left + window->width - 1, bi->ModeInfo->Width - 1);
    LONG bottom      = min(top + window->height - 1, bi->ModeInfo->Height - 1);
    window->clipLeft = max(-left, 0);
    window->clipTop  = max(-top, 0);
    left             = max(left, 0);
    top              = max(top, 0);
    if (right < left || bottom < top) {
        disable_overlay(bi);
        return;
    }
    struct RadeonOverlay image;
    image.videoKeyLow = image.videoKeyHigh = 0;
    image.graphicsKeyLow = image.graphicsKeyHigh = 0;
    image.start                                  = ((ULONG)top << RADEON_FIELD_HIGH_SHIFT) | left;
    image.end                                    = ((ULONG)bottom << RADEON_FIELD_HIGH_SHIFT) | right;
    radeon_overlay_scale(&image, window);
    struct BitMap *bitmap = window->buffers[window->displayedBuffer];
    ULONG memory          = bi->GetBitMapAttr(bi, bitmap, GBMA_MEMORY);
    ULONG pitch           = bi->GetBitMapAttr(bi, bitmap, GBMA_BYTESPERROW);
    ULONG bytes           = bi->GetBitMapAttr(bi, bitmap, GBMA_BYTESPERPIXEL);
    ULONG offset          = memory - (ULONG)bi->MemorySpaceBase;
    if (!memory || !pitch || !bytes || offset >= bi->MemorySpaceSize ||
        window->sourceHeight > (bi->MemorySpaceSize - offset) / pitch) {
        disable_overlay(bi);
        return;
    }
    ULONG clipBytes = (ULONG)window->clipLeft * window->sourceWidth / window->width * bytes;
    ULONG clipRows  = (ULONG)window->clipTop * window->sourceHeight / window->height * pitch;
    for (ULONG i = 0; i < 6; ++i)
        image.base[i] = offset + clipRows + clipBytes;
    image.pitch0 = image.pitch1 = pitch;
    ULONG format                = overlay_format(window->format);
    if (format == OV_YUV9 || format == OV_YUV12) {
        ULONG divisor = format == OV_YUV9 ? 4 : 2;
        ULONG chroma  = offset + (ULONG)window->sourceHeight * pitch;
        image.base[0] &= ~(OV_LUMA_ALIGNMENT - 1);
        image.base[1] = (chroma + clipBytes / divisor) & ~(OV_CHROMA_ALIGNMENT - 1);
        image.base[2] = (chroma + (pitch + clipBytes) / divisor) & ~(OV_CHROMA_ALIGNMENT - 1);
        if (format == OV_YUV9) {
            image.base[1] |= OV_CHROMA_PITCH_SELECT;
            image.base[2] |= OV_CHROMA_PITCH_SELECT;
            image.pitch1 >>= 1;
        }
        for (ULONG i = 0; i < 3; ++i)
            image.base[i + 3] = image.base[i];
    }
    image.key = OV_KEY_AND | OV_VIDEO_KEY_TRUE | OV_GRAPHICS_KEY_TRUE;
    if (window->occlusion || (bi->EssentialFormats & (1UL << bi->RGBFormat))) {
        /* P96 marks uncovered pixels with a reserved key pattern. The
         * comparison spans all alpha values, preserving screen occlusion.
         */
        image.graphicsKeyLow  = window->modeFormat == RGBFB_R5G6B5PC || window->modeFormat == RGBFB_R5G6B5 ||
                                        window->modeFormat == RGBFB_B5G6R5PC
                                    ? OV_KEY_RGB16
                                    : OV_KEY_RGB32;
        image.graphicsKeyHigh = image.graphicsKeyLow | OV_KEY_ALPHA_MASK;
        image.key             = OV_KEY_AND | OV_VIDEO_KEY_TRUE | OV_GRAPHICS_KEY_EQUAL;
    }
    if (commit_overlay(bi, &image))
        getCardData(bi)->state->visibleWindow = window;
}

static void apply_tags(struct RadeonWindow *window, const struct TagItem *tags)
{
    const struct TagItem *tag;
    while ((tag = next_tag(&tags))) {
        ULONG value = tag->ti_Data;
        switch (tag->ti_Tag) {
        case FA_Onboard:
            window->flags = (window->flags & ~RADEON_WINDOW_ONBOARD) | (value ? RADEON_WINDOW_ONBOARD : 0);
            break;
        case FA_Active:
            window->flags = (window->flags & ~RADEON_WINDOW_ACTIVE) | (value ? RADEON_WINDOW_ACTIVE : 0);
            break;
        case FA_Left:
            window->left = value;
            break;
        case FA_Top:
            window->top = value;
            break;
        case FA_Width:
            window->width = value;
            break;
        case FA_Height:
            window->height = value;
            break;
        case FA_Format:
            window->format = AS_RGBF(value);
            break;
        case FA_SourceWidth:
            window->sourceWidth = value;
            break;
        case FA_SourceHeight:
            window->sourceHeight = value;
            break;
        case FA_Color:
            window->color = value;
            break;
        case FA_Pen:
            window->pen = value;
            break;
        case FA_Occlusion:
            window->occlusion = value;
            break;
        case FA_DisplayedBuffer:
            if (value < window->numBuffers)
                window->displayedBuffer = value;
            break;
        case FA_RenderBuffer:
            if (value < window->numBuffers)
                window->renderBuffer = value;
            break;
        default:
            break;
        }
    }
}

APTR ASM RadeonDriver::createFeature(__REGD0(ULONG type), __REGA1(struct TagItem *tags))
{
    BoardInfo *bi = this;
    if (type != SFT_MEMORYWINDOW || !bi->AllocBitMap || !bi->FreeBitMap || !bi->GetBitMapAttr)
        return NULL;
    LOCAL_SYSBASE();
    struct RadeonWindow *window = static_cast<RadeonWindow *>(AllocVec(sizeof(*window), MEMF_PUBLIC | MEMF_CLEAR));
    if (!window)
        return NULL;
    window->numBuffers           = 1;
    window->mode                 = bi->ModeInfo;
    window->modeFormat           = bi->RGBFormat;
    window->allocationTags[0]    = TagItem{ABMA_RGBFormat, 0};
    window->allocationTags[1]    = TagItem{ABMA_Clear, TRUE};
    window->allocationTags[2]    = TagItem{ABMA_Displayable, TRUE};
    window->allocationTags[3]    = TagItem{ABMA_Visible, TRUE};
    const struct TagItem *cursor = tags, *tag;
    ULONG count                  = 4;
    while ((tag = next_tag(&cursor))) {
        ULONG mapped = TAG_IGNORE;
        switch (tag->ti_Tag) {
        case FA_NumBuffers:
            window->numBuffers = max(1, minu(tag->ti_Data, RADEON_OVERLAY_BUFFERS));
            break;
        case FA_ModeInfo:
            window->mode = (struct ModeInfo *)tag->ti_Data;
            break;
        case FA_ModeFormat:
            window->modeFormat = AS_RGBF(tag->ti_Data);
            break;
        case FA_NoMemory:
            mapped = ABMA_NoMemory;
            break;
        case FA_RenderFunc:
            mapped = ABMA_RenderFunc;
            break;
        case FA_SaveFunc:
            mapped = ABMA_SaveFunc;
            break;
        case FA_UserData:
            mapped = ABMA_UserData;
            break;
        case FA_Alignment:
            mapped = ABMA_Alignment;
            break;
        case FA_ConstantBytesPerRow:
            mapped = ABMA_ConstantBytesPerRow;
            break;
        default:
            break;
        }
        if (mapped != TAG_IGNORE && count + 1 < sizeof(window->allocationTags) / sizeof(window->allocationTags[0]))
            window->allocationTags[count++] = TagItem{mapped, tag->ti_Data};
    }
    window->renderBuffer = window->numBuffers > 1 ? 1 : 0;
    apply_tags(window, tags);
    if (!window->width)
        window->width = window->sourceWidth;
    if (!window->height)
        window->height = window->sourceHeight;
    if (!valid_window(window) || !allocate_buffers(bi, window)) {
        FreeVec(window);
        return NULL;
    }
    window->next                    = getCardData(bi)->state->windows;
    getCardData(bi)->state->windows = window;
    show_window(bi, window);
    return window;
}

static APTR ASM RadeonCreateFeature(__REGA0(struct BoardInfo *bi), __REGD0(ULONG type), __REGA1(struct TagItem *tags))
{
    return asRadeon(bi)->createFeature(type, tags);
}

ULONG ASM RadeonDriver::setFeatureAttrs(__REGA1(APTR feature), __REGD0(ULONG type), __REGA2(struct TagItem *tags))
{
    BoardInfo *bi = this;
    if (type != SFT_MEMORYWINDOW || !feature)
        return 0;
    LOCAL_SYSBASE();
    struct RadeonWindow *window = static_cast<RadeonWindow *>(feature), updated;
    CopyMem(window, &updated, sizeof(updated));
    apply_tags(&updated, tags);
    if (!valid_window(&updated))
        return 0;
    if (updated.sourceWidth != window->sourceWidth || updated.sourceHeight != window->sourceHeight ||
        updated.format != window->format) {
        for (ULONG i = 0; i < RADEON_OVERLAY_BUFFERS; ++i)
            updated.buffers[i] = NULL;
        /* Keep the displayed allocation alive until its replacement exists. */
        if (!allocate_buffers(bi, &updated))
            return 0;
        if (getCardData(bi)->state->visibleWindow == window)
            disable_overlay(bi);
        free_buffers(bi, window);
    }
    CopyMem(&updated, window, sizeof(updated));
    show_window(bi, window);
    return 0;
}

static ULONG ASM RadeonSetFeatureAttrs(__REGA0(struct BoardInfo *bi), __REGA1(APTR feature), __REGD0(ULONG type),
                                       __REGA2(struct TagItem *tags))
{
    return asRadeon(bi)->setFeatureAttrs(feature, type, tags);
}

ULONG ASM RadeonDriver::getFeatureAttrs(__REGA1(APTR feature), __REGD0(ULONG type), __REGA2(struct TagItem *tags))
{
    BoardInfo *bi = this;
    (void)bi;
    if (type != SFT_MEMORYWINDOW || !feature)
        return 0;
    const struct RadeonWindow *window = static_cast<const RadeonWindow *>(feature);
    const struct TagItem *cursor      = tags, *tag;
    while ((tag = next_tag(&cursor))) {
        ULONG value;
        switch (tag->ti_Tag) {
        case FA_Active:
            value = !!(window->flags & RADEON_WINDOW_ACTIVE);
            break;
        case FA_Left:
            value = window->left;
            break;
        case FA_Top:
            value = window->top;
            break;
        case FA_Width:
            value = window->width;
            break;
        case FA_Height:
            value = window->height;
            break;
        case FA_Format:
            value = window->format;
            break;
        case FA_SourceWidth:
            value = window->sourceWidth;
            break;
        case FA_SourceHeight:
            value = window->sourceHeight;
            break;
        case FA_MinWidth:
            value = window->sourceWidth / OV_MIN_SCALE_DIVISOR;
            break;
        case FA_MinHeight:
            value = window->sourceHeight / OV_MIN_SCALE_DIVISOR;
            break;
        case FA_MaxWidth:
            value = window->sourceWidth * OV_MAX_SCALE_FACTOR;
            break;
        case FA_MaxHeight:
            value = window->sourceHeight * OV_MAX_SCALE_FACTOR;
            break;
        case FA_NumBuffers:
            value = window->numBuffers;
            break;
        case FA_DisplayedBuffer:
            value = window->displayedBuffer;
            break;
        case FA_RenderBuffer:
            value = window->renderBuffer;
            break;
        case FA_BitMap:
            value = (ULONG)window->buffers[window->renderBuffer];
            break;
        default:
            continue;
        }
        if (tag->ti_Data)
            *(ULONG *)tag->ti_Data = value;
    }
    return 0;
}

static ULONG ASM RadeonGetFeatureAttrs(__REGA0(struct BoardInfo *bi), __REGA1(APTR feature), __REGD0(ULONG type),
                                       __REGA2(struct TagItem *tags))
{
    return asRadeon(bi)->getFeatureAttrs(feature, type, tags);
}

BOOL ASM RadeonDriver::deleteFeature(__REGA1(APTR feature), __REGD0(ULONG type))
{
    BoardInfo *bi = this;
    if (type != SFT_MEMORYWINDOW || !feature)
        return FALSE;
    struct RadeonWindow *window = static_cast<RadeonWindow *>(feature), **link = &getCardData(bi)->state->windows;
    while (*link && *link != window)
        link = &(*link)->next;
    if (!*link)
        return FALSE;
    if (getCardData(bi)->state->visibleWindow == window)
        disable_overlay(bi);
    *link = window->next;
    free_buffers(bi, window);
    LOCAL_SYSBASE();
    FreeVec(window);
    return TRUE;
}

static BOOL ASM RadeonDeleteFeature(__REGA0(struct BoardInfo *bi), __REGA1(APTR feature), __REGD0(ULONG type))
{
    return asRadeon(bi)->deleteFeature(feature, type);
}

void radeon_refresh_overlay(struct BoardInfo *bi)
{
    struct RadeonState *state = getCardData(bi)->state;
    if (!state)
        return;
    for (struct RadeonWindow *window = state->windows; window; window = window->next)
        show_window(bi, window);
}

void radeon_free_windows(struct BoardInfo *bi)
{
    while (getCardData(bi)->state->windows)
        RadeonDeleteFeature(bi, getCardData(bi)->state->windows, SFT_MEMORYWINDOW);
}

void radeon_install_overlay(struct BoardInfo *bi)
{
    P96_HOOK(bi->CreateFeature, RadeonCreateFeature);
    P96_HOOK(bi->SetFeatureAttrs, RadeonSetFeatureAttrs);
    P96_HOOK(bi->GetFeatureAttrs, RadeonGetFeatureAttrs);
    P96_HOOK(bi->DeleteFeature, RadeonDeleteFeature);
    bi->Flags |= BIF_VIDEOWINDOW;
    disable_overlay(bi);
    radeon_write(bi, RADEON_OV0_TEST, 0);
    radeon_write(bi, RADEON_OV0_COLOUR_CNTL, 0);
    /* BT.601 studio-range YCbCr to RGB coefficients, including the luma
     * pedestal and chroma center offsets, in the scaler's fixed-point format.
     */
    static const ULONG conversion[] = {0x12a00000UL, 0x1990190eUL, 0x12a0f9c0UL,
                                       0xf3000442UL, 0x12a02040UL, 0x0000175fUL};
    for (ULONG i = 0; i < sizeof(conversion) / sizeof(conversion[0]); ++i)
        radeon_write(bi, static_cast<RadeonReg::Id>(RADEON_OV0_LIN_TRANS_A + i * sizeof(ULONG)), conversion[i]);
    /* Unity slope and continuous offsets across the piecewise gamma curve. */
    static const struct
    {
        RadeonReg::Id reg;
        UWORD offset;
    } gamma[] = {{RADEON_OV0_GAMMA_000_00F, 0},    {RADEON_OV0_GAMMA_010_01F, 32},   {RADEON_OV0_GAMMA_020_03F, 64},
                 {RADEON_OV0_GAMMA_040_07F, 128},  {RADEON_OV0_GAMMA_080_0BF, 256},  {RADEON_OV0_GAMMA_0C0_0FF, 256},
                 {RADEON_OV0_GAMMA_100_13F, 512},  {RADEON_OV0_GAMMA_140_17F, 512},  {RADEON_OV0_GAMMA_180_1BF, 768},
                 {RADEON_OV0_GAMMA_1C0_1FF, 768},  {RADEON_OV0_GAMMA_200_23F, 1024}, {RADEON_OV0_GAMMA_240_27F, 1024},
                 {RADEON_OV0_GAMMA_280_2BF, 1280}, {RADEON_OV0_GAMMA_2C0_2FF, 1280}, {RADEON_OV0_GAMMA_300_33F, 1536},
                 {RADEON_OV0_GAMMA_340_37F, 1536}, {RADEON_OV0_GAMMA_380_3BF, 1792}, {RADEON_OV0_GAMMA_3C0_3FF, 1792}};
    for (ULONG i = 0; i < sizeof(gamma) / sizeof(gamma[0]); ++i)
        radeon_write(bi, gamma[i].reg, RADEON_OVERLAY_GAMMA_UNITY | gamma[i].offset);
}
