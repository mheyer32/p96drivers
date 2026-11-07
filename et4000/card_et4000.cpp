#include "card_common.h"
#include "et4000_common.h"

#define __NOLIBBASE__
#include <clib/debug_protos.h>
#include <exec/nodes.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/openpci.h>
#include <proto/picasso96_chip.h>
#include <proto/timer.h>
#include <proto/utility.h>

#include <libraries/openpci.h>
#include <libraries/pcitags.h>

#ifndef TESTEXE
extern const char LibName[]     = "ET4000.card";
extern const char LibIdString[] = "Tseng ET4000/W32p Picasso96 card driver version 1.0";
extern const UWORD LibVersion   = 1;
extern const UWORD LibRevision  = 0;
#endif

#ifdef DBG
int debugLevel = VERBOSE;
#endif

#define CHIP_NAME_ET4000 "picasso96/ET4000.chip"

BOOL releaseCard(__REGA0(struct BoardInfo *bi))
{
    releasePciCard(bi);
    return TRUE;
}

extern "C" BOOL FindCard(__REGA0(struct BoardInfo *bi), __REGA1(CONST_STRPTR *ToolTypes))
{
    LOCAL_SYSBASE();
    CardData_t *cd = getCardData(bi);

    ULONG deviceId = 0, vendorId = VENDOR_ID_TSENG, slot = (ULONG)-1, bus = (ULONG)-1;
    struct Library *OpenPciBase = NULL;
    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        DFUNC(ERROR, "Cannot open openpci.library v%ld+\n", MIN_OPENPCI_VERSION);
        return FALSE;
    }

    if (ToolTypes) {
        parseToolTypes(bi, ToolTypes, &deviceId, &vendorId, &slot, &bus);
    }

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
    if ((LONG)slot >= 0) {
        D(INFO, "SLOT: %ld\n", slot);
        tags[numTags].ti_Tag  = PRM_SlotNumber;
        tags[numTags].ti_Data = slot;
        numTags++;
    }
    if ((LONG)bus >= 0) {
        D(INFO, "BUS: %ld\n", bus);
        tags[numTags].ti_Tag  = PRM_BusNumber;
        tags[numTags].ti_Data = bus;
        numTags++;
    }
    tags[numTags].ti_Tag = TAG_END;

    struct pci_dev *board = NULL;
    while ((board = FindBoardA(board, tags)) != NULL) {
        D(INFO, "Tseng board found\n");

        ULONG foundDeviceId = 0, revision = 0, foundSlot = 0, foundBus = 0;
        struct Node *owner = NULL;
        ULONG count =
            GetBoardAttrs(board, PRM_Device, (Tag)&foundDeviceId, PRM_Revision, (Tag)&revision, PRM_BoardOwner,
                          (Tag)&owner, PRM_SlotNumber, (Tag)&foundSlot, PRM_BusNumber, (Tag)&foundBus, TAG_END);
        if (count < 2) {
            DFUNC(ERROR, "Could not retrieve all required board attributes\n");
            continue;
        }
        D(INFO, "Tseng device %lx revision %lx\n", foundDeviceId, revision);
        ChipFamily_t chipFamily = getChipFamily(foundDeviceId, revision);
        if (chipFamily == ET4000_UNKNOWN) {
            D(WARN, "Unknown Chip family\n");
            continue;
        }
        D(INFO, "%s found\n", getChipFamilyName(chipFamily));

        if (owner) {
            D(INFO, "Board already owned by: %s\n", (owner && owner->ln_Name) ? owner->ln_Name : "Unknown");
            continue;
        }

        if (!cd->board) {
            generateBoardName(cd->boardName, "ET4000", foundBus, foundSlot);
            cd->boardNode.ln_Name = cd->boardName;
            if (!SetBoardAttrs(board, PRM_BoardOwner, (Tag)&cd->boardNode, TAG_END)) {
                D(ERROR, "Could not claim board\n");
                continue;
            }

            cd->board       = board;
            cd->OpenPciBase = OpenPciBase;

            bi->BoardType              = BT_ET4000W32p;
            bi->GraphicsControllerType = GCT_ETW32;
            bi->PaletteChipType        = PCT_Chrontel8498;
            bi->BoardName              = cd->boardName;
        } else {
            D(INFO, "Turning off IO for board 0x%08lx\n", board);
            UWORD command = pci_read_config_word(PCI_COMMAND, board);
            command &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
            pci_write_config_word(PCI_COMMAND, command, board);
        }
    }

    if (!cd->board) {
        CloseLibrary(OpenPciBase);
        cd->OpenPciBase = NULL;
        return FALSE;
    }

    return TRUE;
}

extern "C" BOOL InitCard(__REGA0(struct BoardInfo *bi), __REGA1(CONST_STRPTR *ToolTypes))
{
    CardData_t *cd = getCardData(bi);
    if (!cd->board || !cd->OpenPciBase) {
        DFUNC(ERROR, "ET4000.card: No board claimed\n");
        return FALSE;
    }

    LOCAL_OPENPCIBASE();
    LOCAL_SYSBASE();
    (void)OpenPciBase;
    (void)SysBase;

    if (ToolTypes) {
        parseBlackLevelToolType(bi, ToolTypes);
    }

    if (!initRegisterAndMemoryBases(bi)) {
        D(ERROR, "ET4000.card: could not init card\n");
        return FALSE;
    }

    struct ChipBase *ChipBase = NULL;

    if (!(ChipBase = (struct ChipBase *)OpenLibrary(CHIP_NAME_ET4000, 0))) {
        D(ERROR, "ET4000.card: could not open chip library %s\n", CHIP_NAME_ET4000);
        return FALSE;
    }

    bi->ChipBase = ChipBase;

    D(INFO, "calling init chip...\n");
    if (!InitChip(bi)) {
        DFUNC(ERROR, "InitChip() failed\n");
        return FALSE;
    }
    D(INFO, "card has %ldkb usable memory\n", bi->MemorySize / 1024);

    return TRUE;
}

#ifdef TESTEXE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct UtilityBase *UtilityBase;

static struct BoardInfo boardInfo;

BOOL releaseCard(__REGA0(struct BoardInfo *bi));

void sigIntHandler(int dummy)
{
    (void)dummy;
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
#endif
