#ifndef S3TRIO64_COMMON_H
#define S3TRIO64_COMMON_H

#include "common.h"

#include <exec/types.h>

#define VENDOR_ID_S3 0x5333

// Beware: if we ever add a new chip family, make sure to check the chip library loading code
// in InitCard, because its using static arrays indexed by this enum!
typedef enum ChipFamily
{
    UNKNOWN,
    VISION864,  // pre-Trio64, separate RAMDAC, oldstyle MMIO, no packed MMIO
    TRIO64,     // integrated RAMDAC, oldstyle+packed MMIO
    TRIO64PLUS  // integrated RAMDAC, newstyle+packed MMIO
} ChipFamily_t;

extern ChipFamily_t getChipFamily(UWORD deviceId, UWORD revision);
extern const char *getChipFamilyName(ChipFamily_t family);

#ifdef MMIO_ONLY // On S3Trio there's a couple of 32bit registers accessed through I/O only
#undef W_IO_L
#endif

#endif  // S3TRIO64_COMMON_H
