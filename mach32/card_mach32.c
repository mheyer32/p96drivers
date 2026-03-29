#include "card_common.h"
#include "chip_mach32.h"

#define __NOLIBBASE__

#include <clib/debug_protos.h>
#include <exec/nodes.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/picasso96_chip.h>
#include <proto/utility.h>
#include <utility/tagitem.h>

#define OPENPCI_SWAP
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/openpci.h>

#ifndef TESTEXE
const char LibName[]     = "ATIMach32.card";
const char LibIdString[] = "ATIMach32 Picasso96 card driver version 0.1";
#ifndef LIB_VERSION
#define LIB_VERSION 1
#endif
#ifndef LIB_REVISION
#define LIB_REVISION 0
#endif
const UWORD LibVersion  = LIB_VERSION;
const UWORD LibRevision = LIB_REVISION;
#endif

#ifdef DBG
int debugLevel = VERBOSE;
#endif

#define CHIP_NAME_MACH32 "picasso96/ATIMach32.chip"

#define VENDOR_ID_ATI    0x1002
#define DEVICE_ID_MACH32 0x4158

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
        goto exit;
    }

    ULONG matchDevice = DEVICE_ID_MACH32;
    LONG vendorId     = VENDOR_ID_ATI;
    LONG slot         = -1;
    LONG bus          = -1;

    if (ToolTypes) {
        ULONG parsedDev = 0;
        ULONG parsedVen = 0;
        parseToolTypes(bi, ToolTypes, &parsedDev, &parsedVen, (ULONG *)&slot, (ULONG *)&bus);
        if (parsedDev != 0) {
            matchDevice = parsedDev;
        }
        if (parsedVen != 0) {
            vendorId = (LONG)parsedVen;
        }
    }

    int numTags            = 0;
    struct TagItem tags[5] = {{TAG_END, 0}};
    tags[numTags].ti_Tag   = PRM_Vendor;
    tags[numTags].ti_Data  = (ULONG)vendorId;
    numTags++;
    tags[numTags].ti_Tag  = PRM_Device;
    tags[numTags].ti_Data = matchDevice;
    numTags++;
    if (slot >= 0) {
        tags[numTags].ti_Tag  = PRM_SlotNumber;
        tags[numTags].ti_Data = (ULONG)slot;
        numTags++;
    }
    if (bus >= 0) {
        tags[numTags].ti_Tag  = PRM_BusNumber;
        tags[numTags].ti_Data = (ULONG)bus;
        numTags++;
    }
    tags[numTags].ti_Tag = TAG_END;

    struct pci_dev *board = NULL;
    while ((board = FindBoardA(board, tags)) != NULL) {
        ULONG deviceId = 0, revision = 0;
        struct Node *owner = NULL;
        ULONG slotn = 0, busn = 0;

        ULONG ac = GetBoardAttrs(board, PRM_Device, (Tag)&deviceId, PRM_Revision, (Tag)&revision, PRM_BoardOwner,
                                 (Tag)&owner, PRM_SlotNumber, (Tag)&slotn, PRM_BusNumber, (Tag)&busn, TAG_END);
        if (ac < 5) {
            DFUNC(ERROR, "Could not read PCI board attributes (got %lu)\n", ac);
            continue;
        }

        if (deviceId != matchDevice) {
            continue;
        }

        if (owner) {
            D(INFO, "Mach32 board already owned\n");
            continue;
        }

        cd->boardNode.ln_Name = "ATIMach32.card";
        if (!SetBoardAttrs(board, PRM_BoardOwner, (Tag)&cd->boardNode, TAG_END)) {
            D(ERROR, "Could not claim Mach32 board\n");
            continue;
        }

        cd->board       = board;
        cd->OpenPciBase = OpenPciBase;

        bi->BoardType              = BT_Mach32;
        bi->GraphicsControllerType = GCT_ATIRV100;
        bi->PaletteChipType        = PCT_Unknown;

        generateBoardName(getCardData(bi)->boardName, "Mach32", busn, slotn);
        bi->BoardName = getCardData(bi)->boardName;
        break;
    }

    /* Disable I/O decode on other unclaimed ATI boards to avoid conflicts. */
    board = NULL;
    while ((board = FindBoard(board, PRM_Vendor, (ULONG)vendorId)) != NULL) {
        if (board == cd->board) {
            continue;
        }

        ULONG deviceId     = 0;
        struct Node *owner = NULL;
        if (GetBoardAttrs(board, PRM_Device, (Tag)&deviceId, PRM_BoardOwner, (Tag)&owner, TAG_END) < 2) {
            continue;
        }
        if (owner) {
            continue;
        }

        D(INFO, "Disabling PCI I/O+MEM for unclaimed ATI board dev 0x%lx\n", deviceId);
        UWORD command = pci_read_config_word(PCI_COMMAND, board);
        command &= (UWORD) ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
        pci_write_config_word(PCI_COMMAND, command, board);
    }

