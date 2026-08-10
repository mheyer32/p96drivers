#ifndef MACH32_DRIVER_HPP
#define MACH32_DRIVER_HPP

#include "mach32_reg_apertures.hpp"
#include "p96_driver.hpp"

#include <assert.h>

/*
 * Non-virtual Mach32 view of BoardInfo. P96 ABI entry points are thin
 * registerized trampolines that call methods (this ≡ a0).
 */
class Mach32Driver : public P96Driver
{
   public:
	ChipData_t *chip() { return getChipData(this); }
	const ChipData_t *chip() const { return getConstChipData(this); }

	Mach32Io io() { return Mach32Io(ioBase()); }
	Mach32Io io() const { return Mach32Io(ioBase()); }
	Mach32IoQ ioQ() { return Mach32IoQ(ioBase()); }
	Mach32IoQ ioQ() const { return Mach32IoQ(ioBase()); }
	Mach32IoNoSwapQ ioNoSwapQ() { return Mach32IoNoSwapQ(ioBase()); }
	Mach32IoNoSwapQ ioNoSwapQ() const { return Mach32IoNoSwapQ(ioBase()); }
	Mach32IoNoSwap ioNoSwap() { return Mach32IoNoSwap(ioBase()); }
	Mach32IoNoSwap ioNoSwap() const { return Mach32IoNoSwap(ioBase()); }

	/* MULTI_FUNC_CNTL (0xBEE8): index in [15:12], data in [11:0]. */
	INLINE void writeBee8(UWORD idx, UWORD value)
	{
		io().writeW(IoReg::MULTI_FUNC_CNTL, (UWORD)((idx << 12) | (value & 0x0FFF)));
	}

	/*
	 * EXT_GE_CONFIG is write @ 0x7AEE; readable image is R_EXT_GE_CONFIG @ 0x8EEE
	 * (same port as PATT_DATA write). Match legacy readModifyWrite NoSwap RMW.
	 */
	INLINE void writeExtGeConfigMask(UWORD mask, UWORD value)
	{
		Mach32IoNoSwap ns = ioNoSwap();
		UWORD raw         = ns.readWRaw(IoReg::R_EXT_GE_CONFIG);
		raw               = (raw & ~SWAPW_IO(mask)) | SWAPW_IO(value & mask);
		ns.writeW(IoReg::EXT_GE_CONFIG, raw);
	}

	static INLINE UWORD fifoStatConsume(UWORD stat, UBYTE entries)
	{
		return ((ULONG)(stat + 1) << entries) - 1;
	}

	INLINE void waitFifo(UBYTE slots)
	{
		using namespace IoReg;

		flushWrites();

		if (!slots)
			return;

		UWORD mask = 0xffffU << (16 - slots);

		ChipData_t *cd = chip();
		if (!(cd->fifoSlotsCached & mask)) {
			cd->fifoSlotsCached = fifoStatConsume(cd->fifoSlotsCached, slots);
			return;
		}

		Mach32IoNoSwapQ io = ioNoSwapQ();
		UWORD maskSwapped  = SWAPW_IO(mask);
		UWORD raw;
		do {
			raw = io.readWRaw(EXT_FIFO_STATUS);
		} while (raw & maskSwapped);

		cd->fifoSlotsCached = fifoStatConsume(SWAPW_IO(raw), slots);
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
	ULONG ASM getVBeamPos();
	BOOL ASM setInterrupt(__REGD0(BOOL state));
	ULONG interruptServer();
	void ASM waitVerticalSync(__REGD0(BOOL end));
	void ASM setSpritePosition(__REGD0(WORD xpos), __REGD1(WORD ypos), __REGD7(RGBFTYPE_REG fmt));
	void ASM setSpriteImage(__REGD7(RGBFTYPE_REG fmt));
	void ASM setSpriteColor(__REGD0(UBYTE idx), __REGD1(UBYTE r), __REGD2(UBYTE g), __REGD3(UBYTE b),
	                        __REGD7(RGBFTYPE_REG fmt));
	BOOL ASM setSprite(__REGD0(BOOL show), __REGD7(RGBFTYPE_REG fmt));
	void ASM setWriteMask(__REGD0(UBYTE mask));
	void ASM setClearMask(__REGD0(UBYTE mask));
	void ASM setReadPlane(__REGD0(UBYTE mask));
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
};

static_assert(sizeof(Mach32Driver) == sizeof(BoardInfo), "Mach32Driver must not grow BoardInfo");
static_assert(std::is_standard_layout<Mach32Driver>::value, "Mach32Driver must be standard layout");

static INLINE Mach32Driver *asMach32(BoardInfo *bi)
{
	return static_cast<Mach32Driver *>(bi);
}

static INLINE const Mach32Driver *asMach32(const BoardInfo *bi)
{
	return static_cast<const Mach32Driver *>(bi);
}

static INLINE void waitFifo(BoardInfo_t *bi, UBYTE slots)
{
	asMach32(bi)->waitFifo(slots);
}

/* Bind drv/io from BoardInfo* or Mach32Driver* (this). */
#define DRIVER_LOCALS(self_)                  \
	Mach32Driver *drv    = asMach32(self_);   \
	Mach32Io io          = drv->io();         \
	Mach32IoQ ioQ        = drv->ioQ();        \
	Mach32IoNoSwapQ ioNS = drv->ioNoSwapQ()

#endif /* MACH32_DRIVER_HPP */
