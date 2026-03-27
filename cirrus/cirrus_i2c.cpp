#include "cirrus_common.h"
#include "edid_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DDC2B on CL-GD543x/4x (Alpine): sequencer SR8 — see cl-gd543-4x_technical_1425231213.txt §9.3, Appendix B2.
 * With SR8[6]=1 (DDC2B vs EEPROM bit meanings): EECS drives MID[3] (SCL), EEDI drives MID[1] (SDA).
 * Outputs: bit0=SCL (1=hi-Z), bit1=SDA (1=hi-Z). Readbacks: bit2=SCL sense, bit7=SDA sense.
 */

#define SR8_IDX       0x08
#define SR8_DDC2B_EN  BIT(6)
#define SR8_SCL_OUT   BIT(0)
#define SR8_SDA_OUT   BIT(1)
#define SR8_SCL_IN    BIT(2)
#define SR8_SDA_IN    BIT(7)

#define I2C_DELAY_US 5

static BOOL i2cInit(struct BoardInfo *bi)
{
    VgaIo vga = asCirrus(bi)->vga();

    UBYTE v = vga.readSR(SR8_IDX);
    v |= SR8_DDC2B_EN | SR8_SCL_OUT | SR8_SDA_OUT;
    vga.writeSR(SR8_IDX, v);
    delayMicroSeconds(10);

    D(VERBOSE, "Cirrus DDC2B: SR8=0x%02lx\n", (ULONG)v);
    return TRUE;
}

static void i2cSetScl(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching)
{
    VgaIo vga = asCirrus(bi)->vga();

    UBYTE v = vga.readSR(SR8_IDX);
    v |= SR8_DDC2B_EN;
    if (high)
        v |= SR8_SCL_OUT;
    else
        v &= (UBYTE)~SR8_SCL_OUT;
    vga.writeSR(SR8_IDX, v);
    delayMicroSeconds(I2C_DELAY_US);

    if (high && checkClockStretching) {
        int timeout = 100;
        while (timeout-- > 0) {
            UBYTE r = vga.readSR(SR8_IDX);
            if ((r & SR8_SCL_IN) != 0)
                break;
            delayMicroSeconds(I2C_DELAY_US);
        }
        if (timeout <= 0)
            D(ERROR, "Cirrus I2C: SCL stuck low (SR8=0x%02lx)\n", (ULONG)vga.readSR(SR8_IDX));
    }
}

static void i2cSetSda(struct BoardInfo *bi, BOOL high)
{
    VgaIo vga = asCirrus(bi)->vga();

    UBYTE v = vga.readSR(SR8_IDX);
    v |= SR8_DDC2B_EN;
    if (high)
        v |= SR8_SDA_OUT;
    else
        v &= (UBYTE)~SR8_SDA_OUT;
    vga.writeSR(SR8_IDX, v);
    delayMicroSeconds(I2C_DELAY_US);
}

static BOOL i2cReadScl(struct BoardInfo *bi)
{
    VgaIo vga = asCirrus(bi)->vga();
    return (vga.readSR(SR8_IDX) & SR8_SCL_IN) != 0;
}

static BOOL i2cReadSda(struct BoardInfo *bi)
{
    VgaIo vga = asCirrus(bi)->vga();
    return (vga.readSR(SR8_IDX) & SR8_SDA_IN) != 0;
}

static const I2COps_t i2c_ops = {
    .init    = i2cInit,
    .setScl  = i2cSetScl,
    .setSda  = i2cSetSda,
    .readScl = i2cReadScl,
    .readSda = i2cReadSda,
};

const I2COps_t *getI2COps(struct BoardInfo *bi)
{
    (void)bi;
    return &i2c_ops;
}

void queryEDID(BoardInfo_t *bi)
{
    UBYTE edid_data[EDID_BLOCK_SIZE];

    DFUNC(INFO, "Cirrus: read EDID (DDC2B, SR8)\n");

    if (!readEDID(bi, edid_data)) {
        DFUNC(INFO, "Cirrus: EDID unavailable or read failed\n");
        return;
    }

    char manufacturer[4];
    char product_name[14];

    getEDIDManufacturer(edid_data, manufacturer);
    DFUNC(INFO, "EDID manufacturer: %s\n", manufacturer);

    if (getEDIDProductName(edid_data, product_name))
        DFUNC(INFO, "EDID product: %s\n", product_name);

    DFUNC(INFO, "EDID version %d.%d, week %d, year %d\n", (int)edid_data[18], (int)edid_data[19], (int)edid_data[16],
          (int)edid_data[17] + 1990);

    queryEDIDResolutions(edid_data);
}

#ifdef __cplusplus
}
#endif
