#include "common.h"
#include "vga_aperture.hpp"

#include <libraries/configvars.h>
#include <libraries/expansion.h>
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/openpci.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility/tagitem.h>

#define PHASE5_MANUFACTURER_ID      8512
#define CYBERVISION64_PRODUCT_ID    34
#define CYBERVISION3D_PRODUCT_ID_Z3 67
#define CYBERVISION3D_PRODUCT_ID_Z2 50

#define VENDOR_ID_S3 0x5333

#define S3_TRIO32_64      0x8810
#define S3_TRIO32_64_V2   0x8811
#define S3_AURORA64PLUS   0x8812
#define S3_TRIO32_64_V3   0x8813
#define S3_TRIO64UVPLUS   0x8814
#define S3_TRIO64V2_DX    0x8900
#define S3_TRIO64V2_DXGX  0x8901
#define S3_TRIO64PLUS_MIN 0x8905
#define S3_TRIO64PLUS_MAX 0x890F

#define S3_VIRGE_325    0x5631
#define S3_VIRGE_VX     0x883D
#define S3_VIRGE_DXGX   0x8A01
#define S3_VIRGE_GX2    0x8A10
#define S3_VIRGE_GX2P_A 0x8A11
#define S3_VIRGE_GX2P_B 0x8A12

#define CV3D_REGISTER_OFFSET 0x02000000
#define MIN_MCLK_KHZ         20000
#define MAX_MCLK_KHZ         100000

LOCAL_DEBUGLEVEL(INFO);

typedef enum CardKind
{
    CARD_NONE,
    CARD_ZORRO_S3,
    CARD_PCI_S3
} CardKind_t;

typedef struct S3MclkCard
{
    CardKind_t kind;
    const char *name;
    volatile UBYTE *regBase;
    struct Library *ExpansionBase;
    struct Library *OpenPciBase;
    struct pci_dev *pciBoard;
    UWORD pciCommand;
    BOOL restorePciCommand;
} S3MclkCard_t;

static const struct svga_pll virgeMclkPll = {3, 129, 3, 33, 0, 3, 135000, 270000, 14318};

static BOOL isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static const char *skipSpaces(const char *arg)
{
    while (isSpace(*arg)) {
        arg++;
    }
    return arg;
}

static BOOL onlySpaces(const char *arg)
{
    while (*arg) {
        if (!isSpace(*arg)) {
            return FALSE;
        }
        arg++;
    }
    return TRUE;
}

static BOOL isOption(const char *arg, char option)
{
    arg = skipSpaces(arg);
    if (*arg == '-' || *arg == '/') {
        arg++;
    }
    return (arg[0] == option || arg[0] == option - ('a' - 'A')) && onlySpaces(arg + 1);
}

static void copyArg(char *dst, ULONG dstSize, const char *src)
{
    ULONG i = 0;

    src = skipSpaces(src);
    while (i + 1 < dstSize && src[i] && !isSpace(src[i])) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static BOOL parseNumber(const char *arg, ULONG *value)
{
    ULONG result  = 0;
    const char *p = skipSpaces(arg);

    if (!*p) {
        return FALSE;
    }

    while (*p >= '0' && *p <= '9') {
        result = (result * 10) + (*p - '0');
        p++;
    }

    if (!onlySpaces(p)) {
        return FALSE;
    }

    *value = result;
    return TRUE;
}

static BOOL parseClockKhz(const char *arg, ULONG *clockKhz)
{
    ULONG mhz     = 0;
    ULONG frac    = 0;
    ULONG scale   = 100;
    const char *p = skipSpaces(arg);

    if (!*p) {
        return FALSE;
    }

    while (*p >= '0' && *p <= '9') {
        mhz = (mhz * 10) + (*p - '0');
        p++;
    }

    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9' && scale) {
            frac += (*p - '0') * scale;
            scale /= 10;
            p++;
        }
        while (*p >= '0' && *p <= '9') {
            p++;
        }
    }

    if (!onlySpaces(p) || (mhz == 0 && frac == 0)) {
        return FALSE;
    }

    *clockKhz = mhz * 1000 + frac;
    return TRUE;
}

