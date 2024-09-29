#include "chip_mach64.h"

#include <graphics/rastport.h>
#include <proto/prometheus.h>

#include <string.h>  // memcmp

#define PCI_VENDOR 0x1002

/******************************************************************************/
/*                                                                            */
/* library exports                                                                    */
/*                                                                            */
/******************************************************************************/

const char LibName[]     = "ATIMach64.chip";
const char LibIdString[] = "ATIMach64 Picasso96 chip driver version 1.0";

const UWORD LibVersion  = 1;
const UWORD LibRevision = 0;

/*******************************************************************************/

int debugLevel = VERBOSE;

static const struct svga_pll mach64_pll = {3, 129, 0x80, 0xFF, 0, 3, 100000, 200000, 14318};

static const UBYTE g_bitWidths[] = {
    COLOR_DEPTH_4,   // RGBFB_NONE 4bit
    COLOR_DEPTH_8,   // RGBFB_CLUT
    COLOR_DEPTH_24,  // RGBFB_R8G8B8
    COLOR_DEPTH_24,  // RGBFB_B8G8R8
    COLOR_DEPTH_16,  // RGBFB_R5G6B5PC
    COLOR_DEPTH_15,  // RGBFB_R5G5B5PC
    COLOR_DEPTH_32,  // RGBFB_A8R8G8B8
    COLOR_DEPTH_32,  // RGBFB_A8B8G8R8
    COLOR_DEPTH_32,  // RGBFB_R8G8B8A8
    COLOR_DEPTH_32,  // RGBFB_B8G8R8A8
    COLOR_DEPTH_16,  // RGBFB_R5G6B5
    COLOR_DEPTH_15,  // RGBFB_R5G5B5
    COLOR_DEPTH_16,  // RGBFB_B5G6R5PC
    COLOR_DEPTH_15,  // RGBFB_B5G5R5PC
    COLOR_DEPTH_1,   // RGBFB_YUV422CGX
    COLOR_DEPTH_1,   // RGBFB_YUV411
    COLOR_DEPTH_1,   // RGBFB_YUV411PC
    COLOR_DEPTH_1,   // RGBFB_YUV422
    COLOR_DEPTH_1,   // RGBFB_YUV422PC
    COLOR_DEPTH_1,   // RGBFB_YUV422PA
    COLOR_DEPTH_1,   // RGBFB_YUV422PAPC
};

// FIXME: No good, need dynamic allocations. If the same chipdriver needs to talk to multiple
//  cards, these tables will have different values for each card
static PLLTable_t g_pllTable;
static MaxColorDepthTableEntry_t g_maxCDepthTable[50];
static MaxColorDepthTableEntry_t g_maxCDepthSecondTable[50];

void printFrequencyTable(const FrequencyTable_t *ft)
{
    D(VERBOSE, "Frequency Table ID: 0x%02lX\n", (ULONG)ft->frequency_table_id);
    D(VERBOSE, "Minimum PCLK Frequency: %lu KHz\n", (ULONG)swapw(ft->min_pclk_freq) * 10);
    D(VERBOSE, "Maximum PCLK Frequency: %lu KHz\n", (ULONG)swapw(ft->max_pclk_freq) * 10);
    D(VERBOSE, "Extended Coprocessor Mode: 0x%02lX\n", (ULONG)ft->extended_coprocessor_mode);
    D(VERBOSE, "Extended VGA Mode: 0x%02lX\n", (ULONG)ft->extended_vga_mode);
    D(VERBOSE, "Reference Clock Frequency: %lu KHz\n", (ULONG)swapw(ft->ref_clock_freq) * 10);
    D(VERBOSE, "Reference Clock Divider: %lu\n", (ULONG)swapw(ft->ref_clock_divider));
    D(VERBOSE, "Hardware Specific Information: 0x%04lX\n", (ULONG)swapw(ft->hardware_specific_info));
    D(VERBOSE, "MCLK Frequency (Power Down Mode): %lu KHz\n", (ULONG)swapw(ft->mclk_freq_power_down) * 10);
    D(VERBOSE, "MCLK Frequency (Normal DRAM Mode): %lu KHz\n", (ULONG)swapw(ft->mclk_freq_normal_dram) * 10);
    D(VERBOSE, "MCLK Frequency (Normal VRAM Mode): %lu KHz\n", (ULONG)swapw(ft->mclk_freq_normal_vram) * 10);
    D(VERBOSE, "SCLK Frequency: %lu KHz\n", (ULONG)swapw(ft->sclk_freq) * 10);
    D(VERBOSE, "MCLK Entry Number: 0x%02lX\n", ft->mclk_entry_num);
    D(VERBOSE, "SCLK Entry Number: 0x%02lX\n", ft->sclk_entry_num);
    if (ft->coprocessor_mode_mclk_freq != 0) {
        D(0, "Coprocessor Mode MCLK Frequency: %lu KHz\n", (ULONG)swapw(ft->coprocessor_mode_mclk_freq) * 10);
    }
    D(VERBOSE, "Reserved: 0x%04lX\n", (ULONG)swapw(ft->reserved));
    D(VERBOSE, "Terminator: 0x%04lX\n", (ULONG)swapw(ft->terminator));
}

UBYTE getDACType(BoardInfo_t *bi)
{
    REGBASE();
    return (R_BLKIO_B(DAC_CNTL, 2) & 0x7);
}

void printPLLTable(const PLLTable_t *pllTable)
{
    for (int i = 0; i < 20; i++) {
        D(VERBOSE, "PLL[%ld] = %lu\n", i, (ULONG)pllTable->pllValues[i]);
    }
}

void printCdepthTable(const MaxColorDepthTableEntry_t *table)
{
    for (int i = 0; table[i].h_disp != 0; i++) {
        D(VERBOSE, "MaxColorDepthTableEntry[%ld]:\n", i);
        D(VERBOSE, "  h_disp: %lu\n", table[i].h_disp);
        D(VERBOSE, "  dacmask: %lu\n", table[i].dacmask);
        D(VERBOSE, "  ram_req: %lu\n", table[i].ram_req);
        D(VERBOSE, "  max_dot_clk: %lu\n", table[i].max_dot_clk);
        D(VERBOSE, "  color_depth: %lu\n", table[i].color_depth);
    }
}

const Mach64RomHeader_t *findRomHeader(struct BoardInfo *bi)
{
#define ROM_WORD(offset)              (swapw(*(UWORD *)(romBase + (offset))))
#define ROM_BYTE(offset)              (*(romBase + (offset)))
#define ROM_TABLE(name, type, offset) const type *name = (type *)(romBase + (offset))

    LOCAL_PROMETHEUSBASE();

    UBYTE *romBase = NULL;
    Prm_GetBoardAttrsTags((PCIBoard *)bi->CardPrometheusDevice, PRM_ROM_Address, (ULONG)&romBase, TAG_END);
    if (!romBase)
        return NULL;

    ROM_TABLE(romHeader, OptionRomHeader_t, 0);
    if (swapw(romHeader->signature) != 0xaa55)
        return NULL;

    ROM_TABLE(pciData, PciRomData_t, swapw(romHeader->pcir_offset));
    if (memcmp(pciData->signature, "PCIR", 4) != 0)
        return NULL;

    WORD atiRomHeaderOffset = ROM_WORD(0x48);

    ROM_TABLE(mach64RomHeader, Mach64RomHeader_t, atiRomHeaderOffset - 2);

    const char *logOnMessage = romBase + swapw(mach64RomHeader->logon_message_ptr);
    // Doesn't seem to point to anything useful
    const char *configString = romBase + swapw(mach64RomHeader->config_string_ptr);

    UWORD ioBase = swapw(mach64RomHeader->io_base_address);

    D(0, "ATI Mach64 ROM header found at offset 0x%lx, Block IO Base Address 0x%lx, \n%s\n", (ULONG)atiRomHeaderOffset,
      (ULONG)ioBase, logOnMessage);

    USHORT freqTableOffset = swapw(mach64RomHeader->freq_table_ptr);
    ROM_TABLE(freqTable, FrequencyTable_t, freqTableOffset);
    printFrequencyTable(freqTable);

    USHORT pllTableOffset = ROM_WORD(freqTableOffset - 2);
    ROM_TABLE(pllTable, PLLTable_t, pllTableOffset);
    for (int i = 0; i < 20; i++) {
        g_pllTable.pllValues[i] = swapw(pllTable->pllValues[i]);
    }
    printPLLTable(&g_pllTable);

    getChipData(bi)->referenceFrequency = swapw(freqTable->ref_clock_freq);
    getChipData(bi)->referenceDivider   = swapw(freqTable->ref_clock_divider);
    getChipData(bi)->memClock           = swapw(freqTable->mclk_freq_normal_dram);
    getChipData(bi)->minPClock          = swapw(freqTable->min_pclk_freq);
    getChipData(bi)->maxPClock          = swapw(freqTable->max_pclk_freq);

    USHORT cdepthTableOffset = ROM_WORD(freqTableOffset - 6);
    ROM_TABLE(cdepthTable, MaxColorDepthTableEntry_t, cdepthTableOffset);
    UBYTE entrySize = ROM_BYTE(cdepthTableOffset - 1);

    UBYTE dacType = getDACType(bi);
    {
        int e;
        for (e = 0; ROM_BYTE(cdepthTableOffset) != 0; cdepthTableOffset += entrySize) {
            ROM_TABLE(cdepthTable, MaxColorDepthTableEntry_t, cdepthTableOffset);
            if (dacType & cdepthTable->dacmask) {
                // No endian conversion; works because all struct members are byte
                g_maxCDepthTable[e] = *cdepthTable;
                ++e;
            }
        }
        // Mark end of list
        g_maxCDepthTable[e].h_disp = 0;
    }

    // printCdepthTable(g_maxCDepthTable);

    if (ROM_BYTE(cdepthTableOffset + 1) == 0) {
        // Secondary table
        cdepthTableOffset += 2;
        int e;
        for (e = 0; ROM_BYTE(cdepthTableOffset) != 0; cdepthTableOffset += entrySize) {
            ROM_TABLE(cdepthTable, MaxColorDepthTableEntry_t, cdepthTableOffset);
            if (dacType == cdepthTable->dacmask) {
                // No endian conversion; works because all struct members are byte
                g_maxCDepthSecondTable[e] = *cdepthTable;
                ++e;
            }
        }
        // Mark end of list
        g_maxCDepthSecondTable[e].h_disp = 0;

        // printCdepthTable(g_maxCDepthSecondTable);
    }

    return mach64RomHeader;
}

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

enum PLL_REGS
{
    PLL_MACRO_CNTL    = 1,
    PLL_REF_DIV       = 2,
    PLL_GEN_CNTL      = 3,
    PLL_MCLK_FB_DIV   = 4,
    PLL_VCLK_CNTL     = 5,
    PLL_VCLK_POST_DIV = 6,
    PLL_VCLK0_FB_DIV  = 7,
    PLL_VCLK1_FB_DIV  = 8,
    PLL_VCLK2_FB_DIV  = 9,
    PLL_VCLK3_FB_DIV  = 10,
    PLL_XCLK_CNTL     = 11,
    PLL_FCP_CNTL      = 12,
    PLL_VFC_CNTL      = 13,
};

#define PLL_ADDR_MASK      (0xF << 10)
#define PLL_ADDR(x)        ((x) << 10)
#define PLL_DATA_MASK      (0xFF << 16)
#define PLL_DATA(x)        ((x) << 16)
#define PLL_WR_ENABLE      BIT(9)
#define PLL_WR_ENABLE_MASK BIT(9)
#define CLOCK_SEL_MASK     (0x7)
#define CLOCK_SEL(x)       ((x) & CLOCK_SEL_MASK)

void WritePLL(struct BoardInfo *bi, UBYTE pllAddr, UBYTE pllDataMask, UBYTE pllData)
{
    REGBASE();

    DFUNC(8, "pllAddr: %d, pllDataMask: 0x%02X, pllData: 0x%02X\n", (ULONG)pllAddr, (ULONG)pllDataMask, (ULONG)pllData);

    // FIXME: its possible older Mach chips want 8bit access here
    ULONG oldClockCntl = R_BLKIO_AND_L(CLOCK_CNTL, ~(PLL_ADDR_MASK | PLL_DATA_MASK | PLL_WR_ENABLE_MASK));

    ULONG clockCntl = oldClockCntl;
    // Set PLL Adress
    clockCntl |= PLL_ADDR(pllAddr);
    W_BLKIO_L(CLOCK_CNTL, clockCntl);
    // Read back old data
    clockCntl = R_BLKIO_L(CLOCK_CNTL);
    // write new PLL_DATA
    clockCntl &= ~PLL_DATA(pllDataMask);
    clockCntl |= PLL_DATA(pllData & pllDataMask);
    clockCntl |= PLL_WR_ENABLE;
    W_BLKIO_L(CLOCK_CNTL, clockCntl);

    // Disable PLL_WR_EN again
    W_BLKIO_L(CLOCK_CNTL, oldClockCntl);
}

UBYTE ReadPLL(struct BoardInfo *bi, UBYTE pllAddr)
{
    REGBASE();
    DFUNC(8, "ReadPLL: pllAddr: %d\n", (ULONG)pllAddr);

    // FIXME: its possible older Mach chips want 8bit access here
    ULONG clockCntl = R_BLKIO_AND_L(CLOCK_CNTL, ~(PLL_ADDR_MASK | PLL_DATA_MASK | PLL_WR_ENABLE_MASK));

    // Set PLL Adress
    clockCntl |= PLL_ADDR(pllAddr);
    W_BLKIO_L(CLOCK_CNTL, clockCntl);
    // Read back data
    clockCntl = R_BLKIO_L(CLOCK_CNTL);

    UBYTE pllValue = (clockCntl >> 16) & 0xFF;

    DFUNC(8, "pllValue: %d\n", (ULONG)pllValue);

    return pllValue;
}

