#include "edid_common.h"
#include "s3trio64_common.h"

using namespace MmioReg;

#ifdef __cplusplus
extern "C" {
#endif

#define SERIAL_SCW (1 << 0)  // Serial Clock Write (bit 0)
#define SERIAL_SDW (1 << 1)  // Serial Data Write (bit 1)
#define SERIAL_SCR (1 << 2)  // Serial Clock Read (bit 2, read-only)
#define SERIAL_SDR (1 << 3)  // Serial Data Read (bit 3, read-only)
#define SERIAL_SPE (1 << 4)  // Serial Port Enable (bit 4)

#define I2C_DELAY_US 5

BOOL s3I2cInit(struct BoardInfo *bi)
{
    DRIVER_LOCALS(bi);

    ULONG serialReg = mmio.readL(SERIAL_PORT);
    mmio.writeL(SERIAL_PORT, serialReg | SERIAL_SPE);

    serialReg = mmio.readL(SERIAL_PORT);
    mmio.writeL(SERIAL_PORT, serialReg | SERIAL_SCW | SERIAL_SDW);

    delayMicroSeconds(10);

    D(VERBOSE, "I2C bus initialized\n");
    return TRUE;
}

void s3I2cSetScl(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching)
{
    DFUNC(VERBOSE, " %s\n", high ? "HIGH" : "LOW");
    DRIVER_LOCALS(bi);
    ULONG serialReg = mmio.readL(SERIAL_PORT);
    if (high) {
        mmio.writeL(SERIAL_PORT, serialReg | SERIAL_SCW);
        delayMicroSeconds(I2C_DELAY_US);

#ifdef DBG
        int settle_attempts = 5;
        BOOL scl_high       = FALSE;
        while (settle_attempts-- > 0) {
            serialReg = mmio.readL(SERIAL_PORT);
            if (serialReg & SERIAL_SCR) {
                scl_high = TRUE;
                break;
            }
            delayMicroSeconds(I2C_DELAY_US);
        }

        if (!scl_high) {
            D(ERROR, "I2C SCL failed to go high after release (serialReg=0x%08lx) - check pull-up resistors\n",
              serialReg);
            mmio.readL(S3_MMIO_ID(0xFF08));
        }
#endif

        if (checkClockStretching) {
            delayMicroSeconds(I2C_DELAY_US);
            serialReg = mmio.readL(SERIAL_PORT);

            if (!(serialReg & SERIAL_SCR)) {
                int timeout = 100;
                while (timeout-- > 0) {
                    delayMicroSeconds(I2C_DELAY_US);
                    serialReg = mmio.readL(SERIAL_PORT);
                    if (serialReg & SERIAL_SCR) {
                        D(VERBOSE, "Clock stretching detected and released after %d iterations\n", 100 - timeout);
                        break;
                    }
                }
                if (timeout <= 0) {
                    D(ERROR, "I2C clock stretching timeout - SCL stuck low (serialReg=0x%08lx)\n", serialReg);
                    mmio.readL(S3_MMIO_ID(0xFF08));
                }
            }
        }

#ifdef DBG
        delayMicroSeconds(I2C_DELAY_US);
        serialReg = mmio.readL(SERIAL_PORT);
        if (!(serialReg & SERIAL_SCR)) {
            D(ERROR, "I2C SCL not high after release (serialReg=0x%08lx)\n", serialReg);
        }
#endif
    } else {
        mmio.writeL(SERIAL_PORT, serialReg & ~SERIAL_SCW);
        delayMicroSeconds(I2C_DELAY_US);

#ifdef DBG
        serialReg = mmio.readL(SERIAL_PORT);
        if (serialReg & SERIAL_SCR) {
            D(ERROR, "I2C SCL failed to go low when driven (serialReg=0x%08lx)\n", serialReg);
        }
#endif
    }
}

void s3I2cSetSda(struct BoardInfo *bi, BOOL high)
{
    DFUNC(VERBOSE, " %s\n", high ? "HIGH" : "LOW");
    DRIVER_LOCALS(bi);
    ULONG serialReg = mmio.readL(SERIAL_PORT);
    if (high) {
        mmio.writeL(SERIAL_PORT, serialReg | SERIAL_SDW);
        delayMicroSeconds(I2C_DELAY_US);
    } else {
        mmio.writeL(SERIAL_PORT, serialReg & ~SERIAL_SDW);
        delayMicroSeconds(I2C_DELAY_US);

#ifdef DBG
        serialReg = mmio.readL(SERIAL_PORT);
        if (serialReg & SERIAL_SDR) {
            D(ERROR, "I2C SDA failed to go low when driven (serialReg=0x%08lx)\n", serialReg);
        }
#endif
    }
}

BOOL s3I2cReadScl(struct BoardInfo *bi)
{
    DRIVER_LOCALS(bi);
    return (mmio.readL(SERIAL_PORT) & SERIAL_SCR) != 0;
}

BOOL s3I2cReadSda(struct BoardInfo *bi)
{
    DRIVER_LOCALS(bi);
    return (mmio.readL(SERIAL_PORT) & SERIAL_SDR) != 0;
}

static const I2COps_t s3_i2c_ops = {.init    = s3I2cInit,
                                    .setScl  = s3I2cSetScl,
                                    .setSda  = s3I2cSetSda,
                                    .readScl = s3I2cReadScl,
                                    .readSda = s3I2cReadSda};

const I2COps_t *getI2COps(struct BoardInfo *bi)
{
    (void)bi;
    return &s3_i2c_ops;
}

#ifdef __cplusplus
}
#endif
