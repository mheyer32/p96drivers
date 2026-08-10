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
	S3Mmio mmio() { return S3Mmio(mmioBase()); }
	S3Mmio mmio() const { return S3Mmio(mmioBase()); }
	S3MmioQ mmioQ() { return S3MmioQ(mmioBase()); }
	S3MmioQ mmioQ() const { return S3MmioQ(mmioBase()); }

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

#endif
