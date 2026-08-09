#include "chip_mach64.h"
#include "edid_common.h"
#include "mach64CT.h"
#include "mach64GT.h"
#include "mach64GX.h"
#include "mach64VT.h"
#include "mach64_common.h"
#include "mach64_eeprom.h"
#include "mach64_i2c.h"

#include <graphics/rastport.h>
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/openpci.h>

#include <string.h>  // memcmp

using namespace MmioReg;

#ifdef __cplusplus
extern "C" {
#endif

#define PCI_VENDOR 0x1002

/******************************************************************************/
/*                                                                            */
/* library exports                                                                    */
/*                                                                            */
/******************************************************************************/

#if MACH64_PCI_RETRY
extern const char LibName[]     = "ATIMach64.chip";
extern const char LibIdString[] = "ATIMach64 Picasso96 chip driver version 1.0";
#else
extern const char LibName[]     = "ATIMach64GX.chip";
extern const char LibIdString[] = "ATIMach64GX Picasso96 chip driver version 1.0";
#endif

#ifndef LIB_VERSION
#define LIB_VERSION 1
#endif
#ifndef LIB_REVISION
#define LIB_REVISION 0
#endif
extern const UWORD LibVersion  = LIB_VERSION;
extern const UWORD LibRevision = LIB_REVISION;

/*******************************************************************************/

int debugLevel = VERBOSE;

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
typedef struct PLLTable
{
    USHORT pllValues[20];
} PLLTable_t;
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
    DRIVER_LOCALS(bi);
    return (mmio.readB(DAC_CNTL, 2) & 0x7);
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

const Mach64RomHeader_t *parseRomHeader(struct BoardInfo *bi)
{
#define ROM_WORD(offset)              (swapw(*(UWORD *)(romBase + (offset))))
#define ROM_BYTE(offset)              (*(romBase + (offset)))
#define ROM_TABLE(name, type, offset) const type *name = (type *)(romBase + (offset))

    LOCAL_OPENPCIBASE();

    UBYTE *romBase = NULL;
    GetBoardAttrs(getCardData(bi)->board, PRM_ROM_Address, (Tag)&romBase, TAG_END);
    if (!romBase) {
        DFUNC(ERROR, "Unable to get ROM address\n");
        return NULL;
    }

    ROM_TABLE(romHeader, OptionRomHeader_t, 0);
    if (swapw(romHeader->signature) != 0xaa55) {
        DFUNC(ERROR, "Unable find OptionROM signature at 0x%lx\n", &romHeader->signature);
        return NULL;
    }

    ROM_TABLE(pciData, PciRomData_t, swapw(romHeader->pcir_offset));
    if (memcmp(pciData->signature, "PCIR", 4) != 0) {
        DFUNC(ERROR, "Unable find PCIR signature at 0x%lx\n", romHeader->pcir_offset);
        return NULL;
    }

    WORD atiRomHeaderOffset = ROM_WORD(0x48);

    ROM_TABLE(mach64RomHeader, Mach64RomHeader_t, atiRomHeaderOffset - 2);

    const char *logOnMessage = (const char *)(romBase + swapw(mach64RomHeader->logon_message_ptr));
    // Doesn't seem to point to anything useful
    const char *configString = (const char *)(romBase + swapw(mach64RomHeader->config_string_ptr));

    UWORD ioBase       = swapw(mach64RomHeader->io_base_address);
    UWORD ioSparseBase = swapw(mach64RomHeader->io_address_sparse);

    D(0, "ATI Mach64 ROM header found at offset 0x%lx, Block IO Base Address 0x%lx, Sparse IO Base: 0x%lx\n%s\n",
      (ULONG)atiRomHeaderOffset, (ULONG)ioBase, (ULONG)ioSparseBase, logOnMessage);

    USHORT freqTableOffset = swapw(mach64RomHeader->freq_table_ptr);
    ROM_TABLE(freqTable, FrequencyTable_t, freqTableOffset);
    printFrequencyTable(freqTable);

    USHORT pllTableOffset = ROM_WORD(freqTableOffset - 2);
    ROM_TABLE(pllTable, PLLTable_t, pllTableOffset);
    for (int i = 0; i < 20; i++) {
        g_pllTable.pllValues[i] = swapw(pllTable->pllValues[i]);
    }
    printPLLTable(&g_pllTable);

    ChipData_t *cd     = getChipData(bi);
    ChipSpecific_t *cs = getChipSpecific(bi);

    cd->ioSparseBase       = ioSparseBase;
    cs->referenceFrequency = swapw(freqTable->ref_clock_freq);
    cs->referenceDivider   = swapw(freqTable->ref_clock_divider);
    cs->memClock           = swapw(freqTable->mclk_freq_normal_dram);
    cs->minPClock          = swapw(freqTable->min_pclk_freq);
    cs->maxPClock          = swapw(freqTable->max_pclk_freq);
    cs->minMClock          = swapw(freqTable->mclk_freq_power_down);
    cs->maxDRAMClock       = swapw(freqTable->mclk_freq_normal_dram);
    cs->maxVRAMClock       = swapw(freqTable->mclk_freq_normal_vram);

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

// void printMemoryClock(BoardInfo_t *bi)
// {
//     UBYTE refDiv     = READ_PLL(PLL_REF_DIV);
//     UBYTE fbDiv      = READ_PLL(PLL_MCLK_FB_DIV);
//     UBYTE mClkSrcSel = (READ_PLL(PLL_GEN_CNTL) >> 4) & 7;

//     const char *mClockSrc = "PLLMCLK";
//     switch (mClkSrcSel) {
//     case 0b100:
//         mClockSrc = "CPUCLK";
//         break;
//     case 0b101:
//         mClockSrc = "DCLK";
//         break;
//     case 0b110:
//         mClockSrc = "PLLREFCLK";
//         break;
//     case 0b111:
//         mClockSrc = "XTALIN";
//         break;
//     default:
//         break;
//     }

//     mClkSrcSel &= 3;

//     ULONG xclkCntl = READ_PLL(PLL_XCLK_CNTL);
//     if (xclkCntl & MFB_TIMES_4_2b) {
//         fbDiv <<= 1;
//     }
//     const ChipSpecific_t *cs = getConstChipSpecific(bi);
//     ULONG mClock             = ComputeFrequencyKhz10(cs->referenceFrequency, fbDiv, refDiv, mClkSrcSel);

//     DFUNC(5, "clock source: %s, PLL frequency: %ld0 KHz, R: %ld0 KHz, M: %ld, P: %ld, N: %ld\n", mClockSrc, mClock,
//           (ULONG)cs->referenceFrequency, (ULONG)refDiv, (ULONG)1 << mClkSrcSel, (ULONG)fbDiv);
// }

UWORD resolveMemoryClockKhz10(BoardInfo_t *bi)
{
    const ChipSpecific_t *cs = getConstChipSpecific(bi);
    ChipFamily_t family      = getChipData(bi)->chipFamily;
    ULONG khz10;

    /* P96 MemoryClock is Hz; ROM table entries are 10 kHz units. */
    if (bi->MemoryClock)
        khz10 = bi->MemoryClock / 10000UL;
    else if (family >= MACH64GT)
        khz10 = 10000UL; /* previous initClocks default (100 MHz) */
    else if (cs->memClock)
        khz10 = cs->memClock;
    else if (cs->maxDRAMClock)
        khz10 = cs->maxDRAMClock;
    else
        khz10 = 5050UL; /* ~50.5 MHz CT ROM DRAM entry */

    if (cs->minMClock && khz10 < cs->minMClock)
        khz10 = cs->minMClock;

    {
        UWORD max = cs->maxDRAMClock;
        if (family >= MACH64GT) {
            if (cs->maxVRAMClock > max)
                max = cs->maxVRAMClock;
            if (max < 10000)
                max = 10000;
        } else if (!max) {
            max = 6800;
        }
        if (khz10 > max)
            khz10 = max;
    }

    return (UWORD)khz10;
}

void SetMemoryClock(BoardInfo_t *bi, UWORD freqKhz10)
{
    ChipFamily_t family = getChipData(bi)->chipFamily;

    DFUNC(INFO, "MCLK request %ld0 kHz (family %s)\n", (ULONG)freqKhz10, getChipFamilyName(family));

#if !MACH64_PCI_RETRY
    if (family == MACH64CT)
        SetMemoryClock_CT(bi, freqKhz10);
        /* GX: factory ICS2595 MCLK — do not reprogram. */
#else
    if (family == MACH64VT)
        SetMemoryClock_VT(bi, freqKhz10);
    else if (family >= MACH64GT)
        SetMemoryClock_GT(bi, freqKhz10);
#endif

    bi->MemoryClock = (ULONG)freqKhz10 * 10000UL;
}

#define DAC_W_INDEX 0
#define DAC_W_DATA  1
#define DAC_MASK    2
#define DAC_R_INDEX 3

#define DAC_VGA_ADR_EN      BIT(13)
#define DAC_VGA_ADR_EN_MASK BIT(13)

UWORD ASM Mach64Driver::calculateBytesPerRow(__REGD0(UWORD width), __REGD1(UWORD height), __REGA1(struct ModeInfo *mi),
                                             __REGD7(RGBFTYPE_REG format))
{
    // Pitch is a multiple of 8 bytes
    UBYTE bpp = getBPP(format);

    UWORD bytesPerRow = width * bpp;
#if MACH64_PCI_RETRY
    // RagePro manual says that SGRAM needs to be aligned to 64byte and pitch needs to be 64byte aligned
    bytesPerRow = (bytesPerRow + 63) & ~63;
#else
    bytesPerRow = (bytesPerRow + 7) & ~7;
#endif

    ULONG maxHeight = 2048;  // FIXME: check this value
    if (height > maxHeight) {
        return 0;
    }
    return bytesPerRow;
}

#define OVR_CLR_8(x)   (x)
#define OVR_CLR_8_MASK (0xFF)
#define OVR_CLR_B(x)   ((x) << 8)
#define OVR_CLR_B_MASK (0xFF << 8)
#define OVR_CLR_G(x)   ((x) << 16)
#define OVR_CLR_G_MASK (0xFF << 16)
#define OVR_CLR_R(x)   ((x) << 24)
#define OVR_CLR_R_MASK (0xFF << 24)

void Mach64Driver::writeOvrClr(UBYTE index8, UBYTE r, UBYTE g, UBYTE b)
{
    DRIVER_LOCALS(this);
    mmio.writeL(OVR_CLR, OVR_CLR_R(r) | OVR_CLR_G(g) | OVR_CLR_B(b) | OVR_CLR_8(index8));
}

void Mach64Driver::setColorArrayInternal(UWORD startIndex, UWORD count, const struct CLUTEntry *colors)
{
    DRIVER_LOCALS(this);
    struct ExecBase *SysBase = ExecBase;

    /* DAC auto-increments R→G→B→next index; an IRQ mid-sequence desyncs the
     * channel (false colors). Same rule as Mach32 SetColorArray. */
    Disable();

    mmio.writeB(DAC_REGS, DAC_W_INDEX, (UBYTE)startIndex);

    for (UWORD c = startIndex; c < startIndex + count; ++c) {
        /* Re-index each entry. Device read after index drains posted/coalesced
         * PCI writes (flushWrites/nop is not enough even with MAPP_IO MMIO). */
        (void)mmio.readB(DAC_REGS, DAC_MASK);
        mmio.writeB(DAC_REGS, DAC_W_DATA, colors[c].Red);
        mmio.writeB(DAC_REGS, DAC_W_DATA, colors[c].Green);
        mmio.writeB(DAC_REGS, DAC_W_DATA, colors[c].Blue);
    }

    Enable();

    /* 4/8 bpp overscan uses palette index 0; direct-color border is set in SetGC. */
    if (startIndex == 0 && count > 0 && (!ModeInfo || ModeInfo->Depth <= 8)) {
        writeOvrClr(0, colors[0].Red, colors[0].Green, colors[0].Blue);
    }
}

void ASM Mach64Driver::setColorArray(__REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    DFUNC(VERBOSE, "startIndex %ld, count %ld\n", (ULONG)startIndex, (ULONG)count);

    setColorArrayInternal(startIndex, count, CLUT);
}

void ASM Mach64Driver::setDAC(__REGD0(UWORD region), __REGD7(RGBFTYPE_REG format))
{
    DRIVER_LOCALS(this);

    DFUNC(VERBOSE, "format %ld\n", (ULONG)format);

    ULONG crtcGen = mmio.readL(CRTC_GEN_CNTL);
    crtcGen &= ~CRTC_PIX_WIDTH_MASK;
    crtcGen |= CRTC_PIX_WIDTH(g_bitWidths[format]);
    /* SetGC's CRTC_DBL_SCAN_EN can fail to stick on CT; always re-apply. */
    if (ModeInfo) {
        crtcGen &= ~(CRTC_DBL_SCAN_EN | CRTC_INTERLACE_EN | CRTC_PIC_BY_2_EN);
        if (ModeInfo->Flags & GMF_DOUBLESCAN)
            crtcGen |= CRTC_DBL_SCAN_EN;
        if (ModeInfo->Flags & GMF_INTERLACE)
            crtcGen |= CRTC_INTERLACE_EN;
    }
    mmio.writeL(CRTC_GEN_CNTL, crtcGen);
    if (RegisterBase) {
        blkIo().writeL(BlkIoReg::CRTC_GEN_CNTL, crtcGen);
    }
    {
        ULONG rb = mmio.readL(CRTC_GEN_CNTL);
        if (ModeInfo && (ModeInfo->Flags & GMF_DOUBLESCAN) && !(rb & CRTC_DBL_SCAN_EN)) {
            D(ALWAYS, "SetDAC: DBL_SCAN lost (wrote 0x%08lx read 0x%08lx)\n", crtcGen, rb);
        }
    }

    /* Integrated DAC: 8-bit LUT (BitsPerCannon=8). GX external DAC is SetDAC_GX. */
    if (chip()->chipFamily != MACH64GX) {
        mmio.writeMaskL(DAC_CNTL, DAC_8BIT_EN_MASK, DAC_8BIT_EN);
    }
    if (format != RGBFB_CLUT) {
        /* Do not put colors[256] on this stack frame — with BoardInfo on main's
         * stack, that 768-byte array blows the default 4K CLI stack (Guru). */
        static struct CLUTEntry colors[256];
        for (int c = 0; c < 256; c++) {
            colors[c].Red   = c;
            colors[c].Green = c;
            colors[c].Blue  = c;
        }
        setColorArrayInternal(0, 256, colors);
    } else {
        setColorArrayInternal(0, 256, CLUT);
    }
}

static INLINE REGARGS UWORD ToScanLines(UWORD y, UWORD modeFlags)
{
    /* GMF_DOUBLESCAN: ModeInfo V is logical (FB lines); CRTC wants physical
     * scanlines (×2) plus CRTC_DBL_SCAN_EN.
     * Interlace: programmed totals are half. */
    if (modeFlags & GMF_DOUBLESCAN)
        y *= 2;
    if (modeFlags & GMF_INTERLACE)
        y /= 2;
    return y;
}

static INLINE REGARGS UWORD AdjustBorder(UWORD x, BOOL border, UWORD defaultX)
{
    if (!border || x == 0)
        x = defaultX;
    return x;
}

#define TO_SCANLINES(y) ToScanLines((y), modeFlags)
#define TO_CHARS(x)     ((x + 7) >> 3)

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

#define CRTC_CRNT_VLINE_MASK (0x7FF << 16)

#define CRTC_OFFSET(x)   (x)
#define CRTC_OFFSET_MASK (0xFFFFF)
#define CRTC_PITCH(x)    ((x) << 22)
#define CRTC_PITCH_MASK  (0x3FF << 22)

#define CRTC_VBLANK        BIT(0)
#define CRTC_VBLANK_MASK   BIT(0)
#define CRTC_VBLANK_INT_EN BIT(1)
#define CRTC_VBLANK_INT    BIT(2)
#define CRTC_VBLANK_INT_AK CRTC_VBLANK_INT
#define CRTC_INT_ACKS      CRTC_VBLANK_INT
#define CRTC_INT_EN_MASK   CRTC_VBLANK_INT_EN

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

void ASM Mach64Driver::setGC(__REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    Mach64MmioQ mmio = mmioQ();
    ChipData_t *cd   = chip();

    BOOL isInterlaced;
    UBYTE modeFlags;

    DFUNC(INFO,
          "W %ld, H %ld,\n"
          "HTotal %ld, HBlankSize %ld, HSyncStart %ld, HSyncSize %ld,\n"
          "nVTotal %ld, VBlankSize %ld,  VSyncStart %ld ,  VSyncSize %ld\n",
          (ULONG)mi->Width, (ULONG)mi->Height, (ULONG)mi->HorTotal, (ULONG)mi->HorBlankSize, (ULONG)mi->HorSyncStart,
          (ULONG)mi->HorSyncSize, (ULONG)mi->VerTotal, (ULONG)mi->VerBlankSize, (ULONG)mi->VerSyncStart,
          (ULONG)mi->VerSyncSize);

    ModeInfo = mi;
    Border   = border;

    modeFlags    = mi->Flags;
    isInterlaced = !!(modeFlags & GMF_INTERLACE);

    ULONG crtcGenCntl = mmio.readL(CRTC_GEN_CNTL);
    crtcGenCntl &= ~(CRTC_DBL_SCAN_EN | CRTC_INTERLACE_EN | CRTC_PIC_BY_2_EN);
    if (isInterlaced)
        crtcGenCntl |= CRTC_INTERLACE_EN;
    if (modeFlags & GMF_DOUBLESCAN)
        crtcGenCntl |= CRTC_DBL_SCAN_EN;

    UWORD hTotalChars = TO_CHARS(mi->HorTotal) - 1;
    D(VERBOSE, "Horizontal Total %ld\n", (ULONG)hTotalChars);
    UWORD hDisplay = TO_CHARS(mi->Width) - 1;
    D(VERBOSE, "Display %ld\n", (ULONG)hDisplay);
    mmio.writeL(CRTC_H_TOTAL_DISP, CRTC_H_TOTAL(hTotalChars) | CRTC_H_DISP(hDisplay));

    UWORD hSyncStart = TO_CHARS(mi->HorSyncStart + mi->Width) - 1;
    D(VERBOSE, "HSync start %ld\n", (ULONG)hSyncStart);

    /* HorSyncSize is in pixels; CRTC_H_SYNC_WID is in characters. */
    UWORD hSyncWid = TO_CHARS(mi->HorSyncSize);
    ULONG crtcHSyncStrtWid =
        CRTC_H_SYNC_STRT(hSyncStart) | CRTC_H_SYNC_STRT_HI(hSyncStart >> 8) | CRTC_H_SYNC_WID(hSyncWid);
    if (modeFlags & GMF_HPOLARITY) {
        crtcHSyncStrtWid |= CRTC_H_SYNC_POL;
    }
    mmio.writeL(CRTC_H_SYNC_STRT_WID, crtcHSyncStrtWid);

    UWORD vTotal = TO_SCANLINES(mi->VerTotal) - 1;
    D(VERBOSE, "VTotal %ld\n", (ULONG)vTotal);
    UWORD vDisp = TO_SCANLINES(mi->Height) - 1;
    mmio.writeL(CRTC_V_TOTAL_DISP, CRTC_V_TOTAL(vTotal) | CRTC_V_DISP(vDisp));

    UWORD vSyncStart = TO_SCANLINES(mi->VerSyncStart + mi->Height) - 1;
    D(VERBOSE, "VSync Start %ld\n", (ULONG)vSyncStart);

    ULONG crtcVSyncStrtWid = CRTC_V_SYNC_STRT(vSyncStart) | CRTC_V_SYNC_WID(TO_SCANLINES(mi->VerSyncSize));
    if (modeFlags & GMF_VPOLARITY) {
        crtcVSyncStrtWid |= CRTC_V_SYNC_POL;
    }
    mmio.writeL(CRTC_V_SYNC_STRT_WID, crtcVSyncStrtWid);

    if (border) {
        UWORD hBorder = TO_CHARS(mi->HorBlankSize);
        UWORD vBorder = TO_SCANLINES(mi->VerBlankSize);
        mmio.writeL(OVR_WID_LEFT_RIGHT, OVR_WID_LEFT(hBorder) | OVR_WID_RIGHT(hBorder));
        mmio.writeL(OVR_WID_TOP_BOTTOM, OVR_WID_TOP(vBorder) | OVR_WID_BOT(vBorder));
    } else {
        mmio.writeL(OVR_WID_LEFT_RIGHT, 0);
        mmio.writeL(OVR_WID_TOP_BOTTOM, 0);
    }

    /* Overscan widths work in all depths. Color: LUT index 0 for 4/8 bpp,
     * black RGB for direct color (no palette index 0). */
    if (mi->Depth <= 8) {
        writeOvrClr(0, CLUT[0].Red, CLUT[0].Green, CLUT[0].Blue);
    } else {
        writeOvrClr(0, 0, 0, 0);
    }

    crtcGenCntl |= CRTC_ENABLE;
    crtcGenCntl &= ~CRTC_PIC_BY_2_EN;
    mmio.writeL(CRTC_GEN_CNTL, crtcGenCntl);
    /* CT: also program DBL_SCAN via block-I/O; MMIO dword alone may not stick. */
    if (RegisterBase) {
        blkIo().writeL(BlkIoReg::CRTC_GEN_CNTL, crtcGenCntl);
    }

    {
        ULONG rb = mmio.readL(CRTC_GEN_CNTL);
        if ((modeFlags & GMF_DOUBLESCAN) && !(rb & CRTC_DBL_SCAN_EN)) {
            D(ALWAYS, "CRTC_DBL_SCAN_EN did not stick (wrote 0x%08lx read 0x%08lx)\n", crtcGenCntl, rb);
        } else {
            D(ALWAYS, "CRTC_GEN_CNTL 0x%08lx dbl=%ld int=%ld\n", rb, !!(rb & CRTC_DBL_SCAN_EN),
              !!(rb & CRTC_INTERLACE_EN));
        }
    }

#if MACH64_PCI_RETRY
    if (cd->chipFamily == MACH64VT)
        AdjustCrtcFifo_VT(this);
#else
    if (cd->chipFamily == MACH64CT)
        AdjustCrtcFifo_CT(this);
#endif

    if (cd->chipFamily < MACH64GT) {
        ULONG dpChainMask = 0x8080;

        // FIXME: replace with fixed table?
        switch (mi->Depth) {
        case 15:
            dpChainMask = 0x4210;
            break;
        case 16:
            dpChainMask = 0x8410;
            break;
        default:
            // fallthrough
            break;
        }

        waitFifo(1);
        mmio.writeL(DP_CHAIN_MSK, dpChainMask);
    }
}

void ASM Mach64Driver::setPanning(__REGA1(UBYTE *memory), __REGD0(UWORD width), __REGD3(UWORD height),
                                  __REGD1(WORD xoffset), __REGD2(WORD yoffset), __REGD7(RGBFTYPE_REG format))
{
    DRIVER_LOCALS(this);

    DFUNC(INFO,
          "mem 0x%lx, width %ld, height %ld, xoffset %ld, yoffset %ld, "
          "format %ld\n",
          memory, (ULONG)width, (ULONG)height, (LONG)xoffset, (LONG)yoffset, (ULONG)format);

#ifndef NDEBUG
    if (width & 7) {
        DFUNC(ERROR, "Panning pitch not a multiple of 8\n");
        return;
    }
#endif

    LONG panOffset;
    ULONG pitch;
    ULONG memOffset;

    XOffset   = xoffset;
    YOffset   = yoffset;
    memOffset = (ULONG)memory - (ULONG)MemoryBase;

    UBYTE bpp = getBPP(format);
    panOffset = (yoffset * width + xoffset) * bpp;

    pitch     = width / 8;                    // pitch in 8 pixels
    panOffset = (panOffset + memOffset) / 8;  // offset in 64bit words

    D(VERBOSE, "panOffset 0x%lx, pitch %ld qwords\n", panOffset, (ULONG)pitch);
    ULONG offPitch = CRTC_OFFSET(panOffset) | CRTC_PITCH(pitch);
    mmio.writeL(CRTC_OFF_PITCH, offPitch);
}

APTR ASM Mach64Driver::calculateMemory(__REGA1(APTR memory), __REGD0(struct RenderInfo *ri),
                                       __REGD7(RGBFTYPE_REG format))
{
    UBYTE *mem = (UBYTE *)memory;

    DFUNC(VERBOSE, "mem 0x%lx, format %ld\n", mem, (ULONG)format);
    /* CT/VT+: BE alias at +8MB (RRG/PG dual 8MB). GX: LE only. */
    if (chip()->chipFamily >= MACH64CT) {
        switch (format) {
        case RGBFB_A8R8G8B8:
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
            mem += 0x800000;
            D(CHATTY, "redirecting to big endian window 0x%lx\n", mem);
            return mem;
        default:
            break;
        }
    }

#if MACH64_PCI_RETRY
    // RagePro manual says that SGRAM needs to be aligned to 64byte and pitch needs to be 64byte aligned
    return (APTR)(((ULONG)mem + 63) & ~63);
#else
    return mem;  // P96 aligns to 16 byte by default
#endif
}

ULONG ASM Mach64Driver::getCompatibleFormats(__REGD7(RGBFTYPE_REG format))
{
    if (format == RGBFB_NONE)
        return (ULONG)0;

    // These formats can always reside in the Little Endian Window.
    // We never need to change any aperture setting for them
    ULONG compatible = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC;
    if (chip()->chipFamily == MACH64GX)
        compatible |= RGBFF_R8G8B8A8;
    else
        compatible |= RGBFF_B8G8R8A8;

    if (chip()->chipFamily >= MACH64CT) {
        switch (format) {
        case RGBFB_A8R8G8B8:
            // In Big Endian aperture, configured MEM_CNTL for byte swapping in long word
            compatible |= RGBFF_A8R8G8B8;
            break;
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
            // In Big Endian aperture, configured MEM_CNTL for byte swapping in words only
            compatible |= RGBFF_R5G6B5 | RGBFF_R5G5B5;
            break;
        }
    }
    return compatible;
}

BOOL ASM Mach64Driver::setDisplay(__REGD0(BOOL state))
{
    DRIVER_LOCALS(this);

    DFUNC(VERBOSE, " state %ld\n", (ULONG)state);

    mmio.writeMaskL(CRTC_GEN_CNTL, CRTC_DISPLAY_DIS_MASK, state ? 0 : CRTC_DISPLAY_DIS);

    return TRUE;
}

void ASM Mach64Driver::setDPMSLevel(__REGD0(ULONG level))
{
    DFUNC(VERBOSE, "level=%ld\n", level);

    static const ULONG dpmsBits[4] = {
        0,                               /* ON */
        CRTC_HSYNC_DIS,                  /* STANDBY */
        CRTC_VSYNC_DIS,                  /* SUSPEND */
        CRTC_HSYNC_DIS | CRTC_VSYNC_DIS, /* OFF */
    };

    if (level > 3)
        level = 3;

    DRIVER_LOCALS(this);
    mmio.writeMaskL(CRTC_GEN_CNTL, CRTC_HSYNC_DIS | CRTC_VSYNC_DIS, dpmsBits[level]);
}

LONG ASM Mach64Driver::resolvePixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG pixelClock),
                                         __REGD7(RGBFTYPE_REG RGBFormat))
{
    DFUNC(CHATTY, "ModeInfo 0x%lx pixelclock %ld, format %ld\n", mi, pixelClock, (ULONG)RGBFormat);

    const ChipSpecific_t *cs = chipSpecific();

    UWORD targetFreq = pixelClock / 10000;

    if (PixelClockCount[CHUNKY] == 0 || !cs->vclkPllValues || !cs->computeVCLKFrequency) {
        DFUNC(ERROR, "PLL table not initialized\n");
        mi->PixelClock = 0;
        return 0;
    }

    // find pixel clock in pllValues via bisection
    UWORD upper     = PixelClockCount[CHUNKY] - 1;
    UWORD upperFreq = cs->computeVCLKFrequency(this, &cs->vclkPllValues[upper]);
    UWORD lower     = 0;
    UWORD lowerFreq = cs->computeVCLKFrequency(this, &cs->vclkPllValues[lower]);

    while (lower + 1 < upper) {
        UWORD middle     = (upper + lower) / 2;
        UWORD middleFreq = cs->computeVCLKFrequency(this, &cs->vclkPllValues[middle]);
        if (middleFreq < targetFreq) {
            lower     = middle;
            lowerFreq = middleFreq;
        } else {
            upper     = middle;
            upperFreq = middleFreq;
        }
    }
    // Return the closest of upper/lower (signed; avoids UWORD underflow when target < lower)
    {
        LONG dLower = (LONG)targetFreq - (LONG)lowerFreq;
        LONG dUpper = (LONG)upperFreq - (LONG)targetFreq;
        if (dLower < 0) {
            dLower = -dLower;
        }
        if (dUpper < 0) {
            dUpper = -dUpper;
        }
        if (dLower > dUpper) {
            lower     = upper;
            lowerFreq = upperFreq;
        }
    }

    mi->PixelClock = lowerFreq * 10000;

    D(CHATTY, "Resulting pixelclock Hz: %ld\n\n", mi->PixelClock);

    mi->pll1.Numerator   = cs->vclkPllValues[lower].N;
    mi->pll2.Denominator = cs->vclkPllValues[lower].Pidx;

    return lower;
}

