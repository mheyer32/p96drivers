#ifndef RADEON_DRIVER_HPP
#define RADEON_DRIVER_HPP

#include "p96_driver.hpp"
#include "radeon_common.h"

/* Method-only view of P96-owned BoardInfo records. State remains in CardData. */
class RadeonDriver : public P96Driver
{
   public:
    struct ChipData *chip() { return getChipData(this); }
    const struct ChipData *chip() const { return getConstChipData(this); }
    RadeonState *state() { return getCardData(this)->state; }
    const RadeonState *state() const { return getConstCardData(this)->state; }

    RadeonMmio mmio() const { return RadeonMmio(RegisterBase); }
    RadeonMmioQ mmioQ() const { return RadeonMmioQ(RegisterBase); }

    UWORD ASM calculateBytesPerRow(__REGD0(UWORD width), __REGD1(UWORD height), __REGA1(struct ModeInfo *mode),
                                   __REGD7(RGBFTYPE_REG formatValue));
    ULONG ASM getPixelClock(__REGA1(struct ModeInfo *mode), __REGD0(ULONG index), __REGD7(RGBFTYPE_REG formatValue));
    LONG ASM resolvePixelClock(__REGA1(struct ModeInfo *mode), __REGD0(ULONG requested),
                               __REGD7(RGBFTYPE_REG formatValue));
    BOOL ASM setSprite(__REGD0(BOOL enabled), __REGD7(RGBFTYPE_REG formatValue));
    void ASM setSpritePosition(__REGD0(WORD x), __REGD1(WORD y), __REGD7(RGBFTYPE_REG formatValue));
    void ASM setSpriteImage(__REGD7(RGBFTYPE_REG formatValue));
    void ASM setSpriteColor(__REGD0(UBYTE index), __REGD1(UBYTE red), __REGD2(UBYTE green), __REGD3(UBYTE blue),
                            __REGD7(RGBFTYPE_REG formatValue));
    BOOL ASM setSwitch(__REGD0(BOOL on));
    void ASM setColorArray(__REGD0(UWORD start), __REGD1(UWORD count));
    void ASM setDAC(__REGD0(UWORD border), __REGD7(RGBFTYPE_REG formatValue));
    void ASM setGC(__REGA1(struct ModeInfo *mode), __REGD0(BOOL border));
    void ASM setPanning(__REGA1(UBYTE *memory), __REGD0(UWORD width), __REGD3(UWORD height), __REGD1(WORD x),
                        __REGD2(WORD y), __REGD7(RGBFTYPE_REG formatValue));
    APTR ASM calculateMemory(__REGA1(APTR memory), __REGD0(struct RenderInfo *render),
                             __REGD7(RGBFTYPE_REG formatValue));
    ULONG ASM getCompatibleFormats(__REGD7(RGBFTYPE_REG formatValue));
    BOOL ASM setDisplay(__REGD0(BOOL on));
    void ASM setMemoryMode(__REGD7(RGBFTYPE_REG formatValue));
    void ASM setDPMSLevel(__REGD0(ULONG level));
    void ASM waitVerticalSync(__REGD0(BOOL toggle));
    BOOL ASM getVSyncState(__REGD0(BOOL toggle));
    void ASM setClock();
    BOOL ASM setInterrupt(__REGD0(BOOL enabled));
    ULONG ASM interruptServer();
    void ASM waitBlitter();
    void ASM fillRect(__REGA1(struct RenderInfo *render), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                      __REGD3(WORD height), __REGD4(ULONG pen), __REGD5(UBYTE mask), __REGD7(RGBFTYPE_REG formatValue));
    void ASM invertRect(__REGA1(struct RenderInfo *render), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                        __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG formatValue));
    void ASM blitRect(__REGA1(struct RenderInfo *render), __REGD0(WORD sx), __REGD1(WORD sy), __REGD2(WORD dx),
                      __REGD3(WORD dy), __REGD4(WORD width), __REGD5(WORD height), __REGD6(UBYTE mask),
                      __REGD7(RGBFTYPE_REG formatValue));
    void ASM blitRectNoMaskComplete(__REGA1(struct RenderInfo *source), __REGA2(struct RenderInfo *dest),
                                    __REGD0(WORD sx), __REGD1(WORD sy), __REGD2(WORD dx), __REGD3(WORD dy),
                                    __REGD4(WORD width), __REGD5(WORD height), __REGD6(UBYTE minterm),
                                    __REGD7(RGBFTYPE_REG formatValue));
    void ASM blitTemplate(__REGA1(struct RenderInfo *render), __REGA2(struct Template *tmpl), __REGD0(WORD x),
                          __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                          __REGD7(RGBFTYPE_REG formatValue));
    void ASM blitPattern(__REGA1(struct RenderInfo *render), __REGA2(struct Pattern *pattern), __REGD0(WORD x),
                         __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                         __REGD7(RGBFTYPE_REG formatValue));
    void ASM drawLine(__REGA1(struct RenderInfo *render), __REGA2(struct Line *line), __REGD0(UBYTE mask),
                      __REGD7(RGBFTYPE_REG formatValue));
    void ASM blitPlanar2Chunky(__REGA1(struct BitMap *source), __REGA2(struct RenderInfo *dest), __REGD0(SHORT sx),
                               __REGD1(SHORT sy), __REGD2(SHORT dx), __REGD3(SHORT dy), __REGD4(SHORT width),
                               __REGD5(SHORT height), __REGD6(UBYTE minterm), __REGD7(UBYTE mask));
    void ASM blitPlanar2Direct(__REGA1(struct BitMap *source), __REGA2(struct RenderInfo *dest),
                               __REGA3(struct ColorIndexMapping *colors), __REGD0(SHORT sx), __REGD1(SHORT sy),
                               __REGD2(SHORT dx), __REGD3(SHORT dy), __REGD4(SHORT width), __REGD5(SHORT height),
                               __REGD6(UBYTE minterm), __REGD7(UBYTE mask));
    APTR ASM createFeature(__REGD0(ULONG type), __REGA1(struct TagItem *tags));
    ULONG ASM setFeatureAttrs(__REGA1(APTR feature), __REGD0(ULONG type), __REGA2(struct TagItem *tags));
    ULONG ASM getFeatureAttrs(__REGA1(APTR feature), __REGD0(ULONG type), __REGA2(struct TagItem *tags));
    BOOL ASM deleteFeature(__REGA1(APTR feature), __REGD0(ULONG type));
    ULONG composite(BOOL r100, ULONG operation, RenderInfo *source, RenderInfo *destination,
                    struct BitMapExtra *sourceExtra, struct BitMapExtra *destinationExtra,
                    struct BitMapExtra *sourceAlphaExtra, struct BitMapExtra *destinationAlphaExtra,
                    const TagItem *tags);
};

static_assert(sizeof(RadeonDriver) == sizeof(BoardInfo), "RadeonDriver must not grow BoardInfo");
static_assert(__is_standard_layout(RadeonDriver), "RadeonDriver must be standard layout");

static INLINE RadeonDriver *asRadeon(BoardInfo *bi)
{
    return static_cast<RadeonDriver *>(bi);
}
static INLINE const RadeonDriver *asRadeon(const BoardInfo *bi)
{
    return static_cast<const RadeonDriver *>(bi);
}

/* P96 and resource clients share the queue. The guard releases the recursive
 * semaphore on every return path, including software fallbacks.
 */
class RadeonResourceLock
{
   public:
    explicit RadeonResourceLock(BoardInfo *board) : bi(board) { radeon_resource_lock(bi); }
    ~RadeonResourceLock() { radeon_resource_unlock(bi); }
    RadeonResourceLock(const RadeonResourceLock &)            = delete;
    RadeonResourceLock &operator=(const RadeonResourceLock &) = delete;

   private:
    BoardInfo *bi;
};

#endif
