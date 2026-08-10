#ifndef S3_DRIVER_HPP
#define S3_DRIVER_HPP

#include "p96_driver.hpp"
#include "s3_reg_apertures.hpp"

#include <assert.h>

class S3Driver : public P96Driver
{
   public:
    ChipData_t *chip() { return getChipData(this); }
    const ChipData_t *chip() const { return getConstChipData(this); }
    CardData_t *card() { return getCardData(this); }
    const CardData_t *card() const { return getConstCardData(this); }

    S3Io io() { return S3Io(ioBase()); }
    S3Io io() const { return S3Io(ioBase()); }
    S3IoQ ioQ() { return S3IoQ(ioBase()); }
    S3IoQ ioQ() const { return S3IoQ(ioBase()); }
    VgaIo vga() { return VgaIo(ioBase()); }
    VgaIo vga() const { return VgaIo(ioBase()); }
    VgaIoQ vgaQ() { return VgaIoQ(ioBase()); }
    VgaIoQ vgaQ() const { return VgaIoQ(ioBase()); }
    S3Mmio mmio() { return S3Mmio(mmioBase()); }
    S3Mmio mmio() const { return S3Mmio(mmioBase()); }
    S3MmioQ mmioQ() { return S3MmioQ(mmioBase()); }
    S3MmioQ mmioQ() const { return S3MmioQ(mmioBase()); }

    VgaIo legacyVga() { return VgaIo(card()->legacyIOBase); }
    VgaIo legacyVga() const { return VgaIo(getConstCardData(this)->legacyIOBase); }

#if defined(CONFIG_CYBERVISION64)
    INLINE Cv64Cached cv64()
    {
        CardData_t *cd = card();
        return Cv64Cached(Cv64Io(cd->cv64CtrlReg), Cv64Reg::CTRL, &cd->cv64Ctrl);
    }
#endif

    /* MULTI_FUNC_CNTL (0xBEE8): index in [15:12], data in [11:0]; MMIO write. */
    INLINE void writeBee8(UWORD idx, UWORD value)
    {
        mmio().writeW(MmioReg::MULTI_FUNC_CNTL, (UWORD)((idx << 12) | (value & 0x0FFF)));
    }

    /* Read path is I/O-only on older series; index encoding differs from write. */
    INLINE UWORD readBee8(UBYTE idx)
    {
        switch (idx) {
        case 0xA:
            idx = 0b0101;
            break;
        case 0xD:
            idx = 0b1010;
            break;
        case 0xE:
            idx = 0b0110;
            break;
        }
        S3Io port = io();
        port.writeW(IoReg::MULTI_FUNC_CNTL, (UWORD)((0xF << 12) | idx));
        return port.readW(IoReg::MULTI_FUNC_CNTL) & 0xFFF;
    }

    INLINE void waitFifo(BYTE numSlots)
    {
#if defined(CONFIG_VISION864) || defined(CONFIG_S3TRIO3264) || defined(CONFIG_CYBERVISION64)
        if (!numSlots)
            return;
        BYTE testBit = (BYTE)((7 - (numSlots - 1)) & 0xF);
#if BUILD_VISION864
        S3IoQ port = ioQ();
        while (port.readW(static_cast<IoReg::Id>(0x9AE8)) & (1 << testBit)) {
        }
#else
        S3MmioQ port = mmioQ();
        while (port.readW(static_cast<MmioReg::Id>(0x9AE8)) & (1 << testBit)) {
        }
#endif
#else
        (void)numSlots;
#endif
    }

    INLINE void waitForBlitter()
    {
#if BUILD_VISION864
        S3IoQ port = ioQ();
        while (port.readW(static_cast<IoReg::Id>(0x9AE8)) & BIT(9)) {
        }
#else
        S3MmioQ port = mmioQ();
        while (port.readW(static_cast<MmioReg::Id>(0x9AE8)) & BIT(9)) {
        }
#endif
    }