ULONG ASM Mach64Driver::getPixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG index), __REGD7(RGBFTYPE_REG format))
{
    DFUNC(VERBOSE, "\n");
    (void)mi;
    (void)format;

    const ChipSpecific_t *cs = chipSpecific();

    UWORD freq = cs->computeVCLKFrequency(this, &cs->vclkPllValues[index]);

    return freq * 10000;
}

// FIXME: split out into family-specific functions
INLINE void Mach64Driver::setMemoryModeInternal(RGBFTYPE format)
{
    DRIVER_LOCALS(this);
    DFUNC(VERBOSE, "format %ld\n", (ULONG)format);

    if (cd->chipFamily < MACH64GT) {
#define MEM_PIX_WIDTH(x)   ((x) << 24)
#define MEM_PIX_WIDTH_MASK (0x7 << 24)
        // These are the formats we place in the big endian aperture.
        // And only for those we have to do anything.
        switch (format) {
        case RGBFB_A8R8G8B8:
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
            if (cd->MemFormat == format) {
                return;
            }
            cd->MemFormat = format;
            mmio.writeMaskL(MEM_CNTL, MEM_PIX_WIDTH_MASK, MEM_PIX_WIDTH(g_bitWidths[format]));
            break;
        }
    } else {
#define UPPER_APER_ENDIAN(x)   ((x) << 26)
#define UPPER_APER_ENDIAN_MASK (0x3 << 26)
        ULONG byteSwap = 0x0;
        // These are the formats we place in the big endian aperture.
        // And only for those we have to do anything.
        switch (format) {
        case RGBFB_A8R8G8B8:
            byteSwap = UPPER_APER_ENDIAN(0b10);
            break;
        case RGBFB_R5G6B5:
        case RGBFB_R5G5B5:
            byteSwap = UPPER_APER_ENDIAN(0b01);
            break;
        default:;  // fallthrough
        }

        if (cd->MemFormat == format) {
            return;
        }
        cd->MemFormat = format;

        mmio.writeMaskL(MEM_CNTL, UPPER_APER_ENDIAN_MASK, byteSwap);
    }
}

