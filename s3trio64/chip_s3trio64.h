#ifndef CHIP_S3TRIO64_H
#define CHIP_S3TRIO64_H

#if defined(CONFIG_VISION864)

// Vision864 cannot read from MMIO, just write
// This define indicates multiple things, like external RAMDAC; should probably find a different name
#define BUILD_VISION864 1

#define MMIO_ONLY       0
#define BIGENDIAN_MMIO  0
#define BIGENDIAN_IO    0
#define HAS_PACKED_MMIO 0

#elif defined(CONFIG_S3TRIO3264)

#define MMIO_ONLY       0
#define BIGENDIAN_MMIO  0
#define BIGENDIAN_IO    0
#define HAS_PACKED_MMIO 1
#define BUILD_VISION864 0

#elif defined(CONFIG_S3TRIO64PLUS)

#define MMIO_ONLY       0
#define BIGENDIAN_MMIO  1
#define BIGENDIAN_IO    (BIGENDIAN_MMIO && MMIO_ONLY)
#define HAS_PACKED_MMIO 1
#define BUILD_VISION864 0

#elif defined(CONFIG_S3TRIO64V2)

#define MMIO_ONLY       1
#define BIGENDIAN_MMIO  1
#define BIGENDIAN_IO    (BIGENDIAN_MMIO && MMIO_ONLY)
#define HAS_PACKED_MMIO 1
#define BUILD_VISION864 0

#else
#pragma GCC error "no CONFIG_xxx defined"
#endif

// include after setting the above defines
#include "s3trio64_common.h"

#if MMIO_ONLY  // On S3Trio there's a couple of 32bit registers accessed through I/O only
#undef W_IO_L
#endif

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

#define LEGACYIOBASE()     volatile UBYTE *RegBase = getCardData(bi)->legacyIOBase
#define R_BEE8(idx)        readBEE8(RegBase, idx)
#define W_BEE8(idx, value) W_MMIO_W(0xBEE8, ((idx << 12) | value))

#endif