#define WRITE_PLL(pllAddr, data)            WritePLL(bi, (pllAddr), 0xFF, (data))
#define WRITE_PLL_MASK(pllAddr, mask, data) WritePLL(bi, (pllAddr), (mask), (data))
#define READ_PLL(pllAddr)                   ReadPLL(bi, (pllAddr))

// Frequency Synthesis Description
// To generate a specific output frequency, the reference (M), feedback (N), and post
// dividers (P) must be loaded with the appropriate divide-down ratios. The internal PLLs for
// CT and ET are optimized to lock to output frequencies in the range from 135 MHz to 68
// MHz. The PLLs for other members of the mach64CT family are optimized to lock with
// output frequencies from 100 MHz to 200 MHz. Setting the PLLs to lock outside these
// ranges can result in increased jitter or total mis-function (no lock).
// The PLLREFCLK lower limit is found based on the upper limit of the PLL lock range
// (e.g. 135 MHz) and the maximum feedback divider (255) as follows:
// Minimum PLLREFCLK = 135 MHz / (2 * 255) = 265 kHz
// This is then used to find the reference divider based on the XTALIN frequency.
// XTALIN is normally 14.318 MHz and the maximum reference divider M is found by:
// M = Floor[ 14.318 MHz / 265 kHz ] = 54
// (the Floor function means round down)
// Using the maximum reference divider allowed (in this case is 54) ensures the best clock
// step resolution. However, lower reference dividers might be used to improve clock jitter.
// Feedback dividers (N) should kept in the range 80h to FFh. The effective feedback divider
// is twice the register setting due to the structure of the internal PLL. The post divider (P)
// may be either 1, 2, 4, or 8.
// To determine the N and P values to program for a target frequency, follow the procedure
// below (where R is the frequency of XTALIN and T is the target frequency):
//
// 1. Calculate the value of P. Find the value of Q from the equation below and use it
// to find P in the following table:
// Q = (T * M) / (2 * R)
//
// 2. Calculate the value of N by using the value of P obtained in step 1. N is given by:
//    N = Q * P
//    The result N is rounded to the nearest whole number
//
// 3. Determine the actual frequency. Given P and the rounded-off N, the actual output
//    frequency is found by:
//    Output_Frequency = (2 * R * N) / (M * P)

// Output_Frequency = (2 * R * N) / (M * P)
// R = Reference Frequency from BIOS
// M = Reference Divider from BIOS
//

static ULONG computeFrequencyKhz10(UWORD R, UWORD N, UWORD M, UBYTE Plog2)
{
    return ((ULONG)2 * R * N) / (M << Plog2);
}

static ULONG computeFrequencyKhz10FromPllValue(const BoardInfo_t *bi, const PLLValue_t *pllValues)
{
    const ChipData_t *cd = getConstChipData(bi);
    return computeFrequencyKhz10(cd->referenceFrequency, pllValues->N, cd->referenceDivider, pllValues->Plog2);
}

static ULONG computePLLValues(const BoardInfo_t *bi, ULONG targetFrequency, PLLValue_t *pllValues)
{
    DFUNC(VERBOSE, "bi %lx, targetFrequency: %ld0 KHz, pllValues %lx \n", bi, targetFrequency, pllValues);

    UWORD R = getConstChipData(bi)->referenceFrequency;
    UWORD M = getConstChipData(bi)->referenceDivider;

    ULONG Qtimes2 = (targetFrequency * M + R - 1) / R;
    if (Qtimes2 >= 511)
        goto failure;

    UBYTE P = 0;
    if (Qtimes2 < 31)  // 16
        goto failure;
    else if (Qtimes2 < 63)  // 31.5
        P = 3;
    else if (Qtimes2 < 127)  // 63.5
        P = 2;
    else if (Qtimes2 < 255)  // 127.5
        P = 1;
    else if (Qtimes2 < 511)  // 255
        P = 0;
    else
        goto failure;

    // Round up
    UBYTE N = ((Qtimes2 << P) + 1) >> 1;

    pllValues->N     = N;
    pllValues->Plog2 = P;

    ULONG outputFreq = computeFrequencyKhz10(R, N, M, P);

    DFUNC(CHATTY, "target: %ld0 KHz, Output: %ld0 KHz, R: %ld0 KHz, M: %ld, P: %ld, N: %ld\n", targetFrequency,
          outputFreq, R, M, (ULONG)1 << P, (ULONG)N);

    return outputFreq;

failure:
    DFUNC(ERROR, "target frequency out of range:  %ld0Khz\n", targetFrequency);
    return 0;
}

#define PLL_OVERRIDE      BIT(0)
#define PLL_OVERRIDE_MASK BIT(0)
#define PLL_MRESET        BIT(1)
#define PLL_MRESET_MASK   BIT(1)
#define OSC_EN            BIT(2)
#define OSC_EN_MASK       BIT(2)
#define MCLK_SRC_SEL(x)   (((x) & 7) << 4)
#define MCLK_SRC_SEL_MASK (7 << 4)

#define MFB_TIMES_4_2b      BIT(2)
#define MFB_TIMES_4_2b_MASK BIT(2)

void printMemoryClock(BoardInfo_t *bi)
{
    UBYTE refDiv     = READ_PLL(PLL_REF_DIV);
    UBYTE fbDiv      = READ_PLL(PLL_MCLK_FB_DIV);
    UBYTE mClkSrcSel = (READ_PLL(PLL_GEN_CNTL) >> 4) & 7;

    const char *mClockSrc = "PLLMCLK";
    switch (mClkSrcSel) {
    case 0b100:
        mClockSrc = "CPUCLK";
        break;
    case 0b101:
        mClockSrc = "DCLK";
        break;
    case 0b110:
        mClockSrc = "PLLREFCLK";
        break;
    case 0b111:
        mClockSrc = "XTALIN";
        break;
    default:
        break;
    }

    mClkSrcSel &= 3;

    ULONG xclkCntl = READ_PLL(PLL_XCLK_CNTL);
    if (xclkCntl & MFB_TIMES_4_2b) {
        fbDiv <<= 1;
    }
    ULONG mClock = computeFrequencyKhz10(getChipData(bi)->referenceFrequency, fbDiv, refDiv, mClkSrcSel);

    DFUNC(5, "clock source: %s, PLL frequency: %ld0 KHz, R: %ld0 KHz, M: %ld, P: %ld, N: %ld\n", mClockSrc, mClock,
          (ULONG)getChipData(bi)->referenceFrequency, (ULONG)refDiv, (ULONG)1 << mClkSrcSel, (ULONG)fbDiv);
}

void SetMemoryClock(BoardInfo_t *bi, USHORT kHz10)
{
    DFUNC(5, "Setting memory clock to %ld0 KHz\n", (ULONG)kHz10);

    PLLValue_t pllValues;
    if (computePLLValues(bi, kHz10, &pllValues)) {
        UBYTE minN     = 0x80;
        ULONG xclkCntl = READ_PLL(PLL_XCLK_CNTL);
        if (xclkCntl & MFB_TIMES_4_2b) {
            minN = 0x40;
        }
        if (pllValues.N < minN) {
            DFUNC(WARN, "N value too low for MCLK: %ld\n", (ULONG)pllValues.N);
            return;
        }
        WRITE_PLL_MASK(PLL_GEN_CNTL, (PLL_OVERRIDE_MASK | PLL_MRESET_MASK | OSC_EN_MASK | MCLK_SRC_SEL_MASK),
                       (OSC_EN | MCLK_SRC_SEL(0b100)));
        WRITE_PLL(PLL_MCLK_FB_DIV, pllValues.N);
        delayMilliSeconds(5);
        WRITE_PLL_MASK(PLL_GEN_CNTL, MCLK_SRC_SEL_MASK, MCLK_SRC_SEL(pllValues.Plog2));
        printMemoryClock(bi);
    } else {
        DFUNC(0, "Unable to compute PLL values for %ld0 KHz\n", (ULONG)kHz10);
    }
}

BOOL TestMemory(BoardInfo_t bi) {}

static void InitPLLTable(BoardInfo_t *bi)
{
    DFUNC(5, "bi %p\n", bi);

    LOCAL_SYSBASE();

    ChipData_t *cd        = getChipData(bi);
    UWORD maxNumEntries   = (cd->maxPClock + 99) / 100 - (cd->minPClock + 99) / 100;
    PLLValue_t *pllValues = AllocVec(sizeof(PLLValue_t) * maxNumEntries, MEMF_PUBLIC);
    cd->pllValues         = pllValues;

    UWORD minFreq = cd->minPClock;
    UWORD e       = 0;
    for (; e < maxNumEntries; ++e) {
        ULONG frequency = computePLLValues(bi, minFreq, &pllValues[e]);
        if (!frequency) {
            DFUNC(0, "Unable to compute PLL values for %ld0 KHz\n", minFreq);
            break;
        } else {
            DFUNC(VERBOSE, "Pixelclock %03ld %09ldHz: \n", e, frequency * 10000);
        }
        minFreq += 100;
    }
    // See if we can squeeze the max frequency still in there
    if (e < maxNumEntries - 1 && minFreq < cd->maxPClock) {
        ULONG frequency = computePLLValues(bi, cd->maxPClock, &pllValues[e]);
        if (frequency) {
            ++e;
        }
    }

    memset(bi->PixelClockCount, 0, sizeof(bi->PixelClockCount));

    // FIXME: Account for OVERCLOCK
    for (UWORD i = 0; i < e; ++i) {
        ULONG frequency = computeFrequencyKhz10FromPllValue(bi, &pllValues[i]);
        D(VERBOSE, "Pixelclock %03ld %09ldHz: \n", i, frequency * 10000);
        if (frequency <= 13500) {
            bi->PixelClockCount[CHUNKY]++;
        }
        if (frequency <= 8000) {
            bi->PixelClockCount[HICOLOR]++;
            bi->PixelClockCount[TRUECOLOR]++;
            bi->PixelClockCount[TRUEALPHA]++;
        }
    }
}

void InitPLL(BoardInfo_t *bi)
{
    InitPLLTable(bi);

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
}

#define CFG_VGA_DIS            BIT(19)
#define CFG_VGA_DIS_MASK       BIT(19)
#define CFG_MEM_VGA_AP_EN      BIT(2)
#define CFG_MEM_VGA_AP_EN_MASK BIT(2)
#define CFG_MEM_AP_LOC(x)      ((x) << 4)
#define CFG_MEM_AP_LOC_MASK    (0x3FF << 4)

#define VGA_128KAP_PAGING      BIT(20)
#define VGA_128KAP_PAGING_MASK BIT(20)
#define CRTC_EXT_DISP_EN       BIT(24)
#define CRTC_EXT_DISP_EN_MASK  BIT(24)
#define VGA_ATI_LINEAR         BIT(27)
#define VGA_ATI_LINEAR_MASK    BIT(27)

#define CFG_MEM_TYPE(x)         ((x) & 7)
#define CFG_MEM_TYPE_MASK       (7)
#define CFG_MEM_TYPE_DISABLE    0b000
#define CFG_MEM_TYPE_DRAM       0b001
#define CFG_MEM_TYPE_EDO        0b010
#define CFG_MEM_TYPE_PSEUDO_EDO 0b011
#define CFG_MEM_TYPE_SDRAM      0b100
#define CFG_DUAL_CAS_EN         BIT(3)
#define CFG_DUAL_CAS_EN_MASK    BIT(3)
#define CFG_VGA_EN              BIT(4)
#define CFG_VGA_EN_MASK         BIT(4)
#define CFG_CLOCK_EN            BIT(5)
#define CFG_CLOCK_EN_MASK       BIT(5)

#define DAC_W_INDEX 0
#define DAC_W_DATA  1
#define DAC_MASK    2
#define DAC_R_INDEX 3

#define DAC_BLANKING      BIT(0)
#define DAC_BLANKING_MASK BIT(0)
#define DAC_8BIT_EN       BIT(8)
#define DAC_8BIT_EN_MASK  BIT(8)

static UWORD CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                  __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE format))
{
    // Pitch is a multiple of 8 bytes
    UBYTE bpp = getBPP(format);

    UWORD bytesPerRow = width * bpp;
    bytesPerRow       = (bytesPerRow + 7) & ~7;

    ULONG maxHeight = 2048;  // FIXME: check this value
    if (height > maxHeight) {
        return 0;
    }
    return bytesPerRow;
}

static void ASM SetColorArrayInternal(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count),
                                      __REGA1(const struct CLUTEntry *colors))
{
    REGBASE();

    const UBYTE bppDiff = 0;  // 8 - bi->BitsPerCannon;

    W_BLKIO_B(DAC_REGS, DAC_W_INDEX, startIndex);

    for (UWORD c = startIndex; c < startIndex + count; ++c) {
        writeReg(RegBase, DWORD_OFFSET(DAC_REGS) + DAC_W_DATA, colors[c].Red >> bppDiff);
        writeReg(RegBase, DWORD_OFFSET(DAC_REGS) + DAC_W_DATA, colors[c].Green >> bppDiff);
        writeReg(RegBase, DWORD_OFFSET(DAC_REGS) + DAC_W_DATA, colors[c].Blue >> bppDiff);
    }
}

static void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    DFUNC(5, "startIndex %ld, count %ld\n", (ULONG)startIndex, (ULONG)count);

    SetColorArrayInternal(bi, startIndex, count, bi->CLUT);
}