void ASM Mach64Driver::setMemoryMode(__REGD7(RGBFTYPE_REG format))
{
    __asm __volatile("\t movem.l d0-d1/a0-a1,-(sp)\n"
                     : /* no result */
                     :
                     :);

    setMemoryModeInternal(AS_RGBF(format));

    __asm __volatile("\t movem.l (sp)+,d0-d1/a0-a1\n"
                     : /* no result */
                     :
                     : "d0", "d1", "a0", "a1");
}

BOOL ASM Mach64Driver::getVSyncState(__REGD0(BOOL expected))
{
    DRIVER_LOCALS(this);
    DFUNC(VERBOSE, "\n");
    (void)expected;
    return mmio.testL(CRTC_INT_CNTL, CRTC_VBLANK);
}

ULONG ASM Mach64Driver::getVBeamPos()
{
    DRIVER_LOCALS(this);
    return (mmio.readL(CRTC_VLINE_CRNT_VLINE) & CRTC_CRNT_VLINE_MASK) >> 16;
}

/* write only enable bits (+ W1C acks), never status bits back. */
void Mach64Driver::syncCrtcInterruptEnables()
{
    if (!(Flags & BIF_VBLANKINTERRUPT))
        return;

    DRIVER_LOCALS(this);
    ULONG en = cd->p96VBlankInt ? CRTC_VBLANK_INT_EN : 0;

    mmio.writeL(CRTC_INT_CNTL, en | CRTC_INT_ACKS);
    mmio.writeL(CRTC_INT_CNTL, en);
}

BOOL ASM Mach64Driver::setInterrupt(__REGD0(BOOL state))
{
    struct ExecBase *SysBase = ExecBase;
    Disable();

    chip()->p96VBlankInt = state ? 1 : 0;
    syncCrtcInterruptEnables();

    Enable();

    return TRUE;
}

/* OpenPCI/Exec interrupt server: is_Data (BoardInfo *) in a1.
 * Return non-zero if we handled this board's IRQ; else 0. Entry sets CCR.Z.
 * Non-static: DEFINE_INTSERVER asm must jsr the C symbol. */
ULONG Mach64Driver::interruptServer()
{
    DRIVER_LOCALS(this);

    ULONG status = mmio.readL(CRTC_INT_CNTL);

    if (!(status & CRTC_VBLANK_INT))
        return 0;

    mmio.writeL(CRTC_INT_CNTL, (status & CRTC_INT_EN_MASK) | CRTC_VBLANK_INT_AK);

    if (cd->p96VBlankInt) {
        struct ExecBase *SysBase = ExecBase;
        Cause(&SoftInterrupt);
    }

    // /* If W1C did not clear, drop enables so a stuck INTA cannot soft-lock. */
    // if (mmio.readL(CRTC_INT_CNTL) & CRTC_VBLANK_INT)
    //     mmio.writeL(CRTC_INT_CNTL, CRTC_INT_ACKS);

    return 1;
}

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

void ASM Mach64Driver::setSpritePosition(__REGD0(WORD xpos), __REGD1(WORD ypos), __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(VERBOSE, "\n");
    DRIVER_LOCALS(this);
    (void)fmt;

    MouseX = xpos;
    MouseY = ypos;

    WORD spriteX = xpos - XOffset;
    WORD spriteY = ypos - YOffset + YSplit;

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
    if (ModeInfo->Flags & GMF_DOUBLESCAN) {
        spriteY *= 2;
    }

    D(CHATTY, "SpritePos X: %ld 0x%lx, Y: %ld 0x%lx\n", (LONG)spriteX, (ULONG)spriteX, (LONG)spriteY, (ULONG)spriteY);

    ULONG memOffset = (ULONG)MouseImageBuffer - (ULONG)MemoryBase;
    mmio.writeL(CUR_OFFSET, memOffset / 8);

    mmio.writeL(CUR_HORZ_VERT_POSN, CUR_HORZ_POSN(spriteX) | CUR_VERT_POSN(spriteY));
    mmio.writeL(CUR_HORZ_VERT_OFF, CUR_HORZ_OFF(offsetX) | CUR_VERT_OFF(offsetY));
}

void ASM Mach64Driver::setSpriteImage(__REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(VERBOSE, "\n");
    (void)fmt;
    packAtiHwCursorImage(this);
}

#define CUR_CLR_8(x)   (x)
#define CUR_CLR_8_MASK (0xFF)
#define CUR_CLR_B(x)   ((x) << 8)
#define CUR_CLR_B_MASK (0xFF << 8)
#define CUR_CLR_G(x)   ((x) << 16)
#define CUR_CLR_G_MASK (0xFF << 16)
#define CUR_CLR_R(x)   ((x) << 24)
#define CUR_CLR_R_MASK (0xFF << 24)

void ASM Mach64Driver::setSpriteColor(__REGD0(UBYTE index), __REGD1(UBYTE red), __REGD2(UBYTE green),
                                      __REGD3(UBYTE blue), __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(VERBOSE, "Index %ld, Red %ld, Green %ld, Blue %ld\n", (ULONG)index, (ULONG)red, (ULONG)green, (ULONG)blue);
    (void)fmt;
    DRIVER_LOCALS(this);
    switch (index) {
    case 0:
        mmio.writeL(CUR_CLR0, CUR_CLR_R(red) | CUR_CLR_G(green) | CUR_CLR_B(blue) | CUR_CLR_8(17));
        break;
    case 2:
        mmio.writeL(CUR_CLR1, CUR_CLR_R(red) | CUR_CLR_G(green) | CUR_CLR_B(blue) | CUR_CLR_8(19));
        break;
    default:
        break;
    }
}

BOOL ASM Mach64Driver::setSprite(__REGD0(BOOL activate), __REGD7(RGBFTYPE_REG RGBFormat))
{
    DFUNC(VERBOSE, "\n");
    DRIVER_LOCALS(this);

    mmio.writeMaskL(GEN_TEST_CNTL, GEN_CUR_ENABLE_MASK, (activate ? GEN_CUR_ENABLE : 0));

    if (activate) {
        setSpriteColor(0, CLUT[17].Red, CLUT[17].Green, CLUT[17].Blue, RGBFormat);
        setSpriteColor(1, CLUT[18].Red, CLUT[18].Green, CLUT[18].Blue, RGBFormat);
        setSpriteColor(2, CLUT[19].Red, CLUT[19].Green, CLUT[19].Blue, RGBFormat);
    }

    return TRUE;
}

void Mach64Driver::waitIdle()
{
    DRIVER_LOCALS(this);

    waitFifo(16);

    ULONG cnt = 0;

    while (mmio.testL(GUI_STAT, 1)) {
#ifdef DBG
        if (cnt++ > 100) {
            ULONG busCntl = mmio.readL(BUS_CNTL);
            if (busCntl & (BUS_FIFO_ERR_INT | BUS_HOST_ERR_INT)) {
                resetEngine();
            }
            break;
        }
#endif
    }
}

void Mach64Driver::setWriteMask(UBYTE mask, ULONG fmt, BYTE waitFifoSlots)
{
    DRIVER_LOCALS(this);

    if (fmt != RGBFB_CLUT && cd->GEmask != 0xFF) {
        // 16/32 bit modes ignore the mask
        cd->GEmask = 0xFF;
        waitFifo(waitFifoSlots + 1);
        mmio.writeL(DP_WRITE_MSK, 0xFFFFFFFF);
    } else {
        // 8bit modes use the mask
        if (cd->GEmask != mask) {
            cd->GEmask = mask;

            waitFifo(waitFifoSlots + 1);

            UWORD wordMask = (mask << 8) | mask;

            mmio.writeL(DP_WRITE_MSK, copyToUpper(wordMask));
        } else {
            waitFifo(waitFifoSlots);
        }
    }
}

LONG Mach64Driver::memoryOffset(APTR memory) const
{
    return (LONG)((ULONG)memory - (ULONG)MemoryBase);
}

BOOL Mach64Driver::isVideoMemory(APTR memory) const
{
    LONG offset = memoryOffset(memory);
    return offset > 0 && offset < MemorySize;
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

BOOL Mach64Driver::setDstBuffer(const struct RenderInfo *ri, ULONG format)
{
    DRIVER_LOCALS(this);

    if (memcmp(ri, &cd->dstBuffer, sizeof(struct RenderInfo)) == 0) {
        return TRUE;
    }
    cd->dstBuffer = *ri;
    BYTE bppLog2  = getBPPLog2(format);

    waitFifo(2);

    // Offset is in units of '64 bit words' (8 bytes), while pitch is in units of '8 Pixels'
    // So convert BytesPerRow to "number of groups of 8 pixels"
    // FIXME: For SGRAM configuration, DST_OFFSET must be aligned on a 64 byte boundary!!!
    // For SGRAM configuration, DST_PITCH must be a multiple of 64 bytes.
    mmio.writeL(DST_OFF_PITCH, DST_OFFSET(memoryOffset(ri->Memory) / 8) | DST_PITCH(ri->BytesPerRow >> (bppLog2 + 3)));

    UBYTE dstPixWidth = COLOR_DEPTH_8;
    if (format != RGBFB_CLUT && format != RGBFB_B8G8R8 && format != RGBFB_R8G8B8) {
        dstPixWidth = g_bitWidths[format];
    }

    // FIXME: reading from the register is not FIFO'd. In theory to use writeMaskL we would need to wait for
    // engine idle to be sure
    //  that the previous write has completed. For now we just wait for 2 slots and write the register directly.
    mmio.writeMaskL(DP_PIX_WIDTH, DP_DST_PIX_WIDTH_MASK | DP_HOST_PIX_WIDTH_MASK,
                    DP_DST_PIX_WIDTH(dstPixWidth) | DP_HOST_PIX_WIDTH(COLOR_DEPTH_1));

    return TRUE;
}

#define SRC_OFFSET(x)   (x)
#define SRC_OFFSET_MASK (0xFFFFF)
#define SRC_PITCH(x)    ((x) << 22)
#define SRC_PITCH_MASK  (0x3FF << 22)

BOOL Mach64Driver::setSrcBuffer(const struct RenderInfo *ri, ULONG format)
{
    DRIVER_LOCALS(this);

    // if (memcmp(ri, &cd->srcBuffer, sizeof(struct RenderInfo)) == 0) {
    //     return TRUE;
    // }
    // cd->dstBuffer = *ri;

    waitFifo(2);

    UBYTE bppLog2 = getBPPLog2(format);

    // Offset is in unite of '64 bit words' (8 bytes), while pitch is in units of '8 Pixels'
    // So convert BytesPerRow for
    mmio.writeL(SRC_OFF_PITCH,
                SRC_OFFSET(memoryOffset(ri->Memory) / 8) | SRC_PITCH((ri->BytesPerRow >> (bppLog2 + 3))));

    UBYTE srcPixWidth = COLOR_DEPTH_8;
    if (format != RGBFB_CLUT && format != RGBFB_B8G8R8 && format != RGBFB_R8G8B8) {
        srcPixWidth = g_bitWidths[format];
    }

    mmio.writeMaskL(DP_PIX_WIDTH, DP_SRC_PIX_WIDTH_MASK, DP_SRC_PIX_WIDTH(srcPixWidth));

    return TRUE;
}

static INLINE ULONG REGARGS penToColor(ULONG pen, ULONG fmt)
{
    switch ((RGBFTYPE)fmt) {
    case RGBFB_R8G8B8A8:
        /* GX 68860 GMR E3 FB is RGBA; without swapl, GE writes ABGR via LE MMIO. */
        pen = swapl(pen);
        break;
    case RGBFB_B8G8R8A8:
        pen = swapl(pen);
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
        pen = swapw(pen);
        // Fallthrough
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        pen |= pen << 16;
        break;
    case RGBFB_CLUT:
        pen = (pen & 0xFF) | ((pen & 0xFF) << 8) | ((pen & 0xFF) << 16) | ((pen & 0xFF) << 24);
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
#define DST_X_MASK (0x1FFF << 16)
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

void Mach64Driver::drawRect(WORD x, WORD y, WORD width, WORD height)
{
    Mach64MmioNoSwapQ mmio(mmioBase());

    // micro-optimization to save on some redundant rol/swap/rol sequences
    mmio.writeL(DST_Y_X, makeDWORD(swapw(y), swapw(x)));
    mmio.writeL(DST_HEIGHT_WIDTH, makeDWORD(swapw(height), swapw(width)));
    flushWrites();
}

void ASM Mach64Driver::fillRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                                __REGD3(WORD height), __REGD4(ULONG pen), __REGD5(UBYTE mask),
                                __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\npen %08lx, mask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)pen, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    setDstBuffer(ri, fmt);

    DRIVER_LOCALS(this);

    if (cd->GEOp != FILLRECT) {
        cd->GEOp = FILLRECT;

        waitFifo(3);

        mmio.writeL(DP_SRC,
                    DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) | DP_FRGD_SRC(CLR_SRC_FRGD_COLOR) | DP_MONO_SRC(MONO_SRC_ONE));
        mmio.writeL(DP_MIX, DP_BKGD_MIX(MIX_CURRENT) | DP_FRGD_MIX(MIX_NEW));
        mmio.writeL(GUI_TRAJ_CNTL, DST_X_DIR | DST_Y_DIR);
    }

    if (cd->GEfgPen != pen) {
        cd->GEfgPen    = pen;
        cd->GEdrawMode = 0xFF;  // invalidate drawmode cache

        pen = penToColor(pen, fmt);

        waitFifo(1);

        mmio.writeL(DP_FRGD_CLR, pen);
    }

    setWriteMask(mask, fmt, 2);

    drawRect(x, y, width, height);
}

void ASM Mach64Driver::invertRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                                  __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    setDstBuffer(ri, fmt);

    DRIVER_LOCALS(this);

    if (cd->GEOp != INVERTRECT) {
        cd->GEOp       = INVERTRECT;
        cd->GEdrawMode = 0xFF;  // invalidate minterm cache

        waitFifo(3);

        mmio.writeL(DP_SRC,
                    DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) | DP_FRGD_SRC(CLR_SRC_FRGD_COLOR) | DP_MONO_SRC(MONO_SRC_ONE));
        mmio.writeL(DP_MIX, DP_BKGD_MIX(MIX_ZERO) | DP_FRGD_MIX(MIX_NOT_CURRENT));
        mmio.writeL(GUI_TRAJ_CNTL, DST_X_DIR | DST_Y_DIR | DST_LAST_PEL);
    }

    setWriteMask(mask, fmt, 2);

    drawRect(x, y, width, height);
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
#define SRC_X_MASK (0x1FFF << 16)
#define SRC_Y(y)   (y)
#define SRC_Y_MASK (0x7FFF)

#define SRC_WIDTH1(x)    ((x) << 16)
#define SRC_WIDTH1_MASK  (0x1FFF << 16)
#define SRC_HEIGHT1(y)   (y)
#define SRC_HEIGHT1_MASK (0x7FFF)

