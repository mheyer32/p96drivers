#include <common.h>
#include <string.h>  // memcmp

#define VENDOR 2195
#define DEVICE_PICCOLO_IO 6
#define DEVICE_PICCOLO_FB 5

/******************************************************************************/
/*                                                                            */
/* library exports                                                                    */
/*                                                                            */
/******************************************************************************/

const char LibName[]     = "Piccolo.card";
const char LibIdString[] = "Piccolo2.1 Picasso96 card driver version 1.0";

const UWORD LibVersion  = 1;
const UWORD LibRevision = 0;

/*******************************************************************************/

int debugLevel = CHATTY;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef TESTEXE

#include <boardinfo.h>
#include <proto/expansion.h>
#include <proto/prometheus.h>
#include <proto/utility.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/dos.h>

APTR findDevice(int device)
{
    struct ConfigDev *cd;

    APTR legacyIOBase = NULL;
    if (cd = FindConfigDev(NULL, VENDOR, device))
        legacyIOBase = cd->cd_BoardAddr;

    return legacyIOBase;
}

int main()
{
    int rval = EXIT_FAILURE;

    D(0, "Looking for Piccolo card IO\n");

    APTR legacyIOBase = NULL;
    if (!(legacyIOBase = findDevice(DEVICE_PICCOLO_IO))) {
        D(0, "Unable to find legacy IO base\n");
        goto exit;
    }

    D(0, "Taking card out of reset\n");

    volatile UBYTE *ioBase = (volatile UBYTE *)legacyIOBase;
    struct BoardInfo boardInfo;
    struct BoardInfo *bi = &boardInfo;
    bi->RegisterBase = ioBase + REGISTER_OFFSET;
    REGBASE();

    // Reset card
    W_REG(0x8000, 0x01);
    delayMicroSeconds(500);
    W_REG(0x8000,0x11);

    APTR memoryBase = NULL;
    if (!(memoryBase = findDevice(DEVICE_PICCOLO_FB))) {
        D(0, "Unable to find FB memory base\n");
        goto exit;
    }

    D(0, "Memory Base at 0x%08lx\n", memoryBase);
    bi->MemoryBase = memoryBase;

    // Chip Wakeup
    W_REG(0x46E8, 0x16);
    W_REG(0x102, 0x01);
    W_REG(0x46E8, 0xE);

    // W_REG(0x3C3, 0x01);

    // Color mode, enable display memory
    W_MISC_MASK(0x03, 0x03);

    W_SR(0x6, 0x12); // Unlock extended registers

    UBYTE memCfg = R_SR(0xF);
    memCfg &= 7;
    D(VERBOSE, "SR_F: 0x%lx\n", (LONG)memCfg);

    memCfg |= BIT(7); // 2MB, Configure for 4 16bit DRAMS /OE becomes /RAS1
    memCfg |= BIT(4); // 32Bit data bus width, 2MB
    memCfg |= BIT(5); // CRT FIFO is 16 levels, 64bytes
    W_SR(0xF, memCfg);

    W_SR(0x7, 0x80); // Memory Window 3 (Piccolo pulls A23 high)
    W_SR(0x4, 0x02); // Extended Memory Mode, make all 2MB available
    W_GR(0xB, 0x20); // 'Offset Granularity' -> in combination with linear addressing, 2mb window


    BYTE deviceId = R_CR(0x27) >> 2;

    D(0, "Device ID: 0x%lx\n", (LONG)deviceId);

    if (deviceId != 0x24) {
        D(0, "Device ID does not match 0x24 for a GD5426\n");
    }

    // Get minimal VCLK going to get memory refresh
    W_SR(0x1, 0x29);
    W_MISC_MASK(0x0C, 0x00);
    W_SR(0x0B, 0x0);
    W_SR(0x1B, 0x3f);
    W_CR(0x11, 0x40);
    W_CR(0x00, 0x09);
    W_CR(0x01, 0x02);
    W_CR(0x04, 0x03);
    W_CR(0x05, 0x02);

    // program MCLK to 7.14Mhz
    W_SR(0x1F, 0x04);

    rval = EXIT_SUCCESS;

exit:
    return rval;
}
#endif  // TESTEXE
