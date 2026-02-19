#ifndef CHIP_AT3D_H
#define CHIP_AT3D_H

#include "at3d_common.h"
#include "at3dconfig.h"

#define SR_SCRATCH_PAD 0x21  // Scratch pad register in sequencer registers 0x20-0x27

// Drawing engine control (register 040h). AT3D-only; per AT3D Table 4.3 / "Drawing engine control" p.188.
#define DRAW_CMD 0x040

#define DRAW_ENGINE_START        BIT(31)
#define DRAW_QUICK_START(x)      ((x) << 29)
#define DRAW_QUICK_START_MASK    (0x3 << 29)
#define DRAW_DST_UPDATE(x)       ((x) << 27)
#define DRAW_DST_UPDATE_MASK     (0x3 << 27)
#define DRAW_ADDRESS_MODEL(x)    ((x) << 24)
#define DRAW_ADDRESS_MODEL_MASK  (0x7 << 24)
#define DRAW_PATTERN_FORMAT(x)   ((x) << 22)
#define DRAW_PATTERN_FORMAT_MASK (0x3 << 22)
#define DRAW_DST_TRANS_POLARITY  BIT(21)
#define DRAW_DST_TRANSPARENT     BIT(20)
#define DRAW_DST_CONTIGUOUS      BIT(19)
#define DRAW_DST_ADDR_LINEAR     BIT(18)
#define DRAW_EZ_LINEAR           BIT(17)
#define DRAW_PIXEL_DEPTH_MASK    (0x7 << 14)
#define DRAW_PIXEL_DEPTH(x)      ((x) << 14)
#define DRAW_SRC_TRANSPARENT     BIT(13)
#define DRAW_SRC_MONOCHROME      BIT(12)
#define DRAW_SRC_CONTIGUOUS      BIT(11)
#define DRAW_6422_PATTERN        BIT(10)
#define DRAW_SRC_ADDR_LINEAR     BIT(9)
#define DRAW_MAJOR_AXIS_X        BIT(8)
#define DRAW_DIR_Y_NEGATIVE      BIT(7)
#define DRAW_DIR_X_NEGATIVE      BIT(6)
#define DRAW_CMD_OP(x)           (x)
#define DRAW_CMD_OP_MASK         (0xF)

#define DRAW_CMD_NOP                0b0000
#define DRAW_CMD_BLT                0b0001  // screen-screen BLT
#define DRAW_CMD_RECT               0b0010  // rectangle
#define DRAW_CMD_STRIP              0b0100  // strip draw is similar to rectangle, but with a height of one pixel.
#define DRAW_CMD_HOST_BLT_WRITE     0b1000  // host BLT write, write memory to screen
#define DRAW_CMD_HOST_BLT_READ      0b1001  // host BLT read,  read screen to memory
#define DRAW_CMD_VECTOR_ENDPOINT    0b1100  // vector, draw endpoint
#define DRAW_CMD_VECTOR_NO_ENDPOINT 0b1101  // vector, don't draw endpoint

#define DST_UPDATE_LAST              0b11
#define DST_UPDATE_BELOW_BOTTOM_LEFT 0b10
#define DST_UPDATE_TOP_RIGHT         0b01
#define DST_UPDATE_DISABLED          0b00

#define QUICKSTART_NONE      0b00
#define QUICKSTART_DIM_WIDTH 0b01
#define QUICKSTART_SRC       0b10
#define QUICKSTART_DST       0b11

#define CLIP_CTRL   0x030
#define CLIP_LEFT   0x038
#define CLIP_TOP    0x03A
#define CLIP_RIGHT  0x03C
#define CLIP_BOTTOM 0x03E

#define RASTEROP  0x046
#define BYTE_MASK 0x047

#define PATTERN             0x048
#define SRC_LOCATION_X_LOW  0x050
#define SRC_LOCATION_Y_HIGH 0x052
#define DST_LOCATION_X_LOW  0x054
#define DST_LOCATION_Y_HIGH 0x056
#define SRC_SIZE_X          0x058  // FIXME: rename?
#define SRC_SIZE_Y          0x05A
// FIXE: this limits 32bit modes to 1024 pixels in width?
#define DST_PITCH              0x05C  // On AT25/3D byte pitch between rows. AT24 and earlier, applies only to 24bit mode
#define SRC_PITCH              0x05E  // On AT25/3D byte pitch between rows. AT24 and earlier, applies only to 24bit mode
#define FRGD_COLOR             0x060
#define BKGD_COLOR             0x064
#define DST_TRANSPARENCY_COLOR 0x06C
#define DST_TRANSPARENCY_MASK  0x06F
#define DDA_AXIAL_STEP         0x070
#define DDA_DIAGONAL_STEP      0x072
#define DDA_ERROR_TERM         0x074

