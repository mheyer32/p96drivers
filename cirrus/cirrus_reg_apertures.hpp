#ifndef CIRRUS_REG_APERTURES_HPP
#define CIRRUS_REG_APERTURES_HPP

#include "cirrus_regs.hpp"
#include "vga_aperture.hpp"

#if BIGENDIAN_IO
#define CIRRUS_IO_ENDIAN RegEndian::NoSwap
#else
#define CIRRUS_IO_ENDIAN RegEndian::SwapIO
#endif

#ifndef REGISTER_OFFSET
#error REGISTER_OFFSET required
#endif

using CirrusIo  = AbsRegAperture<IoReg::Id, CIRRUS_IO_ENDIAN, REGISTER_OFFSET, RegLog::Verbose>;
using CirrusIoQ = AbsRegAperture<IoReg::Id, CIRRUS_IO_ENDIAN, REGISTER_OFFSET, RegLog::Quiet>;

#endif
