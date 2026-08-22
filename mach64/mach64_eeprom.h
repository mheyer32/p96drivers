#ifndef MACH64_EEPROM_H
#define MACH64_EEPROM_H

#include "common.h"

struct BoardInfo;

#ifdef __cplusplus
extern "C" {
#endif

/* Dump Microwire EEPROM via GEN_TEST_CNTL and decode CRTC mode tables (ATI.TXT). */
void dumpMach64Eeprom(struct BoardInfo *bi);

#ifdef __cplusplus
}
#endif

#endif
