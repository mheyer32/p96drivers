#ifndef _INCLUDE_PRAGMA_PICASSO96_CHIP_LIB_H
#define _INCLUDE_PRAGMA_PICASSO96_CHIP_LIB_H

#ifndef CLIB_PICASSO96_CHIP_PROTOS_H
#include <clib/Picasso96_chip_protos.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#ifdef NO_OBSOLETE
#error "Please include the proto file and not the compiler specific file!"
#endif
#include <inline/Picasso96_chip.h>
#endif

#if defined(AZTEC_C) || defined(__MAXON__) || defined(__STORM__)
#pragma amicall(ChipBase,0x01E,InitChip(a0))
#endif
#if defined(_DCC) || defined(__SASC)
#pragma  libcall ChipBase InitChip     01E 801
#endif

#ifdef __cplusplus
}
#endif

#endif	/*  _INCLUDE_PRAGMA_PICASSO96_CHIP_LIB_H  */
