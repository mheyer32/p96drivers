#include "s3trio64_common.h"
#include "edid_common.h"

// Serial Port Register (MMFF20) - MMIO offset 0xFF20
#define SERIAL_PORT_REG 0xFF20

// Serial Port Register bits
#define SERIAL_SCW (1 << 0)  // Serial Clock Write (bit 0)
#define SERIAL_SDW (1 << 1)  // Serial Data Write (bit 1)
#define SERIAL_SCR (1 << 2)  // Serial Clock Read (bit 2, read-only)
#define SERIAL_SDR (1 << 3)  // Serial Data Read (bit 3, read-only)
#define SERIAL_SPE (1 << 4)  // Serial Port Enable (bit 4)

// I2C timing delays (standard I2C: 100kHz)
#define I2C_DELAY_US 5  // 5 microseconds for standard I2C timing

/**
 * Initialize I2C bus by enabling the serial port
 * @param bi BoardInfo structure
 * @return TRUE if successful, FALSE otherwise
 */
BOOL s3I2cInit(struct BoardInfo *bi)
{
    MMIOBASE();
    
    // Enable serial port by setting SPE bit
    ULONG serialReg = R_MMIO_L(SERIAL_PORT_REG);
    W_MMIO_L(SERIAL_PORT_REG, serialReg | SERIAL_SPE);
    
    // Release both SDA and SCL lines (set to high/tri-state)
    serialReg = R_MMIO_L(SERIAL_PORT_REG);
    W_MMIO_L(SERIAL_PORT_REG, serialReg | SERIAL_SCW | SERIAL_SDW);
    
    // Wait a bit for lines to stabilize
    delayMicroSeconds(10);
    
    D(VERBOSE, "I2C bus initialized\n");
    return TRUE;
}

/**
 * Set SCL line state
 * @param bi BoardInfo structure
 * @param high TRUE to release (tri-state), FALSE to drive low
 * @param checkClockStretching TRUE to check for clock stretching
 */
void s3I2cSetScl(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching)
{
    DFUNC(VERBOSE, " %s\n", high ? "HIGH" : "LOW");
    MMIOBASE();
    ULONG serialReg = R_MMIO_L(SERIAL_PORT_REG);
    if (high) {
        // Release SCL (set to tri-state/high)
        W_MMIO_L(SERIAL_PORT_REG, serialReg | SERIAL_SCW);
        
        // Small delay to allow hardware to actually release the line
        delayMicroSeconds(I2C_DELAY_US);
        
#ifdef DBG
        // First, verify that SCL actually goes high (pull-up working)
        // Give it a few attempts to account for hardware settling time
        int settle_attempts = 5;
        BOOL scl_high = FALSE;
        while (settle_attempts-- > 0) {
            serialReg = R_MMIO_L(SERIAL_PORT_REG);
            if (serialReg & SERIAL_SCR) {
                scl_high = TRUE;
                break;
            }
            delayMicroSeconds(I2C_DELAY_US);
        }
        
        if (!scl_high) {
            // SCL never went high - this is a hardware issue, not clock stretching
            // Could be: missing pull-up resistor, hardware not tri-stating properly
            D(ERROR, "I2C SCL failed to go high after release (serialReg=0x%08lx) - check pull-up resistors\n", serialReg);
            R_MMIO_L(0xFF08);
            // Continue anyway - might still work if slave drives it
        }
#endif
        
        if (checkClockStretching) {
            // SCL went high, now check for clock stretching
            // Clock stretching: slave pulls SCL low after master releases it
            // Wait a bit and check if slave pulled it low
            delayMicroSeconds(I2C_DELAY_US);
            serialReg = R_MMIO_L(SERIAL_PORT_REG);
            
            if (!(serialReg & SERIAL_SCR)) {
                // SCL went high but then went low - slave is clock stretching
                // Wait for SCL to go high again (slave releases it)
                int timeout = 100;  // Maximum iterations (~1ms with 10us delay)
                while (timeout-- > 0) {
                    delayMicroSeconds(I2C_DELAY_US);
                    serialReg = R_MMIO_L(SERIAL_PORT_REG);
                    if (serialReg & SERIAL_SCR) {
                        // SCL is high again, slave has released it
                        D(VERBOSE, "Clock stretching detected and released after %d iterations\n", 100 - timeout);
                        break;
                    }
                }
                if (timeout <= 0) {
                    D(ERROR, "I2C clock stretching timeout - SCL stuck low (serialReg=0x%08lx)\n", serialReg);
                    R_MMIO_L(0xFF08);
                }
            }
            // If SCL stayed high, no clock stretching occurred
        }
        
#ifdef DBG
        // Final verification: after all processing, SCL should be high when released
        delayMicroSeconds(I2C_DELAY_US);
        serialReg = R_MMIO_L(SERIAL_PORT_REG);
        if (!(serialReg & SERIAL_SCR)) {
            // SCL should be high after release (unless slave is still clock stretching, which we already handled)
            D(ERROR, "I2C SCL not high after release (serialReg=0x%08lx)\n", serialReg);
        }
#endif
    } else {
        // Drive SCL low (no clock stretching check needed here)
        W_MMIO_L(SERIAL_PORT_REG, serialReg & ~SERIAL_SCW);
        delayMicroSeconds(I2C_DELAY_US);
        
#ifdef DBG
        // Verify that SCL is actually low (we're driving it, so it should be)
        serialReg = R_MMIO_L(SERIAL_PORT_REG);
        if (serialReg & SERIAL_SCR) {
            D(ERROR, "I2C SCL failed to go low when driven (serialReg=0x%08lx)\n", serialReg);
        }
#endif
    }
}

