#include "s3ramdac.h"
#include "chip_s3trio64.h"

BOOL CheckForSDAC(struct BoardInfo *bi)
{
    REGBASE();

    /* probe for S3 GENDAC or SDAC */
    /*
     * S3 GENDAC and SDAC have two fixed read only PLL clocks
     *     CLK0 f0: 25.255MHz   M-byte 0x28  N-byte 0x61
     *     CLK0 f1: 28.311MHz   M-byte 0x3d  N-byte 0x62
     * which can be used to detect GENDAC and SDAC since there is no chip-id
     * for the GENDAC.
     *
     * NOTE: for the GENDAC on a MIRO 10SD (805+GENDAC) reading PLL values
     * for CLK0 f0 and f1 always returns 0x7f (but is documented "read only")
     */

    UBYTE saveCR55, saveCR45, saveCR43, savelut[6];
    ULONG clock01, clock23;
    BOOL found = FALSE;

    // Set OLD RS2 to 0
    saveCR43 = R_CR(0x43);
    W_CR_MASK(0x43, 0x20, 0);

    // ??
//    saveCR45 = R_CR(0x45);
//    W_CR_MASK(0x45, 0x20, 0);

    saveCR55 = R_CR(0x55);
    // Extended RAMDAC Control Register (EX_DAC_CT) (CR55)
    // Make RS2 take on the value o CR43 (see above)
    W_CR_MASK(0x55, 0x01, 0);

    // This is done to restore the state of a RAMDAC, in case its not an SDAC
    // Read Index
    W_REG(0x3c7, 0);
    for (int i = 0; i < 2 * 3; i++) /* save first two LUT entries */
        savelut[i] = R_REG(0x3c9);
    W_REG(0x3c8, 0);
    for (int i = 0; i < 2 * 3; i++) /* set first two LUT entries to zero */
        W_REG(0x3c9, 0);

    // Force RS2 = 1
    W_CR_MASK(0x55, 0x01, 0x01);

    // Read Index
    W_REG(0x3c7, 0);
    for (int i = clock01 = 0; i < 4; i++)
        clock01 = (clock01 << 8) | R_REG(0x3c9);
    for (int i = clock23 = 0; i < 4; i++)
        clock23 = (clock23 << 8) | R_REG(0x3c9);

    // Back to regular palette access?
    W_CR_MASK(0x55, 0x01, 0);

    W_REG(0x3c8, 0);
    for (int i = 0; i < 2 * 3; i++) /* restore first two LUT entries */
        W_REG(0x3c9, savelut[i]);

    W_CR(0x55, saveCR55);

    if (clock01 == 0x28613d62 || (clock01 == 0x7f7f7f7f && clock23 != 0x7f7f7f7f)) {
        W_REG(0x3c8, 0);
        // Reading DAC Mask returns chip ID?
        R_REG(0x3c6);
        R_REG(0x3c6);
        R_REG(0x3c6);

        /* the fourth read will show the SDAC chip ID and revision */
        if ((R_REG(0x3c6) & 0xf0) == 0x70) {
            DFUNC(0, "Found S3 SDAC\n");
            saveCR43 &= ~0x02;
            saveCR45 &= ~0x20;
        } else
            DFUNC(0, "Found S3 GENDAC\n");
        {
            saveCR43 &= ~0x02;
            saveCR45 &= ~0x20;
        }
        W_REG(0x3c8, 0);

        found = TRUE;
    } else {
        DFUNC(0, "Unknown RAMDAC\n");
    }

//    W_CR(0x45, saveCR45);
    W_CR(0x43, saveCR43);

    return found;
}

