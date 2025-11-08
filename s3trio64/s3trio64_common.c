#include "s3trio64_common.h"

#define OPENPCI_SWAP  // don't make it define its own SWAP macros
#include <proto/openpci.h>
#include <libraries/openpci.h>
#include <libraries/pcitags.h>

ChipFamily_t getChipFamily(UWORD deviceId, UWORD revision)
{
    switch (deviceId) {
    case 0x88C0:  // 86c864 Vision 864
    case 0x88C1:  // 86c864 Vision 864
        return VISION864;
    case 0x88f0:  // 86c968 [Vision 968 VRAM] rev 0
    case 0x88f1:  // 86c968 [Vision 968 VRAM] rev 1
    case 0x88f2:  // 86c968 [Vision 968 VRAM] rev 2
    case 0x88f3:  // 86c968 [Vision 968 VRAM] rev 3
        return VISION968;
    case 0x8813:        // 86c764_3 [Trio 32/64 vers 3]
        return TRIO64;  // correct?
    case 0x8811:        // 86c764/765 [Trio32/64/64V+]
        return revision & 0x40 ? TRIO64PLUS : TRIO64;
    case 0x8812:  // 86CM65 Aurora64V+
    case 0x8814:  // 86c767 [Trio 64UV+]
        return TRIO64PLUS;
    case 0x8900:  // 86c755 [Trio 64V2/DX]
    case 0x8901:  // 86c775/86c785 [Trio 64V2/DX or /GX]
        return TRIO64V2;
    case 0x8905:  // Trio 64V+ family
    case 0x8906:  // Trio 64V+ family
    case 0x8907:  // Trio 64V+ family
    case 0x8908:  // Trio 64V+ family
    case 0x8909:  // Trio 64V+ family
    case 0x890a:  // Trio 64V+ family
    case 0x890b:  // Trio 64V+ family
    case 0x890c:  // Trio 64V+ family
    case 0x890d:  // Trio 64V+ family
    case 0x890e:  // Trio 64V+ family
    case 0x890f:  // Trio 64V+ family
        return TRIO64PLUS;
    default:
        DFUNC(WARN, "Unknown chip family, aborting\n");
        return UNKNOWN;
    }
}

const char *getChipFamilyName(ChipFamily_t family)
{
    switch (family) {
    case VISION864:
        return "Vision864";
    case VISION968:
        return "Vision968";
    case TRIO64:
        return "Trio32/64";
    case TRIO64PLUS:
        return "Trio64+";
    case TRIO64V2:
        return "Trio64V2";
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

    if (chip->chipFamily == UNKNOWN) {
        DFUNC(ERROR, "Unknown Chip family\n");
        return FALSE;
    }
    D(INFO, "LegacyIOBase 0x%lx , MemoryBase 0x%lx, MemorySize %ld\n", legacyIOBase, memory0, memory0Size);
    APTR physicalAddress = pci_logic_to_physic_addr(memory0, card->board);
    D(INFO, "physicalAddress 0x%08lx\n", physicalAddress);

    card->legacyIOBase = (UBYTE *)legacyIOBase + REGISTER_OFFSET;
    if (chip->chipFamily >= VISION968) {
        // The Trio64
        // S3Trio64.chip expects register base adress to be offset by 0x8000
        // to be able to address all registers with just regular signed 16bit
        // offsets
        bi->RegisterBase = (UBYTE *)legacyIOBase + REGISTER_OFFSET;
        // Point to LE MMIO registers at offset 16MB. The chip driver will adjust to BE if BIGENDIAN_MMIO is enabled
        bi->MemoryIOBase = (UBYTE *)memory0 + 0x1000000 + MMIOREGISTER_OFFSET;
        // No need to fudge with the base address here
        bi->MemoryBase = (UBYTE *)memory0;
    } else {
        bi->RegisterBase = (UBYTE *)legacyIOBase + REGISTER_OFFSET;
        // This is how I understand Trio64/32 MMIO approach: 0xA0000 is
        // hardcoded as the base of the enhanced registers I need to make
        // sure, the first 1 MB of address space don't overlap with anything.
        bi->MemoryIOBase = (UBYTE *)pci_physic_to_logic_addr((APTR)0xA0000, card->board);

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
        if (pci_logic_to_physic_addr(bi->MemoryBase, card->board) <= (APTR)0xB0000) {
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

    // enable special cache mode settings(?)
    bi->MemorySpaceBase = memory0;
    bi->MemorySpaceSize = memory0Size;
    bi->Flags |= BIF_CACHEMODECHANGE;

    return TRUE;
}
