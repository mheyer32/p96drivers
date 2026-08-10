#include "chip_mach32.h"

#ifdef __cplusplus
extern "C" {
#endif

#define __NOLIBBASE__

#include <exec/types.h>
#include <proto/exec.h>

/*
 * Mach32 external serial EEPROM (Microwire) via EXT_GE_CONFIG[15:12] and EXT_GE_STATUS[14]
 * (EE_DATA_IN). REG688000-15 §9-17/9-18, §9-68; EEPROM map Appendix C.
 * Boards often use ST M93C56 (marking e.g. C56M1): 2 Kbit, 128x16 in x16 org — READ uses
 * 10 serial bits (110 + A6..A0), not the 93C46 9-bit (64-word) sequence.
 * CRT should be held in reset (DISP_CNTL[6:5]=2) during access; ALIAS_ENA must be 0.
 */

#define EE_DATA_OUT_BIT 12
#define EE_CLK_BIT      13
#define EE_CS_BIT       14
#define EE_SELECT_BIT   15

#define EXT_GE_EE_MASK (BIT(EE_DATA_OUT_BIT) | BIT(EE_CLK_BIT) | BIT(EE_CS_BIT) | BIT(EE_SELECT_BIT))
#define EE_DATA_IN_BIT 14 /* EXT_GE_STATUS */

/* M93C56 x16 (128 words): READ = 1 + 10 + A6..A0 = 10 bits MSB first; prefix bits 9..7 = 110. */
#define EEPROM_CMD_READ_PREFIX_M93C56 0x300u

#ifndef DBG

void dumpMach32Eeprom(BoardInfo_t *bi)
{
    (void)bi;
}

#else

static void eeWriteExtGe(BoardInfo_t *bi, UWORD baseNoEe, UWORD eeBits)
{
    DRIVER_LOCALS(bi);
    io.writeW(IoReg::id_EXT_GE_CONFIG, (UWORD)(baseNoEe | (eeBits & EXT_GE_EE_MASK)));
    delayMicroSeconds(2);
}

static void eeSendBit(BoardInfo_t *bi, UWORD baseNoEe, int di)
{
    UWORD lo = (UWORD)(baseNoEe | BIT(EE_SELECT_BIT) | BIT(EE_CS_BIT) | (di ? BIT(EE_DATA_OUT_BIT) : 0));
    eeWriteExtGe(bi, baseNoEe, lo);
    eeWriteExtGe(bi, baseNoEe, (UWORD)(lo | BIT(EE_CLK_BIT)));
    eeWriteExtGe(bi, baseNoEe, lo);
}

static int eeReadDataBit(BoardInfo_t *bi, UWORD baseNoEe)
{
    DRIVER_LOCALS(bi);
    UWORD lo = (UWORD)(baseNoEe | BIT(EE_SELECT_BIT) | BIT(EE_CS_BIT));
    eeWriteExtGe(bi, baseNoEe, lo);
    eeWriteExtGe(bi, baseNoEe, (UWORD)(lo | BIT(EE_CLK_BIT)));
    UWORD st = io.readW(IoReg::id_EXT_GE_STATUS);
    eeWriteExtGe(bi, baseNoEe, lo);
    return (st & BIT(EE_DATA_IN_BIT)) ? 1 : 0;
}

static UWORD eeReadWordM93c56x16(BoardInfo_t *bi, UWORD baseNoEe, UBYTE addr7)
{
    UWORD cmd10 = (UWORD)(EEPROM_CMD_READ_PREFIX_M93C56 | (addr7 & 0x7Fu));
    int i;

    eeWriteExtGe(bi, baseNoEe, (UWORD)(BIT(EE_SELECT_BIT)));
    delayMicroSeconds(10);
    eeWriteExtGe(bi, baseNoEe, (UWORD)(BIT(EE_SELECT_BIT) | BIT(EE_CS_BIT)));

    for (i = 9; i >= 0; i--) {
        eeSendBit(bi, baseNoEe, (cmd10 >> i) & 1);
    }

    (void)eeReadDataBit(bi, baseNoEe);

    {
        UWORD data = 0;
        for (i = 0; i < 16; i++) {
            data <<= 1;
            data |= (UWORD)(eeReadDataBit(bi, baseNoEe) & 1);
        }
        eeWriteExtGe(bi, baseNoEe, (UWORD)(BIT(EE_SELECT_BIT)));
        delayMicroSeconds(5);
        return data;
    }
}

void dumpMach32Eeprom(BoardInfo_t *bi)
{
    enum
    {
        NUM_WORDS = 128u
    };

    DRIVER_LOCALS(bi);

    UWORD geSaved  = io.readW(IoReg::id_R_EXT_GE_CONFIG);
    UWORD baseNoEe = (UWORD)(geSaved & (UWORD) ~(EXT_GE_EE_MASK | BIT(3)));

    io.writeMaskW(IoReg::id_DISP_CNTL, ENA_DISPLAY_MASK, CRT_RESET);

    DFUNC(ALWAYS, "Mach32 EEPROM (M93C56/C56-class Microwire read, %u words):\n", (unsigned)NUM_WORDS);

    {
        UWORD buf[128];
        UWORD w;

        for (w = 0; w < NUM_WORDS; w++) {
            buf[w] = eeReadWordM93c56x16(bi, baseNoEe, (UBYTE)w);
        }

        for (w = 0; w < NUM_WORDS; w += 4u) {
            DFUNC(ALWAYS, "  %02x: %04lx %04lx %04lx %04lx\n", (unsigned)w, (ULONG)buf[w], (ULONG)buf[w + 1u],
                  (ULONG)buf[w + 2u], (ULONG)buf[w + 3u]);
        }
    }

    io.writeW(IoReg::id_EXT_GE_CONFIG, geSaved);
    io.writeMaskW(IoReg::id_DISP_CNTL, ENA_DISPLAY_MASK, CRT_ENABLED);
}

#endif /* DBG */

#ifdef __cplusplus
}
#endif
