#include <stddef.h>

/* GCC emits these under -ffreestanding; release drivers use -nodefaultlibs.
 * __stdargs matches the stack ABI GCC/libnix use for mem* libcalls.
 * Keep C linkage when this TU is compiled as C++ (makefile uses $(CXX) for .c). */
#ifdef __cplusplus
extern "C" {
#endif

__stdargs void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

__stdargs void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *dd       = (unsigned char *)d;
    const unsigned char *ss = (const unsigned char *)s;
    while (n--)
        *dd++ = *ss++;
    return d;
}

__stdargs void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *dd       = (unsigned char *)d;
    const unsigned char *ss = (const unsigned char *)s;
    if (dd < ss) {
        while (n--)
            *dd++ = *ss++;
    } else if (dd > ss) {
        dd += n;
        ss += n;
        while (n--)
            *--dd = *--ss;
    }
    return d;
}

__stdargs int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *aa = (unsigned char *)a;
    const unsigned char *bb = (unsigned char *)b;
    while (n--) {
        if (*aa != *bb)
            return (int)*aa - (int)*bb;
        ++aa;
        ++bb;
    }
    return 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
