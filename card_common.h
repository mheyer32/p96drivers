#ifndef CARD_COMMON_H
#define CARD_COMMON_H

#include "common.h"
#include <exec/types.h>

/**
 * Parse a hexadecimal or decimal string to ULONG value
 * Supports both "0x1234" (hex) and "1234" (decimal) formats
 * Uses manual parsing to avoid dependencies on library functions
 */
ULONG parseHexOrDecimal(CONST_STRPTR str);

/**
 * Parse tooltypes for card-specific settings
 * @param bi BoardInfo pointer (for debug logging)
 * @param ToolTypes Array of tooltype strings
 * @param deviceId Output parameter for DEVICEID tooltype (can be NULL)
 * @param vendorId Output parameter for VENDORID tooltype (can be NULL)
 * @param slot Output parameter for SLOT tooltype (can be NULL)
 * @param bus Output parameter for BUS tooltype (can be NULL)
 * @return TRUE on success, FALSE on failure
 */
BOOL parseToolTypes(struct BoardInfo *bi, CONST_STRPTR *ToolTypes, ULONG *deviceId, ULONG *vendorId, ULONG *slot,
                    ULONG *bus);

/**
 * Generate a unique board name based on card name, bus, and slot
 * Format: "CardName_B_S" where B is bus number and S is slot number
 * @param boardName Output buffer (must be at least 16 bytes)
 * @param cardName Base card name (e.g., "S3Trio64" or "ATIMach64")
 * @param bus Bus number
 * @param slot Slot number
 */
void generateBoardName(char *boardName, const char *cardName, ULONG bus, ULONG slot);

#endif  // CARD_COMMON_H

