#ifndef MACH64_DRIVER_HPP
#define MACH64_DRIVER_HPP

#include "mach64_reg_apertures.hpp"
#include "p96_driver.hpp"

#include <assert.h>

/*
 * Non-virtual Mach64 view of BoardInfo. Chip-family differences use distinct
 * BoardInfo hook pointers (InitMach64GX/CT/VT/GT), not C++ virtual overrides.
 * P96 ABI entry points are thin registerized trampolines that call methods.
 *
 * Hook-facing methods use the same ASM/__REGxx layout as BoardInfo (this ≡ a0).
 * Format args in d7 are RGBFTYPE_REG (ULONG) — g++ drops __asm on enum types.
 *
 * Pass Mach64Driver* into I2C/RAMDAC helpers. Build with -ffunction-sections and
 * link with --gc-sections so out-of-line method bodies unused after trampoline
 * inlining can be dropped.
 */
class Mach64Driver : public P96Driver
{
   public:

    /* --- regular members --- */
    ChipData_t *chip() { return getChipData(this); }
    const ChipData_t *chip() const { return getConstChipData(this); }

    ChipSpecific_t *chipSpecific() { return chip()->chipSpecific; }
    const ChipSpecific_t *chipSpecific() const { return chip()->chipSpecific; }

    Mach64Mmio mmio() { return Mach64Mmio(mmioBase()); }
    Mach64Mmio mmio() const { return Mach64Mmio(mmioBase()); }
    Mach64MmioQ mmioQ() { return Mach64MmioQ(mmioBase()); }
    Mach64MmioQ mmioQ() const { return Mach64MmioQ(mmioBase()); }
    Mach64BlkIo blkIo() { return Mach64BlkIo(ioBase()); }
    Mach64BlkIo blkIo() const { return Mach64BlkIo(ioBase()); }
    Mach64BlkIoQ blkIoQ() { return Mach64BlkIoQ(ioBase()); }
    Mach64BlkIoQ blkIoQ() const { return Mach64BlkIoQ(ioBase()); }

    void writeOvrClr(UBYTE index8, UBYTE r, UBYTE g, UBYTE b);
    void setColorArrayInternal(UWORD startIndex, UWORD count, const struct CLUTEntry *colors);
    INLINE void setMemoryModeInternal(RGBFTYPE format);

    /* Mark `entries` FIFO slots used in a FIFO_STAT-shaped value (ones from LSB). */
    static INLINE UWORD fifoStatConsume(UWORD stat, UBYTE entries) { return ((ULONG)(stat + 1) << entries) - 1; }

    INLINE void waitFifo(UBYTE entries)
    {
#if MACH64_PCI_RETRY
        (void)entries;
#else
        if (!entries)
            return;

        /* FIFO_STAT: 0 = empty; ones pack from LSB. entries free ⇒ top entries bits clear. */
        UWORD mask = 0xffffU << (16 - entries);

        ChipData_t *cd = chip();
        if (!(cd->fifoSlotsCached & mask)) {
            cd->fifoSlotsCached = fifoStatConsume(cd->fifoSlotsCached, entries);
            return;
        }

        Mach64MmioQ mmio = mmioQ();
        do {
        } while (mmio.testW(MmioReg::FIFO_STAT, mask));

        cd->fifoSlotsCached = fifoStatConsume(mmio.readW(MmioReg::FIFO_STAT), entries);
#endif
    }

