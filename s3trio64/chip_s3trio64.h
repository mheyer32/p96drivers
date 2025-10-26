#ifndef CHIP_S3TRIO64_H
#define CHIP_S3TRIO64_H

#include "s3trio64_common.h"

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
