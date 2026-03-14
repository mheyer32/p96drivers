#ifndef S3I2C_H
#define S3I2C_H

#include "chip_s3trio64.h"
#include "edid_common.h"

// I2C bit-banging function declarations for S3
// These are low-level functions that manipulate the I2C registers

/**
 * Initialize I2C bus by enabling the serial port
 * @param bi BoardInfo structure
 * @return TRUE if successful, FALSE otherwise
 */
BOOL s3I2cInit(struct BoardInfo *bi);

/**
 * Set SCL line state
 * @param bi BoardInfo structure
 * @param high TRUE to release (tri-state), FALSE to drive low
 * @param checkClockStretching TRUE to check for clock stretching
 */
void s3I2cSetScl(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching);

/**
 * Set SDA line state
 * @param bi BoardInfo structure
 * @param high TRUE to release (tri-state), FALSE to drive low
 */
void s3I2cSetSda(struct BoardInfo *bi, BOOL high);

/**
 * Read SCL line state
 * @param bi BoardInfo structure
 * @return TRUE if SCL is high, FALSE if low
 */
BOOL s3I2cReadScl(struct BoardInfo *bi);

/**
 * Read SDA line state
 * @param bi BoardInfo structure
 * @return TRUE if SDA is high, FALSE if low
 */
BOOL s3I2cReadSda(struct BoardInfo *bi);

#endif  // S3I2C_H
