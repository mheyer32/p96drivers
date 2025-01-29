#include "mach64VT.h"
#include "mach64_common.h"
#include "chip_mach64.h"

#define CRTC_FIFO_OVERFILL(x)   ((x) << 14)
#define CRTC_FIFO_OVERFILL_MASK (0x3 << 14)
#define CRTC_FIFO_LWM(x)        ((x) << 16)
#define CRTC_FIFO_LWM_MASK      (0xF << 16)
#define CRTC_DISPREQ_ONLY       BIT(21)
#define CRTC_DISPREQ_ONLY_MASK  BIT(21)

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

typedef struct
{
    ULONG reserved2 : 3;         // Bits 29-31: Reserved
    ULONG mem_oe_select : 2;     // Bits 27-28: OEb(0) Pin Function Select
    ULONG mem_pix_width : 3;     // Bits 24-26: Memory Pixel Width
    ULONG reserved : 1;          // Bit 23: Reserved
    ULONG cde_pullback : 1;      // Bit 22: CDE Pullback
    ULONG low_latency_mode : 1;  // Bit 21: Low Latency Mode
    ULONG mem_tile_select : 2;   // Bits 19-20: Memory Tile Select
    ULONG mem_sdram_reset : 1;   // Bit 18: SDRAM Reset
    ULONG dll_gain_cntl : 2;     // Bits 16-17: DLL Gain Control
    ULONG mem_actv_pre : 2;      // Bits 14-15: SDRAM Active to Precharge Minimum Latency
    ULONG dll_reset : 1;         // Bit 13: DLL Reset
    ULONG mem_refresh_rate : 2;  // Bits 11-12: Memory refresh rate
    ULONG mem_cyc_lnth : 2;      // Bits 9-10: DRAM non-page memory cycle length
    ULONG mem_cyc_lnth_aux : 2;  // Bits 7-8: SDRAM Actv to Cmd cycle length
    ULONG mem_refresh : 4;       // Bits 3-6: Refresh Cycle Length
    ULONG mem_size : 3;          // Bits 0-2: Memory size
} MEM_CNTL_Register;

static void print_MEM_CNTL_Register(const MEM_CNTL_Register *reg)
{
    D(0, "MEM_CNTL_Register:\n");
    D(0, "  mem_size         : 0x%lx\n", reg->mem_size);
    D(0, "  mem_refresh      : 0x%lx\n", reg->mem_refresh);
    D(0, "  mem_cyc_lnth_aux : 0x%lx\n", reg->mem_cyc_lnth_aux);
    D(0, "  mem_cyc_lnth     : 0x%lx\n", reg->mem_cyc_lnth);
    D(0, "  mem_refresh_rate : 0x%lx\n", reg->mem_refresh_rate);
    D(0, "  dll_reset        : 0x%lx\n", reg->dll_reset);
    D(0, "  mem_actv_pre     : 0x%lx\n", reg->mem_actv_pre);
    D(0, "  dll_gain_cntl    : 0x%lx\n", reg->dll_gain_cntl);
    D(0, "  mem_sdram_reset  : 0x%lx\n", reg->mem_sdram_reset);
    D(0, "  mem_tile_select  : 0x%lx\n", reg->mem_tile_select);
    D(0, "  low_latency_mode : 0x%lx\n", reg->low_latency_mode);
    D(0, "  cde_pullback     : 0x%lx\n", reg->cde_pullback);
    D(0, "  reserved         : 0x%lx\n", reg->reserved);
    D(0, "  mem_pix_width    : 0x%lx\n", reg->mem_pix_width);
    D(0, "  mem_oe_select    : 0x%lx\n", reg->mem_oe_select);
    D(0, "  reserved2        : 0x%lx\n", reg->reserved2);
}
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

     W_BLKIO_MASK_L(BUS_CNTL, BUS_ROM_DIS_MASK, 0);

    MEM_CNTL_Register memCntl;
    *(ULONG *)&memCntl = R_BLKIO_L(MEM_CNTL);
    print_MEM_CNTL_Register(&memCntl);

    return TRUE;
}
