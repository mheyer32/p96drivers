#ifndef CHIP_ATIMACH64_H
#define CHIP_ATIMACH64_H

#include "common.h"
#include "mach64_common.h"

#include <exec/types.h>  // This header is required for UBYTE and UWORD

#include <assert.h>

typedef ULONG (*ComputeFrequencyFromPllValueFunc_t)(const struct BoardInfo *bi, const PLLValue_t *pllValues);

typedef struct ChipSpecific
{
    UWORD referenceFrequency;
    UWORD referenceDivider;
    UWORD memClock;
    UWORD minPClock;
    UWORD maxPClock;
    UWORD minMClock;
    UWORD maxDRAMClock;
    UWORD maxVRAMClock;
    UBYTE mclkFBDiv;
    UBYTE mclkPostDiv;
    struct PLLValue *vclkPllValues;
    ComputeFrequencyFromPllValueFunc_t computeFrequencyFromPllValue;
} ChipSpecific_t;

typedef struct ChipData
{
    ULONG GEfgPen;
    ULONG GEbgPen;
    UBYTE GEmask;  // programmed mask
    UBYTE GEdrawMode;
    UBYTE GEOp;       // programmed graphics engine setup
    UBYTE MemFormat;  // programmed memory layout/format (FIXME: could be byte sized)

    struct RenderInfo dstBuffer;
    // struct RenderInfo srcBuffer;
    ULONG *patternVideoBuffer;  // points to video memory
    ULONG patternCacheKey;
    UWORD *patternCacheBuffer;  // points to system memory

    UWORD chipFamily;  // chip family
    ChipSpecific_t *chipSpecific;
} ChipData_t;

STATIC_ASSERT(sizeof(ChipData_t) < SIZEOF_MEMBER(BoardInfo_t, ChipData), check_chipdata_size);

static INLINE ChipSpecific_t *getChipSpecific(struct BoardInfo *bi)
{
    return getChipData(bi)->chipSpecific;
}

static INLINE const ChipSpecific_t *getConstChipSpecific(const struct BoardInfo *bi)
{
    return getConstChipData(bi)->chipSpecific;
}

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

#endif