#define SRC_WIDTH2(x)    ((x) << 16)
#define SRC_WIDTH2_MASK  (0x1FFF << 16)
#define SRC_HEIGHT2(y)   (y)
#define SRC_HEIGHT2_MASK (0x7FFF)

void ASM Mach64Driver::blitRectNoMaskComplete(__REGA1(struct RenderInfo *sri), __REGA2(struct RenderInfo *dri),
                                              __REGD0(WORD srcX), __REGD1(WORD srcY), __REGD2(WORD dstX),
                                              __REGD3(WORD dstY), __REGD4(WORD width), __REGD5(WORD height),
                                              __REGD6(UBYTE opCode), __REGD7(RGBFTYPE_REG format))
{
    DFUNC(VERBOSE,
          "\nx1 %ld, y1 %ld, x2 %ld, y2 %ld, w %ld, \n"
          "h %ld\nminTerm 0x%lx fmt %ld\n"
          "sri->bytesPerRow %ld, sri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)opCode, (ULONG)format,
          (ULONG)sri->BytesPerRow, (ULONG)sri->Memory);

    setDstBuffer(dri, format);
    setSrcBuffer(sri, format);

    DRIVER_LOCALS(this);

    if (cd->GEOp != BLITRECTNOMASKCOMPLETE) {
        cd->GEOp       = BLITRECTNOMASKCOMPLETE;
        cd->GEmask     = 0xFF;
        cd->GEdrawMode = 0xFF;  // invalidate minterm cache

        waitFifo(2);

        mmio.writeL(DP_WRITE_MSK, 0xFFFFFFFF);
        mmio.writeL(DP_SRC,
                    DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) | DP_FRGD_SRC(CLR_SRC_BLIT_SRC) | DP_MONO_SRC(MONO_SRC_ONE));
    }

    if (cd->GEdrawMode != opCode) {
        cd->GEdrawMode = opCode;

        waitFifo(1);
        mmio.writeL(DP_MIX, DP_BKGD_MIX(MIX_CURRENT) | DP_FRGD_MIX(minTermToMix[opCode]));
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

    waitFifo(5);
    mmio.writeL(GUI_TRAJ_CNTL, dir);

    mmio.writeL(SRC_Y_X, SRC_Y(srcY) | SRC_X(srcX));
    mmio.writeL(SRC_HEIGHT1_WIDTH1, SRC_HEIGHT1(height) | SRC_WIDTH1(width));

    drawRect(dstX, dstY, width, height);
}

void ASM Mach64Driver::blitRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD srcX), __REGD1(WORD srcY),
                                __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width), __REGD5(WORD height),
                                __REGD6(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(VERBOSE,
          "\nx1 %ld, y1 %ld, x2 %ld, y2 %ld, w %ld, \n"
          "h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt,
          (ULONG)ri->BytesPerRow, (ULONG)ri->Memory);

    // FIXME: optimize into one function
    setDstBuffer(ri, fmt);
    setSrcBuffer(ri, fmt);

    DRIVER_LOCALS(this);

    if (cd->GEOp != BLITRECT) {
        cd->GEOp       = BLITRECT;
        cd->GEdrawMode = 0xFF;  // invalidate minterm cache

        waitFifo(2);

        mmio.writeL(DP_SRC,
                    DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) | DP_FRGD_SRC(CLR_SRC_BLIT_SRC) | DP_MONO_SRC(MONO_SRC_ONE));
        mmio.writeL(DP_MIX, DP_BKGD_MIX(MIX_CURRENT) | DP_FRGD_MIX(MIX_NEW));
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
    setWriteMask(mask, fmt, 5);

    mmio.writeL(GUI_TRAJ_CNTL, dir);

    mmio.writeL(SRC_Y_X, SRC_Y(srcY) | SRC_X(srcX));
    mmio.writeL(SRC_HEIGHT1_WIDTH1, SRC_HEIGHT1(height) | SRC_WIDTH1(width));

    drawRect(dstX, dstY, width, height);
}

void Mach64Driver::setDrawMode(ULONG FgPen, ULONG BgPen, UBYTE DrawMode, ULONG format, BYTE monoSource)
{
    ChipData_t *cd = chip();

    if (cd->GEfgPen != FgPen || cd->GEbgPen != BgPen || cd->GEdrawMode != DrawMode) {
        cd->GEfgPen    = FgPen;
        cd->GEbgPen    = BgPen;
        cd->GEdrawMode = DrawMode;

        ULONG fgPen = penToColor(FgPen, format);
        ULONG bgPen = penToColor(BgPen, format);

        UWORD writeMode = (DrawMode & COMPLEMENT) ? MIX_NOT_CURRENT : MIX_NEW;
        UWORD fMix, bMix;
        switch (DrawMode & 1) {
        case JAM1:
            fMix = writeMode;
            bMix = MIX_CURRENT;
            break;
        case JAM2:
            fMix = writeMode;
            bMix = writeMode;
            break;
        }

        UWORD fSrc = CLR_SRC_FRGD_COLOR;
        UWORD bSrc = CLR_SRC_BKGD_COLOR;

        if (DrawMode & INVERSVID) {
            UWORD t = fMix;
            fMix    = bMix;
            bMix    = t;
            t       = fSrc;
            fSrc    = bSrc;
            bSrc    = t;
        }

        waitFifo(4);

        DRIVER_LOCALS(this);

        mmio.writeL(DP_FRGD_CLR, fgPen);
        mmio.writeL(DP_BKGD_CLR, bgPen);

        mmio.writeL(DP_MIX, DP_BKGD_MIX(bMix) | DP_FRGD_MIX(fMix));
        mmio.writeL(DP_SRC, DP_FRGD_SRC(fSrc) | DP_BKGD_SRC(bSrc) | DP_MONO_SRC(monoSource));
    }
}

void ASM Mach64Driver::blitTemplate(__REGA1(struct RenderInfo *ri), __REGA2(struct Template *tmpl), __REGD0(WORD x),
                                    __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                                    __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    if (isVideoMemory(tmpl->Memory)) {
        D(ERROR, "Template is in video memory\n");
    }

    setDstBuffer(ri, fmt);

    DRIVER_LOCALS(this);
    Mach64MmioNoSwapQ raw(mmioBase());

    if (cd->GEOp != BLITTEMPLATE) {
        cd->GEOp       = BLITTEMPLATE;
        cd->GEdrawMode = 0xFF;
        waitFifo(1);
        mmio.writeL(GUI_TRAJ_CNTL, SRC_LINEAR_EN | DST_X_DIR | DST_Y_DIR);

        //        mmio.writeMaskL(DP_PIX_WIDTH, DP_HOST_PIX_WIDTH_MASK, DP_HOST_PIX_WIDTH(COLOR_DEPTH_1));
    }

    setDrawMode(tmpl->FgPen, tmpl->BgPen, tmpl->DrawMode, fmt, MONO_SRC_HOST_DATA);
    setWriteMask(mask, fmt, 0);

    // 0 <= XOffset <= 15
    UWORD blitWidth     = (width + tmpl->XOffset + 31) & ~31;
    UWORD dwordsPerLine = blitWidth / 32;

    UWORD numFifoSlots = dwordsPerLine * height + 3;
    if (numFifoSlots > 16) {
        numFifoSlots = 16;
    }
    waitFifo(numFifoSlots);

    // Since we feed the monochrome expansion in units of 32bit (i.e. 32 pixels width),
    // we need to align the width to the next 32bit boundary. To make that padding not get rendered, use
    // the right scissor. And since we're setting the scissor anyways, we might as well set the left side
    // and spare ourselves the CPU work to "left-rotate" the template bits.
    mmio.writeL(SC_LEFT_RIGHT, SC_RIGHT(width + x - 1) | SC_LEFT(x));

    drawRect(x - tmpl->XOffset, y, blitWidth, height);

    // We already used up 3 fifo slots for the setup above
    UWORD hostDataReg = 3;

    const UBYTE *bitmap = (const UBYTE *)tmpl->Memory;
    WORD bitmapPitch    = tmpl->BytesPerRow;

    for (UWORD y = 0; y < height; ++y) {
        for (UWORD x = 0; x < dwordsPerLine; ++x) {
            D(VERBOSE, "writing to HOST_DATA%u: 0x%08lx\n", hostDataReg, ((const ULONG *)bitmap)[x]);

            raw.writeL(static_cast<MmioReg::Id>(static_cast<LONG>(HOST_DATA0) + hostDataReg),
                       ((const ULONG *)bitmap)[x]);

            hostDataReg = (hostDataReg + 1) & 15;
            if (!hostDataReg) {
                waitFifo(16);
            }
        }
        bitmap += bitmapPitch;
    }

    waitFifo(1);
    // reset right scissor
    mmio.writeL(SC_LEFT_RIGHT, ((SC_RIGHT_MASK >> 1) & SC_RIGHT_MASK) | SC_LEFT(0));
}

void Mach64Driver::performBlitPlanar2ChunkyBlits(SHORT dstX, SHORT dstY, SHORT width, SHORT height, UBYTE *bitmap,
                                                 UWORD dwordsPerLine, WORD bmPitch, UBYTE rol)
{
    DRIVER_LOCALS(this);
    Mach64MmioNoSwapQ raw(mmioBase());

    if ((ULONG)bitmap == 0x00000000) {
        waitFifo(3);
        mmio.writeL(DP_SRC, DP_FRGD_SRC(CLR_SRC_BKGD_COLOR) | DP_BKGD_SRC(CLR_SRC_FRGD_COLOR) |
                                DP_MONO_SRC(MONO_SRC_ONE));  // Background color, 0x0
        drawRect(dstX, dstY, width, height);
    } else if ((ULONG)bitmap == 0xFFFFFFFF) {
        waitFifo(3);
        mmio.writeL(DP_SRC, DP_FRGD_SRC(CLR_SRC_FRGD_COLOR) | DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) |
                                DP_MONO_SRC(MONO_SRC_ONE));  // Forground color, 0xFF
        drawRect(dstX, dstY, width, height);
    } else {
        UWORD numFifoSlots = dwordsPerLine * height + 3;
        if (numFifoSlots > 16) {
            numFifoSlots = 16;
        }
        waitFifo(numFifoSlots);

        UWORD hostDataReg = 3;

        // planar bitmap selects between 0x00 and 0xFF
        mmio.writeL(DP_SRC, DP_FRGD_SRC(CLR_SRC_FRGD_COLOR) | DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) |
                                DP_MONO_SRC(MONO_SRC_HOST_DATA));
        drawRect(dstX, dstY, width, height);

        if (!rol) {
            for (UWORD y = 0; y < height; ++y) {
                for (UWORD x = 0; x < dwordsPerLine; ++x) {
                    raw.writeL(static_cast<MmioReg::Id>(static_cast<LONG>(HOST_DATA0) + hostDataReg),
                               ((ULONG *)bitmap)[x]);

                    hostDataReg = (hostDataReg + 1) & 15;
                    if (!hostDataReg) {
                        waitFifo(16);
                    }
                }
                bitmap += bmPitch;
            }
        } else {
            for (UWORD y = 0; y < height; ++y) {
                for (UWORD x = 0; x < dwordsPerLine; ++x) {
                    ULONG left  = ((ULONG *)bitmap)[x] << rol;
                    ULONG right = ((ULONG *)bitmap)[x + 1] >> (32 - rol);

                    raw.writeL(static_cast<MmioReg::Id>(static_cast<LONG>(HOST_DATA0) + hostDataReg), (left | right));

                    hostDataReg = (hostDataReg + 1) & 15;
                    if (!hostDataReg) {
                        waitFifo(16);
                    }
                }
                bitmap += bmPitch;
            }
        }
    }
}

void ASM Mach64Driver::blitPlanar2Chunky(__REGA1(struct BitMap *bm), __REGA2(struct RenderInfo *ri),
                                         __REGD0(SHORT srcX), __REGD1(SHORT srcY), __REGD2(SHORT dstX),
                                         __REGD3(SHORT dstY), __REGD4(SHORT width), __REGD5(SHORT height),
                                         __REGD6(UBYTE minTerm), __REGD7(UBYTE mask))
{
    DFUNC(VERBOSE,
          "\nsrcX %ld, srcY %ld, dstX %ld, dstY %ld, w %ld, h %ld"
          "\nmask 0x%lx minTerm %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)minTerm,
          (ULONG)ri->BytesPerRow, (ULONG)ri->Memory);

    DRIVER_LOCALS(this);

    WORD bytesPerRow = ri->BytesPerRow;
    // how many dwords per line in the source plane
    UWORD numPlanarBytes = width / 8 * height * bm->Depth;
    (void)bytesPerRow;
    (void)numPlanarBytes;

    setDstBuffer(ri, RGBFB_CLUT);

    if (cd->GEOp != BLITPLANAR2CHUNKY) {
        cd->GEOp = BLITPLANAR2CHUNKY;
        // Invalidate the pen and drawmode caches
        cd->GEdrawMode = 0xFF;
        cd->GEmask     = mask;
        cd->GEfgPen    = 0xFFFFFFFF;
        cd->GEbgPen    = 0x0;

        waitFifo(3);
        mmio.writeL(GUI_TRAJ_CNTL, SRC_LINEAR_EN | DST_X_DIR | DST_Y_DIR);
        mmio.writeL(DP_FRGD_CLR, 0xFFFFFFFF);
        mmio.writeL(DP_BKGD_CLR, 0x0);
    }

    if (cd->GEdrawMode != minTerm) {
        cd->GEdrawMode = minTerm;

        waitFifo(1);
        // Set the mix mode (minterm)
        UWORD mixMode = minTermToMix[minTerm];
        mmio.writeL(DP_MIX, DP_BKGD_MIX(mixMode) | DP_FRGD_MIX(mixMode));
    }

    waitFifo(1);
    // clip potential 32bit padding
    mmio.writeL(SC_LEFT_RIGHT, SC_RIGHT(dstX + width - 1) | SC_LEFT(dstX));
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

        setWriteMask(writeMask, RGBFB_CLUT, 0);

        UBYTE *bitmap = (UBYTE *)bm->Planes[p];
        if (bitmap != 0x0 && (ULONG)bitmap != 0xffffffff) {
            bitmap += bmStartOffset;
        }

        performBlitPlanar2ChunkyBlits(dstX, dstY, width, height, bitmap, dwordsPerLine, bmPitch, rol);
    }

    waitFifo(1);
    // reset right scissor
    mmio.writeL(SC_LEFT_RIGHT, ((SC_RIGHT_MASK >> 1) & SC_RIGHT_MASK) | SC_LEFT(0));
}

