#include "chip_et4000.h"
#include "ch8398ramdac.h"

#define __NOLIBBASE__

#include <clib/debug_protos.h>
#include <debuglib.h>
#include <exec/types.h>
#include <graphics/rastport.h>

#define OPENPCI_SWAP  // don't make it define its own SWAP macros
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/exec.h>
#include <proto/openpci.h>

#include <SDI_stdarg.h>

#ifdef DBG
int debugLevel = VERBOSE;
#endif

/******************************************************************************/
/*                                                                            */
/* library exports                                                                    */
/*                                                                            */
/******************************************************************************/

const char LibName[]     = "ET4000.chip";
const char LibIdString[] = "Tseng ET4000/W32p Picasso96 chip driver version 1.0";

const UWORD LibVersion  = 1;
const UWORD LibRevision = 0;

static void ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    REGBASE();
    DFUNC(VERBOSE, " state %ld\n", (ULONG)state);

    // ET4000 uses standard VGA sequencer register SR1 (Clocking Mode)
    // Bit 5 (0x20) controls screen on/off
    W_SR_MASK(0x01, 0x20, (~(UBYTE)state & 1) << 5);
}

static ULONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi),
                                   __REGD0(ULONG pixelClock), __REGD7(RGBFTYPE RGBFormat))
{
    DFUNC(VERBOSE, "ModeInfo 0x%lx pixelclock %ld, format %ld\n", mi, pixelClock, (ULONG)RGBFormat);

    // ET4000/W32p supports pixel clocks up to 86MHz
    // For now, return the requested clock (actual PLL programming would go here)
    mi->PixelClock = pixelClock;

    // Set basic flags
    mi->Flags &= ~GMF_ALWAYSBORDER;
    if (0x3ff < mi->VerTotal) {
        mi->Flags |= GMF_ALWAYSBORDER;
    }

    mi->Flags &= ~GMF_DOUBLESCAN;
    if (mi->Height < 400) {
        mi->Flags |= GMF_DOUBLESCAN;
    }

    return 0;  // Return index (simplified for now)
}

static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD7(RGBFTYPE format))
{
    DFUNC(INFO, "Index: %ld\n", index);
    // Simplified implementation - return stored pixel clock
    return mi->PixelClock;
}

static ULONG ASM SetMemoryClock(__REGA0(struct BoardInfo *bi), __REGD0(ULONG clockHz))
{
    DFUNC(INFO, "clockHz %ld\n", clockHz);

    ChipData_t *cd = getChipData(bi);

    // If CH8398 RAMDAC is available, use it for memory clock
    if (cd->hasCH8398) {
        return SetMemoryClockCH8398(bi, clockHz);
    }

    // Fallback: return requested clock (no PLL programming available)
    DFUNC(WARN, "SetMemoryClock: CH8398 not available, returning requested clock\n");
    return clockHz;
}

static void ASM SetClock(__REGA0(struct BoardInfo *bi))
{
    DFUNC(INFO, "\n");

    struct ModeInfo *mi = bi->ModeInfo;
    D(INFO, "SetClock: PixelClock %ldHz\n", mi->PixelClock);

    REGBASE();

    // ET4000/W32p clock programming would go here
    // This is a placeholder - actual implementation would program the PLL
    // based on the pixel clock value
}

static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    DFUNC(VERBOSE, "format %ld\n", (ULONG)format);
    REGBASE();

    // ET4000 supports both planar and linear modes
    // Memory mode setup would go here
    getChipData(bi)->MemFormat = format;
}

static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    REGBASE();
    // ET4000 uses GDC Bit Mask register (GR8)
    W_GR(0x08, mask);
}

static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask)) {}

static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    REGBASE();
    // ET4000 uses GDC Read Plane Select register (GR4)
    W_GR(0x04, mask);
}

static BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL expected))
{
    REGBASE();
    // ET4000 uses Input Status Register One (3DA/3BA)
    // Bit 3 indicates vertical retrace
    return (R_REG(0x3DA) & 0x08) != 0;
}

