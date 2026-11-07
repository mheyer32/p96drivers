#include "chip_et4000.h"
#include "ch8398ramdac.h"

#define __NOLIBBASE__

#include <clib/debug_protos.h>
#include <debuglib.h>
#include <exec/types.h>
#include <graphics/rastport.h>

#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/exec.h>
#include <proto/openpci.h>

#include <SDI_stdarg.h>

using namespace VgaReg;

#ifdef DBG
int debugLevel = VERBOSE;
#endif

#if !defined(TESTEXE)
extern const char LibName[]     = "ET4000.chip";
extern const char LibIdString[] = "Tseng ET4000/W32p Picasso96 chip driver version 1.0";

extern const UWORD LibVersion  = 1;
extern const UWORD LibRevision = 0;
#endif

static BOOL ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    DRIVER_LOCALS(bi);
    DFUNC(VERBOSE, " state %ld\n", (ULONG)state);
    vga.writeSRMask(0x01, 0x20, (~(UBYTE)state & 1) << 5);
    return TRUE;
}

static LONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi),
                                  __REGD0(ULONG pixelClock), __REGD7(RGBFTYPE RGBFormat))
{
    (void)bi;
    DFUNC(VERBOSE, "ModeInfo 0x%lx pixelclock %ld, format %ld\n", mi, pixelClock, (ULONG)RGBFormat);

    mi->PixelClock = pixelClock;

    mi->Flags &= ~GMF_ALWAYSBORDER;
    if (0x3ff < mi->VerTotal) {
        mi->Flags |= GMF_ALWAYSBORDER;
    }

    mi->Flags &= ~GMF_DOUBLESCAN;
    if (mi->Height < 400) {
        mi->Flags |= GMF_DOUBLESCAN;
    }

    return 0;
}

static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD7(RGBFTYPE format))
{
    (void)bi;
    (void)format;
    DFUNC(INFO, "Index: %ld\n", index);
    return mi->PixelClock;
}

static ULONG ASM SetMemoryClock(__REGA0(struct BoardInfo *bi), __REGD0(ULONG clockHz))
{
    DFUNC(INFO, "clockHz %ld\n", clockHz);

    ChipData_t *cd = getChipData(bi);

    if (cd->hasCH8398) {
        return SetMemoryClockCH8398(bi, clockHz);
    }

    DFUNC(WARN, "SetMemoryClock: CH8398 not available, returning requested clock\n");
    return clockHz;
}

static void ASM SetClock(__REGA0(struct BoardInfo *bi))
{
    DFUNC(INFO, "\n");

    struct ModeInfo *mi = bi->ModeInfo;
    D(INFO, "SetClock: PixelClock %ldHz\n", mi->PixelClock);
}

static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    DFUNC(VERBOSE, "format %ld\n", (ULONG)format);
    getChipData(bi)->MemFormat = format;
}

static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    DRIVER_LOCALS(bi);
    vga.writeGR(0x08, mask);
}

static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    (void)bi;
    (void)mask;
}

static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    DRIVER_LOCALS(bi);
    vga.writeGR(0x04, mask);
}

static BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL expected))
{
    (void)expected;
    DRIVER_LOCALS(bi);
    return (vga.readB(INPUT_STATUS1_R) & 0x08) != 0;
}

static void ASM WaitVerticalSync(__REGA0(struct BoardInfo *bi), __REGD0(BOOL waitForEnd))
{
    DRIVER_LOCALS(bi);
    (void)waitForEnd;
    while (!(vga.readB(INPUT_STATUS1_R) & 0x08)) {
    }
    while (vga.readB(INPUT_STATUS1_R) & 0x08) {
    }
}

static void ASM SetDPMSLevel(__REGA0(struct BoardInfo *bi), __REGD0(ULONG level))
{
    (void)level;
    DRIVER_LOCALS(bi);
    (void)vga;
}

static UWORD ASM CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                      __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE format))
{
    (void)bi;
    (void)height;
    (void)mi;
    UBYTE bpp         = getBPP(format);
    UWORD bytesPerRow = width * bpp;
    bytesPerRow       = (bytesPerRow + 3) & ~3;
    return bytesPerRow;
}

static APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi), __REGA1(APTR memory),
                                __REGD0(struct RenderInfo *ri), __REGD7(RGBFTYPE format))
{
    (void)bi;
    (void)ri;
    (void)format;
    return memory;
}

