#ifndef MACH64_EEPROM_H
#define MACH64_EEPROM_H

#include "common.h"

class Mach64Driver;

/* Dump Microwire EEPROM via GEN_TEST_CNTL and decode CRTC mode tables (ATI.TXT). */
void dumpMach64Eeprom(Mach64Driver &bi);

#endif
