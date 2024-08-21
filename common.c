#include "common.h"

#ifdef DBG

#include <stdarg.h>
#include <stdio.h>

void myPrintF(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

#ifdef TESTEXE
    vprintf(fmt, args);
#else
    KPrintF(fmt, args)
#endif

    va_end(args);
}
#endif
