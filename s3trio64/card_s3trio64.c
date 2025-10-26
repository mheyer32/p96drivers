#include "chip_s3trio64.h"
#include "common.h"

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
const char LibName[]     = "S3Trio64.card";
const char LibIdString[] = "S3Vision864/Trio32/64/64Plus Picasso96 card driver version 1.0";
const UWORD LibVersion   = 1;
const UWORD LibRevision  = 0;
#endif
#include <proto/picasso96_chip.h>

#ifdef DBG
int debugLevel = VERBOSE;
#endif

#define CHIP_NAME_TRIO64PLUS "picasso96/S3Trio64Plus.chip"
#define CHIP_NAME_TRIO3264   "picasso96/S3Trio3264.chip"
#define CHIP_NAME_VISION864  "picasso96/S3Vision864.chip"

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
    while (board = FindBoard(board, PRM_Vendor, VENDOR_ID_S3, TAG_END)) {
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

        cd->boardNode.ln_Name = "S3Trio64.card";
        if (!SetBoardAttrs(board, PRM_BoardOwner, (Tag)&cd->boardNode, TAG_END)) {
            struct Node *owner;
            GetBoardAttrs(board, PRM_BoardOwner, (Tag)&owner, TAG_END);
            D(INFO, "Card already claimed by: %s\n", (owner && owner->ln_Name) ? owner->ln_Name : "Unknown");
            continue;
        }

        cd->board       = board;
        cd->OpenPciBase = OpenPciBase;

        bi->BoardType              = BT_S3Trio64;
        bi->GraphicsControllerType = GCT_S3Trio64;
        bi->PaletteChipType        = PCT_S3Trio64;
        bi->BoardName              = "S3Trio64";

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
        DFUNC(ERROR, "S3Trio.card: No board claimed\n");
        return FALSE;
    }

    LOCAL_OPENPCIBASE();
    LOCAL_SYSBASE();

    if (!initRegisterAndMemoryBases(bi))
    {
        D(ERROR, "S3Trio.card: could not init card\n");
        return FALSE;
    }

    static const char *libNames[] = {CHIP_NAME_VISION864, CHIP_NAME_TRIO3264, CHIP_NAME_TRIO64PLUS,
                                     CHIP_NAME_TRIO64PLUS, CHIP_NAME_TRIO64PLUS};

    struct ChipBase *ChipBase = NULL;

    ChipFamily_t chipFamily = getChipData(bi)->chipFamily;
    if (!(ChipBase = (struct ChipBase *)OpenLibrary(libNames[chipFamily - 1], 0)) != NULL) {
        D(ERROR, "S3Trio.card: could not open chip library %d\n", libNames[chipFamily - 1]);
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