void ASM Mach64Driver::blitPattern(__REGA1(struct RenderInfo *ri), __REGA2(struct Pattern *pattern), __REGD0(WORD x),
                                   __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                                   __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    if (pattern->Size > 8) {
        BlitPatternDefault(this, ri, pattern, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    if (isVideoMemory(pattern->Memory)) {
        D(ERROR, "Pattern is in video memory\n");
    }

    Mach64MmioNoSwapQ raw(mmioBase());
    DRIVER_LOCALS(this);

    if (cd->GEOp != BLITPATTERN) {
        cd->GEOp            = BLITPATTERN;
        cd->GEdrawMode      = 0xFF;
        cd->patternCacheKey = 0xFFFFFFFF;

        waitFifo(2);

        // Offset is in units of '64 bit words' (8 bytes), while pitch is in units of '8 Pixels'.
        mmio.writeL(SRC_OFF_PITCH, SRC_OFFSET(memoryOffset(cd->patternVideoBuffer) / 8) | SRC_PITCH(8));
        mmio.writeMaskL(DP_PIX_WIDTH, DP_SRC_PIX_WIDTH_MASK, DP_SRC_PIX_WIDTH(COLOR_DEPTH_1));
    }

    setDstBuffer(ri, fmt);
    setWriteMask(mask, fmt, 0);

    // First, figure out if the new pattern would actually fit into an 8x8 mono pattern.
    // Then we can use the hardware pattern registers, which are much faster.
    // If not, upload the pattern to video memory and use that as mono blit source.
    // We cache the last pattern to avoid re-uploading it if it didn't change.
    UWORD patternHeight        = 1 << pattern->Size;
    const UWORD *sysMemPattern = (const UWORD *)pattern->Memory;
    UWORD *cachedPattern       = cd->patternCacheBuffer;
    ULONG *videoMemPattern     = cd->patternVideoBuffer;

    // Try to avoid wait-for-idle by first checking if the pattern changed.
    // I'm not expecting huge patterns, so this will hopefully be fast
    BOOL patternChanged = FALSE;
    BOOL is8x8          = (patternHeight <= 8);
    for (UWORD i = 0; i < patternHeight; ++i) {
        UWORD row = sysMemPattern[i];
        // Compare new pattern with last one uploaded
        if (row != cachedPattern[i]) {
            cachedPattern[i] = row;
            patternChanged   = TRUE;
        }
        // Check if upper half and lower half of the 16bit pattern row are identical,
        // so the pattern width is essentially 8bit
        if ((UBYTE)(row >> 8) != (UBYTE)row) {
            is8x8 = FALSE;
        }
    }

    BOOL was8x8 = (cd->patternCacheKey & 0x80000000) != 0;
    if (is8x8 != was8x8) {
        patternChanged = TRUE;
    }

    if (is8x8) {
        // The Rage 8x8 mono patttern cannot be offset directly.
        // Instead, its "destination aligned". So in order to offset the pattern, we
        // need to manually rotate it here.
        UBYTE pattOffX = (UBYTE)((x - pattern->XOffset) & 7);
        UBYTE pattOffY = (UBYTE)((y - pattern->YOffset) & 7);

        ULONG pattCacheKey = (pattOffX << 16) | (pattOffY << 8) | pattern->Size | 0x80000000;
        if (pattCacheKey != cd->patternCacheKey) {
            cd->patternCacheKey = pattCacheKey;
            patternChanged      = TRUE;
        }

        if (patternChanged) {
            // replicate the 8xN pattern to 8x8
            ULONG pat0;
            ULONG pat1;

            // Build the 8x8 pattern in the two registers
            // Source patterns that are smaller than 8 in height will be extended to height 8
            switch (pattern->Size) {
            case 0:
                // our pattern data is already 16bit, with the upper half and lower half determined to be identical
                pat0 = cachedPattern[0] | (cachedPattern[0] << 16);
                pat1 = pat0;
                break;
            case 1:
                pat0 = (cachedPattern[0] & 0xFF00) | ((cachedPattern[1] & 0xFF));
                pat0 |= (pat0 << 16);
                pat1 = pat0;
                break;
            case 2:
                pat0 = ((cachedPattern[0] & 0xFF00) << 16) | ((cachedPattern[1] & 0xFF00) << 8) |
                       (cachedPattern[2] & 0xFF00) | (cachedPattern[3] & 0xFF);
                pat1 = pat0;
                break;
            case 3:
                pat0 = ((cachedPattern[0] & 0xFF00) << 16) | ((cachedPattern[1] & 0xFF00) << 8) |
                       (cachedPattern[2] & 0xFF00) | (cachedPattern[3] & 0xFF);
                pat1 = ((cachedPattern[4] & 0xFF00) << 16) | ((cachedPattern[5] & 0xFF00) << 8) |
                       (cachedPattern[6] & 0xFF00) | (cachedPattern[7] & 0xFF);
                break;
            default:
                // fallthrough
                break;
            }

            // Since the Mach64 pattern is "destination aligned", emulate offsetting the pattern by uploading
            // a rotated pattern
            if (pattOffX) {
                // Rotate 'right' in X direction, we need to rotate within each byte
                ULONG maskLower = (1 << pattOffX) - 1;
                maskLower |= (maskLower << 8) | (maskLower << 16) | (maskLower << 24);
                ULONG maskUpper = ~maskLower;
                pat0            = ((pat0 & maskUpper) >> pattOffX) | ((pat0 & maskLower) << (8 - pattOffX));
                pat1            = ((pat1 & maskUpper) >> pattOffX) | ((pat1 & maskLower) << (8 - pattOffX));
            }

            if (pattOffY) {
                // Rotate 'down' in Y direction
                ULONG temp;
                if (pattOffY & 1) {
                    temp = pat0;
                    pat0 = (pat0 >> 8) | (pat1 << 24);
                    pat1 = (pat1 >> 8) | (temp << 24);
                }
                if (pattOffY & 2) {
                    temp = pat0;
                    pat0 = (pat0 >> 16) | (pat1 << 16);
                    pat1 = (pat1 >> 16) | (temp << 16);
                }
                if (pattOffY & 4) {
                    temp = pat0;
                    pat0 = pat1;
                    pat1 = temp;
                }
            }

            waitFifo(3);

            raw.writeL(PAT_REG0, pat0);
            raw.writeL(PAT_REG1, pat1);
        } else {
            waitFifo(1);
        }

        ULONG trajectory = DST_X_DIR | DST_Y_DIR | PAT_MONO_EN;
        mmio.writeL(GUI_TRAJ_CNTL, trajectory);

        setDrawMode(pattern->FgPen, pattern->BgPen, pattern->DrawMode, fmt, MONO_SRC_PATTERN);

        waitFifo(2);
    } else {
        if (patternChanged) {
            waitIdle();

            for (UWORD i = 0; i < patternHeight; ++i) {
                // The video pattern has an 8-byte pitch. 64pixels (bits) is the minimum pitch for monochrome src blit
                // data.
                videoMemPattern[i * 2] = cachedPattern[i] << 16;
            }
        }

        setDrawMode(pattern->FgPen, pattern->BgPen, pattern->DrawMode, fmt, MONO_SRC_BLIT_SRC);

        UBYTE xOff = pattern->XOffset & 15;
        UWORD yOff = pattern->YOffset & (patternHeight - 1);

        ULONG pattCacheKey = (yOff << 16) | (xOff << 8) | pattern->Size;
        if (pattCacheKey != cd->patternCacheKey) {
            cd->patternCacheKey = pattCacheKey;

            waitFifo(6);

            mmio.writeL(SRC_Y_X, SRC_X(xOff) | SRC_Y(yOff));
            mmio.writeL(SRC_HEIGHT1_WIDTH1, SRC_HEIGHT1(patternHeight - yOff) | SRC_WIDTH1(16 - xOff));
            mmio.writeL(SRC_HEIGHT2_WIDTH2, SRC_HEIGHT2(patternHeight) | SRC_WIDTH2(16));
        } else {
            waitFifo(3);
        }
        ULONG trajectory = DST_X_DIR | DST_Y_DIR | SRC_PATT_EN | SRC_PATT_ROT_EN;
        mmio.writeL(GUI_TRAJ_CNTL, trajectory);
    }

    drawRect(x, y, width, height);
}

void ASM Mach64Driver::drawLine(__REGA1(struct RenderInfo *ri), __REGA2(struct Line *line), __REGD0(UBYTE mask),
                                __REGD7(RGBFTYPE_REG fmt))
{
    DFUNC(VERBOSE, "\n");

    setDstBuffer(ri, fmt);

    DRIVER_LOCALS(this);

    if (cd->GEOp != LINE) {
        cd->GEOp            = LINE;
        cd->GEdrawMode      = 0xFF;
        cd->patternCacheKey = 0xFFFFFFFF;

        waitFifo(4);

        // Offset is in units of '64 bit words' (8 bytes), while pitch is in units of '8 Pixels'.
        mmio.writeL(SRC_OFF_PITCH, SRC_OFFSET(memoryOffset(cd->patternVideoBuffer) / 8) | SRC_PITCH(8));
        mmio.writeMaskL(DP_PIX_WIDTH, DP_SRC_PIX_WIDTH_MASK, DP_SRC_PIX_WIDTH(COLOR_DEPTH_1));
        mmio.writeL(GUI_TRAJ_CNTL, SRC_LINE_X_DIR | SRC_PATT_EN | SRC_PATT_ROT_EN);
        mmio.writeL(SRC_HEIGHT2_WIDTH2, SRC_HEIGHT2(1) | SRC_WIDTH2(16));
    }
    UWORD *cachedPattern   = cd->patternCacheBuffer;
    ULONG *videoMemPattern = cd->patternVideoBuffer;

    // Try to avoid wait-for-idle by first checking if the pattern changed.
    BOOL patternChanged = FALSE;
    if (line->LinePtrn != cachedPattern[0]) {
        cachedPattern[0] = line->LinePtrn;
        patternChanged   = TRUE;
        waitIdle();
        videoMemPattern[0] = line->LinePtrn << 16;
    }

    UBYTE xOff = line->PatternShift & 15;

    ULONG pattCache = xOff;
    if (pattCache != cd->patternCacheKey) {
        cd->patternCacheKey = pattCache;

        waitFifo(5);

        mmio.writeL(SRC_Y_X, SRC_X(xOff) | SRC_Y(0));
        mmio.writeL(SRC_HEIGHT1_WIDTH1, SRC_HEIGHT1(1) | SRC_WIDTH1(16 - xOff));
    }

    setDrawMode(line->FgPen, line->BgPen, line->DrawMode, fmt, MONO_SRC_BLIT_SRC);

    UWORD direction = 0;

    WORD absMAX = myabs(line->lDelta);
    WORD absMIN = myabs(line->sDelta);

    WORD errTerm = 2 * absMIN - absMAX;
    if (line->dX > 0) {
        direction |= DST_X_DIR;
    }
    if (line->dY > 0) {
        direction |= DST_Y_DIR;
    }
    if (!line->Horizontal) {
        direction |= DST_Y_MAJOR;
    }

    setWriteMask(mask, fmt, 6);

    mmio.writeL(DST_CNTL, DST_LAST_PEL | direction);
    mmio.writeL(DST_BRES_INC, 2 * absMIN);
    mmio.writeL(DST_BRES_DEC, 2 * (absMIN - absMAX));
    mmio.writeL(DST_BRES_ERR, errTerm);
    mmio.writeL(DST_Y_X, DST_X(line->X) | DST_Y(line->Y));
    mmio.writeL(DST_BRES_LNTH, line->Length + 1);
}

void ASM Mach64Driver::waitBlitter()
{
    D(CHATTY, "Waiting for blitter...");

    waitIdle();

    D(CHATTY, "done\n");
}

APTR ASM Mach64Driver::allocCardMem(__REGD0(ULONG size), __REGD1(BOOL force), __REGD2(BOOL system),
                                    __REGD3(ULONG bytesperrow), __REGA1(struct ModeInfo *mi),
                                    __REGD7(RGBFTYPE_REG format))
{
#if MACH64_PCI_RETRY
    // SGRAM requires 64byte alignment (CalculateMemory)
    size += 64;
#endif
    return getConstCardData(this)->AllocCardMemDefault(this, size, force, system, bytesperrow, mi, AS_RGBF(format));
}

/* P96 BoardInfo entry stubs */
APTR ASM AllocCardMem(__REGA0(struct BoardInfo *bi), __REGD0(ULONG size), __REGD1(BOOL force), __REGD2(BOOL system),
                      __REGD3(ULONG bytesperrow), __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE_REG format))
{
    return asMach64(bi)->allocCardMem(size, force, system, bytesperrow, mi, format);
}

static void ASM SetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    asMach64(bi)->setGC(mi, border);
}

static void ASM SetPanning(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD3(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset),
                           __REGD7(RGBFTYPE_REG format))
{
    asMach64(bi)->setPanning(memory, width, height, xoffset, yoffset, format);
}

static UWORD ASM CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                      __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE_REG format))
{
    return asMach64(bi)->calculateBytesPerRow(width, height, mi, format);
}

static APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi), __REGA1(APTR memory), __REGD0(struct RenderInfo *ri),
                                __REGD7(RGBFTYPE_REG format))
{
    return asMach64(bi)->calculateMemory(memory, ri, format);
}

static ULONG ASM GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE_REG format))
{
    return asMach64(bi)->getCompatibleFormats(format);
}

static void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD0(UWORD region), __REGD7(RGBFTYPE_REG format))
{
    asMach64(bi)->setDAC(region, format);
}

static void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    asMach64(bi)->setColorArray(startIndex, count);
}

void ASM SetColorArrayInternal(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count),
                               __REGA1(const struct CLUTEntry *colors))
{
    asMach64(bi)->setColorArrayInternal(startIndex, count, colors);
}

static BOOL ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    return asMach64(bi)->setDisplay(state);
}

static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE_REG format))
{
    asMach64(bi)->setMemoryMode(format);
}

static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    (void)bi;
    (void)mask;
}

static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    (void)bi;
    (void)mask;
}

static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    (void)bi;
    (void)mask;
}

static LONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi),
                                  __REGD0(ULONG pixelClock), __REGD7(RGBFTYPE_REG RGBFormat))
{
    return asMach64(bi)->resolvePixelClock(mi, pixelClock, RGBFormat);
}

static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD7(RGBFTYPE_REG format))
{
    return asMach64(bi)->getPixelClock(mi, index, format);
}

static void ASM WaitVerticalSync(__REGA0(struct BoardInfo *bi), __REGD0(BOOL end))
{
    (void)bi;
    (void)end;
}

static BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL expected))
{
    return asMach64(bi)->getVSyncState(expected);
}

static ULONG ASM GetVBeamPos(__REGA0(struct BoardInfo *bi))
{
    return asMach64(bi)->getVBeamPos();
}

static BOOL ASM SetInterrupt(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    return asMach64(bi)->setInterrupt(state);
}

ULONG ASM interruptServer(__REGA1(struct BoardInfo *bi))
{
    return asMach64(bi)->interruptServer();
}
DEFINE_INTSERVER(interruptServerTrampoline, interruptServer);

static void ASM SetDPMSLevel(__REGA0(struct BoardInfo *bi), __REGD0(ULONG level))
{
    asMach64(bi)->setDPMSLevel(level);
}

static BOOL ASM SetSprite(__REGA0(struct BoardInfo *bi), __REGD0(BOOL activate), __REGD7(RGBFTYPE_REG RGBFormat))
{
    return asMach64(bi)->setSprite(activate, RGBFormat);
}

static void ASM SetSpritePosition(__REGA0(struct BoardInfo *bi), __REGD0(WORD xpos), __REGD1(WORD ypos),
                                  __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->setSpritePosition(xpos, ypos, fmt);
}

static void ASM SetSpriteImage(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->setSpriteImage(fmt);
}

static void ASM SetSpriteColor(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE index), __REGD1(UBYTE red),
                               __REGD2(UBYTE green), __REGD3(UBYTE blue), __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->setSpriteColor(index, red, green, blue, fmt);
}

static void ASM WaitBlitter(__REGA0(struct BoardInfo *bi))
{
    asMach64(bi)->waitBlitter();
}

static void ASM BlitRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD srcX),
                         __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                         __REGD5(WORD height), __REGD6(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->blitRect(ri, srcX, srcY, dstX, dstY, width, height, mask, fmt);
}

static void ASM InvertRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                           __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                           __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->invertRect(ri, x, y, width, height, mask, fmt);
}

