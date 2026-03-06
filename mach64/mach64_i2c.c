#include "mach64_i2c.h"
#include "chip_mach64.h"
#include "edid_common.h"
#include "mach64_common.h"

// LT_GIO register (0x2A) bit definitions for DDC/I2C
// Offset: 0x2A, Index: 7
// Based on mach64 GT hardware documentation
#define LT_GIO_REG 0x2A

// GPIO data bits (read/write the pin state)
#define LT_GIO_GPIO_14 BIT(12)  // GPIO_14 data (Pin: GPIO(14)) - Used for SCL
#define LT_GIO_GPIO_15 BIT(13)  // GPIO_15 data (Pin: GPIO(15)) - Used for SDA
#define LT_GIO_GPIO_16 BIT(14)  // GPIO_16 data (Pin: GPIO(16)) - Possibly enable/control

// GPIO direction bits (0 = Input, 1 = Output)
#define LT_GIO_GPIO_DIR_14 BIT(28)  // GPIO_14 direction (0=Input, 1=Output)
#define LT_GIO_GPIO_DIR_15 BIT(29)  // GPIO_15 direction (0=Input, 1=Output)
#define LT_GIO_GPIO_DIR_16 BIT(30)  // GPIO_16 direction (0=Input, 1=Output)

// DDC I/O drive strength control (applies to GPIO pins 14-16)
#define LT_GIO_DDC_IO_DRIVE BIT(31)  // DDC IO output drive strength (0=No boost, 1=Boost)

// Convenience macros for I2C
#define LT_GIO_SCL_DATA LT_GIO_GPIO_14
#define LT_GIO_SDA_DATA LT_GIO_GPIO_15
#define LT_GIO_SCL_DIR  LT_GIO_GPIO_DIR_14
#define LT_GIO_SDA_DIR  LT_GIO_GPIO_DIR_15

// I2C timing delays (standard I2C: 100kHz)
#define I2C_DELAY_US 5  // 5 microseconds for standard I2C timing

/**
 * Initialize I2C bus by enabling DDC mode
 * @param bi BoardInfo structure
 * @return TRUE if successful, FALSE otherwise
 */
BOOL mach64I2cInit(struct BoardInfo *bi)
{
    REGBASE();

    // Read current LT_GIO register value
    ULONG lt_gio = R_BLKIO_L(LT_GIO_REG);

    // Configure GPIO_14 (SCL) and GPIO_15 (SDA) as inputs initially (released/tri-state)
    // Set direction bits to 0 (input) to allow pull-ups to work
    lt_gio &= ~(LT_GIO_SCL_DIR | LT_GIO_SDA_DIR);  // Set as inputs (released)

    // GPIO_16 might be an enable line - set it as output and high
    // (This may need adjustment based on actual hardware requirements)
    // If GPIO_16 is not needed, we can leave it as input
    lt_gio |= LT_GIO_GPIO_DIR_16 | LT_GIO_GPIO_16;

    // Enable DDC I/O drive boost for better signal integrity on GPIO pins 14-16
    // This increases output drive strength when pins are configured as outputs
    lt_gio |= LT_GIO_DDC_IO_DRIVE;

    W_BLKIO_L(LT_GIO_REG, lt_gio);

    // Wait a bit for lines to stabilize
    delayMicroSeconds(10);

    D(VERBOSE, "I2C bus initialized (LT_GIO=0x%08lx)\n", lt_gio);
    return TRUE;
}

/**
 * Set SCL line state
 * @param bi BoardInfo structure
 * @param high TRUE to release (tri-state), FALSE to drive low
 * @param checkClockStretching TRUE to check for clock stretching
 */
