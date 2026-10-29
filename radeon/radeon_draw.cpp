#include "radeon_driver.hpp"

#include <graphics/rastport.h>

using namespace RadeonReg;

constexpr auto RADEON_ENGINE_FORMAT_CLUT    = 2u;
constexpr auto RADEON_ENGINE_FORMAT_RGB15   = 3u;
constexpr auto RADEON_ENGINE_FORMAT_RGB16   = 3u;
constexpr auto RADEON_ENGINE_FORMAT_RGB24   = 5u;
constexpr auto RADEON_ENGINE_FORMAT_RGB32   = 6u;
constexpr auto RADEON_PITCH_PIXELS_PER_UNIT = 8u;
constexpr auto RADEON_SETUP_FIFO_SLOTS      = 8u;
constexpr auto RADEON_INIT_FIFO_SLOTS       = 11u;
constexpr auto RADEON_LINE_SETUP_FIFO_SLOTS = 13u;
constexpr auto RADEON_FILL_FIFO_SLOTS       = 3u;
constexpr auto RADEON_INVERT_FIFO_SLOTS     = 4u;
constexpr auto RADEON_BLIT_FIFO_SLOTS       = 5u;
constexpr auto RADEON_MINTERM_SOURCE_MASK   = 0x0fu;
constexpr auto RADEON_MINTERM_SOURCE_SHIFT  = 4u;
constexpr auto RADEON_FIFO_DEPTH            = 64u;
constexpr auto RADEON_BITS_PER_BYTE         = 8u;
constexpr auto RADEON_PATTERN_BITS          = 16u;

BOOL radeon_wait_fifo(struct BoardInfo *bi, ULONG slots)
{
    for (ULONG poll = 0; poll < RADEON_POLL_LIMIT; ++poll)
        if ((radeon_read(bi, RADEON_RBBM_STATUS) & RADEON_FIFO_COUNT_MASK) >= slots)
            return TRUE;
    getCardData(bi)->state->engineFault = TRUE;
    return FALSE;
}

BOOL radeon_wait_idle(struct BoardInfo *bi)
{
    if (!radeon_wait_fifo(bi, RADEON_FIFO_DEPTH))
        return FALSE;
    for (ULONG poll = 0; poll < RADEON_POLL_LIMIT; ++poll) {
        if (radeon_read(bi, RADEON_RBBM_STATUS) & RADEON_ENGINE_ACTIVE)
            continue;
        radeon_update(bi, RADEON_RB2D_DSTCACHE_CTLSTAT, 0, RADEON_DSTCACHE_FLUSH_ALL);
        for (ULONG flush = 0; flush < RADEON_POLL_LIMIT; ++flush)
            if (!(radeon_read(bi, RADEON_RB2D_DSTCACHE_CTLSTAT) & RADEON_DSTCACHE_BUSY))
                return TRUE;
        break;
    }
    getCardData(bi)->state->engineFault = TRUE;
    return FALSE;
}

/* All accelerated drawing uses the same register stream. The command
 * processor serializes that stream with 3D operations when it is enabled.
 */
static void engine_write(struct BoardInfo *bi, RadeonReg::Id reg, ULONG value)
{
    if (getCardData(bi)->state->cp.active)
        (void)radeon_cp_register(bi, reg, value);
    else
        radeon_write(bi, reg, value);
}

void ASM RadeonDriver::waitBlitter()
{
    BoardInfo *bi = this;
    RadeonResourceLock lock(bi);
    (void)radeon_wait_idle(bi);
}

static void ASM RadeonWaitBlitter(__REGA0(struct BoardInfo *bi))
{
    asRadeon(bi)->waitBlitter();
}

static ULONG engine_format(RGBFTYPE format)
{
    switch (format) {
    case RGBFB_CLUT:
        return RADEON_ENGINE_FORMAT_CLUT;
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G5B5:
    case RGBFB_B5G5R5PC:
        return RADEON_ENGINE_FORMAT_RGB15;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G6B5:
    case RGBFB_B5G6R5PC:
        return RADEON_ENGINE_FORMAT_RGB16;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        return RADEON_ENGINE_FORMAT_RGB24;
    case RGBFB_A8R8G8B8:
    case RGBFB_A8B8G8R8:
    case RGBFB_R8G8B8A8:
    case RGBFB_B8G8R8A8:
        return RADEON_ENGINE_FORMAT_RGB32;
    default:
        return 0;
    }
}

