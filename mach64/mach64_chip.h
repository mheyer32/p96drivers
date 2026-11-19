#ifndef MACH64_CHIP_H
#define MACH64_CHIP_H

#include "mach64_common.h"

#include <assert.h>

class Mach64Driver;

using ComputeFrequencyFromPllValueFunc = ULONG (*)(const Mach64Driver &bi, const PLLValue *pllValues);

struct ChipSpecific
{
    UWORD referenceFrequency;
    UWORD referenceDivider;
    UWORD memClock;
    UWORD minPClock;
    UWORD maxPClock;
    UWORD minMClock;
    UWORD maxDRAMClock;
    UWORD maxVRAMClock;
    UWORD mclkFBDiv; /* feedback divider; ×4 PLL path stores 2*N */
    UBYTE mclkPostDiv;
    PLLValue *vclkPllValues;
    ComputeFrequencyFromPllValueFunc computeVCLKFrequency;
};

struct I2COps;

struct ChipData
{
    ULONG GEfgPen;
    ULONG GEbgPen;
    UBYTE GEmask;
    UBYTE GEdrawMode;
    UBYTE GEOp;
    UBYTE MemFormat;

    struct RenderInfo dstBuffer;
    ULONG *patternVideoBuffer;
    ULONG patternCacheKey;
    UWORD *patternCacheBuffer;

    UWORD ioSparseBase;
    UBYTE p96VBlankInt;
    ChipFamily chipFamily;
    ChipSpecific *chipSpecific;

    const I2COps *i2cOps;

    /* GX external DAC cursor pens: 68860 uses [0..1]; RGB514 Mode0 uses [0..2]. */
    UBYTE cursorRGB[3][3];

    /* Cached FIFO_STAT after accounting for pending GE writes (GX path). */
    UWORD fifoSlotsCached;
};

STATIC_ASSERT(sizeof(ChipData) < SIZEOF_MEMBER(BoardInfo_t, ChipData), check_chipdata_size);

static INLINE ChipSpecific &getChipSpecific(struct BoardInfo *bi)
{
    return *getChipData(bi)->chipSpecific;
}

static INLINE const ChipSpecific &getConstChipSpecific(const struct BoardInfo *bi)
{
    return *getConstChipData(bi)->chipSpecific;
}

#include "mach64_driver.hpp"

static INLINE ChipSpecific &getChipSpecific(Mach64Driver &bi)
{
    return bi.chipSpecific();
}

static INLINE const ChipSpecific &getConstChipSpecific(const Mach64Driver &bi)
{
    return bi.chipSpecific();
}

/* bi->MemoryClock is Hz; ROM clocks are 10 kHz units. */
UWORD resolveMemoryClockKhz10(Mach64Driver &bi);
void SetMemoryClock(Mach64Driver &bi, UWORD freqKhz10);
extern "C" BOOL InitChip(__REGA0(struct BoardInfo *bi));

struct __attribute__((packed)) Mach64RomHeader
{
    UBYTE struct_size[2];
    UBYTE type_definition;
    UBYTE extended_function_code;
    UBYTE bios_revision_major;
    UBYTE bios_revision_minor;
    UWORD io_address_sparse;
    UWORD reserved_1;
    UWORD reserved_2;
    UWORD reserved_3;
    UWORD dram_memory_cycle;
    UWORD vram_memory_cycle;
    UWORD freq_table_ptr;
    UWORD logon_message_ptr;
    UWORD misc_info_ptr;
    UWORD pci_bus_dev_init_code;
    UWORD reserved_4;
    UWORD io_base_address;
    UWORD reserved_5;
    UWORD reserved_6;
    UWORD reserved_7;
    UWORD int10h_offset;
    UWORD int10h_segment;
    UWORD monitor_info_oem;
    ULONG memory_mapped_location;
    ULONG reserved_8;
    UWORD tvout_info;
    UWORD fixed_values[3];
    UWORD bios_runtime_address;
    UWORD reserved_9;
    UWORD feature_id;
    UWORD subsystem_vendor_id;
    UWORD subsystem_id;
    UWORD device_id;
    UWORD config_string_ptr;
    UWORD video_feature_table_ptr;
    UWORD hardware_info_table_ptr;
    UBYTE signatures[16];
};

struct __attribute__((packed)) FrequencyTable
{
    UBYTE frequency_table_id;
    UBYTE reserved2;
    UWORD min_pclk_freq;
    UWORD max_pclk_freq;
    UBYTE extended_coprocessor_mode;
    UBYTE extended_vga_mode;
    UWORD ref_clock_freq;
    UWORD ref_clock_divider;
    UWORD hardware_specific_info;
    UWORD mclk_freq_power_down;
    UWORD mclk_freq_normal_dram;
    UWORD mclk_freq_normal_vram;
    UWORD sclk_freq;
    UBYTE mclk_entry_num;
    UBYTE sclk_entry_num;
    UWORD coprocessor_mode_mclk_freq;
    UWORD reserved;
    UWORD terminator;
};

#define COLOR_DEPTH_1  0
#define COLOR_DEPTH_4  1
#define COLOR_DEPTH_8  2
#define COLOR_DEPTH_15 3
#define COLOR_DEPTH_16 4
#define COLOR_DEPTH_24 5
#define COLOR_DEPTH_32 6

#endif
