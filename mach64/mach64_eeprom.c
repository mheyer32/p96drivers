#include "mach64_eeprom.h"
#include "mach64_common.h"

/*
 * Mach64 external Microwire EEPROM via GEN_TEST_CNTL (ATI.TXT / GX VBIOS eeprom_ReadWord_BX).
 * Map + CRTC parameter tables: ATI.TXT “EEPROM Data structure” / “CRTC Parameter Table”.
 */

#define GEN_EE_DATA_OUT BIT(0)
#define GEN_EE_CLOCK    BIT(1)
#define GEN_EE_CHIP_SEL BIT(2)
#define GEN_EE_DATA_IN  BIT(3)
#define GEN_EE_EN       BIT(4)
#define GEN_EE_MASK     (GEN_EE_DATA_OUT | GEN_EE_CLOCK | GEN_EE_CHIP_SEL | GEN_EE_EN)

#define EEPROM_NUM_WORDS 128u

/* Word offsets of up to 7 CRTC parameter tables (ATI.TXT). Each table is 15 words. */
static const UWORD g_crtcTableOff[] = {0x17, 0x26, 0x35, 0x44, 0x53, 0x62, 0x71};

#ifndef DBG

void dumpMach64Eeprom(struct BoardInfo *bi)
{
    (void)bi;
}

#else

/* RMW only GEN_EE_* bits; quiet MMIO (bit-bang is chatty if logged). */
static ULONG eeSetBits(BoardInfo_t *bi, ULONG set, ULONG clear)
{
    MMIOBASE();
    ULONG v = R_MMIO_L_QI(GEN_TEST_CNTL);
    v       = (v & ~clear) | set;
    W_MMIO_L_QI(GEN_TEST_CNTL, v);
    delayMicroSeconds(2);
    return R_MMIO_L_QI(GEN_TEST_CNTL);
}

static ULONG eeClock(BoardInfo_t *bi)
{
    eeSetBits(bi, GEN_EE_CLOCK, 0);
    return eeSetBits(bi, 0, GEN_EE_CLOCK);
}

static UWORD eeReadWord(BoardInfo_t *bi, UBYTE addr)
{
    ULONG cur;
    UWORD data;
    int i;

    /* Idle then enable interface (VBIOS sequence). */
    eeSetBits(bi, 0, GEN_EE_MASK);
    eeClock(bi);
    eeSetBits(bi, GEN_EE_EN, GEN_EE_DATA_OUT);
    eeClock(bi);
    eeSetBits(bi, GEN_EE_CHIP_SEL, 0);
    eeClock(bi);

    /* Microwire READ opcode 110, then 8-bit address MSB first. */
    for (i = 2; i >= 0; i--) {
        if ((6 >> i) & 1)
            eeSetBits(bi, GEN_EE_DATA_OUT, 0);
        else
            eeSetBits(bi, 0, GEN_EE_DATA_OUT);
        eeClock(bi);
    }
    for (i = 7; i >= 0; i--) {
        if ((addr >> i) & 1)
            eeSetBits(bi, GEN_EE_DATA_OUT, 0);
        else
            eeSetBits(bi, 0, GEN_EE_DATA_OUT);
        eeClock(bi);
    }

    eeSetBits(bi, 0, GEN_EE_DATA_OUT);
    eeClock(bi);

    data = 0;
    for (i = 0; i < 16; i++) {
        cur = eeClock(bi);
        data <<= 1;
        if (cur & GEN_EE_DATA_IN)
            data |= 1;
    }

    eeSetBits(bi, 0, GEN_EE_CHIP_SEL);
    eeClock(bi);
    eeSetBits(bi, 0, GEN_EE_EN);
    delayMicroSeconds(5);
    return data;
}