static BOOL in_framebuffer(struct BoardInfo *bi, struct RenderInfo *render)
{
    ULONG address = (ULONG)render->Memory;
    ULONG start   = (ULONG)bi->MemorySpaceBase;
    return address >= start && address - start < bi->MemorySpaceSize;
}

static ULONG engine_pen(ULONG pen, RGBFTYPE format)
{
    switch (format) {
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
        return swapw(pen);
    case RGBFB_B8G8R8A8:
        return swapl(pen);
    default:
        return pen;
    }
}

static ULONG coordinates(WORD x, WORD y)
{
    return ((ULONG)(UWORD)x << RADEON_FIELD_HIGH_SHIFT) | (UWORD)y;
}

static ULONG pitch_offset(struct BoardInfo *bi, const struct RenderInfo *render)
{
    ULONG offset = (ULONG)render->Memory - (ULONG)bi->MemorySpaceBase;
    return (offset >> RADEON_OFFSET_BYTE_SHIFT) | ((ULONG)(UWORD)render->BytesPerRow >> RADEON_PITCH_BYTE_SHIFT)
                                                      << RADEON_PITCH_FIELD_SHIFT;
}

/* Pitch/offset registers address 1 KiB blocks. Account for the part of a
 * bitmap's allocation inside that block in its drawing coordinates.
 */
static void adjust_coordinates(struct BoardInfo *bi, const struct RenderInfo *render, WORD *x, WORD *y, RGBFTYPE format)
{
    ULONG remainder = ((ULONG)render->Memory - (ULONG)bi->MemorySpaceBase) & RADEON_OFFSET_BYTE_MASK;
    *x += (remainder % (UWORD)render->BytesPerRow) / radeon_bytes_per_pixel(format);
    *y += remainder / (UWORD)render->BytesPerRow;
}

static BOOL can_accelerate(struct BoardInfo *bi, struct RenderInfo *render, RGBFTYPE format)
{
    return engine_format(format) && in_framebuffer(bi, render) && render->BytesPerRow > 0 &&
           !(render->BytesPerRow & ((1u << RADEON_PITCH_BYTE_SHIFT) - 1)) && !(bi->Flags & BIF_NOBLITTER) &&
           !getCardData(bi)->state->engineFault;
}

static BOOL prepare_destination(struct BoardInfo *bi, struct RenderInfo *render, RGBFTYPE format, ULONG rop, BOOL copy,
                                UBYTE mask)
{
    if (!can_accelerate(bi, render, format) || !radeon_wait_fifo(bi, RADEON_SETUP_FIFO_SLOTS))
        return FALSE;
    engine_write(bi, RADEON_SRC_PITCH_OFFSET, pitch_offset(bi, render));
    engine_write(bi, RADEON_DST_PITCH_OFFSET, pitch_offset(bi, render));
    engine_write(bi, RADEON_DP_DATATYPE,
                 engine_format(format) | RADEON_DATATYPE_BYTE_LSB_TO_MSB | RADEON_DATATYPE_BRUSH_NONE |
                     RADEON_DATATYPE_SRC_COLOR);
    engine_write(bi, RADEON_DP_MIX, (rop << RADEON_GMC_ROP_SHIFT) | RADEON_MIX_SOURCE_MEMORY);
    engine_write(bi, RADEON_DP_WRITE_MASK, format == RGBFB_CLUT ? (~(ULONG)UINT8_MAX | mask) : ~0UL);
    engine_write(bi, RADEON_DP_CNTL, RADEON_DP_LEFT_TO_RIGHT | RADEON_DP_TOP_TO_BOTTOM);
    (void)copy;
    return TRUE;
}

/* Reuse the named trampoline signatures for software callbacks too. Some
 * Amiga g++ versions ignore register attributes on the SDK's anonymous
 * parameters, especially RGBFTYPE; RGBFTYPE_REG keeps the P96 D7 ABI.
 */
