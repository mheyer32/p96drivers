#include "at3d_common.h"

#define OPENPCI_SWAP  // don't make it define its own SWAP macros
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/openpci.h>

ChipFamily_t getChipFamily(UWORD deviceId, UWORD revision)
{
    switch (deviceId) {
    case 0x6422:  // ProMotion 6422
        return AT6422;
    case 0x6424:  // ProMotion 6424
        return AT6424;
    case 0x6425:  // ProMotion AT25
        return AT25;
    case 0x643D:  // ProMotion AT3D
        return AT3D;
    default:
        DFUNC(WARN, "Unknown chip family, device ID: 0x%04lx\n", (ULONG)deviceId);
        return UNKNOWN;
    }
}

const char *getChipFamilyName(ChipFamily_t family)
{
    switch (family) {
    case AT6422:
        return "ProMotion 6422";
    case AT6424:
        return "ProMotion 6424";
    case AT25:
        return "ProMotion AT25";
    case AT3D:
        return "ProMotion AT3D";
    case UNKNOWN:
    default:
        return "Unknown";
    }
}

BOOL initRegisterAndMemoryBases(BoardInfo_t *bi)
{
    LOCAL_OPENPCIBASE();

    CardData_t *card = getCardData(bi);
    ChipData_t *chip = getChipData(bi);

    ULONG deviceId = 0, revision = 0;
    APTR memory0 = 0, memory1 = 0, legacyIOBase = 0;
    ULONG memory0Size = 0, memory1Size = 0;

    ULONG count = GetBoardAttrs(card->board, PRM_Device, (Tag)&deviceId, PRM_Revision, (Tag)&revision, PRM_MemoryAddr0,
                                (Tag)&memory0, PRM_MemorySize0, (Tag)&memory0Size, PRM_MemoryAddr1, (Tag)&memory1,
                                PRM_MemorySize1, (Tag)&memory1Size, PRM_LegacyIOSpace, (Tag)&legacyIOBase, TAG_END);
    if (count < 6) {
        DFUNC(ERROR, "Could not retrieve all required board attributes\n");
        return FALSE;
    }

    chip->chipFamily = getChipFamily(deviceId, revision);

    if (chip->chipFamily == UNKNOWN) {
        DFUNC(ERROR, "Unknown Chip family\n");
        return FALSE;
    }

    D(INFO, "Memory0 (BAR0) 0x%lx, Size %ld\n", memory0, memory0Size);
    D(INFO, "Memory1 (BAR1) 0x%lx, Size %ld\n", memory1, memory1Size);

    bi->MemoryBase      = (UBYTE *)memory0;
    bi->MemorySpaceBase = memory0;
    bi->MemorySpaceSize = memory0Size;
    getCardData(bi)->legacyIOBase = legacyIOBase + REGISTER_OFFSET;

    if (chip->chipFamily <= AT24)
    {
        bi->RegisterBase = legacyIOBase + REGISTER_OFFSET;
        bi->MemoryIOBase = bi->MemoryBase + 4*1024*1024 - 2048 + MMIOREGISTER_OFFSET;
    }
    else
    {
        bi->RegisterBase = bi->MemoryBase + 0xFFF000 + REGISTER_OFFSET;
        bi->MemoryIOBase = bi->MemoryBase + 0xFFEC00 + MMIOREGISTER_OFFSET;
        setCacheMode(bi, bi->MemoryBase + 0xFFEC00, 2048, MAPP_IO | MAPP_CACHEINHIBIT, CACHEFLAGS);
    }

    D(INFO, "AT3D: Framebuffer (BAR0) = 0x%lx\n", (ULONG)bi->MemoryBase);
    D(INFO, "AT3D: RegisterBase 0x%lx\n", bi->RegisterBase - REGISTER_OFFSET);
    D(INFO, "AT3D: Extended Registers 0x%lx\n", bi->MemoryIOBase - MMIOREGISTER_OFFSET);

    // enable special cache mode settings
    bi->Flags |= BIF_CACHEMODECHANGE;

    return TRUE;
}