void mach64I2cSetScl(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching)
{
    DFUNC(VERBOSE, " %s\n", high ? "HIGH" : "LOW");
    REGBASE();

    ULONG lt_gio = R_BLKIO_L(LT_GIO_REG);

    if (high) {
        // Release SCL: switch to input mode (tri-state) to allow pull-up to work
        lt_gio &= ~LT_GIO_SCL_DIR;  // Set direction to input (0)
        W_BLKIO_L(LT_GIO_REG, lt_gio);

        // Small delay to allow hardware to actually release the line
        delayMicroSeconds(I2C_DELAY_US);

#ifdef DBG
        // Verify that SCL actually goes high (pull-up working)
        int settle_attempts = 5;
        BOOL scl_high       = FALSE;
        while (settle_attempts-- > 0) {
            lt_gio = R_BLKIO_L(LT_GIO_REG);
            if (lt_gio & LT_GIO_SCL_DATA) {
                scl_high = TRUE;
                break;
            }
            delayMicroSeconds(I2C_DELAY_US);
        }

        if (!scl_high) {
            D(ERROR, "I2C SCL failed to go high after release (LT_GIO=0x%08lx) - check pull-up resistors\n", lt_gio);
        }
#endif

        if (checkClockStretching) {
            // Check for clock stretching (SCL is already in input mode)
            delayMicroSeconds(I2C_DELAY_US);
            lt_gio = R_BLKIO_L(LT_GIO_REG);

            if (!(lt_gio & LT_GIO_SCL_DATA)) {
                // SCL is low - slave is clock stretching
                int timeout = 100;  // Maximum iterations (~1ms with 10us delay)
                while (timeout-- > 0) {
                    delayMicroSeconds(I2C_DELAY_US);
                    lt_gio = R_BLKIO_L(LT_GIO_REG);
                    if (lt_gio & LT_GIO_SCL_DATA) {
                        // SCL is high again, slave has released it
                        D(VERBOSE, "Clock stretching detected and released after %d iterations\n", 100 - timeout);
                        break;
                    }
                }
                if (timeout <= 0) {
                    D(ERROR, "I2C clock stretching timeout - SCL stuck low (LT_GIO=0x%08lx)\n", lt_gio);
                }
            }
            // SCL remains in input mode after release
        }

#ifdef DBG
        // Final verification
        delayMicroSeconds(I2C_DELAY_US);
        lt_gio = R_BLKIO_L(LT_GIO_REG);
        if (!(lt_gio & LT_GIO_SCL_DATA)) {
            D(ERROR, "I2C SCL not high after release (LT_GIO=0x%08lx)\n", lt_gio);
        }
#endif
    } else {
        // Drive SCL low: switch to output mode and drive low
        lt_gio |= LT_GIO_SCL_DIR;    // Set direction to output (1)
        lt_gio &= ~LT_GIO_SCL_DATA;  // Set data bit low (0)
        W_BLKIO_L(LT_GIO_REG, lt_gio);
        delayMicroSeconds(I2C_DELAY_US);

#ifdef DBG
        // Verify that SCL is actually low
        lt_gio = R_BLKIO_L(LT_GIO_REG);
        if (lt_gio & LT_GIO_SCL_DATA) {
            D(ERROR, "I2C SCL failed to go low when driven (LT_GIO=0x%08lx)\n", lt_gio);
        }
#endif
    }
}

/**
 * Set SDA line state
 * @param bi BoardInfo structure
 * @param high TRUE to release (tri-state), FALSE to drive low
 */
void mach64I2cSetSda(struct BoardInfo *bi, BOOL high)
{
    DFUNC(VERBOSE, " %s\n", high ? "HIGH" : "LOW");
    REGBASE();

    ULONG lt_gio = R_BLKIO_L(LT_GIO_REG);

    if (high) {
        // Release SDA: switch to input mode (tri-state) to allow pull-up to work
        lt_gio &= ~LT_GIO_SDA_DIR;  // Set direction to input (0)
        W_BLKIO_L(LT_GIO_REG, lt_gio);
        delayMicroSeconds(I2C_DELAY_US);

#ifdef DBG
        // Verify SDA goes high after release (slave may pull it low during ACK, which is expected)
        lt_gio = R_BLKIO_L(LT_GIO_REG);
        // This is expected during ACK/NACK, so we don't error, just note it
#endif
    } else {
        // Drive SDA low: switch to output mode and drive low
        lt_gio |= LT_GIO_SDA_DIR;    // Set direction to output (1)
        lt_gio &= ~LT_GIO_SDA_DATA;  // Set data bit low (0)
        W_BLKIO_L(LT_GIO_REG, lt_gio);
        delayMicroSeconds(I2C_DELAY_US);

#ifdef DBG
        // Verify that SDA is actually low
        lt_gio = R_BLKIO_L(LT_GIO_REG);
        if (lt_gio & LT_GIO_SDA_DATA) {
            D(ERROR, "I2C SDA failed to go low when driven (LT_GIO=0x%08lx)\n", lt_gio);
        }
#endif
    }
}