static void ASM RadeonFillRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *render), __REGD0(WORD x),
                               __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(ULONG pen),
                               __REGD5(UBYTE mask), __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->fillRect(render, x, y, width, height, pen, mask, format);
}

void ASM RadeonDriver::fillRect(__REGA1(struct RenderInfo *render), __REGD0(WORD x), __REGD1(WORD y),
                                __REGD2(WORD width), __REGD3(WORD height), __REGD4(ULONG pen), __REGD5(UBYTE mask),
                                __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi = this;
    RadeonResourceLock lock(bi);
    RGBFTYPE format = AS_RGBF(formatValue);
    if (width <= 0 || height <= 0)
        return;
    if (prepare_destination(bi, render, format, RADEON_ROP_PATCOPY, FALSE, mask) &&
        radeon_wait_fifo(bi, RADEON_FILL_FIFO_SLOTS)) {
        adjust_coordinates(bi, render, &x, &y, format);
        engine_write(bi, RADEON_DP_BRUSH_FRGD_CLR, engine_pen(pen, format));
        engine_write(bi, RADEON_DST_X_Y, coordinates(x, y));
        engine_write(bi, RADEON_DST_WIDTH_HEIGHT, coordinates(width, height));
    } else if (radeon_wait_idle(bi)) {
        reinterpret_cast<decltype(&RadeonFillRect)>(bi->FillRectDefault)(bi, render, x, y, width, height, pen, mask,
                                                                         format);
    }
}

static void ASM RadeonInvertRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *render), __REGD0(WORD x),
                                 __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                                 __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->invertRect(render, x, y, width, height, mask, format);
}

void ASM RadeonDriver::invertRect(__REGA1(struct RenderInfo *render), __REGD0(WORD x), __REGD1(WORD y),
                                  __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                                  __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi = this;
    RadeonResourceLock lock(bi);
    RGBFTYPE format = AS_RGBF(formatValue);
    if (width <= 0 || height <= 0)
        return;
    if (prepare_destination(bi, render, format, RADEON_ROP_DSTINVERT, FALSE, mask) &&
        radeon_wait_fifo(bi, RADEON_INVERT_FIFO_SLOTS)) {
        engine_write(bi, RADEON_DP_DATATYPE, engine_format(format) | RADEON_DATATYPE_BRUSH_SOLID);
        engine_write(bi, RADEON_DP_MIX, RADEON_ROP_DSTINVERT << RADEON_GMC_ROP_SHIFT);
        adjust_coordinates(bi, render, &x, &y, format);
        engine_write(bi, RADEON_DST_X_Y, coordinates(x, y));
        engine_write(bi, RADEON_DST_WIDTH_HEIGHT, coordinates(width, height));
    } else if (radeon_wait_idle(bi)) {
        reinterpret_cast<decltype(&RadeonInvertRect)>(bi->InvertRectDefault)(bi, render, x, y, width, height, mask,
                                                                             format);
    }
}

static BOOL copy_rect(struct BoardInfo *bi, struct RenderInfo *source, struct RenderInfo *dest, WORD sx, WORD sy,
                      WORD dx, WORD dy, WORD width, WORD height, ULONG rop, UBYTE mask, RGBFTYPE format)
{
    if (!can_accelerate(bi, source, format) || !prepare_destination(bi, dest, format, rop, TRUE, mask) ||
        !radeon_wait_fifo(bi, RADEON_BLIT_FIFO_SLOTS))
        return FALSE;
    ULONG direction = 0;
    if (sx >= dx)
        direction |= RADEON_DP_LEFT_TO_RIGHT;
    else {
        sx += width - 1;
        dx += width - 1;
    }
    if (sy >= dy)
        direction |= RADEON_DP_TOP_TO_BOTTOM;
    else {
        sy += height - 1;
        dy += height - 1;
    }
    adjust_coordinates(bi, source, &sx, &sy, format);
    adjust_coordinates(bi, dest, &dx, &dy, format);
    engine_write(bi, RADEON_SRC_PITCH_OFFSET, pitch_offset(bi, source));
    engine_write(bi, RADEON_DP_CNTL, direction);
    engine_write(bi, RADEON_SRC_Y_X, coordinates(sy, sx));
    engine_write(bi, RADEON_DST_Y_X, coordinates(dy, dx));
    engine_write(bi, RADEON_DST_WIDTH_HEIGHT, coordinates(width, height));
    return TRUE;
}