#define CRTC_DBL_SCAN_EN      BIT(0)
#define CRTC_INTERLACE_EN     BIT(1)
#define CRTC_HSYNC_DIS        BIT(2)
#define CRTC_VSYNC_DIS        BIT(3)
#define CRTC_DISPLAY_DIS      BIT(6)
#define CRTC_DISPLAY_DIS_MASK BIT(6)
#define CRTC_PIX_WIDTH(x)     ((x) << 8)
#define CRTC_PIX_WIDTH_MASK   (0x7 << 8)
#define CRTC_FIFO_LWM(x)      ((x) << 16)
#define CRTC_FIFO_LWM_MASK    (0xF << 16)
#define CRTC_EXT_DISP_EN      BIT(24)
#define CRTC_EXT_DISP_EN_MASK BIT(24)
#define CRTC_ENABLE           BIT(25)
#define CRTC_ENABLE_MASK      BIT(25)

static void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    REGBASE();

    DFUNC(5, "format %ld\n", (ULONG)format);

    W_BLKIO_MASK_L(CRTC_GEN_CNTL, CRTC_PIX_WIDTH_MASK, CRTC_PIX_WIDTH(g_bitWidths[format]));
    if (format != RGBFB_CLUT) {
        // I think in Hi-Color modes, the palette acts as gamma ramp
        struct CLUTEntry colors[256];
        for (int c = 0; c < 256; c++) {
            colors[c].Red   = c;
            colors[c].Green = c;
            colors[c].Blue  = c;
        }
        SetColorArrayInternal(bi, 0, 256, colors);
    } else {
        SetColorArrayInternal(bi, 0, 256, bi->CLUT);
    }

    return;
}

static inline REGARGS UWORD ToScanLines(UWORD y, UWORD modeFlags)
{
    if (modeFlags & GMF_DOUBLESCAN)
        y *= 2;
    if (modeFlags & GMF_INTERLACE)
        y /= 2;
    return y;
}

static inline REGARGS UWORD AdjustBorder(UWORD x, BOOL border, UWORD defaultX)
{
    if (!border || x == 0)
        x = defaultX;
    return x;
}

#define TO_CHARS(x)     ((x) >> 3)
#define TO_SCANLINES(y) ToScanLines((y), modeFlags)

#define CRTC_H_TOTAL(x)   (x)
#define CRTC_H_TOTAL_MASK (0x1FF)
#define CRTC_H_DISP(x)    ((x) << 16)
#define CRTC_H_DISP_MASK  (0xFF << 16)

#define CRTC_H_SYNC_STRT(x)      (x)
#define CRTC_H_SYNC_STRT_MASK    (0xFF)
#define CRTC_H_SYNC_DLY(x)       ((x) << 8)
#define CRTC_H_SYNC_DLY_MASK     (0x7 << 8)
#define CRTC_H_SYNC_STRT_HI(x)   ((x) << 12)
#define CRTC_H_SYNC_STRT_HI_MASK (0x1 << 12)
#define CRTC_H_SYNC_WID(x)       ((x) << 16)
#define CRTC_H_SYNC_WID_MASK     (0x1F << 16)
#define CRTC_H_SYNC_POL          BIT(21)

#define CRTC_V_TOTAL(x)   (x)
#define CRTC_V_TOTAL_MASK (0x7FF)
#define CRTC_V_DISP(x)    ((x) << 16)
#define CRTC_V_DISP_MASK  (0x7FF << 16)

#define CRTC_V_SYNC_STRT(x)   (x)
#define CRTC_V_SYNC_STRT_MASK (0x7FF)
#define CRTC_V_SYNC_WID(x)    ((x) << 16)
#define CRTC_V_SYNC_WID_MASK  (0x1F << 16)
#define CRTC_V_SYNC_POL       BIT(21)

#define CRTC_VLINE(x)        (x)
#define CRTC_VLINE_MASK      (0x7FF)
#define CRTC_CRNT_VLINE(x)   ((x) << 16)
#define CRTC_CRNT_VLINE_MASK (0x7FF << 16)

#define CRTC_OFFSET(x)   (x)
#define CRTC_OFFSET_MASK (0xFFFFF)
#define CRTC_PITCH(x)    ((x) << 22)
#define CRTC_PITCH_MASK  (0x3FF << 22)

#define OVR_CLR_8(x)   (x)
#define OVR_CLR_8_MASK (0xFF)
#define OVR_CLR_B(x)   ((x) << 8)
#define OVR_CLR_B_MASK (0xFF << 8)
#define OVR_CLR_G(x)   ((x) << 16)
#define OVR_CLR_G_MASK (0xFF << 16)
#define OVR_CLR_R(x)   ((x) << 24)
#define OVR_CLR_R_MASK (0xFF << 24)

#define CRTC_VBLANK      BIT(0)
#define CRTC_VBLANK_MASK BIT(0)

// in characters (pixels/8)
#define OVR_WID_LEFT(x)    (x)
#define OVR_WID_LEFT_MASK  (0xF)
#define OVR_WID_RIGHT(x)   ((x) << 16)
#define OVR_WID_RIGHT_MASK (0xF << 16)

// In scanlines
#define OVR_WID_TOP(x)   (x)
#define OVR_WID_TOP_MASK (0xFF)
#define OVR_WID_BOT(x)   ((x) << 16)
#define OVR_WID_BOT_MASK (0xFF << 16)

static void ASM SetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    REGBASE();

    BOOL isInterlaced;
    UBYTE modeFlags;

    DFUNC(INFO,
          "W %ld, H %ld,\n"
          "HTotal %ld, HBlankSize %ld, HSyncStart %ld, HSyncSize %ld,\n"
          "nVTotal %ld, VBlankSize %ld,  VSyncStart %ld ,  VSyncSize %ld\n",
          (ULONG)mi->Width, (ULONG)mi->Height, (ULONG)mi->HorTotal, (ULONG)mi->HorBlankSize, (ULONG)mi->HorSyncStart,
          (ULONG)mi->HorSyncSize, (ULONG)mi->VerTotal, (ULONG)mi->VerBlankSize, (ULONG)mi->VerSyncStart,
          (ULONG)mi->VerSyncSize);

    bi->ModeInfo = mi;
    bi->Border   = border;

    modeFlags    = mi->Flags;
    isInterlaced = !!(modeFlags & GMF_INTERLACE);

    UWORD hTotalChars = TO_CHARS(mi->HorTotal) - 1;
    D(VERBOSE, "Horizontal Total %ld\n", (ULONG)hTotalChars);
    UWORD hDisplay = TO_CHARS(mi->Width) - 1;
    D(VERBOSE, "Display %ld\n", (ULONG)hDisplay);
    W_BLKIO_L(CRTC_H_TOTAL_DISP, CRTC_H_TOTAL(hTotalChars) | CRTC_H_DISP(hDisplay));

    UWORD hSyncStart = TO_CHARS(mi->HorSyncStart + mi->Width) - 1;
    D(VERBOSE, "HSync start %ld\n", (ULONG)hSyncStart);

    ULONG crtcHSyncStrtWid =
        CRTC_H_SYNC_STRT(hSyncStart) | CRTC_H_SYNC_STRT_HI(hSyncStart >> 8) | CRTC_H_SYNC_WID(mi->HorSyncSize);

    if (modeFlags & GMF_HPOLARITY) {
        crtcHSyncStrtWid |= CRTC_H_SYNC_POL;
    }
    W_BLKIO_L(CRTC_H_SYNC_STRT_WID, crtcHSyncStrtWid);

    UWORD vTotal = TO_SCANLINES(mi->VerTotal) - 1;
    D(VERBOSE, "VTotal %ld\n", (ULONG)vTotal) - 1;
    UWORD vDisp = TO_SCANLINES(mi->Height) - 1;
    W_BLKIO_L(CRTC_V_TOTAL_DISP, CRTC_V_TOTAL(vTotal) | CRTC_V_DISP(vDisp));

    UWORD vSyncStart = TO_SCANLINES(mi->VerSyncStart + mi->Height) - 1;
    D(VERBOSE, "VSync Start %ld\n", (ULONG)vSyncStart);

    ULONG crtcVSyncStrtWid = CRTC_V_SYNC_STRT(vSyncStart) | CRTC_V_SYNC_WID(TO_SCANLINES(mi->VerSyncSize));
    if (modeFlags & GMF_VPOLARITY) {
        crtcVSyncStrtWid |= CRTC_V_SYNC_POL;
    }
    W_BLKIO_L(CRTC_V_SYNC_STRT_WID, crtcVSyncStrtWid);

    if (border) {
        UWORD hBorder = TO_CHARS(mi->HorBlankSize);
        UWORD vBorder = TO_SCANLINES(mi->VerBlankSize);
        W_BLKIO_L(OVR_WID_LEFT_RIGHT, OVR_WID_LEFT(hBorder) | OVR_WID_RIGHT(hBorder));
        W_BLKIO_L(OVR_WID_TOP_BOTTOM, OVR_WID_TOP(vBorder) | OVR_WID_BOT(vBorder));
    } else {
        W_BLKIO_L(OVR_WID_LEFT_RIGHT, 0);
        W_BLKIO_L(OVR_WID_TOP_BOTTOM, 0);
    }

    ULONG crtcGenCntl = R_BLKIO_L(CRTC_GEN_CNTL);
    crtcGenCntl &= ~(CRTC_DBL_SCAN_EN | CRTC_INTERLACE_EN);
    if (isInterlaced) {
        crtcGenCntl |= CRTC_INTERLACE_EN;
    }
    if (modeFlags & GMF_DOUBLESCAN) {
        crtcGenCntl |= CRTC_DBL_SCAN_EN;
    }
    W_BLKIO_L(CRTC_GEN_CNTL, crtcGenCntl);
}

static void ASM SetPanning(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD4(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset),
                           __REGD7(RGBFTYPE format))
{
    REGBASE();
    LOCAL_SYSBASE();

    DFUNC(INFO,
          "mem 0x%lx, width %ld, height %ld, xoffset %ld, yoffset %ld, "
          "format %ld\n",
          memory, (ULONG)width, (ULONG)height, (LONG)xoffset, (LONG)yoffset, (ULONG)format);

    if (width & 7) {
        D(0, "Panning pitch not a multiple of 8\n");
        return;
    }

    LONG panOffset;
    ULONG pitch;
    ULONG memOffset;

    bi->XOffset = xoffset;
    bi->YOffset = yoffset;
    memOffset   = (ULONG)memory - (ULONG)bi->MemoryBase;

    UBYTE bpp = getBPP(format);
    panOffset = (yoffset * width + xoffset) * bpp;

    pitch     = width / 8;                    // pitch in 8 pixels
    panOffset = (panOffset + memOffset) / 8;  // offset in 64bit words

    D(VERBOSE, "panOffset 0x%lx, pitch %ld qwords\n", panOffset, (ULONG)pitch);

    W_BLKIO_L(CRTC_OFF_PITCH, CRTC_OFFSET(panOffset) | CRTC_PITCH(pitch));

    return;
}

static APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi), __REGA1(APTR mem), __REGD7(RGBFTYPE format))
{
    DFUNC(VERBOSE, "mem 0x%lx, format %ld\n", mem, (ULONG)format);
    switch (format) {
    case RGBFB_A8R8G8B8:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        mem += 0x800000;
        D(CHATTY, "redirecting to  %ld\n", mem);
        // Redirect to Big Endian Linear Address Window.
        return mem;
        break;
    default:
        break;
    }
    return mem;
}

static ULONG ASM GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    if (format == RGBFB_NONE)
        return (ULONG)0;

    // These formats can always reside in the Little Endian Window.
    // We never need to change any aperture setting for them
    ULONG compatible = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_A8B8G8R8;

    switch (format) {
    case RGBFB_A8B8G8R8:
        // In Big Endian aperture, configured MEM_CNTL for byte swapping in long word
        compatible |= RGBFF_A8B8G8R8;
        break;
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        // In Big Endian aperture, configured MEM_CNTL for byte swapping in words only
        compatible |= RGBFF_R5G6B5 | RGBFF_R5G5B5;
        break;
    }
    return compatible;
}

static void ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    // Clocking Mode Register (ClK_MODE) (SR1)
    REGBASE();

    DFUNC(5, " state %ld\n", (ULONG)state);

    W_BLKIO_MASK_L(CRTC_GEN_CNTL, CRTC_DISPLAY_DIS_MASK, state ? 0 : CRTC_DISPLAY_DIS);
}

static ULONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi),
                                   __REGD0(ULONG pixelClock), __REGD7(RGBFTYPE RGBFormat))
{
    DFUNC(CHATTY, "ModeInfo 0x%lx pixelclock %ld, format %ld\n", mi, pixelClock, (ULONG)RGBFormat);

    const ChipData_t *cd = getChipData(bi);

    UWORD targetFreq = pixelClock / 10000;

    // find pixel clock in pllValues via bisection
    UWORD upper     = bi->PixelClockCount[CHUNKY] - 1;
    UWORD upperFreq = computeFrequencyKhz10FromPllValue(bi, &cd->pllValues[upper]);
    UWORD lower     = 0;
    UWORD lowerFreq = computeFrequencyKhz10FromPllValue(bi, &cd->pllValues[lower]);

    while (lower + 1 < upper) {
        UWORD middle     = (upper + lower) / 2;
        UWORD middleFreq = computeFrequencyKhz10FromPllValue(bi, &cd->pllValues[middle]);
        if (middleFreq < targetFreq) {
            lower     = middle;
            lowerFreq = middleFreq;
        } else {
            upper     = middle;
            upperFreq = middleFreq;
        }
    }
    // Return the best match between upper and lower
    if (targetFreq - lowerFreq > upperFreq - targetFreq) {
        lower     = upper;
        lowerFreq = upperFreq;
    }

    mi->PixelClock = lowerFreq * 10000;

    D(CHATTY, "Resulting pixelclock Hz: %ld\n\n", mi->PixelClock);

    mi->pll1.Numerator   = cd->pllValues[lower].N;
    mi->pll2.Denominator = cd->pllValues[lower].Plog2;

    return lower;
}

static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD7(RGBFTYPE format))
{
    DFUNC(5, "\n");

    const ChipData_t *cd = getChipData(bi);
    UWORD freq           = computeFrequencyKhz10FromPllValue(bi, &cd->pllValues[index]);

    return freq * 10000;
}