static void dumpCrtcTable(const UWORD *buf, UWORD wordOff, int idx)
{
    const UWORD *t = &buf[wordOff];
    UWORD w1 = t[1], w2 = t[2];

    D(ALWAYS, "CRTC table %ld @ word 0x%02lx: %04lx %04lx %04lx %04lx %04lx %04lx %04lx %04lx\n", (ULONG)idx,
      (ULONG)wordOff, (ULONG)t[0], (ULONG)t[1], (ULONG)t[2], (ULONG)t[3], (ULONG)t[4], (ULONG)t[5], (ULONG)t[6],
      (ULONG)t[7]);
    D(ALWAYS, "  cont: %04lx %04lx %04lx %04lx %04lx %04lx %04lx\n", (ULONG)t[8], (ULONG)t[9], (ULONG)t[10],
      (ULONG)t[11], (ULONG)t[12], (ULONG)t[13], (ULONG)t[14]);

    if ((w1 & 0xFF) == 0x80) {
        D(ALWAYS,
          "  accel: modeSel=0x%02lx flags=0x%04lx dbl=%ld int=%ld mux=%ld\n"
          "  H tot=%ld disp=%ld sync=%ld wid=%ld\n"
          "  V tot=%ld disp=%ld sync=%ld wid=%ld clk=0x%02lx dot=%ld\n",
          (ULONG)(w1 >> 8), (ULONG)w2, (ULONG)!!(w2 & BIT(8)), (ULONG)!!(w2 & BIT(9)), (ULONG)!!(w2 & BIT(13)),
          (ULONG)(t[3] & 0xFF), (ULONG)(t[3] >> 8), (ULONG)(t[4] & 0xFF), (ULONG)(t[4] >> 8), (ULONG)(t[5] & 0x7FF),
          (ULONG)(t[6] & 0x7FF), (ULONG)(t[7] & 0x7FF), (ULONG)(t[8] & 0xFF), (ULONG)(t[8] >> 8), (ULONG)t[9]);
    } else {
        D(ALWAYS, "  vga/other: dbl=%ld int=%ld\n", (ULONG)!!(w2 & BIT(8)), (ULONG)!!(w2 & BIT(9)));
    }
}

void dumpMach64Eeprom(struct BoardInfo *bi)
{
    UWORD buf[EEPROM_NUM_WORDS];
    UWORD w;
    ULONG saved;
    UBYTE sum;
    unsigned i;

    MMIOBASE();
    saved = R_MMIO_L_QI(GEN_TEST_CNTL);

    DFUNC(ALWAYS, "Mach64 EEPROM (GEN_TEST_CNTL Microwire, %lu words):\n", (ULONG)EEPROM_NUM_WORDS);

    for (w = 0; w < EEPROM_NUM_WORDS; w++)
        buf[w] = eeReadWord(bi, (UBYTE)w);

    W_MMIO_L_QI(GEN_TEST_CNTL, saved);

    for (w = 0; w < EEPROM_NUM_WORDS; w += 8) {
        D(ALWAYS, "  %02lx:", (ULONG)w);
        for (i = 0; i < 8 && (w + i) < EEPROM_NUM_WORDS; i++)
            D(ALWAYS, " %04lx", (ULONG)buf[w + i]);
        D(ALWAYS, "\n");
    }

    sum = 0;
    for (w = 0; w < EEPROM_NUM_WORDS; w++) {
        sum += (UBYTE)(buf[w] & 0xFF);
        sum += (UBYTE)(buf[w] >> 8);
    }
    D(ALWAYS, "header: writes=%ld cksum_byte=0x%02lx (sum8=0x%02lx) rev=%ld\n", (ULONG)buf[0],
      (ULONG)(buf[1] & 0xFF), (ULONG)sum, (ULONG)(buf[3] & 0xF));

    for (i = 0; i < sizeof(g_crtcTableOff) / sizeof(g_crtcTableOff[0]); i++) {
        if ((ULONG)g_crtcTableOff[i] + 15u > EEPROM_NUM_WORDS)
            break;
        dumpCrtcTable(buf, g_crtcTableOff[i], (int)i);
    }
}

#endif /* DBG */
