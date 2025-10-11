/* Automatically generated header (sfdc 1.11e)! Do not edit! */

#ifndef _INLINE_PICASSO96_CARD_H
#define _INLINE_PICASSO96_CARD_H

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

#ifndef PICASSO96_CARD_BASE_NAME
#define PICASSO96_CARD_BASE_NAME CardBase
#endif /* !PICASSO96_CARD_BASE_NAME */

#define FindCard(___bi) \
      LP1(0x1e, BOOL, FindCard , struct BoardInfo *, ___bi, a0,\
      , PICASSO96_CARD_BASE_NAME)

#define InitCard(___bi, ___tooltypes) \
      LP2(0x24, BOOL, InitCard , struct BoardInfo *, ___bi, a0, STRPTR*, ___tooltypes, a1,\
      , PICASSO96_CARD_BASE_NAME)

#endif /* !_INLINE_PICASSO96_CARD_H */
