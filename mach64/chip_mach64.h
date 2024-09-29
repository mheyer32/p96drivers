#ifndef CHIP_ATIMACH64_H
#define CHIP_ATIMACH64_H

#include "common.h"

#include <exec/types.h>  // This header is required for UBYTE and UWORD


typedef enum ChipFamily
{
    UNKNOWN,
    MACH64GX,
    MACH64VT  // no 8mb aperture only
} ChipFamily_t;

typedef struct PLLValue
{
    UBYTE Plog2;  // post divider log2
    UBYTE N;      // feedback divider
} PLLValue_t;

typedef struct ChipData
{
    RGBFTYPE MemFormat;  // programmed memory layout/format
    BlitterOp_t GEOp;    // programmed graphics engine setup
    ULONG GEfgPen;
    ULONG GEbgPen;
    // RGBFTYPE GEFormat;

    struct RenderInfo dstBuffer;
    // struct RenderInfo srcBuffer;
    ULONG *patternVideoBuffer; // points to video memory
    ULONG patternSetupCache;
    UWORD *patternCacheBuffer; // points to system memory

    UBYTE GEmask;         // programmed mask
    UBYTE GEdrawMode;

    ChipFamily_t chipFamily;  // chip family
    UWORD referenceFrequency;
    UWORD referenceDivider;
    UWORD memClock;
    UWORD minPClock;
    UWORD maxPClock;
    PLLValue_t *pllValues;
} ChipData_t;

STATIC_ASSERT(sizeof(ChipData_t) < SIZEOF_MEMBER(BoardInfo_t, ChipData), check_chipdata_size);

typedef struct Mach64RomHeader
{
    UBYTE struct_size[2];           // -1, -2: Size of the structure in number of bytes
    UBYTE type_definition;          // 0: Type definition
    UBYTE extended_function_code;   // 1: Extended function code (0a0h, 0a1h...etc.)
    UBYTE bios_revision_major;      // 2: BIOS internal revision, major
    UBYTE bios_revision_minor;      // 3: BIOS internal revision, minor
    UWORD io_address_sparse;        // 4 - 5: I/O address, for sparse only
    UWORD reserved_1;               // 6 - 7: Reserved
    UWORD reserved_2;               // 8 - 9: Reserved
    UWORD reserved_3;               // 10 - 11: Reserved
    UWORD dram_memory_cycle;        // 12 - 13: DRAM memory cycle in extended and VGA
    UWORD vram_memory_cycle;        // 14 - 15: VRAM memory cycle in extended and VGA
    UWORD freq_table_ptr;           // 16 - 17: Pointer to frequency table
    UWORD logon_message_ptr;        // 18 - 19: Pointer to log-on message
    UWORD misc_info_ptr;            // 20 - 21: Pointer to miscellaneous information
    UWORD pci_bus_dev_init_code;    // 22 - 23: PCI, Bus, Dev, Init code
    UWORD reserved_4;               // 24 - 25: Reserved
    UWORD io_base_address;          // 26 - 27: I/O base address if non-zero, block I/O enable
    UWORD reserved_5;               // 28 - 29: Reserved (used)
    UWORD reserved_6;               // 30 - 31: Reserved (used)
    UWORD reserved_7;               // 32 - 33: Reserved (used)
    UWORD int10h_offset;            // 34 - 35: Int 10h offset, Coprocessor Only BIOS
    UWORD int10h_segment;           // 36 - 37: Int 10h segment, Coprocessor Only BIOS
    UWORD monitor_info_oem;         // 38 - 39: Monitor information, OEM specific
    ULONG memory_mapped_location;   // 40 - 43: 4K memory mapped location
    ULONG reserved_8;               // 44 - 47: Reserved (used)
    UWORD tvout_info;               // 48 - 49: TVOut (see below for details)
    UWORD fixed_values[3];          // 50 - 55: 0ffffh, 0, 0ffffh
    UWORD bios_runtime_address;     // 56 - 57: BIOS runtime address
    UWORD reserved_9;               // 58 - 59: Reserved (used)
    UWORD feature_id;               // 60 - 61: Feature ID
    UWORD subsystem_vendor_id;      // 62 - 63: Subsystem vendor ID
    UWORD subsystem_id;             // 64 - 65: Subsystem ID
    UWORD device_id;                // 66 - 67: Device ID
    UWORD config_string_ptr;        // 68 - 69: Pointer to Config string
    UWORD video_feature_table_ptr;  // 70 - 71: Pointer to Video Feature table
    UWORD hardware_info_table_ptr;  // 72 - 73: Pointer to Hardware Info table
    UBYTE signatures[16];           // 74 - 89: $??? Signatures indicating pointers to hardware information table
} Mach64RomHeader_t;