#define VCLK_SRC_SEL(x)   ((x))
#define VCLK_SRC_SEL_MASK (0x3)
#define PLL_PRESET        BIT(2)
#define PLL_PRESET_MASK   BIT(2)
#define VCLK0_POST_MASK   (0x3)
#define VCLK0_POST(x)     ((x) & 3)
#define VCLK1_POST_MASK   (0x3 << 2)
#define VCLK1_POST(x)     (((x) & 3) << 2)
#define VCLK2_POST_MASK   (0x3 << 4)
#define VCLK2_POST(x)     (((x) & 3) << 4)
#define VCLK3_POST_MASK   (0x3 << 6)
#define VCLK3_POST(x)     (((x) & 3) << 6)
#define DCLK_BY2_EN       BIT(7)
#define DCLK_BY2_EN_MASK  BIT(7)

static void ASM SetClock(__REGA0(struct BoardInfo *bi))
{
    REGBASE();

    DFUNC(0, "\n");

    struct ModeInfo *mi = bi->ModeInfo;
    ULONG pixelClock    = mi->PixelClock;

#ifdef DBG
    ULONG minVClkKhz10 = 2 * getChipData(bi)->referenceFrequency * 128 / (getChipData(bi)->referenceDivider * 8);

    D(10, "minimm VCLK %ldHz\n", minVClkKhz10 * 10000);
    if (pixelClock < minVClkKhz10 * 10000) {
        D(0, "PixelClock %ldHz is too low, minimum is %ldHz\n", pixelClock, minVClkKhz10 * 100000);
        return;
    }

    D(10, "SetClock: PixelClock %ldHz -> %ldHz \n", bi->ModeInfo->PixelClock, pixelClock);
    if (mi->pll1.Numerator < 0x80 || mi->pll1.Numerator > 0xFF) {
        D(0, "Invalid PLL1 Numerator %d\n", mi->pll1.Numerator);
        return;
    }
#endif

    // Select PLLVCLK as VCLK source
    WRITE_PLL_MASK(PLL_VCLK_CNTL, (PLL_PRESET_MASK | VCLK_SRC_SEL_MASK), VCLK_SRC_SEL(0b11) | PLL_PRESET);
    WRITE_PLL_MASK(PLL_FCP_CNTL, DCLK_BY2_EN_MASK, 0);
    WRITE_PLL(PLL_VCLK0_FB_DIV, mi->pll1.Numerator);
    WRITE_PLL_MASK(PLL_VCLK_POST_DIV, VCLK0_POST_MASK, VCLK0_POST(mi->pll2.Denominator));

    WRITE_PLL_MASK(PLL_VCLK_CNTL, PLL_PRESET_MASK, 0);

    delayMilliSeconds(5);

    // Select VLK0 as VCLK source
    W_BLKIO_MASK_L(CLOCK_CNTL, CLOCK_SEL_MASK, CLOCK_SEL(0));
}

#define MEM_PIX_WIDTH(x)   ((x) << 24)
#define MEM_PIX_WIDTH_MASK (0x7 << 24)

static inline void ASM SetMemoryModeInternal(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    REGBASE();
    DFUNC(VERBOSE, "format %ld\n", (ULONG)format);

    // These are the formats we place in the big endian aperture.
    // And only for those we have to do anything.
    switch (format) {
    case RGBFB_A8B8G8R8:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        if (getChipData(bi)->MemFormat == format) {
            return;
        }
        getChipData(bi)->MemFormat = format;
        W_BLKIO_MASK_L(MEM_CNTL, MEM_PIX_WIDTH_MASK, MEM_PIX_WIDTH(g_bitWidths[format]));
        break;
    }
}

static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    __asm __volatile("\t movem.l d0-d1/a0-a1,-(sp)\n"
                     : /* no result */
                     :
                     : "sp");

    SetMemoryModeInternal(bi, format);

    __asm __volatile("\t movem.l (sp)+,d0-d1/a0-a1\n"
                     : /* no result */
                     :
                     : "d0", "d1", "a0", "a1", "sp");
}

static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    __asm __volatile("\t movem.l d0-d1/a0-a1,-(sp)\n"
                     : /* no result */
                     :
                     : "sp");

    //  SetWriteMaskInternal(bi, format);

    __asm __volatile("\t movem.l (sp)+,d0-d1/a0-a1\n"
                     : /* no result */
                     :
                     : "d0", "d1", "a0", "a1", "sp");
}

static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask)) {}

static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask)) {}

static BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL expected))
{
    REGBASE();
    DFUNC(5, "\n");
    return (R_BLKIO_B(CRTC_INT_CNTL, 0) & CRTC_VBLANK) != 0;
}

static void WaitVerticalSync(__REGA0(struct BoardInfo *bi)) {}

#define CUR_OFFSET_X(x)   (x)
#define CUR_OFFSET_X_MASK (0xFFFFF)
#define CUR_HORZ_OFF(x)   ((x))
#define CUR_HORZ_OFF_MASK (0x3F)
#define CUR_VERT_OFF(x)   ((x) << 16)
#define CUR_VERT_OFF_MASK (0x3F << 16)

#define CUR_HORZ_POSN(x)   ((x))
#define CUR_HORZ_POSN_MASK (0x7FF)
#define CUR_VERT_POSN(x)   ((x) << 16)
#define CUR_VERT_POSN_MASK (0x7FF << 16)

static void ASM SetSpritePosition(__REGA0(struct BoardInfo *bi), __REGD0(WORD xpos), __REGD1(WORD ypos),
                                  __REGD7(RGBFTYPE fmt))
{
    DFUNC(5, "\n");
    REGBASE();

    bi->MouseX = xpos;
    bi->MouseY = ypos;

    WORD spriteX = xpos - bi->XOffset;
    WORD spriteY = ypos - bi->YOffset + bi->YSplit;

    WORD offsetX = 0;
    if (spriteX < 0) {
        if (spriteX > -64)
            offsetX = -spriteX;
        else
            offsetX = 64;
        spriteX = 0;
    }
    WORD offsetY = 0;
    if (spriteY < 0) {
        if (spriteY > -64)
            offsetY = -spriteY;
        else
            offsetY = 64;
        spriteY = 0;
    }

    // FIXME: what about BIB_DBLSCANDBLSPRITEY? Doesn't seem to do anything
    if (bi->ModeInfo->Flags & GMF_DOUBLESCAN) {
        spriteY *= 2;
    }

    D(5, "SpritePos X: %ld 0x%lx, Y: %ld 0x%lx\n", (LONG)spriteX, (ULONG)spriteX, (LONG)spriteY, (ULONG)spriteY);

    ULONG memOffset = (ULONG)bi->MouseImageBuffer - (ULONG)bi->MemoryBase;
    W_BLKIO_L(CUR_OFFSET, memOffset / 8);

    W_BLKIO_L(CUR_HORZ_VERT_POSN, CUR_HORZ_POSN(spriteX) | CUR_VERT_POSN(spriteY));
    W_BLKIO_L(CUR_HORZ_VERT_OFF, CUR_HORZ_OFF(offsetX) | CUR_VERT_OFF(offsetY));
}

ULONG spreadBits(ULONG word)
{
    // reverse the bits in the word
    word = ((word & 0xFF00) >> 8) | ((word & 0x00FF) << 8);
    word = ((word & 0xF0F0) >> 4) | ((word & 0x0F0F) << 4);
    word = ((word & 0xCCCC) >> 2) | ((word & 0x3333) << 2);
    word = ((word & 0xAAAA) >> 1) | ((word & 0x5555) << 1);

    // spread out the bits ( could probably be done more efficiently in combination with above)
    word = (word | (word << 8)) & 0x00FF00FF;
    word = (word | (word << 4)) & 0x0F0F0F0F;
    word = (word | (word << 2)) & 0x33333333;
    word = (word | (word << 1)) & 0x55555555;

    return word;
}

static void ASM SetSpriteImage(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE fmt))
{
    DFUNC(5, "\n");

    const UWORD *image = bi->MouseImage + 2;
    ULONG *cursor      = (ULONG *)bi->MouseImageBuffer;
    for (UWORD y = 0; y < bi->MouseHeight; ++y) {
        // first 16 bit
        ULONG plane0 = *image++;
        ULONG plane1 = *image++;

        plane0 = ~plane0;

        *cursor++ = swapl((spreadBits(plane0) << 1) | spreadBits((plane1)));
        *cursor++ = 0xAAAAAAAA;  // encodes 0b10 per cursor pixel (transparent)
        *cursor++ = 0xAAAAAAAA;
        *cursor++ = 0xAAAAAAAA;
    }
    // Pad the rest of the cursor image
    for (UWORD y = bi->MouseHeight; y < 64; ++y) {
        for (UWORD p = 0; p < 4; ++p) {
            *cursor++ = 0xAAAAAAAA;
        }
    }
}

#define CUR_CLR_8(x)   (x)
#define CUR_CLR_8_MASK (0xFF)
#define CUR_CLR_B(x)   ((x) << 8)
#define CUR_CLR_B_MASK (0xFF << 8)
#define CUR_CLR_G(x)   ((x) << 16)
#define CUR_CLR_G_MASK (0xFF << 16)
#define CUR_CLR_R(x)   ((x) << 24)
#define CUR_CLR_R_MASK (0xFF << 24)

static void ASM SetSpriteColor(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE index), __REGD1(UBYTE red),
                               __REGD2(UBYTE green), __REGD3(UBYTE blue), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE, "Index %ld, Red %ld, Green %ld, Blue %ld\n", (ULONG)index, (ULONG)red, (ULONG)green, (ULONG)blue);
    REGBASE();
    switch (index) {
    case 0:
        W_BLKIO_L(CUR_CLR0, CUR_CLR_R(red) | CUR_CLR_G(green) | CUR_CLR_B(blue) | CUR_CLR_8(17));
        break;
    case 2:
        W_BLKIO_L(CUR_CLR1, CUR_CLR_R(red) | CUR_CLR_G(green) | CUR_CLR_B(blue) | CUR_CLR_8(19));
        break;
    default:
        break;
    }
}

#define GEN_CUR_ENABLE      BIT(7)
#define GEN_CUR_ENABLE_MASK BIT(7)
#define GEN_GUI_RESETB      BIT(8)
#define GEN_GUI_RESETB_MASK BIT(8)

static BOOL ASM SetSprite(__REGA0(struct BoardInfo *bi), __REGD0(BOOL activate), __REGD7(RGBFTYPE RGBFormat))
{
    DFUNC(VERBOSE, "\n");
    REGBASE();

    W_BLKIO_MASK_L(GEN_TEST_CNTL, GEN_CUR_ENABLE_MASK, activate ? GEN_CUR_ENABLE : 0);

    if (activate) {
        SetSpriteColor(bi, 0, bi->CLUT[17].Red, bi->CLUT[17].Green, bi->CLUT[17].Blue, bi->RGBFormat);
        SetSpriteColor(bi, 1, bi->CLUT[18].Red, bi->CLUT[18].Green, bi->CLUT[18].Blue, bi->RGBFormat);
        SetSpriteColor(bi, 2, bi->CLUT[19].Red, bi->CLUT[19].Green, bi->CLUT[19].Blue, bi->RGBFormat);
    }

    return TRUE;
}

static inline void waitFifo(const BoardInfo_t *bi, UBYTE entries)
{
    MMIOBASE();

    if (!entries)
        return;

    while ((R_MMIO_L(FIFO_STAT) & 0xffff) > ((ULONG)(0x8000 >> entries)))
        ;
}

static inline void waitIdle(const BoardInfo_t *bi)
{
    MMIOBASE();

    waitFifo(bi, 16);
    while (R_MMIO_L(GUI_STAT) & 1)
        ;
}

static inline void REGARGS setWriteMask(BoardInfo_t *bi, UBYTE mask, RGBFTYPE fmt, BYTE waitFifoSlots)
{
    MMIOBASE();
    ChipData_t *cd = getChipData(bi);

    if (fmt != RGBFB_CLUT && cd->GEmask != 0xFF) {
        // 16/32 bit modes ignore the mask
        cd->GEmask = 0xFF;
        waitFifo(bi, waitFifoSlots + 1);
        W_MMIO_L(DP_WRITE_MSK, 0xFFFFFFFF);
    } else {
        // 8bit modes use the mask
        if (cd->GEmask != mask) {
            cd->GEmask = mask;

            waitFifo(bi, waitFifoSlots + 1);

            UWORD wordMask = (mask << 8) | mask;

            W_MMIO_L(DP_WRITE_MSK, makeDWORD(wordMask, wordMask));
        } else {
            waitFifo(bi, waitFifoSlots);
        }
    }
}

static inline ULONG REGARGS getMemoryOffset(struct BoardInfo *bi, APTR memory)
{
    ULONG offset = (ULONG)memory - (ULONG)bi->MemoryBase;
    return offset;
}

#define DST_OFFSET(x)   (x)
#define DST_OFFSET_MASK (0xFFFFF)
#define DST_PITCH(x)    ((x) << 22)
#define DST_PITCH_MASK  (0x3FF << 22)

#define DP_DST_PIX_WIDTH(x)    (x)
#define DP_DST_PIX_WIDTH_MASK  (0x7)
#define DP_SRC_PIX_WIDTH(x)    ((x) << 8)
#define DP_SRC_PIX_WIDTH_MASK  (0x7 << 8)
#define DP_HOST_PIX_WIDTH(x)   ((x) << 16)
#define DP_HOST_PIX_WIDTH_MASK (0x7 << 16)
#define DP_BYTE_PIX_ORDER      BIT(24)
#define DP_BYTE_PIX_ORDER_MASK BIT(24)

// #define HOST_BIG_ENDIAN      BIT(1)
// #define HOST_BIG_ENDIAN_MASK BIT(1)
// #define HOST_BYTE_ALIGN      BIT(0)
// #define HOST_BYTE_ALIGN_MASK BIT(0)

