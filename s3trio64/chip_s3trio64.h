#ifndef CHIP_S3TRIO64_H
#define CHIP_S3TRIO64_H

#include "common.h"
#include "s3trio64_common.h"

typedef struct PLLValue
{
    UBYTE m;  // M numerant
    UBYTE n;  // N divider
    UBYTE r;  // 2 << R divider
} PLLValue_t;

typedef struct ChipData
{
    ULONG GEfgPen;
    ULONG GEbgPen;

    UWORD GEbytesPerRow;  // programmed graphics engine bytes per row
    UWORD GEsegs;         // programmed src/dst memory segments

    UBYTE GEOp;  // programmed graphics engine setup
    UBYTE GEFormat;

    UBYTE GEbpp;   // programmed graphics engine bpp
    UBYTE GEmask;  // programmed mask

    UBYTE GEdrawMode;
    UBYTE MemFormat;            // programmed memory layout/format
                                //  struct Library *DOSBase;
    UBYTE chipFamily;           // chip family
    UWORD pattSegment;          // segment where pattern is stored FIXME can use UBYTE to store both segments
    UWORD pattX;                // x offset in pattern
    UWORD pattY;                // y offset in pattern
    ULONG *patternVideoBuffer;  // points to video memory
    UWORD *patternCacheBuffer;  // points to system memory
    ULONG patternCacheKey;

    // PLL table for pixel clocks
    PLLValue_t *pllValues;
    UWORD numPllValues;

} ChipData_t;

STATIC_ASSERT(sizeof(ChipData_t) <= sizeof(((BoardInfo_t *)0)->ChipData), ChipData_t_too_large);

typedef struct CardData
{
    BYTE* legacyIOBase;  // legacy I/O base address
    struct Library *OpenPciBase;
    struct pci_dev *board;
    struct Node boardNode;
} CardData_t;

STATIC_ASSERT(sizeof(CardData_t) < SIZEOF_MEMBER(BoardInfo_t, CardData), check_carddata_size);

static INLINE UWORD readBEE8(volatile UBYTE *RegBase, UBYTE idx)
{
    // BEWARE: the read index bit value does not fully match 'idx'
    // We do not cover 9AE8, 42E8, 476E8 here (which can be read, too, through this
    // register)
    switch (idx) {
    case 0xA:
        idx = 0b0101;
        break;
    case 0xD:
        idx = 0b1010;
        break;
    case 0xE:
        idx = 0b0110;
        break;
    }

    // The read select register cannot be MMIO mapped on older chip series, thus
    // always read through I/O register
    W_IO_W(0xBEE8, (0xF << 12) | idx);
    return R_IO_W(0xBEE8) & 0xFFF;
}

#define LEGACYIOBASE() volatile UBYTE *RegBase  = getCardData(bi)->legacyIOBase
#define R_BEE8(idx)        readBEE8(RegBase, idx)
#define W_BEE8(idx, value) W_MMIO_W(0xBEE8, ((idx << 12) | value))

#endif
