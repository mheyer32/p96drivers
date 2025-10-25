#include "mach64_common.h"

#include <clib/debug_protos.h>
#include <exec/nodes.h>
#include <exec/types.h>
#include <proto/exec.h>

#define OPENPCI_SWAP  // don't make it define its own SWAP macros
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/openpci.h>
#include <proto/timer.h>

#ifndef TESTEXE
const char LibName[]     = "ATIMach64.card";
const char LibIdString[] = "ATIMach64 Picasso96 card driver version 1.0";
const UWORD LibVersion   = 1;
const UWORD LibRevision  = 0;
#endif
#include <proto/picasso96_chip.h>

#ifdef DBG
int debugLevel = VERBOSE;
#endif

#define CHIP_NAME_MACH64 "picasso96/ATIMach64.chip"

#define VENDOR_ID_ATI 0x1002

BOOL FindCard(__REGA0(struct BoardInfo *bi))
{
    LOCAL_SYSBASE();
    CardData_t *cd = getCardData(bi);

    struct Library *OpenPciBase = NULL;
    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        DFUNC(ERROR, "Cannot open openpci.library v%ld+\n", MIN_OPENPCI_VERSION);
        goto exit;
    }

    struct pci_dev *board = NULL;
    while (board = FindBoard(board, PRM_Vendor, VENDOR_ID_ATI, TAG_END)) {
        D(INFO, "ATI board found\n");

        ULONG deviceId, revision;

        ULONG count = GetBoardAttrs(board, PRM_Device, (Tag)&deviceId, PRM_Revision, (Tag)&revision, TAG_END);
        if (count < 2) {
            DFUNC(ERROR, "Could not retrieve all required board attributes\n");
            continue;
        }
        D(INFO, "ATI device %lx revision %lx\n", deviceId, revision);
        ChipFamily_t chipFamily = getChipFamily(deviceId);
        if (chipFamily == UNKNOWN) {
            D(WARN, "Unknown Chip family\n");
            continue;
        }
        D(INFO, "%s found\n", getChipFamilyName(chipFamily));

        cd->boardNode.ln_Name = "ATIMach64.card";
        if (!SetBoardAttrs(board, PRM_BoardOwner, (Tag)&cd->boardNode, TAG_END)) {
            struct Node *owner = NULL;
            GetBoardAttrs(board, PRM_BoardOwner, (Tag)&owner, TAG_END);
            D(INFO, "Card already claimed by: %s\n", (owner && owner->ln_Name) ? owner->ln_Name : "Unknown");
            continue;
        }

        cd->board       = board;
        cd->OpenPciBase = OpenPciBase;

        bi->BoardType              = BT_Mach64;
        bi->GraphicsControllerType = GCT_ATIRV100;
        bi->PaletteChipType        = PCT_ATT_20C492;
        bi->BoardName              = "ATIMach64";

        // success!
        break;
    }

exit:

    if (!cd->board) {
        CloseLibrary(OpenPciBase);
        cd->OpenPciBase = NULL;
        return FALSE;
    }

    return TRUE;
}