    INLINE void waitForIdle()
    {
#if BUILD_VISION864
        S3IoQ port = ioQ();
        while ((port.readW(static_cast<IoReg::Id>(0x9AE8)) & (BIT(9) | BIT(10))) != BIT(10)) {
        }
#else
        S3MmioQ port = mmioQ();
        while ((port.readW(static_cast<MmioReg::Id>(0x9AE8)) & (BIT(9) | BIT(10))) != BIT(10)) {
        }
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
    void ASM setClock();
    void ASM setMemoryMode(__REGD7(RGBFTYPE_REG format));
    BOOL ASM getVSyncState(__REGD0(BOOL expected));
    BOOL ASM setInterrupt(__REGD0(BOOL state));
    ULONG interruptServer();
    void ASM waitVerticalSync(__REGD0(BOOL waitForEnd));
    void ASM setSplitPosition(__REGD0(SHORT splitPos));
    void ASM setSpritePosition(__REGD0(WORD xpos), __REGD1(WORD ypos), __REGD7(RGBFTYPE_REG fmt));
    void ASM setSpriteImage(__REGD7(RGBFTYPE_REG fmt));
    void ASM setSpriteColor(__REGD0(UBYTE index), __REGD1(UBYTE red), __REGD2(UBYTE green), __REGD3(UBYTE blue),
                            __REGD7(RGBFTYPE_REG fmt));
    BOOL ASM setSprite(__REGD0(BOOL activate), __REGD7(RGBFTYPE_REG RGBFormat));
    void ASM setWriteMask(__REGD0(UBYTE mask));
    void ASM setClearMask(__REGD0(UBYTE mask));
    void ASM setReadPlane(__REGD0(UBYTE mask));
    void ASM fillRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                      __REGD3(WORD height), __REGD4(ULONG pen), __REGD5(UBYTE mask), __REGD7(RGBFTYPE_REG fmt));
    void ASM invertRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                        __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt));
    void ASM blitRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD srcX), __REGD1(WORD srcY), __REGD2(WORD dstX),
                      __REGD3(WORD dstY), __REGD4(WORD width), __REGD5(WORD height), __REGD6(UBYTE mask),
                      __REGD7(RGBFTYPE_REG fmt));
    void ASM blitRectNoMaskComplete(__REGA1(struct RenderInfo *sri), __REGA2(struct RenderInfo *dri),
                                    __REGD0(WORD srcX), __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY),
                                    __REGD4(WORD width), __REGD5(WORD height), __REGD6(UBYTE minTerm),
                                    __REGD7(RGBFTYPE_REG format));
    void ASM blitTemplate(__REGA1(struct RenderInfo *ri), __REGA2(struct Template *tmpl), __REGD0(WORD x),
                          __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                          __REGD7(RGBFTYPE_REG fmt));
    void ASM blitPattern(__REGA1(struct RenderInfo *ri), __REGA2(struct Pattern *pattern), __REGD0(WORD x),
                         __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                         __REGD7(RGBFTYPE_REG fmt));
    void ASM blitPlanar2Chunky(__REGA1(struct BitMap *bm), __REGA2(struct RenderInfo *ri), __REGD0(SHORT srcX),
                               __REGD1(SHORT srcY), __REGD2(SHORT dstX), __REGD3(SHORT dstY), __REGD4(SHORT width),
                               __REGD5(SHORT height), __REGD6(UBYTE minTerm), __REGD7(UBYTE mask));
    void ASM drawLine(__REGA1(struct RenderInfo *ri), __REGA2(struct Line *line), __REGD0(UBYTE mask),
                      __REGD7(RGBFTYPE_REG fmt));
    void ASM waitBlitter();

    /* internal helpers */
    ULONG getMemoryOffset(APTR memory);
    BOOL setGEFormat(UWORD bytesPerRow, UBYTE bpp);
    void setMix(UWORD frgdMix, UWORD bkgdMix);
    void setForegroundColor32(ULONG fgPen);
    void setBackgroundColor32(ULONG bgPen);
    void setForegroundColor(UWORD fgPen);
    void setBackgroundColor(UWORD bgPen);
    void setDrawMode(ULONG FgPen, ULONG BgPen, UBYTE DrawMode, RGBFTYPE format);
    void setGEWriteMask(UBYTE mask, RGBFTYPE fmt, BYTE waitFifoSlots);
    void setBlitSrcPosAndSize(UWORD x, UWORD y, UWORD w, UWORD h);
    void setBlitDestPos(UWORD dstX, UWORD dstY);
    void writePIX_TRANS(ULONG value);
    void performBlitPlanar2ChunkyBlits(SHORT dstX, SHORT dstY, SHORT width, SHORT height, UWORD mixMode, UBYTE *bitmap,
                                       UWORD dwordsPerLine, WORD bmPitch, UBYTE rol);
    void setMemoryModeInternal(RGBFTYPE format);
};

static_assert(sizeof(S3Driver) == sizeof(BoardInfo), "S3Driver must not grow BoardInfo");
static_assert(std::is_standard_layout<S3Driver>::value, "S3Driver must be standard layout");

static INLINE S3Driver *asS3(BoardInfo *bi)
{
    return static_cast<S3Driver *>(bi);
}
static INLINE const S3Driver *asS3(const BoardInfo *bi)
{
    return static_cast<const S3Driver *>(bi);
}

static INLINE void waitFifo(BoardInfo *bi, BYTE numSlots)
{
    asS3(bi)->waitFifo(numSlots);
}

#define S3_IO_ID(x)   static_cast<IoReg::Id>(x)
#define S3_MMIO_ID(x) static_cast<MmioReg::Id>(x)

#define DRIVER_LOCALS(self_)     \
    S3Driver *drv = asS3(self_); \
    VgaIo vga     = drv->vga();  \
    S3Io io       = drv->io();   \
    S3Mmio mmio   = drv->mmio()

#endif
