#include "cirrus_common.h"

#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/openpci.h>

#ifdef __cplusplus
extern "C" {
#endif

ChipFamily_t getChipFamily(UWORD deviceId)
{
    switch (deviceId) {
    case 0x00A0:
        return GD5430;
    case 0x00A2:
        return GD5432;
    case 0x00A4:
    case 0x00A8:
        return GD5434;
    case 0x00AC:
    case 0x00E8: /* GD5436U */
        return GD5436;
    case 0x00B0:
        return GD5440;
    case 0x00B8:
        return GD5446;
    case 0x00BC:
        return GD5480;
    default:
        DFUNC(WARN, "Unsupported Cirrus PCI device 0x%04lx\n", (ULONG)deviceId);
        return UNKNOWN;
    }
}

const char *getChipFamilyName(ChipFamily_t family)
{
    switch (family) {
    case GD5430:
        return "CL-GD5430/40 Alpine";
    case GD5432:
        return "CL-GD5432 Alpine";
    case GD5434:
        return "CL-GD5434";
    case GD5436:
        return "CL-GD5436";
    case GD5440:
        return "CL-GD5440";
    case GD5446:
        return "CL-GD5446";
    case GD5480:
        return "CL-GD5480";
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
    APTR memory0 = 0, legacyIOBase = 0;
    ULONG memory0Size = 0;

    ULONG count = GetBoardAttrs(card->board, PRM_Device, (Tag)&deviceId, PRM_Revision, (Tag)&revision, PRM_MemoryAddr0,
                                (Tag)&memory0, PRM_MemorySize0, (Tag)&memory0Size, PRM_LegacyIOSpace,
                                (Tag)&legacyIOBase, TAG_END);
    if (count < 5) {
        DFUNC(ERROR, "Could not retrieve required board attributes\n");
        return FALSE;
    }

    chip->chipFamily = getChipFamily((UWORD)deviceId);
    if (chip->chipFamily == UNKNOWN) {
        DFUNC(ERROR, "Unknown Cirrus chip (device 0x%04lx)\n", (ULONG)deviceId);
        return FALSE;
    }

    D(INFO, "Cirrus %s: BAR0 0x%lx size %ld, Legacy IO 0x%lx\n",
      getChipFamilyName((ChipFamily_t)chip->chipFamily), (ULONG)memory0, memory0Size, (ULONG)legacyIOBase);

    bi->MemoryBase      = (UBYTE *)memory0;
    bi->MemorySpaceBase = memory0;
    bi->MemorySpaceSize = memory0Size;

    card->legacyIOBase = (volatile UBYTE *)legacyIOBase;
    bi->RegisterBase   = (UBYTE *)legacyIOBase + REGISTER_OFFSET;

    {
        UWORD cmd = pci_read_config_word(PCI_COMMAND, card->board);
        pci_write_config_word(PCI_COMMAND, cmd | PCI_COMMAND_MEMORY | PCI_COMMAND_IO, card->board);
    }

    if (!setCacheMode(bi, memory0, memory0Size, MAPP_CACHEINHIBIT | MAPP_IMPRECISE | MAPP_NONSERIALIZED,
                      CACHEFLAGS)) {
        DFUNC(ERROR, "setCacheMode BAR0 failed\n");
    } else {
        D(INFO, "setCacheMode BAR0 OK (%ld bytes @ 0x%lx)\n", memory0Size, (ULONG)memory0);
    }

    /* MMIO window mapping:
     * - Generic GD543x/4x MMIO window begins at linear address 0x88000.
     * - GD5430/GD5436/GD5440 can relocate MMIO to top 256 bytes of linear
     *   aperture when SR17[6]=1 (configured in InitChip). */
    if (chip->chipFamily == GD5430 || chip->chipFamily == GD5436 || chip->chipFamily == GD5440) {
        bi->MemoryIOBase = bi->MemoryBase + memory0Size - 256 + MMIOREGISTER_OFFSET;
        setCacheMode(bi, bi->MemoryBase + memory0Size - 256, 256, MAPP_IO | MAPP_CACHEINHIBIT, CACHEFLAGS);
    } else {
        bi->MemoryIOBase = bi->MemoryBase + 0x88000 + MMIOREGISTER_OFFSET;
        setCacheMode(bi, bi->MemoryBase + 0x88000, 256, MAPP_IO | MAPP_CACHEINHIBIT, CACHEFLAGS);
    }

    return TRUE;
}

#ifdef __cplusplus
}
#endif
