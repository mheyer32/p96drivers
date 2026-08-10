#ifndef S3_REG_APERTURES_HPP
#define S3_REG_APERTURES_HPP

#include "s3_regs.hpp"
#include "vga_aperture.hpp"

#if BIGENDIAN_IO
#define S3_IO_ENDIAN RegEndian::NoSwap
#else
#define S3_IO_ENDIAN RegEndian::SwapIO
#endif

#if BIGENDIAN_MMIO
#define S3_MMIO_ENDIAN RegEndian::NoSwap
#else
#define S3_MMIO_ENDIAN RegEndian::SwapMMIO
#endif

#ifndef REGISTER_OFFSET
#error REGISTER_OFFSET required
#endif
#ifndef MMIOREGISTER_OFFSET
#error MMIOREGISTER_OFFSET required
#endif

using S3Io    = AbsRegAperture<IoReg::Id, S3_IO_ENDIAN, REGISTER_OFFSET, RegLog::Verbose>;
using S3IoQ   = AbsRegAperture<IoReg::Id, S3_IO_ENDIAN, REGISTER_OFFSET, RegLog::Quiet>;
using S3Mmio  = AbsRegAperture<MmioReg::Id, S3_MMIO_ENDIAN, MMIOREGISTER_OFFSET, RegLog::Verbose>;
using S3MmioQ = AbsRegAperture<MmioReg::Id, S3_MMIO_ENDIAN, MMIOREGISTER_OFFSET, RegLog::Quiet>;

#if defined(CONFIG_CYBERVISION64)
/* Roxxler board control (MemBase+0x40001); write-only — use CachedReg + CardData::cv64Ctrl. */
namespace Cv64Reg {
enum Id : LONG
{
    CTRL = 0
};
#ifdef DBG
static INLINE const char *regName(Id)
{
    return "CV64_CTRL";
}
#endif
}  // namespace Cv64Reg
using Cv64Io     = AbsRegAperture<Cv64Reg::Id, RegEndian::NoSwap, REGISTER_OFFSET, RegLog::Verbose>;
using Cv64Cached = CachedReg<Cv64Io, Cv64Reg::Id, UBYTE>;
#endif

#endif