/**
 * Read SCL line state
 * @param bi BoardInfo structure
 * @return TRUE if SCL is high, FALSE if low
 */
BOOL mach64I2cReadScl(struct BoardInfo *bi)
{
    REGBASE();
    ULONG lt_gio = R_BLKIO_L(LT_GIO_REG);

    // If already in input mode, just read it
    // If in output mode, we need to switch to input to read actual line state
    if (lt_gio & LT_GIO_SCL_DIR) {
        // Currently in output mode, switch to input temporarily
        W_BLKIO_L(LT_GIO_REG, lt_gio & ~LT_GIO_SCL_DIR);
        delayMicroSeconds(I2C_DELAY_US);
        lt_gio = R_BLKIO_L(LT_GIO_REG);
        // Note: We leave it in input mode (caller should restore if needed)
    }

    // Read the line state (data bit when in input mode)
    return (lt_gio & LT_GIO_SCL_DATA) != 0;
}

/**
 * Read SDA line state
 * @param bi BoardInfo structure
 * @return TRUE if SDA is high, FALSE if low
 */
BOOL mach64I2cReadSda(struct BoardInfo *bi)
{
    REGBASE();
    ULONG lt_gio = R_BLKIO_L(LT_GIO_REG);

    // If already in input mode, just read it
    // If in output mode, we need to switch to input to read actual line state
    if (lt_gio & LT_GIO_SDA_DIR) {
        // Currently in output mode, switch to input temporarily
        W_BLKIO_L(LT_GIO_REG, lt_gio & ~LT_GIO_SDA_DIR);
        delayMicroSeconds(I2C_DELAY_US);
        lt_gio = R_BLKIO_L(LT_GIO_REG);
        // Note: We leave it in input mode (caller should restore if needed)
    }

    // Read the line state (data bit when in input mode)
    return (lt_gio & LT_GIO_SDA_DATA) != 0;
}

// Static I2C operations structure for mach64 (GT and higher)
static const I2COps_t mach64_i2c_ops = {.init    = mach64I2cInit,
                                        .setScl  = mach64I2cSetScl,
                                        .setSda  = mach64I2cSetSda,
                                        .readScl = mach64I2cReadScl,
                                        .readSda = mach64I2cReadSda};

/**
 * Get I2C operations for Mach64 card
 * This function is required by edid_common.c
 */
const I2COps_t *getI2COps(struct BoardInfo *bi)
{
    return &mach64_i2c_ops;
}

BOOL queryEDID(struct BoardInfo *bi)
{
    ChipData_t *cd = getChipData(bi);
    // Initialize I2C operations for EDID support (GT and higher only)
    if (cd->chipFamily == MACH64GT || cd->chipFamily == MACH64GM) {
        cd->i2cOps = &mach64_i2c_ops;
        D(INFO, "I2C operations initialized for EDID support\n");

        // Read EDID after I2C initialization
        UBYTE edid_data[EDID_BLOCK_SIZE];
        if (readEDID(bi, edid_data)) {
            D(INFO, "EDID read successfully for mach64\n");
        } else {
            D(WARN, "Failed to read EDID for mach64\n");
        }
    } else {
        cd->i2cOps = NULL;
        D(INFO, "I2C operations not available for this chip family\n");
    }

    return TRUE;
}