#define SIGANALYSER_CTRL       0x0B4
#define DPMS_SYNC_CTRL         0x0D0  // DPMS/sync control register at memory offset 0D0h
#define MONITOR_INTERLACE_CTRL 0x0D2  // Monitor interlace control register at memory offset 0D2h
#define PIXEL_FIFO_REQ_POINT   0x0D4  // Pixel FIFO request point register at memory offset 0D4h
#define FIFO_UNDERFLOW         0x0D8
#define EXTSIG_TIMING          0x0D9  // Extended signal timing register at memory offset 0D9h
#define UNKNOWN                0x0DA
#define ENABLE_EXT_REGS        0x0DB  // Enable extended registers at memory offset 0DBh
#define BIENDIAN_CTRL          0x0DC  // Bi-endian control register (16-bit at 0xDC-0xDD)
#define APERTURE_CTRL          0x0C2  // Aperture control register memory offset 0C2h
#define DISP_MEM_CFG           0x0C4  // Display memory configuration register at memory offset 0C4h
#define VGA_OVERRIDE           0x0C8
#define FEATURE_CTRL           0x0CC
#define COLOR_CORRECTION       0x0E0  // Color correction register at memory offset 0E0h
#define DAC_CTRL               0x0E4  // DAC control register at memory offset 0E4h
#define OVERCURRENT_RED        0x0E5
#define OVERCURRENT_GREEN      0x0E6
#define OVERCURRENT_BLUE       0x0E7

#define VMI_PORT0_CTRL         0x100
#define VMI_PORT0_TIMING       0x101
#define VMI_PORT0_INDEX_OFFSET 0x102  // 16-bit at 102-103h

#define VMI_PORT1_CTRL         0x104
#define VMI_PORT1_TIMING       0x105
#define VMI_PORT1_INDEX_OFFSET 0x106  // 16-bit at 106-107h

#define THP_CTRL      0x110  // THP control register
#define VMI_PORT_CTRL 0x120  //

// Writes to registers -x140-0x1FF do not pass through the command FIFO
#define HW_CURSOR_CTRL  0x140
#define HW_CURSOR_COL1  0x141
#define HW_CURSOR_COL2  0x142
#define HW_CURSOR_COL3  0x143
#define HW_CURSOR_BASE  0x144  // Hardware cursor base address register (16-bit at 144-145h)
#define HW_CURSOR_X     0x148  // Hardware cursor X position register (16-bit at 148-149h)
#define HW_CURSOR_Y     0x14A  // Hardware cursor Y position register (16-bit at 14A-14Bh)
#define HW_CURSOR_OFF_X 0x14C  // Hardware cursor X offset register (6 bits)
#define HW_CURSOR_OFF_Y 0x14D  // Hardware cursor Y offset register (6 bits), doc table 4.7

#define DEVICE_ID      0x182  // Device ID register at memory offset 182-183h
#define GPIO_CTRL      0x1F0
#define EXT_DAC_STATUS 0x1FC  // Extended/DAC status register at memory offset 1FC-1FFh
#define ABORT          0x1FF  //

#define FAST_RAS_DISABLE_MASK BIT(1)
#define FAST_RAS_DISABLE      BIT(1)

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
#define I2C_SCL_CTRL_MASK 0x48  // Mask for both SCL control bits

// SDA control: 0D0[5:4] as 2-bit field
//   11 = drive SDA pin high
//   10 = drive SDA pin low
//   0x = input/tri-state
#define I2C_SDA_CTRL_LOW  0x20  // 0D0[4] = bit 0 of SDA control
#define I2C_SDA_CTRL_HIGH 0x30  // 0D0[5] = bit 1 of SDA control
#define I2C_SDA_CTRL_MASK 0x30  // Mask for both SDA control bits

// I2C bit definitions for EXT_DAC_STATUS (0x1FC-0x1FF)
// Per AT3D documentation page 178:
// [16] - SDA input (equivalent to 0D0[4])
// [17] - SCL input (returns the input on the SCL pin)
#define I2C_SDA_IN BIT(16)  // SDA input read bit
#define I2C_SCL_IN BIT(17)  // SCL input read bit

// Extended/DAC status register bit definitions
// Per AT3D documentation pages 178, 3937-3981:
// [8] - host BLT in progress (must be high before writing to host BLT port)
// [9] - drawing engine busy
#define EXT_DAC_HOST_BLT_IN_PROGRESS BIT(8)   // Host BLT in progress
#define EXT_DAC_DRAWING_ENGINE_BUSY  BIT(10)  // Drawing engine busy

// Host BLT port: last 32K of flat space (Remap Control maps it here)
#define HOST_BLT_OFFSET 0x3F8000

// Clock registers (MMVGA window - BAR0)
// Per AT3D documentation pages 257-261:
// Clock formula: FOUT = (N+1)(FREF) / ((M+1)(2^L))
// FREF: 8-20 MHz (recommended 14.318 MHz)
// VCO: 185-370 MHz
// N: 8-127 (numerator)
// M: 1-5 (denominator)
// L: 0-3 (postscaler)

#define MCLK_CTRL 0x0E8  // MCLK control register
#define MCLK_DEN  0x0E9  // MCLK denominator (M) register
#define MCLK_NUM  0x0EA  // MCLK numerator (N) register

