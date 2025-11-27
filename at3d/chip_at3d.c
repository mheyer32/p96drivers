#include "chip_at3d.h"
#include "at3d_i2c.h"
#include "edid_common.h"

#define __NOLIBBASE__

#include <clib/debug_protos.h>
#include <debuglib.h>
#include <exec/types.h>

#define OPENPCI_SWAP  // don't make it define its own SWAP macros
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/exec.h>
#include <proto/openpci.h>

#define MIN_OPENPCI_VERSION 3  // Version 3 or more

#include <SDI_stdarg.h>

#ifdef DBG
int debugLevel = VERBOSE;
#endif

/******************************************************************************/
/*                                                                            */
/* library exports                                                            */
/*                                                                            */
/******************************************************************************/

#ifndef TESTEXE
const char LibName[]     = "AT3D.chip";
const char LibIdString[] = "Alliance ProMotion AT3D Picasso96 chip driver version 1.0";

const UWORD LibVersion  = 1;
const UWORD LibRevision = 0;
#endif

// Helper function to probe framebuffer memory size
static ULONG probeFramebufferSize(BoardInfo_t *bi)
{
    LOCAL_SYSBASE();

    DFUNC(INFO, "Probing framebuffer memory size...\n");

    volatile UBYTE *memBase = (volatile UBYTE *)bi->MemoryBase;
    ULONG maxSize           = 4 * 1024 * 1024;

    // Test pattern for memory probing
    ULONG testOffset  = 0;

    // Try to find memory boundary by testing at power-of-2 offsets
    for (ULONG size = 1 * 1024 * 1024; size <= maxSize; size *= 2) {
        testOffset = size - 32768 - 4;  // Test near the boundary

        // Save original values at test locations
        ULONG original = *(volatile ULONG *)(memBase + testOffset);
        ULONG originalAtHalf = *(volatile ULONG *)(memBase + size / 2);
        ULONG prevSize = size / 2;
        
        // Use a unique test pattern based on the test offset to detect wraparound
        // If memory wraps, writing at testOffset might appear at a different location
        ULONG uniquePattern = (ULONG)memBase + testOffset;  // Make pattern unique to this offset

        // Write unique pattern at test offset
        *(volatile ULONG *)(memBase + testOffset) = uniquePattern;
        // Write unique pattern at start of current segment
        *(volatile ULONG *)(memBase + size / 2) = uniquePattern;

        CacheClearU();

        // Read back from test offset
        ULONG readback = *(volatile ULONG *)(memBase + testOffset);
        ULONG readback0 = *(volatile ULONG *)(memBase + size / 4);

        // Restore original values
        *(volatile ULONG *)(memBase + testOffset) = original;
        *(volatile ULONG *)(memBase + size / 2) = originalAtHalf;
        CacheClearU();

        if (readback0 == uniquePattern) {
            // Wraparound detected - pattern written at testOffset or size/2 appeared at size/4
            DFUNC(INFO, "Memory wraparound detected, returning size: %ld KB\n", prevSize / 1024);
            return prevSize;  // Return previous size
        }
        if (readback != uniquePattern) {
            // Memory doesn't respond at this offset, we've found the boundary
            DFUNC(INFO, "Memory boundary detected returning size: %ld KB\n", prevSize / 1024);
            return prevSize;  // Return previous size
        }
    }

    // If we got here, use the maximum size we tested
    DFUNC(INFO, "Using maximum tested size: %ld KB\n", maxSize / 1024);
    return maxSize;
}

// Test register aperture (BAR1) access
static BOOL testRegisterAperture(BoardInfo_t *bi)
{
    DFUNC(INFO, "Testing register aperture access...\n");

    if (!bi->RegisterBase) {
        DFUNC(ERROR, "Register base is NULL\n");
        return FALSE;
    }

    REGBASE();

    // Test scratch pad register in sequencer registers (0x20-0x27)
    // Sequencer registers are accessed via SEQX (0x3C4) index and SEQ_DATA (0x3C5) data
    // Read current value from scratch pad register
    UBYTE original = R_SR(SR_SCRATCH_PAD);

    // Write test pattern to scratch pad register
    UBYTE testPattern = 0xAA;
    W_SR(SR_SCRATCH_PAD, testPattern);

    // Read back from scratch pad register
    UBYTE readback = R_SR(SR_SCRATCH_PAD);

    // Restore original value
    W_SR(SR_SCRATCH_PAD, original);

    DFUNC(INFO, "Scratch pad register (SR%02lx): wrote 0x%02lx, read 0x%02lx, original 0x%02lx\n",
          (ULONG)SR_SCRATCH_PAD, (ULONG)testPattern, (ULONG)readback, (ULONG)original);

    // Check if we can read back what we wrote
    if (readback == testPattern) {
        DFUNC(INFO, "Register test: PASSED - scratch pad register responds correctly\n");
        return TRUE;
    } else {
        DFUNC(ERROR, "Registertest: FAILED - scratch pad register readback mismatch\n");
        return FALSE;
    }
}

