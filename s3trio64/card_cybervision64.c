#include "chip_s3trio64.h"
#include "s3trio64_common.h"

#include <clib/debug_protos.h>
#include <exec/nodes.h>
#include <exec/types.h>
#include <libraries/configvars.h>
#include <libraries/expansion.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/icon.h>
#include <proto/timer.h>

#ifndef TESTEXE
const char LibName[]     = "Cybervision64.card";
const char LibIdString[] = "Cybervision64 Picasso96 card driver version 1.0";
const UWORD LibVersion   = 1;
const UWORD LibRevision  = 0;
#endif

#ifdef DBG
int debugLevel = VERBOSE;
#endif

// Phase5 Cybervision64 Autoconfig IDs
#define PHASE5_MANUFACTURER_ID   8512
#define CYBERVISION64_PRODUCT_ID 34

BOOL ASM SetSwitch(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    DFUNC(INFO, "state %ld\n", (LONG)state);
    if (bi->MoniSwitch == state) {
        return state;
    }

    bi->MoniSwitch = state;

    if (state) {
        W_CV64_MASK(CV64_MONITOR_SWITCH_BIT, CV64_MONITOR_SWITCH_BIT);
    } else {
        W_CV64_MASK(CV64_MONITOR_SWITCH_BIT, 0);
    }

    return state;
}

extern BOOL InitChip(__REGA0(struct BoardInfo *bi));

BOOL FindCard(__REGA0(struct BoardInfo *bi), __REGA1(CONST_STRPTR *Tooltypes))
{
    LOCAL_SYSBASE();
    CardData_t *cd = getCardData(bi);

    // Open Expansion Library
    struct Library *ExpansionBase = NULL;
    if (!(ExpansionBase = OpenLibrary(EXPANSIONNAME, 0))) {
        DFUNC(ERROR, "Cannot open expansion.library\n");
        return FALSE;
    }

    // Loop through all Phase5 Cybervision64 cards to find an unclaimed one
    struct ConfigDev *configDev = NULL;
    while ((configDev = FindConfigDev(configDev, PHASE5_MANUFACTURER_ID, CYBERVISION64_PRODUCT_ID)) != NULL) {
        D(INFO, "Found Cybervision64 card at 0x%08lx\n", configDev->cd_BoardAddr);

        // Check if card is already configured/claimed
        if (!(configDev->cd_Flags & CDF_CONFIGME)) {
            D(INFO, "Cybervision64 card already configured by: %s\n",
              (configDev->cd_Node.ln_Name) ? configDev->cd_Node.ln_Name : "Unknown");
            continue;  // Try next card
        }

        // Found an unclaimed card - claim it by clearing CDB_CONFIGME flag
        configDev->cd_Flags &= ~CDF_CONFIGME;

        cd->configDev = configDev;

        bi->BoardType = BT_CyberVision;
        bi->BoardName = "CV64";

        getChipData(bi)->chipFamily = TRIO64;

        D(INFO, "Phase5 Cybervision64 card claimed at 0x%08lx, size 0x%08lx\n", configDev->cd_BoardAddr,
          configDev->cd_BoardSize);

        return TRUE;
    }

    // No unclaimed cards found
    D(INFO, "No unclaimed Phase5 Cybervision64 card found\n");
    CloseLibrary(ExpansionBase);
    return FALSE;
}

BOOL releaseCard(__REGA0(struct BoardInfo *bi))
{
    CardData_t *cd = getCardData(bi);

    if (cd->configDev) {
        // Release the card by setting CDB_CONFIGME flag
        cd->configDev->cd_Flags |= CDF_CONFIGME;
        cd->configDev = NULL;
    }
#ifndef TESTEXE
    {
        if (getCardData(bi)->cv64CtrlReg) {
            W_CV64_MASK(CV64_MONITOR_SWITCH_BIT, 0);
        }
    }
#endif
}

