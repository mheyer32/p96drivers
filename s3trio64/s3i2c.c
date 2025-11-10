#include "s3trio64_common.h"

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
BOOL i2cInit(struct BoardInfo *bi)
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
 */
static inline void i2cSetScl(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching)
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
static inline void i2cSetSda(struct BoardInfo *bi, BOOL high)
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
static inline BOOL i2cReadScl(struct BoardInfo *bi)
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
static inline BOOL i2cReadSda(struct BoardInfo *bi)
{
    MMIOBASE();
    ULONG serialReg = R_MMIO_L(SERIAL_PORT_REG);
    return (serialReg & SERIAL_SDR) != 0;
}

/**
 * Generate I2C start condition
 * SDA goes from high to low while SCL is high
 * @param bi BoardInfo structure
 */
void i2cStart(struct BoardInfo *bi)
{
    // Ensure SDA and SCL are released (high)
    i2cSetSda(bi, TRUE);
    i2cSetScl(bi, TRUE, FALSE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Start condition: SDA goes low while SCL is high
    i2cSetSda(bi, FALSE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Pull SCL low to prepare for data transfer
    i2cSetScl(bi, FALSE, FALSE);
    delayMicroSeconds(I2C_DELAY_US);
}

/**
 * Generate I2C stop condition
 * SDA goes from low to high while SCL is high
 * @param bi BoardInfo structure
 */
void i2cStop(struct BoardInfo *bi)
{
    // Ensure SDA is low and SCL is low
    i2cSetSda(bi, FALSE);
    i2cSetScl(bi, FALSE, FALSE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Pull SCL high first (check for clock stretching)
    i2cSetScl(bi, TRUE, TRUE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Stop condition: SDA goes high while SCL is high
    i2cSetSda(bi, TRUE);
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
    // Set SDA to desired value while SCL is low
    i2cSetSda(bi, bit != 0);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Clock the bit by pulling SCL high (check for clock stretching)
    i2cSetScl(bi, TRUE, TRUE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Pull SCL low to complete the bit transfer (no clock stretching check)
    i2cSetScl(bi, FALSE, FALSE);
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
    // Release SDA (set to input/tri-state)
    i2cSetSda(bi, TRUE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Clock the bit by pulling SCL high (check for clock stretching)
    i2cSetScl(bi, TRUE, TRUE);
    delayMicroSeconds(I2C_DELAY_US);
    
    // Read SDA value while SCL is high
    UBYTE bit = i2cReadSda(bi) ? 1 : 0;
    
    // Pull SCL low to complete the bit transfer (no clock stretching check)
    i2cSetScl(bi, FALSE, FALSE);
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

