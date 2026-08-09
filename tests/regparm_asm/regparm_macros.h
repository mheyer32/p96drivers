#ifndef REGPARM_MACROS_H
#define REGPARM_MACROS_H

/*
 * Same register-parameter macros as Picasso96 boardinfo.h (GCC path).
 */
#ifdef __GNUC__
#define ASM
#define __REGD0(x) x __asm("d0")
#define __REGD1(x) x __asm("d1")
#define __REGD2(x) x __asm("d2")
#define __REGD3(x) x __asm("d3")
#define __REGD4(x) x __asm("d4")
#define __REGD5(x) x __asm("d5")
#define __REGD6(x) x __asm("d6")
#define __REGD7(x) x __asm("d7")
#define __REGA0(x) x __asm("a0")
#define __REGA1(x) x __asm("a1")
#define __REGA2(x) x __asm("a2")
#define __REGA3(x) x __asm("a3")
#define __REGA4(x) x __asm("a4")
#define __REGA5(x) x __asm("a5")
#define __REGA6(x) x __asm("a6")
#else
#error regparm test suite requires GCC
#endif

#endif /* REGPARM_MACROS_H */
