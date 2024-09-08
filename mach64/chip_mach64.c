#include "chip_mach64.h"

#include <proto/prometheus.h>

#include <string.h>  // memcmp

#define PCI_VENDOR 0x1002

/******************************************************************************/
/*                                                                            */
/* library exports                                                                    */
/*                                                                            */
/******************************************************************************/

const char LibName[] = "ATIMach64.chip";
const char LibIdString[] = "ATIMach64 Picasso96 chip driver version 1.0";

const UWORD LibVersion = 1;
const UWORD LibRevision = 0;

/*******************************************************************************/

int debugLevel = 15;

static const struct svga_pll mach64_pll = {3, 129, 0x80, 0xFF, 0, 3, 100000, 200000, 14318};

static PLLTable_t g_pllTable;
static MaxColorDepthTableEntry_t g_maxCDepthTable[50];
static MaxColorDepthTableEntry_t g_maxCDepthSecondTable[50];  // JY 04/07/94

void printFrequencyTable(FrequencyTable_t *ft)
{
    D(0, "Frequency Table ID: 0x%02X\n", (ULONG)ft->frequency_table_id);
    D(0, "Minimum PCLK Frequency: %u KHz\n", (ULONG)swapw(ft->min_pclk_freq) * 10);
    D(0, "Maximum PCLK Frequency: %u KHz\n", (ULONG)swapw(ft->max_pclk_freq) * 10);
    D(0, "Extended Coprocessor Mode: 0x%02X\n", (ULONG)ft->extended_coprocessor_mode);
    D(0, "Extended VGA Mode: 0x%02X\n", (ULONG)ft->extended_vga_mode);
    D(0, "Reference Clock Frequency: %u KHz\n", (ULONG)swapw(ft->ref_clock_freq) * 10);
    D(0, "Reference Clock Divider: %u\n", (ULONG)swapw(ft->ref_clock_divider));
    D(0, "Hardware Specific Information: 0x%04X\n", (ULONG)swapw(ft->hardware_specific_info));
    D(0, "MCLK Frequency (Power Down Mode): %u KHz\n", (ULONG)swapw(ft->mclk_freq_power_down) * 10);
    D(0, "MCLK Frequency (Normal DRAM Mode): %u KHz\n", (ULONG)swapw(ft->mclk_freq_normal_dram) * 10);
    D(0, "MCLK Frequency (Normal VRAM Mode): %u KHz\n", (ULONG)swapw(ft->mclk_freq_normal_vram) * 10);
    D(0, "SCLK Frequency: %u KHz\n", (ULONG)swapw(ft->sclk_freq) * 10);
    D(0, "MCLK Entry Number: 0x%02X\n", ft->mclk_entry_num);
    D(0, "SCLK Entry Number: 0x%02X\n", ft->sclk_entry_num);
    if (ft->coprocessor_mode_mclk_freq != 0) {
        D(0, "Coprocessor Mode MCLK Frequency: %u KHz\n", (ULONG)swapw(ft->coprocessor_mode_mclk_freq) * 10);
    }
    D(0, "Reserved: 0x%04X\n", (ULONG)swapw(ft->reserved));
    D(0, "Terminator: 0x%04X\n", (ULONG)swapw(ft->terminator));
}

UBYTE getDACType(BoardInfo_t *bi)
{
    REGBASE();
    // ULONG cfgStat0 = R_BLKIO_L(CONFIG_STAT0);
    // return (cfgStat0>>8) & 0x7;
    // return (R_BLKIO_B(CONFIG_STAT0, 1) & 0x7);
    return (R_BLKIO_B(DAC_CNTL, 2) & 0x7);
}

void printPLLTable(PLLTable_t *pllTable)
{
    for (int i = 0; i < 20; i++) {
        D(0, "PLL[%d] = %lu\n", i, (ULONG)pllTable->pllValues[i]);
    }
}

