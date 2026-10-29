#ifndef RADEON_REG_APERTURES_HPP
#define RADEON_REG_APERTURES_HPP

#include "radeon_regs.hpp"
#include "reg_access.hpp"

using RadeonMmio  = AbsRegAperture<RadeonReg::Id, RegEndian::Swap, 0, RegLog::Verbose>;
using RadeonMmioQ = AbsRegAperture<RadeonReg::Id, RegEndian::Swap, 0, RegLog::Quiet>;

#endif
