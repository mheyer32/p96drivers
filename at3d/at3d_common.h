#ifndef AT3D_COMMON_H
#define AT3D_COMMON_H

#include "common.h"
#include "edid_common.h"

#include <exec/types.h>

#define VENDOR_ID_ALLIANCE 0x1142

typedef enum ChipFamily
{
    UNKNOWN,
    AT6422,  // Alliance ProMotion 6422
    AT6424,  // Alliance ProMotion 6424
    AT24,    // Alliance ProMotion AT24
    AT25,    // Alliance AT25
    AT3D,    // Alliance ProMotion AT3D
} ChipFamily_t;

typedef struct AT3DPLLValue
{
    UWORD freq10khz;  // frequency in 10 kHz units (e.g. 2517 = 25.17 MHz), fits up to ~17500
    UBYTE n;          // N numerator (8-127)
    UBYTE m;          // M denominator (1-5)
    UBYTE l;          // L postscaler log2 (0-3)
    UBYTE f;          // frequency range setting
} AT3DPLLValue_t;

typedef struct ChipData
{
    UBYTE GEOp;  // programmed graphics engine setup
    UBYTE GEFormat;
    UWORD GEbytesPerRow;  // programmed graphics engine bytes per row
    ULONG GEfgPen;
    ULONG GEdrawCmd;
    ULONG GEbgPen;
    UBYTE GEbppLog2;   // programmed graphics engine bpp
    UBYTE GElinear;   // programmed graphics engine linear/xy address model
    UBYTE GEopCode;   // programmed minTerm
    UBYTE memFormat;   // programmed memory layout/format
    UBYTE chipFamily;  // chip family
    UBYTE pad;
    UWORD pattX;                // x offset in pattern
    UWORD pattY;                // y offset in pattern
    ULONG *patternVideoBuffer;  // points to video memory
    UWORD *patternCacheBuffer;  // points to system memory
    ULONG patternCacheKey;

    // PLL table for pixel clocks
    AT3DPLLValue_t *pllValues;
    UWORD numPllValues;

} ChipData_t;

STATIC_ASSERT(sizeof(ChipData_t) <= sizeof(((BoardInfo_t *)0)->ChipData), ChipData_t_too_large);

typedef struct CardData
{
    BYTE *legacyIOBase;  // legacy I/O base address (not used for AT3D)
    struct Library *OpenPciBase;
    struct pci_dev *board;
    struct Node boardNode;
    char boardName[16];
    I2COps_t i2cOps;  // I2C operations for EDID support
} CardData_t;

STATIC_ASSERT(sizeof(CardData_t) < SIZEOF_MEMBER(BoardInfo_t, CardData), check_carddata_size);

extern ChipFamily_t getChipFamily(UWORD deviceId, UWORD revision);
extern const char *getChipFamilyName(ChipFamily_t family);
extern BOOL initRegisterAndMemoryBases(BoardInfo_t *bi);

#endif  // AT3D_COMMON_H