void printCdepthTable(MaxColorDepthTableEntry_t *table)
{
    for (int i = 0; table[i].h_disp != 0; i++) {
        D(0, "MaxColorDepthTableEntry[%d]:\n", i);
        D(0, "  h_disp: %u\n", table[i].h_disp);
        D(0, "  dacmask: %u\n", table[i].dacmask);
        D(0, "  ram_req: %u\n", table[i].ram_req);
        D(0, "  max_dot_clk: %u\n", table[i].max_dot_clk);
        D(0, "  color_depth: %u\n", table[i].color_depth);
    }
}

Mach64RomHeader_t *findRomHeader(struct BoardInfo *bi)
{
#define ROM_WORD(offset) (swapw(*(UWORD *)(romBase + (offset))))
#define ROM_BYTE(offset) (*(romBase + (offset)))
#define ROM_TABLE(name, type, offset) type *name = (type *)(romBase + (offset))

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
    getChipData(bi)->referenceDivider = swapw(freqTable->ref_clock_divider);
    getChipData(bi)->memClock = swapw(freqTable->mclk_freq_normal_dram);

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
    PLL_MACRO_CNTL = 1,
    PLL_REF_DIV = 2,
    PLL_GEN_CNTL = 3,
    PLL_MCLK_FB_DIV = 4,
    PLL_VCLK_CNTL = 5,
    PLL_VCLK_POST_DIV = 6,
    PLL_VCLK0_FB_DIV = 7,
    PLL_VCLK1_FB_DIV = 8,
    PLL_VCLK2_FB_DIV = 9,
    PLL_VCLK3_FB_DIV = 10,
    PLL_XCLK_CNTL = 11,
    PLL_FCP_CNTL = 12,
    PLL_VFC_CNTL = 13,
};

#define PLL_ADDR_MASK (0xF << 10)
#define PLL_ADDR(x) ((x) << 10)
#define PLL_DATA_MASK (0xFF << 16)
#define PLL_DATA(x) ((x) << 16)
#define PLL_WR_ENABLE BIT(9)
#define PLL_WR_ENABLE_MASK BIT(9)

void WritePLL(struct BoardInfo *bi, UBYTE pllAddr, UBYTE pllDataMask, UBYTE pllData)
{
    REGBASE();

    DFUNC(8, "pllAddr: %d, pllDataMask: 0x%02X, pllData: 0x%02X\n", (ULONG)pllAddr, (ULONG)pllDataMask, (ULONG)pllData);

    // FIXME: its possible older Mach chips want 8bit access here
    ULONG oldClockCntl = R_BLKIO_L(CLOCK_CNTL);
    ULONG clockCntl = oldClockCntl & ~(PLL_ADDR_MASK | PLL_DATA_MASK | PLL_WR_ENABLE_MASK);  // Clear PLL_ADDR, PLL_DATA

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
    ULONG oldClockCntl = R_BLKIO_L(CLOCK_CNTL);
    ULONG clockCntl =
        oldClockCntl & ~(PLL_ADDR_MASK | PLL_DATA_MASK | PLL_WR_ENABLE_MASK);  // Clear PLL_ADDR, PLL_DATA, PLL_WR_EN

    // Set PLL Adress
    clockCntl |= PLL_ADDR(pllAddr);
    W_BLKIO_L(CLOCK_CNTL, clockCntl);
    // Read back data
    clockCntl = R_BLKIO_L(CLOCK_CNTL);

    UBYTE pllValue = (clockCntl >> 16) & 0xFF;

    DFUNC(8, "pllValue: %d\n", (ULONG)pllValue);

    return pllValue;
}

#define WRITE_PLL(pllAddr, data) WritePLL(bi, (pllAddr), 0xFF, (data))
#define WRITE_PLL_MASK(pllAddr, mask, data) WritePLL(bi, (pllAddr), (mask), (data))
#define READ_PLL(pllAddr) ReadPLL(bi, (pllAddr))

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
typedef struct PLLValue
{
    UBYTE Plog2;  // post divider log2
    UBYTE N;      // feedback divider
} PLLValue_t;

ULONG computeFrequency(UWORD R, UBYTE N, UWORD M, UBYTE P)
{
    return ((ULONG)2 * R * N) / (M << P);
}

