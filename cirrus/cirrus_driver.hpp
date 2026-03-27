#ifndef CIRRUS_DRIVER_HPP
#define CIRRUS_DRIVER_HPP

#include "cirrus_reg_apertures.hpp"
#include "p96_driver.hpp"

#include <assert.h>

class CirrusDriver : public P96Driver
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
    CirrusIo io() { return CirrusIo(ioBase()); }
    CirrusIo io() const { return CirrusIo(ioBase()); }

    VgaIo legacyVga() { return VgaIo(card()->legacyIOBase); }
    VgaIo legacyVga() const { return VgaIo(getConstCardData(this)->legacyIOBase); }

    void ASM setGC(__REGA1(struct ModeInfo *mi), __REGD0(BOOL border));
    UWORD ASM calculateBytesPerRow(__REGD0(UWORD width), __REGD1(UWORD height), __REGA1(struct ModeInfo *mi),
                                   __REGD7(RGBFTYPE_REG format));
    void ASM setColorArray(__REGD0(UWORD startIndex), __REGD1(UWORD count));
    void ASM setDAC(__REGD0(UWORD region), __REGD7(RGBFTYPE_REG format));
    void ASM setPanning(__REGA1(UBYTE *memory), __REGD0(UWORD width), __REGD3(UWORD height), __REGD1(WORD xoffset),
                        __REGD2(WORD yoffset), __REGD7(RGBFTYPE_REG format));
    APTR ASM calculateMemory(__REGA1(APTR memory), __REGD0(struct RenderInfo *ri), __REGD7(RGBFTYPE_REG format));
    ULONG ASM getCompatibleFormats(__REGD7(RGBFTYPE_REG format));
    BOOL ASM setDisplay(__REGD0(BOOL state));
    LONG ASM resolvePixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG pixelClock),
                               __REGD7(RGBFTYPE_REG RGBFormat));
    ULONG ASM getPixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG index), __REGD7(RGBFTYPE_REG format));
    void ASM setClock();
    void ASM setMemoryMode(__REGD7(RGBFTYPE_REG format));
    BOOL ASM getVSyncState(__REGD0(BOOL expected));
    ULONG ASM getVBeamPos();
    void ASM waitVerticalSync(__REGD0(BOOL end));
    void ASM setWriteMask(__REGD0(UBYTE mask));
    void ASM setClearMask(__REGD0(UBYTE mask));
    void ASM setReadPlane(__REGD0(UBYTE mask));
    void ASM waitBlitter();

    ULONG getMemoryOffset(APTR memory) const;
    void writeHDR(UBYTE val);
    void programVclk(UBYTE nom, UBYTE denRaw, UBYTE divBit);
    void setSequencerPathWidth(RGBFTYPE_REG format);
};

static_assert(sizeof(CirrusDriver) == sizeof(BoardInfo), "CirrusDriver must not grow BoardInfo");
static_assert(std::is_standard_layout<CirrusDriver>::value, "CirrusDriver must be standard layout");

static INLINE CirrusDriver *asCirrus(BoardInfo *bi)
{
    return static_cast<CirrusDriver *>(bi);
}
static INLINE const CirrusDriver *asCirrus(const BoardInfo *bi)
{
    return static_cast<const CirrusDriver *>(bi);
}

#define DRIVER_LOCALS(self_)             \
    CirrusDriver *drv = asCirrus(self_); \
    VgaIo vga         = drv->vga()

#endif
