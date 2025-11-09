#include "s3trio64_common.h"

#include <clib/debug_protos.h>
#include <exec/nodes.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/picasso96_chip.h>

#define OPENPCI_SWAP  // don't make it define its own SWAP macros
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/openpci.h>
#include <proto/timer.h>
#include <utility/tagitem.h>

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

/**
 * Parse a hexadecimal or decimal string to ULONG value
 * Supports both "0x1234" (hex) and "1234" (decimal) formats
 * Uses manual parsing to avoid dependencies on library functions
 */
static ULONG parseHexOrDecimal(CONST_STRPTR str)
{
    ULONG value = 0;
    if (!str || !*str)
        return 0;

    DFUNC(INFO, "parsing %s\n", str);

    // Check for hex prefix
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        // Parse hexadecimal
        str += 2;
        while (*str) {
            char c = *str++;
            if (c >= '0' && c <= '9')
                value = (value << 4) | (c - '0');
            else if (c >= 'a' && c <= 'f')
                value = (value << 4) | (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                value = (value << 4) | (c - 'A' + 10);
            else
                break;
        }
    } else {
        // Parse decimal
        while (*str) {
            char c = *str++;
            if (c >= '0' && c <= '9')
                value = value * 10 + (c - '0');
            else
                break;
        }
    }

    DFUNC(INFO, "parsed to 0x%08lx (%lu)\n", value, value);
    return value;
}

static BOOL parseToolTypes(struct BoardInfo *bi, CONST_STRPTR *ToolTypes, ULONG *deviceId, ULONG *vendorId, ULONG *slot,
                           ULONG *bus)
{
    LOCAL_SYSBASE();

    struct Library *IconBase = OpenLibrary("icon.library", 0);
    if (!IconBase) {
        DFUNC(ERROR, "Cannot open icon.library\n");
        return FALSE;
    }

    if (deviceId) {
        CONST_STRPTR deviceStr = (CONST_STRPTR)FindToolType(ToolTypes, "DEVICEID");
        if (deviceStr) {
            *deviceId = parseHexOrDecimal(deviceStr);
            D(INFO, "Tooltype DEVICEID=%s parsed to 0x%08lx\n", deviceStr, *deviceId);
        }
    }

    if (vendorId) {
        CONST_STRPTR vendorStr = (CONST_STRPTR)FindToolType(ToolTypes, "VENDORID");
        if (vendorStr) {
            *vendorId = parseHexOrDecimal(vendorStr);
            D(INFO, "Tooltype VENDORID=%s parsed to 0x%08lx\n", vendorStr, *vendorId);
        }
    }

    if (slot) {
        CONST_STRPTR slotStr = (CONST_STRPTR)FindToolType(ToolTypes, "SLOT");
        if (slotStr) {
            ULONG slotValue = parseHexOrDecimal(slotStr);
            *slot           = slotValue;
            D(INFO, "Tooltype SLOT=%s parsed to %ld\n", slotStr, *slot);
        }
    }

    if (bus) {
        CONST_STRPTR busStr = (CONST_STRPTR)FindToolType(ToolTypes, "BUS");
        if (busStr) {
            ULONG busValue = parseHexOrDecimal(busStr);
            *bus           = busValue;
            D(INFO, "Tooltype BUS=%s parsed to %ld\n", busStr, *bus);
        }
    }

    CloseLibrary(IconBase);
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

    int numTags            = 0;
    struct TagItem tags[5] = {{TAG_END, 0}};
    // Parse tooltypes for card-specific settings (deviceId, vendorId, slot)
    LONG deviceId = 0, vendorId = VENDOR_ID_S3, slot = -1, bus = -1;
    if (ToolTypes) {
        parseToolTypes(bi, ToolTypes, &deviceId, &vendorId, &slot, &bus);
    }

    if (vendorId) {
        D(INFO, "Tooltype VENDORID override: 0x%08lx\n", vendorId);
        tags[numTags].ti_Tag  = PRM_Vendor;
        tags[numTags].ti_Data = vendorId;
        numTags++;
    }
    if (deviceId) {
        D(INFO, "Tooltype DEVICEID override: 0x%08lx\n", deviceId);
        tags[numTags].ti_Tag  = PRM_Device;
        tags[numTags].ti_Data = deviceId;
        numTags++;
    }
    if (slot >= 0) {
        D(INFO, "Tooltype SLOT override: %ld\n", slot);
        tags[numTags].ti_Tag  = PRM_SlotNumber;
        tags[numTags].ti_Data = slot;
        numTags++;
    }
    if (bus >= 0) {
        D(INFO, "Tooltype BUS override: %ld\n", bus);
        tags[numTags].ti_Tag  = PRM_BusNumber;
        tags[numTags].ti_Data = bus;
        numTags++;
    }
    tags[numTags].ti_Tag = TAG_END;

    struct pci_dev *board = NULL;
    while (board = FindBoardA(board, tags)) {
        D(INFO, "S3 board found\n");

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
        GetBoardAttrs(board, PRM_BoardOwner, (Tag)&owner, TAG_END);
        if (owner) {
            D(INFO, "Board already owned by: %s\n", (owner && owner->ln_Name) ? owner->ln_Name : "Unknown");
            continue;
        } else {
            // Claim the first board that is not yet owned
            if (!cd->board) {
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
                bi->BoardName              = "S3Trio64";
            } else {
                D(INFO, "Turning off IO for board 0x%08lx\n", board);
                // turn off IO response for all other unclaimed boards
                UWORD command = pci_read_config_word(PCI_COMMAND, board);
                command &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
                pci_write_config_word(PCI_COMMAND, command, board);
            }
        }
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

    static const char *libNames[] = {CHIP_NAME_VISION864, CHIP_NAME_TRIO3264, CHIP_NAME_TRIO64PLUS,
                                     CHIP_NAME_TRIO64PLUS, CHIP_NAME_TRIO64V2};

    struct ChipBase *ChipBase = NULL;

    ChipFamily_t chipFamily = getChipData(bi)->chipFamily;
    if (!(ChipBase = (struct ChipBase *)OpenLibrary(libNames[chipFamily - 1], 0))) {
        D(ERROR, "S3Trio.card: could not open chip library %s\n", libNames[chipFamily - 1]);
        return FALSE;
    }

    bi->ChipBase = ChipBase;

    D(INFO, "calling init chip...\n");
    if (!InitChip(bi)) {
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
