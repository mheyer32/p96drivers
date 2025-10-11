#include "s3trio64_common.h"

#include "common.h"

ChipFamily_t getChipFamily(UWORD deviceId, UWORD revision)
{
    switch (deviceId) {
    case 0x88C0:  // 86c864 Vision 864
    case 0x88C1:  // 86c864 Vision 864
        return VISION864;
    case 0x8813:        // 86c764_3 [Trio 32/64 vers 3]
        return TRIO64;  // correct?
    case 0x8811:        // 86c764/765 [Trio32/64/64V+]
        return revision & 0x40 ? TRIO64PLUS : TRIO64;
    case 0x8812:  // 86CM65 Aurora64V+
    case 0x8814:  // 86c767 [Trio 64UV+]
    case 0x8900:  // 86c755 [Trio 64V2/DX]
    case 0x8901:  // 86c775/86c785 [Trio 64V2/DX or /GX]
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
    case TRIO64:
        return "Trio32/64";
    case TRIO64PLUS:
        return "Trio64Plus";
    case UNKNOWN:
    default:
        return "Unknown";
    }
}