static void WaitVerticalSync(__REGA0(struct BoardInfo *bi))
{
    REGBASE();
    // Wait for vertical retrace
    while (!(R_REG(0x3DA) & 0x08)) {
        // Wait for bit 3 to be set
    }
    while (R_REG(0x3DA) & 0x08) {
        // Wait for bit 3 to clear
    }
}

static void ASM SetDPMSLevel(__REGA0(struct BoardInfo *bi), __REGD0(ULONG level))
{
    // DPMS_ON,      /* Full operation                             */
    // DPMS_STANDBY, /* Optional state of minimal power reduction  */
    // DPMS_SUSPEND, /* Significant reduction of power consumption */
    // DPMS_OFF      /* Lowest level of power consumption */

    REGBASE();
    // ET4000 DPMS would be controlled via sequencer registers
    // This is a placeholder implementation
}

static UWORD CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                  __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE format))
{
    UBYTE bpp         = getBPP(format);
    UWORD bytesPerRow = width * bpp;

    // Align to 4 bytes for better performance
    bytesPerRow = (bytesPerRow + 3) & ~3;

    return bytesPerRow;
}

static ULONG CalculateMemory(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                             __REGD7(RGBFTYPE format))
{
    UWORD bytesPerRow = CalculateBytesPerRow(bi, width, height, bi->ModeInfo, format);
    return (ULONG)bytesPerRow * (ULONG)height;
}

static void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    DFUNC(VERBOSE, "startIndex %ld, count %ld\n", (ULONG)startIndex, (ULONG)count);

    REGBASE();
    LOCAL_SYSBASE();

    const UBYTE bppDiff = 2;  // 8-bit DAC, shift by 2 bits

    Disable();

    W_REG(DAC_WR_AD, startIndex);

    struct CLUTEntry *entry = &bi->CLUT[startIndex];

    for (UWORD c = 0; c < count; ++c) {
        writeReg(RegBase, DAC_DATA, entry->Red >> bppDiff);
        writeReg(RegBase, DAC_DATA, entry->Green >> bppDiff);
        writeReg(RegBase, DAC_DATA, entry->Blue >> bppDiff);
        ++entry;
    }

    Enable();
}

static void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    DFUNC(VERBOSE, "format %ld\n", (ULONG)format);
    // ET4000 uses standard VGA DAC
    // No special setup needed for most formats
}

static void ASM SetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    DFUNC(VERBOSE, "\n");
    REGBASE();

    // Set Graphics Controller mode
    UBYTE gr6 = R_GR(0x06);
    gr6 |= 0x01;  // Graphics mode

    W_GR(0x06, gr6);
}

static void ASM SetPanning(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD4(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset),
                           __REGD7(RGBFTYPE format))
{
    DFUNC(VERBOSE, "memory 0x%lx, width %ld, height %ld, xoffset %ld, yoffset %ld\n", (ULONG)memory, (ULONG)width,
          (ULONG)height, (ULONG)xoffset, (ULONG)yoffset);
    REGBASE();

    // ET4000 panning would be set via CRTC registers
    // Set starting address for display
    ULONG startAddr = (ULONG)(memory - bi->MemoryBase) / (getBPP(format));
    startAddr += (ULONG)yoffset * (ULONG)width + (ULONG)xoffset;

    // Set CRTC starting address registers (CR0C, CR0D, CR33)
    W_CR(0x0C, (startAddr >> 8) & 0xFF);
    W_CR(0x0D, startAddr & 0xFF);
    // Extended start address (CR33) for addresses > 64KB
    if (startAddr > 0xFFFF) {
        W_CR(0x33, (startAddr >> 16) & 0x0F);
    }
}

static ULONG GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    // ET4000/W32p supports CLUT, 15/16-bit, and 24/32-bit modes
    ULONG formats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC;

    // Add 24/32-bit support if available
    formats |= RGBFF_B8G8R8A8;

    return formats;
}

BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    DFUNC(ALWAYS, "\n");

    ChipData_t *cd = getChipData(bi);

    bi->GraphicsControllerType = GCT_ETW32;
    bi->PaletteChipType        = PCT_Chrontel8498;

    // ET4000 supports various little endian RGB formats
    bi->RGBFormats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;

    REGBASE();

    // Chip wakeup via 0x3C3
    W_REG(0x3C3, BIT(0));
    // Chip wakeup via 0x46E8(?)
    W_REG(0x46E8, BIT(3));

    // Enable IO and Memory access + Color Emulation
    W_MISC_MASK(0x03, 0x03);

    // Set "KEY" To unlock CRTC registers > 0x18
    W_REG(0x3BF, 0x03);
    W_REG(0x3D8, BIT(7)|BIT(5));

    {
        UBYTE revision = R_CRB(0xEC) >> 4;
        switch (revision) {
        case 0b0010:
            revision = 0xA;
            break;
        case 0b0101:
            revision = 0xB;
            break;
        case 0b0111:
            revision = 0xC;
            break;
        case 0b0110:
            revision = 0xD;
            break;
        default:
            revision = 0xFF;
            break;
        }
        DFUNC(INFO, "ET4000/W32p Revision %lX detected\n", (ULONG)revision);
    }

    // Check meaning of the MUX pins
    R_CRB(0xF7); // IMA Indexed Register F7: Image Port Control
    R_CRB(0xEF); // CRTCB/Sprite Control, Bits 7:3 contain FCG 4:3 strap bits


    // Enable "System linear" aperture and MMIO registers
    W_CR_MASK(0x36, BIT(4)|BIT(5), BIT(4)|BIT(5));

    {
        LOCAL_OPENPCIBASE();
        ULONG physicalAddress = (ULONG)pci_logic_to_physic_addr(bi->MemoryBase, getCardData(bi)->board);
        D(INFO, "physicalAddress 0x%08lx\n", physicalAddress);

        // CRTC Idexed Register 30: Address Map Comparator
        R_CR(0x30);
        W_CR(0x30, (UBYTE)(physicalAddress >> 22));
    }


    // Check for CH8398 RAMDAC
    cd->hasCH8398 = FALSE;
    if (CheckForCH8398(bi)) {
        InitCH8398(bi);
        cd->hasCH8398 = TRUE;
        D(INFO, "CH8398 RAMDAC detected and initialized\n");
    } else {
        D(INFO, "CH8398 RAMDAC not detected, using standard VGA DAC\n");
    }

    if (bi->MemoryClock < 40000000)
        bi->MemoryClock = 40000000;
    else if (bi->MemoryClock > 65000000)
        bi->MemoryClock = 65000000;

    SetMemoryClock(bi, bi->MemoryClock);

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

    // VSYNC
    bi->WaitVerticalSync = WaitVerticalSync;
    bi->GetVSyncState    = GetVSyncState;

    // DPMS
    bi->SetDPMSLevel = SetDPMSLevel;

    return TRUE;
}

#ifdef TESTEXE

#include <boardinfo.h>
#include <libraries/openpci.h>
#include <proto/dos.h>
#include <proto/expansion.h>
#include <proto/openpci.h>
#include <proto/timer.h>
#include <proto/utility.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *OpenPciBase = NULL;
struct IORequest ioRequest;
struct Device *TimerBase = NULL;
struct UtilityBase *UtilityBase;

void sigIntHandler(int dummy)
{
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
    }
    abort();
}

