#include "card_common.h"
#include "cirrus_common.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#include <libraries/openpci.h>
#include <libraries/pcitags.h>

#ifndef TESTEXE
extern const char LibName[]     = "CirrusGD542x.card";
extern const char LibIdString[] = "Cirrus Logic CL-GD542x Picasso96 card driver version 1.0";
#ifndef LIB_VERSION
#define LIB_VERSION 1
#endif
#ifndef LIB_REVISION
#define LIB_REVISION 0
#endif
extern const UWORD LibVersion  = LIB_VERSION;
extern const UWORD LibRevision = LIB_REVISION;
#endif

#ifdef DBG
int debugLevel = VERBOSE;
#endif

#define CHIP_NAME_CIRRUS "picasso96/CirrusGD542x.chip"

BOOL releaseCard(__REGA0(struct BoardInfo *bi))
{
    CardData_t *cd = getCardData(bi);

    if (cd->OpenPciBase) {
        LOCAL_OPENPCIBASE();
        if (cd->board) {
            SetBoardAttrs(cd->board, PRM_BoardOwner, (Tag)NULL, TAG_END);
        }

        LOCAL_SYSBASE();
        CloseLibrary(cd->OpenPciBase);
        cd->OpenPciBase = NULL;
    }
    return TRUE;
}

BOOL FindCard(__REGA0(struct BoardInfo *bi), __REGA1(CONST_STRPTR *ToolTypes))
{
    LOCAL_SYSBASE();
    CardData_t *cd = getCardData(bi);

    struct Library *OpenPciBase = NULL;
    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        DFUNC(ERROR, "Cannot open openpci.library v%ld+\n", MIN_OPENPCI_VERSION);
    }

    ULONG deviceId = 0, vendorId = VENDOR_ID_CIRRUS, slot = (ULONG)-1, bus = (ULONG)-1;
    if (ToolTypes) {
        parseToolTypes(bi, ToolTypes, &deviceId, &vendorId, &slot, &bus);
    }

    int numTags            = 0;
    struct TagItem tags[5] = {{TAG_END, 0}};
    if (vendorId) {
        tags[numTags].ti_Tag  = PRM_Vendor;
        tags[numTags].ti_Data = vendorId;
        numTags++;
    }
    if (deviceId) {
        tags[numTags].ti_Tag  = PRM_Device;
        tags[numTags].ti_Data = deviceId;
        numTags++;
    }
    if ((LONG)slot >= 0) {
        tags[numTags].ti_Tag  = PRM_SlotNumber;
        tags[numTags].ti_Data = slot;
        numTags++;
    }
    if ((LONG)bus >= 0) {
        tags[numTags].ti_Tag  = PRM_BusNumber;
        tags[numTags].ti_Data = bus;
        numTags++;
    }
    tags[numTags].ti_Tag = TAG_END;

    struct pci_dev *board = NULL;
    while ((board = FindBoardA(board, tags))) {
        ULONG dev = 0, revision = 0;

        ULONG count = GetBoardAttrs(board, PRM_Device, (Tag)&dev, PRM_Revision, (Tag)&revision, TAG_END);
        if (count < 2) {
            DFUNC(ERROR, "Could not retrieve board attributes\n");
            continue;
        }

        ChipFamily_t chipFamily = getChipFamily((UWORD)dev);
        if (chipFamily == UNKNOWN) {
            D(WARN, "Unknown Cirrus device 0x%04lx\n", dev);
            continue;
        }
        D(INFO, "%s (PCI %04lx)\n", getChipFamilyName(chipFamily), dev);

        struct Node *owner = NULL;
        ULONG slotNum = 0, busNum = 0;
        GetBoardAttrs(board, PRM_BoardOwner, (Tag)&owner, PRM_SlotNumber, (Tag)&slotNum, PRM_BusNumber, (Tag)&busNum,
                      TAG_END);
        if (owner) {
            D(INFO, "Board already owned\n");
            continue;
        }

        cd->boardNode.ln_Name = (char *)"CirrusGD54xx.card";
        if (!SetBoardAttrs(board, PRM_BoardOwner, (Tag)&cd->boardNode, TAG_END)) {
            D(ERROR, "Could not claim board\n");
            continue;
        }

        cd->board       = board;
        cd->OpenPciBase = OpenPciBase;

        bi->BoardType = BT_powerpci;
        switch (chipFamily) {
        case GD5446:
        case GD5440:
            bi->GraphicsControllerType = GCT_CirrusGD5446;
            bi->PaletteChipType        = PCT_CirrusGD5446;
            break;
        case GD5434:
            bi->GraphicsControllerType = GCT_CirrusGD5434;
            bi->PaletteChipType        = PCT_CirrusGD5434;
            break;
        default:
            bi->GraphicsControllerType = GCT_CirrusGD542x;
            bi->PaletteChipType        = PCT_CirrusGD542x;
            break;
        }

        generateBoardName(getCardData(bi)->boardName, "CL542x", busNum, slotNum);
        bi->BoardName = getCardData(bi)->boardName;

        break;
    }

    board = NULL;
    while ((board = FindBoard(board, PRM_Vendor, (ULONG)VENDOR_ID_CIRRUS))) {
        if (board == cd->board)
            continue;

        ULONG dev = 0, revision = 0;
        struct Node *owner = NULL;
        ULONG count = GetBoardAttrs(board, PRM_Device, (Tag)&dev, PRM_Revision, (Tag)&revision, PRM_BoardOwner,
                                    (Tag)&owner, TAG_END);
        if (count < 3)
            continue;
        if (owner)
            continue;
        if (getChipFamily((UWORD)dev) == UNKNOWN)
            continue;

        D(INFO, "Disabling PCI decode on unclaimed Cirrus 0x%08lx\n", (ULONG)board);
        UWORD command = pci_read_config_word(PCI_COMMAND, board);
        command &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
        pci_write_config_word(PCI_COMMAND, command, board);
    }

    if (!cd->board) {
        if (OpenPciBase)
            CloseLibrary(OpenPciBase);
        cd->OpenPciBase = NULL;
        return FALSE;
    }

    return TRUE;
}