// Test MMVGA window (BAR0) access
static BOOL testMMIO(BoardInfo_t *bi)
{
    DFUNC(INFO, "Testing MMIO access...\n");

    LOCAL_OPENPCIBASE();

    // Get device ID from PCI configuration space
    ULONG pciDeviceId = 0;
    CardData_t *card  = getCardData(bi);
    if (!GetBoardAttrs(card->board, PRM_Device, (Tag)&pciDeviceId, TAG_END)) {
        DFUNC(ERROR, "Could not retrieve device ID from PCI configuration\n");
        return FALSE;
    }

    MMIOBASE();

    // MMVGA window maps PCI configuration space
    // Read device ID from MMVGA window at DEVICE_ID (memory offset 182-183h per AT3D documentation)
    UWORD deviceId = R_MMIO_W(DEVICE_ID);

    DFUNC(INFO, "Device ID from PCI config: 0x%04lx, from MMIO window: 0x%04lx\n", (ULONG)pciDeviceId, (ULONG)deviceId);

    // Compare device IDs
    if (deviceId == (UWORD)pciDeviceId) {
        DFUNC(INFO, "MMIO test: PASSED - device ID matches\n");
        return TRUE;
    } else {
        DFUNC(ERROR, "MMIO test: FAILED - device ID mismatch\n");
        return FALSE;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define LDEV_MASK (0x3 << 4)
#define LDEV(x)   ((x) << 4)

/**
 * Get I2C operations structure for EDID support
 * @param bi BoardInfo structure
 * @return Pointer to I2COps_t structure, or NULL if not initialized
 */
I2COps_t *getI2COps(struct BoardInfo *bi)
{
    CardData_t *card = getCardData(bi);
    return &card->i2cOps;
}

BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    DFUNC(ALWAYS, "AT3D InitChip - Testing hardware access\n");

    LOCAL_SYSBASE();

    ChipData_t *cd = getChipData(bi);

    {
        LEGACYIOBASE();
        // Unlock extended registers
        W_SR(0x10, 0x12);
        // Map HOST BLT port to last 32K of flat space less final 2K
        // Map ProMotion registers to  last 2K of flat space.
        W_SR_MASK(0x1b, 0x3F, (0b100 << 3) | (0b100));
        // Enable flat memory access, set aperture to 4MB and disable VGA memory (A000:0–BFFF:F) access
        W_SR_MASK(0x1c, 0x3F, 0b001101);
    }

    MMIOBASE();

    if (getChipData(bi)->chipFamily >= AT24)
    {
        // Enable extended registers, disable classic VGA IO range,
        // enable coprocessor aperture, enable second linear aperture
        W_MMIO_B(ENABLE_EXT_REGS, 0x0E);
        // Doc say about the MMVGA address space:
        // LDEV wait states register field (0xD9[5:4]) must be programmed to value 2 in order to access this space.
        W_MMIO_MASK_W(EXTSIG_TIMING, LDEV_MASK, LDEV(2));
    }

    // Test MMVGA window (BAR0)
    BOOL mmioOK = testMMIO(bi);

    REGBASE();
    char chipId[10] = {0};
    for (int c = 0; c < 9; ++c) {
        chipId[c] = R_SR(0x11 + c);
    }

    D(INFO, "Chip ID: %s\n", chipId);

    // Test register aperture (BAR1)
    BOOL regApertureOK = testRegisterAperture(bi);

    // Probe framebuffer memory size
    ULONG memSize  = probeFramebufferSize(bi);
    bi->MemorySize = memSize;

    // Initialize I2C operations for EDID support
    CardData_t *card = getCardData(bi);
    card->i2cOps.init     = at3dI2cInit;
    card->i2cOps.setScl   = at3dI2cSetScl;
    card->i2cOps.setSda   = at3dI2cSetSda;
    card->i2cOps.readScl  = at3dI2cReadScl;
    card->i2cOps.readSda  = at3dI2cReadSda;
    
    D(INFO, "I2C operations initialized for EDID support\n");

            // Try to read EDID from monitor (only for chips that support serial port register)
            // TRIO64PLUS and TRIO64V2 have the serial port register at MMIO offset 0xFF20
    UBYTE edid_data[EDID_BLOCK_SIZE];
    if (readEDID(bi, edid_data)) {
        char manufacturer[4];
        char product_name[14];

        getEDIDManufacturer(edid_data, manufacturer);
        D(INFO, "EDID: Manufacturer: %s\n", manufacturer);

        if (getEDIDProductName(edid_data, product_name)) {
            D(INFO, "EDID: Product Name: %s\n", product_name);
        }

        D(INFO, "EDID: Version %d.%d, Year: %d, Week: %d\n", edid_data[18], edid_data[19], edid_data[17] + 1990,
          edid_data[16]);
    } else {
        D(INFO, "EDID: Not available or read failed (monitor may not support EDID)\n");
    }

    // Set up basic BoardInfo structure
    bi->GraphicsControllerType = 0;  // Will need to define AT3D type
    bi->PaletteChipType        = 0;  // Will need to define AT3D type
    bi->Flags                  = 0;  // Minimal flags for now

    // Stub function pointers (minimal implementation)
    // These will be implemented in future phases

    return (regApertureOK || mmioOK) && (memSize > 0);
}

// Stub implementations for required functions
static void ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state)) {}