#define VCLK_CTRL 0x0EC  // VCLK control register
#define VCLK_DEN  0x0ED  // VCLK denominator (M) register
#define VCLK_NUM  0x0EE  // VCLK numerator (N) register

// MCLK/VCLK control register bits (0E8/0EC)
#define CLK_BYPASS          BIT(0)      // Clock bypass
#define CLK_POWER_OFF       BIT(1)      // Clock power off
#define CLK_POSTSCALER_MASK (0x3 << 2)  // Postscaler L [3:2]
#define CLK_POSTSCALER(x)   ((x) << 2)
#define CLK_FREQ_RANGE_MASK (0x7 << 4)  // Frequency range [6:4]
#define CLK_FREQ_RANGE(x)   ((x) << 4)
#define CLK_HIGH_SPEED      BIT(7)  // High speed mode

// Clock numerator/denominator register bits
#define CLK_NUM_MASK 0x7F  // N value [6:0] (8-127)
#define CLK_DEN_MASK 0x7F  // M value [6:0] (1-5)

// Reference frequency (recommended 14.318 MHz = 14318 * 10 KHz)
#define AT3D_REF_FREQ_KHZ10 14318

// Desktop pixel format register (0x080)
// Bits [2:0]: Desktop pixel depth
//   000 = VGA modes
//   001 = 4 bits per pixel
//   010 = 8 bits per pixel (CLUT)
//   100 = 15 bits per pixel
//   101 = 16 bits per pixel
//   111 = 32 bits per pixel
// Bits [4:3]: Desktop pixel format
//   00 = indexed (CLUT)
//   01 = direct RGB
//   1x = reserved
#define SERIAL_CTRL 0x080

#define PALETTE_ACCESS_MASK (0x3 << 5)
#define PALETTE_ACCESS(x)   ((x) << 5)

// Desktop pixel format register bit masks
#define DESKTOP_PIXEL_DEPTH_MASK  0x07  // Bits [2:0]
#define DESKTOP_PIXEL_FORMAT_MASK 0x18  // Bits [4:3]

// Desktop pixel depth values
#define DESKTOP_DEPTH_VGA   0x00  // 000 = VGA modes
#define DESKTOP_DEPTH_4BPP  0x01  // 001 = 4 bits per pixel
#define DESKTOP_DEPTH_8BPP  0x02  // 010 = 8 bits per pixel (CLUT)
#define DESKTOP_DEPTH_15BPP 0x04  // 100 = 15 bits per pixel
#define DESKTOP_DEPTH_16BPP 0x05  // 101 = 16 bits per pixel
#define DESKTOP_DEPTH_32BPP 0x07  // 111 = 32 bits per pixel

// Desktop pixel format values
#define DESKTOP_FORMAT_INDEXED 0x00  // 00 = indexed (CLUT)
#define DESKTOP_FORMAT_DIRECT  0x08  // 01 = direct RGB

// Bi-endian control register (0xDC-0xDD)
// Per AT3D documentation pages 111-112:
// This register controls byte swapping for different apertures and modules
// Bits [1:0]: Host Memory Aperture 0 transform code (Memory offset 0-8MB)
// Bits [3:2]: Host Memory Aperture 1 transform code (Memory offset 8-16MB)
// Transform codes:
//   00 = no transform (default)
//   01 = 16-bit transforms
//   10 = 32-bit transforms
//   11 = reserved
// Bi-endian transform codes
#define BIENDIAN_NO_TRANSFORM 0x00  // No transform
#define BIENDIAN_16BIT_TRANS  0x01  // 16-bit transforms
#define BIENDIAN_32BIT_TRANS  0x02  // 32-bit transforms

// Bi-endian control register bit masks
#define BIENDIAN_APERTURE0_MASK 0x03  // Bits [1:0] - Aperture 0 transform
#define BIENDIAN_APERTURE1_MASK 0x0c  // Bits [3:2] - Aperture 1 transform

// Extended CRTC registers
// Per AT3D documentation pages 183-184:
// CR1D: Character clock adjust (bits [2:0])
#define CR_CHAR_CLOCK_ADJUST 0x1d  // Character clock adjust register

// CR1E: Extended CRTC autoreset (bit 0 = disable automatic CRTC reset)
#define CR_EXT_AUTORESET         0x1e    // Extended CRTC autoreset register
#define CR_EXT_AUTORESET_DISABLE BIT(0)  // Disable automatic CRTC reset

// Sequencer register 1 (SR1) - Clocking mode
// Per AT3D documentation page 129:
// Bit 0: Character clock (1 = 8 dot wide characters, 0 = 9 dot wide characters)
#define SR_CHAR_CLOCK_8_DOT BIT(0)  // 8 pixels per DCLK

// VGA Palette DAC registers
// Per AT3D documentation pages 162-165:
// Standard VGA palette registers for CLUT (Color Look-Up Table)
#define DAC_MASK  0x3C6  // Palette RAM pel mask
#define DAC_RD_AD 0x3C7  // Palette RAM state/read address
#define DAC_WR_AD 0x3C8  // Palette RAM write address
#define DAC_DATA  0x3C9  // Palette RAM data

#endif  // CHIP_AT3D_H
