#ifndef AT3D_DRIVER_HPP
#define AT3D_DRIVER_HPP

#include "at3d_reg_apertures.hpp"
#include "p96_driver.hpp"

#include <assert.h>

class At3dDriver : public P96Driver
{
   public:
	ChipData_t *chip() { return getChipData(this); }
	const ChipData_t *chip() const { return getConstChipData(this); }

	VgaIo vga() { return VgaIo(ioBase()); }
	VgaIo vga() const { return VgaIo(ioBase()); }
	VgaIoQ vgaQ() { return VgaIoQ(ioBase()); }
	VgaIoQ vgaQ() const { return VgaIoQ(ioBase()); }
	At3dMmio mmio() { return At3dMmio(mmioBase()); }
	At3dMmio mmio() const { return At3dMmio(mmioBase()); }
	At3dMmioQ mmioQ() { return At3dMmioQ(mmioBase()); }
	At3dMmioQ mmioQ() const { return At3dMmioQ(mmioBase()); }

	VgaIo legacyVga() { return VgaIo(getCardData(this)->legacyIOBase); }
	VgaIo legacyVga() const { return VgaIo(getConstCardData(this)->legacyIOBase); }

	INLINE ULONG waitFifo(UBYTE numSlots)
	{
		using namespace MmioReg;
		At3dMmioQ mmio = mmioQ();
		ULONG status;
		do {
			status = mmio.readL(EXT_DAC_STATUS);
		} while ((status & 0x0F) < numSlots);
		return status;
	}


