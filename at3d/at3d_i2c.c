#include "at3d_i2c.h"
#include "chip_at3d.h"
#include "common.h"

// I2C timing delays (standard I2C: 100kHz)
#define I2C_DELAY_US 5  // 5 microseconds for standard I2C timing

/**
 * Initialize I2C bus by enabling I2C/DDC mode
 * @param bi BoardInfo structure
 * @return TRUE if successful, FALSE otherwise
 */
BOOL at3dI2cInit(struct BoardInfo *bi)
{
    MMIOBASE();
    
    // Release both SDA and SCL lines (set to input/tri-state)
    // 0x = input/tri-state (both control bits clear)
    W_MMIO_MASK_B(DPMS_SYNC_CTRL, I2C_SCL_CTRL_MASK | I2C_SDA_CTRL_MASK, 0);
    
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
void at3dI2cSetScl(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching)
{
    DFUNC(VERBOSE, " %s\n", high ? "HIGH" : "LOW");
    MMIOBASE();
    
    if (high) {
        // Release SCL (set to input/tri-state)
        // 0x = input/tri-state (both bits clear)
        W_MMIO_MASK_B(DPMS_SYNC_CTRL, I2C_SCL_CTRL_MASK, 0);
        
        // Small delay to allow hardware to actually release the line
        delayMicroSeconds(I2C_DELAY_US);
        
#ifdef DBG
        // Verify that SCL actually goes high (pull-up working)
        // Read from 1FC[17] which is the SCL input
        int settle_attempts = 5;
        BOOL scl_high = FALSE;
        while (settle_attempts-- > 0) {
            ULONG statusReg = R_MMIO_L(EXT_DAC_STATUS);
            if (statusReg & BIT(I2C_SCL_IN_BIT)) {
                scl_high = TRUE;
                break;
            }
            delayMicroSeconds(I2C_DELAY_US);
        }
        
        if (!scl_high) {
            D(ERROR, "I2C SCL failed to go high after release - check pull-up resistors\n");
        }
#endif
        
        if (checkClockStretching) {
            // Check for clock stretching: slave pulls SCL low after master releases it
            delayMicroSeconds(I2C_DELAY_US);
            ULONG statusReg = R_MMIO_L(EXT_DAC_STATUS);
            
            if (!(statusReg & BIT(I2C_SCL_IN_BIT))) {
                // SCL went high but then went low - slave is clock stretching
                // Wait for SCL to go high again (slave releases it)
                int timeout = 100;  // Maximum iterations (~1ms with 10us delay)
                while (timeout-- > 0) {
                    delayMicroSeconds(I2C_DELAY_US);
                    statusReg = R_MMIO_L(EXT_DAC_STATUS);
                    if (statusReg & BIT(I2C_SCL_IN_BIT)) {
                        // SCL is high again, slave has released it
                        D(VERBOSE, "Clock stretching detected and released after %d iterations\n", 100 - timeout);
                        break;
                    }
                }
                if (timeout <= 0) {
                    D(ERROR, "I2C clock stretching timeout - SCL stuck low\n");
                }
            }
        }
    } else {
        // Drive SCL low: 10 = drive low (bit 1 set, bit 0 clear)
        W_MMIO_MASK_B(DPMS_SYNC_CTRL, I2C_SCL_CTRL_MASK, I2C_SCL_CTRL_LOW);
        delayMicroSeconds(I2C_DELAY_US);
        
#ifdef DBG
        // Verify that SCL is actually low (we're driving it, so it should be)
        ULONG statusReg = R_MMIO_L(EXT_DAC_STATUS);
        if (statusReg & BIT(I2C_SCL_IN_BIT)) {
            D(ERROR, "I2C SCL failed to go low when driven\n");
        }
#endif
    }
}

/**
 * Set SDA line state
 * @param bi BoardInfo structure
 * @param high TRUE to release (tri-state), FALSE to drive low
 */
void at3dI2cSetSda(struct BoardInfo *bi, BOOL high)
{
    DFUNC(VERBOSE, " %s\n", high ? "HIGH" : "LOW");
    MMIOBASE();
    
    if (high) {
        // Release SDA (set to input/tri-state)
        // 0x = input/tri-state (both bits clear)
        W_MMIO_MASK_B(DPMS_SYNC_CTRL, I2C_SDA_CTRL_MASK, 0);
        delayMicroSeconds(I2C_DELAY_US);
    } else {
        // Drive SDA low: 10 = drive low (bit 1 set, bit 0 clear)
        W_MMIO_MASK_B(DPMS_SYNC_CTRL, I2C_SDA_CTRL_MASK, I2C_SDA_CTRL_LOW);
        delayMicroSeconds(I2C_DELAY_US);
        
#ifdef DBG
        // Verify that SDA is actually low (we're driving it, so it should be)
        // Read from 1FC[16] which is the SDA input
        ULONG statusReg = R_MMIO_L(EXT_DAC_STATUS);
        if (statusReg & BIT(I2C_SDA_IN_BIT)) {
            D(ERROR, "I2C SDA failed to go low when driven\n");
        }
#endif
    }
}

/**
 * Read SCL line state
 * @param bi BoardInfo structure
 * @return TRUE if SCL is high, FALSE if low
 */
BOOL at3dI2cReadScl(struct BoardInfo *bi)
{
    MMIOBASE();
    // Read SCL input from 1FC[17] per AT3D documentation
    ULONG statusReg = R_MMIO_L(EXT_DAC_STATUS);
    return (statusReg & BIT(I2C_SCL_IN_BIT)) != 0;
}

/**
 * Read SDA line state
 * @param bi BoardInfo structure
 * @return TRUE if SDA is high, FALSE if low
 */
BOOL at3dI2cReadSda(struct BoardInfo *bi)
{
    MMIOBASE();
    // Read SDA input from 1FC[16] per AT3D documentation (equivalent to 0D0[4])
    ULONG statusReg = R_MMIO_L(EXT_DAC_STATUS);
    return (statusReg & BIT(I2C_SDA_IN_BIT)) != 0;
}

