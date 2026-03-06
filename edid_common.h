#ifndef EDID_COMMON_H
#define EDID_COMMON_H

#include "common.h"

// EDID I2C addresses
#define EDID_I2C_ADDR_PRIMARY  0x50  // Primary EDID block (and extension blocks via paging)
// Note: EDID 1.3+ uses paging on the same I2C address (0x50) for extension blocks.
// Some older/non-standard implementations may use separate addresses (0x51, 0x52, etc.),
// but the standard method (which we use) is paging on 0x50.

// EDID block size
#define EDID_BLOCK_SIZE 128

// Low-level I2C operations interface
// This abstracts only the register-level SDA/SCL manipulation
// The I2C protocol (start, stop, bit banging) is implemented in common code
typedef struct I2COps
{
    BOOL (*init)(struct BoardInfo *bi);
    void (*setScl)(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching);
    void (*setSda)(struct BoardInfo *bi, BOOL high);
    BOOL (*readScl)(struct BoardInfo *bi);
    BOOL (*readSda)(struct BoardInfo *bi);
} I2COps_t;

// EDID structure (basic fields)
typedef struct EDIDData
{
    UBYTE header[8];        // EDID header (should be 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00)
    UBYTE manufacturer[2];  // Manufacturer ID (bytes 8-9)
    UBYTE product[2];       // Product ID (bytes 10-11)
    UBYTE serial[4];        // Serial number (bytes 12-15)
    UBYTE week;             // Week of manufacture (byte 16)
    UBYTE year;             // Year of manufacture (byte 17) - year + 1990
    UBYTE version;          // EDID version (byte 18)
    UBYTE revision;         // EDID revision (byte 19)
    UBYTE video_input;      // Video input definition (byte 20)
    UBYTE max_h_size;       // Maximum horizontal image size (byte 21) - cm
    UBYTE max_v_size;       // Maximum vertical image size (byte 22) - cm
    UBYTE gamma;            // Display gamma (byte 23)
    UBYTE features;         // Feature support (byte 24)
    // ... timing and descriptor blocks follow ...
    UBYTE checksum;  // Checksum byte (byte 127)
} EDIDData_t;

// EDID timing information structure
typedef struct EDIDTiming
{
    UWORD width;           // Horizontal resolution in pixels
    UWORD height;          // Vertical resolution in pixels
    ULONG pixel_clock;     // Pixel clock in 10kHz units (e.g., 25175 = 251.75 MHz)
    UWORD h_total;         // Total horizontal pixels (active + blanking)
    UWORD v_total;         // Total vertical lines (active + blanking)
    UWORD h_sync_offset;   // Horizontal sync offset in pixels
    UWORD h_sync_width;    // Horizontal sync pulse width in pixels
    UWORD v_sync_offset;   // Vertical sync offset in lines
    UWORD v_sync_width;    // Vertical sync pulse width in lines
    UWORD image_width_mm;  // Image width in millimeters
    UWORD image_height_mm; // Image height in millimeters
    UBYTE refresh;         // Refresh rate in Hz
    UBYTE flags;           // Flags (interlaced, stereo, etc.)
} EDIDTiming_t;

// Function to get I2C operations
// Default implementation in edid_common.c returns NULL
// Card drivers must provide their own implementation that accesses their CardData structure
const I2COps_t *getI2COps(struct BoardInfo *bi);

// I2C protocol functions (common implementation using I2COps)
void i2cStart(struct BoardInfo *bi);
void i2cStop(struct BoardInfo *bi);
BOOL i2cWriteBit(struct BoardInfo *bi, UBYTE bit);
UBYTE i2cReadBit(struct BoardInfo *bi);
BOOL i2cWriteByte(struct BoardInfo *bi, UBYTE data);
UBYTE i2cReadByte(struct BoardInfo *bi, BOOL ack);

// EDID functions
BOOL readEDIDBlock(struct BoardInfo *bi, UBYTE *edid_data, UBYTE i2c_addr, UBYTE block_number);
BOOL validateEDID(const UBYTE *edid_data);
BOOL readEDID(struct BoardInfo *bi, UBYTE *edid_data);
void getEDIDManufacturer(const UBYTE *edid_data, char *name);
BOOL getEDIDProductName(const UBYTE *edid_data, char *name);
void queryEDIDResolutions(const UBYTE *edid_data);
BOOL parseEDIDDetailedTiming(const UBYTE *desc, EDIDTiming_t *timing);
void getEDIDMaxFrequencies(const UBYTE *edid_data, ULONG *max_h_freq, ULONG *max_v_freq);
BOOL writeEDIDToFile(struct BoardInfo *bi, const UBYTE *edid_data);
UBYTE readEDIDWithExtensions(struct BoardInfo *bi, UBYTE *edid_data, UBYTE max_blocks);

#endif  // EDID_COMMON_H


