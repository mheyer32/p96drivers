#include "et4000_common.h"

#define OPENPCI_SWAP  // don't make it define its own SWAP macros
#include <proto/openpci.h>
#include <libraries/openpci.h>
#include <libraries/pcitags.h>

ChipFamily_t getChipFamily(UWORD deviceId, UWORD revision)
{
    // ET4000/W32p device IDs typically start with 0x3200
    // The device ID format is 0x32xx where xx includes revision info
    // For now, we'll identify based on device ID range
    switch (deviceId) {
    case 0x3202:  // W32p rev A
    case 0x3205:  // W32p Rev B
    case 0x3206:  // W32p Rev C
    case 0x3207:  // W32p Rev D
            return ET4000_W32P;
    default:
        DFUNC(WARN, "Unknown ET4000 chip family, device ID: 0x%04x\n", deviceId);
        return ET4000_UNKNOWN;
    }
}

const char *getChipFamilyName(ChipFamily_t family)
{
    switch (family) {
    case ET4000_W32P:
        return "ET4000/W32p";
    case ET4000_UNKNOWN:
    default:
        return "Unknown";
    }
}

BOOL initRegisterAndMemoryBases(BoardInfo_t *bi)
{
    LOCAL_OPENPCIBASE();

    CardData_t *card = getCardData(bi);
    ChipData_t *chip = getChipData(bi);

    ULONG deviceId = 0, revision = 0, memory0Size = 0, legacyIOBase = 0;
    APTR memory0 = 0;

    ULONG count = GetBoardAttrs(card->board, PRM_Device, (Tag)&deviceId, PRM_Revision, (Tag)&revision, PRM_MemoryAddr0,
                                (Tag)&memory0, PRM_MemorySize0, (Tag)&memory0Size, PRM_LegacyIOSpace,
                                (Tag)&legacyIOBase, TAG_END);
    if (count < 5) {
        DFUNC(ERROR, "Could not retrieve all required board attributes\n");
        return FALSE;
    }

    chip->chipFamily = getChipFamily(deviceId, revision);

    if (chip->chipFamily == ET4000_UNKNOWN) {
        DFUNC(ERROR, "Unknown Chip family\n");
        return FALSE;
    }
    D(INFO, "LegacyIOBase 0x%lx , MemoryBase 0x%lx, MemorySize %ld\n", legacyIOBase, memory0, memory0Size);
    APTR physicalAddress = pci_logic_to_physic_addr(memory0, card->board);
    D(INFO, "physicalAddress 0x%08lx\n", physicalAddress);

    // ET4000 uses standard VGA I/O registers
    card->legacyIOBase = (UBYTE *)legacyIOBase + REGISTER_OFFSET;
    bi->RegisterBase    = (UBYTE *)legacyIOBase + REGISTER_OFFSET;
    
    // ET4000/W32p may have MMIO registers, but primarily uses I/O space
    // For now, set MemoryIOBase to NULL if no MMIO is available
    bi->MemoryIOBase = NULL;
    
    // Set memory base
    bi->MemoryBase = (UBYTE *)memory0;

    // enable special cache mode settings
    bi->MemorySpaceBase = memory0;
    bi->MemorySpaceSize = memory0Size;
    bi->Flags |= BIF_CACHEMODECHANGE;

    return TRUE;
}
