/* Automatically generated header! Do not edit! */

#ifndef _INLINE_PICASSO96_CHIP_H
#define _INLINE_PICASSO96_CHIP_H

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif /* !__INLINE_MACROS_H */

#ifndef CHIP_BASE_NAME
#define CHIP_BASE_NAME ChipBase
#endif /* !CHIP_BASE_NAME */

#define InitChip(bi) \
	((BOOL (*)(struct BoardInfo * __asm("a0"), struct Library * __asm("a6"))) \
  (((char *) CHIP_BASE_NAME) - 30))(bi, (struct Library *) CHIP_BASE_NAME)

#endif /* !_INLINE_PICASSO96_CHIP_H */
