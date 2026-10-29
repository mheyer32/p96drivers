#ifndef RADEON_CORE_H
#define RADEON_CORE_H

#include <stdint.h>

/* Keep the arithmetic core usable by both Amiga and host-side checks. */
enum RadeonFormat
{
#define RADEON_FORMAT(name, value, bytes) RADEON_FORMAT_##name = value,
#include "radeon_formats.h"
#undef RADEON_FORMAT
    RADEON_FORMAT_COUNT
};

constexpr auto RADEON_DEVICE_SENTINEL                 = UINT16_MAX;
constexpr auto RADEON_PIXEL_ALIGNMENT                 = 8u;
constexpr auto RADEON_PACKED_YUV_PIXEL_ALIGNMENT      = 2u;
constexpr auto RADEON_PLANAR_YUV_GROUP_PIXELS         = 2u;
constexpr auto RADEON_SWAPPED_PLANAR_YUV_GROUP_PIXELS = 4u;
constexpr auto RADEON_RGB_PITCH_ALIGNMENT             = 64u;
constexpr auto RADEON_PLANAR_YUV_PITCH_ALIGNMENT      = 128u;
constexpr auto RADEON_CLOCK_UNIT_HZ                   = 10000u;
constexpr auto RADEON_CLOCK_STEP_UNITS                = 20u;

/* Family values are private driver identifiers, not PCI device IDs. */
enum RadeonFamily
{
    RADEON_FAMILY_UNKNOWN = 0,
    RADEON_FAMILY_R100    = 1,
    RADEON_FAMILY_R200    = 2,
    RADEON_FAMILY_R250    = 3,
    RADEON_FAMILY_R300    = 4,
    RADEON_FAMILY_R350    = 5,
    RADEON_FAMILY_RV100   = 6,
    RADEON_FAMILY_RV200   = 7,
    RADEON_FAMILY_RV250   = 8,
    RADEON_FAMILY_RV280   = 9,
    RADEON_FAMILY_RV350   = 10,
    RADEON_FAMILY_RV370   = 11,
    RADEON_FAMILY_RV380   = 12
};

enum RadeonEngine
{
    RADEON_ENGINE_NONE = 0,
    RADEON_ENGINE_R100 = 1,
    RADEON_ENGINE_R200 = 2,
    RADEON_ENGINE_R300 = 3
};

struct RadeonDevice
{
    uint16_t device;
    const char *name;
    uint16_t family;
    uint16_t engine;
};

const struct RadeonDevice *radeon_find_device(uint16_t device);
uint32_t radeon_bytes_per_pixel(uint32_t format);
uint16_t radeon_calculate_bytes_per_row(uint16_t width, uint32_t format);
uint32_t radeon_get_pixel_clock(uint32_t base, uint32_t index);
uint32_t radeon_resolve_pixel_clock(uint32_t base, uint32_t requested, uint32_t *actual);

#endif
