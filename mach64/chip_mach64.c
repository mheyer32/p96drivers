#include "chip_mach64.h"

#include <proto/prometheus.h>

#include <string.h> // memcmp

#ifdef TESTEXE
#include <stdio.h>
#define KPrintf printf
#endif

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

Mach64RomHeader_t *findRomHeader(struct BoardInfo *bi)
{
    LOCAL_PROMETHEUSBASE();

    UBYTE *romBase = NULL;
    Prm_GetBoardAttrsTags((PCIBoard *)bi->CardPrometheusDevice, PRM_ROM_Address, (ULONG)&romBase, TAG_END);
    if (!romBase)
        return NULL;

    OptionRomHeader_t *romHeader = (OptionRomHeader_t *)romBase;
    if (swapw(romHeader->signature) != 0xaa55)
        return NULL;

    PciRomData_t *pciData = (PciRomData_t *)(romBase + swapw(romHeader->pcir_offset));
    if (memcmp(pciData->signature, "PCIR", 4) != 0)
        return NULL;

    WORD atiRomHeaderOffset = swapw(*(UWORD *)(romBase + 0x48));

    Mach64RomHeader_t *mach64RomHeader = (Mach64RomHeader_t *)(romBase + atiRomHeaderOffset - 2);

    const char *logOnMessage = romBase + swapw(mach64RomHeader->logon_message_ptr);

    D(0, "ATI Mach64 ROM header found at 0x%lx\n%s\n", mach64RomHeader, logOnMessage);

    return mach64RomHeader;
}

BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    REGBASE();
    MMIOBASE();
    LOCAL_SYSBASE();

    DFUNC(0, "\n");

    bi->GraphicsControllerType = GCT_S3Trio64;
    bi->PaletteChipType = PCT_S3Trio64;
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
#if BUILD_VISION864
    bi->PixelClockCount[CHUNKY] = 135;
    bi->PixelClockCount[HICOLOR] = 80;
    bi->PixelClockCount[TRUECOLOR] = 50;
    bi->PixelClockCount[TRUEALPHA] = 50;
#else
    bi->PixelClockCount[CHUNKY] = 135;  // > 67Mhz can be achieved via Double Clock mode
    bi->PixelClockCount[HICOLOR] = 80;
    bi->PixelClockCount[TRUECOLOR] = 50;
    bi->PixelClockCount[TRUEALPHA] = 50;
#endif

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
        default:
            cd->chipFamily = UNKNOWN;
            DFUNC(0, "Unknown chip family, aborting\n");
            return FALSE;
        }
    }

    // Test scratch register response
    W_IO_L(SCRATCH_REG1, 0xAAAAAAAA);
    ULONG scratchA = R_IO_L(SCRATCH_REG1);
    W_IO_L(SCRATCH_REG1, 0x55555555);
    ULONG scratch5 = R_IO_L(SCRATCH_REG1);
    if (scratchA != 0xAAAAAAAA || scratch5 != 0x55555555) {
        DFUNC(0, "scratch register response broken.\n");
        return FALSE;
    }
    DFUNC(0, "scratch register response good.\n");

    findRomHeader(bi);

    W_IO_L(BUS_CNTL, BUS_FIFO_ERR_AK | BUS_HOST_ERR_AK);

    ULONG memSize = R_IO_L(MEM_CNTL) & 0x7;
    // W_IO_L(MEM_CNTL, )

    ULONG clock = bi->MemoryClock;
#if BUILD_VISION864
    if (clock < 40000000) {
        clock = 40000000;
    }
    if (60000000 < clock) {
        clock = 60000000;
    }
#else
    if (clock < 54000000) {
        clock = 54000000;
    }
    if (65000000 < clock) {
        clock = 65000000;
    }