static void ASM SetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(BOOL border)) {}

static void ASM SetClock(__REGA0(struct BoardInfo *bi)) {}

static ULONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi),
                                   __REGD0(ULONG desiredPixelClock), __REGD1(RGBFTYPE rgbFormat))
{
    return 0;
}

static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD1(RGBFTYPE rgbFormat))
{
    return 0;
}

static UWORD ASM CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                      __REGD7(RGBFTYPE rgbFormat))
{
    return 0;
}

static APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi), __REGA1(APTR mem), __REGD7(RGBFTYPE format))
{
    return NULL;
}

static ULONG ASM GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    return 0;
}

static void ASM WaitBlitter(__REGA0(struct BoardInfo *bi)) {}

static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format)) {}

static void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format)) {}

static void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count)) {}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef TESTEXE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct UtilityBase *UtilityBase;

static struct BoardInfo boardInfo  = {0};
static struct Library *OpenPciBase = NULL;

void sigIntHandler(int dummy)
{
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
        OpenPciBase = NULL;
    }
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

    D(INFO, "AT3D Test Executable\n");
    D(INFO, "UtilityBase 0x%lx\n", bi->UtilBase);

    // Open openpci library
    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        DFUNC(ERROR, "Cannot open openpci.library v%ld+\n", MIN_OPENPCI_VERSION);
        goto exit;
    }

    // Find AT3D card
    struct pci_dev *board = NULL;
    CardData_t *card      = getCardData(bi);
    card->OpenPciBase     = OpenPciBase;

    D(INFO, "Looking for Alliance Promotion card, Vendor ID " STRINGIFY(VENDOR_ID_ALLIANCE) "\n");

    board = FindBoard(board, PRM_Vendor, VENDOR_ID_ALLIANCE, TAG_END);

    if (!board) {
        DFUNC(ERROR, "No card found\n");
        goto exit;
    }

    D(INFO, "Alliance Promotion card found\n");
    card->board = board;

    // Initialize register and memory bases
    if (!initRegisterAndMemoryBases(bi)) {
        DFUNC(ERROR, "Failed to initialize register and memory bases\n");
        goto exit;
    }

    // Test chip initialization
    D(INFO, "Calling InitChip...\n");
    if (!InitChip(bi)) {
        DFUNC(ERROR, "InitChip failed\n");
        goto exit;
    }

    D(INFO, "Alliance Promotion test completed successfully\n");
    rval = EXIT_SUCCESS;

exit:
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
    }
    return rval;
}

#endif  // TESTEXE