static inline BOOL setDstBuffer(struct BoardInfo *bi, const struct RenderInfo *ri, RGBFTYPE format)
{
    ChipData_t *cd = getChipData(bi);

    if (memcmp(ri, &cd->dstBuffer, sizeof(struct RenderInfo)) == 0) {
        return TRUE;
    }
    cd->dstBuffer = *ri;
    BYTE bppLog2  = getBPPLog2(format);

    waitFifo(bi, 2);

    MMIOBASE();

    // Offset is in units of '64 bit words' (8 bytes), while pitch is in units of '8 Pixels'
    // So convert BytesPerRow to "number of groups of 8 pixels"
    W_MMIO_L(DST_OFF_PITCH,
             DST_OFFSET(getMemoryOffset(bi, ri->Memory) / 8) | DST_PITCH(ri->BytesPerRow >> (bppLog2 + 3)));

    UBYTE dstPixWidth = COLOR_DEPTH_8;
    if (format != RGBFB_CLUT && format != RGBFB_B8G8R8 && format != RGBFB_R8G8B8) {
        dstPixWidth = g_bitWidths[format];
    }

    // FIXME: reading from the register is not FIFO'd. In theory to use W_MMIO_MASK_L we would need to wait for engine
    // idle to be sure
    //  that the previous write has completed. For now we just wait for 2 slots and write the register directly.
    W_MMIO_MASK_L(DP_PIX_WIDTH, DP_DST_PIX_WIDTH_MASK | DP_HOST_PIX_WIDTH_MASK,
                  DP_DST_PIX_WIDTH(dstPixWidth) | DP_HOST_PIX_WIDTH(COLOR_DEPTH_1));

    return TRUE;
}

#define SRC_OFFSET(x)   (x)
#define SRC_OFFSET_MASK (0xFFFFF)
#define SRC_PITCH(x)    ((x) << 22)
#define SRC_PITCH_MASK  (0x3FF << 22)

static inline BOOL setSrcBuffer(struct BoardInfo *bi, const struct RenderInfo *ri, RGBFTYPE format)
{
    ChipData_t *cd = getChipData(bi);

    // if (memcmp(ri, &cd->srcBuffer, sizeof(struct RenderInfo)) == 0) {
    //     return TRUE;
    // }
    // cd->dstBuffer = *ri;

    waitFifo(bi, 2);

    MMIOBASE();

    UBYTE bppLog2 = getBPPLog2(format);

    // Offset is in unite of '64 bit words' (8 bytes), while pitch is in units of '8 Pixels'
    // So convert BytesPerRow for
    W_MMIO_L(SRC_OFF_PITCH,
             SRC_OFFSET(getMemoryOffset(bi, ri->Memory) / 8) | SRC_PITCH((ri->BytesPerRow >> (bppLog2 + 3))));

    UBYTE srcPixWidth = COLOR_DEPTH_8;
    if (format != RGBFB_CLUT && format != RGBFB_B8G8R8 && format != RGBFB_R8G8B8) {
        srcPixWidth = g_bitWidths[format];
    }

    W_MMIO_MASK_L(DP_PIX_WIDTH, DP_SRC_PIX_WIDTH_MASK, DP_SRC_PIX_WIDTH(srcPixWidth));

    return TRUE;
}

static inline ULONG REGARGS penToColor(ULONG pen, RGBFTYPE fmt)
{
    switch (fmt) {
    case RGBFB_A8R8G8B8:
        pen = swapl(pen);
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
        pen = swapw(pen);
        break;
    default:
        break;
    }
    return pen;
}

#define DP_BKGD_SRC(x)      (x)
#define DP_BKGD_SRC_MASK(x) (0x7)
#define DP_FRGD_SRC(x)      ((x) << 8)
#define DP_FRGD_SRC_MASK(x) ((0x7) << 8)
#define DP_MONO_SRC(x)      ((x) << 16)
#define DP_MONO_SRC_MASK(x) ((0x3) << 16)

#define CLR_SRC_BKGD_COLOR 0x0
#define CLR_SRC_FRGD_COLOR 0x1
#define CLR_SRC_HOST_DATA  0x2
#define CLR_SRC_BLIT_SRC   0x3
#define CLR_SRC_PATTERN    0x4

#define MONO_SRC_ONE       0x0
#define MONO_SRC_PATTERN   0x1
#define MONO_SRC_HOST_DATA 0x2
#define MONO_SRC_BLIT_SRC  0x3

#define DP_BKGD_MIX(x)      (x)
#define DP_BKGD_MIX_MASK(x) (0x1F)
#define DP_FRGD_MIX(x)      ((x) << 16)
#define DP_FRGD_MIX_MASK    (0x1F << 16)

#define DST_X(x)   ((x) << 16)
#define DST_X_MASK (0x1FFF < 16)
#define DST_Y(y)   (y)
#define DST_Y_MASK (0x7FFF)

#define DST_WIDTH(w)    ((w) << 16)
#define DST_WIDTH_MASK  ((0x1FFF) << 16)
#define DST_HEIGHT(h)   (h)
#define DST_HEIGHT_MASK (0x7FFF)

#define DST_X_DIR              BIT(0)
#define DST_Y_DIR              BIT(1)
#define DST_Y_MAJOR            BIT(2)
#define DST_TILE_X             BIT(3)
#define DST_TILE_Y             BIT(4)
#define DST_LAST_PEL           BIT(5)
#define DST_24_ROT_EN          BIT(7)
#define DST_24_ROT(x)          ((x) << 8)
#define DST_24_ROT_MASK        (0x7 << 8)
#define DST_BRES_SIGN          BIT(11)
#define DST_POLYGON_RTEDGE_DIS BIT(12)
#define SRC_PATT_EN            BIT(16)
#define SRC_PATT_ROT_EN        BIT(17)
#define SRC_LINEAR_EN          BIT(18)
#define SRC_BYTE_ALIGN         BIT(19)
#define SRC_LINE_X_DIR         BIT(20)
#define PAT_MONO_EN            BIT(24)
#define PAT_CLR_4x2_EN         BIT(25)
#define PAT_CLR_8x1_EN         BIT(26)
#define HOST_BYTE_ALIGN        BIT(28)
#define HOST_BIG_ENDIAN_EN     BIT(29)

#define SC_LEFT(x)     (x)
#define SC_LEFT_MASK   (0x1FFF)
#define SC_RIGHT(x)    ((x) << 16)
#define SC_RIGHT_MASK  (0x1FFF << 16)
#define SC_TOP(x)      (x)
#define SC_TOP_MASK    (0x7FFF)
#define SC_BOTTOM(x)   ((x) << 16)
#define SC_BOTTOM_MASK (0x7FFF << 16)

static void drawRect(struct BoardInfo *bi, WORD x, WORD y, WORD width, WORD height)
{
    MMIOBASE();
    // W_MMIO_L(DST_Y_X, DST_Y(y) | DST_X(x));
    // W_MMIO_L(DST_HEIGHT_WIDTH, DST_HEIGHT(height) | DST_WIDTH(width));

    // micro-optimization to save on some redundant rol/swap/rol sequences
    writeRegLNoSwap(MMIOBase, DWORD_OFFSET(DST_Y_X), makeDWORD(swapw(y), swapw(x)));
    writeRegLNoSwap(MMIOBase, DWORD_OFFSET(DST_HEIGHT_WIDTH), makeDWORD(swapw(height), swapw(width)));
}

static void ASM FillRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                         __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(ULONG pen),
                         __REGD5(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\npen %08lx, mask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)pen, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    setDstBuffer(bi, ri, fmt);

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != FILLRECT) {
        cd->GEOp = FILLRECT;

        waitFifo(bi, 3);

        MMIOBASE();

        W_MMIO_L(DP_SRC, DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) | DP_FRGD_SRC(CLR_SRC_FRGD_COLOR) | DP_MONO_SRC(MONO_SRC_ONE));
        W_MMIO_L(DP_MIX, DP_BKGD_MIX(MIX_ZERO) | DP_FRGD_MIX(MIX_NEW));
        W_MMIO_L(GUI_TRAJ_CNTL, DST_X_DIR | DST_Y_DIR);
    }

    if (cd->GEfgPen != pen) {
        cd->GEfgPen    = pen;
        cd->GEdrawMode = 0xFF;  // invalidate drawmode cache

        pen = penToColor(pen, fmt);

        waitFifo(bi, 1);

        MMIOBASE();
        W_MMIO_L(DP_FRGD_CLR, pen);
    }

    setWriteMask(bi, mask, fmt, 2);

    drawRect(bi, x, y, width, height);
}

static void ASM InvertRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                           __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                           __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    setDstBuffer(bi, ri, fmt);

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != INVERTRECT) {
        cd->GEOp       = INVERTRECT;
        cd->GEdrawMode = 0xFF;  // invalidate minterm cache

        waitFifo(bi, 3);

        MMIOBASE();

        W_MMIO_L(DP_SRC, DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) | DP_FRGD_SRC(CLR_SRC_FRGD_COLOR) | DP_MONO_SRC(MONO_SRC_ONE));
        W_MMIO_L(DP_MIX, DP_BKGD_MIX(MIX_ZERO) | DP_FRGD_MIX(MIX_NOT_CURRENT));
        W_MMIO_L(GUI_TRAJ_CNTL, DST_X_DIR | DST_Y_DIR | DST_LAST_PEL);
    }

    setWriteMask(bi, mask, fmt, 2);

    drawRect(bi, x, y, width, height);
}

const static UWORD minTermToMix[16] = {
    MIX_ZERO,                     // 0000
    MIX_NOT_CURRENT_AND_NOT_NEW,  // 0001  (!dst ^ !src)
    MIX_CURRENT_AND_NOT_NEW,      // 0010  (dst ^ !src)
    MIX_NOT_NEW,                  // 0011  (!dst ^ !src) v (dst ^ !src)
    MIX_NOT_CURRENT_AND_NEW,      // 0100  (!dst ^ src)
    MIX_NOT_CURRENT,              // 0101  (!dst ^ src) v (!dst ^ !src)
    MIX_CURRENT_XOR_NEW,          // 0110  (!dst ^ src) v (dst ^ !src)
    MIX_NOT_CURRENT_OR_NOT_NEW,   // 0111  (!dst ^ src) v (dst ^ !src) v (!dst ^ !src)
    MIX_CURRENT_AND_NEW,          // 1000  (dst ^ src)
    MIX_NOT_CURRENT_XOR_NEW,      // 1001  (!dst ^ !src) v (dst ^ src)
    MIX_CURRENT,                  // 1010  (dst ^ src) v (dst ^ !src)
    MIX_CURRENT_OR_NOT_NEW,       // 1011  (dst ^ src) v (dst ^ !src) v (!dst ^ !src)
    MIX_NEW,                      // 1100  (dst ^ src) v (!dst ^ src)
    MIX_NOT_CURRENT_OR_NEW,       // 1101  (dst ^ src) v (!dst ^ src) v (!dst ^ !src)
    MIX_CURRENT_OR_NEW,           // 1110  (dst ^ src) v (!dst ^ src) v (dst ^ !src)
    MIX_ONE,                      // 1111  (!dst ^ !src) v (dst ^ !src) v (!dst ^ src) v (dst ^ src)
};

#define SRC_X(x)   ((x) << 16)
#define SRC_X_MASK (0x1FFF < 16)
#define SRC_Y(y)   (y)
#define SRC_Y_MASK (0x7FFF)

#define SRC_WIDTH1(x)    ((x) << 16)
#define SRC_WIDTH1_MASK  (0x1FFF < 16)
#define SRC_HEIGHT1(y)   (y)
#define SRC_HEIGHT1_MASK (0x7FFF)

#define SRC_WIDTH2(x)    ((x) << 16)
#define SRC_WIDTH2_MASK  (0x1FFF < 16)
#define SRC_HEIGHT2(y)   (y)
#define SRC_HEIGHT2_MASK (0x7FFF)

static void ASM BlitRectNoMaskComplete(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *sri),
                                       __REGA2(struct RenderInfo *dri), __REGD0(WORD srcX), __REGD1(WORD srcY),
                                       __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                                       __REGD5(WORD height), __REGD6(UBYTE opCode), __REGD7(RGBFTYPE format))
{
    DFUNC(VERBOSE,
          "\nx1 %ld, y1 %ld, x2 %ld, y2 %ld, w %ld, \n"
          "h %ld\nminTerm 0x%lx fmt %ld\n"
          "sri->bytesPerRow %ld, sri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)opCode, (ULONG)format,
          (ULONG)sri->BytesPerRow, (ULONG)sri->Memory);

    setDstBuffer(bi, dri, format);
    setSrcBuffer(bi, sri, format);

    MMIOBASE();

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITRECTNOMASKCOMPLETE) {
        cd->GEOp       = BLITRECTNOMASKCOMPLETE;
        cd->GEmask     = 0xFF;
        cd->GEdrawMode = 0xFF;  // invalidate minterm cache

        waitFifo(bi, 2);

        W_MMIO_L(DP_WRITE_MSK, 0xFFFFFFFF);
        W_MMIO_L(DP_SRC, DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) | DP_FRGD_SRC(CLR_SRC_BLIT_SRC) | DP_MONO_SRC(MONO_SRC_ONE));
    }

    if (cd->GEdrawMode != opCode) {
        cd->GEdrawMode = opCode;

        waitFifo(bi, 1);
        W_MMIO_L(DP_MIX, DP_BKGD_MIX(MIX_ZERO) | DP_FRGD_MIX(minTermToMix[opCode]));
    }

    ULONG dir = DST_X_DIR | DST_Y_DIR;  // left-to-right, top-to-bottom
    if (dstX > srcX) {
        dir &= ~DST_X_DIR;
        srcX = srcX + width - 1;
        dstX = dstX + width - 1;
    }
    if (dstY > srcY) {
        dir &= ~DST_Y_DIR;
        srcY = srcY + height - 1;
        dstY = dstY + height - 1;
    }

    waitFifo(bi, 5);
    W_MMIO_L(GUI_TRAJ_CNTL, dir);

    W_MMIO_L(SRC_Y_X, SRC_Y(srcY) | SRC_X(srcX));
    W_MMIO_L(SRC_HEIGHT1_WIDTH1, SRC_HEIGHT1(height) | SRC_WIDTH1(width));

    drawRect(bi, dstX, dstY, width, height);
}

