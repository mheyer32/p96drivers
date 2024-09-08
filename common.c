#include "common.h"
#include <hardware/cia.h>
#include <clib/debug_protos.h>

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
#endif



// Compute PLL parameters accordng to
// f_wanted_kHz = f_base * m / n * 1/(2^r)

// f_wanted_khz is in Khz
int svga_compute_pll(const struct svga_pll *pll, ULONG f_wanted_khz, USHORT *m, USHORT *n, USHORT *r)
{
    USHORT am, an, ar;
    ULONG f_vco, f_current, delta_current, delta_best;

    DFUNC(8, "ideal frequency: %ld kHz\n", (unsigned int)f_wanted_khz);

    ar = pll->r_max;
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
    *m = 0;
    *n = 0;
    *r = ar;

    am = pll->m_min;
    an = pll->n_min;

    while ((am <= pll->m_max) && (an <= pll->n_max)) {
        f_current = (pll->f_base * am) / an;
        delta_current = abs_diff(f_current, f_vco);

        if (delta_current < delta_best) {
            delta_best = delta_current;
            *m = am;
            *n = an;
        }

        if (f_current <= f_vco) {
            am++;
        } else {
            an++;
        }
    }

    f_current = (pll->f_base * *m) / *n;

    D(15, "found frequency: %ld kHz (VCO %ld kHz)\n", (int)(f_current >> ar), (int)f_current);
    D(15, "m = %ld n = %ld r = %ld\n", (unsigned int)*m, (unsigned int)*n, (unsigned int)*r);

    return (f_current >> ar);
}

void delayMicroSeconds(ULONG us)
{
    UWORD count = ((us << 4) + 15) / (UWORD)22;
    // CIA access has deterministic speed, use it for a short delay
    extern volatile FAR struct CIA ciaa;
    for (UWORD i = 0; i < count; ++i) {
        UBYTE x = ciaa.ciapra;
    }
}

void deleyMilliSeconds(ULONG ms)
{
    delayMicroSeconds(ms * 1000);
}