static void ASM FillRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                         __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(ULONG pen),
                         __REGD5(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->fillRect(ri, x, y, width, height, pen, mask, fmt);
}

static void ASM BlitTemplate(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                             __REGA2(struct Template *tmpl), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                             __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->blitTemplate(ri, tmpl, x, y, width, height, mask, fmt);
}

static void ASM BlitPlanar2Chunky(__REGA0(struct BoardInfo *bi), __REGA1(struct BitMap *bm),
                                  __REGA2(struct RenderInfo *ri), __REGD0(SHORT srcX), __REGD1(SHORT srcY),
                                  __REGD2(SHORT dstX), __REGD3(SHORT dstY), __REGD4(SHORT width), __REGD5(SHORT height),
                                  __REGD6(UBYTE minTerm), __REGD7(UBYTE mask))
{
    asMach64(bi)->blitPlanar2Chunky(bm, ri, srcX, srcY, dstX, dstY, width, height, minTerm, mask);
}

static void ASM BlitRectNoMaskComplete(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *sri),
                                       __REGA2(struct RenderInfo *dri), __REGD0(WORD srcX), __REGD1(WORD srcY),
                                       __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                                       __REGD5(WORD height), __REGD6(UBYTE opCode), __REGD7(RGBFTYPE_REG format))
{
    asMach64(bi)->blitRectNoMaskComplete(sri, dri, srcX, srcY, dstX, dstY, width, height, opCode, format);
}

void ASM DrawLine(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGA2(struct Line *line),
                  __REGD0(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->drawLine(ri, line, mask, fmt);
}

static void ASM BlitPattern(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                            __REGA2(struct Pattern *pattern), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                            __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->blitPattern(ri, pattern, x, y, width, height, mask, fmt);
}

BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    LOCAL_SYSBASE();

    DFUNC(ALWAYS, "\n");

    bi->GraphicsControllerType = GCT_ATIRV100;
    bi->PaletteChipType        = PCT_ATT_20C492;
    bi->Flags                  = bi->Flags | BIF_GRANTDIRECTACCESS | BIF_HARDWARESPRITE | BIF_BLITTER;

    /* RGBFormats filled after chipFamily is known (GX DAC vs CT+ BE window). */

    bi->SoftSpriteFlags = 0;

    getCardData(bi)->AllocCardMemDefault = bi->AllocCardMem;
    P96_HOOK(bi->AllocCardMem, AllocCardMem);

    P96_HOOK(bi->SetGC, SetGC);
    P96_HOOK(bi->SetPanning, SetPanning);
    P96_HOOK(bi->CalculateBytesPerRow, CalculateBytesPerRow);
    P96_HOOK(bi->CalculateMemory, CalculateMemory);
    P96_HOOK(bi->GetCompatibleFormats, GetCompatibleFormats);
    P96_HOOK(bi->SetDAC, SetDAC);
    P96_HOOK(bi->SetColorArray, SetColorArray);
    P96_HOOK(bi->SetDisplay, SetDisplay);
    P96_HOOK(bi->SetMemoryMode, SetMemoryMode);
    P96_HOOK(bi->SetWriteMask, SetWriteMask);
    P96_HOOK(bi->SetReadPlane, SetReadPlane);
    P96_HOOK(bi->SetClearMask, SetClearMask);
    P96_HOOK(bi->ResolvePixelClock, ResolvePixelClock);
    P96_HOOK(bi->GetPixelClock, GetPixelClock);
    // SetClock is set up by chip-specific init functions (InitMach64GT, etc.)

    // VBlank IRQ (card registers HardInterrupt via pci_add_intserver)
    P96_HOOK(bi->WaitVerticalSync, WaitVerticalSync);
    P96_HOOK(bi->GetVSyncState, GetVSyncState);
    P96_HOOK(bi->GetVBeamPos, GetVBeamPos);
    P96_HOOK(bi->SetInterrupt, SetInterrupt);
    bi->HardInterrupt.is_Code = (void (*)())interruptServerTrampoline;

    // DPMS
    P96_HOOK(bi->SetDPMSLevel, SetDPMSLevel);

    // // Mouse Sprite
    P96_HOOK(bi->SetSprite, SetSprite);
    P96_HOOK(bi->SetSpritePosition, SetSpritePosition);
    P96_HOOK(bi->SetSpriteImage, SetSpriteImage);
    P96_HOOK(bi->SetSpriteColor, SetSpriteColor);

    // // Blitter acceleration
    P96_HOOK(bi->WaitBlitter, WaitBlitter);
    P96_HOOK(bi->BlitRect, BlitRect);
    P96_HOOK(bi->InvertRect, InvertRect);
    P96_HOOK(bi->FillRect, FillRect);
    P96_HOOK(bi->BlitTemplate, BlitTemplate);
    P96_HOOK(bi->BlitPlanar2Chunky, BlitPlanar2Chunky);
    P96_HOOK(bi->BlitRectNoMaskComplete, BlitRectNoMaskComplete);
    P96_HOOK(bi->DrawLine, DrawLine);
    P96_HOOK(bi->BlitPattern, BlitPattern);

    // Informed by the largest X/Y coordinates the blitter can talk to
    bi->MaxBMWidth  = 4096;
    bi->MaxBMHeight = 16384;

    bi->BitsPerCannon          = 8;
    bi->MaxHorValue[PLANAR]    = 4088;
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

    /* MMIO is last 1KB of the LE 8MB window (+0x7FFC00), not +0x7FF800/2KB. */
    setCacheMode(bi, bi->MemoryBase + mach64MmioOffsetInBar0(0x800000UL), 1024, MAPP_IO | MAPP_CACHEINHIBIT,
                 CACHEFLAGS);

    DRIVER_LOCALS(bi);
    {
        struct pci_dev *board = getCardData(bi)->board;
        DFUNC(INFO, "Determine Chip Family\n");

        // Allocate ChipSpecific structure
        cd->chipSpecific = (ChipSpecific_t *)AllocMem(sizeof(ChipSpecific_t), MEMF_ANY | MEMF_CLEAR);
        if (!cd->chipSpecific) {
            DFUNC(ERROR, "Failed to allocate ChipSpecific\n");
            return FALSE;
        }

        ULONG revision;
        ULONG deviceId;
        LOCAL_OPENPCIBASE();
        GetBoardAttrs(board, PRM_Device, (Tag)&deviceId, PRM_Revision, (Tag)&revision, TAG_END);

        cd->chipFamily      = getChipFamily(deviceId);
        cd->fifoSlotsCached = 0xffff; /* no free slots known yet */

        if (cd->chipFamily == UNKNOWN || !mach64ChipFamilySupported(cd->chipFamily)) {
            DFUNC(ERROR, "Unsupported chip family for this driver, aborting\n");
            return FALSE;
        }
        D(INFO, "Chip family: %s\n", getChipFamilyName(cd->chipFamily));

        /* LE aperture formats for all; CT+ also advertise BE (+8MB) aliases. */
        bi->RGBFormats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC;
        if (cd->chipFamily >= MACH64CT) {
            bi->RGBFormats |= RGBFF_B8G8R8A8 | RGBFF_A8R8G8B8 | RGBFF_R5G6B5 | RGBFF_R5G5B5;
        }
        /* GX: 32bpp LE order is DAC-specific — InitMach64GX ORs it. */

        /* Required for BAR decode; GX also needs IO for sparse CONFIG_CNTL. */
        {
            UWORD cmd = pci_read_config_word(PCI_COMMAND, board);
            pci_write_config_word(PCI_COMMAND, cmd | PCI_COMMAND_MEMORY | PCI_COMMAND_IO, board);
        }

        // User-Defines configuration
        UBYTE config    = pci_read_config_byte(0x40, board);
        ULONG prmStatus = pci_read_config_long(0x60, board);

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

        // Try making it more compatible with other VGA cards down the line
        // By disabling all classic IO decoding
        config |= 0x08;  // Disable decoding GENENA (no response at IO 0x46E8)
        pci_write_config_byte(0x40, config, board);

        /* Seed sparse I/O before ROM parse (ROM itself carries the same base). */
        if (ioBase)
            cd->ioSparseBase = ioBase;
    }

    // Test scratch register response
    D(INFO, "MMIO base address: 0x%08lx\n", (ULONG)asMach64(bi)->mmioBase());
    D(INFO, "Register base address: 0x%08lx\n", (ULONG)asMach64(bi)->ioBase());
    if (cd->chipFamily != MACH64GX) {
        /* CT letter f / VT+: AP_SIZE=2 → 2×8M (LE+BE). GX uses sparse I/O below. */
        if ((mmio.readL(CONFIG_CNTL) & CFG_MEM_AP_SIZE_MASK) != CFG_MEM_AP_SIZE_8M)
            mmio.writeMaskL(CONFIG_CNTL, CFG_MEM_AP_SIZE_MASK, CFG_MEM_AP_SIZE_8M);
        D(INFO, "CONFIG_CNTL=0x%08lx (AP_SIZE=%ld)\n", mmio.readL(CONFIG_CNTL),
          mmio.readL(CONFIG_CNTL) & CFG_MEM_AP_SIZE_MASK);

        /* CT/VT+: CONFIG_CNTL via MMIO — CT has no GX sparse-IO aperture programming. */
        ULONG saveScratchReg1 = mmio.readL(SCRATCH_REG1);
        mmio.writeL(SCRATCH_REG1, 0xAAAAAAAA);
        ULONG scratchA = mmio.readL(SCRATCH_REG1);
        mmio.writeL(SCRATCH_REG1, 0x55555555);
        ULONG scratch5 = mmio.readL(SCRATCH_REG1);
        mmio.writeL(SCRATCH_REG1, saveScratchReg1);
        if (scratchA != 0xAAAAAAAA || scratch5 != 0x55555555) {
            DFUNC(ERROR, "scratch register response broken.\n");
            return FALSE;
        }
    } else {
        LEGACYIOBASE();
        /* Warm reinit: aperture already 8MB — avoid redundant CONFIG_CNTL RMW. */
        if ((R_IO_L(CONFIG_CNTL) & CFG_MEM_AP_SIZE_MASK) != CFG_MEM_AP_SIZE_8M)
            W_IO_MASK_L(CONFIG_CNTL, CFG_MEM_AP_SIZE_MASK, CFG_MEM_AP_SIZE_8M);

        ULONG saveScratchReg1 = mmio.readL(SCRATCH_REG1);
        mmio.writeL(SCRATCH_REG1, 0xAAAAAAAA);
        ULONG scratchA = mmio.readL(SCRATCH_REG1);
        mmio.writeL(SCRATCH_REG1, 0x55555555);
        ULONG scratch5 = mmio.readL(SCRATCH_REG1);
        mmio.writeL(SCRATCH_REG1, saveScratchReg1);
        if (scratchA != 0xAAAAAAAA || scratch5 != 0x55555555) {
            DFUNC(ERROR, "scratch register response broken.\n");
            return FALSE;
        }
    }

    D(INFO, "scratch register response good.\n");

    /* Warm reinit: CFG_VGA_DIS must be clear for Expansion ROM to be accessible*/
    {
        if (cd->chipFamily == MACH64GX) {
            LEGACYIOBASE();
            W_IO_MASK_L(CONFIG_CNTL, CFG_VGA_DIS_MASK, 0);
        } else {
            mmio.writeMaskL(CONFIG_CNTL, CFG_VGA_DIS_MASK, 0);
        }
        mmio.writeMaskL(BUS_CNTL, BUS_ROM_DIS_MASK, 0);
    }

    if (!parseRomHeader(bi)) {
        DFUNC(ERROR, "Failed to parse ROM header\n");
        return FALSE;
    }

    switch (cd->chipFamily) {
#if !MACH64_PCI_RETRY
    case MACH64GX:
        if (!InitMach64GX(bi)) {
            return FALSE;
        }
        break;
    case MACH64CT:
        if (!InitMach64CT(bi)) {
            return FALSE;
        }
        break;
#else
    case MACH64VT:
        if (!InitMach64VT(bi)) {
            return FALSE;
        }
        break;
    case MACH64GT:
    case MACH64GM:
        if (!InitMach64GT(bi)) {
            return FALSE;
        }
        break;
#endif
    default:
        D(ERROR, "Unsupported chip family\n");
        return FALSE;
    }

    if (cd->chipFamily >= MACH64CT) {
        /* CT/VT+: VGA off via CONFIG_CNTL only (RRG CT STAT0 has no CFG_VGA_EN). */
        mmio.writeMaskL(CONFIG_CNTL, CFG_VGA_DIS_MASK | CFG_MEM_VGA_AP_EN_MASK, CFG_VGA_DIS);
    } else {
        /* GX: never touch CONFIG_STAT0 (CFG_MEM_TYPE_GX bits 3–5 — wedges BAR0).
         * Set CFG_VGA_DIS for accelerator CRT, knowing it disables ROM access
         * until the next InitChip clears it again (see CFG_VGA_DIS define). */
        LEGACYIOBASE();
        W_IO_MASK_L(CONFIG_CNTL, CFG_VGA_DIS_MASK | CFG_MEM_VGA_AP_EN_MASK, CFG_VGA_DIS);
    }

    /* MCLK: CT/VT/GT program in InitMach64*; GX reports ROM default only. */
    if (cd->chipFamily == MACH64GX && !bi->MemoryClock)
        bi->MemoryClock = (ULONG)resolveMemoryClockKhz10(bi) * 10000UL;
    D(INFO, "MemoryClock %ld Hz\n", bi->MemoryClock);

    // FIXME: no need to crop FB size on later chips with auxilliary MMIO aperture
    if (bi->MemorySize == 8 * 1024 * 1024 && cd->chipFamily <= MACH64VT) {
        bi->MemorySize -= 2048;  // Upper 2kb are reserved for MMIO register blocks 0 and 1
    }

    // Init DAC: 8-bit LUT + optional blanking pedestal; disable legacy VGA DAC
    // decode. GX external DAC is programmed in SetDAC_GX.
    if (cd->chipFamily != MACH64GX) {
        ULONG dacBits = DAC_8BIT_EN;
        if (!(bi->CardFlags & CFF_BLACKLEVEL_BLACK))
            dacBits |= DAC_BLANKING;
        mmio.writeMaskL(DAC_CNTL, DAC_BLANKING_MASK | DAC_VGA_ADR_EN | DAC_8BIT_EN_MASK, dacBits);
        mmio.writeB(DAC_REGS, DAC_MASK, 0xFF);
        D(INFO, "DAC_CNTL=0x%08lx BUS_CNTL=0x%08lx\n", mmio.readL(DAC_CNTL), mmio.readL(BUS_CNTL));
    }

    // Init CRTC. Display FIFO LWM/OVERFILL: mode-tuned in AdjustCrtcFifo_VT / AdjustDSP.
    // Do not set CRTC_LOCK_REGS — that blocks later CRTC programming.
    // CRTC_DISP_REQ_ENB = 0 _enables_ display requests.
    mmio.writeMaskL(CRTC_GEN_CNTL,
                    CRTC_ENABLE_MASK | CRTC_EXT_DISP_EN_MASK | CRTC_DISP_REQ_ENB_MASK | VGA_XCRT_CNT_EN_MASK |
                        VGA_ATI_LINEAR_MASK | CRTC_CSYNC_EN | CRTC_PIC_BY_2_EN | CRTC_HSYNC_DIS | CRTC_VSYNC_DIS |
                        CRTC_LOCK_REGS_MASK,
                    CRTC_ENABLE | CRTC_EXT_DISP_EN /*| VGA_XCRT_CNT_EN*/);
    if (cd->chipFamily == MACH64CT) {
        /* Mode set retunes via AdjustCrtcFifo_CT; start below max (0xF starves GE). */
        mmio.writeMaskL(CRTC_GEN_CNTL, CRTC_FIFO_LWM_MASK, CRTC_FIFO_LWM(0x8));
    } else if (cd->chipFamily == MACH64VT) {
        mmio.writeMaskL(CRTC_GEN_CNTL, CRTC_FIFO_LWM_MASK | CRTC_FIFO_OVERFILL_VT_MASK | CRTC_DISPREQ_ONLY_VT_MASK,
                        CRTC_FIFO_LWM(0x8) | CRTC_FIFO_OVERFILL_VT(1));
    }

    // Init Engine
    ResetEngine(bi);

    waitFifo(bi, 16);
    mmio.writeL(CONTEXT_MASK, 0xFFFFFFFF);
    mmio.writeL(DST_Y_X, 0);
    mmio.writeL(DST_BRES_ERR, 0);
    mmio.writeL(DST_BRES_INC, 0);
    mmio.writeL(DST_BRES_DEC, 0);
    mmio.writeL(DST_CNTL, DST_LAST_PEL | DST_Y_DIR | DST_X_DIR);

    mmio.writeL(SRC_Y_X, 0);
    mmio.writeL(SRC_HEIGHT1_WIDTH1, 1);
    mmio.writeL(SRC_Y_X_START, 0);
    mmio.writeL(SRC_HEIGHT2_WIDTH2, 1);
    mmio.writeL(SC_LEFT_RIGHT, SC_LEFT(0) | ((SC_RIGHT_MASK >> 1) & SC_RIGHT_MASK));
    mmio.writeL(SC_TOP_BOTTOM, SC_TOP(0) | ((SC_BOTTOM_MASK >> 1) & SC_BOTTOM_MASK));
    mmio.writeL(SRC_Y_X_START, 0);
    // FIXME: Docs say: Use SRC_CNTL register only if a blit source is selected in the pixel data path.
    // What does that mean? Also, if I don't do this here, I get the dreaded GUI hangs. Is it about Busmastering
    // being tuned off vaia SRC_CNTL?
    mmio.writeL(SRC_CNTL, 0);

    waitFifo(bi, 16);
    mmio.writeL(DP_BKGD_CLR, 0x0);
    mmio.writeL(DP_FRGD_CLR, 0xFFFFFFFF);
    mmio.writeL(DP_WRITE_MSK, 0xFFFFFFFF);
    mmio.writeL(DP_PIX_WIDTH,
                DP_DST_PIX_WIDTH(COLOR_DEPTH_8) | DP_SRC_PIX_WIDTH(COLOR_DEPTH_8) | DP_HOST_PIX_WIDTH(COLOR_DEPTH_8));
    mmio.writeL(DP_MIX, DP_BKGD_MIX(MIX_ZERO) | DP_FRGD_MIX(MIX_NEW));
    mmio.writeL(DP_SRC, DP_BKGD_SRC(CLR_SRC_BKGD_COLOR) | DP_FRGD_SRC(CLR_SRC_FRGD_COLOR) | DP_MONO_SRC(MONO_SRC_ONE));
    mmio.writeL(CLR_CMP_CNTL, 0x0);
    mmio.writeL(GUI_TRAJ_CNTL, DST_X_DIR | DST_Y_DIR | DST_LAST_PEL);

    D(INFO, "Monitor is %s present\n", ((mmio.readB(DAC_CNTL, 0) & 0x80) ? "NOT" : ""));

    queryEDID(bi);

    // Two sprite images, each 64x64*2 bits
    // BEWARE: softsprite data would use 4 byte per pixel
    const ULONG maxSpriteBuffersSize = (64 * 64 * 2 / 8) * 2;
    bi->MemorySize                   = (bi->MemorySize - maxSpriteBuffersSize) & ~(63);  // align to 64 byte boundary

    bi->MouseImageBuffer = bi->MemoryBase + bi->MemorySize;
    // FIXME: is this one even needed?
    bi->MouseSaveBuffer = bi->MemoryBase + bi->MemorySize + maxSpriteBuffersSize / 2;

    // reserve memory for a pattern that can be up to 256 lines high (2kb)
    // Since the minimum pitch for SRC_PITCH is 64 monochrome pixels (8 byte), we need to overallocate.
    // The P96 pattern is just 16 pixels (bits) wide.
    ULONG patternSize      = 8 * 256;
    bi->MemorySize         = (bi->MemorySize - patternSize) & ~(7);
    cd->patternVideoBuffer = (ULONG *)(bi->MemoryBase + bi->MemorySize);
    cd->patternCacheBuffer = (UWORD *)AllocVec(patternSize, MEMF_PUBLIC);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef TESTEXE

#include <boardinfo.h>
#include <exec/interrupts.h>
#include <exec/nodes.h>
#include <libraries/openpci.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/openpci.h>
#include <proto/utility.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VENDOR_E3B 0xE3B
/* BoardInfo alone is ~2.5KB on stack; default CLI stack is 4KB. */
ULONG __stack = 65536;
#define VENDOR_MATAY      0xAD47
#define DEVICE_FIRESTORM  200
#define DEVICE_PROMETHEUS 1

#ifdef __cplusplus
} /* OpenPciBase was declared with C++ linkage via proto/openpci.h */
struct Library *OpenPciBase = NULL;
extern "C" {
#else
struct Library *OpenPciBase = NULL;
#endif

/*
 * Byte stores into linear FB (same idea as TestMach32). Known layout for med:
 *   rows 0..15: horizontal ramp x&0xFF
 *   rest:       x^y
 * Example: med db <BAR0> 64  → 00 01 02 … on first line.
 */
static void testFillPattern8bppBytes(BoardInfo_t *bi, UWORD width, UWORD height)
{
    LOCAL_SYSBASE();
    volatile UBYTE *mem = (volatile UBYTE *)bi->MemoryBase;
    UWORD bpr           = width;
    if (bi->CalculateBytesPerRow)
        bpr = bi->CalculateBytesPerRow(bi, width, height, bi->ModeInfo, RGBFB_CLUT);

    UWORD gradientRows = 16;
    if (gradientRows > height)
        gradientRows = height;
    for (UWORD y = 0; y < gradientRows; y++) {
        for (UWORD x = 0; x < width; x++)
            mem[(ULONG)y * (ULONG)bpr + (ULONG)x] = (UBYTE)(x & 0xFF);
    }
    for (UWORD y = gradientRows; y < height; y++) {
        for (UWORD x = 0; x < width; x++)
            mem[(ULONG)y * (ULONG)bpr + (ULONG)x] = (UBYTE)(x ^ y);
    }
    CacheClearU();
    D(INFO, "CPU FB pattern: %ldx%ld bpr=%ld (ramp then x^y) at 0x%08lx\n", (ULONG)width, (ULONG)height, (ULONG)bpr,
      (ULONG)bi->MemoryBase);
}

/* True 8-bit match, or 6-bit DAC trunc/expand (drop low 2 bits). Else broken. */
static BOOL lutGunOk(UBYTE got, UBYTE wr, BOOL *sixBit)
{
    if (got == wr)
        return TRUE;
    if (got == (UBYTE)(wr & 0xFC) || got == (UBYTE)(wr >> 2)) {
        *sixBit = TRUE;
        return TRUE;
    }
    return FALSE;
}

static void verifyPaletteReadback(BoardInfo_t *bi)
{
    DRIVER_LOCALS(bi);
    LOCAL_SYSBASE();
    static const UWORD idxs[] = {0, 1, 2, 127, 128, 254, 255};
    ULONG broken              = 0;
    BOOL sixBit               = FALSE;
    UBYTE got[7][3];
    UBYTE wr[7][3];
    BOOL bad[7];

    mmio.writeL(DAC_CNTL, mmio.readL(DAC_CNTL) & ~3UL);

    Disable();
    mmio.writeB(DAC_REGS, DAC_MASK, 0xFF);
    /* VGA-style waste before switching to read index (aty_dac_waste4). */
    (void)mmio.readB(DAC_REGS, DAC_W_INDEX);
    (void)mmio.readB(DAC_REGS, DAC_MASK);
    (void)mmio.readB(DAC_REGS, DAC_MASK);
    (void)mmio.readB(DAC_REGS, DAC_MASK);
    (void)mmio.readB(DAC_REGS, DAC_MASK);

    for (UWORD i = 0; i < sizeof(idxs) / sizeof(idxs[0]); ++i) {
        UWORD idx = idxs[i];
        wr[i][0]  = bi->CLUT[idx].Red;
        wr[i][1]  = bi->CLUT[idx].Green;
        wr[i][2]  = bi->CLUT[idx].Blue;
        mmio.writeB(DAC_REGS, DAC_R_INDEX, (UBYTE)idx);
        got[i][0] = mmio.readB(DAC_REGS, DAC_W_DATA);
        got[i][1] = mmio.readB(DAC_REGS, DAC_W_DATA);
        got[i][2] = mmio.readB(DAC_REGS, DAC_W_DATA);
        bad[i]    = !lutGunOk(got[i][0], wr[i][0], &sixBit) || !lutGunOk(got[i][1], wr[i][1], &sixBit) ||
                 !lutGunOk(got[i][2], wr[i][2], &sixBit);
        if (bad[i])
            ++broken;
    }
    Enable();

    if (broken) {
        D(ALWAYS, "LUT readback: %ld mismatches - DAC write path broken\n", broken);
        for (UWORD i = 0; i < sizeof(idxs) / sizeof(idxs[0]); ++i) {
            if (bad[i]) {
                D(ALWAYS, "  LUT[%ld] read R=%ld G=%ld B=%ld wrote %ld/%ld/%ld\n", (ULONG)idxs[i], (ULONG)got[i][0],
                  (ULONG)got[i][1], (ULONG)got[i][2], (ULONG)wr[i][0], (ULONG)wr[i][1], (ULONG)wr[i][2]);
            }
        }
    } else if (sixBit) {
        D(ALWAYS, "LUT readback OK (warning: DAC drops low 2 bits / 6-bit)\n");
    } else {
        D(ALWAYS, "LUT readback OK (8-bit)\n");
    }
}

static void dumpScanout8bpp(BoardInfo_t *bi)
{
    DRIVER_LOCALS(bi);
    ULONG gen   = mmio.readL(CRTC_GEN_CNTL);
    ULONG pitch = mmio.readL(CRTC_OFF_PITCH);
    ULONG vtd   = mmio.readL(CRTC_V_TOTAL_DISP);
    ULONG dac   = mmio.readL(DAC_CNTL);
    ULONG cfg0  = mmio.readL(CONFIG_STAT0);
    D(ALWAYS, "CRTC_GEN_CNTL=0x%08lx pixw=%ld dbl=%ld int=%ld pic_by2=%ld ext=%ld en=%ld\n", gen, (gen >> 8) & 7,
      !!(gen & CRTC_DBL_SCAN_EN), !!(gen & CRTC_INTERLACE_EN), !!(gen & CRTC_PIC_BY_2_EN), !!(gen & CRTC_EXT_DISP_EN),
      !!(gen & CRTC_ENABLE));
    D(ALWAYS, "CRTC_V_TOTAL_DISP=0x%08lx vtot=%ld vdisp=%ld\n", vtd, vtd & 0x7ff, (vtd >> 16) & 0x7ff);
    D(ALWAYS, "CRTC_OFF_PITCH=0x%08lx off=0x%lx pitch8=%ld\n", pitch, pitch & 0xfffff, (pitch >> 22) & 0x3ff);
    D(ALWAYS, "DAC_CNTL=0x%08lx dac8=%ld rs=%ld type_byte=%ld\n", dac, !!(dac & BIT(8)), dac & 3, (dac >> 16) & 7);
    if (cd->chipFamily == MACH64GX) {
        D(ALWAYS, "CONFIG_STAT0_GX bus=%ld mem_type=%ld dac_strap=%ld vga_en=%ld\n",
          (ULONG)(cfg0 & CFG_BUS_TYPE_GX_MASK), (ULONG)((cfg0 & CFG_MEM_TYPE_GX_MASK) >> 3),
          (ULONG)((cfg0 & CFG_INIT_DAC_TYPE_GX_MASK) >> 9), !!(cfg0 & CFG_VGA_EN_GX));
    } else {
        D(ALWAYS, "CONFIG_STAT0_CT mem_type=%ld dual_cas=%ld clock_en=%ld\n", (ULONG)(cfg0 & CFG_MEM_TYPE_CT_MASK),
          !!(cfg0 & CFG_DUAL_CAS_EN_CT), !!(cfg0 & CFG_CLOCK_EN_CT));
    }
    ULONG mem = mmio.readL(MEM_CNTL);
    if (cd->chipFamily == MACH64CT) {
        D(ALWAYS, "MEM_CNTL=0x%08lx CT size=%ld refresh=%ld refrate=%ld\n", mem, mem & 7, (mem >> 3) & 0xf,
          (mem >> 11) & 3);
    } else {
        D(ALWAYS, "MEM_CNTL=0x%08lx size=%ld latch=%ld\n", mem, mem & 7, !!(mem & 0x00f0));
    }
}

void intHandler(int dummy)
{
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
    }
    abort();
}

