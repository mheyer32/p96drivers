#ifndef AT3D_REG_APERTURES_HPP
#define AT3D_REG_APERTURES_HPP

#include "at3d_regs.hpp"
#include "vga_aperture.hpp"

// AT3D configuration
// AT3D doesn't have MMIO_ONLY, nor Packed MMIO
// BIGENDIAN_MMIO and BIGENDIAN_IO will both be 0

using At3dMmio  = AbsRegAperture<AT3DMmioReg::Id, RegEndian::Swap, 0, RegLog::Verbose>;
using At3dMmioQ = AbsRegAperture<AT3DMmioReg::Id, RegEndian::Swap, 0, RegLog::Quiet>;

#endif