	void ASM setGC(__REGA1(struct ModeInfo *mi), __REGD0(BOOL border));
	UWORD ASM calculateBytesPerRow(__REGD0(UWORD width), __REGD1(UWORD height), __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE_REG format));
	void ASM setColorArray(__REGD0(UWORD startIndex), __REGD1(UWORD count));
	void ASM setDAC(__REGD0(UWORD region), __REGD7(RGBFTYPE_REG format));
	void ASM setPanning(__REGA1(UBYTE *memory), __REGD0(UWORD width), __REGD3(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset), __REGD7(RGBFTYPE_REG format));
	APTR ASM calculateMemory(__REGA1(APTR memory), __REGD0(struct RenderInfo *ri), __REGD7(RGBFTYPE_REG format));
	ULONG ASM getCompatibleFormats(__REGD7(RGBFTYPE_REG format));
	BOOL ASM setDisplay(__REGD0(BOOL state));
	void ASM setDPMSLevel(__REGD0(ULONG level));
	LONG ASM resolvePixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG pixelClock), __REGD7(RGBFTYPE_REG RGBFormat));
	ULONG ASM getPixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG index), __REGD7(RGBFTYPE_REG format));
	void ASM setClock();
	void ASM setMemoryMode(__REGD7(RGBFTYPE_REG format));
	BOOL ASM getVSyncState(__REGD0(BOOL expected));
	ULONG ASM getVBeamPos();
	BOOL ASM setInterrupt(__REGD0(BOOL state));
	ULONG interruptServer();
	void ASM waitVerticalSync(__REGD0(BOOL end));
	void ASM setSplitPosition(__REGD0(SHORT splitPos));
	void ASM setSpritePosition(__REGD0(WORD xpos), __REGD1(WORD ypos), __REGD7(RGBFTYPE_REG fmt));
	void ASM setSpriteImage(__REGD7(RGBFTYPE_REG fmt));
	void ASM setSpriteColor(__REGD0(UBYTE idx), __REGD1(UBYTE r), __REGD2(UBYTE g), __REGD3(UBYTE b), __REGD7(RGBFTYPE_REG fmt));
	BOOL ASM setSprite(__REGD0(BOOL show), __REGD7(RGBFTYPE_REG fmt));
	void ASM setWriteMask(__REGD0(UBYTE mask));
	void ASM setClearMask(__REGD0(UBYTE mask));
	void ASM setReadPlane(__REGD0(UBYTE mask));
	void ASM fillRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(ULONG pen), __REGD5(UBYTE mask), __REGD7(RGBFTYPE_REG fmt));
	void ASM invertRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt));
	void ASM blitRectNoMaskComplete(__REGA1(struct RenderInfo *sri), __REGA2(struct RenderInfo *dri), __REGD0(WORD srcX), __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width), __REGD5(WORD height), __REGD6(UBYTE opCode), __REGD7(RGBFTYPE_REG format));
	void ASM blitRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD srcX), __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width), __REGD5(WORD height), __REGD6(UBYTE mask), __REGD7(RGBFTYPE_REG fmt));
	void ASM blitTemplate(__REGA1(struct RenderInfo *ri), __REGA2(struct Template *tmpl), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt));
	void ASM blitTemplate6422(__REGA1(struct RenderInfo *ri), __REGA2(struct Template *tmpl), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt));
	void ASM blitPlanar2Chunky(__REGA1(struct BitMap *bm), __REGA2(struct RenderInfo *ri), __REGD0(SHORT srcX), __REGD1(SHORT srcY), __REGD2(SHORT dstX), __REGD3(SHORT dstY), __REGD4(SHORT width), __REGD5(SHORT height), __REGD6(UBYTE minTerm), __REGD7(UBYTE mask));
	void ASM blitPattern(__REGA1(struct RenderInfo *ri), __REGA2(struct Pattern *pattern), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt));
	void ASM drawLine(__REGA1(struct RenderInfo *ri), __REGA2(struct Line *line), __REGD0(UBYTE mask), __REGD7(RGBFTYPE_REG fmt));
	void ASM waitBlitter();

	/* internal helpers */
	ULONG getMemoryOffset(APTR memory);
	ULONG getLinearPixelOffset(const struct RenderInfo *ri, UWORD x, UWORD y, UBYTE bppLog2);
	BOOL getStartCoordinates(const struct RenderInfo *ri, UBYTE bppLog2, UWORD *originX, UWORD *originY);
	ULONG getLocationRegisterValue(const struct RenderInfo *ri, UWORD x, UWORD y, UBYTE bppLog2, BOOL useLinearAddressing);
	BOOL setLocationRegister(const struct RenderInfo *ri, UWORD x, UWORD y, UBYTE bppLog2, BOOL useLinearAddressing, WORD reg);
	BOOL setDstLocation(const struct RenderInfo *ri, UWORD x, UWORD y, UBYTE bppLog2, BOOL useLinearAddressing);
	BOOL setSrcLocation(const struct RenderInfo *ri, UWORD x, UWORD y, UBYTE bppLog2, BOOL useLinearAddressing);
	void setDstPitch(UWORD bytesPerRow);
	void setDrawSize(UWORD width, UWORD height);
	void setFormat(RGBFTYPE fmt);
	void setForegroundPen(ULONG fgPen, RGBFTYPE fmt);
	void setBackgroundPen(ULONG bgPen, RGBFTYPE fmt);
	void setDrawCmd(ULONG drawCmd);
	volatile ULONG *getHostBltPort();
	void setDrawMode(UBYTE drawMode, ULONG fgPen, ULONG bgPen, RGBFTYPE fmt);
	void performPlanarPlaneBlit(UWORD width, UWORD height, UBYTE *bitmap, UWORD dwordsPerLine, WORD bmPitch, UBYTE rol, UBYTE planeIndex);
	void setMemoryModeInternal(RGBFTYPE format);

};

static_assert(sizeof(At3dDriver) == sizeof(BoardInfo), "At3dDriver must not grow BoardInfo");
static_assert(std::is_standard_layout<At3dDriver>::value, "At3dDriver must be standard layout");

static INLINE At3dDriver *asAt3d(BoardInfo *bi) { return static_cast<At3dDriver *>(bi); }
static INLINE const At3dDriver *asAt3d(const BoardInfo *bi) { return static_cast<const At3dDriver *>(bi); }

static INLINE ULONG waitFifo(const BoardInfo_t *bi, UBYTE numSlots)
{
	return asAt3d(const_cast<BoardInfo_t *>(bi))->waitFifo(numSlots);
}

#define AT3D_MMIO_ID(x) static_cast<MmioReg::Id>(x)

#define DRIVER_LOCALS(self_)                \
	At3dDriver *drv = asAt3d(self_);    \
	VgaIo vga       = drv->vga();       \
	At3dMmio mmio   = drv->mmio()

#endif
