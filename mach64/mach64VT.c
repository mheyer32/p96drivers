#include "mach64VT.h"
#include "mach64_common.h"
#include "chip_mach64.h"



ULONG ComputeFrequencyKhz10(UWORD R, UWORD N, UWORD M, UBYTE Plog2)
{
    return ((ULONG)2 * R * N) / (M << Plog2);
}

ULONG ComputeFrequencyKhz10FromPllValue(const BoardInfo_t *bi, const PLLValue_t *pllValues)
{
    const ChipData_t *cd = getConstChipData(bi);
    return ComputeFrequencyKhz10(cd->referenceFrequency, pllValues->N, cd->referenceDivider, pllValues->Pidx);
}

ULONG ComputePLLValues(const BoardInfo_t *bi, ULONG targetFreqKhz10, PLLValue_t *pllValues)
{
    DFUNC(VERBOSE, "targetFrequency: %ld0 KHz\n", targetFreqKhz10);

    UWORD R = getConstChipData(bi)->referenceFrequency;
    UWORD M = getConstChipData(bi)->referenceDivider;

    ULONG Qtimes2 = (targetFreqKhz10 * M + R - 1) / R;
    if (Qtimes2 >= 511) {
        goto failure;
    }

    UBYTE P = 0;
    if (Qtimes2 < 31)  // 16
        goto failure;
    else if (Qtimes2 < 63)  // 31.5
        P = 3; // 8x
    else if (Qtimes2 < 127)  // 63.5
        P = 2; // 4x
    else if (Qtimes2 < 255)  // 127.5
        P = 1; // 2x
    else if (Qtimes2 < 511)  // 255
        P = 0; // 1
    else
        goto failure;

            // Round up
    UBYTE N = ((Qtimes2 << P) + 1) >> 1;

    pllValues->N     = N;
    pllValues->Pidx = P;

    ULONG outputFreq = ComputeFrequencyKhz10(R, N, M, P);

    DFUNC(CHATTY, "target: %ld0 KHz, Output: %ld0 KHz, R: %ld0 KHz, M: %ld, P: %ld, N: %ld\n", targetFreqKhz10,
          outputFreq, R, M, (ULONG)1 << P, (ULONG)N);

    return outputFreq;

failure:
    DFUNC(ERROR, "Qtimes2: %ld\n", Qtimes2);
    DFUNC(ERROR, "target frequency out of range:  %ld0Khz\n", targetFreqKhz10);
    return 0;
}

static const UWORD defaultRegs_VT[] = {0x00a2, 0x6007,  //
                                       0x00a0, 0x20f8,  //
                                       0x0018, 0x0000,  //
                                       0x001c, 0x0200,  //
                                       0x001e, 0x040b,  //
                                       0x00d2, 0x0000,  //
                                       0x00e4, 0x0020,  //
                                       0x00b0, 0x0021,  //
                                       0x00b2, 0x0801,  //
                                       0x00d0, 0x0000,  //
                                       0x001e, 0x0000,  //
                                       0x0080, 0x0000,  //
                                       0x0082, 0x0000,  //
                                       0x0084, 0x0000,  //
                                       0x0086, 0x0000,  //
                                       0x00c4, 0x0000,  //
                                       0x00c6, 0x8000,  //
                                       0x007a, 0x0000,  //
                                       0x00d0, 0x0100};

BOOL InitMach64VT(struct BoardInfo *bi)
{
    REGBASE();

    WRITE_PLL(PLL_MACRO_CNTL, 0xa0);  // PLL_DUTY_CYC = 5

    // WRITE_PLL(PLL_VFC_CNTL, 0x1b);
    WRITE_PLL(PLL_VFC_CNTL, 0x03);

    WRITE_PLL(PLL_REF_DIV, getChipData(bi)->referenceDivider);
    WRITE_PLL(PLL_VCLK_CNTL, 0x00);

    // WRITE_PLL(PLL_FCP_CNTL, 0xc0);
    WRITE_PLL(PLL_FCP_CNTL, 0x40);

    WRITE_PLL(PLL_XCLK_CNTL, 0x00);
    WRITE_PLL(PLL_VCLK_POST_DIV, 0x9c);

    WRITE_PLL(PLL_VCLK_CNTL, 0x0b);
    WRITE_PLL(PLL_MACRO_CNTL, 0x90);

    WriteDefaultRegList(bi, defaultRegs_VT, ARRAY_SIZE(defaultRegs_VT));

    // These values are being set by the Mach64VT BIOS, but are inline with the code, not somewhere in an accessible
    // table. CONFIG_STAT0 is apparently not strapped to the right memory type and only by setting it here we get access
    // to the framebuffer memory.
    W_BLKIO_MASK_L(CONFIG_STAT0, CFG_MEM_TYPE_MASK | CFG_DUAL_CAS_EN_MASK | CFG_VGA_EN_MASK | CFG_CLOCK_EN_MASK,
                   CFG_MEM_TYPE(CFG_MEM_TYPE_PSEUDO_EDO) | CFG_DUAL_CAS_EN | CFG_CLOCK_EN);

    // W_BLKIO_MASK_B(CONFIG_STAT0, 0, ~0xf8, 0x3b);
    return TRUE;
}
