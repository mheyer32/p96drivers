#include "chip_mach32.h"

#define __NOLIBBASE__

#include <exec/types.h>
#include <graphics/rastport.h>
#include <libraries/pcitags.h>
#include <proto/exec.h>

#if OPENPCI
#define OPENPCI_SWAP
#include <libraries/openpci.h>
#include <proto/openpci.h>
#endif

#include <string.h>

/******************************************************************************/

const char LibName[]     = "ATIMach32.chip";
const char LibIdString[] = "ATI Mach32 Picasso96 chip driver version 0.1 (skeleton)";

#ifndef LIB_VERSION
#define LIB_VERSION 1
#endif
#ifndef LIB_REVISION
#define LIB_REVISION 0
#endif
const UWORD LibVersion  = LIB_VERSION;
const UWORD LibRevision = LIB_REVISION;

/******************************************************************************/

#ifdef DBG
int debugLevel = TELLALL;
#endif


static void INLINE waitFifo(const BoardInfo_t *bi, UBYTE slots)
{
    (void)slots;
    /* TODO: poll GE_STAT / EXT_FIFO_STATUS per REG688000-15 §7-16 */
    (void)bi;
}

static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    (void)bi;
    (void)mask;
}

static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    (void)bi;
    (void)mask;
}

static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    (void)bi;
    (void)mask;
}

static BOOL ASM SetSprite(__REGA0(struct BoardInfo *bi), __REGD0(BOOL show), __REGD7(RGBFTYPE fmt))
{
    (void)bi;
    (void)show;
    (void)fmt;
    return TRUE;
}

static void ASM SetSpritePosition(__REGA0(struct BoardInfo *bi), __REGD0(WORD xpos), __REGD1(WORD ypos),
                                  __REGD7(RGBFTYPE fmt))
{
    (void)bi;
    (void)xpos;
    (void)ypos;
    (void)fmt;
}

static void ASM SetSpriteImage(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE fmt))
{
    (void)bi;
    (void)fmt;
}

static void ASM SetSpriteColor(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE idx), __REGD1(UBYTE r),
                               __REGD2(UBYTE g), __REGD3(UBYTE b), __REGD7(RGBFTYPE fmt))
{
    (void)bi;
    (void)idx;
    (void)r;
    (void)g;
    (void)b;
    (void)fmt;
}

void ASM SetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    (void)border;
    bi->ModeInfo = mi;
    /* TODO: program Mach32 extended CRTC (REG688000-15) */
}

void ASM SetPanning(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD4(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset),
                           __REGD7(RGBFTYPE format))
{
    (void)memory;
    (void)width;
    (void)height;
    (void)format;
    bi->XOffset = xoffset;
    bi->YOffset = yoffset;
}

UWORD ASM CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                      __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE format))
{
    (void)bi;
    (void)height;
    (void)mi;
    UBYTE bpp = 1;
    switch (format) {
    case RGBFB_CLUT:
        bpp = 1;
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_B5G6R5PC:
    case RGBFB_B5G5R5PC:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        bpp = 2;
        break;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        bpp = 3;
        break;
    default:
        bpp = 4;
        break;
    }
    UWORD bpr = width * bpp;
    return (bpr + 63) & (UWORD)~63;
}

APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *mem), __REGD0(struct RenderInfo *ri),
                                __REGD7(RGBFTYPE format))
{
    (void)bi;
    (void)ri;
    (void)format;
    return (APTR)(((ULONG)mem + 63) & ~63UL);
}

ULONG ASM GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    (void)bi;
    if (format == RGBFB_NONE)
        return 0;
    return (ULONG)(RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC);
}

void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD0(UWORD region), __REGD7(RGBFTYPE format))
{
    (void)region;
    const RamdacOps_t *ops = getConstChipData(bi)->ramdacOps;
    if (ops && ops->setDac)
        ops->setDac(bi, format);
}

void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    (void)startIndex;
    (void)count;
    /* TODO: 8514/A DAC ports via I/O (RegisterBase, §8) */
}

BOOL ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    (void)bi;
    (void)state;
    return TRUE;
}

void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    (void)bi;
    (void)format;
}

LONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi),
                                  __REGD0(ULONG pixelClock), __REGD7(RGBFTYPE RGBFormat))
{
    (void)bi;
    (void)RGBFormat;
    mi->PixelClock = pixelClock;
    return 0;
}

ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD7(RGBFTYPE format))
{
    (void)mi;
    (void)index;
    (void)format;
    if (bi->PixelClockCount[CHUNKY] == 0)
        return 25175000;
    return 25175000;
}

void ASM SetClock(__REGA0(struct BoardInfo *bi))
{
    const RamdacOps_t *ops = getConstChipData(bi)->ramdacOps;
    if (ops && ops->setClock)
        ops->setClock(bi);
}

BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL expected))
{
    (void)expected;
    (void)bi;
    return FALSE;
}

void ASM WaitVerticalSync(__REGA0(struct BoardInfo *bi))
{
    (void)bi;
}

static ULONG ASM GetVBeamPos(__REGA0(struct BoardInfo *bi))
{
    REGBASE();
    return R_IO_W(VERT_LINE_CNTR) & 0x7FF;
}


void ASM WaitBlitter(__REGA0(struct BoardInfo *bi))
{
    REGBASE();
    waitFifo(bi, 16);
    while (TST_IO_W(EXT_GE_STATUS, BIT(13))) {
        /* wait for GE idle */
    }
}



BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    DFUNC(ALWAYS, "\n");

    REGBASE();

    bi->GraphicsControllerType = GCT_ATIRV100;
    bi->PaletteChipType        = PCT_Unknown;
    bi->Flags |= BIF_GRANTDIRECTACCESS | BIF_HARDWARESPRITE;
    bi->RGBFormats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC;

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

    bi->WaitVerticalSync = WaitVerticalSync;
    bi->GetVSyncState    = GetVSyncState;

    bi->SetSprite         = SetSprite;
    bi->SetSpritePosition = SetSpritePosition;
    bi->SetSpriteImage    = SetSpriteImage;
    bi->SetSpriteColor    = SetSpriteColor;

    bi->WaitBlitter            = WaitBlitter;
    // bi->BlitRect               = bi->BlitRectDefault;
    // bi->InvertRect             = bi->InvertRectDefault;
    // bi->FillRect               = bi->FillRectDefault;
    // bi->BlitTemplate           = bi->BlitTemplateDefault;
    // bi->BlitPlanar2Chunky      = bi->BlitPlanar2ChunkyDefault;
    // bi->BlitRectNoMaskComplete = bi->BlitRectNoMaskCompleteDefault;
    // bi->DrawLine               = bi->DrawLineDefault;
    // bi->BlitPattern            = bi->BlitPatternDefault;

    bi->MaxBMWidth  = 2048;
    bi->MaxBMHeight = 2048;

    bi->BitsPerCannon          = 8;
    bi->MaxHorValue[PLANAR]    = 2040;
    bi->MaxHorValue[CHUNKY]    = 2040;
    bi->MaxHorValue[HICOLOR]   = 2040;
    bi->MaxHorValue[TRUECOLOR] = 2040;
    bi->MaxHorValue[TRUEALPHA] = 2040;

    bi->MaxVerValue[PLANAR]    = 2047;
    bi->MaxVerValue[CHUNKY]    = 2047;
    bi->MaxVerValue[HICOLOR]   = 2047;
    bi->MaxVerValue[TRUECOLOR] = 2047;
    bi->MaxVerValue[TRUEALPHA] = 2047;

    bi->MaxHorResolution[PLANAR]    = 1280;
    bi->MaxVerResolution[PLANAR]    = 1024;
    bi->MaxHorResolution[CHUNKY]    = 1280;
    bi->MaxVerResolution[CHUNKY]    = 1024;
    bi->MaxHorResolution[HICOLOR]   = 1280;
    bi->MaxVerResolution[HICOLOR]   = 1024;
    bi->MaxHorResolution[TRUECOLOR] = 1280;
    bi->MaxVerResolution[TRUECOLOR] = 1024;
    bi->MaxHorResolution[TRUEALPHA] = 1280;
    bi->MaxVerResolution[TRUEALPHA] = 1024;

    bi->PixelClockCount[CHUNKY]   = 1;
    bi->PixelClockCount[HICOLOR]  = 1;
    bi->PixelClockCount[TRUECOLOR] = 1;
    bi->PixelClockCount[TRUEALPHA] = 1;


    W_IO_W(SCRATCH_PAD0, 0xAAAA);
    UWORD scratch0 = R_IO_W(SCRATCH_PAD0);
    W_IO_W(SCRATCH_PAD0, 0x5555);
    UWORD scratch1 = R_IO_W(SCRATCH_PAD0);
    if (scratch0 != 0xAAAA || scratch1 != 0x5555) {
        DFUNC(ERROR, "Scratch pad test failed: read 0x%04X and 0x%04X\n", scratch0, scratch1);
        return FALSE;
    }

    UWORD chipId = R_IO_W(CHIP_ID);
    char chipName[3] = { 0 };
    chipName[1] =  (chipId & 0x1F) + 0x41;
    chipName[0] = (chipId >> 5) & 0x1F + 0x41;
    ULONG class = (chipId >> 10) & 3;
    ULONG revision = (chipId >> 12) & 0xF;

    DFUNC(ALWAYS, "Chip Version detected: Mach32 %s, revision %ld, class 0x%lx\n", chipName, revision, class);

    UWORD configStat0 = R_IO_W(CONFIG_STATUS_1);
    ULONG vgaEnabled = !(configStat0 & BIT(0));
    ULONG busType = (configStat0 >> 1) & 7;
    ULONG memType = (configStat0 >> 4) & 7;
    ULONG chipEnabled = !(configStat0 & BIT(7));
    ULONG dacType = (configStat0 >> 9) & 0x7;

    DFUNC(ALWAYS, "CONFIG_STATUS_1: VGA %s, bus type 0x%lx, mem type 0x%lx, chip %s, DAC type 0x%lx\n",
          vgaEnabled ? "enabled" : "disabled", busType, memType, chipEnabled ? "enabled" : "disabled", dacType);

    W_IO_MASK_W(CONFIG_STATUS_1, BIT(0), BIT(0)); // Disable VGA core, "8514 ONLY "
    W_IO_MASK_W(CLOCK_SEL, BIT(0), BIT(0));
    InitRAMDAC(bi, dacType);

    return TRUE;
}