/**
 * Set SDA line state
 * @param bi BoardInfo structure
 * @param high TRUE to release (tri-state), FALSE to drive low
 */
void s3I2cSetSda(struct BoardInfo *bi, BOOL high)
{
   DFUNC(VERBOSE, " %s\n", high ? "HIGH" : "LOW");
    MMIOBASE();
    ULONG serialReg = R_MMIO_L(SERIAL_PORT_REG);
    if (high) {
        // Release SDA (set to tri-state/high)
        W_MMIO_L(SERIAL_PORT_REG, serialReg | SERIAL_SDW);
        delayMicroSeconds(I2C_DELAY_US);
        
#ifdef DBG
        // Verify SDA goes high after release (slave may pull it low during ACK, which is expected)
        serialReg = R_MMIO_L(SERIAL_PORT_REG);
        if (!(serialReg & SERIAL_SDR)) {
            // SDA is low - could be slave pulling it low (ACK) or hardware issue
            // This is expected during ACK/NACK, so we don't error, just note it
            // (The actual ACK check happens in i2cReadBit/i2cWriteByte)
        }
#endif
    } else {
        // Drive SDA low
        W_MMIO_L(SERIAL_PORT_REG, serialReg & ~SERIAL_SDW);
        delayMicroSeconds(I2C_DELAY_US);
        
#ifdef DBG
        // Verify that SDA is actually low (we're driving it, so it should be)
        serialReg = R_MMIO_L(SERIAL_PORT_REG);
        if (serialReg & SERIAL_SDR) {
            D(ERROR, "I2C SDA failed to go low when driven (serialReg=0x%08lx)\n", serialReg);
        }
#endif
    }
}

/**
 * Read SCL line state
 * @param bi BoardInfo structure
 * @return TRUE if SCL is high, FALSE if low
 */
BOOL s3I2cReadScl(struct BoardInfo *bi)
{
    MMIOBASE();
    ULONG serialReg = R_MMIO_L(SERIAL_PORT_REG);
    return (serialReg & SERIAL_SCR) != 0;
}

/**
 * Read SDA line state
 * @param bi BoardInfo structure
 * @return TRUE if SDA is high, FALSE if low
 */
BOOL s3I2cReadSda(struct BoardInfo *bi)
{
    MMIOBASE();
    ULONG serialReg = R_MMIO_L(SERIAL_PORT_REG);
    return (serialReg & SERIAL_SDR) != 0;
}

// I2C protocol functions (start, stop, bit/byte read/write) are now in edid_common.c
// They use the I2COps interface provided by s3I2cInit, s3I2cSetScl, etc.