static void ASM BlitRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD srcX),
                         __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                         __REGD5(WORD height), __REGD6(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE,
          "\nx1 %ld, y1 %ld, x2 %ld, y2 %ld, w %ld, \n"
          "h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt,
          (ULONG)ri->BytesPerRow, (ULONG)ri->Memory);

    // FIXME: optimize into one function
    setDstBuffer(bi, ri, fmt);
    setSrcBuffer(bi, ri, fmt);

    MMIOBASE();

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITRECT) {
        cd->GEOp       = BLITRECT;
        cd->GEdrawMode = 0xFF;  // invalidate minterm cache

        waitFifo(bi, 2);

        W_MMIO_L(DP_SRC, DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) | DP_FRGD_SRC(CLR_SRC_BLIT_SRC) | DP_MONO_SRC(MONO_SRC_ONE));
        W_MMIO_L(DP_MIX, DP_BKGD_MIX(MIX_ZERO) | DP_FRGD_MIX(MIX_NEW));
    }

    ULONG dir = DST_X_DIR | DST_Y_DIR;  // left-to-right, top-to-bottom
    if (dstX > srcX) {
        dir &= ~DST_X_DIR;
        srcX = srcX + width - 1;
        dstX = dstX + width - 1;
    }
    if (dstY > srcY) {
        dir &= ~DST_Y_DIR;
        srcY = srcY + height - 1;
        dstY = dstY + height - 1;
    }

    // FIFO wait in setWriteMask
    setWriteMask(bi, mask, fmt, 5);

    W_MMIO_L(GUI_TRAJ_CNTL, dir);

    W_MMIO_L(SRC_Y_X, SRC_Y(srcY) | SRC_X(srcX));
    W_MMIO_L(SRC_HEIGHT1_WIDTH1, SRC_HEIGHT1(height) | SRC_WIDTH1(width));

    drawRect(bi, dstX, dstY, width, height);
}

static void REGARGS setDrawModeInternal(BoardInfo_t *bi, UBYTE drawMode, ULONG fgPen, ULONG bgPen, BYTE monoSource)
{
    UWORD writeMode = (drawMode & COMPLEMENT) ? MIX_NOT_CURRENT : MIX_NEW;
    UWORD fMix, bMix;
    switch (drawMode & 1) {
    case JAM1:
        fMix = writeMode;
        bMix = MIX_CURRENT;
        break;
    case JAM2:
        fMix = writeMode;
        bMix = writeMode;
        break;
    }

    UWORD fSrc, bSrc;
    fSrc = CLR_SRC_FRGD_COLOR;
    bSrc = CLR_SRC_BKGD_COLOR;

    if (drawMode & INVERSVID) {
        UWORD t = fMix;
        fMix    = bMix;
        bMix    = t;
        t       = fSrc;
        fSrc    = bSrc;
        bSrc    = t;
    }

    waitFifo(bi, 4);

    MMIOBASE();

    W_MMIO_L(DP_FRGD_CLR, fgPen);
    W_MMIO_L(DP_BKGD_CLR, bgPen);

    W_MMIO_L(DP_MIX, DP_BKGD_MIX(bMix) | DP_FRGD_MIX(fMix));
    W_MMIO_L(DP_SRC, DP_FRGD_SRC(fSrc) | DP_BKGD_SRC(bSrc) | DP_MONO_SRC(monoSource));
}

static inline void REGARGS setDrawMode(struct BoardInfo *bi, ULONG FgPen, ULONG BgPen, UBYTE DrawMode, RGBFTYPE format,
                                       BYTE monoSource)
{
    ChipData_t *cd = getChipData(bi);

    if (cd->GEfgPen != FgPen || cd->GEbgPen != BgPen || cd->GEdrawMode != DrawMode) {
        cd->GEfgPen    = FgPen;
        cd->GEbgPen    = BgPen;
        cd->GEdrawMode = DrawMode;

        UWORD frgdMix, bkgdMix;
        ULONG fgPen = penToColor(FgPen, format);
        ULONG bgPen = penToColor(BgPen, format);

        setDrawModeInternal(bi, DrawMode, fgPen, bgPen, monoSource);
    }
}

static void ASM BlitTemplate(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                             __REGA2(struct Template *template), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                             __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    setDstBuffer(bi, ri, fmt);

    MMIOBASE();

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITTEMPLATE) {
        cd->GEOp       = BLITTEMPLATE;
        cd->GEdrawMode = 0xFF;
        waitFifo(bi, 1);
        W_MMIO_L(GUI_TRAJ_CNTL, SRC_LINEAR_EN | DST_X_DIR | DST_Y_DIR);
    }

    setDrawMode(bi, template->FgPen, template->BgPen, template->DrawMode, fmt, MONO_SRC_HOST_DATA);
    setWriteMask(bi, mask, fmt, 0);

    // 0 <= XOffset <= 15
    UWORD blitWidth     = (width + template->XOffset + 31) & ~31;
    UWORD dwordsPerLine = blitWidth / 32;

    UWORD numFifoSlots = dwordsPerLine * height + 3;
    if (numFifoSlots > 16) {
        numFifoSlots = 16;
    }
    waitFifo(bi, numFifoSlots);

    // Since we feed the monochrome expansion in units of 32bit (i.e. 32 pixels width),
    // we need to align the width to the next 32bit boundary. To make that padding not get rendered, use
    // the right scissor. And since we're setting the scissor anyways, we might as well set the left side
    // and spare ourselves the CPU work to "left-rotate" the template bits.
    W_MMIO_L(SC_LEFT_RIGHT, SC_RIGHT(width + x - 1) | SC_LEFT(x));
    drawRect(bi, x - template->XOffset, y, blitWidth, height);

    // We already used up 3 fifo slots for the setup above
    UWORD hostDataReg = 3;

    const UBYTE *bitmap = (const UBYTE *)template->Memory;
    WORD bitmapPitch    = template->BytesPerRow;

    for (UWORD y = 0; y < height; ++y) {
        for (UWORD x = 0; x < dwordsPerLine; ++x) {
            writeRegLNoSwap(MMIOBase, DWORD_OFFSET(HOST_DATA0 + hostDataReg), ((const ULONG *)bitmap)[x]);

            hostDataReg = (hostDataReg + 1) & 15;
            if (!hostDataReg) {
                waitFifo(bi, 16);
            }
        }
        bitmap += bitmapPitch;
    }

    waitFifo(bi, 1);
    // reset right scissor
    W_MMIO_L(SC_LEFT_RIGHT, ((SC_RIGHT_MASK >> 1) & SC_RIGHT_MASK) | SC_LEFT(0));
}

static void REGARGS performBlitPlanar2ChunkyBlits(struct BoardInfo *bi, SHORT dstX, SHORT dstY, SHORT width,
                                                  SHORT height, UBYTE *bitmap, UWORD dwordsPerLine, WORD bmPitch,
                                                  UBYTE rol)
{
    MMIOBASE();

    if ((ULONG)bitmap == 0x00000000) {
        waitFifo(bi, 3);
        W_MMIO_L(DP_SRC, DP_FRGD_SRC(CLR_SRC_BKGD_COLOR) | DP_BKGD_SRC(CLR_SRC_FRGD_COLOR) |
                             DP_MONO_SRC(MONO_SRC_ONE));  // Background color, 0x0
        drawRect(bi, dstX, dstY, width, height);
    } else if ((ULONG)bitmap == 0xFFFFFFFF) {
        waitFifo(bi, 3);
        W_MMIO_L(DP_SRC, DP_FRGD_SRC(CLR_SRC_FRGD_COLOR) | DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) |
                             DP_MONO_SRC(MONO_SRC_ONE));  // Forground color, 0xFF
        drawRect(bi, dstX, dstY, width, height);
    } else {
        UWORD numFifoSlots = dwordsPerLine * height + 3;
        if (numFifoSlots > 16) {
            numFifoSlots = 16;
        }
        waitFifo(bi, numFifoSlots);

        UWORD hostDataReg = 3;

        // planar bitmap selects between 0x00 and 0xFF
        W_MMIO_L(DP_SRC,
                 DP_FRGD_SRC(CLR_SRC_FRGD_COLOR) | DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) | DP_MONO_SRC(MONO_SRC_HOST_DATA));
        drawRect(bi, dstX, dstY, width, height);

        if (!rol) {
            for (UWORD y = 0; y < height; ++y) {
                for (UWORD x = 0; x < dwordsPerLine; ++x) {
                    writeRegLNoSwap(MMIOBase, DWORD_OFFSET(HOST_DATA0 + hostDataReg), ((ULONG *)bitmap)[x]);

                    hostDataReg = (hostDataReg + 1) & 15;
                    if (!hostDataReg) {
                        waitFifo(bi, 16);
                    }
                }
                bitmap += bmPitch;
            }
        } else {
            for (UWORD y = 0; y < height; ++y) {
                for (UWORD x = 0; x < dwordsPerLine; ++x) {
                    ULONG left  = ((ULONG *)bitmap)[x] << rol;
                    ULONG right = ((ULONG *)bitmap)[x + 1] >> (32 - rol);

                    writeRegLNoSwap(MMIOBase, DWORD_OFFSET(HOST_DATA0 + hostDataReg), (left | right));

                    hostDataReg = (hostDataReg + 1) & 15;
                    if (!hostDataReg) {
                        waitFifo(bi, 16);
                    }
                }
                bitmap += bmPitch;
            }
        }
    }
}

static void ASM BlitPlanar2Chunky(__REGA0(struct BoardInfo *bi), __REGA1(struct BitMap *bm),
                                  __REGA2(struct RenderInfo *ri), __REGD0(SHORT srcX), __REGD1(SHORT srcY),
                                  __REGD2(SHORT dstX), __REGD3(SHORT dstY), __REGD4(SHORT width), __REGD5(SHORT height),
                                  __REGD6(UBYTE minTerm), __REGD7(UBYTE mask))
{
    DFUNC(VERBOSE,
          "\nsrcX %ld, srcY %ld, dstX %ld, dstY %ld, w %ld, h %ld"
          "\nmask 0x%lx minTerm %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)minTerm,
          (ULONG)ri->BytesPerRow, (ULONG)ri->Memory);

    MMIOBASE();

    WORD bytesPerRow = ri->BytesPerRow;
    // how many dwords per line in the source plane
    UWORD numPlanarBytes = width / 8 * height * bm->Depth;
    // UWORD projectedRegisterWriteBytes = (9 + 8 * 8) * 2;

    // if ((projectedRegisterWriteBytes > numPlanarBytes) || !setCR50(bi, bytesPerRow, 1)) {
    //     DFUNC(1, "fallback to BlitPlanar2ChunkyDefault\n");
    //     bi->BlitPlanar2ChunkyDefault(bi, bm, ri, srcX, srcY, dstX, dstY, width, height, minTerm, mask);
    //     return;
    // }

    setDstBuffer(bi, ri, RGBFB_CLUT);

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITPLANAR2CHUNKY) {
        cd->GEOp = BLITPLANAR2CHUNKY;
        // Invalidate the pen and drawmode caches
        cd->GEdrawMode = 0xFF;
        cd->GEmask     = mask;
        cd->GEfgPen    = 0xFFFFFFFF;
        cd->GEbgPen    = 0x0;

        waitFifo(bi, 3);
        W_MMIO_L(GUI_TRAJ_CNTL, SRC_LINEAR_EN | DST_X_DIR | DST_Y_DIR);
        W_MMIO_L(DP_FRGD_CLR, 0xFFFFFFFF);
        W_MMIO_L(DP_BKGD_CLR, 0x0);
    }

    if (cd->GEdrawMode != minTerm) {
        cd->GEdrawMode = minTerm;

        waitFifo(bi, 1);
        // Set the mix mode (minterm)
        UWORD mixMode = minTermToMix[minTerm];
        W_MMIO_L(DP_MIX, DP_BKGD_MIX(mixMode) | DP_FRGD_MIX(mixMode));
    }

    waitFifo(bi, 1);
    // clip potential 32bit padding
    W_MMIO_L(SC_LEFT_RIGHT, SC_RIGHT(dstX + width - 1) | SC_LEFT(dstX));
    // pad up to 32bit
    width = (width + 31) & ~31;

    WORD bmPitch        = bm->BytesPerRow;
    ULONG bmStartOffset = (srcY * bmPitch) + (srcX / 32) * 4;
    UWORD dwordsPerLine = width / 32;
    UBYTE rol           = srcX % 32;

    for (short p = 0; p < 8; ++p) {
        UBYTE writeMask = 1 << p;

        if (!(mask & writeMask)) {
            continue;
        }

        setWriteMask(bi, writeMask, RGBFB_CLUT, 0);

        UBYTE *bitmap = (UBYTE *)bm->Planes[p];
        if (bitmap != 0x0 && (ULONG)bitmap != 0xffffffff) {
            bitmap += bmStartOffset;
        }

        performBlitPlanar2ChunkyBlits(bi, dstX, dstY, width, height, bitmap, dwordsPerLine, bmPitch, rol);
    }

    waitFifo(bi, 1);
    // reset right scissor
    W_MMIO_L(SC_LEFT_RIGHT, ((SC_RIGHT_MASK >> 1) & SC_RIGHT_MASK) | SC_LEFT(0));
}

