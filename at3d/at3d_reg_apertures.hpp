#ifndef AT3D_REG_APERTURES_HPP
#define AT3D_REG_APERTURES_HPP

#include "at3d_regs.hpp"
#include "reg_access.hpp"

#if BIGENDIAN_MMIO
#define AT3D_MMIO_ENDIAN RegEndian::NoSwap
#else
#define AT3D_MMIO_ENDIAN RegEndian::SwapMMIO
#endif

#ifndef MMIOREGISTER_OFFSET
#error MMIOREGISTER_OFFSET required
#endif

using At3dMmio  = AbsRegAperture<MmioReg::Id, AT3D_MMIO_ENDIAN, MMIOREGISTER_OFFSET, RegLog::Verbose>;
using At3dMmioQ = AbsRegAperture<MmioReg::Id, AT3D_MMIO_ENDIAN, MMIOREGISTER_OFFSET, RegLog::Quiet>;

#endif
