#ifndef S3I2C_H
#define S3I2C_H

#include "chip_s3trio64.h"

// I2C bit-banging function declarations

/**
 * Initialize I2C bus by enabling the serial port
 * @param bi BoardInfo structure
 * @return TRUE if successful, FALSE otherwise
 */
BOOL i2cInit(struct BoardInfo *bi);

/**
 * Generate I2C start condition
 * @param bi BoardInfo structure
 */
void i2cStart(struct BoardInfo *bi);

/**
 * Generate I2C stop condition
 * @param bi BoardInfo structure
 */
void i2cStop(struct BoardInfo *bi);

/**
 * Write a single bit on I2C bus
 * @param bi BoardInfo structure
 * @param bit Bit value to write (0 or 1)
 * @return TRUE if successful
 */
BOOL i2cWriteBit(struct BoardInfo *bi, UBYTE bit);

/**
 * Read a single bit from I2C bus
 * @param bi BoardInfo structure
 * @return Bit value (0 or 1)
 */
UBYTE i2cReadBit(struct BoardInfo *bi);

/**
 * Write a byte on I2C bus and check for ACK
 * @param bi BoardInfo structure
 * @param data Byte to write
 * @return TRUE if ACK received, FALSE if NACK
 */
BOOL i2cWriteByte(struct BoardInfo *bi, UBYTE data);

/**
 * Read a byte from I2C bus
 * @param bi BoardInfo structure
 * @param ack TRUE to send ACK, FALSE to send NACK
 * @return Byte read from bus
 */
UBYTE i2cReadByte(struct BoardInfo *bi, BOOL ack);

#endif // S3I2C_H