ULONG computePLLValues(BoardInfo_t *bi, ULONG targetFrequency, PLLValue_t *pllValues)
{
    UWORD R = getChipData(bi)->referenceFrequency;
    UWORD M = getChipData(bi)->referenceDivider;

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

    pllValues->N = N;
    pllValues->Plog2 = P;

    ULONG outputFreq = computeFrequency(R, N, M, P);

    DFUNC(5, "target: %ld0 KHz, Output: %ld0 KHz, R: %ld0 KHz, M: %ld, P: %d, N: %d\n", targetFrequency, outputFreq, R,
          M, (ULONG)1 << P, (ULONG)N);

    return outputFreq;

failure:
    DFUNC(0, "target frequency out of range:  %ld0Khz", targetFrequency);
    return 0;
}

#define PLL_OVERRIDE BIT(0)
#define PLL_OVERRIDE_MASK BIT(0)
#define PLL_MRESET BIT(1)
#define PLL_MRESET_MASK BIT(1)
#define OSC_EN BIT(2)
#define OSC_EN_MASK BIT(2)
#define MCLK_SRC_SEL(x) (((x) & 7) << 4)
#define MCLK_SRC_SEL_MASK (7 << 4)

void printMemoryClock(BoardInfo_t *bi)
{
    UBYTE refDiv = READ_PLL(PLL_REF_DIV);
    UBYTE fbDiv = READ_PLL(PLL_MCLK_FB_DIV);
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

    ULONG mClock = computeFrequency(getChipData(bi)->referenceFrequency, fbDiv, refDiv, mClkSrcSel);

    DFUNC(5, "Current memory clock source: %s, PLL frequency: %ld0 KHz, R: %ld0 KHz, M: %ld, P: %d, N: %d\n", mClockSrc,
          mClock, (ULONG)getChipData(bi)->referenceFrequency, (ULONG)refDiv, (ULONG)1 << mClkSrcSel, (ULONG)fbDiv);
}

void SetMemoryClock(BoardInfo_t *bi, USHORT kHz10)
{
    PLLValue_t pllValues;
    if (computePLLValues(bi, kHz10, &pllValues)) {
        DFUNC(5, "\n");
        WRITE_PLL_MASK(PLL_GEN_CNTL, (PLL_OVERRIDE_MASK | PLL_MRESET_MASK | OSC_EN_MASK | MCLK_SRC_SEL_MASK),
                       (OSC_EN | MCLK_SRC_SEL(0b100)));
        WRITE_PLL(PLL_MCLK_FB_DIV, pllValues.N);
        delayMilliSeconds(5);
        WRITE_PLL_MASK(PLL_GEN_CNTL, MCLK_SRC_SEL_MASK, MCLK_SRC_SEL(pllValues.Plog2));
        printMemoryClock(bi);
    } else {
        DFUNC(0, "Unable to compute PLL values for %d0 KHz\n", kHz10);
    }
}

BOOL TestMemory(BoardInfo_t bi) {}

void InitPLL(BoardInfo_t *bi)
{
    WRITE_PLL(PLL_MACRO_CNTL, 0xa0);  // PLL_DUTY_CYC = 5

    WRITE_PLL(PLL_VFC_CNTL, 0x1b);
    WRITE_PLL(PLL_REF_DIV, getChipData(bi)->referenceDivider);
    WRITE_PLL(PLL_VCLK_CNTL, 0x00);
    WRITE_PLL(PLL_FCP_CNTL, 0xc0);
    WRITE_PLL(PLL_XCLK_CNTL, 0x00);
    WRITE_PLL(PLL_VCLK_POST_DIV, 0x9c);
    WRITE_PLL(PLL_VCLK_CNTL, 0x0b);
    WRITE_PLL(PLL_MACRO_CNTL, 0x90);
}

#define CFG_VGA_DIS BIT(19)
#define CFG_VGA_DIS_MASK BIT(19)
#define CFG_MEM_VGA_AP_EN BIT(2)
#define CFG_MEM_VGA_AP_EN_MASK BIT(2)
#define CFG_MEM_AP_LOC(x) ((x) << 4)
#define CFG_MEM_AP_LOC_MASK (0x3FF << 4)

