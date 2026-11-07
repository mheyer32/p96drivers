#include "et4000_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/openpci.h>

ChipFamily_t getChipFamily(UWORD deviceId, UWORD revision)
{
    (void)revision;
    switch (deviceId) {
    case 0x3202:  // W32p rev A
    case 0x3205:  // W32p Rev B
    case 0x3206:  // W32p Rev C
    case 0x3207:  // W32p Rev D
        return ET4000_W32P;
    default:
        DFUNC(WARN, "Unknown ET4000 chip family, device ID: 0x%04lx\n", (ULONG)deviceId);
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

    ULONG deviceId = 0, revision = 0, memory0Size = 0;
    APTR memory0 = 0, legacyIOBase = 0;

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

    card->legacyIOBase = (volatile UBYTE *)legacyIOBase + REGISTER_OFFSET;
    bi->RegisterBase   = (UBYTE *)legacyIOBase + REGISTER_OFFSET;
    bi->MemoryIOBase   = NULL;
    bi->MemoryBase     = (UBYTE *)memory0;

    {
        UWORD cmd = pci_read_config_word(PCI_COMMAND, card->board);
        pci_write_config_word(PCI_COMMAND, cmd | PCI_COMMAND_MEMORY | PCI_COMMAND_IO, card->board);
    }

    bi->MemorySpaceBase = memory0;
    bi->MemorySpaceSize = memory0Size;
    bi->Flags |= BIF_CACHEMODECHANGE;

    setCacheMode(bi, memory0, memory0Size, MAPP_CACHEINHIBIT | MAPP_IMPRECISE | MAPP_NONSERIALIZED, CACHEFLAGS);

    return TRUE;
}

#ifdef __cplusplus
}
#endif
