#ifndef ET4000_COMMON_H
#define ET4000_COMMON_H

#include "common.h"

#include <exec/types.h>

STATIC_ASSERT(REGISTER_OFFSET == MMIOREGISTER_OFFSET, check_register_offset);

#define VENDOR_ID_TSENG 0x100C

typedef enum ChipFamily
{
    ET4000_UNKNOWN,
    ET4000_W32P,
} ChipFamily_t;

typedef struct ChipData
{
    ULONG GEfgPen;
    ULONG GEbgPen;

    UWORD GEbytesPerRow;
    UWORD GEsegs;

    UBYTE GEOp;
    UBYTE GEFormat;

    UBYTE GEbpp;
    UBYTE GEmask;

    UBYTE GEdrawMode;
    UBYTE MemFormat;
    UBYTE chipFamily;
    UBYTE hasCH8398;
    UWORD pattSegment;
    UWORD pattX;
    UWORD pattY;
    ULONG *patternVideoBuffer;
    UWORD *patternCacheBuffer;
    ULONG patternCacheKey;

} ChipData_t;

STATIC_ASSERT(sizeof(ChipData_t) <= sizeof(((BoardInfo_t *)0)->ChipData), ChipData_t_too_large);

typedef struct CardData
{
    volatile UBYTE *legacyIOBase;
    struct Library *OpenPciBase;
    struct pci_dev *board;
    struct Node boardNode;
    char boardName[16];
} CardData_t;

STATIC_ASSERT(sizeof(CardData_t) < SIZEOF_MEMBER(BoardInfo_t, CardData), check_carddata_size);

#ifdef __cplusplus
#include "et4000_driver.hpp"
#endif

#ifdef __cplusplus
extern "C" {
#endif
ChipFamily_t getChipFamily(UWORD deviceId, UWORD revision);
const char *getChipFamilyName(ChipFamily_t family);
BOOL initRegisterAndMemoryBases(BoardInfo_t *bi);
BOOL InitChip(__REGA0(struct BoardInfo *bi));
#ifdef __cplusplus
}
#endif

#endif
