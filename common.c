#include "common.h"

#define __NOLIBBASE__

#include <hardware/cia.h>

#include <mmu/alerts.h>
#include <proto/mmu.h>
#include <dos/dos.h>

#ifdef DBG

#include <stdarg.h>
#include <stdio.h>

#if !defined(TESTEXE)
/* Freestanding .chip/.card: no libnix SysBase init. Pass AbsExecBase explicitly —
 * works as C or C++ (NDK RawDoFmt inline is C-only: void* → VOID(*)()). */
#define ABS_SYSBASE (*(struct ExecBase *const *)4)

static void dbgRawPutChar(struct ExecBase *SysBase, UBYTE c)
{
    register struct ExecBase *a6 asm("a6") = SysBase;
    register ULONG d0 asm("d0")            = c;
    asm volatile("jsr a6@(-516)" : "+d"(d0), "+r"(a6) : : "a0", "a1", "d1", "memory");
}

static void dbgRawDoFmt(struct ExecBase *SysBase, CONST_STRPTR fmt, APTR data, VOID (*putCh)(),
                        APTR putData)
{
    register struct ExecBase *a6 asm("a6") = SysBase;
    register CONST_STRPTR a0 asm("a0")     = fmt;
    register APTR a1 asm("a1")             = data;
    register VOID (*a2)() asm("a2")        = putCh;
    register APTR a3 asm("a3")             = putData;
    asm volatile("jsr a6@(-522)" : "+r"(a6), "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3) : : "d0", "d1",
                 "memory");
}

static void ASM dbgPutCh(__REGD0(char c), __REGA3(APTR unused))
{
    (void)unused;
    dbgRawPutChar(ABS_SYSBASE, (UBYTE)c);
}
#endif

void __stdargs myPrintF(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#if defined(TESTEXE)
    vprintf(fmt, ap);
#else
    dbgRawDoFmt(ABS_SYSBASE, (CONST_STRPTR)fmt, (APTR)ap, (VOID(*)())dbgPutCh, NULL);
#endif
    va_end(ap);
}

#endif

// Compute PLL parameters accordng to
// f_wanted_kHz = f_base * m / n * 1/(2^r)

// f_wanted_khz is in Khz
int svga_compute_pll(const struct svga_pll *pll, ULONG f_wanted_khz, USHORT *m, USHORT *n, USHORT *r)
{
    USHORT am, an, ar;
    ULONG f_vco, f_current, delta_current, delta_best;

    DFUNC(CHATTY, "ideal frequency: %ld kHz\n", (unsigned int)f_wanted_khz);

    ar    = pll->r_max;
    f_vco = f_wanted_khz << ar;

    /* overflow check */
    if ((f_vco >> ar) != f_wanted_khz) {
        DFUNC(ERROR, "pixelclock overflow\n");
        return -1;
    }

    /* It is usually better to have greater VCO clock
       because of better frequency stability.
       So first try r_max, then r smaller. */
    while ((ar > pll->r_min) && (f_vco > pll->f_vco_max)) {
        ar--;
        f_vco = f_vco >> 1;
    }

    /* VCO bounds check */
    if ((f_vco < pll->f_vco_min) || (f_vco > pll->f_vco_max)) {
        DFUNC(ERROR, "VCO frequency out of range %ldkHz for desired frequency %ldkHz\n", f_vco, f_wanted_khz);
        return -1;
    }

    delta_best = 0xFFFFFFFF;
    *m         = 0;
    *n         = 0;
    *r         = ar;

    am = pll->m_min;
    an = pll->n_min;

    while ((am <= pll->m_max) && (an <= pll->n_max)) {
        f_current     = (pll->f_base * am) / an;
        delta_current = abs_diff(f_current, f_vco);

        if (delta_current < delta_best) {
            delta_best = delta_current;
            *m         = am;
            *n         = an;
        }

        if (f_current <= f_vco) {
            am++;
        } else {
            an++;
        }
    }

    if (!*n) {
        DFUNC(ERROR, "PLL search failed\n");
        return -1;
    }

    f_current = (pll->f_base * *m) / *n;

    D(CHATTY, "found frequency: %ld kHz (VCO %ld kHz)\n", (int)(f_current >> ar), (int)f_current);
    D(CHATTY, "m = %ld n = %ld r = %ld\n", (unsigned int)*m, (unsigned int)*n, (unsigned int)*r);

    return (f_current >> ar);
}

void delayMicroSeconds(ULONG us)
{
    flushWrites();
    int count = ((us << 4) + 15) / 22;
    // CIA access has deterministic speed, use it for a short delay
    // Address the chip directly instead of via amiga.lib's ciaa symbol: this stays absolute
    // even under -fbaserel, where a symbol reference would be resolved against a4.
    volatile struct CIA *cia = (volatile struct CIA *)0xBFE001;
    for (int i = 0; i < count; ++i) {
        UBYTE x = cia->ciapra;
    }
}

void delayMilliSeconds(ULONG ms)
{
    delayMicroSeconds(ms * 1000);
}

// Taken from MMULib example code
BOOL setCacheMode(struct BoardInfo *bi, APTR from, ULONG size, ULONG flags, ULONG mask)
{
    LOCAL_SYSBASE();

    struct MMUContext *ctx, *sctx; /* default context, supervisorcontext */
    struct MinList *ctxl, *sctxl;
    struct Library *MMUBase = NULL;

    BOOL err = FALSE;

    if (MMUBase = OpenLibrary("mmu.library", 0)) {
        ctx  = DefaultContext();  /* get the default context */
        sctx = SuperContext(ctx); /* get the supervisor context for this one */

        LockContextList();
        LockMMUContext(ctx);
        LockMMUContext(sctx);

        ULONG pageSize = GetPageSize(ctx);
        ULONG start    = (ULONG)from & ~(pageSize - 1);
        size += (ULONG)from - start;
        size = (size + pageSize - 1) & ~(pageSize - 1);

        if (ctxl = GetMapping(ctx)) {
            if (sctxl = GetMapping(sctx)) {
                if (SetProperties(ctx, flags, mask, start, size, TAG_DONE)) {
                    if (SetProperties(sctx, flags, mask, start, size, TAG_DONE)) {
                        if (RebuildTree(ctx)) {
                            if (RebuildTree(sctx)) {
                                err = 0;
                            }
                        }
                    }
                }

                if (err) {
                    SetPropertyList(ctx, ctxl);
                    SetPropertyList(sctx, sctxl);
                }

                ReleaseMapping(sctx, sctxl);
            }
            ReleaseMapping(ctx, ctxl);
        }

        UnlockMMUContext(sctx);
        UnlockMMUContext(ctx);
        UnlockContextList();

        CloseLibrary(MMUBase);
    }
    if (err) {
        DFUNC(ERROR, "failed\n");
    }
    return err == 0;
}
