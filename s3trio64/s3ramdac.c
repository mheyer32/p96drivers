#include "s3ramdac.h"
#include "s3trio64_common.h"

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

#define DAC_IDX_LO   0x3C8
#define DAC_IDX_HI   0x3C9
#define DAC_IDX_DATA 0x3C6
#define DAC_IDX_CNTL 0x3C7

BOOL CheckForRGB524(struct BoardInfo *bi)
{
    REGBASE();

    /* probe for IBM RGB524 RAMDAC */
    /*
     * The IBM RGB524 RAMDAC has a dedicated Product Identification Code register
     * at index 0x0001 that returns 0x02. This is the proper way to identify the chip.
     * The RGB524 uses direct register access via RS[2:0] and D[7:0] data bus.
     */
    BOOL found = FALSE;

    // Set up for RAMDAC direct register access
    // The RGB524 uses RS[2:0] for register selection and D[7:0] for data
    DAC_ENABLE_RS2();

    // Access the Product Identification Code register (index 0x0001)
    // According to the datasheet, this register should return 0x02 for RGB524
    W_REG(DAC_IDX_HI, 0x00);
    W_REG(DAC_IDX_LO, 0x00);                // Dac Register index 0x0000
    UBYTE revision = R_REG(DAC_IDX_DATA);   // Read the Revision
    W_REG(DAC_IDX_LO, 0x01);                // Dac register index 0x0001
    UBYTE productId = R_REG(DAC_IDX_DATA);  // Read the Product ID value

    if (productId == 0x02) {
        DFUNC(0, "Found IBM RGB524 RAMDAC (Product ID: 0x%02x)\n", productId);
        found = TRUE;
    } else {
        DFUNC(0, "RGB524 not detected, Product ID: 0x%02x (expected: 0x02)\n", productId);
    }

    DAC_DISABLE_RS2();

    return found;
}

BOOL InitRGB524(struct BoardInfo *bi)
{
    REGBASE();

    /* probe for IBM RGB524 RAMDAC */
    /*
     * The IBM RGB524 RAMDAC has a dedicated Product Identification Code register
     * at index 0x0001 that returns 0x02. This is the proper way to identify the chip.
     * The RGB524 uses direct register access via RS[2:0] and D[7:0] data bus.
     */
    BOOL found = FALSE;

    // Set up for RAMDAC direct register access
    // The RGB524 uses RS[2:0] for register selection and D[7:0] for data
    DAC_ENABLE_RS2();

    W_REG(DAC_IDX_CNTL, 0x01);  // auto-increment register indices
    W_REG(DAC_IDX_HI, 0x00);

    W_REG(DAC_IDX_LO, 0x06);  // DAC operation
    W_REG(DAC_DATA, BIT(0));  // Blanking pedestal

    W_REG(DAC_IDX_LO, 0x0a);  // Pixel Format
    W_REG(DAC_DATA, 0xb011);  // 8Bit

    W_REG(DAC_IDX_LO, 0x0b);  // 8 Bit Pixel Control
    W_REG(DAC_DATA, 0);       // Indirect through Palette enabled

    W_REG(DAC_IDX_LO, 0x0c);  // 16 Bit Pixel Control
    W_REG(DAC_DATA, (0xb11 << 6) | BIT(2) | BIT(1));  // Fill low order 0 bits with high order bits and 565 mode

    W_REG(DAC_IDX_LO, 0x0d);  // 24 Bit Pixel Control
    W_REG(DAC_DATA, 0x01);  // Direct, palette bypass

    W_REG(DAC_IDX_LO, 0x0e);  // 32 Bit Pixel Control
    W_REG(DAC_DATA,  BIT(2) | (0xb11));  // Direct, Palette bypass


}
