#ifndef CHIP_ET4000_H
#define CHIP_ET4000_H

// include after setting the above defines
#include "et4000_common.h"

// FIXME: in theory 21xA and 21xB these are strapped by IOD<2:0> and thus may not be fixed.
#define CRTCB_IDX  0x217A
#define CRTCB_DATA 0x217B

#define DAC_STATE DAC_RD_AD  // Read DAC State Register(?)

static INLINE UBYTE REGARGS readCRBx(volatile UBYTE *regbase, UBYTE regIndex)
{
    writeReg(regbase, CRTCB_IDX, regIndex);
    UBYTE value = readReg(regbase, CRTCB_DATA);

    D(VERBOSE, "R CRB_%lX -> 0x%02lx\n", (LONG)regIndex, (LONG)value);
    return value;
}

static INLINE void REGARGS writeCRBx(volatile UBYTE *regbase, UBYTE regIndex, UBYTE value)
{
    writeReg(regbase, CRTCB_IDX, regIndex);
    writeReg(regbase, CRTCB_DATA, value);
    // FIXME: this doesn't work. I was hoping to write CRTC_IDX and CRTC_DATA in
    // one go
    //  writeRegW(regbase, CRTC_IDX, (regIndex << 8) | value );

    D(VERBOSE, "W CRB_%lX <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
}

static INLINE void REGARGS writeCRBxMask(volatile UBYTE *regbase, UBYTE regIndex, UBYTE mask, UBYTE value)
{
    UBYTE regvalue = (readCRBx(regbase, regIndex) & ~mask) | (value & mask);
    // Keep index register from previous read
    writeReg(regbase, CRTCB_DATA, regvalue);

    D(VERBOSE, "W CRB_%lX <- 0x%02lx\n", (LONG)regIndex, (LONG)regvalue);
}

#define R_CRB(reg)                   readCRBx(RegBase, reg)
#define W_CRB(reg, value)            writeCRBx(RegBase, reg, value)
#define W_CRB_MASK(reg, mask, value) writeCRBxMask(RegBase, reg, mask, value)

#endif
