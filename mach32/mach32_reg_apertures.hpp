#ifndef MACH32_REG_APERTURES_HPP
#define MACH32_REG_APERTURES_HPP

#include "mach32_regs.hpp"
#include "reg_access.hpp"

#if BIGENDIAN_IO
#define MACH32_IO_ENDIAN RegEndian::NoSwap
#else
#define MACH32_IO_ENDIAN RegEndian::Swap
#endif

#ifndef REGISTER_OFFSET
#error REGISTER_OFFSET required (mach32config.h)
#endif

using Mach32Io       = AbsRegAperture<IoReg::Id, MACH32_IO_ENDIAN, REGISTER_OFFSET, RegLog::Verbose>;
using Mach32IoQ      = AbsRegAperture<IoReg::Id, MACH32_IO_ENDIAN, REGISTER_OFFSET, RegLog::Quiet>;
using Mach32IoNoSwap = AbsRegAperture<IoReg::Id, RegEndian::NoSwap, REGISTER_OFFSET, RegLog::Verbose>;
/* Forced NoSwap quiet — EXT_FIFO_STATUS raw poll (waitFifo) / PIX_TRANS. */
using Mach32IoNoSwapQ = AbsRegAperture<IoReg::Id, RegEndian::NoSwap, REGISTER_OFFSET, RegLog::Quiet>;

#endif /* MACH32_REG_APERTURES_HPP */
