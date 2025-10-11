/* Automatically generated header (sfdc 1.11e)! Do not edit! */

#ifndef _INLINE_PICASSO96_CHIP_H
#define _INLINE_PICASSO96_CHIP_H

#ifndef _SFDC_VARARG_DEFINED
#define _SFDC_VARARG_DEFINED
#ifdef __HAVE_IPTR_ATTR__
typedef APTR _sfdc_vararg __attribute__((iptr));
#else
typedef ULONG _sfdc_vararg;
#endif /* __HAVE_IPTR_ATTR__ */
#endif /* _SFDC_VARARG_DEFINED */

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif /* !__INLINE_MACROS_H */

#ifndef PICASSO96_CHIP_BASE_NAME
#define PICASSO96_CHIP_BASE_NAME ChipBase
#endif /* !PICASSO96_CHIP_BASE_NAME */

#define InitChip(___bi) \
      LP1(0x1e, BOOL, InitChip , struct BoardInfo *, ___bi, a0,\
      , PICASSO96_CHIP_BASE_NAME)

#endif /* !_INLINE_PICASSO96_CHIP_H */
