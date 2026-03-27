#ifndef CIRRUS_COMMON_H
#define CIRRUS_COMMON_H

#include "common.h"
#include "edid_common.h"

#include <exec/types.h>

#define VENDOR_ID_CIRRUS 0x1013

typedef enum ChipFamily
{
    UNKNOWN = 0,
    GD5430,   /* PCI 0x00A0 */
    GD5432,   /* PCI 0x00A2 */
    GD5434,   /* 0x00A4 / 0x00A8 variants per PCI ID */
    GD5436,   /* 0x00AC */
    GD5440,   /* 0x00B0 */
    GD5446,   /* 0x00B8 */
    GD5480,   /* 0x00BC */
} ChipFamily_t;

typedef struct CirrusPLLValue
{
    UWORD freq10khz;
    UBYTE nom;
    UBYTE den;
    UBYTE div; /* 0 or 1: extra /2 */
    UBYTE pad;
} CirrusPLLValue_t;

typedef struct ChipData
{
    UBYTE GEFormat;
    UBYTE GEbppLog2;
    UBYTE memFormat;
    UBYTE chipFamily;

    CirrusPLLValue_t *pllValues;
    UWORD numPllValues;
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
#include "cirrus_driver.hpp"
#endif

#ifdef __cplusplus
extern "C" {
#endif
ChipFamily_t getChipFamily(UWORD deviceId);
const char *getChipFamilyName(ChipFamily_t family);
BOOL initRegisterAndMemoryBases(BoardInfo_t *bi);
void queryEDID(BoardInfo_t *bi);
#ifdef __cplusplus
}
#endif

#endif
