#include "radeon_driver.hpp"

using namespace RadeonReg;

constexpr auto RADEON_SPRITE_WORD_BITS   = 16u;
constexpr auto RADEON_SPRITE_LONG_BITS   = 32u;
constexpr auto RADEON_SPRITE_PLANES      = 2u;
constexpr auto RADEON_CURSOR_COLOR_COUNT = 3u;

BOOL ASM RadeonDriver::setSprite(__REGD0(BOOL enabled), __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi   = this;
    RGBFTYPE format = AS_RGBF(formatValue);
    (void)format;
    radeon_update(bi, RADEON_CUR_OFFSET, 0, RADEON_CURSOR_LOCK);
    if (enabled && bi->MouseWidth && bi->MouseHeight)
        radeon_update(bi, RADEON_CRTC_GEN_CNTL, 0, RADEON_CRTC_CURSOR_ENABLE | RADEON_CRTC_CURSOR_ARGB);
    else
        radeon_update(bi, RADEON_CRTC_GEN_CNTL, RADEON_CRTC_CURSOR_ENABLE, 0);
    radeon_update(bi, RADEON_CUR_OFFSET, RADEON_CURSOR_LOCK, 0);
    return TRUE;
}

static BOOL ASM RadeonSetSprite(__REGA0(struct BoardInfo *bi), __REGD0(BOOL enabled), __REGD7(RGBFTYPE_REG format))
{
    return asRadeon(bi)->setSprite(enabled, format);
}

void ASM RadeonDriver::setSpritePosition(__REGD0(WORD x), __REGD1(WORD y), __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi   = this;
    RGBFTYPE format = AS_RGBF(formatValue);
    (void)format;
    LONG xpos = x - bi->XOffset, ypos = y - bi->YOffset;
    xpos = max(1 - (LONG)bi->MouseWidth, xpos);
    ypos = max(1 - (LONG)bi->MouseHeight, ypos);
    if (bi->ModeInfo) {
        xpos = min(xpos, bi->ModeInfo->Width);
        ypos = min(ypos, bi->ModeInfo->Height);
        if (bi->ModeInfo->Flags & GMF_DOUBLESCAN)
            ypos *= 2;
    }
    ULONG skipRows = ypos < 0 ? -ypos : 0;
    ULONG offset   = getCardData(bi)->state->cursorOffset + skipRows * RADEON_CURSOR_WIDTH * RADEON_CURSOR_PIXEL_BYTES;
    radeon_write(bi, RADEON_CUR_OFFSET, offset | RADEON_CURSOR_LOCK);
    ULONG hotspotX = bi->MouseXOffset + (xpos < 0 ? -xpos : 0);
    ULONG hotspotY = bi->MouseYOffset + skipRows;
    radeon_write(bi, RADEON_CUR_HOTSPOT, (hotspotX << RADEON_FIELD_HIGH_SHIFT) | hotspotY | RADEON_CURSOR_LOCK);
    radeon_write(bi, RADEON_CUR_POSITION, ((ULONG)max(xpos, 0) << RADEON_FIELD_HIGH_SHIFT) | max(ypos, 0));
}

static void ASM RadeonSetSpritePosition(__REGA0(struct BoardInfo *bi), __REGD0(WORD x), __REGD1(WORD y),
                                        __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->setSpritePosition(x, y, format);
}

