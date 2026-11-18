#ifndef MACH64GM_H
#define MACH64GM_H

#include <boardinfo.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Run the card option ROM, then continue from the state it left. */
BOOL mach64GMRunBios(struct BoardInfo *bi);
BOOL InitMach64GM(struct BoardInfo *bi);

#ifdef __cplusplus
}
#endif

#endif
