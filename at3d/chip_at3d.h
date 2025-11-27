#ifndef CHIP_AT3D_H
#define CHIP_AT3D_H

#include "at3d_common.h"
#include "at3dconfig.h"

#define SR_SCRATCH_PAD 0x20  // Scratch pad register in sequencer registers 0x20-0x27

#define EXTSIG_TIMING   0xd9  // Extended signal timing register at memory offset 0D9h
#define ENABLE_EXT_REGS 0xdb  // Enable extended registers at memory offset 0DBh
#define DEVICE_ID 0x182  // Device ID register at memory offset 182-183h

#endif  // CHIP_AT3D_H