static volatile ULONG softVBlankCount;

static void ASM SoftVBlankCount(__REGA1(ULONG *count))
{
    (*count)++;
}

/* Register PCI VBlank server, enable chip IRQ, count SoftInterrupts for 2s. */
static void testVBlankInterrupt(BoardInfo_t *bi, struct pci_dev *board)
{
    DRIVER_LOCALS(bi);
    LOCAL_SYSBASE();
    softVBlankCount = 0;

    /* OpenPCI: server may run immediately — chip must not assert INTA yet. */
    Disable();
    {
        mmio.writeL(CRTC_INT_CNTL, CRTC_INT_ACKS);
        mmio.writeL(CRTC_INT_CNTL, 0);
    }
    cd->p96VBlankInt = 0;
    Enable();

    bi->SoftInterrupt.is_Node.ln_Type = NT_INTERRUPT;
    bi->SoftInterrupt.is_Node.ln_Pri  = 0;
    bi->SoftInterrupt.is_Node.ln_Name = (char *)"TestMach64SoftVBlank";
    bi->SoftInterrupt.is_Data         = (APTR)&softVBlankCount;
    bi->SoftInterrupt.is_Code         = (void (*)())SoftVBlankCount;

    bi->HardInterrupt.is_Node.ln_Type = NT_INTERRUPT;
    bi->HardInterrupt.is_Node.ln_Pri  = 0;
    bi->HardInterrupt.is_Node.ln_Name = (char *)"TestMach64VBlank";
    bi->HardInterrupt.is_Data         = bi;
    /* is_Code set by InitChip */

    if (!pci_add_intserver(&bi->HardInterrupt, board)) {
        D(ERROR, "VBlank IRQ test: pci_add_intserver failed\n");
        return;
    }

    bi->Flags |= BIF_VBLANKINTERRUPT;
    bi->SetInterrupt(bi, TRUE);

    {
        D(ALWAYS, "VBlank IRQ test: CRTC_INT_CNTL=0x%08lx — counting 2s...\n", mmio.readL(CRTC_INT_CNTL));
    }

    delayMilliSeconds(2000);

    ULONG count = softVBlankCount;
    bi->SetInterrupt(bi, FALSE);
    {
        mmio.writeL(CRTC_INT_CNTL, CRTC_INT_ACKS);
        mmio.writeL(CRTC_INT_CNTL, 0);
    }
    pci_rem_intserver(&bi->HardInterrupt, board);
    bi->Flags &= ~BIF_VBLANKINTERRUPT;

    D(ALWAYS, "VBlank IRQ test: %lu softints in 2s (~%lu Hz)\n", count, count / 2);
    if (count < 50)
        D(ERROR, "VBlank IRQ test: too few interrupts (expected ~120 @60Hz)\n");
}

