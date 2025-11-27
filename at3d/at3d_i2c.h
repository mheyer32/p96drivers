#ifndef AT3D_I2C_H
#define AT3D_I2C_H

#include "chip_at3d.h"

// I2C bit-banging function declarations for AT3D
// These are low-level functions that manipulate the I2C registers

/**
 * Initialize I2C bus
 * @param bi BoardInfo structure
 * @return TRUE if successful, FALSE otherwise
 */
BOOL at3dI2cInit(struct BoardInfo *bi);

/**
 * Set SCL line state
 * @param bi BoardInfo structure
 * @param high TRUE to release (tri-state), FALSE to drive low
 * @param checkClockStretching TRUE to check for clock stretching
 */
void at3dI2cSetScl(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching);

/**
 * Set SDA line state
 * @param bi BoardInfo structure
 * @param high TRUE to release (tri-state), FALSE to drive low
 */
void at3dI2cSetSda(struct BoardInfo *bi, BOOL high);

/**
 * Read SCL line state
 * @param bi BoardInfo structure
 * @return TRUE if SCL is high, FALSE if low
 */
BOOL at3dI2cReadScl(struct BoardInfo *bi);

/**
 * Read SDA line state
 * @param bi BoardInfo structure
 * @return TRUE if SDA is high, FALSE if low
 */
BOOL at3dI2cReadSda(struct BoardInfo *bi);

#endif // AT3D_I2C_H