typedef struct
{
    ULONG reserved2 : 3;         // Bits 29-31: Reserved
    ULONG mem_oe_select : 2;     // Bits 27-28: OEb(0) Pin Function Select
    ULONG mem_pix_width : 3;     // Bits 24-26: Memory Pixel Width
    ULONG reserved : 1;          // Bit 23: Reserved
    ULONG cde_pullback : 1;      // Bit 22: CDE Pullback
    ULONG low_latency_mode : 1;  // Bit 21: Low Latency Mode
    ULONG mem_tile_select : 2;   // Bits 19-20: Memory Tile Select
    ULONG mem_sdram_reset : 1;   // Bit 18: SDRAM Reset
    ULONG dll_gain_cntl : 2;     // Bits 16-17: DLL Gain Control
    ULONG mem_actv_pre : 2;      // Bits 14-15: SDRAM Active to Precharge Minimum Latency
    ULONG dll_reset : 1;         // Bit 13: DLL Reset
    ULONG mem_refresh_rate : 2;  // Bits 11-12: Memory refresh rate
    ULONG mem_cyc_lnth : 2;      // Bits 9-10: DRAM non-page memory cycle length
    ULONG mem_cyc_lnth_aux : 2;  // Bits 7-8: SDRAM Actv to Cmd cycle length
    ULONG mem_refresh : 4;       // Bits 3-6: Refresh Cycle Length
    ULONG mem_size : 3;          // Bits 0-2: Memory size
} MEM_CNTL_Register;

typedef struct FrequencyTable
{
    UBYTE frequency_table_id;  // Frequency table identification
    UBYTE reserved2;
    UWORD min_pclk_freq;               // Minimum PCLK frequency (in 100Hz)
    UWORD max_pclk_freq;               // Maximum PCLK frequency (in 100Hz)
    UBYTE extended_coprocessor_mode;   // Extended coprocessor mode PCLK entry if <> 0xffh
    UBYTE extended_vga_mode;           // Extended VGA mode PCLK entry if <> 0xffh
    UWORD ref_clock_freq;              // Reference clock frequency (in 100Hz)
    UWORD ref_clock_divider;           // Reference clock divider
    UWORD hardware_specific_info;      // Hardware specific information
    UWORD mclk_freq_power_down;        // MCLK frequency in power down mode
    UWORD mclk_freq_normal_dram;       // MCLK frequency in normal mode for DRAM boards
    UWORD mclk_freq_normal_vram;       // MCLK frequency in normal mode for VRAM boards
    UWORD sclk_freq;                   // SCLK frequency
    UBYTE mclk_entry_num;              // MCLK entry number
    UBYTE sclk_entry_num;              // SCLK entry number
    UWORD coprocessor_mode_mclk_freq;  // Coprocessor mode MCLK frequency if != 0
    UWORD reserved;                    // Reserved
    UWORD terminator;                  // 0xffh
} FrequencyTable_t;

typedef struct PLLTable
{
    USHORT pllValues[20];
} PLLTable_t;

#define COLOR_DEPTH_1  0
#define COLOR_DEPTH_4  1
#define COLOR_DEPTH_8  2
#define COLOR_DEPTH_15 3
#define COLOR_DEPTH_16 4
#define COLOR_DEPTH_24 5
#define COLOR_DEPTH_32 6

typedef struct MaxColorDepthTableEntry
{
    UBYTE h_disp;       // max horizontal resolution in chars
    UBYTE dacmask;      // DAC this applies to
    UBYTE ram_req;      // memory required
    UBYTE max_dot_clk;  // max dot clock
    UBYTE color_depth;  // max color depth
    UBYTE DUMMY;
} MaxColorDepthTableEntry_t;

#define DWORD_OFFSET(x) ((x) * 4)

#define SCRATCH_REG0   (0x20)
#define SCRATCH_REG1   (0x21)
#define BUS_CNTL       (0x28)
#define MEM_CNTL       (0x2C)
#define GEN_TEST_CNTL  (0x34)
#define CONFIG_CNTL    (0x37)
#define CONFIG_CHIP_ID (0x38)
#define CONFIG_STAT0   (0x39)
#define CLOCK_CNTL     (0x24)
#define DAC_REGS       (0x30)
#define DAC_CNTL       (0x31)

