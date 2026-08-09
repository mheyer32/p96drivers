#ifndef MACH64_REG_APERTURES_HPP
#define MACH64_REG_APERTURES_HPP

#include "mach64_regs.hpp"
#include "reg_access.hpp"

/*
 * Aperture typedefs: endian from BIGENDIAN_MMIO / BIGENDIAN_IO (via SWAP* macros).
 * When BIGENDIAN_*=1, Swap* ops are already identity in common.h.
 */
#if BIGENDIAN_MMIO
#define MACH64_MMIO_ENDIAN RegEndian::NoSwap
#else
#define MACH64_MMIO_ENDIAN RegEndian::SwapMMIO
#endif

#if BIGENDIAN_IO
#define MACH64_IO_ENDIAN RegEndian::NoSwap
#else
#define MACH64_IO_ENDIAN RegEndian::SwapIO
#endif

using Mach64Mmio =
    AtiRegAperture<MmioReg::Id, MACH64_MMIO_ENDIAN, 0, RegLog::Verbose>;
using Mach64MmioQ =
    AtiRegAperture<MmioReg::Id, MACH64_MMIO_ENDIAN, 0, RegLog::Quiet>;
/* Forced NoSwap quiet — FIFO_STAT word poll. */
using Mach64MmioNoSwapQ =
    AtiRegAperture<MmioReg::Id, RegEndian::NoSwap, 0, RegLog::Quiet>;

using Mach64BlkIo =
    AtiRegAperture<BlkIoReg::Id, MACH64_IO_ENDIAN, 0, RegLog::Verbose>;
using Mach64BlkIoQ =
    AtiRegAperture<BlkIoReg::Id, MACH64_IO_ENDIAN, 0, RegLog::Quiet>;

/*
 * GX sparse I/O: byte offset = (SparseIoReg::Id << 10) from
 * legacyIOBase + ioSparseBase (same as old R/W_IO_L).
 */
template <RegEndian E, RegLog L>
struct AtiSparseIoAperture
{
	volatile UBYTE *base;

	explicit AtiSparseIoAperture(volatile UBYTE *b) : base(b) {}

	static INLINE LONG byteOff(SparseIoReg::Id id) { return (LONG)id << 10; }

	INLINE ULONG readL(SparseIoReg::Id id) const
	{
		return RegAperture<E, 0, L>(base).template readOff<ULONG>(byteOff(id)
#ifdef DBG
		                                                              ,
		                                                              "SparseIo"
#endif
		);
	}

	INLINE void writeMaskL(SparseIoReg::Id id, ULONG mask, ULONG val) const
	{
		RegAperture<E, 0, L>(base).template writeMaskOff<ULONG>(byteOff(id), mask, val
#ifdef DBG
		                                                        ,
		                                                        "SparseIo"
#endif
		);
	}
};

using Mach64SparseIo = AtiSparseIoAperture<MACH64_IO_ENDIAN, RegLog::Verbose>;

static INLINE Mach64Mmio mach64Mmio(volatile UBYTE *base)
{
	return Mach64Mmio(base);
}

static INLINE Mach64MmioQ mach64MmioQ(volatile UBYTE *base)
{
	return Mach64MmioQ(base);
}

static INLINE Mach64MmioNoSwapQ mach64MmioNoSwapQ(volatile UBYTE *base)
{
	return Mach64MmioNoSwapQ(base);
}

#endif /* MACH64_REG_APERTURES_HPP */
