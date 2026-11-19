#ifndef MACH64_VBIOS_H
#define MACH64_VBIOS_H

#include "common.h"

class Mach64Driver;

/* Run the card option ROM (pciinit), then leave the chip in the state it POSTed. */
BOOL RunVBIOS(Mach64Driver &drv);

#endif
