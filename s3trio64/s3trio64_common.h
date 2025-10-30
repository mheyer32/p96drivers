#ifndef S3TRIO64_COMMON_H
#define S3TRIO64_COMMON_H

#include "common.h"

#include <exec/types.h>

STATIC_ASSERT(REGISTER_OFFSET == MMIOREGISTER_OFFSET, check_register_offset);

#define VENDOR_ID_S3 0x5333

#define DAC_MASK  0x3C6
#define DAC_WR_AD 0x3C8
#define DAC_DATA  0x3C9

#define SDAC_COMMAND   0x3C6
#define SDAC_RD_ADR    0x3C7
#define SDAC_WR_ADR    0x3C8
#define SDAC_PLL_PARAM 0x3C9

// Beware: if we ever add a new chip family, make sure to check the chip library loading code
// in InitCard, because its using static arrays indexed by this enum!
typedef enum ChipFamily
{
    UNKNOWN,
    VISION864,   // pre-Trio64, separate RAMDAC, oldstyle MMIO, no packed MMIO
    TRIO64,      // integrated 135Mhz RAMDAC, oldstyle+packed MMIO
    VISION968,   // pre-Trio64(?), separate RAMDAC, newstyle MMIO, packed MMIO, ROPBLT
    TRIO64PLUS,  // integrated 135Mhz RAMDAC, newstyle+packed MMIO
    TRIO64V2,    // integrated 170Mhz RAMDAC, newstyle+packed MMIO 60-66Mhz RAM
} ChipFamily_t;

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
    BYTE *legacyIOBase;  // legacy I/O base address
    struct Library *OpenPciBase;
    struct pci_dev *board;
    struct Node boardNode;
} CardData_t;

STATIC_ASSERT(sizeof(CardData_t) < SIZEOF_MEMBER(BoardInfo_t, CardData), check_carddata_size);

extern ChipFamily_t getChipFamily(UWORD deviceId, UWORD revision);
extern const char *getChipFamilyName(ChipFamily_t family);
extern BOOL initRegisterAndMemoryBases(BoardInfo_t *bi);

#ifdef MMIO_ONLY  // On S3Trio there's a couple of 32bit registers accessed through I/O only
#undef W_IO_L
#endif

#endif  // S3TRIO64_COMMON_H