#define VGA_128KAP_PAGING BIT(20)
#define VGA_128KAP_PAGING_MASK BIT(20)
#define CRTC_EXT_DISP_EN BIT(24)
#define CRTC_EXT_DISP_EN_MASK BIT(24)
#define VGA_ATI_LINEAR BIT(27)
#define VGA_ATI_LINEAR_MASK BIT(27)

#define CFG_VGA_EN BIT(4)
#define CFG_VGA_EN_MASK BIT(4)
#define CFG_MEM_TYPE(x) ((x) & 7)
#define CFG_MEM_TYPE_MASK (7)
#define CFG_MEM_TYPE_DISABLE b000
#define CFG_MEM_TYPE_DRAM 0b001
#define CFG_MEM_TYPE_EDO 0b010
#define CFG_MEM_TYPE_PSEUDO_EDO 0b011
#define CFG_MEM_TYPE_SDRAM 0b100
#define CFG_DUAL_CAS_EN BIT(3)
#define CFG_DUAL_CAS_EN_MASK BIT(3)

BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    REGBASE();
    MMIOBASE();
    LOCAL_SYSBASE();

    DFUNC(0, "\n");

    bi->GraphicsControllerType = GCT_ATIRV100;
    bi->PaletteChipType = PCT_ATT_20C492;
    bi->Flags = bi->Flags | BIF_NOMEMORYMODEMIX | BIF_BORDERBLANK | BIF_BLITTER | BIF_GRANTDIRECTACCESS |
                BIF_VGASCREENSPLIT | BIF_HASSPRITEBUFFER | BIF_HARDWARESPRITE;

    bi->RGBFormats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;
    // We can support byte-swapped formats on this chip via the Big Linear
    // Adressing Window
    bi->RGBFormats |= RGBFF_A8R8G8B8 | RGBFF_R5G6B5 | RGBFF_R5G5B5;

    // We don't support these modes, but if we did, they would not allow for a HW
    // sprite
    bi->SoftSpriteFlags = RGBFF_B8G8R8 | RGBFF_R8G8B8;

    // bi->SetGC = SetGC;
    // bi->SetPanning = SetPanning;
    // bi->CalculateBytesPerRow = CalculateBytesPerRow;
    // bi->CalculateMemory = CalculateMemory;
    // bi->GetCompatibleFormats = GetCompatibleFormats;
    // bi->SetDAC = SetDAC;
    // bi->SetColorArray = SetColorArray;
    // bi->SetDisplay = SetDisplay;
    // bi->SetMemoryMode = SetMemoryMode;
    // bi->SetWriteMask = SetWriteMask;
    // bi->SetReadPlane = SetReadPlane;
    // bi->SetClearMask = SetClearMask;
    // bi->ResolvePixelClock = ResolvePixelClock;
    // bi->GetPixelClock = GetPixelClock;
    // bi->SetClock = SetClock;

    // // VSYNC
    // bi->WaitVerticalSync = WaitVerticalSync;
    // bi->GetVSyncState = GetVSyncState;

    // // DPMS
    // bi->SetDPMSLevel = SetDPMSLevel;

    // // VGA Splitscreen
    // bi->SetSplitPosition = SetSplitPosition;

    // // Mouse Sprite
    // bi->SetSprite = SetSprite;
    // bi->SetSpritePosition = SetSpritePosition;
    // bi->SetSpriteImage = SetSpriteImage;
    // bi->SetSpriteColor = SetSpriteColor;

    // // Blitter acceleration
    // bi->WaitBlitter = WaitBlitter;
    // bi->BlitRect = BlitRect;
    // bi->InvertRect = InvertRect;
    // bi->FillRect = FillRect;
    // bi->BlitTemplate = BlitTemplate;
    // bi->BlitPlanar2Chunky = BlitPlanar2Chunky;
    // bi->BlitRectNoMaskComplete = BlitRectNoMaskComplete;
    // bi->DrawLine = DrawLine;
    // bi->BlitPattern = BlitPattern;

    DFUNC(15,
          "WaitBlitter 0x%08lx\nBlitRect 0x%08lx\nInvertRect 0x%08lx\nFillRect "
          "0x%08lx\n"
          "BlitTemplate 0x%08lx\n BlitPlanar2Chunky 0x%08lx\n"
          "BlitRectNoMaskComplete 0x%08lx\n DrawLine 0x%08lx\n",
          bi->WaitBlitter, bi->BlitRect, bi->InvertRect, bi->FillRect, bi->BlitTemplate, bi->BlitPlanar2Chunky,
          bi->BlitRectNoMaskComplete, bi->DrawLine);

    bi->PixelClockCount[PLANAR] = 0;
    bi->PixelClockCount[CHUNKY] = 135;
    bi->PixelClockCount[HICOLOR] = 80;
    bi->PixelClockCount[TRUECOLOR] = 80;
    bi->PixelClockCount[TRUEALPHA] = 80;

    // Informed by the largest X/Y coordinates the blitter can talk to
    bi->MaxBMWidth = 2048;
    bi->MaxBMHeight = 2048;

    bi->BitsPerCannon = 6;
    bi->MaxHorValue[PLANAR] = 4088;  // 511 * 8dclks
    bi->MaxHorValue[CHUNKY] = 4088;
    bi->MaxHorValue[HICOLOR] = 8176;     // 511 * 8 * 2
    bi->MaxHorValue[TRUECOLOR] = 16352;  // 511 * 8 * 4
    bi->MaxHorValue[TRUEALPHA] = 16352;

    bi->MaxVerValue[PLANAR] = 2047;
    bi->MaxVerValue[CHUNKY] = 2047;
    bi->MaxVerValue[HICOLOR] = 2047;
    bi->MaxVerValue[TRUECOLOR] = 2047;
    bi->MaxVerValue[TRUEALPHA] = 2047;

    // Determined by 10bit value divided by bpp
    bi->MaxHorResolution[PLANAR] = 1600;
    bi->MaxVerResolution[PLANAR] = 1600;

    bi->MaxHorResolution[CHUNKY] = 1600;
    bi->MaxVerResolution[CHUNKY] = 1600;

    bi->MaxHorResolution[HICOLOR] = 1280;
    bi->MaxVerResolution[HICOLOR] = 1280;

    bi->MaxHorResolution[TRUECOLOR] = 1280;
    bi->MaxVerResolution[TRUECOLOR] = 1280;

    bi->MaxHorResolution[TRUEALPHA] = 1280;
    bi->MaxVerResolution[TRUEALPHA] = 1280;

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
        BOOL blockIO = !!(config & 0x04);
        BOOL enableGENENA = !(config & 0x08);

        D(0, "PCI config: ioBase : 0x%lx, sparse/block IO : %ld, genableGENENA : %ld\n", (ULONG)ioBase, (ULONG)blockIO,
          (ULONG)enableGENENA);
    }

    // Test scratch register response
    ULONG saveScratchReg1 = R_BLKIO_L(SCRATCH_REG1);
    W_BLKIO_L(SCRATCH_REG1, 0xAAAAAAAA);
    ULONG scratchA = R_BLKIO_L(SCRATCH_REG1);
    W_BLKIO_L(SCRATCH_REG1, 0x55555555);
    ULONG scratch5 = R_BLKIO_L(SCRATCH_REG1);
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

    for (int r = 0; r < ARRAY_SIZE(defaultRegs); r+=2) {
        D(10, "[%lX_%ldh] = 0x%lx\n", (ULONG)defaultRegs[r] / 4, (ULONG)defaultRegs[r] % 4,(ULONG)defaultRegs[r + 1]);
        W_IO_W(defaultRegs[r], defaultRegs[r + 1]);
    }

    ULONG clock = bi->MemoryClock;
    if (clock < 33000000) {
        clock = 33000000;
    }
    if (68000000 < clock) {
        clock = 68000000;
    }

    R_BLKIO_L(CONFIG_STAT0);
    W_BLKIO_L(BUS_CNTL, BUS_FIFO_ERR_AK | BUS_HOST_ERR_AK);

    W_BLKIO_MASK_L(CONFIG_CNTL, CFG_VGA_DIS_MASK, CFG_VGA_DIS);
    W_BLKIO_MASK_L(CONFIG_STAT0, CFG_VGA_EN_MASK, 0);

    ULONG configCntl = R_BLKIO_L(CONFIG_CNTL);

    InitPLL(bi);

    SetMemoryClock(bi, getChipData(bi)->memClock);
    bi->MemoryClock = getChipData(bi)->memClock * 100;

    // These values are being set by the Mach64VT BIOS, but are inline with the code, not somewhere in an accessible table.
    // CONFIG_STAT0 is apparently not strapped to the right memory type and only by setting it here we get access
    // to the framebuffer memory.
    // W_BLKIO_MASK_L(CONFIG_STAT0, CFG_MEM_TYPE_MASK | CFG_DUAL_CAS_EN_MASK,
    //                CFG_MEM_TYPE(CFG_MEM_TYPE_EDO) | CFG_DUAL_CAS_EN);
    W_BLKIO_MASK_B(CONFIG_STAT0, 0, 0xf8, 0x3b);

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
    framebuffer[0] = 0;

    int memSizeIdx = 0b11;
    while (memSizeIdx >= 0) {
        D(1, "Probing memory size %ld\n", bi->MemorySize);

        W_BLKIO_MASK_L(MEM_CNTL, 0x7, memSizeIdx);

        CacheClearU();

        // Probe the last and the first longword for the current segment,
        // as well as offset 0 to check for wrap arounds
        volatile ULONG *highOffset = framebuffer + (bi->MemorySize >> 2) - 512 - 1;
        volatile ULONG *lowOffset = framebuffer + (bi->MemorySize >> 3);
        // Probe  memory
        *framebuffer = 0;
        *highOffset = (ULONG)highOffset;
        *lowOffset = (ULONG)lowOffset;

        CacheClearU();

        ULONG readbackHigh = *highOffset;
        ULONG readbackLow = *lowOffset;
        ULONG readbackZero = *framebuffer;

        D(10, "Probing memory at 0x%lx ?= 0x%lx; 0x%lx ?= 0x%lx, 0x0 ?= 0x%lx\n", highOffset, readbackHigh, lowOffset,
          readbackLow, readbackZero);

        if (readbackHigh == (ULONG)highOffset && readbackLow == (ULONG)lowOffset && readbackZero == 0) {
            break;
        }
        // reduce available memory size
        bi->MemorySize >>= 1;
        memSizeIdx -= 1;
    }

    D(1, "MemorySize: %ldmb\n", bi->MemorySize / (1024 * 1024));

    MEM_CNTL_Register memCntl;
    *(ULONG *)&memCntl = R_BLKIO_L(MEM_CNTL);
    print_MEM_CNTL_Register(&memCntl);

    // Input Status ? Register (STATUS_O)
    // D(1, "Monitor is %s present\n", (!(R_REG(0x3C2) & 0x10) ? "" : "NOT"));

    // Two sprite images, each 64x64*2 bits
    const ULONG maxSpriteBuffersSize = (64 * 64 * 2 / 8) * 2;

    // take sprite image data off the top of the memory
    // sprites can be placed at segment boundaries of 1kb
    bi->MemorySize = (bi->MemorySize - maxSpriteBuffersSize) & ~(1024 - 1);
    bi->MouseImageBuffer = bi->MemoryBase + bi->MemorySize;
    bi->MouseSaveBuffer = bi->MemoryBase + bi->MemorySize + maxSpriteBuffersSize / 2;

    return TRUE;
}

#ifdef TESTEXE

#include <boardinfo.h>
#include <libraries/prometheus.h>
#include <proto/expansion.h>
#include <proto/prometheus.h>
#include <proto/utility.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VENDOR_E3B 3643
#define VENDOR_MATAY 44359
#define DEVICE_FIRESTORM 200
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

            bi->ExecBase = SysBase;
            bi->UtilBase = UtilityBase;
            bi->CardPrometheusBase = (ULONG)PrometheusBase;
            bi->CardPrometheusDevice = (ULONG)board;
            bi->ChipBase = ChipBase;

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
