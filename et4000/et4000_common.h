#ifndef ET4000_COMMON_H
#define ET4000_COMMON_H

#include "common.h"

#include <exec/types.h>

STATIC_ASSERT(REGISTER_OFFSET == MMIOREGISTER_OFFSET, check_register_offset);

#define VENDOR_ID_TSENG 0x100C

// Tseng ET4000/6000 chip variants
typedef enum ChipFamily
{
    ET4000_UNKNOWN,
    ET4000_W32P,
} ChipFamily_t;

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
    UBYTE chipFamily;           // chip family
    UBYTE hasCH8398;            // CH8398 RAMDAC detected
    UWORD pattSegment;          // segment where pattern is stored
    UWORD pattX;                // x offset in pattern
    UWORD pattY;                // y offset in pattern
    ULONG *patternVideoBuffer;  // points to video memory
    UWORD *patternCacheBuffer;  // points to system memory
    ULONG patternCacheKey;

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

#endif  // ET4000_COMMON_H
