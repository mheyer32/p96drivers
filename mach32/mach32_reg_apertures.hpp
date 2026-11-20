#ifndef MACH32_REG_APERTURES_HPP
#define MACH32_REG_APERTURES_HPP

#include "mach32_regs.hpp"
#include "reg_access.hpp"

#ifndef REGISTER_OFFSET
#error REGISTER_OFFSET required (mach32config.h)
#endif

/* Register I/O is always LE on Mach32. */
using Mach32Io       = AbsRegAperture<IoReg::Id, RegEndian::Swap, REGISTER_OFFSET, RegLog::Verbose>;
using Mach32IoQ      = AbsRegAperture<IoReg::Id, RegEndian::Swap, REGISTER_OFFSET, RegLog::Quiet>;
using Mach32IoNoSwap = AbsRegAperture<IoReg::Id, RegEndian::NoSwap, REGISTER_OFFSET, RegLog::Verbose>;
/* Forced NoSwap quiet — EXT_FIFO_STATUS raw poll (waitFifo) / PIX_TRANS. */
using Mach32IoNoSwapQ = AbsRegAperture<IoReg::Id, RegEndian::NoSwap, REGISTER_OFFSET, RegLog::Quiet>;

#endif /* MACH32_REG_APERTURES_HPP */