static void ASM BlitPattern(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                            __REGA2(struct Pattern *pattern), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                            __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    if (pattern->Size > 8) {
        bi->BlitPatternDefault(bi, ri, pattern, x, y, width, height, mask, fmt);
        return;
    }

    ChipData_t *cd = getChipData(bi);

    UWORD patternHeight        = 1 << pattern->Size;
    const UWORD *sysMemPattern = (const UWORD *)pattern->Memory;
    UWORD *cachedPattern       = cd->patternCacheBuffer;
    ULONG *videoMemPattern     = cd->patternVideoBuffer;

    // Try to avoid wait-for-idle by first checking if the pattern changed.
    // I'm not expecting huge patterns, so this will hopefully be fast
    BOOL patternChanged = FALSE;
    for (UWORD i = 0; i < patternHeight; ++i) {
        if (sysMemPattern[i] != cachedPattern[i]) {
            cachedPattern[i] = sysMemPattern[i];
            patternChanged   = TRUE;
        }
    }
    if (patternChanged) {
        waitIdle(bi);

        for (UWORD i = 0; i < patternHeight; ++i) {
            // The video pattern has an 8-byte pitch. 64pixels (bits) is the minimum pitch for monochrome src blit data.
            videoMemPattern[i * 2] = cachedPattern[i] << 16;
        }
    }

    if (cd->GEOp != BLITPATTERN) {
        cd->GEOp         = BLITPATTERN;
        cd->GEdrawMode   = 0xFF;
        cd->patternCache = 0xFFFFFFFF;

        waitFifo(bi, 3);

        MMIOBASE();

        ULONG trajectory = DST_X_DIR | DST_Y_DIR | SRC_PATT_EN | SRC_PATT_ROT_EN;

        W_MMIO_L(GUI_TRAJ_CNTL, trajectory);

        // Offset is in units of '64 bit words' (8 bytes), while pitch is in units of '8 Pixels'.
        W_MMIO_L(SRC_OFF_PITCH, SRC_OFFSET(getMemoryOffset(bi, cd->patternVideoBuffer) / 8) | SRC_PITCH(8));
        W_MMIO_MASK_L(DP_PIX_WIDTH, DP_SRC_PIX_WIDTH_MASK, DP_SRC_PIX_WIDTH(COLOR_DEPTH_1));
    }

    setDstBuffer(bi, ri, fmt);
    setDrawMode(bi, pattern->FgPen, pattern->BgPen, pattern->DrawMode, fmt, MONO_SRC_BLIT_SRC);
    setWriteMask(bi, mask, fmt, 0);

    UBYTE xOff = pattern->XOffset & 15;
    UWORD yOff = pattern->YOffset & (patternHeight - 1);

    ULONG pattCache = (yOff << 16) | (xOff << 8) | pattern->Size;
    if (pattCache != cd->patternCache) {
        cd->patternCache = pattCache;

        MMIOBASE();

        waitFifo(bi, 5);

        W_MMIO_L(SRC_Y_X, SRC_X(xOff) | SRC_Y(yOff));
        W_MMIO_L(SRC_HEIGHT1_WIDTH1, SRC_HEIGHT1(patternHeight - yOff) | SRC_WIDTH1(16 - xOff));
        W_MMIO_L(SRC_HEIGHT2_WIDTH2, SRC_HEIGHT2(patternHeight) | SRC_WIDTH2(16));
    } else {
        waitFifo(bi, 2);
    }

    drawRect(bi, x, y, width, height);
}

static inline void ASM WaitBlitter(__REGA0(struct BoardInfo *bi))
{
    D(CHATTY, "Waiting for blitter...");

    waitIdle(bi);

    D(CHATTY, "done\n");
}

BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    REGBASE();
    MMIOBASE();
    LOCAL_SYSBASE();

    DFUNC(0, "\n");

    bi->GraphicsControllerType = GCT_ATIRV100;
    bi->PaletteChipType        = PCT_ATT_20C492;
    bi->Flags                  = bi->Flags | BIF_GRANTDIRECTACCESS | BIF_HASSPRITEBUFFER | BIF_HARDWARESPRITE | BIF_BLITTER;
    // BIF_VGASCREENSPLIT ;

    bi->RGBFormats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;
    // We can support byte-swapped formats on this chip via the Big Linear
    // Adressing Window
    bi->RGBFormats |= RGBFF_A8R8G8B8 | RGBFF_R5G6B5 | RGBFF_R5G5B5;

    bi->SoftSpriteFlags = 0;

    bi->SetGC                = SetGC;
    bi->SetPanning           = SetPanning;
    bi->CalculateBytesPerRow = CalculateBytesPerRow;
    bi->CalculateMemory      = CalculateMemory;
    bi->GetCompatibleFormats = GetCompatibleFormats;
    bi->SetDAC               = SetDAC;
    bi->SetColorArray        = SetColorArray;
    bi->SetDisplay           = SetDisplay;
    bi->SetMemoryMode        = SetMemoryMode;
    bi->SetWriteMask         = SetWriteMask;
    bi->SetReadPlane         = SetReadPlane;
    bi->SetClearMask         = SetClearMask;
    bi->ResolvePixelClock    = ResolvePixelClock;
    bi->GetPixelClock        = GetPixelClock;
    bi->SetClock             = SetClock;

    // // VSYNC
    bi->WaitVerticalSync = WaitVerticalSync;
    bi->GetVSyncState    = GetVSyncState;

    // // DPMS
    // bi->SetDPMSLevel = SetDPMSLevel;

    // // VGA Splitscreen
    // bi->SetSplitPosition = SetSplitPosition;

    // // Mouse Sprite
    bi->SetSprite         = SetSprite;
    bi->SetSpritePosition = SetSpritePosition;
    bi->SetSpriteImage    = SetSpriteImage;
    bi->SetSpriteColor    = SetSpriteColor;

    // // Blitter acceleration
    bi->WaitBlitter            = WaitBlitter;
    bi->BlitRect               = BlitRect;
    bi->InvertRect             = InvertRect;
    bi->FillRect               = FillRect;
    bi->BlitTemplate           = BlitTemplate;
    bi->BlitPlanar2Chunky      = BlitPlanar2Chunky;
    bi->BlitRectNoMaskComplete = BlitRectNoMaskComplete;
    // bi->DrawLine = DrawLine;
    bi->BlitPattern = BlitPattern;

    // Informed by the largest X/Y coordinates the blitter can talk to
    bi->MaxBMWidth  = 16384;  // 15bits
    bi->MaxBMHeight = 16384;  // 15bits

    bi->BitsPerCannon          = 8;
    bi->MaxHorValue[PLANAR]    = 4088;  // 511 * 8
    bi->MaxHorValue[CHUNKY]    = 4088;
    bi->MaxHorValue[HICOLOR]   = 4088;
    bi->MaxHorValue[TRUECOLOR] = 4088;
    bi->MaxHorValue[TRUEALPHA] = 4088;

    bi->MaxVerValue[PLANAR]    = 2047;
    bi->MaxVerValue[CHUNKY]    = 2047;
    bi->MaxVerValue[HICOLOR]   = 2047;
    bi->MaxVerValue[TRUECOLOR] = 2047;
    bi->MaxVerValue[TRUEALPHA] = 2047;

    UWORD maxWidth               = 4096;  // 13bit signed
    UWORD maxHeight              = 8192;  // 14bit signed
    bi->MaxHorResolution[PLANAR] = maxWidth;
    bi->MaxVerResolution[PLANAR] = maxHeight;

    bi->MaxHorResolution[CHUNKY] = maxWidth;
    bi->MaxVerResolution[CHUNKY] = maxHeight;

    bi->MaxHorResolution[HICOLOR] = maxWidth;
    bi->MaxVerResolution[HICOLOR] = maxHeight;

    bi->MaxHorResolution[TRUECOLOR] = maxWidth;
    bi->MaxVerResolution[TRUECOLOR] = maxHeight;

    bi->MaxHorResolution[TRUEALPHA] = maxWidth;
    bi->MaxVerResolution[TRUEALPHA] = maxHeight;

    {
        DFUNC(0, "Determine Chip Family\n");

        ULONG revision;
        ULONG deviceId;
        LOCAL_PROMETHEUSBASE();
        Prm_GetBoardAttrsTags((PCIBoard *)bi->CardPrometheusDevice, PRM_Device, (ULONG)&deviceId, PRM_Revision,
                              (ULONG)&revision, TAG_END);

        ChipData_t *cd = getChipData(bi);
        cd->chipFamily = UNKNOWN;

        switch (deviceId) {
        case 0x5654:
            cd->chipFamily = MACH64VT;
            break;
        case 0x4758:
            cd->chipFamily = MACH64GX;
            break;
        default:
            cd->chipFamily = UNKNOWN;
            DFUNC(0, "Unknown chip family, aborting\n");
            return FALSE;
        }

        // User-Defines configuration
        UBYTE config = Prm_ReadConfigByte((PCIBoard *)bi->CardPrometheusDevice, 0x40);

        UWORD ioBase = 0;
        switch (config & 0x03) {
        case 0b00:
            ioBase = 0x2ec;
            break;
        case 0b01:
            ioBase = 0x1cc;
            break;
        case 0b11:
            ioBase = 0x1c8;
            break;
        }
        BOOL blockIO      = !!(config & 0x04);
        BOOL enableGENENA = !(config & 0x08);

        D(0, "PCI config: ioBase : 0x%lx, sparse/block IO : %ld, enable GENENA : %ld\n", (ULONG)ioBase, (ULONG)blockIO,
          (ULONG)enableGENENA);
    }

    // Test scratch register response
    ULONG saveScratchReg1 = R_MMIO_L(SCRATCH_REG1);
    W_BLKIO_L(SCRATCH_REG1, 0xAAAAAAAA);
    ULONG scratchA = R_MMIO_L(SCRATCH_REG1);
    W_BLKIO_L(SCRATCH_REG1, 0x55555555);
    ULONG scratch5 = R_MMIO_L(SCRATCH_REG1);
    W_BLKIO_L(SCRATCH_REG1, saveScratchReg1);
    if (scratchA != 0xAAAAAAAA || scratch5 != 0x55555555) {
        DFUNC(0, "scratch register response broken.\n");
        return FALSE;
    }
    DFUNC(0, "scratch register response good.\n");

    findRomHeader(bi);

    static const UWORD defaultRegs[] = {0x00a2, 0x6007, 0x00a0, 0x20f8, 0x0018, 0x0000, 0x001c, 0x0200, 0x001e, 0x040b,
                                        0x00d2, 0x0000, 0x00e4, 0x0020, 0x00b0, 0x0021, 0x00b2, 0x0801, 0x00d0, 0x0000,
                                        0x001e, 0x0000, 0x0080, 0x0000, 0x0082, 0x0000, 0x0084, 0x0000, 0x0086, 0x0000,
                                        0x00c4, 0x0000, 0x00c6, 0x8000, 0x007a, 0x0000, 0x00d0, 0x0100};

    for (int r = 0; r < ARRAY_SIZE(defaultRegs); r += 2) {
        D(10, "[%lX_%ldh] = 0x%04lx\n", (ULONG)defaultRegs[r] / 4, (ULONG)defaultRegs[r] % 4,
          (ULONG)defaultRegs[r + 1]);
        W_IO_W(defaultRegs[r], defaultRegs[r + 1]);
    }

    InitPLL(bi);

    ULONG clock = bi->MemoryClock;
    if (!clock) {
        clock = getChipData(bi)->memClock;
    } else {
        clock /= 10000;
    }

    if (clock < 3300) {
        clock = 3300;
    }
    if (clock > 7000) {
        clock = 7000;
    }

    SetMemoryClock(bi, clock);
    bi->MemoryClock = clock * 10000;

    R_BLKIO_L(CONFIG_STAT0);
    W_BLKIO_L(BUS_CNTL, BUS_FIFO_ERR_AK | BUS_HOST_ERR_AK);

    W_BLKIO_MASK_L(CONFIG_CNTL, CFG_VGA_DIS_MASK, CFG_VGA_DIS);
    W_BLKIO_MASK_L(CONFIG_STAT0, CFG_VGA_EN_MASK, 0);
    ULONG configCntl = R_BLKIO_L(CONFIG_CNTL);
    // These values are being set by the Mach64VT BIOS, but are inline with the code, not somewhere in an accessible
    // table. CONFIG_STAT0 is apparently not strapped to the right memory type and only by setting it here we get access
    // to the framebuffer memory.
    W_BLKIO_MASK_L(CONFIG_STAT0, CFG_MEM_TYPE_MASK | CFG_DUAL_CAS_EN_MASK | CFG_VGA_EN_MASK | CFG_CLOCK_EN_MASK,
                   CFG_MEM_TYPE(CFG_MEM_TYPE_PSEUDO_EDO) | CFG_DUAL_CAS_EN | CFG_CLOCK_EN);
    // W_BLKIO_MASK_B(CONFIG_STAT0, 0, ~0xf8, 0x3b);

    D(0, "MMIO base address: 0x%lx\n", (ULONG)getMMIOBase(bi));

    // Determine memory size of the card (typically 1-2MB, but can be up to 4MB)
    switch (getChipData(bi)->chipFamily) {
    case MACH64VT:
        bi->MemorySize = 0x400000;
        break;
    default:
        bi->MemorySize = 0x400000;
    }

    volatile ULONG *framebuffer = (volatile ULONG *)bi->MemoryBase;
    framebuffer[0]              = 0;

    int memSizeIdx = 0b11;
    while (memSizeIdx >= 0) {
        D(VERBOSE, "Probing memory size %ld\n", bi->MemorySize);

        W_BLKIO_MASK_L(MEM_CNTL, 0x7, memSizeIdx);

        CacheClearU();

        // Probe the last and the first longword for the current segment,
        // as well as offset 0 to check for wrap arounds
        volatile ULONG *highOffset = framebuffer + (bi->MemorySize >> 2) - 512 - 1;
        volatile ULONG *lowOffset  = framebuffer + (bi->MemorySize >> 3);
        // Probe  memory
        *framebuffer = 0;
        *highOffset  = (ULONG)highOffset;
        *lowOffset   = (ULONG)lowOffset;

        CacheClearU();

        ULONG readbackHigh = *highOffset;
        ULONG readbackLow  = *lowOffset;
        ULONG readbackZero = *framebuffer;

        D(VERBOSE, "Probing memory at 0x%lx ?= 0x%lx; 0x%lx ?= 0x%lx, 0x0 ?= 0x%lx\n", highOffset, readbackHigh,
          lowOffset, readbackLow, readbackZero);

        if (readbackHigh == (ULONG)highOffset && readbackLow == (ULONG)lowOffset && readbackZero == 0) {
            break;
        }
        // reduce available memory size
        bi->MemorySize >>= 1;
        memSizeIdx -= 1;
    }

    D(INFO, "MemorySize: %ldmb\n", bi->MemorySize / (1024 * 1024));

    MEM_CNTL_Register memCntl;
    *(ULONG *)&memCntl = R_BLKIO_L(MEM_CNTL);
    print_MEM_CNTL_Register(&memCntl);

    // Init DAC
    W_BLKIO_MASK_L(DAC_CNTL, DAC_8BIT_EN_MASK | DAC_BLANKING_MASK, DAC_8BIT_EN | DAC_BLANKING);
    W_BLKIO_B(DAC_REGS, DAC_MASK, 0xFF);

    // Init CRTC
    W_BLKIO_MASK_L(CRTC_GEN_CNTL, CRTC_ENABLE_MASK | CRTC_EXT_DISP_EN_MASK | CRTC_FIFO_LWM_MASK,
                   CRTC_ENABLE | CRTC_EXT_DISP_EN | CRTC_FIFO_LWM(0xF));

    // Init Engine
    // Reset engine. FIXME: older models use the same bit, but in a different way(?)
    ULONG genTestCntl = R_MMIO_L(GEN_TEST_CNTL) & ~GEN_GUI_RESETB_MASK;
    W_MMIO_L(GEN_TEST_CNTL, genTestCntl | GEN_GUI_RESETB);
    W_MMIO_L(GEN_TEST_CNTL, genTestCntl);
    W_MMIO_L(GEN_TEST_CNTL, genTestCntl | GEN_GUI_RESETB);

    waitFifo(bi, 16);
    W_MMIO_L(CONTEXT_MSK, 0xFFFFFFFF);
    W_MMIO_L(DST_Y_X, 0);
    W_MMIO_L(DST_BRES_ERR, 0);
    W_MMIO_L(DST_BRES_INC, 0);
    W_MMIO_L(DST_BRES_DEC, 0);
    W_MMIO_L(DST_CNTL, DST_LAST_PEL | DST_Y_DIR | DST_X_DIR);

    W_MMIO_L(SRC_Y_X, 0);
    W_MMIO_L(SRC_HEIGHT1_WIDTH1, 1);
    W_MMIO_L(SRC_Y_X_START, 0);
    W_MMIO_L(SRC_HEIGHT2_WIDTH2, 1);
    // set source pixel retrieving attributes
    // W_MMIO_L(SRC_CNTL, SRC_LINE_X_LEFT_TO_RIGHT);

    waitFifo(bi, 16);
    W_MMIO_L(DP_BKGD_CLR, 0x0);
    W_MMIO_L(DP_FRGD_CLR, 0xFFFFFFFF);
    W_MMIO_L(DP_WRITE_MSK, 0xFFFFFFFF);
    W_MMIO_L(DP_MIX, DP_BKGD_MIX(MIX_ZERO) | DP_FRGD_MIX(MIX_NEW));
    W_MMIO_L(DP_SRC, DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) | DP_FRGD_SRC(CLR_SRC_FRGD_COLOR) | DP_MONO_SRC(MONO_SRC_ONE));
    W_MMIO_L(DP_PIX_WIDTH,
             DP_DST_PIX_WIDTH(COLOR_DEPTH_8) | DP_SRC_PIX_WIDTH(COLOR_DEPTH_8) | DP_HOST_PIX_WIDTH(COLOR_DEPTH_8));
    W_MMIO_L(CLR_CMP_CNTL, 0x0);
    W_MMIO_L(GUI_TRAJ_CNTL, DST_X_DIR | DST_Y_DIR | DST_LAST_PEL);
    W_MMIO_L(SC_LEFT_RIGHT, SC_LEFT(0) | ((SC_RIGHT_MASK >> 1) & SC_RIGHT_MASK));
    W_MMIO_L(SC_TOP_BOTTOM, SC_TOP(0) | ((SC_BOTTOM_MASK >> 1) & SC_BOTTOM_MASK));
    W_MMIO_L(SRC_Y_X_START, 0);

    D(INFO, "Monitor is %s present\n", ((R_BLKIO_B(DAC_CNTL, 0) & 0x80) ? "NOT" : ""));

    // Two sprite images, each 64x64*2 bits
    // BEWARE: softsprite data would use 4 byte per pixel
    const ULONG maxSpriteBuffersSize = (64 * 64 * 2 / 8) * 2;
    bi->MemorySize                   = (bi->MemorySize - maxSpriteBuffersSize) & ~(63); // align to 64 byte boundary

    bi->MouseImageBuffer = bi->MemoryBase + bi->MemorySize;
    bi->MouseSaveBuffer  = bi->MemoryBase + bi->MemorySize + maxSpriteBuffersSize / 2;

    // reserve memory for a pattern that can be up to 256 lines high (2kb)
    // Since the minimum pitch for SRC_PITCH is 64 monochrome pixels (8 byte), we need to overallocate.
    // The P96 pattern is just 16 pixels wide.
    ULONG patternSize                   = 8 * 256;
    bi->MemorySize                      = (bi->MemorySize - patternSize) & ~(7);
    getChipData(bi)->patternVideoBuffer = (ULONG *)(bi->MemoryBase + bi->MemorySize);
    getChipData(bi)->patternCacheBuffer = AllocVec(patternSize, MEMF_PUBLIC);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef TESTEXE

