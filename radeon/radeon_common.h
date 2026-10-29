#ifndef RADEON_COMMON_H
#define RADEON_COMMON_H
#include "common.h"
#include "radeon_core.h"
#include "radeon_cp.h"
#include "radeon_overlay.h"
#include "radeon_p96.h"
#include "radeon_reg_apertures.hpp"
#include "radeon_regs.h"
#include "radeon_resource.h"

constexpr auto RADEON_VENDOR_ID               = 0x1002;
constexpr auto RADEON_MIN_MMIO_SIZE           = 0x4000UL;
constexpr auto RADEON_MIN_VRAM_SIZE           = (2UL * 1024 * 1024);
constexpr auto RADEON_ROM_SIGNATURE           = 0xaa55;
constexpr auto RADEON_ROM_HEADER_POINTER      = 0x48;
constexpr auto RADEON_ROM_PLL_POINTER         = 0x30;
constexpr auto RADEON_ROM_PLL_REFERENCE       = 0x0e;
constexpr auto RADEON_ROM_PLL_REFERENCE_DIV   = 0x10;
constexpr auto RADEON_ROM_PLL_MINIMUM         = 0x12;
constexpr auto RADEON_ROM_PLL_MAXIMUM         = 0x16;
constexpr auto RADEON_DEFAULT_REFERENCE_CLOCK = 2700UL;
constexpr auto RADEON_DEFAULT_REFERENCE_DIV   = 60UL;
constexpr auto RADEON_DEFAULT_PLL_MINIMUM     = 12000UL;
constexpr auto RADEON_DEFAULT_PLL_MAXIMUM     = 35000UL;
constexpr auto RADEON_DEFAULT_CLOCK_BASE      = 750UL;

struct RadeonWindow;
struct RadeonState
{
    const struct RadeonDevice *device;
    ULONG framebufferSize;
    ULONG framebufferPhysical;
    ULONG cursorOffset;
    ULONG savedCommand;
    ULONG savedCRTC;
    ULONG savedExtCRTC;
    ULONG savedSurface;
    BOOL engineFault;
    BOOL initialized;
    struct RadeonState *next;
    struct BoardInfo *boardInfo;
    struct RadeonWindow *windows;
    struct RadeonWindow *visibleWindow;
    struct RadeonI2CBus *i2cBuses;
    ULONG savedGPIO[RADEON_DDC_PORT_COUNT];
    struct RadeonCommandProcessor cp;
    struct RadeonResourceBoard *resourceBoard;
    UBYTE pciBus, pciSlot, pciFunction;
};

/* Clock fields retain the private layout expected by the clock callbacks. */
struct ChipData
{
    ULONG referenceClock, referenceDivider, minPLL, maxPLL;
    ULONG clockBase, blankDepth;
    ULONG cursorColors[4];
};

struct CardData
{
    BYTE *legacyIOBase;
    struct Library *OpenPciBase;
    struct pci_dev *board;
    struct Node boardNode;
    char boardName[16];
    struct RadeonState *state;
};
STATIC_ASSERT(sizeof(struct CardData) <= SIZEOF_MEMBER(BoardInfo_t, CardData), radeon_card_data_size);
STATIC_ASSERT(sizeof(struct ChipData) <= SIZEOF_MEMBER(BoardInfo_t, ChipData), radeon_chip_data_size);

static inline ULONG radeon_read(struct BoardInfo *bi, RadeonReg::Id reg)
{
    return RadeonMmioQ(bi->RegisterBase).readL(reg);
}
static inline void radeon_write(struct BoardInfo *bi, RadeonReg::Id reg, ULONG value)
{
    RadeonMmioQ(bi->RegisterBase).writeL(reg, value);
}
static inline void radeon_update(struct BoardInfo *bi, RadeonReg::Id reg, ULONG mask, ULONG value)
{
    radeon_write(bi, reg, (radeon_read(bi, reg) & ~mask) | value);
}
void radeon_init_display(struct BoardInfo *bi);
void radeon_install_display(struct BoardInfo *bi);
void radeon_install_drawing(struct BoardInfo *bi);
void radeon_install_cursor(struct BoardInfo *bi);
ULONG radeon_pll_read(struct BoardInfo *bi, ULONG index);
void radeon_pll_write(struct BoardInfo *bi, ULONG index, ULONG value);
BOOL radeon_init_rom(struct BoardInfo *bi, const UBYTE *rom, ULONG romSize);
BOOL radeon_init_chip(struct BoardInfo *bi, const UBYTE *rom, ULONG romSize);
BOOL radeon_init_engine(struct BoardInfo *bi);
BOOL radeon_wait_idle(struct BoardInfo *bi);
BOOL radeon_wait_fifo(struct BoardInfo *bi, ULONG slots);
BOOL releaseCard(__REGA0(struct BoardInfo *bi));
#endif
