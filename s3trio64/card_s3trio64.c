#include "card_common.h"
#include "s3trio64_common.h"

#define __NOLIBBASE__
#include <clib/debug_protos.h>
#include <exec/nodes.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/openpci.h>
#include <proto/picasso96_chip.h>
#include <proto/timer.h>
#include <proto/utility.h>
#include <utility/tagitem.h>

#define OPENPCI_SWAP  // don't make it define its own SWAP macros
#include <libraries/openpci.h>
#include <libraries/pcitags.h>

#ifndef TESTEXE
const char LibName[]     = "S3Trio64.card";
const char LibIdString[] = "S3Vision864/Trio32/64/64Plus Picasso96 card driver version 1.0";
const UWORD LibVersion   = 1;
const UWORD LibRevision  = 0;
#endif

#ifdef DBG
int debugLevel = VERBOSE;
#endif

#define CHIP_NAME_VISION864  "picasso96/S3Vision864.chip"
#define CHIP_NAME_TRIO3264   "picasso96/S3Trio3264.chip"
#define CHIP_NAME_TRIO64PLUS "picasso96/S3Trio64Plus.chip"
#define CHIP_NAME_TRIO64V2   "picasso96/S3Trio64V2.chip"

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
    LONG deviceId = 0, vendorId = VENDOR_ID_S3, slot = -1, bus = -1;
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
        D(INFO, "S3 board found 0x%lx\n", board);

        ULONG deviceId, revision;

        ULONG count = GetBoardAttrs(board, PRM_Device, (Tag)&deviceId, PRM_Revision, (Tag)&revision, TAG_END);
        if (count < 2) {
            DFUNC(ERROR, "Could not retrieve all required board attributes\n");
            continue;
        }
        D(INFO, "S3 device %lx revision %lx\n", deviceId, revision);
        ChipFamily_t chipFamily = getChipFamily(deviceId, revision);
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
        cd->boardNode.ln_Name = "S3Trio64.card";
        if (!SetBoardAttrs(board, PRM_BoardOwner, (Tag)&cd->boardNode, TAG_END)) {
            D(ERROR, "Could not claim board\n");
            continue;
        }

        cd->board       = board;
        cd->OpenPciBase = OpenPciBase;

        bi->BoardType              = BT_S3Trio64;
        bi->GraphicsControllerType = GCT_S3Trio64;
        bi->PaletteChipType        = PCT_S3Trio64;

        // generate unique board name based on bus/slot
        generateBoardName(getCardData(bi)->boardName, "S3Trio64", bus, slot);
        bi->BoardName = getCardData(bi)->boardName;

        // Found and claimed the board
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

        ChipFamily_t chipFamily = getChipFamily(deviceId, revision);
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
        DFUNC(ERROR, "S3Trio.card: No board claimed\n");
        return FALSE;
    }

    LOCAL_OPENPCIBASE();
    LOCAL_SYSBASE();

    if (!initRegisterAndMemoryBases(bi)) {
        D(ERROR, "S3Trio.card: could not init card\n");
        return FALSE;
    }

    static const char *libNames[] = {CHIP_NAME_VISION864, CHIP_NAME_TRIO3264, CHIP_NAME_TRIO64PLUS,CHIP_NAME_TRIO64PLUS,CHIP_NAME_TRIO64V2,
                                     CHIP_NAME_TRIO64V2, CHIP_NAME_TRIO64V2};

    struct ChipBase *ChipBase = NULL;

    ChipFamily_t chipFamily = getChipData(bi)->chipFamily;
    if (!(ChipBase = (struct ChipBase *)OpenLibrary(libNames[chipFamily - 1], 0))) {
        D(ERROR, "S3Trio.card: could not open chip library %s\n", libNames[chipFamily - 1]);
        return FALSE;
    }

    bi->ChipBase = ChipBase;

    D(INFO, "calling init chip...\n");
    if (!InitChip(bi)) {
        releaseCard(bi);
        DFUNC(ERROR, "InitChip() failed\n");
        return FALSE;
    }
    D(INFO, "card has %ldkb usable memory\n", bi->MemorySize / 1024);

    // register interrupt server
    // pci_add_intserver(&bi->HardInterrupt, board);
    // enable vertical blanking interrupt
    //                bi->Flags |= BIF_VBLANKINTERRUPT;

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

//extern BOOL TestCard(BoardInfo_t *bi);

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

    D(INFO, "UtilityBase 0x%lx\n", bi->UtilBase);

    if (!FindCard(bi, NULL)) {
        goto exit;
    }

    if (!InitCard(bi, NULL)) {
        goto exit;
    }

//    rval = TestCard(bi);

exit:
    releaseCard(bi);
    return rval;
}
#endif  // TESTEXE