static void ASM RadeonBlitRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *render), __REGD0(WORD sx),
                               __REGD1(WORD sy), __REGD2(WORD dx), __REGD3(WORD dy), __REGD4(WORD width),
                               __REGD5(WORD height), __REGD6(UBYTE mask), __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->blitRect(render, sx, sy, dx, dy, width, height, mask, format);
}

void ASM RadeonDriver::blitRect(__REGA1(struct RenderInfo *render), __REGD0(WORD sx), __REGD1(WORD sy),
                                __REGD2(WORD dx), __REGD3(WORD dy), __REGD4(WORD width), __REGD5(WORD height),
                                __REGD6(UBYTE mask), __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi = this;
    RadeonResourceLock lock(bi);
    RGBFTYPE format = AS_RGBF(formatValue);
    if (width <= 0 || height <= 0)
        return;
    if (!copy_rect(bi, render, render, sx, sy, dx, dy, width, height, RADEON_ROP_SRCCOPY, mask, format) &&
        radeon_wait_idle(bi))
        reinterpret_cast<decltype(&RadeonBlitRect)>(bi->BlitRectDefault)(bi, render, sx, sy, dx, dy, width, height,
                                                                         mask, format);
}

static void ASM RadeonBlitRectNoMaskComplete(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *source),
                                             __REGA2(struct RenderInfo *dest), __REGD0(WORD sx), __REGD1(WORD sy),
                                             __REGD2(WORD dx), __REGD3(WORD dy), __REGD4(WORD width),
                                             __REGD5(WORD height), __REGD6(UBYTE minterm), __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->blitRectNoMaskComplete(source, dest, sx, sy, dx, dy, width, height, minterm, format);
}

void ASM RadeonDriver::blitRectNoMaskComplete(__REGA1(struct RenderInfo *source), __REGA2(struct RenderInfo *dest),
                                              __REGD0(WORD sx), __REGD1(WORD sy), __REGD2(WORD dx), __REGD3(WORD dy),
                                              __REGD4(WORD width), __REGD5(WORD height), __REGD6(UBYTE minterm),
                                              __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi = this;
    RadeonResourceLock lock(bi);
    RGBFTYPE format = AS_RGBF(formatValue);
    if (width <= 0 || height <= 0)
        return;
    ULONG truth = minterm & RADEON_MINTERM_SOURCE_MASK;
    ULONG rop   = truth | (truth << RADEON_MINTERM_SOURCE_SHIFT);
    if (!copy_rect(bi, source, dest, sx, sy, dx, dy, width, height, rop, UINT8_MAX, format) && radeon_wait_idle(bi))
        reinterpret_cast<decltype(&RadeonBlitRectNoMaskComplete)>(bi->BlitRectNoMaskCompleteDefault)(
            bi, source, dest, sx, sy, dx, dy, width, height, minterm, format);
}

/* Each row is padded to a complete host-data word. Repacking also avoids
 * reading beyond an odd-sized tmpl and handles arbitrary bit offsets.
 */
static void ASM RadeonBlitTemplate(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *render),
                                   __REGA2(struct Template *tmpl), __REGD0(WORD x), __REGD1(WORD y),
                                   __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                                   __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->blitTemplate(render, tmpl, x, y, width, height, mask, format);
}

