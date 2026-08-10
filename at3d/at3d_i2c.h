#ifndef AT3D_I2C_H
#define AT3D_I2C_H

#include "chip_at3d.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL at3dI2cInit(struct BoardInfo *bi);
void at3dI2cSetScl(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching);
void at3dI2cSetSda(struct BoardInfo *bi, BOOL high);
BOOL at3dI2cReadScl(struct BoardInfo *bi);
BOOL at3dI2cReadSda(struct BoardInfo *bi);

#ifdef __cplusplus
}
#endif

#endif /* AT3D_I2C_H */