BOOL InitCard(__REGA0(struct BoardInfo *bi), __REGA1(CONST_STRPTR *ToolTypes))
{
    (void)ToolTypes;
    CardData_t *cd = getCardData(bi);
    if (!cd->board || !cd->OpenPciBase) {
        DFUNC(ERROR, "CirrusGD542x.card: No board claimed\n");
        return FALSE;
    }

    LOCAL_OPENPCIBASE();
    LOCAL_SYSBASE();

    if (!initRegisterAndMemoryBases(bi)) {
        D(ERROR, "CirrusGD542x.card: could not map BARs\n");
        return FALSE;
    }

    struct ChipBase *ChipBase = NULL;
    if (!(ChipBase = (struct ChipBase *)OpenLibrary(CHIP_NAME_CIRRUS, 0))) {
        D(ERROR, "Could not open %s\n", CHIP_NAME_CIRRUS);
        return FALSE;
    }

    bi->ChipBase = ChipBase;

    if (!InitChip(bi)) {
        releaseCard(bi);
        DFUNC(ERROR, "InitChip failed\n");
        return FALSE;
    }

    D(INFO, "Cirrus framebuffer: %ld KB\n", bi->MemorySize / 1024);
    return TRUE;
}

#ifdef TESTEXE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct UtilityBase *UtilityBase;

static struct BoardInfo boardInfo = {0};

static void sigIntHandler(int dummy)
{
    (void)dummy;
    releaseCard(&boardInfo);
    abort();
}

int main(void)
{
    signal(SIGINT, sigIntHandler);

    int rval = EXIT_FAILURE;
    memset(&boardInfo, 0, sizeof(boardInfo));
    struct BoardInfo *bi = &boardInfo;
    bi->ExecBase = SysBase;
    bi->UtilBase = (struct Library *)UtilityBase;

    if (!FindCard(bi, NULL))
        goto exit;
    if (!InitCard(bi, NULL))
        goto exit;

    rval = EXIT_SUCCESS;
exit:
    releaseCard(bi);
    return rval;
}
#endif

#ifdef __cplusplus
}
#endif