void ASM RadeonDriver::blitTemplate(__REGA1(struct RenderInfo *render), __REGA2(struct Template *tmpl), __REGD0(WORD x),
                                    __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                                    __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi = this;
    RadeonResourceLock lock(bi);
    RGBFTYPE format = AS_RGBF(formatValue);
    if (width <= 0 || height <= 0)
        return;
    if (!can_accelerate(bi, render, format) || tmpl->BytesPerRow <= 0 ||
        (ULONG)tmpl->XOffset + width > (ULONG)tmpl->BytesPerRow * RADEON_BITS_PER_BYTE) {
        if (radeon_wait_idle(bi))
            reinterpret_cast<decltype(&RadeonBlitTemplate)>(bi->BlitTemplateDefault)(bi, render, tmpl, x, y, width,
                                                                                     height, mask, format);
        return;
    }
    if (!prepare_destination(bi, render, format, RADEON_ROP_SRCCOPY, FALSE, mask) || !radeon_wait_fifo(bi, 7))
        return;
    adjust_coordinates(bi, render, &x, &y, format);
    ULONG datatype = engine_format(format) | RADEON_DATATYPE_BRUSH_NONE;
    if (!(tmpl->DrawMode & JAM2))
        datatype |= RADEON_DATATYPE_SRC_MONO_FG;
    ULONG rop = tmpl->DrawMode & COMPLEMENT ? RADEON_ROP_SRCXOR : RADEON_ROP_SRCCOPY;
    engine_write(bi, RADEON_DP_DATATYPE, datatype);
    engine_write(bi, RADEON_DP_MIX, (rop << RADEON_GMC_ROP_SHIFT) | RADEON_MIX_SOURCE_HOST);
    engine_write(bi, RADEON_DP_SRC_FRGD_CLR, engine_pen(tmpl->FgPen, format));
    engine_write(bi, RADEON_DP_SRC_BKGD_CLR, engine_pen(tmpl->BgPen, format));
    engine_write(bi, RADEON_SC_TOP_LEFT, coordinates(y, x));
    engine_write(bi, RADEON_SC_BOTTOM_RIGHT, coordinates(RADEON_SCISSOR_LIMIT, x + width));
    engine_write(bi, RADEON_SRC_SC_BOTTOM_RIGHT, coordinates(RADEON_SCISSOR_LIMIT, width));
    const UBYTE *source = static_cast<const UBYTE *>(tmpl->Memory);
    for (ULONG row = 0; row < (UWORD)height; ++row) {
        ULONG words = ((UWORD)width + RADEON_HOST_WORD_BITS - 1) / RADEON_HOST_WORD_BITS;
        if (!radeon_wait_fifo(bi, 2))
            return;
        engine_write(bi, RADEON_DST_X_Y, coordinates(x, y + row));
        engine_write(bi, RADEON_DST_WIDTH_HEIGHT, coordinates(words * RADEON_HOST_WORD_BITS, 1));
        for (ULONG wordIndex = 0; wordIndex < words; ++wordIndex) {
            ULONG packed = 0;
            for (ULONG bit = 0; bit < RADEON_HOST_WORD_BITS; ++bit) {
                ULONG column = wordIndex * RADEON_HOST_WORD_BITS + bit;
                if (column >= (UWORD)width)
                    break;
                ULONG sourceBit = tmpl->XOffset + column;
                if (source[sourceBit / RADEON_BITS_PER_BYTE] &
                    (1u << (RADEON_BITS_PER_BYTE - 1 - sourceBit % RADEON_BITS_PER_BYTE)))
                    packed |= 1UL << (RADEON_HOST_WORD_BITS - 1 - bit);
            }
            if (tmpl->DrawMode & INVERSVID)
                packed = ~packed;
            if (!radeon_wait_fifo(bi, 1))
                return;
            engine_write(bi, RADEON_HOST_DATA0, swapl(packed));
        }
        source += tmpl->BytesPerRow;
    }
    if (!radeon_wait_fifo(bi, 3))
        return;
    engine_write(bi, RADEON_SC_TOP_LEFT, 0);
    engine_write(bi, RADEON_SC_BOTTOM_RIGHT, RADEON_SCISSOR_MAX);
    engine_write(bi, RADEON_SRC_SC_BOTTOM_RIGHT, RADEON_SCISSOR_MAX);
}

/* P96 owns the generic pattern and planar rasterizers. Synchronize
 * before entering them so CPU writes cannot race queued GPU operations.
 */
static void ASM RadeonBlitPattern(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *render),
                                  __REGA2(struct Pattern *pattern), __REGD0(WORD x), __REGD1(WORD y),
                                  __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                                  __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->blitPattern(render, pattern, x, y, width, height, mask, format);
}