static BOOL isSupportedPciDevice(ULONG deviceId)
{
    if (deviceId >= S3_TRIO64PLUS_MIN && deviceId <= S3_TRIO64PLUS_MAX) {
        return TRUE;
    }

    switch (deviceId) {
    case S3_TRIO32_64:
    case S3_TRIO32_64_V2:
    case S3_AURORA64PLUS:
    case S3_TRIO32_64_V3:
    case S3_TRIO64UVPLUS:
    case S3_TRIO64V2_DX:
    case S3_TRIO64V2_DXGX:
    case S3_VIRGE_325:
    case S3_VIRGE_VX:
    case S3_VIRGE_DXGX:
    case S3_VIRGE_GX2:
    case S3_VIRGE_GX2P_A:
    case S3_VIRGE_GX2P_B:
        return TRUE;
    default:
        return FALSE;
    }
}

static const char *pciS3Name(ULONG deviceId)
{
    if (deviceId >= S3_TRIO64PLUS_MIN && deviceId <= S3_TRIO64PLUS_MAX) {
        return "S3 Trio64+";
    }

    switch (deviceId) {
    case S3_TRIO32_64:
    case S3_TRIO32_64_V2:
    case S3_TRIO32_64_V3:
        return "S3 Trio32/64";
    case S3_AURORA64PLUS:
        return "S3 Aurora64+";
    case S3_TRIO64UVPLUS:
        return "S3 Trio64UV+";
    case S3_TRIO64V2_DX:
    case S3_TRIO64V2_DXGX:
        return "S3 Trio64V2";
    case S3_VIRGE_325:
        return "S3 ViRGE";
    case S3_VIRGE_VX:
        return "S3 ViRGE/VX";
    case S3_VIRGE_DXGX:
        return "S3 ViRGE/DX/GX";
    case S3_VIRGE_GX2:
        return "S3 ViRGE/GX2";
    case S3_VIRGE_GX2P_A:
    case S3_VIRGE_GX2P_B:
        return "S3 ViRGE/GX2+";
    default:
        return "S3 ViRGE";
    }
}

static const char *zorroS3Name(UWORD productId)
{
    return (productId == CYBERVISION64_PRODUCT_ID) ? "CyberVision64" : "CyberVision64/3D";
}

static BOOL findZorroS3Card(S3MclkCard_t *card, ULONG selectedDevice, ULONG *deviceIndex, BOOL listOnly)
{
    static const UWORD productIds[] = {CYBERVISION64_PRODUCT_ID, CYBERVISION3D_PRODUCT_ID_Z3,
                                       CYBERVISION3D_PRODUCT_ID_Z2};

    struct Library *ExpansionBase = OpenLibrary(EXPANSIONNAME, 0);
    if (!ExpansionBase) {
        printf("Cannot open expansion.library\n");
        return FALSE;
    }

    for (int i = 0; i < ARRAY_SIZE(productIds); i++) {
        struct ConfigDev *configDev = NULL;
        while ((configDev = FindConfigDev(configDev, PHASE5_MANUFACTURER_ID, productIds[i])) != NULL) {
            ULONG index             = *deviceIndex;
            volatile UBYTE *regBase = (UBYTE *)configDev->cd_BoardAddr + CV3D_REGISTER_OFFSET;

            (*deviceIndex)++;

            printf("%lu: %s product %u at 0x%08lx, register base 0x%08lx%s\n", index, zorroS3Name(productIds[i]),
                   productIds[i], (ULONG)configDev->cd_BoardAddr, (ULONG)regBase,
                   (configDev->cd_Flags & CDF_CONFIGME) ? "" : " (already configured)");

            if (listOnly || index != selectedDevice) {
                continue;
            }

            card->kind          = CARD_ZORRO_S3;
            card->name          = zorroS3Name(productIds[i]);
            card->ExpansionBase = ExpansionBase;
            card->regBase       = regBase;

            printf("Selected device %lu: %s\n", index, card->name);
            return TRUE;
        }
    }

    CloseLibrary(ExpansionBase);
    return FALSE;
}