void ASM RadeonDriver::setSpriteImage(__REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi                        = this;
    RGBFTYPE format                      = AS_RGBF(formatValue);
    const struct RadeonARGBSprite *argb  = NULL;
    struct RadeonP96Extension *extension = radeon_p96_extension(bi);
    if (extension && RADEON_P96_HAS(extension, sprite))
        argb = extension->sprite;
    ULONG scale      = !argb && (bi->Flags & BIF_BIGSPRITE) ? 2u : 1u;
    ULONG width      = minu(argb ? argb->width : bi->MouseWidth, RADEON_CURSOR_WIDTH / scale);
    ULONG height     = minu(argb ? argb->height : bi->MouseHeight, RADEON_CURSOR_HEIGHT / scale);
    ULONG doubleScan = bi->ModeInfo && (bi->ModeInfo->Flags & GMF_DOUBLESCAN) ? 2u : 1u;
    ULONG lineScale  = scale;
    bi->MouseXOffset = RADEON_CURSOR_WIDTH - width * scale;
    bi->MouseYOffset = max(0, (LONG)RADEON_CURSOR_HEIGHT - (LONG)height * (scale + doubleScan - 1));
    RadeonSetSprite(bi, width && height, format);
    ULONG *destination = (ULONG *)((UBYTE *)bi->MemorySpaceBase + getCardData(bi)->state->cursorOffset);
    radeon_update(bi, RADEON_CUR_OFFSET, 0, RADEON_CURSOR_LOCK);
    /* Surface seven fixes the cursor's byte order independently of the
     * visible bitmap's format. The unused area is transparent ARGB.
     */
    for (ULONG pixel = 0; pixel < RADEON_CURSOR_WIDTH * RADEON_CURSOR_HEIGHT; ++pixel)
        destination[pixel] = 0;
    if (argb) {
        for (ULONG row = 0; row < height; ++row)
            for (ULONG col = 0; col < width; ++col)
                destination[row * RADEON_CURSOR_WIDTH + bi->MouseXOffset + col] = argb->pixels[row * width + col];
    } else if (bi->MouseImage) {
        BOOL hires    = (bi->Flags & BIF_HIRESSPRITE) != 0;
        ULONG rowBits = hires ? RADEON_SPRITE_LONG_BITS : RADEON_SPRITE_WORD_BITS;
        width         = minu(width, rowBits);
        for (ULONG row = 0; row < height; ++row) {
            ULONG plane0, plane1;
            /* Amiga sprite data starts with a control pair, followed by
             * interleaved rows from the two bitplanes.
             */
            if (hires) {
                const ULONG *image = (const ULONG *)bi->MouseImage;
                plane0             = image[(row + 1u) * RADEON_SPRITE_PLANES];
                plane1             = image[(row + 1u) * RADEON_SPRITE_PLANES + 1u];
            } else {
                plane0 = bi->MouseImage[(row + 1u) * RADEON_SPRITE_PLANES];
                plane1 = bi->MouseImage[(row + 1u) * RADEON_SPRITE_PLANES + 1u];
            }
            for (ULONG col = 0; col < width; ++col) {
                ULONG bit   = width - col - 1u;
                ULONG index = ((plane0 >> bit) & 1u) | (((plane1 >> bit) & 1u) << 1);
                ULONG color = index ? getChipData(bi)->cursorColors[index - 1u] : 0;
                for (ULONG yy = 0; yy < lineScale; ++yy)
                    for (ULONG xx = 0; xx < scale; ++xx)
                        destination[(row * lineScale + yy) * RADEON_CURSOR_WIDTH + bi->MouseXOffset + col * scale +
                                    xx] = color;
            }
        }
    }
    radeon_write(bi, RADEON_CUR_HOTSPOT, ((ULONG)bi->MouseXOffset << RADEON_FIELD_HIGH_SHIFT) | bi->MouseYOffset);
}

static void ASM RadeonSetSpriteImage(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->setSpriteImage(format);
}

void ASM RadeonDriver::setSpriteColor(__REGD0(UBYTE index), __REGD1(UBYTE red), __REGD2(UBYTE green),
                                      __REGD3(UBYTE blue), __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi   = this;
    RGBFTYPE format = AS_RGBF(formatValue);
    (void)format;
    if (index >= RADEON_CURSOR_COLOR_COUNT)
        return;
    getChipData(bi)->cursorColors[index] = RADEON_COLOR_ALPHA_OPAQUE | ((ULONG)red << RADEON_COLOR_RED_SHIFT) |
                                           ((ULONG)green << RADEON_COLOR_GREEN_SHIFT) | blue;
}

static void ASM RadeonSetSpriteColor(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE index), __REGD1(UBYTE red),
                                     __REGD2(UBYTE green), __REGD3(UBYTE blue), __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->setSpriteColor(index, red, green, blue, format);
}

void radeon_install_cursor(struct BoardInfo *bi)
{
    P96_HOOK(bi->SetSprite, RadeonSetSprite);
    P96_HOOK(bi->SetSpritePosition, RadeonSetSpritePosition);
    P96_HOOK(bi->SetSpriteImage, RadeonSetSpriteImage);
    P96_HOOK(bi->SetSpriteColor, RadeonSetSpriteColor);
}