void ASM RadeonDriver::blitPattern(__REGA1(struct RenderInfo *render), __REGA2(struct Pattern *pattern),
                                   __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height),
                                   __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi = this;
    RadeonResourceLock lock(bi);
    RGBFTYPE format = AS_RGBF(formatValue);
    if (radeon_wait_idle(bi))
        reinterpret_cast<decltype(&RadeonBlitPattern)>(bi->BlitPatternDefault)(bi, render, pattern, x, y, width, height,
                                                                               mask, format);
}

static void ASM RadeonDrawLine(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *render),
                               __REGA2(struct Line *line), __REGD0(UBYTE mask), __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->drawLine(render, line, mask, format);
}

void ASM RadeonDriver::drawLine(__REGA1(struct RenderInfo *render), __REGA2(struct Line *line), __REGD0(UBYTE mask),
                                __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi = this;
    RadeonResourceLock lock(bi);
    RGBFTYPE format = AS_RGBF(formatValue);
    WORD x = line->X, y = line->Y;
    WORD endX = x + line->dX, endY = y + line->dY;
    /* P96's clipped/complement rasterizer handles cases for which the line
     * engine cannot preserve the supplied Bresenham error term.
     */
    if ((line->DrawMode & COMPLEMENT) || x < 0 || y < 0 || endX < 0 || endY < 0 ||
        !can_accelerate(bi, render, format)) {
        if (radeon_wait_idle(bi))
            reinterpret_cast<decltype(&RadeonDrawLine)>(bi->DrawLineDefault)(bi, render, line, mask, format);
        return;
    }
    WORD left   = max(0, line->dX < 0 ? x - line->Length : x);
    WORD right  = (line->dX < 0 ? x : x + line->Length) + 1;
    WORD top    = max(0, line->dY < 0 ? y - line->Length : y);
    WORD bottom = (line->dY < 0 ? y : y + line->Length) + 1;
    adjust_coordinates(bi, render, &left, &top, format);
    adjust_coordinates(bi, render, &right, &bottom, format);
    adjust_coordinates(bi, render, &x, &y, format);
    adjust_coordinates(bi, render, &endX, &endY, format);
    if (!radeon_wait_fifo(bi, RADEON_LINE_SETUP_FIFO_SLOTS))
        return;
    engine_write(bi, RADEON_SRC_PITCH_OFFSET, pitch_offset(bi, render));
    engine_write(bi, RADEON_DST_PITCH_OFFSET, pitch_offset(bi, render));
    engine_write(bi, RADEON_SC_TOP_LEFT, coordinates(top, left));
    engine_write(bi, RADEON_SC_BOTTOM_RIGHT, coordinates(bottom, right));
    engine_write(bi, RADEON_DP_MIX, RADEON_ROP_PATCOPY << RADEON_GMC_ROP_SHIFT);
    engine_write(bi, RADEON_DP_BRUSH_FRGD_CLR, engine_pen(line->FgPen, format));
    engine_write(bi, RADEON_DP_BRUSH_BKGD_CLR, engine_pen(line->BgPen, format));
    engine_write(bi, RADEON_DP_WRITE_MASK, format == RGBFB_CLUT ? (~(ULONG)UINT8_MAX | mask) : ~0UL);
    engine_write(bi, RADEON_DP_DATATYPE,
                 engine_format(format) |
                     (line->DrawMode & JAM2 ? RADEON_DATATYPE_BRUSH_LINE_BG : RADEON_DATATYPE_BRUSH_LINE_FG));
    engine_write(bi, RADEON_DP_CNTL_XDIR_YDIR,
                 (line->dX >= 0 ? RADEON_LINE_LEFT_TO_RIGHT : 0) | (line->dY >= 0 ? RADEON_LINE_TOP_TO_BOTTOM : 0));
    UWORD pattern = line->LinePtrn;
    if (pattern && pattern != UINT16_MAX) {
        ULONG phase = (line->PatternShift + 1) % RADEON_PATTERN_BITS;
        pattern     = (pattern >> phase) | (pattern << (RADEON_PATTERN_BITS - phase));
        if (getCardData(bi)->state->device->family <= RADEON_FAMILY_R200) {
            UWORD reversed = 0;
            for (ULONG bit = 0; bit < RADEON_PATTERN_BITS; ++bit)
                reversed |= ((pattern >> bit) & 1u) << (RADEON_PATTERN_BITS - 1 - bit);
            pattern = reversed;
        }
    }
    ULONG brush = pattern | ((ULONG)pattern << RADEON_PATTERN_BITS);
    if (line->DrawMode & INVERSVID)
        brush = ~brush;
    engine_write(bi, RADEON_BRUSH_DATA0, brush);
    engine_write(bi, RADEON_DST_LINE_START, coordinates(y, x));
    engine_write(bi, RADEON_DST_LINE_END, coordinates(endY, endX));
    /* The endpoint register excludes the final pixel. Draw that pixel with
     * the next pattern bit, including a background pixel in JAM2 mode.
     */
    BOOL lastForeground = (brush >> ((line->Length + 1) % RADEON_HOST_WORD_BITS)) & 1u;
    if (lastForeground || (line->DrawMode & JAM2)) {
        if (!radeon_wait_fifo(bi, 3))
            return;
        engine_write(bi, RADEON_BRUSH_DATA0, lastForeground ? ~0UL : 0);
        engine_write(bi, RADEON_DST_LINE_START, coordinates(endY, endX));
        engine_write(bi, RADEON_DST_LINE_END, coordinates(endY, endX + 1));
    }
    if (!radeon_wait_fifo(bi, 2))
        return;
    engine_write(bi, RADEON_SC_TOP_LEFT, 0);
    engine_write(bi, RADEON_SC_BOTTOM_RIGHT, RADEON_SCISSOR_MAX);
}

