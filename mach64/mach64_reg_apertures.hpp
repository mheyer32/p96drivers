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
/* Forced NoSwap quiet — FIFO_STAT word poll (R_MMIO_NOSWAP_W_QI). */
using Mach64MmioNoSwapQ =
    AtiRegAperture<MmioReg::Id, RegEndian::NoSwap, 0, RegLog::Quiet>;

using Mach64BlkIo =
    AtiRegAperture<BlkIoReg::Id, MACH64_IO_ENDIAN, 0, RegLog::Verbose>;
using Mach64BlkIoQ =
    AtiRegAperture<BlkIoReg::Id, MACH64_IO_ENDIAN, 0, RegLog::Quiet>;

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