#include <boardinfo.h>
#include <libraries/prometheus.h>
#include <proto/expansion.h>
#include <proto/prometheus.h>
#include <proto/utility.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VENDOR_E3B        0xE3B
#define VENDOR_MATAY      0xAD47
#define DEVICE_FIRESTORM  200
#define DEVICE_PROMETHEUS 1

struct Library *PrometheusBase = NULL;

APTR findLegacyIOBase()
{
    struct ConfigDev *cd;

    APTR legacyIOBase = NULL;
    if (cd = FindConfigDev(NULL, VENDOR_MATAY, DEVICE_PROMETHEUS))
        legacyIOBase = cd->cd_BoardAddr;
    else if (cd = FindConfigDev(NULL, VENDOR_E3B, DEVICE_FIRESTORM))
        legacyIOBase = (APTR)((ULONG)cd->cd_BoardAddr + 0x1fe00000);

    return legacyIOBase;
}

int main()
{
    int rval = EXIT_FAILURE;

    if (!(PrometheusBase = OpenLibrary(PROMETHEUSNAME, 0))) {
        D(0, "Unable to open prometheus.library\n");
        goto exit;
    }

    APTR legacyIOBase = NULL;
    if (!(legacyIOBase = findLegacyIOBase())) {
        D(0, "Unable to find legacy IO base\n");
        goto exit;
    }

    ULONG dmaSize = 128 * 1024;

    APTR board = NULL;

    D(0, "Looking for Mach64 card\n");

    while ((board = (APTR)Prm_FindBoardTags(board, PRM_Vendor, PCI_VENDOR, TAG_END)) != NULL) {
        ULONG Device, Revision, Memory0Size;
        APTR Memory0, Memory1;

        Prm_GetBoardAttrsTags(board, PRM_Device, (ULONG)&Device, PRM_Revision, (ULONG)&Revision, PRM_MemoryAddr0,
                              (ULONG)&Memory0, PRM_MemorySize0, (ULONG)&Memory0Size, PRM_MemoryAddr1, (ULONG)&Memory1,
                              TAG_END);

        D(0, "device %x revision %x\n", Device, Revision);

        ChipFamily_t family = UNKNOWN;

        switch (Device) {
        case 0x5654:  // mach64 VT
            family = MACH64VT;
            break;
        case 0x4758:
            family = MACH64GX;
            break;
        }

        if (family != UNKNOWN) {
            D(0, "ATI Mach64 found\n");

            Prm_WriteConfigWord((PCIBoard *)board, 0x03, 0x04);

            D(0, "cb_LegacyIOBase 0x%x , MemoryBase 0x%x, MemorySize %u, IOBase 0x%x\n", legacyIOBase, Memory0,
              Memory0Size, Memory1);

            APTR physicalAddress = Prm_GetPhysicalAddress(Memory0);
            D(0, "prometheus.card: physicalAdress 0x%08lx\n", physicalAddress);

            struct ChipBase *ChipBase = NULL;

            struct BoardInfo boardInfo;
            memset(&boardInfo, 0, sizeof(boardInfo));
            struct BoardInfo *bi = &boardInfo;

            bi->ExecBase             = SysBase;
            bi->UtilBase             = UtilityBase;
            bi->CardPrometheusBase   = (ULONG)PrometheusBase;
            bi->CardPrometheusDevice = (ULONG)board;
            bi->ChipBase             = ChipBase;

            // Block IO is in BAR1
            bi->RegisterBase = Memory1 + REGISTER_OFFSET;
            // MMIO registers are in top 1kb of the first 8mb memory window
            bi->MemoryIOBase = Memory0 + 0x800000 - 1024 + MMIOREGISTER_OFFSET;
            // Framebuffer is at address 0x0
            bi->MemoryBase = Memory0;

            D(0, "Mach64 init chip....\n");
            InitChip(bi);
            D(0, "Mach64 has %ukb usable memory\n", bi->MemorySize / 1024);

            bi->MemorySpaceBase = Memory0;
            bi->MemorySpaceSize = Memory0Size;

            {
                DFUNC(0, "SetDisplay OFF\n");
                SetDisplay(bi, FALSE);
            }
            {
                // test 640x480 screen
                struct ModeInfo mi;

                mi.Depth            = 8;
                mi.Flags            = 0;
                mi.Height           = 480;
                mi.Width            = 680;
                mi.HorBlankSize     = 0;
                mi.HorEnableSkew    = 0;
                mi.HorSyncSize      = 96;
                mi.HorSyncStart     = 16;
                mi.HorTotal         = 800;
                mi.PixelClock       = 25175000;
                mi.pll1.Numerator   = 190;
                mi.pll2.Denominator = 2;
                mi.VerBlankSize     = 0;
                mi.VerSyncSize      = 2;
                mi.VerSyncStart     = 10;
                mi.VerTotal         = 525;

                bi->ModeInfo = &mi;

                ULONG index = ResolvePixelClock(bi, &mi, mi.PixelClock, RGBFB_CLUT);

                DFUNC(0, "SetClock\n");

                SetClock(bi);

                DFUNC(0, "SetGC\n");

                SetGC(bi, &mi, TRUE);
            }
            {
                DFUNC(0, "SetDAC\n");
                SetDAC(bi, RGBFB_CLUT);
            }
            {
                DFUNC(0, "SetColorArray\n");
                UBYTE colors[256 * 3];
                for (int c = 0; c < 256; c++) {
                    bi->CLUT[c].Red   = c;
                    bi->CLUT[c].Green = c;
                    bi->CLUT[c].Blue  = c;
                }
                SetColorArray(bi, 0, 256);
            }
            {
                SetPanning(bi, bi->MemoryBase, 640, 480, 0, 0, RGBFB_CLUT);
            }
            {
                DFUNC(0, "SetDisplay ON\n");
                SetDisplay(bi, TRUE);
            }

            for (int y = 0; y < 480; y++) {
                for (int x = 0; x < 640; x++) {
                    *(volatile UBYTE *)(bi->MemoryBase + y * 640 + x) = x;
                }
            }

            struct RenderInfo ri;
            ri.Memory      = bi->MemoryBase;
            ri.BytesPerRow = 640;
            ri.RGBFormat   = RGBFB_CLUT;

            {
                FillRect(bi, &ri, 100, 100, 640 - 200, 480 - 200, 0xFF, 0xFF, RGBFB_CLUT);
            }

            {
                UWORD patternData[] = {0xAAAA, 0x5555, 0x3333, 0xCCCC};
                struct Pattern pattern;
                pattern.BgPen    = 127;
                pattern.FgPen    = 255;
                pattern.DrawMode = JAM2;
                pattern.Size     = 2;
                pattern.Memory   = patternData;
                pattern.XOffset  = 0;
                pattern.YOffset  = 0;

                BlitPattern(bi, &ri, &pattern, 150, 150, 640 - 300, 480 - 300, 0xFF, RGBFB_CLUT);
            }

            // RegisterOwner(cb, board, (struct Node *)ChipBase);

            // if ((dmaSize > 0) && (dmaSize <= bi->MemorySize)) {
            //     // Place DMA window at end of memory window 0 and page-align it
            //     ULONG dmaOffset = (bi->MemorySize - dmaSize) & ~(4096 - 1);
            //     InitDMAMemory(cb, bi->MemoryBase + dmaOffset, dmaSize);
            //     bi->MemorySize = dmaOffset;
            //     cb->cb_DMAMemGranted = TRUE;
            // }
            // no need to continue - we have found a match
            rval = EXIT_SUCCESS;
            goto exit;
        }
    }  // while

    D(0, "no Mach64 found.\n");

exit:
    if (PrometheusBase)
        CloseLibrary(PrometheusBase);

    return EXIT_FAILURE;
}
#endif  // TESTEXE
