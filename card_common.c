#include "card_common.h"

#include <clib/debug_protos.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/utility.h>

ULONG parseHexOrDecimal(CONST_STRPTR str)
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

BOOL parseToolTypes(struct BoardInfo *bi, CONST_STRPTR *ToolTypes, ULONG *deviceId, ULONG *vendorId, ULONG *slot,
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

void generateBoardName(char *boardName, const char *cardName, ULONG bus, ULONG slot)
{
    // Format: "CardName_B_S" where B is bus and S is slot
    // We need to construct: cardName + "_" + bus + "_" + slot
    // For simplicity, we'll use a template approach similar to S3Trio64
    const char template[] = "_B_S";
    int i = 0;
    
    // Copy card name
    while (cardName[i] && i < 12) {  // Leave room for "_B_S" (4 chars) + null terminator
        boardName[i] = cardName[i];
        i++;
    }
    
    // Append template with bus and slot
    int j = 0;
    while (template[j] && (i + j) < 15) {
        if (template[j] == 'B') {
            boardName[i + j] = '0' + bus;
        } else if (template[j] == 'S') {
            boardName[i + j] = '0' + slot;
        } else {
            boardName[i + j] = template[j];
        }
        j++;
    }
    boardName[i + j] = '\0';
}

