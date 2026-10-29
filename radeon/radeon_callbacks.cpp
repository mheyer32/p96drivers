#include "boardinfo.h"
#include "radeon_core.h"
#include "radeon_driver.hpp"
#include <stddef.h>

constexpr auto RADEON_CHIPDATA_CLOCK_BASE     = 4;
constexpr auto RADEON_BOARD_CLOCK_BASE_OFFSET = 0x59a;
constexpr auto RADEON_MODE_PIXEL_CLOCK_OFFSET = 0x2c;

/* Reject SDK layouts that would read or overwrite the wrong driver state. */
typedef char check_clock_base
    [offsetof(struct BoardInfo, ChipData[RADEON_CHIPDATA_CLOCK_BASE]) == RADEON_BOARD_CLOCK_BASE_OFFSET ? 1 : -1];
typedef char check_mode_clock[offsetof(struct ModeInfo, PixelClock) == RADEON_MODE_PIXEL_CLOCK_OFFSET ? 1 : -1];

/* Check every portable format identifier against the actual P96 SDK. */
#define RADEON_FORMAT(name, value, bytes) typedef char check_format_##name[RGBFB_##name == value ? 1 : -1];
#include "radeon_formats.h"
#undef RADEON_FORMAT

/* P96 supplies additional parameters to these callbacks, but pitch depends
 * only on width/format and the clock routines use a shared format-independent
 * clock range. Keep the full callback signatures for SDK type checking.
 */
UWORD ASM RadeonDriver::calculateBytesPerRow(__REGD0(UWORD width), __REGD1(UWORD height),
                                             __REGA1(struct ModeInfo *mode), __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi   = this;
    RGBFTYPE format = AS_RGBF(formatValue);
    (void)bi;
    (void)height;
    (void)mode;
    return radeon_calculate_bytes_per_row(width, format);
}

static UWORD ASM RadeonCalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                            __REGA1(struct ModeInfo *mode), __REGD7(RGBFTYPE_REG format))
{
    return asRadeon(bi)->calculateBytesPerRow(width, height, mode, format);
}

ULONG ASM RadeonDriver::getPixelClock(__REGA1(struct ModeInfo *mode), __REGD0(ULONG index),
                                      __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi   = this;
    RGBFTYPE format = AS_RGBF(formatValue);
    (void)mode;
    (void)format;
    return radeon_get_pixel_clock(bi->ChipData[RADEON_CHIPDATA_CLOCK_BASE], index);
}

static ULONG ASM RadeonGetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mode),
                                     __REGD0(ULONG index), __REGD7(RGBFTYPE_REG format))
{
    return asRadeon(bi)->getPixelClock(mode, index, format);
}

LONG ASM RadeonDriver::resolvePixelClock(__REGA1(struct ModeInfo *mode), __REGD0(ULONG requested),
                                         __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi   = this;
    RGBFTYPE format = AS_RGBF(formatValue);
    uint32_t actual;
    ULONG index;

    (void)format;
    index            = radeon_resolve_pixel_clock(bi->ChipData[RADEON_CHIPDATA_CLOCK_BASE], requested, &actual);
    mode->PixelClock = actual;
    return index;
}

static LONG ASM RadeonResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mode),
                                        __REGD0(ULONG requested), __REGD7(RGBFTYPE_REG format))
{
    return asRadeon(bi)->resolvePixelClock(mode, requested, format);
}

void radeon_install_clock_callbacks(struct BoardInfo *bi)
{
    P96_HOOK(bi->CalculateBytesPerRow, RadeonCalculateBytesPerRow);
    P96_HOOK(bi->GetPixelClock, RadeonGetPixelClock);
    P96_HOOK(bi->ResolvePixelClock, RadeonResolvePixelClock);
}