static BOOL findPciS3Card(S3MclkCard_t *card, ULONG selectedDevice, ULONG *deviceIndex, BOOL listOnly)
{
    struct Library *OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION);
    if (!OpenPciBase) {
        printf("Cannot open openpci.library v%u+\n", MIN_OPENPCI_VERSION);
        return FALSE;
    }

    struct TagItem tags[] = {{PRM_Vendor, VENDOR_ID_S3}, {TAG_END, 0}};
    struct pci_dev *board = NULL;

    int foundDevices = 0;

    while ((board = FindBoardA(board, tags)) != NULL) {
        foundDevices++;
        ULONG deviceId     = 0;
        ULONG legacyIOBase = 0;
        ULONG bus          = 0;
        ULONG slot         = 0;
        struct Node *owner = NULL;

        if (GetBoardAttrs(board, PRM_Device, (Tag)&deviceId, TAG_END) < 1) {
            printf("CCCC\n");
            continue;
        }

        if (!isSupportedPciDevice(deviceId)) {
            printf("Unsupported PCI device 0x%04lx\n", deviceId);
            continue;
        }

        GetBoardAttrs(board, PRM_LegacyIOSpace, (Tag)&legacyIOBase, PRM_BoardOwner, (Tag)&owner, PRM_BusNumber,
                      (Tag)&bus, PRM_SlotNumber, (Tag)&slot, TAG_END);

        if (!legacyIOBase) {
            printf("%lu: %s device 0x%04lx on PCI bus %lu slot %lu, no legacy I/O space available%s\n", *deviceIndex,
                   pciS3Name(deviceId), deviceId, bus, slot, owner ? " (already owned)" : "");
            (*deviceIndex)++;
            continue;
        }

        ULONG index = *deviceIndex;
        (*deviceIndex)++;

        printf("%lu: %s device 0x%04lx on PCI bus %lu slot %lu, legacy I/O base 0x%08lx%s\n", index,
               pciS3Name(deviceId), deviceId, bus, slot, legacyIOBase, owner ? " (already owned)" : "");

        if (listOnly || index != selectedDevice) {
            continue;
        }

        card->pciCommand = pci_read_config_word(PCI_COMMAND, board);
        if ((card->pciCommand & PCI_COMMAND_IO) == 0) {
            pci_write_config_word(PCI_COMMAND, card->pciCommand | PCI_COMMAND_IO, board);
            card->restorePciCommand = TRUE;
        }

        card->kind        = CARD_PCI_S3;
        card->name        = pciS3Name(deviceId);
        card->OpenPciBase = OpenPciBase;
        card->pciBoard    = board;
        card->regBase     = (UBYTE *)legacyIOBase;

        printf("Selected device %lu: %s\n", index, card->name);
        return TRUE;
    }

    printf("Found %d PCI S3 devices\n", foundDevices);
    CloseLibrary(OpenPciBase);
    return FALSE;
}

static void releaseCard(S3MclkCard_t *card)
{
    if (card->OpenPciBase) {
        struct Library *OpenPciBase = card->OpenPciBase;
        if (card->restorePciCommand && card->pciBoard) {
            pci_write_config_word(PCI_COMMAND, card->pciCommand, card->pciBoard);
        }
        CloseLibrary(OpenPciBase);
        card->OpenPciBase = NULL;
    }

    if (card->ExpansionBase) {
        CloseLibrary(card->ExpansionBase);
        card->ExpansionBase = NULL;
    }
}

static ULONG decodeMclkKhz(UBYTE sr10, UBYTE sr11)
{
    ULONG n = (sr10 & 0x1F) + 2;
    ULONG r = (sr10 >> 5) & 0x03;
    ULONG m = (sr11 & 0x7F) + 2;

    return ((virgeMclkPll.f_base * m) / n) >> r;
}