static void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    DFUNC(VERBOSE, "startIndex %ld, count %ld\n", (ULONG)startIndex, (ULONG)count);

    DRIVER_LOCALS(bi);
    LOCAL_SYSBASE();

    const UBYTE bppDiff = 2;

    Disable();

    vga.writeB(DAC_WR_INDEX, startIndex);

    struct CLUTEntry *entry = &bi->CLUT[startIndex];

    for (UWORD c = 0; c < count; ++c) {
        vga.writeB(DAC_PEL_DATA, entry->Red >> bppDiff);
        vga.writeB(DAC_PEL_DATA, entry->Green >> bppDiff);
        vga.writeB(DAC_PEL_DATA, entry->Blue >> bppDiff);
        ++entry;
    }

    Enable();
}

static void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD0(UWORD region), __REGD7(RGBFTYPE format))
{
    (void)bi;
    (void)region;
    DFUNC(VERBOSE, "format %ld\n", (ULONG)format);
}

static void ASM SetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    (void)mi;
    (void)border;
    DFUNC(VERBOSE, "\n");
    DRIVER_LOCALS(bi);

    UBYTE gr6 = vga.readGR(0x06);
    gr6 |= 0x01;
    vga.writeGR(0x06, gr6);
}

static void ASM SetPanning(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD3(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset),
                           __REGD7(RGBFTYPE format))
{
    (void)height;
    DFUNC(VERBOSE, "memory 0x%lx, width %ld, height %ld, xoffset %ld, yoffset %ld\n", (ULONG)memory, (ULONG)width,
          (ULONG)height, (ULONG)xoffset, (ULONG)yoffset);
    DRIVER_LOCALS(bi);

    ULONG startAddr = (ULONG)(memory - bi->MemoryBase) / (getBPP(format));
    startAddr += (ULONG)yoffset * (ULONG)width + (ULONG)xoffset;

    vga.writeCR(0x0C, (startAddr >> 8) & 0xFF);
    vga.writeCR(0x0D, startAddr & 0xFF);
    if (startAddr > 0xFFFF) {
        vga.writeCR(0x33, (startAddr >> 16) & 0x0F);
    }
}

static ULONG ASM GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    (void)bi;
    (void)format;
    ULONG formats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC;
    formats |= RGBFF_B8G8R8A8;
    return formats;
}

BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    DFUNC(ALWAYS, "\n");

    ChipData_t *cd = getChipData(bi);
    DRIVER_LOCALS(bi);

    bi->GraphicsControllerType = GCT_ETW32;
    bi->PaletteChipType        = PCT_Chrontel8498;

    bi->RGBFormats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;

    vga.writeB(VGA_ENABLE, BIT(0));
    vga.writeB(VGA_ID(0x46E8), BIT(3));

    vga.writeMiscMask(0x03, 0x03);

    vga.writeB(VGA_ID(0x3BF), 0x03);
    vga.writeB(VGA_ID(0x3D8), BIT(7) | BIT(5));

    {
        UBYTE revision = drv->readCRB(0xEC) >> 4;
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

    drv->readCRB(0xF7);
    drv->readCRB(0xEF);

    vga.writeCRMask(0x36, BIT(4) | BIT(5), BIT(4) | BIT(5));

    {
        LOCAL_OPENPCIBASE();
        ULONG physicalAddress = (ULONG)pci_logic_to_physic_addr(bi->MemoryBase, getCardData(bi)->board);
        D(INFO, "physicalAddress 0x%08lx\n", physicalAddress);

        vga.readCR(0x30);
        vga.writeCR(0x30, (UBYTE)(physicalAddress >> 22));
    }

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
    P96_HOOK(bi->SetClock, SetClock);
    P96_HOOK(bi->WaitVerticalSync, WaitVerticalSync);
    P96_HOOK(bi->GetVSyncState, GetVSyncState);
    P96_HOOK(bi->SetDPMSLevel, SetDPMSLevel);

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
    (void)dummy;
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
        bi->UtilBase      = (struct Library *)UtilityBase;
        bi->ChipBase      = NULL;
        card->OpenPciBase = OpenPciBase;
        card->board       = board;
        bi->MemoryClock   = 54000000;

        if (!initRegisterAndMemoryBases(bi)) {
            continue;
        }

        D(ALWAYS, "ET4000: %s found\n",
          getChipFamilyName((ChipFamily_t)getChipData(bi)->chipFamily));

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

            LONG index = ResolvePixelClock(bi, &mi, mi.PixelClock, RGBFB_CLUT);
            (void)index;

            DFUNC(ALWAYS, "SetClock\n");
            SetClock(bi);

            DFUNC(ALWAYS, "SetGC\n");
            SetGC(bi, &mi, TRUE);
        }
        {
            DFUNC(ALWAYS, "SetDAC\n");
            SetDAC(bi, 0, RGBFB_CLUT);
        }

        {
            DFUNC(0, "SetColorArray\n");
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

        rval = EXIT_SUCCESS;
        goto exit;
    }

    D(ERROR, "no ET4000 found.\n");

exit:
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
    }

    return rval;
}
#endif
