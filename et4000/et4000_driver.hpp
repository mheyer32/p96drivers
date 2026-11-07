#ifndef ET4000_DRIVER_HPP
#define ET4000_DRIVER_HPP

#include "p96_driver.hpp"
#include "vga_aperture.hpp"

#include <assert.h>

/* Tseng CRTCB / Sprite index-data ports (IOD strap; fixed on this board). */
#define CRTCB_IDX  0x217A
#define CRTCB_DATA 0x217B

class Et4000Driver : public P96Driver
{
   public:
    ChipData_t *chip() { return getChipData(this); }
    const ChipData_t *chip() const { return getConstChipData(this); }
    CardData_t *card() { return getCardData(this); }
    const CardData_t *card() const { return getConstCardData(this); }

    VgaIo vga() { return VgaIo(ioBase()); }
    VgaIo vga() const { return VgaIo(ioBase()); }
    VgaIoQ vgaQ() { return VgaIoQ(ioBase()); }
    VgaIoQ vgaQ() const { return VgaIoQ(ioBase()); }

    VgaIo legacyVga() { return VgaIo(card()->legacyIOBase); }
    VgaIo legacyVga() const { return VgaIo(getConstCardData(this)->legacyIOBase); }

    INLINE UBYTE readCRB(UBYTE idx)
    {
        VgaIo port = vga();
        port.writeB(VGA_ID(CRTCB_IDX), idx);
        UBYTE value = port.readB(VGA_ID(CRTCB_DATA));
        D(VERBOSE, "R CRB_%lX -> 0x%02lx\n", (LONG)idx, (LONG)value);
        return value;
    }

    INLINE void writeCRB(UBYTE idx, UBYTE value)
    {
        VgaIo port = vga();
        port.writeB(VGA_ID(CRTCB_IDX), idx);
        port.writeB(VGA_ID(CRTCB_DATA), value);
        D(VERBOSE, "W CRB_%lX <- 0x%02lx\n", (LONG)idx, (LONG)value);
    }

    INLINE void writeCRBMask(UBYTE idx, UBYTE mask, UBYTE value)
    {
        UBYTE regvalue = (readCRB(idx) & ~mask) | (value & mask);
        vga().writeB(VGA_ID(CRTCB_DATA), regvalue);
        D(VERBOSE, "W CRB_%lX <- 0x%02lx\n", (LONG)idx, (LONG)regvalue);
    }
};

static_assert(sizeof(Et4000Driver) == sizeof(BoardInfo), "Et4000Driver must not grow BoardInfo");
static_assert(std::is_standard_layout<Et4000Driver>::value, "Et4000Driver must be standard layout");

static INLINE Et4000Driver *asEt4000(BoardInfo *bi)
{
    return static_cast<Et4000Driver *>(bi);
}
static INLINE const Et4000Driver *asEt4000(const BoardInfo *bi)
{
    return static_cast<const Et4000Driver *>(bi);
}

#define DRIVER_LOCALS(self_)        \
    Et4000Driver *drv = asEt4000(self_); \
    VgaIo vga         = drv->vga()

#endif
