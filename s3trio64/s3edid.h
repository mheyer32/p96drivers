#ifndef S3EDID_H
#define S3EDID_H

#include "chip_s3trio64.h"

// EDID I2C addresses
#define EDID_I2C_ADDR_PRIMARY  0x50  // Primary EDID block (and extension blocks via paging)
// Note: EDID 1.3+ uses paging on the same I2C address (0x50) for extension blocks.
// Some older/non-standard implementations may use separate addresses (0x51, 0x52, etc.),
// but the standard method (which we use) is paging on 0x50.

// EDID block size
#define EDID_BLOCK_SIZE 128

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

/**
 * Read EDID block from monitor
 * @param bi BoardInfo structure
 * @param edid_data Buffer to store EDID data (must be at least 128 bytes)
 * @param i2c_addr I2C address (typically 0x50 for primary EDID)
 * @param block_number Block number to read (0 = base block, 1+ = extension blocks)
 * @return TRUE if EDID was read successfully, FALSE otherwise
 */
BOOL readEDIDBlock(struct BoardInfo *bi, UBYTE *edid_data, UBYTE i2c_addr, UBYTE block_number);

/**
 * Validate EDID data
 * @param edid_data EDID data block to validate
 * @return TRUE if EDID is valid, FALSE otherwise
 */
BOOL validateEDID(const UBYTE *edid_data);

/**
 * Read EDID from monitor (main function)
 * @param bi BoardInfo structure
 * @param edid_data Buffer to store EDID data (must be at least 128 bytes)
 * @return TRUE if EDID was read and validated successfully, FALSE otherwise
 */
BOOL readEDID(struct BoardInfo *bi, UBYTE *edid_data);

/**
 * Get manufacturer name from EDID (3-letter code)
 * @param edid_data EDID data block
 * @param name Buffer to store manufacturer name (must be at least 4 bytes)
 */
void getEDIDManufacturer(const UBYTE *edid_data, char *name);

/**
 * Get product name from EDID (if available in descriptor blocks)
 * @param edid_data EDID data block
 * @param name Buffer to store product name (must be at least 14 bytes)
 * @return TRUE if product name was found, FALSE otherwise
 */
BOOL getEDIDProductName(const UBYTE *edid_data, char *name);

/**
 * Parse and display all supported resolutions and frequencies from EDID
 * @param edid_data EDID data block
 */
void queryEDIDResolutions(const UBYTE *edid_data);

/**
 * Parse detailed timing descriptor (18 bytes)
 * @param desc Pointer to 18-byte descriptor block
 * @param timing Output structure for timing information
 * @return TRUE if valid timing descriptor, FALSE otherwise
 */
BOOL parseEDIDDetailedTiming(const UBYTE *desc, EDIDTiming_t *timing);

/**
 * Calculate maximum horizontal and vertical frequencies from EDID
 * @param edid_data EDID data block
 * @param max_h_freq Output: Maximum horizontal frequency in kHz
 * @param max_v_freq Output: Maximum vertical frequency in Hz
 */
void getEDIDMaxFrequencies(const UBYTE *edid_data, ULONG *max_h_freq, ULONG *max_v_freq);

/**
 * Write EDID binary data to a file in RAM: using monitor name
 * @param bi BoardInfo structure (for ExecBase access)
 * @param edid_data EDID data block (128 bytes)
 * @return TRUE if file was written successfully, FALSE otherwise
 */
BOOL writeEDIDToFile(struct BoardInfo *bi, const UBYTE *edid_data);

/**
 * Read all EDID blocks including extensions
 * @param bi BoardInfo structure
 * @param edid_data Buffer to store EDID data (must be at least 128 * (1 + num_extensions) bytes)
 * @param max_blocks Maximum number of blocks to read (including base block)
 * @return Number of blocks successfully read (0 on failure)
 */
UBYTE readEDIDWithExtensions(struct BoardInfo *bi, UBYTE *edid_data, UBYTE max_blocks);

#endif  // S3EDID_H
