#ifndef VGA_APERTURE_HPP
#define VGA_APERTURE_HPP

/*
 * Shared VGA I/O aperture typedefs. Include after chip *config.h so
 * REGISTER_OFFSET / BIGENDIAN_IO are defined (same as *_reg_apertures.hpp).
 */

#include "reg_access.hpp"

#ifndef REGISTER_OFFSET
#error REGISTER_OFFSET required for VgaIo
#endif

#if BIGENDIAN_IO
#define VGA_IO_ENDIAN RegEndian::NoSwap
#else
#define VGA_IO_ENDIAN RegEndian::SwapIO
#endif

using VgaIo  = VgaAperture<VGA_IO_ENDIAN, REGISTER_OFFSET, RegLog::Verbose>;
using VgaIoQ = VgaAperture<VGA_IO_ENDIAN, REGISTER_OFFSET, RegLog::Quiet>;

#define VGA_ID(x) static_cast<VgaReg::Id>(x)

#endif
