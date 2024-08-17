#ifndef CHIP_ATIMACH64_H
#define CHIP_ATIMACH64_H

#include "common.h"

typedef enum ChipFamily {
  UNKNOWN,
  MACH64VT // no 8mb aperture only
} ChipFamily_t;

typedef struct ChipData {
  RGBFTYPE MemFormat; // programmed memory layout/format
                      //  struct Library *DOSBase;
  BlitterOp_t GEOp;   // programmed graphics engine setup
  ULONG GEfgPen;
  ULONG GEbgPen;
  RGBFTYPE GEFormat;
  UWORD GEbytesPerRow; // programmed graphics engine bytes per row
  UWORD GEsegs;        // programmed src/dst memory segments
  UBYTE GEbpp;         // programmed graphics engine bpp
  UBYTE GEmask;        // programmed mask
  UBYTE GEdrawMode;
  ChipFamily_t chipFamily; // chip family
} ChipData_t;

#define DWORD_OFFSET(x) (x * 4)
#define SCRATCH_REG0 DWORD_OFFSET(0x20)
#define SCRATCH_REG1 DWORD_OFFSET(0x21)
#define BUS_CNTL DWORD_OFFSET(0x28)
#define MEM_CNTL DWORD_OFFSET(0x2C)
#define GEN_TEST_CNTL DWORD_OFFSET(0x34)
#define CONFIG_CNTL DWORD_OFFSET(0x37)
#define CONFIG_CHIP_ID DWORD_OFFSET(0x38)
#define CONFIG_STAT0 DWORD_OFFSET(0x39)

#endif
