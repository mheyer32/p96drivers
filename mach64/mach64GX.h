#ifndef MACH64GX_H
#define MACH64GX_H

#include <boardinfo.h>

#define BUS_PCI_RETRY_EN      BIT(15)
#define BUS_PCI_RETRY_EN_MASK BIT(15)
#define BUS_FIFO_WS(x)        ((x) << 16)
#define BUS_FIFO_WS_MASK      (0xF << 16)

BOOL InitMach64GX(struct BoardInfo *bi);

#endif  // MACH64GX_H