static ULONG setMemoryClock(VgaIo vga, ULONG clockKhz, USHORT *m, USHORT *n, USHORT *r)
{
    int actualKhz = svga_compute_pll(&virgeMclkPll, clockKhz, m, n, r);
    if (actualKhz < 0) {
        return 0;
    }

    vga.writeSR(0x10, (*r << 5) | (*n - 2));
    vga.writeSR(0x11, *m - 2);

    UBYTE sr15 = vga.readSR(0x15) & ~BIT(0);
    vga.writeSR(0x15, sr15);
    vga.writeSR(0x15, sr15 | BIT(0));
    vga.writeSR(0x15, sr15);

    if (clockKhz <= 57000) {
        vga.writeSRMask(0x0A, BIT(7), BIT(7));
        vga.writeSRMask(0x15, BIT(7), (clockKhz >= 55000) ? BIT(7) : 0);
    } else {
        vga.writeSRMask(0x0A, BIT(7), 0);
        vga.writeSRMask(0x15, BIT(7), 0);
    }

    return actualKhz;
}

static int printCurrentMclk(S3MclkCard_t *card)
{
    VgaIo vga       = VgaIo(card->regBase);
    UBYTE savedSr08 = vga.readSR(0x08);
    vga.writeSR(0x08, 0x06);

    UBYTE sr10    = vga.readSR(0x10);
    UBYTE sr11    = vga.readSR(0x11);
    ULONG mclkKhz = decodeMclkKhz(sr10, sr11);

    vga.writeSR(0x08, savedSr08);

    printf("Current MCLK: %lu.%03lu MHz (SR10=0x%02x SR11=0x%02x)\n", mclkKhz / 1000, mclkKhz % 1000, sr10, sr11);
    return EXIT_SUCCESS;
}

static int programMclk(S3MclkCard_t *card, ULONG clockKhz)
{
    VgaIo vga       = VgaIo(card->regBase);
    UBYTE savedSr08 = vga.readSR(0x08);
    vga.writeSR(0x08, 0x06);

    UBYTE oldSr10 = vga.readSR(0x10);
    UBYTE oldSr11 = vga.readSR(0x11);
    ULONG oldKhz  = decodeMclkKhz(oldSr10, oldSr11);

    USHORT m, n, r;
    ULONG actualKhz = setMemoryClock(vga, clockKhz, &m, &n, &r);
    if (!actualKhz) {
        vga.writeSR(0x08, savedSr08);
        printf("Could not compute PLL settings for %lu.%03lu MHz\n", clockKhz / 1000, clockKhz % 1000);
        return EXIT_FAILURE;
    }

    UBYTE newSr10 = vga.readSR(0x10);
    UBYTE newSr11 = vga.readSR(0x11);
    vga.writeSR(0x08, savedSr08);

    printf("Old MCLK: %lu.%03lu MHz (SR10=0x%02x SR11=0x%02x)\n", oldKhz / 1000, oldKhz % 1000, oldSr10, oldSr11);
    printf("New MCLK: %lu.%03lu MHz requested, %lu.%03lu MHz actual (M=%u N=%u R=%u)\n", clockKhz / 1000,
           clockKhz % 1000, actualKhz / 1000, actualKhz % 1000, m, n, r);
    printf("Programmed SR10=0x%02x SR11=0x%02x\n", newSr10, newSr11);

    return EXIT_SUCCESS;
}

static void printUsage(const char *name)
{
    printf("Usage: %s [-l] [-d device] [mclk MHz]\n", name);
    printf("       %s -l\n", name);
}

static BOOL parseOneArgument(const char *arg, const char *nextArg, BOOL *usedNext, BOOL *listOnly,
                             ULONG *selectedDevice, char *clockArg, ULONG clockArgSize)
{
    arg       = skipSpaces(arg);
    *usedNext = FALSE;

    if (!*arg) {
        return TRUE;
    }

    if (isOption(arg, 'l')) {
        *listOnly = TRUE;
        return TRUE;
    }

    if (isOption(arg, 'd')) {
        if (!nextArg || !parseNumber(nextArg, selectedDevice)) {
            return FALSE;
        }
        *usedNext = TRUE;
        return TRUE;
    }

    if ((arg[0] == '-' || arg[0] == '/') && (arg[1] == 'd' || arg[1] == 'D') && arg[2]) {
        return parseNumber(arg + 2, selectedDevice);
    }

    if (!clockArg[0]) {
        copyArg(clockArg, clockArgSize, arg);
        return TRUE;
    }

    return FALSE;
}

