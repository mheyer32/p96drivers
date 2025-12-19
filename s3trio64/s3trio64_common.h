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
#if OPENPCI
    struct Library *OpenPciBase;
    struct pci_dev *board;
    struct Node boardNode;
    char boardName[16];
#else
    //  Expansion Library ConfigDev for Zorro cards (Cybervision64)
    struct ConfigDev *configDev;
    // This register controls the Cybervsion64 specific functions.
    // It is located at MemBase + 0x40001. It seems to only support writes, so shadow it here
    volatile UBYTE *cv64CtrlReg;
    UBYTE cv64Ctrl;
#endif
} CardData_t;

STATIC_ASSERT(sizeof(CardData_t) < SIZEOF_MEMBER(BoardInfo_t, CardData), check_carddata_size);

extern ChipFamily_t getChipFamily(UWORD deviceId, UWORD revision);
extern const char *getChipFamilyName(ChipFamily_t family);
extern BOOL initRegisterAndMemoryBases(BoardInfo_t *bi);

#define LEGACYIOBASE() volatile UBYTE *RegBase = getCardData(bi)->legacyIOBase

#ifdef CONFIG_CYBERVISION64
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Cybervision64 specific stuff
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static INLINE UBYTE readCv64CtrlRegister(const BoardInfo_t *bi)
{
    UBYTE value = getConstCardData(bi)->cv64Ctrl;
    D(VERBOSE, "R CV64 -> 0x%02lx\n", (LONG)value);
    return value;
}
static INLINE void writeCv64CtrlRegister(BoardInfo_t *bi, UBYTE value)
{
    getCardData(bi)->cv64Ctrl = value;
    writeRegister(getCardData(bi)->cv64CtrlReg, 0, value, "CV64");
}

#define R_CV64()                 readCv64CtrlRegister(bi)
#define W_CV64(value)            writeCv64CtrlRegister((bi), value)
#define W_CV64_MASK(mask, value) writeCv64CtrlRegister((bi), (readCv64CtrlRegister(bi) & ~(mask)) | ((value) & (mask)))

#define CV64_RESET_BIT          0x04  // 0 = Reset, 1 = Normal
#define CV64_MONITOR_SWITCH_BIT 0x10  // 0 = show passthrough (Amiga), 1 = show Cybervision
#define CV64_SWAP16_BIT         0x20  // 0 = no word swap, 1 = word swap
#define CV64_SWAP32_BIT         0x40  // 0 = no dword swap, 1 = dword swap
#endif  // CONFIG_CYBERVISION64

#endif  // S3TRIO64_COMMON_H
