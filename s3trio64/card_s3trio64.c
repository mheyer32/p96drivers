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
#include <proto/Picasso96_chip.h>


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
        DFUNC(ERROR, "No board claimed\n");
        return FALSE;
    }

    LOCAL_OPENPCIBASE();
    LOCAL_SYSBASE();

    ULONG deviceId = 0, revision = 0, memory0Size = 0, legacyIOBase = 0;
    APTR memory0 = 0;

    ULONG count = GetBoardAttrs(cd->board, PRM_Device, (Tag)&deviceId, PRM_Revision, (Tag)&revision, PRM_MemoryAddr0,
                                (Tag)&memory0, PRM_MemorySize0, (Tag)&memory0Size, PRM_LegacyIOSpace,
                                (Tag)&legacyIOBase, TAG_END);
    if (count < 5) {
        DFUNC(ERROR, "Could not retrieve all required board attributes\n");
        return FALSE;
    }

    ChipFamily_t chipFamily = getChipFamily(deviceId, revision);

    if (chipFamily == UNKNOWN) {
        DFUNC(ERROR, "Unknown Chip family\n");
        return FALSE;
    }
    D(INFO, "Initializing %s\n", getChipFamilyName(chipFamily));
    D(INFO, "LegacyIOBase 0x%lx , MemoryBase 0x%lx, MemorySize %ld\n", legacyIOBase, memory0, memory0Size);
    APTR physicalAddress = pci_logic_to_physic_addr(memory0, cd->board);
    D(INFO, "physicalAddress 0x%08lx\n", physicalAddress);

    struct ChipBase *ChipBase = NULL;

    static const char *libNames[] = {CHIP_NAME_VISION864, CHIP_NAME_TRIO3264, CHIP_NAME_TRIO64PLUS};

    if (!(ChipBase = (struct ChipBase *)OpenLibrary(libNames[chipFamily - 1], 0)) != NULL) {
        D(ERROR, "S3Trio.card: could not open chip library %d\n", libNames[chipFamily - 1]);
        return FALSE;
    }

    bi->ChipBase                  = ChipBase;
    getCardData(bi)->legacyIOBase = (UBYTE *)legacyIOBase + REGISTER_OFFSET;
    if (chipFamily >= TRIO64PLUS) {
        // The Trio64
        // S3Trio64.chip expects register base adress to be offset by 0x8000
        // to be able to address all registers with just regular signed 16bit
        // offsets
        bi->RegisterBase = (UBYTE *)legacyIOBase + REGISTER_OFFSET;
        // Use the Trio64+ MMIO range in the BE Address Window at BaseAddress +
        // 0x3000000
        bi->MemoryIOBase = (UBYTE *)memory0 + 0x3000000 + MMIOREGISTER_OFFSET;
        // No need to fudge with the base address here
        bi->MemoryBase = (UBYTE *)memory0;
    } else {
        bi->RegisterBase = (UBYTE *)legacyIOBase + REGISTER_OFFSET;
        // This is how I understand Trio64/32 MMIO approach: 0xA0000 is
        // hardcoded as the base of the enhanced registers I need to make
        // sure, the first 1 MB of address space don't overlap with anything.
        bi->MemoryIOBase = (UBYTE *)pci_physic_to_logic_addr((APTR)0xA0000, cd->board);

        if (bi->MemoryIOBase == NULL) {
            D(ERROR, "S3Trio.card: VGA memory window at 0xA0000-BFFFF is not available. Aborting.\n");
            return FALSE;
        }

        D(ERROR, "S3Trio.card: MMIO Base at physical address 0xA0000 virtual: 0x%lx.\n", bi->MemoryIOBase);

        bi->MemoryIOBase += MMIOREGISTER_OFFSET;

        // I have to push out the card's Linear Address Window memory base
        // address to not overlap with its own MMIO address range at
        // 0xA8000-0xAFFFF On Trio64+ this is way easier with the "new MMIO"
        // approach. Here we move the Linear Address Window up by 4MB. This
        // gives us 4MB alignment and moves the LAW while not moving the PCI
        // BAR of teh card. The assumption is that the gfx card is the first
        // one to be initialized and thus sit at 0x00000000 in PCI address
        // space. This way 0xA8000 is in the card's BAR and the LAW should be
        // at 0x400000
        bi->MemoryBase = memory0;
        if (pci_logic_to_physic_addr(bi->MemoryBase, cd->board) <= (APTR)0xB0000) {
            // This shifts the memory base address by 4MB, which should be ok
            // since the S3Trio asks for 8MB PCI address space, typically only
            // utilizing the first 4MB
            bi->MemoryBase += 0x400000;
            D(WARN,
              "WARNING: Trio64/32 memory base overlaps with MMIO address at "
              "0xA8000-0xAFFFF.\n"
              "Moving FB adress window out by 4mb to 0x%lx\n",
              bi->MemoryBase);
        }
    }

    D(INFO, "S3Trio64 calling init chip...\n");
    if (!InitChip(bi)) {
        DFUNC(ERROR, "InitChip() failed\n");
        return FALSE;
    }
    D(INFO, "S3Trio64 has %ldkb usable memory\n", bi->MemorySize / 1024);

    // register interrupt server
    // pci_add_intserver(&bi->HardInterrupt, board);
    // enable vertical blanking interrupt
    //                bi->Flags |= BIF_VBLANKINTERRUPT;

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