static void ASM RadeonBlitPlanar2Chunky(__REGA0(struct BoardInfo *bi), __REGA1(struct BitMap *source),
                                        __REGA2(struct RenderInfo *dest), __REGD0(SHORT sx), __REGD1(SHORT sy),
                                        __REGD2(SHORT dx), __REGD3(SHORT dy), __REGD4(SHORT width),
                                        __REGD5(SHORT height), __REGD6(UBYTE minterm), __REGD7(UBYTE mask))
{
    asRadeon(bi)->blitPlanar2Chunky(source, dest, sx, sy, dx, dy, width, height, minterm, mask);
}

void ASM RadeonDriver::blitPlanar2Chunky(__REGA1(struct BitMap *source), __REGA2(struct RenderInfo *dest),
                                         __REGD0(SHORT sx), __REGD1(SHORT sy), __REGD2(SHORT dx), __REGD3(SHORT dy),
                                         __REGD4(SHORT width), __REGD5(SHORT height), __REGD6(UBYTE minterm),
                                         __REGD7(UBYTE mask))
{
    BoardInfo *bi = this;
    RadeonResourceLock lock(bi);
    if (radeon_wait_idle(bi))
        reinterpret_cast<decltype(&RadeonBlitPlanar2Chunky)>(bi->BlitPlanar2ChunkyDefault)(
            bi, source, dest, sx, sy, dx, dy, width, height, minterm, mask);
}

static void ASM RadeonBlitPlanar2Direct(__REGA0(struct BoardInfo *bi), __REGA1(struct BitMap *source),
                                        __REGA2(struct RenderInfo *dest), __REGA3(struct ColorIndexMapping *colors),
                                        __REGD0(SHORT sx), __REGD1(SHORT sy), __REGD2(SHORT dx), __REGD3(SHORT dy),
                                        __REGD4(SHORT width), __REGD5(SHORT height), __REGD6(UBYTE minterm),
                                        __REGD7(UBYTE mask))
{
    asRadeon(bi)->blitPlanar2Direct(source, dest, colors, sx, sy, dx, dy, width, height, minterm, mask);
}

void ASM RadeonDriver::blitPlanar2Direct(__REGA1(struct BitMap *source), __REGA2(struct RenderInfo *dest),
                                         __REGA3(struct ColorIndexMapping *colors), __REGD0(SHORT sx),
                                         __REGD1(SHORT sy), __REGD2(SHORT dx), __REGD3(SHORT dy), __REGD4(SHORT width),
                                         __REGD5(SHORT height), __REGD6(UBYTE minterm), __REGD7(UBYTE mask))
{
    BoardInfo *bi = this;
    RadeonResourceLock lock(bi);
    if (radeon_wait_idle(bi))
        reinterpret_cast<decltype(&RadeonBlitPlanar2Direct)>(bi->BlitPlanar2DirectDefault)(
            bi, source, dest, colors, sx, sy, dx, dy, width, height, minterm, mask);
}