static BOOL parseArgv(int argc, char **argv, BOOL *listOnly, ULONG *selectedDevice, char *clockArg, ULONG clockArgSize)
{
    for (int i = 1; i < argc; i++) {
        BOOL usedNext = FALSE;
        if (!parseOneArgument(argv[i], (i + 1 < argc) ? argv[i + 1] : NULL, &usedNext, listOnly, selectedDevice,
                              clockArg, clockArgSize)) {
            return FALSE;
        }
        if (usedNext) {
            i++;
        }
    }
    return TRUE;
}

static BOOL parseRawArgs(const char *args, BOOL *listOnly, ULONG *selectedDevice, char *clockArg, ULONG clockArgSize)
{
    char arg[64];
    char nextArg[64];

    args = skipSpaces(args);
    while (*args) {
        copyArg(arg, sizeof(arg), args);
        args = skipSpaces(args + strlen(arg));

        const char *next = skipSpaces(args);
        copyArg(nextArg, sizeof(nextArg), next);

        BOOL usedNext = FALSE;
        if (!parseOneArgument(arg, nextArg[0] ? nextArg : NULL, &usedNext, listOnly, selectedDevice, clockArg,
                              clockArgSize)) {
            return FALSE;
        }

        if (usedNext) {
            args = skipSpaces(next + strlen(nextArg));
        }
    }

    return TRUE;
}

int main(int argc, char **argv)
{
    BOOL listOnly        = FALSE;
    ULONG selectedDevice = 0;
    char clockArg[64]    = {0};
    STRPTR rawArgs       = GetArgStr();

    for (int a = 0; a < argc; a++) {
        printf("argv[%d] = %s\n", a, argv[a]);
    }

    if (rawArgs && *skipSpaces(rawArgs)) {
        if (!parseRawArgs(rawArgs, &listOnly, &selectedDevice, clockArg, sizeof(clockArg))) {
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    } else if (!parseArgv(argc, argv, &listOnly, &selectedDevice, clockArg, sizeof(clockArg))) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    ULONG clockKhz = 0;
    if (!listOnly) {
        if (clockArg[0] &&
            (!parseClockKhz(clockArg, &clockKhz) || clockKhz < MIN_MCLK_KHZ || clockKhz > MAX_MCLK_KHZ)) {
            printf("MCLK must be between %u.%03u and %u.%03u MHz\n", MIN_MCLK_KHZ / 1000, MIN_MCLK_KHZ % 1000,
                   MAX_MCLK_KHZ / 1000, MAX_MCLK_KHZ % 1000);
            return EXIT_FAILURE;
        }
    }

    S3MclkCard_t card;
    memset(&card, 0, sizeof(card));

    ULONG deviceIndex = 0;

    if (listOnly) {
        printf("Listing S3 cards...\n");
        findPciS3Card(&card, selectedDevice, &deviceIndex, TRUE);
        findZorroS3Card(&card, selectedDevice, &deviceIndex, TRUE);
        if (!deviceIndex) {
            printf("No supported S3 card found\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    if (!findZorroS3Card(&card, selectedDevice, &deviceIndex, FALSE) &&
        !findPciS3Card(&card, selectedDevice, &deviceIndex, FALSE)) {
        if (deviceIndex && selectedDevice >= deviceIndex) {
            printf("Device %lu not found; use -l to list available devices\n", selectedDevice);
            return EXIT_FAILURE;
        }
        printf("No supported S3 card found\n");
        return EXIT_FAILURE;
    }

    int result = clockArg[0] ? programMclk(&card, clockKhz) : printCurrentMclk(&card);
    releaseCard(&card);

    return result;
}