#ifdef TESTEXE

#include <libraries/openpci.h>
#include <proto/openpci.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct UtilityBase *UtilityBase;

#define PCI_VENDOR_ATI   0x1002
#define PCI_DEVICE_MACH32TEST 0x4158

struct Library *OpenPciBase = NULL;

static void onSigInt(int dummy)
{
    (void)dummy;
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
        OpenPciBase = NULL;
    }
    abort();
}

int main(void)
{
    signal(SIGINT, onSigInt);

    int rval = EXIT_FAILURE;

    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        D(0, "TestMach32: cannot open openpci.library v%ld+\n", MIN_OPENPCI_VERSION);
        return EXIT_FAILURE;
    }

    struct pci_dev *board = NULL;
    struct TagItem findTags[] = { { PRM_Vendor, PCI_VENDOR_ATI }, { PRM_Device, PCI_DEVICE_MACH32TEST }, { TAG_END, 0 } };

    while ((board = FindBoardA(board, findTags)) != NULL) {
        ULONG Device = 0, Revision = 0, Memory0Size = 0;
        APTR Memory0 = NULL, legacyIOBase = NULL;

        GetBoardAttrs(board, PRM_Device, (Tag)&Device, PRM_Revision, (Tag)&Revision, PRM_MemoryAddr0, (Tag)&Memory0,
                      PRM_MemorySize0, (Tag)&Memory0Size, PRM_LegacyIOSpace, (Tag)&legacyIOBase, TAG_END);

        if (Device != PCI_DEVICE_MACH32TEST) {
            continue;
        }

        D(0, "TestMach32: Mach32 PCI device rev %lu, BAR0 %p size %lu\n", Revision, Memory0, Memory0Size);

        pci_write_config_word(PCI_COMMAND,
                              (UWORD)(pci_read_config_word(PCI_COMMAND, board) | PCI_COMMAND_MEMORY | PCI_COMMAND_IO),
                              board);

        struct BoardInfo boardInfo;
        memset(&boardInfo, 0, sizeof(boardInfo));
        struct BoardInfo *bi = &boardInfo;

        bi->ExecBase                 = SysBase;
        bi->UtilBase                 = UtilityBase;
        getCardData(bi)->OpenPciBase = OpenPciBase;
        getCardData(bi)->board       = board;
        getCardData(bi)->legacyIOBase = (volatile UBYTE *)legacyIOBase + REGISTER_OFFSET;
        bi->RegisterBase             = (UBYTE *)legacyIOBase + REGISTER_OFFSET;
        bi->MemoryBase               = (UBYTE *)Memory0;
        bi->MemorySize               = Memory0Size;

        if (Memory0Size > 0) {
            setCacheMode(bi, (UBYTE *)Memory0, Memory0Size, MAPP_CACHEINHIBIT | MAPP_IMPRECISE | MAPP_NONSERIALIZED,
                         CACHEFLAGS);
        }

        if (!InitChip(bi)) {
            D(0, "TestMach32: InitChip failed\n");
            goto done;
        }

        D(0, "TestMach32: usable VRAM %luk — writing gradient to first scanlines\n", bi->MemorySize / 1024UL);

        ULONG maxOff = bi->MemorySize;
        if (maxOff > 640UL * 64UL) {
            maxOff = 640UL * 64UL;
        }
        for (ULONG y = 0; y < maxOff / 640UL; y++) {
            for (UWORD x = 0; x < 640; x++) {
                *(volatile UBYTE *)(bi->MemoryBase + y * 640UL + x) = (UBYTE)(x + y);
            }
        }

        if (bi->WaitBlitter) {
            bi->WaitBlitter(bi);
        }

        rval = EXIT_SUCCESS;
        goto done;
    }

    D(0, "TestMach32: no PCI Mach32 (0x4158) found\n");

done:
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
        OpenPciBase = NULL;
    }
    return rval;
}
#endif /* TESTEXE */