/* ALL/S — cycle every built-in mode; VBLANK/S — PCI VBlank IRQ count; EEPROM/S — dump Microwire EEPROM. */
static const char testArgsTemplate[] = "ALL/S,VBLANK/S,EEPROM/S";

int main()
{
    signal(SIGINT, intHandler);

    int rval              = EXIT_FAILURE;
    LONG allModes         = FALSE;
    LONG vblankTest       = FALSE;
    LONG eepromDump       = FALSE;
    struct RDArgs *rdargs = NULL;
    struct pci_dev *board = NULL;

    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        D(0, "Unable to open openpci.library\n");
        goto exit;
    }

    {
        static LONG args[3];
        args[0] = args[1] = args[2] = 0;
        rdargs                      = ReadArgs((STRPTR)testArgsTemplate, args, NULL);
        if (!rdargs) {
            PrintFault(IoErr(), (STRPTR) "TestMach64");
            goto exit;
        }
        allModes   = args[0] ? TRUE : FALSE;
        vblankTest = args[1] ? TRUE : FALSE;
        eepromDump = args[2] ? TRUE : FALSE;
        FreeArgs(rdargs);
        rdargs = NULL;
    }

    D(0, "Looking for Mach64 card...\n");

    while ((board = FindBoard(board, PRM_Vendor, PCI_VENDOR, TAG_END)) != NULL) {
        ULONG Device, Revision, Memory0Size = 0, Memory2Size = 0;
        APTR Memory0 = 0, Memory1 = 0, Memory2 = 0, legacyIOBase = 0;

        GetBoardAttrs(board, PRM_Device, (Tag)&Device, PRM_Revision, (Tag)&Revision, PRM_MemoryAddr0, (Tag)&Memory0,
                      PRM_MemorySize0, (Tag)&Memory0Size, PRM_LegacyIOSpace, (Tag)&legacyIOBase, PRM_MemoryAddr1,
                      (Tag)&Memory1, PRM_MemoryAddr2, (Tag)&Memory2, PRM_MemorySize2, (Tag)&Memory2Size, TAG_END);

        D(0, "Found ATI device %x revision %x\n", Device, Revision);

        ChipFamily_t family = getChipFamily((UWORD)Device);

        if (family != UNKNOWN && !mach64ChipFamilySupported(family)) {
            D(ALWAYS, "Skipping %s (this binary is %s)\n", getChipFamilyName(family),
#if MACH64_PCI_RETRY
              "VT+/Rage (TestMach64 / ATIMach64.chip)"
#else
              "GX/CT (TestMach64GX / ATIMach64GX.chip)"
#endif
            );
            continue;
        }

        if (family != UNKNOWN && mach64ChipFamilySupported(family)) {
            D(ALWAYS, "ATI %s found\n", getChipFamilyName(family));

            {
                UWORD cmd = pci_read_config_word(PCI_COMMAND, board);
                D(ALWAYS, "PCI_COMMAND was 0x%04lx\n", (ULONG)cmd);
                /* Only OR in missing bits — a full rewrite can wedge this CT after a soft reset. */
                if ((cmd & (PCI_COMMAND_MEMORY | PCI_COMMAND_IO)) != (PCI_COMMAND_MEMORY | PCI_COMMAND_IO)) {
                    pci_write_config_word(PCI_COMMAND, cmd | PCI_COMMAND_MEMORY | PCI_COMMAND_IO, board);
                    D(ALWAYS, "PCI_COMMAND now 0x%04lx\n", (ULONG)pci_read_config_word(PCI_COMMAND, board));
                } else {
                    D(ALWAYS, "PCI_COMMAND already has MEM+IO\n");
                }
            }

            D(ALWAYS, "MemoryBase 0x%08lx, MemorySize %ld, BlockIOBase 0x%08lx, Aux MMIO Base 0x%08lx\n", Memory0,
              Memory0Size, Memory1, Memory2);

            APTR physicalAddress = pci_logic_to_physic_addr(Memory0, board);
            D(ALWAYS, "physicalAdress 0x%08lx\n", physicalAddress);

            struct ChipBase *ChipBase = NULL;

            /* BoardInfo is large — keep off the CLI stack. */
            static struct BoardInfo boardInfo;
            memset(&boardInfo, 0, sizeof(boardInfo));
            struct BoardInfo *bi = &boardInfo;

            bi->ExecBase                 = SysBase;
            bi->UtilBase                 = (struct Library *)UtilityBase;
            bi->ChipBase                 = ChipBase;
            getCardData(bi)->OpenPciBase = OpenPciBase;
            getCardData(bi)->board       = board;

            getCardData(bi)->legacyIOBase = (volatile UBYTE *)legacyIOBase + REGISTER_OFFSET;

            /* CT: optional BAR1 block-IO. VT+: required. GX: sparse IO via PCI 0x40. */
            if (family >= MACH64VT) {
                if (!Memory1) {
                    D(ERROR, "Cannot find block IO Aperture\n");
                    goto exit;
                }
                bi->RegisterBase = (UBYTE *)Memory1 + REGISTER_OFFSET;
                D(ALWAYS, "RegisterBase (block IO) 0x%08lx\n", bi->RegisterBase);
            } else if (family == MACH64CT && Memory1) {
                bi->RegisterBase = (UBYTE *)Memory1 + REGISTER_OFFSET;
                D(ALWAYS, "RegisterBase (CT block IO) 0x%08lx\n", bi->RegisterBase);
            } else {
                UBYTE userConfig = pci_read_config_byte(0x40, board);
                D(ALWAYS, "PCI 0x40: %02lx\n", (ULONG)userConfig);
            }

            bi->MemoryBase = (UBYTE *)Memory0;
            {
                ULONG mmioOff = mach64MmioOffsetInBar0(Memory0Size);
                /* FB below MMIO hole (+ optional BE half) — never NONSERIALIZED on regs. */
                D(ALWAYS, "setCacheMode FB...\n");
                if (mmioOff)
                    setCacheMode(bi, Memory0, mmioOff, MAPP_CACHEINHIBIT | MAPP_IMPRECISE | MAPP_NONSERIALIZED,
                                 CACHEFLAGS);
                if (Memory0Size > 0x800000UL)
                    setCacheMode(bi, (BYTE *)Memory0 + 0x800000UL, Memory0Size - 0x800000UL,
                                 MAPP_CACHEINHIBIT | MAPP_IMPRECISE | MAPP_NONSERIALIZED, CACHEFLAGS);

                if (Memory2) {
                    D(ALWAYS, "Using auxiliary register aperture at 0x%08lx\n", Memory2);
                    bi->MemoryIOBase = (UBYTE *)Memory2 + 1024 + MMIOREGISTER_OFFSET;
                    setCacheMode(bi, Memory2, Memory2Size, MAPP_IO | MAPP_CACHEINHIBIT, CACHEFLAGS);
                } else {
                    D(ALWAYS, "Using BAR0 MMIO at 0x%08lx (+0x%lx, BAR0 size %ld)\n", (BYTE *)Memory0 + mmioOff,
                      mmioOff, Memory0Size);
                    bi->MemoryIOBase = (UBYTE *)Memory0 + mmioOff + MMIOREGISTER_OFFSET;
                    setCacheMode(bi, (BYTE *)Memory0 + mmioOff, 1024, MAPP_IO | MAPP_CACHEINHIBIT, CACHEFLAGS);
                }
            }

            D(ALWAYS, "Mach64 init chip....\n");
            if (!InitChip(bi)) {
                D(ERROR, "InitChip failed, exit\n");
                rval = EXIT_FAILURE;
                goto exit;
            }
            D(ALWAYS, "Mach64 has %ldkb usable memory\n", bi->MemorySize / 1024);

            /* Usable FB only — full BAR would let a cache-mode change hit MMIO. */
            bi->MemorySpaceBase = Memory0;
            bi->MemorySpaceSize = bi->MemorySize;

            if (eepromDump) {
                dumpMach64Eeprom(bi);
                rval = EXIT_SUCCESS;
                goto exit;
            }

            bi->SetDisplay(bi, FALSE);

            {
                struct
                {
                    const char *name;
                    UWORD w, h, hTot, hSyncStart, hSyncSize, vTot, vSyncStart, vSyncSize;
                    ULONG pclk;
                    UBYTE flags;
                } modes[] = {
                    {"640x480", 640, 480, 800, 16, 96, 525, 10, 2, 25175000, GMF_HPOLARITY | GMF_VPOLARITY},
                    {"800x600", 800, 600, 1056, 40, 128, 628, 1, 4, 40000000, 0},
                    {"1024x768", 1024, 768, 1344, 24, 136, 806, 3, 6, 65000000, GMF_HPOLARITY | GMF_VPOLARITY},
                    /* Doublescan: logical V (ToScanLines ×2 → physical). 320x200d clock
                     * is ~12.59 MHz — CT VPLL near floor can look like a bad LUT. */
                    {"320x200d", 320, 200, 400, 16, 48, 225, 6, 1, 12587500,
                     GMF_DOUBLESCAN | GMF_HPOLARITY | GMF_VPOLARITY},
                    {"1024x384d", 1024, 384, 1344, 24, 136, 403, 2, 3, 65000000,
                     GMF_DOUBLESCAN | GMF_HPOLARITY | GMF_VPOLARITY},
                };
                const int modeCount = allModes ? (int)(sizeof(modes) / sizeof(modes[0])) : 1;

                static struct ModeInfo testMi;

                D(ALWAYS, allModes ? "Mode test: ALL\n" : "Mode test: 640x480 only (use ALL to cycle)\n");

                for (int m = 0; m < modeCount; ++m) {
                    memset(&testMi, 0, sizeof(testMi));
                    testMi.Depth        = 8;
                    testMi.Flags        = modes[m].flags;
                    testMi.Width        = modes[m].w;
                    testMi.Height       = modes[m].h;
                    testMi.HorTotal     = modes[m].hTot;
                    testMi.HorBlankSize = 8;
                    testMi.HorSyncStart = modes[m].hSyncStart;
                    testMi.HorSyncSize  = modes[m].hSyncSize;
                    testMi.VerTotal     = modes[m].vTot;
                    testMi.VerBlankSize = 8;
                    testMi.VerSyncStart = modes[m].vSyncStart;
                    testMi.VerSyncSize  = modes[m].vSyncSize;
                    testMi.PixelClock   = modes[m].pclk;

                    bi->ModeInfo = &testMi;
                    bi->SetDisplay(bi, FALSE);
                    bi->ResolvePixelClock(bi, &testMi, testMi.PixelClock, RGBFB_CLUT);
                    D(ALWAYS, "Resolved PixelClock %ld Hz (N=%ld Pidx=%ld)\n", testMi.PixelClock,
                      (ULONG)testMi.pll1.Numerator, (ULONG)testMi.pll2.Denominator);
                    if (bi->SetClock) {
                        bi->SetClock(bi);
                    }
                    bi->SetGC(bi, &testMi, TRUE);

                    for (int c = 0; c < 256; c++) {
                        bi->CLUT[c].Red = bi->CLUT[c].Green = bi->CLUT[c].Blue = (UBYTE)c;
                    }
                    bi->SetDAC(bi, 0, RGBFB_CLUT);
                    bi->SetColorArray(bi, 0, 256);
                    verifyPaletteReadback(bi);
                    bi->SetPanning(bi, bi->MemoryBase, modes[m].w, modes[m].h, 0, 0, RGBFB_CLUT);
                    bi->SetDisplay(bi, TRUE);

                    {
                        /* CPU pattern for med; small GUI rect proves blitter still works. */
                        testFillPattern8bppBytes(bi, modes[m].w, modes[m].h);
                        struct RenderInfo ri;
                        ri.Memory      = bi->MemoryBase;
                        ri.BytesPerRow = modes[m].w;
                        ri.RGBFormat   = RGBFB_CLUT;
                        FillRect(bi, &ri, (WORD)(modes[m].w / 4), (WORD)(modes[m].h / 4), (WORD)(modes[m].w / 2),
                                 (WORD)(modes[m].h / 2), 0xFF, 0xFF, RGBFB_CLUT);
                        WaitBlitter(bi);
                    }

                    dumpScanout8bpp(bi);
                    DFUNC(ALWAYS, "Showing %s - check sync (3s)\n", modes[m].name);
                    delayMilliSeconds(3000);
                }

                /* Rest on 640x480 for the blit/pattern checks below. */
                memset(&testMi, 0, sizeof(testMi));
                testMi.Depth        = 8;
                testMi.Flags        = GMF_HPOLARITY | GMF_VPOLARITY;
                testMi.Width        = 640;
                testMi.Height       = 480;
                testMi.HorTotal     = 800;
                testMi.HorBlankSize = 8;
                testMi.HorSyncStart = 16;
                testMi.HorSyncSize  = 96;
                testMi.VerTotal     = 525;
                testMi.VerBlankSize = 8;
                testMi.VerSyncStart = 10;
                testMi.VerSyncSize  = 2;
                testMi.PixelClock   = 25175000;
                bi->ModeInfo        = &testMi;
                bi->ResolvePixelClock(bi, &testMi, testMi.PixelClock, RGBFB_CLUT);
                if (bi->SetClock) {
                    bi->SetClock(bi);
                }
                bi->SetGC(bi, &testMi, TRUE);
                bi->SetDAC(bi, 0, RGBFB_CLUT);
                bi->SetPanning(bi, bi->MemoryBase, 640, 480, 0, 0, RGBFB_CLUT);
                bi->SetDisplay(bi, TRUE);
            }

            bi->SetSprite(bi, FALSE, RGBFB_CLUT);
            for (int c = 0; c < 256; c++) {
                bi->CLUT[c].Red = bi->CLUT[c].Green = bi->CLUT[c].Blue = (UBYTE)c;
            }
            bi->SetColorArray(bi, 0, 256);

            {
                struct RenderInfo ri;
                ri.Memory      = bi->MemoryBase;
                ri.BytesPerRow = 640;
                ri.RGBFormat   = RGBFB_CLUT;
                testFillPattern8bppBytes(bi, 640, 480);
                FillRect(bi, &ri, 100, 100, 440, 280, 0xFF, 0xFF, RGBFB_CLUT);

                UWORD patternData[] = {0xAAAA, 0x5555, 0x3333, 0xCCCC};
                struct Pattern pattern;
                pattern.BgPen    = 127;
                pattern.FgPen    = 255;
                pattern.DrawMode = JAM2;
                pattern.Size     = 2;
                pattern.Memory   = patternData;
                pattern.XOffset  = 0;
                pattern.YOffset  = 0;
                BlitPattern(bi, &ri, &pattern, 150, 150, 340, 180, 0xFF, RGBFB_CLUT);
                WaitBlitter(bi);
            }

            if (vblankTest)
                testVBlankInterrupt(bi, board);

            rval = EXIT_SUCCESS;
            goto exit;
        }
    }

    D(ERROR, "no Mach64 found.\n");

exit:
    if (rdargs) {
        FreeArgs(rdargs);
    }
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
        OpenPciBase = NULL;
    }

    return rval;
}
#endif  // TESTEXE

#ifdef __cplusplus
}
#endif
