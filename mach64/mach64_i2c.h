#ifndef MACH64_I2C_H
#define MACH64_I2C_H

#include "common.h"


BOOL queryEDID(struct BoardInfo *bi);

// I2C bit-banging function declarations for mach64
// These are low-level functions that manipulate the GP_IO register

/**
 * Initialize I2C bus by enabling DDC mode
 * @param bi BoardInfo structure
 * @return TRUE if successful, FALSE otherwise
 */
BOOL mach64I2cInit(struct BoardInfo *bi);

/**
 * Set SCL line state
 * @param bi BoardInfo structure
 * @param high TRUE to release (tri-state), FALSE to drive low
 * @param checkClockStretching TRUE to check for clock stretching (unused for now)
 */
void mach64I2cSetScl(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching);

/**
 * Set SDA line state
 * @param bi BoardInfo structure
 * @param high TRUE to release (tri-state), FALSE to drive low
 */
void mach64I2cSetSda(struct BoardInfo *bi, BOOL high);

/**
 * Read SCL line state
 * @param bi BoardInfo structure
 * @return TRUE if SCL is high, FALSE if low
 */
BOOL mach64I2cReadScl(struct BoardInfo *bi);

/**
 * Read SDA line state
 * @param bi BoardInfo structure
 * @return TRUE if SDA is high, FALSE if low
 */
BOOL mach64I2cReadSda(struct BoardInfo *bi);

#endif // MACH64_I2C_H



