#include "common.h"
#include <clib/debug_protos.h>
#include <hardware/cia.h>
#include <proto/mmu.h>

#ifdef DBG

#include <stdarg.h>
#include <stdio.h>

void myPrintF(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

#if defined(TESTEXE)
    vprintf(fmt, args);
#else
    KVPrintF(fmt, args);
#endif

    va_end(args);
}

void mySprintF(struct ExecBase *SysBase, char *outStr, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

#if defined(TESTEXE)
    vprintf(fmt, args);
#else
    KVPrintF(fmt, args);
#endif

    va_end(args);
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
        DFUNC(0, "pixelclock overflow\n");
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
        DFUNC(0, "pixelclock overflow\n");
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
    extern volatile FAR struct CIA ciaa;
    for (int i = 0; i < count; ++i) {
        UBYTE x = ciaa.ciapra;
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

    int err = ERROR_OBJECT_NOT_FOUND;

    if (MMUBase = OpenLibrary("mmu.library", 0)) {
        ctx  = DefaultContext();  /* get the default context */
        sctx = SuperContext(ctx); /* get the supervisor context for this one */

        LockContextList();
        LockMMUContext(ctx);
        LockMMUContext(sctx);

        err = ERROR_NO_FREE_STORE;

        ULONG pageSize = GetPageSize(ctx);
        ULONG start = (ULONG)from & ~(pageSize - 1);
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
    if (err)
    {
        DFUNC(ERROR, "failed with %ld\n", err);
    }
    return err == 0;
}

