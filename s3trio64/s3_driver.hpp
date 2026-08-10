#ifndef S3_DRIVER_HPP
#define S3_DRIVER_HPP

#include "s3_reg_apertures.hpp"
#include "p96_driver.hpp"

#include <assert.h>

/*
 * Non-virtual S3 view of BoardInfo. waitFifo / waitFor* use typed apertures.
 * BoardInfo hooks remain free-function ASM trampolines in chip_s3trio64.cpp
 * (full XxxDriver::method migration hits Amiga g++ ICE on this TU).
 */
class S3Driver : public P96Driver
{
   public:
	ChipData_t *chip() { return getChipData(this); }
	const ChipData_t *chip() const { return getConstChipData(this); }

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

	VgaIo legacyVga() { return VgaIo(getCardData(this)->legacyIOBase); }
	VgaIo legacyVga() const { return VgaIo(getConstCardData(this)->legacyIOBase); }

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
};

static_assert(sizeof(S3Driver) == sizeof(BoardInfo), "S3Driver must not grow BoardInfo");
static_assert(std::is_standard_layout<S3Driver>::value, "S3Driver must be standard layout");

static INLINE S3Driver *asS3(BoardInfo *bi) { return static_cast<S3Driver *>(bi); }
static INLINE const S3Driver *asS3(const BoardInfo *bi) { return static_cast<const S3Driver *>(bi); }

static INLINE void waitFifo(BoardInfo *bi, BYTE numSlots)
{
	asS3(bi)->waitFifo(numSlots);
}

#define S3_IO_ID(x)   static_cast<IoReg::Id>(x)
#define S3_MMIO_ID(x) static_cast<MmioReg::Id>(x)

#define DRIVER_LOCALS(self_)                \
	S3Driver *drv = asS3(self_);        \
	VgaIo vga     = drv->vga();         \
	S3Io io       = drv->io();          \
	S3Mmio mmio   = drv->mmio()

#endif
