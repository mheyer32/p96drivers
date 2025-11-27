#ifndef CHIP_AT3D_H
#define CHIP_AT3D_H

#include "at3d_common.h"
#include "at3dconfig.h"

#define SR_SCRATCH_PAD 0x20  // Scratch pad register in sequencer registers 0x20-0x27

#define EXTSIG_TIMING   0xd9  // Extended signal timing register at memory offset 0D9h
#define ENABLE_EXT_REGS 0xdb  // Enable extended registers at memory offset 0DBh
#define DEVICE_ID 0x182  // Device ID register at memory offset 182-183h

#define DPMS_SYNC_CTRL  0x0d0  // DPMS/sync control register at memory offset 0D0h
#define EXT_DAC_STATUS  0x1fc  // Extended/DAC status register at memory offset 1FC-1FFh

// I2C bit definitions for DPMS_SYNC_CTRL (0x0D0)
// Per AT3D documentation page 232:
// [0] - DPMS HSYNC suspend
// [1] - DPMS VSYNC suspend
// [2] - DDC tri-state HSYNC
// [3] - SCL control bit [0] of [1:0] (bit 1 is 0D0[6])
// [5:4] - SDA control (bit [4] equivalent to 1FC[16])
// [6] - SCL control bit [1] of [1:0] (bit 0 is 0D0[3])
// Note: Reading M0D0[3:6] returns pin values, not register contents

// SCL control: 0D0[6:3] as 2-bit field
//   11 = drive SCL pin high
//   10 = drive SCL pin low
//   0x = input/tri-state
#define I2C_SCL_CTRL_LOW  0x40  // 0D0[3] = bit 0 of SCL control
#define I2C_SCL_CTRL_HIGH 0x48  // 0D0[6] = bit 1 of SCL control
#define I2C_SCL_CTRL_MASK 0x48   // Mask for both SCL control bits

// SDA control: 0D0[5:4] as 2-bit field
//   11 = drive SDA pin high
//   10 = drive SDA pin low
//   0x = input/tri-state
#define I2C_SDA_CTRL_LOW  0x20  // 0D0[4] = bit 0 of SDA control
#define I2C_SDA_CTRL_HIGH 0x30  // 0D0[5] = bit 1 of SDA control
#define I2C_SDA_CTRL_MASK 0x30   // Mask for both SDA control bits

// I2C bit definitions for EXT_DAC_STATUS (0x1FC-0x1FF)
// Per AT3D documentation page 178:
// [16] - SDA input (equivalent to 0D0[4])
// [17] - SCL input (returns the input on the SCL pin)
#define I2C_SDA_IN_BIT  16  // SDA input read bit
#define I2C_SCL_IN_BIT  17  // SCL input read bit

#endif  // CHIP_AT3D_H