int main()
{
    signal(SIGINT, sigIntHandler);

    int rval = EXIT_FAILURE;

    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        D(0, "Unable to open openpci.library\n");
    }

    struct pci_dev *board = NULL;

    D(0, "Looking for ET4000 card\n");

    while ((board = FindBoard(board, PRM_Vendor, VENDOR_ID_TSENG, TAG_END)) != NULL) {
        struct BoardInfo boardInfo;
        memset(&boardInfo, 0, sizeof(boardInfo));
        struct BoardInfo *bi = &boardInfo;

        CardData_t *card  = getCardData(bi);
        bi->ExecBase      = SysBase;
        bi->UtilBase      = UtilityBase;
        bi->ChipBase      = NULL;
        card->OpenPciBase = OpenPciBase;
        card->board       = board;
        bi->MemoryClock   = 54000000;

        if (!initRegisterAndMemoryBases(bi)) {
            continue;
        }

        D(ALWAYS, "ET4000: %s found\n", getChipFamilyName(getChipData(bi)->chipFamily));

        // Write PCI COMMAND register to enable IO and Memory access
        //            pci_write_config_word(0x04, 0x0003, board);

        struct ChipBase *ChipBase = NULL;

        D(ALWAYS, "ET4000 init chip\n");
        if (!InitChip(bi)) {
            D(ERROR, "InitChip failed. Exit");
            goto exit;
        }
        D(ALWAYS, "ET4000 has %ldkb usable memory\n", bi->MemorySize / 1024);

        {
            DFUNC(ALWAYS, "SetDisplay OFF\n");
            SetDisplay(bi, FALSE);
        }

        // test 640x480 screen
        struct ModeInfo mi;

        {
            mi.Depth            = 8;
            mi.Flags            = GMF_HPOLARITY | GMF_VPOLARITY;
            mi.Height           = 480;
            mi.Width            = 640;
            mi.HorBlankSize     = 0;
            mi.HorEnableSkew    = 0;
            mi.HorSyncSize      = 96;
            mi.HorSyncStart     = 16;
            mi.HorTotal         = 800;
            mi.PixelClock       = 25175000;
            mi.pll1.Numerator   = 0;
            mi.pll2.Denominator = 0;
            mi.VerBlankSize     = 0;
            mi.VerSyncSize      = 2;
            mi.VerSyncStart     = 10;
            mi.VerTotal         = 525;

            bi->ModeInfo = &mi;

            // ResolvePixelClock(bi, &mi, 67000000, RGBFB_CLUT);

            ULONG index = ResolvePixelClock(bi, &mi, mi.PixelClock, RGBFB_CLUT);

            DFUNC(ALWAYS, "SetClock\n");

            SetClock(bi);

            DFUNC(ALWAYS, "SetGC\n");

            SetGC(bi, &mi, TRUE);
        }
        {
            DFUNC(ALWAYS, "SetDAC\n");
            SetDAC(bi, RGBFB_CLUT);
        }
        {
            // SetSprite(bi, FALSE, RGBFB_CLUT);
            // Note: ET4000 sprite support would need to be implemented
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
            DFUNC(ALWAYS, "SetDisplay ON\n");
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

        // Test FillRect if implemented
        // Note: ET4000 accelerator support would need to be implemented
        // {
        //     FillRect(bi, &ri, 100, 100, 640 - 200, 480 - 200, 0xFF, 0xFF, RGBFB_CLUT);
        // }

        // {
        //     FillRect(bi, &ri, 64, 64, 128, 128, 0xAA, 0xFF, RGBFB_CLUT);
        // }

        // {
        //     FillRect(bi, &ri, 256, 10, 128, 128, 0x33, 0xFF, RGBFB_CLUT);
        // }

#if 0
        // Pattern blit tests - would need ET4000 accelerator implementation
        for (int i = 0; i < 8; ++i) {
            {
                UWORD patternData[] = {0x0101, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
                struct Pattern pattern;
                pattern.BgPen    = 127;
                pattern.FgPen    = 255;
                pattern.DrawMode = JAM2;
                pattern.Size     = 2;
                pattern.Memory   = patternData;
                pattern.XOffset  = i;
                pattern.YOffset  = i;

                BlitPattern(bi, &ri, &pattern, 100 + i * 32, 150 + i * 32, 24, 24, 0xFF, RGBFB_CLUT);
            }
        }
#endif
        // WaitBlitter(bi);
        // Note: ET4000 accelerator wait would need to be implemented
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
    }  // while

    D(ERROR, "no ET4000 found.\n");

exit:
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
    }

    return rval;
}
#endif  // TESTEXE