exit:
    if (!cd->board) {
        if (OpenPciBase) {
            CloseLibrary(OpenPciBase);
        }
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
        DFUNC(ERROR, "No Mach32 board claimed\n");
        return FALSE;
    }

    LOCAL_OPENPCIBASE();
    LOCAL_SYSBASE();

    ULONG deviceId = 0, revision = 0, memory0Size = 0;
    UBYTE memory0 = NULL, legacyIOBase = NULL;

    ULONG count = GetBoardAttrs(cd->board, PRM_Device, (Tag)&deviceId, PRM_Revision, (Tag)&revision, PRM_MemoryAddr0,
                                (Tag)&memory0, PRM_MemorySize0, (Tag)&memory0Size, PRM_LegacyIOSpace,
                                (Tag)&legacyIOBase, TAG_END);
    if (count < 5) {
        DFUNC(ERROR, "Could not retrieve Mach32 BAR attributes\n");
        return FALSE;
    }

    if (deviceId != DEVICE_ID_MACH32) {
        DFUNC(ERROR, "Wrong PCI device 0x%lx (expected Mach32)\n", deviceId);
        return FALSE;
    }

    D(INFO, "Mach32 BAR0 %p size %lu legacy IO %p\n", memory0, memory0Size, legacyIOBase);

    {
        UWORD cmd = pci_read_config_word(PCI_COMMAND, cd->board);
        pci_write_config_word(PCI_COMMAND, (UWORD)(cmd | PCI_COMMAND_MEMORY | PCI_COMMAND_IO), cd->board);
    }

    struct ChipBase *ChipBase = NULL;
    if (!(ChipBase = (struct ChipBase *)OpenLibrary(CHIP_NAME_MACH32, 0))) {
        D(ERROR, "Could not open chip library %s\n", CHIP_NAME_MACH32);
        return FALSE;
    }

    bi->ChipBase                  = ChipBase;
    bi->RegisterBase              = legacyIOBase + REGISTER_OFFSET;
    getCardData(bi)->legacyIOBase = bi->RegisterBase;

    bi->MemoryBase = memory0;
    bi->MemorySize = memory0Size;

    /* Registers: PCI legacy I/O only (RegisterBase). No framebuffer BAR MMIO alias. */
    bi->MemoryIOBase = NULL;
    if (memory0Size > 0) {
        setCacheMode(bi, memory0, memory0Size, MAPP_CACHEINHIBIT | MAPP_IMPRECISE | MAPP_NONSERIALIZED, CACHEFLAGS);
    }

    D(INFO, "ATIMach32.card calling InitChip...\n");
    if (!InitChip(bi)) {
        CloseLibrary((struct Library *)ChipBase);
        bi->ChipBase = NULL;
        releaseCard(bi);
        DFUNC(ERROR, "InitChip() failed\n");
        return FALSE;
    }

    D(INFO, "ATIMach32 usable VRAM %luk\n", bi->MemorySize / 1024UL);

    bi->MemorySpaceBase = memory0;
    bi->MemorySpaceSize = memory0Size;
    bi->Flags |= BIF_CACHEMODECHANGE;

    return TRUE;
}

#ifdef TESTEXE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct UtilityBase *UtilityBase;

static struct BoardInfo boardInfo;

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
    bi->ExecBase         = SysBase;
    bi->UtilBase         = UtilityBase;

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
