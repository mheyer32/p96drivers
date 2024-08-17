#ifndef CHIP_ATIMACH64_H
#define CHIP_ATIMACH64_H

typedef enum BlitterOp
{
  None,
  FILLRECT,
  INVERTRECT,
  BLITRECT,
  BLITRECTNOMASKCOMPLETE,
  BLITTEMPLATE,
  BLITPLANAR2CHUNKY,
  LINE
} BlitterOp_t;

typedef enum ChipFamily
{
    UNKNOWN,
    MACH64VT,; // no 8mb aperture only
} ChipFamily_t;

typedef struct ChipData
{
  RGBFTYPE MemFormat;   // programmed memory layout/format
//  struct Library *DOSBase;
  BlitterOp_t GEOp;     // programmed graphics engine setup
  ULONG GEfgPen;
  ULONG GEbgPen;
  RGBFTYPE GEFormat;
  UWORD GEbytesPerRow;  // programmed graphics engine bytes per row
  UWORD GEsegs;         // programmed src/dst memory segments
  UBYTE GEbpp;          // programmed graphics engine bpp
  UBYTE GEmask;         // programmed mask
  UBYTE GEdrawMode;
  ChipFamily_t chipFamily;       // chip family
} ChipData_t;

#endif
