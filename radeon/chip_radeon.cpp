#include "radeon_driver.hpp"

#include "radeon_composite.h"

using namespace RadeonReg;

constexpr auto RADEON_MAX_HORIZONTAL_TIMING = 8184;
constexpr auto RADEON_MAX_VERTICAL_TIMING   = 4095;
constexpr auto RADEON_MAX_RESOLUTION        = 2048;
constexpr auto RADEON_DAC_BITS              = 8;
constexpr auto RADEON_VCO_BASE_DIVISOR      = 16u;
constexpr auto RADEON_MEMORY_PAGE_SHIFT     = 16;
constexpr auto RADEON_SUPPORTED_RGB_FORMATS =
    (RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_A8R8G8B8 | RGBFF_R5G6B5 | RGBFF_R5G5B5);

extern void radeon_install_clock_callbacks(struct BoardInfo *bi);

static BOOL rom_range(ULONG size, ULONG offset, ULONG count)
{
    return offset <= size && count <= size - offset;
}

static ULONG rom_word(const UBYTE *rom, ULONG offset)
{
    return (ULONG)rom[offset] | ((ULONG)rom[offset + 1] << 8);
}

static ULONG rom_long(const UBYTE *rom, ULONG offset)
{
    return rom_word(rom, offset) | (rom_word(rom, offset + 2) << 16);
}

static void init_clock_range(struct BoardInfo *bi, const UBYTE *rom, ULONG size)
{
    struct ChipData *chip  = getChipData(bi);
    chip->referenceClock   = RADEON_DEFAULT_REFERENCE_CLOCK;
    chip->referenceDivider = RADEON_DEFAULT_REFERENCE_DIV;
    chip->minPLL           = RADEON_DEFAULT_PLL_MINIMUM;
    chip->maxPLL           = RADEON_DEFAULT_PLL_MAXIMUM;
    chip->clockBase        = RADEON_DEFAULT_CLOCK_BASE;
    chip->blankDepth       = 0;
    if (!rom || !rom_range(size, 0, RADEON_ROM_HEADER_POINTER + sizeof(UWORD)) ||
        rom_word(rom, 0) != RADEON_ROM_SIGNATURE)
        return;
    ULONG header = rom_word(rom, RADEON_ROM_HEADER_POINTER);
    if (!rom_range(size, header, RADEON_ROM_PLL_POINTER + sizeof(UWORD)))
        return;
    ULONG pll = rom_word(rom, header + RADEON_ROM_PLL_POINTER);
    if (!rom_range(size, pll, RADEON_ROM_PLL_MAXIMUM + sizeof(ULONG)))
        return;
    ULONG reference = rom_word(rom, pll + RADEON_ROM_PLL_REFERENCE);
    ULONG divider   = rom_word(rom, pll + RADEON_ROM_PLL_REFERENCE_DIV);
    ULONG minimum   = rom_long(rom, pll + RADEON_ROM_PLL_MINIMUM);
    ULONG maximum   = rom_long(rom, pll + RADEON_ROM_PLL_MAXIMUM);
    if (!reference || !divider || !minimum || minimum > maximum)
        return;
    chip->referenceClock   = reference;
    chip->referenceDivider = divider;
    chip->minPLL           = minimum;
    chip->maxPLL           = maximum;
    chip->clockBase        = minimum / RADEON_VCO_BASE_DIVISOR;
}

BOOL radeon_init_chip(struct BoardInfo *bi, const UBYTE *rom, ULONG romSize)
{
    struct RadeonState *state = getCardData(bi)->state;
    if (!radeon_init_rom(bi, rom, romSize))
        return FALSE;
    ULONG size = bi->MemorySize ? bi->MemorySize : radeon_read(bi, RADEON_CONFIG_MEMSIZE);
    if (!size || size > state->framebufferSize)
        size = state->framebufferSize;
    if (size < RADEON_MIN_VRAM_SIZE)
        return FALSE;
    init_clock_range(bi, rom, romSize);

    /* Keep the cursor in the first surface so changing the framebuffer
     * byte-swap mode cannot change the cursor's ARGB layout.
     */
    state->cursorOffset = 0;
    bi->MemoryBase      = (UBYTE *)bi->MemorySpaceBase + RADEON_CURSOR_BYTES;
    bi->MemorySpaceSize = size;
    bi->MemorySize      = size - RADEON_CURSOR_BYTES;
    bi->MaxMemorySize   = bi->MemorySize;
    bi->MaxChunkSize    = bi->MemorySize;
    bi->BitsPerCannon   = RADEON_DAC_BITS;
    bi->RGBFormats      = RADEON_SUPPORTED_RGB_FORMATS;
    bi->Flags |= BIF_HARDWARESPRITE | BIF_CACHEMODECHANGE | BIF_DBLSCANDBLSPRITEY;
    for (ULONG mode = 0; mode < MAXMODES; ++mode) {
        bi->MaxHorValue[mode]      = mode ? RADEON_MAX_HORIZONTAL_TIMING : 0;
        bi->MaxVerValue[mode]      = mode ? RADEON_MAX_VERTICAL_TIMING : 0;
        bi->MaxHorResolution[mode] = mode ? RADEON_MAX_RESOLUTION : 0;
        bi->MaxVerResolution[mode] = mode ? RADEON_MAX_RESOLUTION : 0;
        bi->PixelClockCount[mode] =
            mode ? (getChipData(bi)->maxPLL - getChipData(bi)->clockBase) / RADEON_CLOCK_STEP_UNITS : 0;
    }
    radeon_install_clock_callbacks(bi);
    radeon_install_display(bi);
    radeon_install_cursor(bi);
    radeon_install_drawing(bi);
    radeon_init_display(bi);
    radeon_install_overlay(bi);
    radeon_install_i2c(bi);
    radeon_write(bi, RADEON_SURFACE7_LOWER_BOUND, 0);
    radeon_write(bi, RADEON_SURFACE7_UPPER_BOUND, RADEON_CURSOR_BYTES - 1);
    radeon_write(bi, RADEON_SURFACE7_INFO, RADEON_SURFACE_SWAP_32);
    radeon_write(bi, RADEON_CUR_OFFSET, state->cursorOffset);
    ULONG firstPage = state->framebufferPhysical >> RADEON_MEMORY_PAGE_SHIFT;
    ULONG lastPage  = (state->framebufferPhysical + size - 1) >> RADEON_MEMORY_PAGE_SHIFT;
    radeon_write(bi, RADEON_MC_FB_LOCATION, firstPage | (lastPage << RADEON_MEMORY_PAGE_SHIFT));
    radeon_write(bi, RADEON_DISPLAY_BASE_ADDR, state->framebufferPhysical);
    radeon_write(bi, RADEON_DISPLAY2_BASE_ADDR, state->framebufferPhysical);
    radeon_write(bi, RADEON_OV0_BASE_ADDR, state->framebufferPhysical);
    (void)radeon_cp_start(bi);
    radeon_install_composite(bi);
    return radeon_init_engine(bi);
}
