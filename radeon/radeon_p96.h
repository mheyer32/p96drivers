#ifndef RADEON_P96_H
#define RADEON_P96_H

#include "radeon_regs.hpp"
#include <stddef.h>

constexpr auto RADEON_P96_EXTENDED         = (1UL << 29);
constexpr auto RADEON_DDC_PORT_COUNT       = 4u;
constexpr auto RADEON_P96_COMPOSITE_OFFSET = 474u;

struct RadeonARGBSprite
{
    UWORD width, height;
    ULONG reserved;
    ULONG pixels[];
};

/* P96's size-versioned extension has a different layout from the old
 * SDK's viewport tail. Never inspect it unless P96 sets its extension
 * flag, and never access a member beyond the supplied BoardInfo size.
 */
struct RadeonP96Extension
{
    ULONG size;
    struct RadeonARGBSprite *sprite;
    ULONG reserved[18];
    struct MinList i2cBuses;
};
constexpr auto RADEON_P96_EXTENSION_OFFSET = 0x0662u;
#define RADEON_P96_HAS(extension, member) \
    ((extension)->size >=                 \
     RADEON_P96_EXTENSION_OFFSET + offsetof(struct RadeonP96Extension, member) + sizeof((extension)->member))

static inline struct RadeonP96Extension *radeon_p96_extension(struct BoardInfo *bi)
{
    return (bi->Flags & RADEON_P96_EXTENDED) ? (struct RadeonP96Extension *)((UBYTE *)bi + RADEON_P96_EXTENSION_OFFSET)
                                             : NULL;
}

/* P96 calls I2C methods with the C stack ABI, unlike BoardInfo callbacks. */
struct RadeonI2CBus
{
    struct Node node;
    UWORD reserved;
    void __stdargs (*setSDA)(struct RadeonI2CBus *, BOOL);
    BOOL __stdargs (*getSDA)(struct RadeonI2CBus *);
    void __stdargs (*setSCL)(struct RadeonI2CBus *, BOOL);
    BOOL __stdargs (*getSCL)(struct RadeonI2CBus *);
    ULONG riseTime, fallTime;
    struct BoardInfo *board;
    RadeonReg::Id gpio;
};
void radeon_install_i2c(struct BoardInfo *);
void radeon_free_i2c(struct BoardInfo *);
#endif