#define CRTC_H_TOTAL_DISP     0x00
#define CRTC_H_SYNC_STRT_WID  0x01
#define CRTC_V_TOTAL_DISP     0x02
#define CRTC_V_SYNC_STRT_WID  0x03
#define CRTC_VLINE_CRNT_VLINE 0x04
#define CRTC_OFF_PITCH        0x05
#define CRTC_INT_CNTL         0x06
#define CRTC_GEN_CNTL         0x07
#define OVR_CLR               0x10
#define OVR_WID_LEFT_RIGHT    0x11
#define OVR_WID_TOP_BOTTOM    0x12
#define CUR_CLR0              0x18
#define CUR_CLR1              0x19
#define CUR_OFFSET            0x1A
#define CUR_HORZ_VERT_POSN    0x1B
#define CUR_HORZ_VERT_OFF     0x1C

#define FIFO_STAT             0xC4
#define GUI_STAT              0xCE
#define HOST_CNTL             0x90
#define HOST_DATA0            0x80  // 16 registers

#define DST_OFF_PITCH    0x40
// #define DST_X            0x41
// #define DST_Y            0x42
#define DST_Y_X          0x43
// #define DST_WIDTH        0x44
// #define DST_HEIGHT       0x45
#define DST_HEIGHT_WIDTH 0x46
#define DST_X_WIDTH      0x47
#define DST_BRES_LNTH    0x48
#define DST_BRES_ERR     0x49
#define DST_BRES_INC     0x4A
#define DST_BRES_DEC     0x4B
#define DST_CNTL         0x4C

#define SRC_OFF_PITCH      0x60
// #define SRC_X              0x61
// #define SRC_Y              0x62
#define SRC_Y_X            0x63
// #define SRC_WIDTH1         0x64
// #define SRC_HEIGHT1        0x65
#define SRC_HEIGHT1_WIDTH1 0x66
// #define SRC_X_START        0x67
// #define SRC_Y_START        0x68
 #define SRC_Y_X_START      0x69
// #define SRC_WIDTH2         0x6A
// #define SRC_HEIGHT2        0x6B
#define SRC_HEIGHT2_WIDTH2 0x6C
#define SRC_CNTL           0x6D

#define PAT_REG0 0xA0
#define PAT_REG1 0xA1
#define PAT_CNTL 0xA2

// #define SC_LEFT       0xA8
// #define SC_RIGHT      0xA9
#define SC_LEFT_RIGHT 0xAA
// #define SC_TOP        0xAB
// #define SC_BOTTOM     0xAC
#define SC_TOP_BOTTOM 0xAD

#define DP_BKGD_CLR  0xB0
#define DP_FRGD_CLR  0xB1
#define DP_WRITE_MSK 0xB2
#define DP_CHAIN_MSK 0xB3
#define DP_PIX_WIDTH 0xB4
#define DP_MIX       0xB5
#define DP_SRC       0xB6

#define CLR_CMP_CLR  0xC0
#define CLR_CMP_MSK  0xC1
#define CLR_CMP_CNTL 0xC2

#define CONTEXT_MSK       0xC8
#define CONTEXT_LOAD_CNTL 0xCB
#define GUI_TRAJ_CNTL     0xCC
#define GUI_STAT          0xCE

#define BUS_FIFO_ERR_INT_EN BIT(20)
#define BUS_FIFO_ERR_INT    BIT(21)
#define BUS_FIFO_ERR_AK     BIT(21)  // INT and ACK are the same bit, distiguished by R/W operation
#define BUS_HOST_ERR_INT_EN BIT(22)
#define BUS_HOST_ERR_INT    BIT(23)
#define BUS_HOST_ERR_AK     BIT(23)  // INT and ACK are the same bit, distiguished by R/W operation



static inline UBYTE REGARGS readATIRegisterB(volatile UBYTE *regbase, UWORD regIndex, UWORD byteIndex,
                                             const char *regName)
{
    UBYTE value = readReg(regbase, DWORD_OFFSET(regIndex) + byteIndex);
    D(VERBOSE, "R %s_%ld -> 0x%02lx\n", regName, (LONG)byteIndex, (LONG)value);

    return value;
}

