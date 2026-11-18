#include "mach64_i2c.h"
#include "chip_mach64.h"
#include "mach64_common.h"

using namespace MmioReg;
using namespace PllReg;

#ifdef __cplusplus
#ifndef _Static_assert
#define _Static_assert(cond, msg) static_assert(cond, msg)
#endif
extern "C" {
#endif

#include "edid_common.h"

/* LT_GIO is LCD indexed register 7 (LCD_INDEX @ 0x29, LCD_DATA @ 0x2A). */
#define LCD_IDX_LT_GIO 7

/* LT_GIO bitfields (Rage XL / LT Pro register guides). */
#define LT_GIO_GPIO_14     BIT(12)
#define LT_GIO_GPIO_15     BIT(13)
#define LT_GIO_GPIO_16     BIT(14)
#define LT_GIO_GPIO_DIR_14 BIT(28)
#define LT_GIO_GPIO_DIR_15 BIT(29)
#define LT_GIO_GPIO_DIR_16 BIT(30)
#define LT_GIO_DDC_IO_DRIVE BIT(31)

/*
 * GT / LT Pro style DDC bitbang (historical): GPIO14=SCL, GPIO15=SDA.
 * Rage XL CRT DDC is MONID[2:1] = GPIO[16:15]; GPIO14 is DIGON — do not drive.
 */
#define GT_SCL_DATA LT_GIO_GPIO_14
#define GT_SDA_DATA LT_GIO_GPIO_15
#define GT_SCL_DIR  LT_GIO_GPIO_DIR_14
#define GT_SDA_DIR  LT_GIO_GPIO_DIR_15

#define GM_SCL_DATA LT_GIO_GPIO_16 /* MONID2 */
#define GM_SDA_DATA LT_GIO_GPIO_15 /* MONID1 */
#define GM_SCL_DIR  LT_GIO_GPIO_DIR_16
#define GM_SDA_DIR  LT_GIO_GPIO_DIR_15

#define I2C_DELAY_US 5

static ULONG readLtGio(Mach64BlkIo &blk)
{
    blk.writeL(BlkIoReg::LCD_INDEX, LCD_IDX_LT_GIO);
    return blk.readL(BlkIoReg::LCD_DATA);
}

static void writeLtGio(Mach64BlkIo &blk, ULONG v)
{
    blk.writeL(BlkIoReg::LCD_INDEX, LCD_IDX_LT_GIO);
    blk.writeL(BlkIoReg::LCD_DATA, v);
}

static void i2cSetLine(struct BoardInfo *bi, ULONG dataBit, ULONG dirBit, BOOL high, BOOL checkStretch)
{
    Mach64BlkIo blk = asMach64(bi)->blkIo();
    ULONG lt_gio    = readLtGio(blk);

    if (high) {
        lt_gio &= ~dirBit;
        writeLtGio(blk, lt_gio);
        delayMicroSeconds(I2C_DELAY_US);

        if (checkStretch) {
            delayMicroSeconds(I2C_DELAY_US);
            lt_gio = readLtGio(blk);
            if (!(lt_gio & dataBit)) {
                int timeout = 100;
                while (timeout-- > 0) {
                    delayMicroSeconds(I2C_DELAY_US);
                    lt_gio = readLtGio(blk);
                    if (lt_gio & dataBit)
                        break;
                }
            }
        }
    } else {
        lt_gio |= dirBit;
        lt_gio &= ~dataBit;
        writeLtGio(blk, lt_gio);
        delayMicroSeconds(I2C_DELAY_US);
    }
}

static BOOL i2cReadLine(struct BoardInfo *bi, ULONG dataBit, ULONG dirBit)
{
    Mach64BlkIo blk = asMach64(bi)->blkIo();
    ULONG lt_gio    = readLtGio(blk);

    if (lt_gio & dirBit) {
        writeLtGio(blk, lt_gio & ~dirBit);
        delayMicroSeconds(I2C_DELAY_US);
        lt_gio = readLtGio(blk);
    }
    return (lt_gio & dataBit) != 0;
}

/* ---- GT path (GPIO14/15) ---- */

static BOOL mach64I2cInit(struct BoardInfo *bi)
{
    Mach64BlkIo blk = asMach64(bi)->blkIo();

    ULONG lt_gio = readLtGio(blk);
    lt_gio &= ~(GT_SCL_DIR | GT_SDA_DIR);
    lt_gio |= LT_GIO_DDC_IO_DRIVE;
    writeLtGio(blk, lt_gio);
    delayMicroSeconds(10);

    lt_gio = readLtGio(blk);
    if (lt_gio == 0) {
        D(WARN, "LT_GIO readback 0 after GT I2C init — skipping DDC\n");
        return FALSE;
    }
    D(VERBOSE, "I2C bus initialized GT (LT_GIO=0x%08lx)\n", lt_gio);
    return TRUE;
}

static void mach64I2cSetScl(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching)
{
    i2cSetLine(bi, GT_SCL_DATA, GT_SCL_DIR, high, checkClockStretching);
}

static void mach64I2cSetSda(struct BoardInfo *bi, BOOL high)
{
    i2cSetLine(bi, GT_SDA_DATA, GT_SDA_DIR, high, FALSE);
}

static BOOL mach64I2cReadScl(struct BoardInfo *bi)
{
    return i2cReadLine(bi, GT_SCL_DATA, GT_SCL_DIR);
}

static BOOL mach64I2cReadSda(struct BoardInfo *bi)
{
    return i2cReadLine(bi, GT_SDA_DATA, GT_SDA_DIR);
}

static const I2COps_t mach64_i2c_ops_gt = {.init    = mach64I2cInit,
                                           .setScl  = mach64I2cSetScl,
                                           .setSda  = mach64I2cSetSda,
                                           .readScl = mach64I2cReadScl,
                                           .readSda = mach64I2cReadSda};

/* ---- Rage XL / GM path: CRT DDC on MONID[2:1] via indexed LT_GIO ---- */

static BOOL mach64GmI2cInit(struct BoardInfo *bi)
{
    Mach64BlkIo blk = asMach64(bi)->blkIo();

    ULONG lt_gio = readLtGio(blk);
    /* Release MONID1/2 only; leave DIGON (GPIO14) alone. */
    lt_gio &= ~(GM_SCL_DIR | GM_SDA_DIR);
    lt_gio |= LT_GIO_DDC_IO_DRIVE;
    writeLtGio(blk, lt_gio);
    delayMicroSeconds(10);

    lt_gio = readLtGio(blk);
    if (lt_gio == 0) {
        D(WARN, "LT_GIO readback 0 after GM I2C init — skipping DDC\n");
        return FALSE;
    }
    if (!(lt_gio & GM_SCL_DATA) || !(lt_gio & GM_SDA_DATA)) {
        D(WARN, "GM DDC lines not high after release (LT_GIO=%08lx) — monitor/pull-ups?\n", lt_gio);
    }
    D(VERBOSE, "I2C bus initialized GM/XL (LT_GIO=0x%08lx)\n", lt_gio);
    return TRUE;
}

static void mach64GmI2cSetScl(struct BoardInfo *bi, BOOL high, BOOL checkClockStretching)
{
    i2cSetLine(bi, GM_SCL_DATA, GM_SCL_DIR, high, checkClockStretching);
}

static void mach64GmI2cSetSda(struct BoardInfo *bi, BOOL high)
{
    i2cSetLine(bi, GM_SDA_DATA, GM_SDA_DIR, high, FALSE);
}

static BOOL mach64GmI2cReadScl(struct BoardInfo *bi)
{
    return i2cReadLine(bi, GM_SCL_DATA, GM_SCL_DIR);
}

static BOOL mach64GmI2cReadSda(struct BoardInfo *bi)
{
    return i2cReadLine(bi, GM_SDA_DATA, GM_SDA_DIR);
}

static const I2COps_t mach64_i2c_ops_gm = {.init    = mach64GmI2cInit,
                                           .setScl  = mach64GmI2cSetScl,
                                           .setSda  = mach64GmI2cSetSda,
                                           .readScl = mach64GmI2cReadScl,
                                           .readSda = mach64GmI2cReadSda};

const I2COps_t *getI2COps(struct BoardInfo *bi)
{
    ChipData_t *cd = getChipData(bi);
    return cd->i2cOps;
}

BOOL queryEDID(struct BoardInfo *bi)
{
    ChipData_t *cd = getChipData(bi);
    const I2COps_t *ops = NULL;

    if (cd->chipFamily == MACH64GM) {
        ops = &mach64_i2c_ops_gm;
    }
    else if (cd->chipFamily == MACH64GT) {
        ops = &mach64_i2c_ops_gt;
    }
    if (!ops) {
        cd->i2cOps = NULL;
        D(INFO, "I2C/EDID not available for this chip family\n");
        return TRUE;
    }

    if (!ops->init(bi)) {
        cd->i2cOps = NULL;
        return TRUE;
    }

    cd->i2cOps = ops;
    D(INFO, "I2C ops ready for EDID (%s)\n", cd->chipFamily == MACH64GM ? "GM/XL MONID" : "GT GPIO14/15");

    UBYTE edid_data[EDID_BLOCK_SIZE];
    if (readEDID(bi, edid_data)) {
        D(INFO, "EDID read successfully for mach64\n");
    } else {
        D(WARN, "Failed to read EDID for mach64\n");
    }

    return TRUE;
}

#ifdef __cplusplus
}
#endif
