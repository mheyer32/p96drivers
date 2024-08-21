#ifndef CHIP_ATIMACH64_H
#define CHIP_ATIMACH64_H

#include "common.h"

typedef enum ChipFamily
{
    UNKNOWN,
    MACH64VT  // no 8mb aperture only
} ChipFamily_t;

typedef struct ChipData
{
    RGBFTYPE MemFormat;  // programmed memory layout/format
                         //  struct Library *DOSBase;
    BlitterOp_t GEOp;    // programmed graphics engine setup
    ULONG GEfgPen;
    ULONG GEbgPen;
    RGBFTYPE GEFormat;
    UWORD GEbytesPerRow;  // programmed graphics engine bytes per row
    UWORD GEsegs;         // programmed src/dst memory segments
    UBYTE GEbpp;          // programmed graphics engine bpp
    UBYTE GEmask;         // programmed mask
    UBYTE GEdrawMode;
    ChipFamily_t chipFamily;  // chip family
} ChipData_t;

#include <exec/types.h> // This header is required for UBYTE and UWORD

typedef struct Mach64RomHeader {
    UBYTE struct_size[2];              // -1, -2: Size of the structure in number of bytes
    UBYTE type_definition;             // 0: Type definition
    UBYTE extended_function_code;      // 1: Extended function code (0a0h, 0a1h...etc.)
    UBYTE bios_revision_major;         // 2: BIOS internal revision, major
    UBYTE bios_revision_minor;         // 3: BIOS internal revision, minor
    UWORD io_address_sparse;           // 4 - 5: I/O address, for sparse only
    UWORD reserved_1;                  // 6 - 7: Reserved
    UWORD reserved_2;                  // 8 - 9: Reserved
    UWORD reserved_3;                  // 10 - 11: Reserved
    UWORD dram_memory_cycle;           // 12 - 13: DRAM memory cycle in extended and VGA
    UWORD vram_memory_cycle;           // 14 - 15: VRAM memory cycle in extended and VGA
    UWORD freq_table_ptr;              // 16 - 17: Pointer to frequency table
    UWORD logon_message_ptr;           // 18 - 19: Pointer to log-on message
    UWORD misc_info_ptr;               // 20 - 21: Pointer to miscellaneous information
    UWORD pci_bus_dev_init_code;       // 22 - 23: PCI, Bus, Dev, Init code
    UWORD reserved_4;                  // 24 - 25: Reserved
    UWORD io_base_address;             // 26 - 27: I/O base address if non-zero, block I/O enable
    UWORD reserved_5;                  // 28 - 29: Reserved (used)
    UWORD reserved_6;                  // 30 - 31: Reserved (used)
    UWORD reserved_7;                  // 32 - 33: Reserved (used)
    UWORD int10h_offset;               // 34 - 35: Int 10h offset, Coprocessor Only BIOS
    UWORD int10h_segment;              // 36 - 37: Int 10h segment, Coprocessor Only BIOS
    UWORD monitor_info_oem;            // 38 - 39: Monitor information, OEM specific
    ULONG memory_mapped_location;      // 40 - 43: 4K memory mapped location
    ULONG reserved_8;                  // 44 - 47: Reserved (used)
    UWORD tvout_info;                  // 48 - 49: TVOut (see below for details)
    UWORD fixed_values[3];             // 50 - 55: 0ffffh, 0, 0ffffh
    UWORD bios_runtime_address;        // 56 - 57: BIOS runtime address
    UWORD reserved_9;                  // 58 - 59: Reserved (used)
    UWORD feature_id;                  // 60 - 61: Feature ID
    UWORD subsystem_vendor_id;         // 62 - 63: Subsystem vendor ID
    UWORD subsystem_id;                // 64 - 65: Subsystem ID
    UWORD device_id;                   // 66 - 67: Device ID
    UWORD config_string_ptr;           // 68 - 69: Pointer to Config string
    UWORD video_feature_table_ptr;     // 70 - 71: Pointer to Video Feature table
    UWORD hardware_info_table_ptr;     // 72 - 73: Pointer to Hardware Info table
    UBYTE signatures[16];              // 74 - 89: $??? Signatures indicating pointers to hardware information table
} Mach64RomHeader_t;

#define DWORD_OFFSET(x) (x * 4)
#define SCRATCH_REG0 (0x20)
#define SCRATCH_REG1 (0x21)
#define BUS_CNTL (0x28)
#define MEM_CNTL (0x2C)
#define GEN_TEST_CNTL (0x34)
#define CONFIG_CNTL (0x37)
#define CONFIG_CHIP_ID (0x38)
#define CONFIG_STAT0 (0x39)

#define BUS_FIFO_ERR_INT_EN BIT(20)
#define BUS_FIFO_ERR_INT BIT(21)
#define BUS_FIFO_ERR_AK BIT(21)
#define BUS_HOST_ERR_INT_EN BIT(22)
#define BUS_HOST_ERR_INT BIT(23)
#define BUS_HOST_ERR_AK BIT(23)

static inline ULONG REGARGS readATIRegisterL(volatile UBYTE *regbase, UWORD regIndex, const char *regName)
{
    ULONG value = readRegL(regbase, DWORD_OFFSET(regIndex));
    D(10, "R %s -> 0x%08lx\n", regName, (LONG)value);

    return value;
}

static inline void REGARGS writeATIRegisterL(volatile UBYTE *regbase, UWORD regIndex, ULONG value, const char *regName)
{
    D(10, "W %s <- 0x%08lx\n", regName, (LONG)value);
    writeRegL(regbase, DWORD_OFFSET(regIndex), value);
}

static inline void REGARGS writeATIRegisterMaskL(volatile UBYTE *regbase, UWORD regIndex, ULONG mask, ULONG value, const char *regName)
{
    ULONG regValue = readATIRegisterL(regbase, regIndex, regName);
    regValue = (regValue & ~mask) | (mask & value);
    writeRegL(regbase, DWORD_OFFSET(regIndex), regValue);
}

// FIXME reusing the same function for IO and MMIO shouldn't work because MMIOREGISTER_OFFSET and REGISTER_OFFSET might
// be different, but in practise they aren't. Refactor the code.
#define R_BLKIO_L(regIndex) readATIRegisterL(RegBase, regIndex, #regIndex)
#define W_BLKIO_L(regIndex, value) writeATIRegisterL(RegBase, regIndex, (ULONG)value, #regIndex)
#define W_BLKIO_MASK_L(regIndex, mask, value) writeATIRegisterMaskL(RegBase, regIndex, mask, (ULONG)value, #regIndex)

#undef R_MMIO_L
#undef W_MMIO_L
#define R_MMIO_L(regIndex) readATIRegisterL(MMIOBase, regIndex, #regIndex)
#define W_MMIO_L(regIndex, value) writeATIRegisterL(MMIOBase, regIndex, value, #regIndex)

#endif