static inline ULONG REGARGS readATIRegisterL(volatile UBYTE *regbase, UWORD regIndex, const char *regName)
{
    ULONG value = readRegL(regbase, DWORD_OFFSET(regIndex));
    D(VERBOSE, "R %s -> 0x%08lx\n", regName, (LONG)value);

    return value;
}

static inline ULONG REGARGS readATIRegisterAndMaskL(volatile UBYTE *regbase, UWORD regIndex, ULONG mask, const char *regName)
{
    ULONG value = swapl(readRegL(regbase, DWORD_OFFSET(regIndex)) & swapl(mask));
    D(VERBOSE, "R %s -> 0x%08lx\n", regName, (LONG)value);

    return value;
}


static inline void REGARGS writeATIRegisterMaskB(volatile UBYTE *regbase, UWORD regIndex, UWORD byteIndex, UBYTE mask,
                                                 UBYTE value, const char *regName)
{
    UBYTE regValue = readReg(regbase, DWORD_OFFSET(regIndex) + byteIndex);
    regValue &= ~mask;
    regValue |= value & mask;
    D(VERBOSE, "W %s_%ld <- 0x%02lx\n", regName, (LONG)byteIndex, (LONG)value);
    writeReg(regbase, DWORD_OFFSET(regIndex) + byteIndex, regValue);
}

static inline void REGARGS writeATIRegisterB(volatile UBYTE *regbase, UWORD regIndex, UWORD byteIndex, UBYTE value,
                                             const char *regName)
{
    D(VERBOSE, "W %s_%ld <- 0x%02lx\n", regName, (LONG)byteIndex, (LONG)value);
    writeReg(regbase, DWORD_OFFSET(regIndex) + byteIndex, value);
}

static inline void REGARGS writeATIRegisterL(volatile UBYTE *regbase, UWORD regIndex, ULONG value, const char *regName)
{
    D(VERBOSE, "W %s <- 0x%08lx\n", regName, (LONG)value);
    writeRegL(regbase, DWORD_OFFSET(regIndex), value);
}

static inline void REGARGS writeATIRegisterMaskL(volatile UBYTE *regbase, UWORD regIndex, ULONG mask, ULONG value,
                                                 const char *regName)
{
    ULONG regValue = readRegLNoSwap(regbase, DWORD_OFFSET(regIndex));
    D(VERBOSE, "R %s -> 0x%08lx\n", regName, (LONG)swapl(regValue));

    mask = swapl(mask);
    value = swapl(value);
    regValue       = (regValue & ~mask) | (mask & value);

    D(VERBOSE, "W %s <- 0x%08lx\n", regName, (LONG)swapl(regValue));
    writeRegLNoSwap(regbase, DWORD_OFFSET(regIndex), regValue);
}

// FIXME reusing the same function for IO and MMIO shouldn't work because MMIOREGISTER_OFFSET and REGISTER_OFFSET might
// be different, but in practise they aren't. Refactor the code.
#define R_BLKIO_B(regIndex, byteIndex)        readATIRegisterB(RegBase, regIndex, byteIndex, #regIndex)
#define R_BLKIO_L(regIndex)                   readATIRegisterL(RegBase, regIndex, #regIndex)
#define R_BLKIO_AND_L(regIndex, mask)        readATIRegisterAndMaskL(RegBase, regIndex, mask, #regIndex)
#define W_BLKIO_B(regIndex, byteIndex, value) writeATIRegisterB(RegBase, regIndex, byteIndex, value, #regIndex)
#define W_BLKIO_MASK_B(regIndex, byteIndex, mask, value) \
    writeATIRegisterMaskB(RegBase, regIndex, byteIndex, mask, value, #regIndex)
#define W_BLKIO_L(regIndex, value)            writeATIRegisterL(RegBase, regIndex, value, #regIndex)
#define W_BLKIO_MASK_L(regIndex, mask, value) writeATIRegisterMaskL(RegBase, regIndex, mask, value, #regIndex)

#undef R_MMIO_L
#undef W_MMIO_L
#undef W_MMIO_MASK_L
#define R_MMIO_L(regIndex)        readATIRegisterL(MMIOBase, regIndex, #regIndex)
#define W_MMIO_L(regIndex, value) writeATIRegisterL(MMIOBase, regIndex, value, #regIndex)
#define W_MMIO_MASK_L(regIndex, mask, value) writeATIRegisterMaskL(MMIOBase, regIndex, mask, value, #regIndex)

#endif
