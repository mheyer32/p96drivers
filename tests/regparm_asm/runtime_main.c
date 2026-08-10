/*
 * Amiga runtime checks: C caller invokes C/C++ probes via function pointers
 * (same shape as Picasso96 BoardInfo hooks). Prototypes use __REG* so the
 * caller places args in the same registers the callees expect.
 */
#include <exec/types.h>
#include <proto/dos.h>

#include "regparm_macros.h"
#include "regparm_types.h"

#define SENTINEL 0xDEADBEEFUL

extern ULONG ASM rp_cxx_d7_enum_calc_bpr(__REGA0(APTR bi), __REGD0(ULONG width), __REGD1(ULONG height),
                                         __REGA1(APTR mi), __REGD7(RP_RGBFTYPE format));
extern ULONG ASM rp_cxx_d7_ulong_calc_bpr(__REGA0(APTR bi), __REGD0(ULONG width), __REGD1(ULONG height),
                                          __REGA1(APTR mi), __REGD7(ULONG format));
extern ULONG ASM rp_cxx_d7_enum_set_panning(__REGA0(APTR bi), __REGA1(APTR mem), __REGD0(UWORD width), __REGD1(WORD x),
                                            __REGD2(WORD y), __REGD7(RP_RGBFTYPE format));
extern ULONG ASM rp_c_d7_enum_calc_bpr(__REGA0(APTR bi), __REGD0(ULONG width), __REGD1(ULONG height), __REGA1(APTR mi),
                                       __REGD7(RP_RGBFTYPE format));
extern ULONG ASM rp_c_d7_enum_single(__REGD7(RP_RGBFTYPE v));
extern ULONG ASM rp_cxx_d7_enum_single(__REGD7(RP_RGBFTYPE v));
extern ULONG ASM rp_cxx_d7_ulong_single(__REGD7(ULONG v));

typedef ULONG ASM (*ProbeCalcBprEnumFn)(__REGA0(APTR bi), __REGD0(ULONG width), __REGD1(ULONG height), __REGA1(APTR mi),
                                        __REGD7(RP_RGBFTYPE format));
typedef ULONG ASM (*ProbeCalcBprUlongFn)(__REGA0(APTR bi), __REGD0(ULONG width), __REGD1(ULONG height),
                                         __REGA1(APTR mi), __REGD7(ULONG format));
typedef ULONG ASM (*ProbeSetPanningEnumFn)(__REGA0(APTR bi), __REGA1(APTR mem), __REGD0(UWORD width), __REGD1(WORD x),
                                           __REGD2(WORD y), __REGD7(RP_RGBFTYPE format));
typedef ULONG ASM (*ProbeSingleEnumFn)(__REGD7(RP_RGBFTYPE v));

static int g_fail;

static void report(const char *name, ULONG got, ULONG expect)
{
    if (got == expect)
        Printf("PASS  %s (got %lx)\n", (ULONG)name, got);
    else {
        Printf("FAIL  %s (got %lx, expected %lx)\n", (ULONG)name, got, expect);
        g_fail = 1;
    }
}

static void known_fail(const char *name, ULONG got, ULONG expect)
{
    if (got == expect)
        Printf("PASS  %s (got %lx) - unexpected: enum reg bind works?\n", (ULONG)name, got);
    else
        Printf("KNOWN %s (got %lx, expected %lx) - g++ enum attribute ignored\n", (ULONG)name, got, expect);
}

int main(void)
{
    APTR dummy = (APTR)0x12345678;

    Printf("TestRegParm runtime (C caller -> C/C++ callees)\n\n");

    report("C d7 enum single (direct)", rp_c_d7_enum_single((RP_RGBFTYPE)SENTINEL), SENTINEL);
    known_fail("C++ d7 enum single (direct)", rp_cxx_d7_enum_single((RP_RGBFTYPE)SENTINEL), SENTINEL);
    report("C++ d7 ulong single (direct)", rp_cxx_d7_ulong_single(SENTINEL), SENTINEL);
    report("C d7 enum calc_bpr (direct)", rp_c_d7_enum_calc_bpr(dummy, 640, 480, dummy, (RP_RGBFTYPE)SENTINEL),
           SENTINEL);
    known_fail("C++ d7 enum calc_bpr (direct)", rp_cxx_d7_enum_calc_bpr(dummy, 640, 480, dummy, (RP_RGBFTYPE)SENTINEL),
               SENTINEL);
    report("C++ d7 ulong calc_bpr (direct)", rp_cxx_d7_ulong_calc_bpr(dummy, 640, 480, dummy, SENTINEL), SENTINEL);

    {
        ProbeCalcBprEnumFn fn = rp_cxx_d7_enum_calc_bpr;
        known_fail("C++ d7 enum calc_bpr (fn ptr)", fn(dummy, 640, 480, dummy, (RP_RGBFTYPE)SENTINEL), SENTINEL);
    }
    {
        ProbeCalcBprUlongFn fn = rp_cxx_d7_ulong_calc_bpr;
        report("C++ d7 ulong calc_bpr (fn ptr)", fn(dummy, 640, 480, dummy, SENTINEL), SENTINEL);
    }
    {
        ProbeSetPanningEnumFn fn = rp_cxx_d7_enum_set_panning;
        known_fail("C++ d7 enum set_panning (fn ptr)", fn(dummy, dummy, 640, 0, 0, (RP_RGBFTYPE)SENTINEL), SENTINEL);
    }
    {
        ProbeSingleEnumFn fn = rp_cxx_d7_enum_single;
        known_fail("C++ d7 enum single (fn ptr)", fn((RP_RGBFTYPE)SENTINEL), SENTINEL);
    }

    Printf("\nRuntime done (%s).\n", (ULONG)(g_fail ? "unexpected FAIL" : "OK (known enum fails only)"));
    return g_fail;
}
