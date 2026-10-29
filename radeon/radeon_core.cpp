#include "radeon_core.h"
#include <stddef.h>

static const struct RadeonDevice devices[] = {
#include "radeon_devices.h"
};

const struct RadeonDevice *radeon_find_device(uint16_t device)
{
    const struct RadeonDevice *entry = devices;

    /* Comparing first preserves lookup of the sentinel record itself. */
    for (;;) {
        if (entry->device == device)
            return entry;
        if (entry->device == RADEON_DEVICE_SENTINEL)
            return NULL;
        ++entry;
    }
}

uint32_t radeon_bytes_per_pixel(uint32_t format)
{
    /* Planar, unsupported YUV411 and out-of-range formats have no size. */
    switch (format) {
#define RADEON_FORMAT(name, value, bytes) \
    case RADEON_FORMAT_##name:            \
        return bytes;
#include "radeon_formats.h"
#undef RADEON_FORMAT
    default:
        return 0;
    }
}

/* All callers supply a power-of-two alignment. Arithmetic remains unsigned
 * so that truncation to a P96 word preserves wrap at the end of the range.
 */
static uint32_t align_up(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

uint16_t radeon_calculate_bytes_per_row(uint16_t width, uint32_t format)
{
    uint32_t pixels = width;
    uint16_t pitch;

    if (format == RADEON_FORMAT_YUV422CGX) {
        pixels = (uint16_t)align_up(pixels, RADEON_PACKED_YUV_PIXEL_ALIGNMENT);
        return (uint16_t)(pixels * radeon_bytes_per_pixel(format));
    }

    if (format == RADEON_FORMAT_YUV422PA || format == RADEON_FORMAT_YUV422PAPC) {
        uint32_t group_pixels =
            format == RADEON_FORMAT_YUV422PA ? RADEON_PLANAR_YUV_GROUP_PIXELS : RADEON_SWAPPED_PLANAR_YUV_GROUP_PIXELS;

        /* Discard an incomplete group before rounding to eight groups.
         * Reversing these operations changes odd-width pitches.
         */
        pixels /= group_pixels;
        pixels = (uint16_t)(align_up(pixels, RADEON_PIXEL_ALIGNMENT) * group_pixels);
        pitch  = (uint16_t)(pixels * radeon_bytes_per_pixel(format));
        return (uint16_t)align_up(pitch, RADEON_PLANAR_YUV_PITCH_ALIGNMENT);
    }

    /* Round pixels first, then bytes. Each word-sized intermediate wraps
     * independently, including widths whose alignment exceeds UINT16_MAX.
     */
    pixels = (uint16_t)align_up(pixels, RADEON_PIXEL_ALIGNMENT);
    pitch  = (uint16_t)(pixels * radeon_bytes_per_pixel(format));
    return (uint16_t)align_up(pitch, RADEON_RGB_PITCH_ALIGNMENT);
}

uint32_t radeon_get_pixel_clock(uint32_t base, uint32_t index)
{
    return (base + index * RADEON_CLOCK_STEP_UNITS) * RADEON_CLOCK_UNIT_HZ;
}

uint32_t radeon_resolve_pixel_clock(uint32_t base, uint32_t requested, uint32_t *actual)
{
    /* The base and step are in 10 kHz units. Round down at both divisions.
     * Subtraction intentionally wraps for requests below the base clock;
     * clamping would change the driver's index and resulting clock.
     */
    uint32_t index = (requested / RADEON_CLOCK_UNIT_HZ - base) / RADEON_CLOCK_STEP_UNITS;
    *actual        = radeon_get_pixel_clock(base, index);
    return index;
}