    /* --- P96 BoardInfo hooks (ASM/__REGxx; this ≡ a0) --- */
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
    void ASM setDPMSLevel(__REGD0(ULONG level));
    LONG ASM resolvePixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG pixelClock),
                               __REGD7(RGBFTYPE_REG RGBFormat));
    ULONG ASM getPixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG index), __REGD7(RGBFTYPE_REG format));
    void ASM setMemoryMode(__REGD7(RGBFTYPE_REG format));
    void ASM setWriteMask(__REGD0(UBYTE mask));
    void ASM setClearMask(__REGD0(UBYTE mask));
    void ASM setReadPlane(__REGD0(UBYTE mask));
    BOOL ASM getVSyncState(__REGD0(BOOL expected));
    void ASM waitVerticalSync(__REGD0(BOOL end));
    ULONG ASM getVBeamPos();
    BOOL ASM setInterrupt(__REGD0(BOOL state));
    ULONG interruptServer();
    void ASM setSpritePosition(__REGD0(WORD xpos), __REGD1(WORD ypos), __REGD7(RGBFTYPE_REG fmt));
    void ASM setSpriteImage(__REGD7(RGBFTYPE_REG fmt));
    void ASM setSpriteColor(__REGD0(UBYTE index), __REGD1(UBYTE red), __REGD2(UBYTE green), __REGD3(UBYTE blue),
                            __REGD7(RGBFTYPE_REG fmt));
    BOOL ASM setSprite(__REGD0(BOOL activate), __REGD7(RGBFTYPE_REG RGBFormat));
    void ASM fillRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                      __REGD3(WORD height), __REGD4(ULONG pen), __REGD5(UBYTE mask), __REGD7(RGBFTYPE_REG fmt));
    void ASM invertRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                        __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt));
    void ASM blitRectNoMaskComplete(__REGA1(struct RenderInfo *sri), __REGA2(struct RenderInfo *dri),
                                    __REGD0(WORD srcX), __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY),
                                    __REGD4(WORD width), __REGD5(WORD height), __REGD6(UBYTE opCode),
                                    __REGD7(RGBFTYPE_REG format));
    void ASM blitRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD srcX), __REGD1(WORD srcY), __REGD2(WORD dstX),
                      __REGD3(WORD dstY), __REGD4(WORD width), __REGD5(WORD height), __REGD6(UBYTE mask),
                      __REGD7(RGBFTYPE_REG fmt));
    void ASM blitTemplate(__REGA1(struct RenderInfo *ri), __REGA2(struct Template *tmpl), __REGD0(WORD x),
                          __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                          __REGD7(RGBFTYPE_REG fmt));
    void ASM blitPlanar2Chunky(__REGA1(struct BitMap *bm), __REGA2(struct RenderInfo *ri), __REGD0(SHORT srcX),
                               __REGD1(SHORT srcY), __REGD2(SHORT dstX), __REGD3(SHORT dstY), __REGD4(SHORT width),
                               __REGD5(SHORT height), __REGD6(UBYTE minTerm), __REGD7(UBYTE mask));
    void ASM blitPattern(__REGA1(struct RenderInfo *ri), __REGA2(struct Pattern *pattern), __REGD0(WORD x),
                         __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                         __REGD7(RGBFTYPE_REG fmt));
    void ASM drawLine(__REGA1(struct RenderInfo *ri), __REGA2(struct Line *line), __REGD0(UBYTE mask),
                      __REGD7(RGBFTYPE_REG fmt));
    void ASM waitBlitter();
    APTR ASM allocCardMem(__REGD0(ULONG size), __REGD1(BOOL force), __REGD2(BOOL system), __REGD3(ULONG bytesperrow),
                          __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE_REG format));

    void ASM setDAC_GX(__REGD0(UWORD region), __REGD7(RGBFTYPE_REG format));
    void ASM setDAC_RGB514(__REGD0(UWORD region), __REGD7(RGBFTYPE_REG format));
    void ASM setClock_GX();
    void ASM setClock_RGB514();
    void ASM setColorArray_RGB514(__REGD0(UWORD startIndex), __REGD1(UWORD count));
    void ASM setSpriteColor_GX(__REGD0(UBYTE index), __REGD1(UBYTE red), __REGD2(UBYTE green), __REGD3(UBYTE blue),
                               __REGD7(RGBFTYPE_REG fmt));
    void ASM setSpriteColor_RGB514(__REGD0(UBYTE index), __REGD1(UBYTE red), __REGD2(UBYTE green), __REGD3(UBYTE blue),
                                   __REGD7(RGBFTYPE_REG fmt));
    void ASM setSpriteImage_RGB514(__REGD7(RGBFTYPE_REG fmt));
    void ASM setSpritePosition_RGB514(__REGD0(WORD xpos), __REGD1(WORD ypos), __REGD7(RGBFTYPE_REG fmt));
    BOOL ASM setSprite_RGB514(__REGD0(BOOL activate), __REGD7(RGBFTYPE_REG RGBFormat));
    void ASM setClock_CT();
    void ASM setClock_VT();
    void ASM setClock_GT();
};

static_assert(sizeof(Mach64Driver) == sizeof(BoardInfo), "Mach64Driver must not grow BoardInfo");
static_assert(std::is_standard_layout<Mach64Driver>::value, "Mach64Driver must be standard layout");

static INLINE Mach64Driver *asMach64(BoardInfo *bi)
{
    return static_cast<Mach64Driver *>(bi);
}

static INLINE const Mach64Driver *asMach64(const BoardInfo *bi)
{
    return static_cast<const Mach64Driver *>(bi);
}

static INLINE Mach64Driver *asMach64(P96Driver *drv)
{
    return static_cast<Mach64Driver *>(drv);
}

static INLINE const Mach64Driver *asMach64(const P96Driver *drv)
{
    return static_cast<const Mach64Driver *>(drv);
}

/* Bind drv/mmio/cd from BoardInfo* or Mach64Driver* (this). */
#define DRIVER_LOCALS(self_)             \
    Mach64Driver *drv = asMach64(self_); \
    Mach64MmioQ mmio  = drv->mmioQ();    \
    ChipData_t *cd    = drv->chip()

static INLINE void waitFifo(BoardInfo_t *bi, UBYTE entries)
{
    asMach64(bi)->waitFifo(entries);
}

#endif /* MACH64_DRIVER_HPP */
