#include "edid_common.h"
#include "common.h"
#include <proto/dos.h>
#include <proto/utility.h>

// I2C timing delays (standard I2C: 100kHz)
#define I2C_DELAY_US 5  // 5 microseconds for standard I2C timing

// getI2COps() is declared in edid_common.h as extern
// Each card driver must provide its own implementation in card_*.c
// that properly accesses their CardData->i2cOps field

/**
 * Generate I2C start condition
 * SDA goes from high to low while SCL is high
 * @param bi BoardInfo structure
 */
void i2cStart(struct BoardInfo *bi)
{
    I2COps_t *ops = getI2COps(bi);
    if (!ops) {
        DFUNC(ERROR, "I2C ops not initialized\n");
        return;
    }

    // Ensure SDA and SCL are released (high)
    ops->setSda(bi, TRUE);
    ops->setScl(bi, TRUE, FALSE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Start condition: SDA goes low while SCL is high
    ops->setSda(bi, FALSE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Pull SCL low to prepare for data transfer
    ops->setScl(bi, FALSE, FALSE);
    delayMicroSeconds(I2C_DELAY_US);
}

/**
 * Generate I2C stop condition
 * SDA goes from low to high while SCL is high
 * @param bi BoardInfo structure
 */
void i2cStop(struct BoardInfo *bi)
{
    I2COps_t *ops = getI2COps(bi);
    if (!ops) {
        DFUNC(ERROR, "I2C ops not initialized\n");
        return;
    }

    // Ensure SDA is low and SCL is low
    ops->setSda(bi, FALSE);
    ops->setScl(bi, FALSE, FALSE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Pull SCL high first (check for clock stretching)
    ops->setScl(bi, TRUE, TRUE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Stop condition: SDA goes high while SCL is high
    ops->setSda(bi, TRUE);
    delayMicroSeconds(I2C_DELAY_US);
}

/**
 * Write a single bit on I2C bus
 * @param bi BoardInfo structure
 * @param bit Bit value to write (0 or 1)
 * @return TRUE if successful
 */
BOOL i2cWriteBit(struct BoardInfo *bi, UBYTE bit)
{
    I2COps_t *ops = getI2COps(bi);
    if (!ops) {
        DFUNC(ERROR, "I2C ops not initialized\n");
        return FALSE;
    }

    // Set SDA to desired value while SCL is low
    ops->setSda(bi, bit != 0);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Clock the bit by pulling SCL high (check for clock stretching)
    ops->setScl(bi, TRUE, TRUE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Pull SCL low to complete the bit transfer (no clock stretching check)
    ops->setScl(bi, FALSE, FALSE);
    delayMicroSeconds(I2C_DELAY_US);
    
    return TRUE;
}

/**
 * Read a single bit from I2C bus
 * @param bi BoardInfo structure
 * @return Bit value (0 or 1)
 */
UBYTE i2cReadBit(struct BoardInfo *bi)
{
    I2COps_t *ops = getI2COps(bi);
    if (!ops) {
        DFUNC(ERROR, "I2C ops not initialized\n");
        return 0;
    }

    // Release SDA (set to input/tri-state)
    ops->setSda(bi, TRUE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Clock the bit by pulling SCL high (check for clock stretching)
    ops->setScl(bi, TRUE, TRUE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Read SDA value while SCL is high
    UBYTE bit = ops->readSda(bi) ? 1 : 0;
    
    // Pull SCL low to complete the bit transfer (no clock stretching check)
    ops->setScl(bi, FALSE, FALSE);
    delayMicroSeconds(I2C_DELAY_US);
    
    return bit;
}

/**
 * Write a byte on I2C bus and check for ACK
 * @param bi BoardInfo structure
 * @param data Byte to write
 * @return TRUE if ACK received, FALSE if NACK
 */
BOOL i2cWriteByte(struct BoardInfo *bi, UBYTE data)
{
    // Write 8 bits, MSB first
    for (int i = 7; i >= 0; i--) {
        i2cWriteBit(bi, (data >> i) & 1);
    }
    
    // Read ACK bit (slave pulls SDA low for ACK)
    UBYTE ack = i2cReadBit(bi);
    
    return (ack == 0);  // ACK is low (0), NACK is high (1)
}

/**
 * Read a byte from I2C bus
 * @param bi BoardInfo structure
 * @param ack TRUE to send ACK, FALSE to send NACK
 * @return Byte read from bus
 */
UBYTE i2cReadByte(struct BoardInfo *bi, BOOL ack)
{
    UBYTE data = 0;
    
    // Read 8 bits, MSB first
    for (int i = 7; i >= 0; i--) {
        UBYTE bit = i2cReadBit(bi);
        data |= (bit << i);
    }
    
    // Send ACK or NACK
    i2cWriteBit(bi, ack ? 0 : 1);  // 0 = ACK, 1 = NACK
    
    return data;
}

/**
 * Read EDID block from monitor via I2C
 * @param bi BoardInfo structure
 * @param edid_data Buffer to store EDID data (must be at least 128 bytes)
 * @param i2c_addr I2C address (typically 0x50 for primary EDID)
 * @param block_number Block number to read (0 = base block, 1+ = extension blocks)
 * @return TRUE if EDID was read successfully, FALSE otherwise
 */
BOOL readEDIDBlock(struct BoardInfo *bi, UBYTE *edid_data, UBYTE i2c_addr, UBYTE block_number)
{
    if (!bi || !edid_data) {
        DFUNC(ERROR, "Invalid parameters\n");
        return FALSE;
    }

    I2COps_t *ops = getI2COps(bi);
    if (!ops) {
        DFUNC(ERROR, "I2C ops not initialized\n");
        return FALSE;
    }

    // Initialize I2C bus
    if (!ops->init(bi)) {
        DFUNC(ERROR, "Failed to initialize I2C bus\n");
        return FALSE;
    }

    // Generate start condition
    i2cStart(bi);

    // Send I2C address with write bit (R/W = 0)
    UBYTE i2c_write_addr = (i2c_addr << 1) | 0;
    if (!i2cWriteByte(bi, i2c_write_addr)) {
        DFUNC(ERROR, "No ACK for I2C address 0x%02lx (write)\n", (ULONG)i2c_addr);
        i2cStop(bi);
        return FALSE;
    }

    // For extension blocks (block_number > 0), we need to set the page/block number
    // EDID paging: write block number to offset 0x00 to select which block to read
    if (block_number > 0) {
        if (!i2cWriteByte(bi, block_number)) {
            DFUNC(ERROR, "No ACK for EDID block number %lu\n", (ULONG)block_number);
            i2cStop(bi);
            return FALSE;
        }
    } else {
        // For base block (block 0), send register address 0x00
        if (!i2cWriteByte(bi, 0x00)) {
            DFUNC(ERROR, "No ACK for EDID register address\n");
            i2cStop(bi);
            return FALSE;
        }
    }

    // Generate repeated start condition
    i2cStart(bi);

    // Send I2C address with read bit (R/W = 1)
    UBYTE i2c_read_addr = (i2c_addr << 1) | 1;
    if (!i2cWriteByte(bi, i2c_read_addr)) {
        DFUNC(ERROR, "No ACK for I2C address 0x%02lx (read)\n", (ULONG)i2c_addr);
        i2cStop(bi);
        return FALSE;
    }

    // Read 128 bytes
    // ACK all bytes except the last one (which gets NACK)
    for (int i = 0; i < EDID_BLOCK_SIZE; i++) {
        BOOL ack     = (i < EDID_BLOCK_SIZE - 1);  // ACK all except last
        edid_data[i] = i2cReadByte(bi, ack);
    }

    // Generate stop condition
    i2cStop(bi);

    D(INFO, "EDID block %lu read successfully from I2C address 0x%02lx\n", (ULONG)block_number, (ULONG)i2c_addr);
    return TRUE;
}

/**
 * Validate EDID data
 * @param edid_data EDID data block to validate
 * @return TRUE if EDID is valid, FALSE otherwise
 */
BOOL validateEDID(const UBYTE *edid_data)
{
    if (!edid_data) {
        return FALSE;
    }

    // Check EDID header (first 8 bytes should be: 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00)
    if (edid_data[0] != 0x00 || edid_data[7] != 0x00) {
        DFUNC(ERROR, "EDID header invalid: 0x%02lx 0x%02lx\n", (ULONG)edid_data[0], (ULONG)edid_data[7]);
        return FALSE;
    }

    for (int i = 1; i < 7; i++) {
        if (edid_data[i] != 0xFF) {
            DFUNC(ERROR, "EDID header byte %ld invalid: 0x%02lx\n", i, (ULONG)edid_data[i]);
            return FALSE;
        }
    }

    // Calculate checksum (sum of all 128 bytes should be 0 mod 256)
    UBYTE checksum = 0;
    for (int i = 0; i < EDID_BLOCK_SIZE; i++) {
        checksum += edid_data[i];
    }

    if (checksum != 0) {
        DFUNC(ERROR, "EDID checksum invalid: 0x%02lx (should be 0x00)\n", checksum);
        return FALSE;
    }

    D(INFO, "EDID validation passed\n");
    return TRUE;
}

/**
 * Read EDID from monitor (main function)
 * @param bi BoardInfo structure
 * @param edid_data Buffer to store EDID data (must be at least 128 bytes)
 * @return TRUE if EDID was read and validated successfully, FALSE otherwise
 */
BOOL readEDID(struct BoardInfo *bi, UBYTE *edid_data)
{
    if (!bi || !edid_data) {
        DFUNC(ERROR, "Invalid parameters\n");
        return FALSE;
    }

#ifdef DBG
    // Save current debug level and temporarily set to INFO
    // to avoid spamming console with verbose messages
    int saved_debug_level = debugLevel;
    debugLevel            = INFO;
#endif

    // Try to read primary EDID block (address 0x50, block 0)
    if (!readEDIDBlock(bi, edid_data, EDID_I2C_ADDR_PRIMARY, 0)) {
        DFUNC(ERROR, "Failed to read primary EDID block\n");
#ifdef DBG
        debugLevel = saved_debug_level;
#endif
        return FALSE;
    }

    for (int i = 0; i < EDID_BLOCK_SIZE; i++) {
        D(VERBOSE, "EDID[%02d]: 0x%02lx\n", i, (ULONG)edid_data[i]);
    }

    // Validate EDID
    if (!validateEDID(edid_data)) {
        DFUNC(ERROR, "EDID validation failed\n");
#ifdef DBG
        debugLevel = saved_debug_level;
#endif
        return FALSE;
    }

    D(INFO, "EDID read and validated successfully\n");

    // Query and display supported resolutions
    queryEDIDResolutions(edid_data);

    // Write EDID data to file in RAM:
    writeEDIDToFile(bi, edid_data);

#ifdef DBG
    // Restore original debug level
    debugLevel = saved_debug_level;
#endif

    return TRUE;
}

/**
 * Get manufacturer name from EDID (3-letter code)
 * @param edid_data EDID data block
 * @param name Buffer to store manufacturer name (must be at least 4 bytes)
 */
void getEDIDManufacturer(const UBYTE *edid_data, char *name)
{
    if (!edid_data || !name) {
        return;
    }

    // Manufacturer ID is stored in bytes 8-9 as:
    // Byte 8: bits 7-2 = first letter (A=1, B=2, ..., Z=26)
    // Byte 9: bits 7-2 = second letter, bits 1-0 = third letter (bits 7-2 of byte 8)
    // Actually, it's a 10-bit value: bits 14-10 from byte 8, bits 9-5 from byte 9, bits 4-0 from byte 9

    // Simplified: bytes 8-9 form a 16-bit value
    // First letter: ((byte8 >> 2) & 0x1F) - 1 + 'A'
    // Second letter: ((byte8 & 0x03) << 3) | ((byte9 >> 5) & 0x07) - 1 + 'A'
    // Third letter: (byte9 & 0x1F) - 1 + 'A'

    UBYTE byte8 = edid_data[8];
    UBYTE byte9 = edid_data[9];

    name[0] = ((byte8 >> 2) & 0x1F) - 1 + 'A';
    name[1] = (((byte8 & 0x03) << 3) | ((byte9 >> 5) & 0x07)) - 1 + 'A';
    name[2] = (byte9 & 0x1F) - 1 + 'A';
    name[3] = '\0';
}

/**
 * Get product name from EDID (if available in descriptor blocks)
 * @param edid_data EDID data block
 * @param name Buffer to store product name (must be at least 14 bytes)
 * @return TRUE if product name was found, FALSE otherwise
 */
BOOL getEDIDProductName(const UBYTE *edid_data, char *name)
{
    if (!edid_data || !name) {
        return FALSE;
    }

    // Descriptor blocks start at byte 54
    // Each descriptor is 18 bytes
    // Descriptor type 0xFC is product name
    for (int i = 0; i < 4; i++) {
        int offset = 54 + (i * 18);
        if (edid_data[offset] == 0x00 && edid_data[offset + 1] == 0x00 && edid_data[offset + 2] == 0x00 &&
            edid_data[offset + 3] == 0xFC) {
            // Found product name descriptor
            // Bytes 5-18 contain the name (up to 13 characters, null-terminated)
            int j;
            for (j = 0; j < 13; j++) {
                UBYTE c = edid_data[offset + 5 + j];
                if (c == 0x0A) {  // Line feed terminates the string
                    name[j] = '\0';
                    break;
                }
                name[j] = c;
            }
            if (j == 13) {
                name[13] = '\0';
            }
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Parse detailed timing descriptor (18 bytes)
 * @param desc Pointer to 18-byte descriptor block
 * @param timing Output structure for timing information
 * @return TRUE if valid timing descriptor, FALSE otherwise
 */
BOOL parseEDIDDetailedTiming(const UBYTE *desc, EDIDTiming_t *timing)
{
    if (!desc || !timing) {
        return FALSE;
    }

    // Check if this is a detailed timing descriptor (first two bytes not both zero)
    if (desc[0] == 0 && desc[1] == 0) {
        return FALSE;  // Not a timing descriptor
    }

    // Byte 0-1: Pixel clock (in 10kHz units)
    timing->pixel_clock = desc[0] | ((ULONG)desc[1] << 8);

    // Byte 2, 4: Horizontal active pixels
    timing->width = desc[2] | ((desc[4] & 0xF0) << 4);

    // Byte 3, 4: Horizontal blanking
    UWORD h_blank = desc[3] | ((desc[4] & 0x0F) << 8);

    // Byte 5, 7: Vertical active lines
    timing->height = desc[5] | ((desc[7] & 0xF0) << 4);

    // Byte 6, 7: Vertical blanking
    UWORD v_blank = desc[6] | ((desc[7] & 0x0F) << 8);

    // Calculate totals
    timing->h_total = timing->width + h_blank;
    timing->v_total = timing->height + v_blank;

    // Byte 8, 11: Horizontal sync offset and width
    timing->h_sync_offset = desc[8] | ((desc[11] & 0xC0) << 2);
    timing->h_sync_width  = desc[9] | ((desc[11] & 0x30) << 4);

    // Byte 9, 10, 11: Vertical sync offset and width
    timing->v_sync_offset = (desc[10] & 0xF0) >> 4 | ((desc[11] & 0x0C) << 2);
    timing->v_sync_width  = (desc[10] & 0x0F) | ((desc[11] & 0x03) << 4);

    // Byte 12-13: Image width in millimeters
    timing->image_width_mm = desc[12] | ((desc[13] & 0xF0) << 4);

    // Byte 14-15: Image height in millimeters
    timing->image_height_mm = desc[14] | ((desc[15] & 0xF0) << 4);

    // Byte 16-17: Border and flags
    // Byte 17 bits:
    //   bit 7: Interlaced
    //   bit 6-5: Stereo mode
    //   bit 4-3: Digital sync (if set)
    //   bit 2: VSync polarity (0=negative, 1=positive)
    //   bit 1: HSync polarity (0=negative, 1=positive)
    //   bit 0: Reserved
    timing->flags = desc[17];

    // Calculate refresh rate
    // Refresh = (pixel_clock * 10000) / (h_total * v_total)
    if (timing->h_total > 0 && timing->v_total > 0) {
        ULONG refresh_hz = (timing->pixel_clock * 10000) / (timing->h_total * timing->v_total);
        timing->refresh  = (UBYTE)(refresh_hz > 255 ? 255 : refresh_hz);
    } else {
        timing->refresh = 0;
    }

    return TRUE;
}

/**
 * Parse and display established timings (bytes 35-37)
 * @param edid_data EDID data block
 */
static void parseEDIDEstablishedTimings(const UBYTE *edid_data)
{
    // Established timings bitmap (bytes 35-37)
    UBYTE est1 = edid_data[35];
    UBYTE est2 = edid_data[36];
    UBYTE est3 = edid_data[37];

    D(INFO, "Established Timings:\n");

    // Byte 35: Standard timings
    if (est1 & 0x80)
        D(INFO, "  800x600@60Hz\n");
    if (est1 & 0x40)
        D(INFO, "  800x600@56Hz\n");
    if (est1 & 0x20)
        D(INFO, "  640x480@75Hz\n");
    if (est1 & 0x10)
        D(INFO, "  640x480@72Hz\n");
    if (est1 & 0x08)
        D(INFO, "  640x480@67Hz\n");
    if (est1 & 0x04)
        D(INFO, "  640x480@60Hz\n");
    if (est1 & 0x02)
        D(INFO, "  720x400@88Hz\n");
    if (est1 & 0x01)
        D(INFO, "  720x400@70Hz\n");

    // Byte 36: Standard timings
    if (est2 & 0x80)
        D(INFO, "  1280x1024@75Hz\n");
    if (est2 & 0x40)
        D(INFO, "  1024x768@75Hz\n");
    if (est2 & 0x20)
        D(INFO, "  1024x768@70Hz\n");
    if (est2 & 0x10)
        D(INFO, "  1024x768@60Hz\n");
    if (est2 & 0x08)
        D(INFO, "  1024x768@87Hz (interlaced)\n");
    if (est2 & 0x04)
        D(INFO, "  832x624@75Hz\n");
    if (est2 & 0x02)
        D(INFO, "  800x600@75Hz\n");
    if (est2 & 0x01)
        D(INFO, "  800x600@72Hz\n");

    // Byte 37: Standard timings
    if (est3 & 0x80)
        D(INFO, "  1280x1024@60Hz\n");
    if (est3 & 0x40)
        D(INFO, "  1152x870@75Hz\n");
}

/**
 * Parse and display standard timings (bytes 38-53)
 * @param edid_data EDID data block
 */
static void parseEDIDStandardTimings(const UBYTE *edid_data)
{
    D(INFO, "Standard Timings:\n");

    // Standard timings are 8 entries of 2 bytes each (bytes 38-53)
    for (int i = 0; i < 8; i++) {
        int offset  = 38 + (i * 2);
        UBYTE byte1 = edid_data[offset];
        UBYTE byte2 = edid_data[offset + 1];

        // If both bytes are 0x01, this entry is unused
        if (byte1 == 0x01 && byte2 == 0x01) {
            continue;
        }

        // Calculate horizontal resolution: (byte1 + 31) * 8
        UWORD h_res = (byte1 + 31) * 8;

        // Calculate aspect ratio and vertical resolution
        UWORD v_res;
        const char *aspect;
        UBYTE aspect_code = (byte2 >> 6) & 0x03;
        switch (aspect_code) {
        case 0:  // 16:10
            v_res  = (h_res * 10) / 16;
            aspect = "16:10";
            break;
        case 1:  // 4:3
            v_res  = (h_res * 3) / 4;
            aspect = "4:3";
            break;
        case 2:  // 5:4
            v_res  = (h_res * 4) / 5;
            aspect = "5:4";
            break;
        case 3:  // 16:9
            v_res  = (h_res * 9) / 16;
            aspect = "16:9";
            break;
        default:
            v_res  = 0;
            aspect = "?";
            break;
        }

        // Calculate refresh rate: byte2 & 0x3F + 60
        UBYTE refresh = (byte2 & 0x3F) + 60;

        if (h_res > 0 && v_res > 0) {
            D(INFO, "  %ldx%ld@%ldHz (%s)\n", (ULONG)h_res, (ULONG)v_res, (ULONG)refresh, aspect);
        }
    }
}

/**
 * Parse and display all supported resolutions and frequencies from EDID
 * @param edid_data EDID data block
 */
void queryEDIDResolutions(const UBYTE *edid_data)
{
    if (!edid_data) {
        DFUNC(ERROR, "Invalid EDID data\n");
        return;
    }

    D(INFO, "=== EDID Supported Resolutions ===\n");

    // Parse established timings (bytes 35-37)
    parseEDIDEstablishedTimings(edid_data);

    // Parse standard timings (bytes 38-53)
    parseEDIDStandardTimings(edid_data);

    // Parse detailed timing descriptors (bytes 54-125, 4 blocks of 18 bytes)
    D(INFO, "Detailed Timings:\n");
    for (int i = 0; i < 4; i++) {
        int offset = 54 + (i * 18);
        EDIDTiming_t timing;

        if (parseEDIDDetailedTiming(&edid_data[offset], &timing)) {
            // Calculate refresh rate
            ULONG refresh_hz = 0;
            if (timing.h_total > 0 && timing.v_total > 0) {
                refresh_hz = (timing.pixel_clock * 10000) / (timing.h_total * timing.v_total);
            }

            // Calculate horizontal frequency (pixel clock / h_total)
            ULONG h_freq_khz = 0;
            if (timing.h_total > 0) {
                h_freq_khz = (timing.pixel_clock * 10) / timing.h_total;
            }

            // Pixel clock in 10kHz units, convert to MHz (divide by 100)
            ULONG pixel_clock_mhz_int  = timing.pixel_clock / 100;
            ULONG pixel_clock_mhz_frac = timing.pixel_clock % 100;

            const char *interlaced = (timing.flags & 0x80) ? " (interlaced)" : "";
            const char *hsync_pol  = (timing.flags & 0x02) ? "+" : "-";
            const char *vsync_pol  = (timing.flags & 0x04) ? "+" : "-";

            D(INFO, "  %ldx%ld @ %luHz%s (pixel clock: %lu.%02lu MHz, h-freq: %lu kHz, sync: H%c V%c)\n", (ULONG)timing.width,
              (ULONG)timing.height, refresh_hz, interlaced, pixel_clock_mhz_int, pixel_clock_mhz_frac, h_freq_khz,
              hsync_pol[0], vsync_pol[0]);

            if (timing.image_width_mm > 0 && timing.image_height_mm > 0) {
                D(INFO, "    Image size: %ld x %ld mm\n", (ULONG)timing.image_width_mm, (ULONG)timing.image_height_mm);
            }
        }
    }

    D(INFO, "=== End of EDID Resolutions ===\n");

    // Calculate and display maximum frequencies
    ULONG max_h_freq = 0;
    ULONG max_v_freq = 0;
    getEDIDMaxFrequencies(edid_data, &max_h_freq, &max_v_freq);

    if (max_h_freq > 0 || max_v_freq > 0) {
        D(INFO, "Maximum Frequencies:\n");
        if (max_h_freq > 0) {
            D(INFO, "  Maximum Horizontal Frequency: %lu kHz\n", max_h_freq);
        }
        if (max_v_freq > 0) {
            D(INFO, "  Maximum Vertical Frequency: %lu Hz\n", max_v_freq);
        }
    }
}

/**
 * Calculate maximum horizontal and vertical frequencies from EDID
 * @param edid_data EDID data block
 * @param max_h_freq Output: Maximum horizontal frequency in kHz
 * @param max_v_freq Output: Maximum vertical frequency in Hz
 */
void getEDIDMaxFrequencies(const UBYTE *edid_data, ULONG *max_h_freq, ULONG *max_v_freq)
{
    if (!edid_data || !max_h_freq || !max_v_freq) {
        return;
    }

    *max_h_freq = 0;
    *max_v_freq = 0;

    // Check all detailed timing descriptors
    for (int i = 0; i < 4; i++) {
        int offset = 54 + (i * 18);
        EDIDTiming_t timing;

        if (parseEDIDDetailedTiming(&edid_data[offset], &timing)) {
            // Calculate horizontal frequency: pixel_clock / h_total
            // pixel_clock is in 10kHz, so result is in kHz
            if (timing.h_total > 0) {
                ULONG h_freq_khz = (timing.pixel_clock * 10) / timing.h_total;
                if (h_freq_khz > *max_h_freq) {
                    *max_h_freq = h_freq_khz;
                }
            }

            // Calculate vertical frequency (refresh rate)
            if (timing.h_total > 0 && timing.v_total > 0) {
                ULONG v_freq_hz = (timing.pixel_clock * 10000) / (timing.h_total * timing.v_total);
                if (v_freq_hz > *max_v_freq) {
                    *max_v_freq = v_freq_hz;
                }
            }
        }
    }

    // Also check standard timings for maximum refresh rates
    for (int i = 0; i < 8; i++) {
        int offset  = 38 + (i * 2);
        UBYTE byte1 = edid_data[offset];
        UBYTE byte2 = edid_data[offset + 1];

        if (byte1 != 0x01 || byte2 != 0x01) {
            UBYTE refresh = (byte2 & 0x3F) + 60;
            if (refresh > *max_v_freq) {
                *max_v_freq = refresh;
            }
        }
    }

    // Check established timings for maximum refresh rates
    // (These are fixed resolutions, so we just check their refresh rates)
    UBYTE est1 = edid_data[35];
    UBYTE est2 = edid_data[36];
    UBYTE est3 = edid_data[37];

    // Established timings have fixed refresh rates, so we check them
    // Most are 60-75Hz, but we'll use the highest we find
    if (est1 || est2 || est3) {
        // Most established timings are 60-75Hz, so we'll use 75 as a conservative max
        // if any are set (actual max would require checking each bit)
        if (75 > *max_v_freq) {
            *max_v_freq = 75;
        }
    }
}

/**
 * Get string length
 * @param str String to measure
 * @return Length of string (excluding null terminator)
 */
static ULONG myStrlen(CONST_STRPTR str)
{
    ULONG len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/**
 * Sanitize a string to be used as a filename
 * Replaces invalid characters with underscores
 * @param name Input string
 * @param output Output buffer (must be at least as large as input)
 * @param max_len Maximum length of output
 */
static void sanitizeFilename(const char *name, char *output, int max_len)
{
    int i;
    for (i = 0; i < max_len - 1 && name[i] != '\0'; i++) {
        char c = name[i];
        // Replace invalid filename characters with underscore
        // Valid: letters, numbers, underscore, dash, dot
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
            c == '.') {
            output[i] = c;
        } else {
            output[i] = '_';
        }
    }
    output[i] = '\0';
}

/**
 * Write EDID binary data to a file in RAM: using monitor name
 * Reads all EDID blocks including extensions before writing
 * @param bi BoardInfo structure (for ExecBase access)
 * @param edid_data EDID base block (128 bytes) - used for monitor name only
 * @return TRUE if file was written successfully, FALSE otherwise
 */
BOOL writeEDIDToFile(struct BoardInfo *bi, const UBYTE *edid_data)
{
    if (!bi || !edid_data) {
        DFUNC(ERROR, "Invalid parameters\n");
        return FALSE;
    }

    LOCAL_SYSBASE();
    LOCAL_UTILITYBASE();

    // Open dos.library
    struct Library *DOSBase = OpenLibrary("dos.library", 0);
    if (!DOSBase) {
        DFUNC(ERROR, "Failed to open dos.library\n");
        return FALSE;
    }

    BOOL success = FALSE;

    // Get monitor name from EDID
    char monitor_name[64];
    char filename[128];
    char manufacturer[4];

    // Try to get product name first
    if (getEDIDProductName(edid_data, monitor_name)) {
        // Use product name
        sanitizeFilename(monitor_name, filename, sizeof(filename));
    } else {
        // Fall back to manufacturer code
        getEDIDManufacturer(edid_data, manufacturer);
        sanitizeFilename(manufacturer, filename, sizeof(filename));
    }

    // If filename is empty or too short, use a default name
    if (filename[0] == '\0' || myStrlen((STRPTR)filename) < 3) {
        Strncpy((STRPTR)filename, (STRPTR)"Unknown_Monitor", sizeof(filename));
    }

    // Construct full path: RAM:filename.edid
    char fullpath[256];
    Strncpy((STRPTR)fullpath, (STRPTR)"RAM:", sizeof(fullpath));
    Strncat((STRPTR)fullpath, (STRPTR)filename, sizeof(fullpath) - myStrlen((STRPTR)fullpath));
    Strncat((STRPTR)fullpath, (STRPTR)".edid", sizeof(fullpath) - myStrlen((STRPTR)fullpath));
    
    // Read all EDID blocks including extensions (max 4 blocks = 512 bytes)
    // This ensures we save complete EDID data including any extension blocks
    UBYTE all_edid_data[EDID_BLOCK_SIZE * 4];
    UBYTE blocks_read = readEDIDWithExtensions(bi, all_edid_data, 4);
    
    if (blocks_read == 0) {
        DFUNC(ERROR, "Failed to read EDID blocks for file writing\n");
        CloseLibrary(DOSBase);
        return FALSE;
    }
    
    ULONG total_bytes = blocks_read * EDID_BLOCK_SIZE;
    
    // Open file for writing (create new file)
    BPTR file = Open((STRPTR)fullpath, MODE_NEWFILE);
    if (file) {
        // Write all EDID blocks (base + extensions)
        LONG bytes_written = Write(file, (APTR)all_edid_data, total_bytes);
        if (bytes_written == (LONG)total_bytes) {
            D(INFO, "EDID data written to %s (%ld bytes, %lu block(s))\n", fullpath, bytes_written, (ULONG)blocks_read);
            success = TRUE;
        } else {
            DFUNC(ERROR, "Failed to write EDID data to %s (wrote %ld bytes, expected %ld)\n", fullpath, bytes_written,
                  total_bytes);
        }
        Close(file);
    } else {
        DFUNC(ERROR, "Failed to open file %s for writing\n", fullpath);
    }

    CloseLibrary(DOSBase);
    return success;
}

/**
 * Read all EDID blocks including extensions
 * @param bi BoardInfo structure
 * @param edid_data Buffer to store EDID data (must be at least 128 * (1 + num_extensions) bytes)
 * @param max_blocks Maximum number of blocks to read (including base block)
 * @return Number of blocks successfully read (0 on failure)
 */
UBYTE readEDIDWithExtensions(struct BoardInfo *bi, UBYTE *edid_data, UBYTE max_blocks)
{
    if (!bi || !edid_data || max_blocks == 0) {
        DFUNC(ERROR, "Invalid parameters\n");
        return 0;
    }

#ifdef DBG
    // Save current debug level and temporarily set to INFO
    int saved_debug_level = debugLevel;
    debugLevel            = INFO;
#endif

    // Read base block (block 0)
    if (!readEDIDBlock(bi, edid_data, EDID_I2C_ADDR_PRIMARY, 0)) {
        DFUNC(ERROR, "Failed to read primary EDID block\n");
#ifdef DBG
        debugLevel = saved_debug_level;
#endif
        return 0;
    }

    // Validate base block
    if (!validateEDID(edid_data)) {
        DFUNC(ERROR, "EDID base block validation failed\n");
#ifdef DBG
        debugLevel = saved_debug_level;
#endif
        return 0;
    }

    // Check number of extension blocks (byte 126)
    UBYTE num_extensions = edid_data[126];
    D(INFO, "EDID base block read, %lu extension block(s) reported\n", (ULONG)num_extensions);

    UBYTE blocks_read = 1;  // Base block already read

    // Read extension blocks if available
    if (num_extensions > 0 && max_blocks > 1) {
        // Limit to available buffer space
        UBYTE max_extensions = (max_blocks - 1 < num_extensions) ? (max_blocks - 1) : num_extensions;
        
        for (UBYTE ext = 1; ext <= max_extensions; ext++) {
            UBYTE *ext_data = edid_data + (ext * EDID_BLOCK_SIZE);
            
            D(INFO, "Reading EDID extension block %lu...\n", (ULONG)ext);
            
            if (!readEDIDBlock(bi, ext_data, EDID_I2C_ADDR_PRIMARY, ext)) {
                DFUNC(ERROR, "Failed to read EDID extension block %lu\n", (ULONG)ext);
                break;  // Stop on first failure
            }

            // Validate extension block checksum
            UBYTE checksum = 0;
            for (int i = 0; i < EDID_BLOCK_SIZE; i++) {
                checksum += ext_data[i];
            }
            
            if (checksum != 0) {
                DFUNC(ERROR, "EDID extension block %lu checksum invalid: 0x%02lx\n", (ULONG)ext, (ULONG)checksum);
                break;  // Stop on checksum error
            }

            blocks_read++;
            D(INFO, "EDID extension block %lu read and validated\n", (ULONG)ext);
        }
    }

#ifdef DBG
    debugLevel = saved_debug_level;
#endif

    D(INFO, "Successfully read %lu EDID block(s) (base + %lu extension(s))\n", 
      (ULONG)blocks_read, (ULONG)(blocks_read - 1));
    
    return blocks_read;
}