BOOL radeon_init_engine(struct BoardInfo *bi)
{
    radeon_update(bi, RADEON_RB2D_DSTCACHE_CTLSTAT, 0, RADEON_DSTCACHE_FLUSH_ALL);
    ULONG clock = radeon_pll_read(bi, RADEON_MCLK_CNTL);
    ULONG reset = radeon_read(bi, RADEON_DISP_MISC_CNTL);
    radeon_pll_write(bi, RADEON_MCLK_CNTL, clock | RADEON_FORCE_ENGINE_CLOCKS);
    radeon_write(bi, RADEON_DISP_MISC_CNTL, reset | RADEON_SOFT_RESET_GRPH_PP);
    (void)radeon_read(bi, RADEON_DISP_MISC_CNTL);
    radeon_write(bi, RADEON_DISP_MISC_CNTL, reset & ~RADEON_SOFT_RESET_GRPH_PP);
    (void)radeon_read(bi, RADEON_DISP_MISC_CNTL);
    radeon_pll_write(bi, RADEON_MCLK_CNTL, clock);
    radeon_write(bi, RADEON_DISP_MISC_CNTL, reset);
    if (!radeon_wait_fifo(bi, RADEON_INIT_FIFO_SLOTS))
        return FALSE;
    radeon_write(bi, RADEON_SC_TOP_LEFT, 0);
    radeon_write(bi, RADEON_SC_BOTTOM_RIGHT, RADEON_SCISSOR_MAX);
    radeon_write(bi, RADEON_SRC_SC_BOTTOM_RIGHT, RADEON_SCISSOR_MAX);
    radeon_write(bi, RADEON_DP_GUI_MASTER_CNTL,
                 RADEON_GMC_EXPLICIT_PITCH | RADEON_GMC_SOURCE_CLIP | RADEON_GMC_DESTINATION_CLIP |
                     RADEON_GMC_CLR_CMP_DISABLE | RADEON_GMC_WRITE_MASK_DISABLE | RADEON_GMC_BRUSH_NONE |
                     RADEON_GMC_SRC_COLOR | RADEON_GMC_SRC_MEMORY | (RADEON_ROP_SRCCOPY << RADEON_GMC_ROP_SHIFT));
    radeon_write(bi, RADEON_DST_LINE_START, 0);
    radeon_write(bi, RADEON_DST_LINE_END, 0);
    radeon_write(bi, RADEON_DP_BRUSH_FRGD_CLR, ~0UL);
    radeon_write(bi, RADEON_DP_BRUSH_BKGD_CLR, 0);
    radeon_write(bi, RADEON_DP_SRC_FRGD_CLR, ~0UL);
    radeon_write(bi, RADEON_DP_SRC_BKGD_CLR, 0);
    radeon_write(bi, RADEON_DP_WRITE_MASK, ~0UL);
    return radeon_wait_idle(bi);
}

void radeon_install_drawing(struct BoardInfo *bi)
{
    P96_HOOK(bi->WaitBlitter, RadeonWaitBlitter);
    P96_HOOK(bi->FillRect, RadeonFillRect);
    P96_HOOK(bi->InvertRect, RadeonInvertRect);
    P96_HOOK(bi->BlitRect, RadeonBlitRect);
    P96_HOOK(bi->BlitRectNoMaskComplete, RadeonBlitRectNoMaskComplete);
    P96_HOOK(bi->BlitTemplate, RadeonBlitTemplate);
    P96_HOOK(bi->BlitPattern, RadeonBlitPattern);
    P96_HOOK(bi->DrawLine, RadeonDrawLine);
    P96_HOOK(bi->BlitPlanar2Chunky, RadeonBlitPlanar2Chunky);
    P96_HOOK(bi->BlitPlanar2Direct, RadeonBlitPlanar2Direct);
    bi->Flags |= BIF_BLITTER;
}
