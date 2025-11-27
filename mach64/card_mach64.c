#include "card_common.h"
#include "mach64_common.h"

#define __NOLIBBASE__

#include <clib/debug_protos.h>
#include <exec/nodes.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/picasso96_chip.h>
#include <proto/timer.h>
#include <proto/utility.h>
#include <utility/tagitem.h>

#define OPENPCI_SWAP  // don't make it define its own SWAP macros
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/openpci.h>

#ifndef TESTEXE
const char LibName[]     = "ATIMach64.card";
const char LibIdString[] = "ATIMach64 Picasso96 card driver version 1.0";
const UWORD LibVersion   = 1;
const UWORD LibRevision  = 0;
#endif

#ifdef DBG
int debugLevel = VERBOSE;
#endif

#define CHIP_NAME_MACH64 "picasso96/ATIMach64.chip"

#define VENDOR_ID_ATI 0x1002

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

BOOL FindCard(__REGA0(struct BoardInfo *bi), __REGA1(CONST_STRPTR *ToolTypes))
{
    LOCAL_SYSBASE();
    CardData_t *cd = getCardData(bi);

    struct Library *OpenPciBase = NULL;
    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        DFUNC(ERROR, "Cannot open openpci.library v%ld+\n", MIN_OPENPCI_VERSION);
        goto exit;
    }

    // Parse tooltypes for card-specific settings (deviceId, vendorId, slot)
    LONG deviceId = 0, vendorId = VENDOR_ID_ATI, slot = -1, bus = -1;
    if (ToolTypes) {
        parseToolTypes(bi, ToolTypes, &deviceId, &vendorId, &slot, &bus);
    }

    // First loop: Find the first board matching tooltype criteria (if any)
    // that is supported and unclaimed
    int numTags            = 0;
    struct TagItem tags[5] = {{TAG_END, 0}};
    if (vendorId) {
        D(INFO, "VENDORID: 0x%04lx\n", vendorId);
        tags[numTags].ti_Tag  = PRM_Vendor;
        tags[numTags].ti_Data = vendorId;
        numTags++;
    }
    if (deviceId) {
        D(INFO, "DEVICEID: 0x%04lx\n", deviceId);
        tags[numTags].ti_Tag  = PRM_Device;
        tags[numTags].ti_Data = deviceId;
        numTags++;
    }
    if (slot >= 0) {
        D(INFO, "SLOT: %ld\n", slot);
        tags[numTags].ti_Tag  = PRM_SlotNumber;
        tags[numTags].ti_Data = slot;
        numTags++;
    }
    if (bus >= 0) {
        D(INFO, "BUS: %ld\n", bus);
        tags[numTags].ti_Tag  = PRM_BusNumber;
        tags[numTags].ti_Data = bus;
        numTags++;
    }
    tags[numTags].ti_Tag = TAG_END;

    struct pci_dev *board = NULL;
    while (board = FindBoardA(board, tags)) {
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

        struct Node *owner = NULL;
        ULONG slot = 0, bus = 0;
        GetBoardAttrs(board, PRM_BoardOwner, (Tag)&owner, PRM_SlotNumber, (Tag)&slot, PRM_BusNumber, (Tag)&bus,
                      TAG_END);
        if (owner) {
            D(INFO, "Board already owned by: %s\n", (owner && owner->ln_Name) ? owner->ln_Name : "Unknown");
            continue;
        }

        // Claim the first matching board
        cd->boardNode.ln_Name = "ATIMach64.card";
        if (!SetBoardAttrs(board, PRM_BoardOwner, (Tag)&cd->boardNode, TAG_END)) {
            D(ERROR, "Could not claim board\n");
            continue;
        }

        cd->board       = board;
        cd->OpenPciBase = OpenPciBase;

        bi->BoardType              = BT_Mach64;
        bi->GraphicsControllerType = GCT_ATIRV100;
        bi->PaletteChipType        = PCT_ATT_20C492;

        // generate unique board name based on bus/slot
        generateBoardName(getCardData(bi)->boardName, "Mach64", bus, slot);
        bi->BoardName = getCardData(bi)->boardName;

        // Found and claimed the board, break out of first loop
        break;
    }

    // Second loop: Find all other supported and unclaimed boards to turn off their IO
    // Use only vendor filter (no device/slot/bus restrictions) to find all supported boards
    board = NULL;
    while (board = FindBoard(board, PRM_Vendor, vendorId)) {
        // Skip the board we already claimed
        if (board == cd->board) {
            continue;
        }

        ULONG deviceId, revision;
        struct Node *owner = NULL;
        ULONG count = GetBoardAttrs(board, PRM_Device, (Tag)&deviceId, PRM_Revision, (Tag)&revision, PRM_BoardOwner,
                                    (Tag)&owner, TAG_END);
        if (count < 3) {
            continue;
        }

        if (owner) {
            // Already claimed by another driver
            continue;
        }

        ChipFamily_t chipFamily = getChipFamily(deviceId);
        if (chipFamily == UNKNOWN) {
            continue;
        }

        D(INFO, "Turning off IO for board 0x%08lx\n", board);
        // turn off IO response for all other unclaimed boards
        UWORD command = pci_read_config_word(PCI_COMMAND, board);
        command &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
        pci_write_config_word(PCI_COMMAND, command, board);
    }

exit:

    if (!cd->board) {
        CloseLibrary(OpenPciBase);
        cd->OpenPciBase = NULL;
        return FALSE;
    }

    return TRUE;
}

BOOL InitCard(__REGA0(struct BoardInfo *bi), __REGA1(CONST_STRPTR *ToolTypes))
{
    CardData_t *cd = getCardData(bi);
    if (!cd->board || !cd->OpenPciBase) {
        DFUNC(ERROR, "No board claimed\n");
        return FALSE;
    }

    LOCAL_OPENPCIBASE();
    LOCAL_SYSBASE();

    ULONG deviceId = 0, revision = 0, memory0Size = 0, memory1Size = 0, memory2Size = 0;
    APTR memory0 = 0, memory1 = 0, memory2 = 0, legacyIOBase = 0;

    ULONG count = GetBoardAttrs(cd->board, PRM_Device, (Tag)&deviceId, PRM_Revision, (Tag)&revision, PRM_MemoryAddr0,
                                (Tag)&memory0, PRM_MemorySize0, (Tag)&memory0Size, PRM_LegacyIOSpace,
                                (Tag)&legacyIOBase, PRM_MemoryAddr1, (Tag)&memory1, PRM_MemorySize1, (Tag)&memory1Size,
                                PRM_MemoryAddr2, (Tag)&memory2, PRM_MemorySize2, (Tag)&memory2Size, TAG_END);
    if (count < 5) {
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

    bi->ChipBase                  = ChipBase;
    getCardData(bi)->legacyIOBase = legacyIOBase + REGISTER_OFFSET;

    // Set up register and memory bases
    if (chipFamily > MACH64GX && !memory1) {
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
        releaseCard(bi);
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

extern struct UtilityBase *UtilityBase;

static struct BoardInfo boardInfo = {0};

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

    if (!FindCard(bi, NULL)) {
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
