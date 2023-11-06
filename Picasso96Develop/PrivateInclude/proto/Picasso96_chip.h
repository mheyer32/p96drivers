#ifndef _PROTO_PICASSO96_CHIP_H
#define _PROTO_PICASSO96_CHIP_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef CLIB_PICASSO96_CHIP_PROTOS_H
#include <clib/Picasso96_chip_protos.h>
#endif

#ifndef __NOLIBBASE__
extern struct ChipBase *ChipBase;
#endif

#ifdef __GNUC__
#if !defined(__cplusplus) && !defined(__PPC__) && !defined(NO_INLINE_LIBCALLS)
#include <inline/Picasso96_chip.h>
#endif
#elif !defined(__VBCC__)
#ifndef __PPC__
#include <pragma/Picasso96_chip_lib.h>
#endif
#endif

#endif	/*  _PROTO_PICASSO96_CHIP_H  */