#endif

    // clock = SetMemoryClock(bi, clock);
    bi->MemoryClock = clock;

    // Enable 8MB Linear Address Window (LAW)
    D(0, "MMIO base address: 0x%lx\n", (ULONG)getMMIOBase(bi));

    // Determine memory size of the card (typically 1-2MB, but can be up to 4MB)
    switch (getChipData(bi)->chipFamily) {
    case MACH64VT:
        bi->MemorySize = 0x400000;
        break;
    default:
        bi->MemorySize = 0x400000;
    }

    // volatile ULONG *framebuffer = (volatile ULONG *)bi->MemoryBase;
    // framebuffer[0] = 0;
    // while (bi->MemorySize) {
    //     D(1, "Probing memory size %ld\n", bi->MemorySize);

    //     CacheClearU();

    //     // Probe the last and the first longword for the current segment,
    //     // as well as offset 0 to check for wrap arounds
    //     volatile ULONG *highOffset = framebuffer + (bi->MemorySize >> 2) - 1;
    //     volatile ULONG *lowOffset = framebuffer + (bi->MemorySize >> 3);
    //     // Probe  memory
    //     *framebuffer = 0;
    //     *highOffset = (ULONG)highOffset;
    //     *lowOffset = (ULONG)lowOffset;

    //     CacheClearU();

    //     ULONG readbackHigh = *highOffset;
    //     ULONG readbackLow = *lowOffset;
    //     ULONG readbackZero = *framebuffer;

    //     D(10, "Probing memory at 0x%lx ?= 0x%lx; 0x%lx ?= 0x%lx, 0x0 ?= 0x%lx\n", highOffset, readbackHigh,
    //     lowOffset,
    //       readbackLow, readbackZero);

    //     if (readbackHigh == (ULONG)highOffset && readbackLow == (ULONG)lowOffset && readbackZero == 0) {
    //         break;
    //     }
    //     // reduce available memory size
    //     bi->MemorySize >>= 1;
    // }

    D(1, "MemorySize: %ldmb\n", bi->MemorySize / (1024 * 1024));

    // Input Status ? Register (STATUS_O)
    // D(1, "Monitor is %s present\n", (!(R_REG(0x3C2) & 0x10) ? "" : "NOT"));

    // Two sprite images, each 64x64*2 bits
    const ULONG maxSpriteBuffersSize = (64 * 64 * 2 / 8) * 2;

    // take sprite image data off the top of the memory
    // sprites can be placed at segment boundaries of 1kb
    bi->MemorySize = (bi->MemorySize - maxSpriteBuffersSize) & ~(1024 - 1);
    bi->MouseImageBuffer = bi->MemoryBase + bi->MemorySize;
    bi->MouseSaveBuffer = bi->MemoryBase + bi->MemorySize + maxSpriteBuffersSize / 2;

    // Start Address in terms of 1024byte segments
    // Sprite image offsets
    // Reset cursor position

    // Write conservative scissors

    // Set MULT_MISC first so that
    // "Bit 4 RSF - Select Upper Word in 32 Bits/Pixel Mode" is set to 0 and
    // Bit 9 CMR 32B - Select 32-Bit Command Registers

    return TRUE;
}

#ifdef TESTEXE

#include <boardinfo.h>
#include <libraries/prometheus.h>
#include <proto/expansion.h>
#include <proto/prometheus.h>
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
        printf("Unable to open prometheus.library\n");
        goto exit;
    }

    APTR legacyIOBase = NULL;
    if (!(legacyIOBase = findLegacyIOBase())) {
        printf("Unable to find legacy IO base\n");
        goto exit;
    }

    ULONG dmaSize = 128 * 1024;

    APTR board = NULL;

    printf("Looking for Mach64 card\n");

    while ((board = (APTR)Prm_FindBoardTags(board, PRM_Vendor, PCI_VENDOR, TAG_END)) != NULL) {
        ULONG Device, Revision, Memory0Size;
        APTR Memory0, Memory1;

        BOOL found = FALSE;

        Prm_GetBoardAttrsTags(board, PRM_Device, (ULONG)&Device, PRM_Revision, (ULONG)&Revision, PRM_MemoryAddr0,
                              (ULONG)&Memory0, PRM_MemorySize0, (ULONG)&Memory0Size, PRM_MemoryAddr1, (ULONG)&Memory1,
                              TAG_END);

        printf("device %x revision %x\n", Device, Revision);

        switch (Device) {
        case 0x5654:  // mach64 VT
            found = TRUE;
            break;
        default:
            found = FALSE;
        }

        if (found) {
            printf("ATI Mach64 found\n");

            printf("cb_LegacyIOBase 0x%x , MemoryBase 0x%x, MemorySize %u, IOBase 0x%x\n", legacyIOBase, Memory0,
                   Memory0Size, Memory1);

            APTR physicalAddress = Prm_GetPhysicalAddress(Memory0);
            printf("prometheus.card: physicalAdress 0x%08lx\n", physicalAddress);

            struct ChipBase *ChipBase = NULL;

            struct BoardInfo boardInfo;
            memset(&boardInfo, 0, sizeof(boardInfo));
            struct BoardInfo *bi = &boardInfo;

            bi->CardPrometheusBase = (ULONG)PrometheusBase;
            bi->CardPrometheusDevice = (ULONG)board;
            bi->ChipBase = ChipBase;

            // Block IO is in BAR1
            bi->RegisterBase = Memory1 + REGISTER_OFFSET;
            // MMIO registers are in top 1kb of the first 8mb memory window
            bi->MemoryIOBase = Memory0 + 0x800000 - 1024 + MMIOREGISTER_OFFSET;
            // Framebuffer is at address 0x0
            bi->MemoryBase = Memory0;

            printf("Mach64 init chip....\n");
            InitChip(bi);

            printf("Mach64 has %ukb usable memory\n", bi->MemorySize / 1024);

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

    printf("no Mach64 found.\n");

exit:
    if (PrometheusBase)
        CloseLibrary(PrometheusBase);

    return EXIT_FAILURE;
}
#endif  // TESTEXE
