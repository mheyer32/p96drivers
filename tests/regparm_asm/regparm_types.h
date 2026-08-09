#ifndef REGPARM_TYPES_H
#define REGPARM_TYPES_H

#include <exec/types.h>

/*
 * Unscoped enum matching Picasso96 RGBFTYPE style (typedef enum { ... } RGBFTYPE).
 */
typedef enum {
    RP_RGBFB_CLUT8 = 0,
    RP_RGBFB_R8G8B8 = 1,
    RP_RGBFB_MaxFormats
} RP_RGBFTYPE;

struct ModeInfoStub {
    ULONG dummy;
};

#ifdef __cplusplus
enum class RP_PixelFormat : ULONG {
    CLUT8 = 0,
    R8G8B8 = 1
};
#endif

#endif /* REGPARM_TYPES_H */