BOOL InitCard(__REGA0(struct BoardInfo *bi), __REGA1(CONST_STRPTR *ToolTypes))
{
    CardData_t *cd = getCardData(bi);
    if (!cd->configDev) {
        DFUNC(ERROR, "Cybervision64.card: No board claimed\n");
        return FALSE;
    }

    LOCAL_SYSBASE();

    // Get board address and size from ConfigDev
    APTR memoryBase  = (APTR)cd->configDev->cd_BoardAddr;
    ULONG memorySize = cd->configDev->cd_BoardSize;

    D(INFO, "Cybervision64 memory base: 0x%08lx, size: 0x%08lx\n", memoryBase, memorySize);

    // Cybervision64 uses S3Trio3264 chip driver
    ChipFamily_t chipFamily     = TRIO64;
    getChipData(bi)->chipFamily = chipFamily;

    // Set up memory and register bases
    // Cybervision64 uses Zorro memory space, so memory base is directly from ConfigDev
    bi->MemoryBase      = (UBYTE *)memoryBase + 0x01400000;
    bi->MemorySize      = memorySize;
    bi->MemorySpaceBase = memoryBase;
    bi->MemorySpaceSize = memorySize;

    // This is where the Cybervision maps its IO registers in Zorro space
    bi->RegisterBase = (UBYTE *)memoryBase + 0x02000000 + REGISTER_OFFSET;

    // We basically don't use MMIO, so trick the driver to access IO registers
    // bi->MemoryIOBase = (UBYTE *)memoryBase + 0x010A0000 + MMIOREGISTER_OFFSET;
    bi->MemoryIOBase = (UBYTE *)memoryBase + 0x02000000 + MMIOREGISTER_OFFSET;

    // Set legacyIOBase for chip driver (used for I/O register access)
    cd->legacyIOBase = bi->RegisterBase;

    // Cybervision64 specific control register
    cd->cv64CtrlReg = (UBYTE *)memoryBase + 0x40001 + REGISTER_OFFSET;

    // bi->Flags |= BIF_CACHEMODECHANGE;

    bi->Flags |= BIF_INDISPLAYCHAIN;  // Cybervision has hardware VGA pass through

    bi->SetSwitch = SetSwitch;

    // Cybervision64 uses built-in chip driver (no separate library)
    bi->ChipBase = NULL;  // Not used when chip driver is built-in

    bi->MemoryClock = 54000000;  // 54 MHz memory clock

    D(INFO, "Cybervision64: Framebuffer at 0x%08lx, Registers at 0x%08lx, board control register 0x%08lx\n",
      bi->MemoryBase, bi->RegisterBase - REGISTER_OFFSET, cd->cv64CtrlReg - REGISTER_OFFSET);

    W_CV64(0);
    delayMicroSeconds(100);
    W_CV64(CV64_RESET_BIT);
    R_CV64();

    D(INFO, "calling init chip...\n");
    if (!InitChip(bi)) {
        DFUNC(ERROR, "InitChip() failed\n");

        W_CV64_MASK(CV64_MONITOR_SWITCH_BIT, 0);
        return FALSE;
    }
    D(INFO, "card has %ldkb usable memory\n", bi->MemorySize / 1024);

    W_CV64_MASK(CV64_MONITOR_SWITCH_BIT, 0);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef TESTEXE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern BOOL TestCard(BoardInfo_t *bi);

static struct UtilityBase *UtilityBase;

static struct BoardInfo boardInfo = {0};

void sigIntHandler(int dummy)
{
    releaseCard(&boardInfo);
    abort();
}

int main()
{
    signal(SIGINT, sigIntHandler);

    int rval = EXIT_FAILURE;

    memset(&boardInfo, 0, sizeof(boardInfo));

    struct BoardInfo *bi = &boardInfo;

    bi->ExecBase = SysBase;
    bi->UtilBase = (struct Library*)UtilityBase;

    if (!FindCard(bi, NULL)) {
        goto exit;
    }

    if (!InitCard(bi, NULL)) {
        goto exit;
    }

    bi->SetSwitch(bi, TRUE);

    rval = TestCard(bi);

//   bi->SetSwitch(bi, FALSE);

exit:
    releaseCard(bi);
    return rval;
}
#endif  // TESTEXE