BOOL InitCard(__REGA0(struct BoardInfo *bi), __REGA1(char **ToolTypes))
{
    CardData_t *cd = getCardData(bi);
    if (!cd->board || !cd->OpenPciBase) {
        DFUNC(ERROR, "No board claimed\n");
        return FALSE;
    }

    LOCAL_OPENPCIBASE();
    LOCAL_SYSBASE();

    ULONG deviceId = 0, revision = 0, memory0Size = 0, memory1Size = 0, memory2Size = 0;
    APTR memory0 = 0, memory1 = 0, memory2 = 0;

    ULONG count = GetBoardAttrs(cd->board, PRM_Device, (Tag)&deviceId, PRM_Revision, (Tag)&revision, PRM_MemoryAddr0,
                                (Tag)&memory0, PRM_MemorySize0, (Tag)&memory0Size, PRM_MemoryAddr1, (Tag)&memory1,
                                PRM_MemorySize1, (Tag)&memory1Size, PRM_MemoryAddr2, (Tag)&memory2, PRM_MemorySize2,
                                (Tag)&memory2Size, TAG_END);
    if (count < 8) {
        DFUNC(ERROR, "Could not retrieve all required board attributes\n");
        return FALSE;
    }

    ChipFamily_t chipFamily = getChipFamily(deviceId);

    if (chipFamily == UNKNOWN) {
        DFUNC(ERROR, "Unknown Chip family\n");
        return FALSE;
    }
    D(INFO, "Initializing %s\n", getChipFamilyName(chipFamily));
    D(INFO, "BAR0 0x%lx, BAR0Size %ld\n", memory0, memory0Size);
    D(INFO, "BAR1 0x%lx, BAR1Size %ld\n", memory1, memory1Size);
    D(INFO, "BAR2 0x%lx, BAR2Size %ld\n", memory2, memory2Size);

    APTR physicalAddress = pci_logic_to_physic_addr(memory0, cd->board);
    D(INFO, "physicalAddress 0x%08lx\n", physicalAddress);

    struct ChipBase *ChipBase = NULL;

    if (!(ChipBase = (struct ChipBase *)OpenLibrary(CHIP_NAME_MACH64, 0))) {
        D(ERROR, "ATIMach64.card: could not open chip library %s\n", CHIP_NAME_MACH64);
        return FALSE;
    }

    bi->ChipBase = ChipBase;

    // Set up register and memory bases
    if (!memory1) {
        DFUNC(ERROR, "Cannot find block IO aperture\n");
        return FALSE;
    }

    // Block IO is in BAR1
    bi->RegisterBase = (UBYTE *)memory1 + REGISTER_OFFSET;

    if (memory2) {
        // Use Auxiliary Aperture for MMIO if available
        D(INFO, "Using auxiliary register aperture at 0x%08lx\n", memory2);
        bi->MemoryIOBase = (UBYTE *)memory2 + 1024 + MMIOREGISTER_OFFSET;
        D(INFO, "MMIO base set to: 0x%08lx\n", bi->MemoryIOBase);
        setCacheMode(bi, memory2, memory2Size, MAPP_IO | MAPP_CACHEINHIBIT, CACHEFLAGS);
    } else {
        // MMIO registers are in top 1kb of the first 8mb memory window
        bi->MemoryIOBase = (UBYTE *)memory0 + 0x800000 - 1024 + MMIOREGISTER_OFFSET;
        setCacheMode(bi, (UBYTE *)memory0 + 0x800000 - 1024, 1024, MAPP_IO | MAPP_CACHEINHIBIT, CACHEFLAGS);
    }

    // Framebuffer is at BAR0
    bi->MemoryBase = (UBYTE *)memory0;

    D(INFO, "ATIMach64 calling init chip...\n");
    if (!InitChip(bi)) {
        DFUNC(ERROR, "InitChip() failed\n");
        return FALSE;
    }
    D(INFO, "ATIMach64 has %ldkb usable memory\n", bi->MemorySize / 1024);

    // enable special cache mode settings(?)
    bi->MemorySpaceBase = memory0;
    bi->MemorySpaceSize = memory0Size;
    bi->Flags |= BIF_CACHEMODECHANGE;

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef TESTEXE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct UtilityBase *UtilityBase;

static struct BoardInfo boardInfo = {0};

BOOL releaseCard(__REGA0(struct BoardInfo *bi))
{
    CardData_t *cd = getCardData(bi);

    if (cd->OpenPciBase) {
        LOCAL_OPENPCIBASE();
        if (cd->board) {
            // release ownership
            SetBoardAttrs(cd->board, PRM_BoardOwner, (Tag)NULL, TAG_END);
        }

        LOCAL_SYSBASE();
        CloseLibrary(cd->OpenPciBase);
        cd->OpenPciBase = NULL;
    }
}

void sigIntHandler(int dummy)
{
    releaseCard(&boardInfo);
    abort();
}

int main()
{
    signal(SIGINT, sigIntHandler);

    int rval = EXIT_FAILURE;

    memset(&boardInfo, 0, sizeof(boardInfo));

    struct BoardInfo *bi = &boardInfo;

    bi->ExecBase = SysBase;
    bi->UtilBase = UtilityBase;

    if (!FindCard(bi)) {
        goto exit;
    }

    if (!InitCard(bi, NULL)) {
        goto exit;
    }

    rval = EXIT_SUCCESS;
exit:
    releaseCard(bi);
    return rval;
}
#endif  // TESTEXE
