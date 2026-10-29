#include "radeon_driver.hpp"

#include "card_common.h"
#define __NOLIBBASE__
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/exec.h>
#include <proto/openpci.h>
#include <utility/tagitem.h>

using namespace RadeonReg;

extern const char LibName[]     = "ATIRadeon.card";
extern const char LibIdString[] = "ATIRadeon Picasso96 card driver " STRINGIFY(LIB_VERSION) "." STRINGIFY(LIB_REVISION);
extern const UWORD LibVersion   = LIB_VERSION;
extern const UWORD LibRevision  = LIB_REVISION;
#ifdef DBG
int debugLevel = VERBOSE;
#endif

static struct RadeonState *claimedCards;

BOOL releaseCard(__REGA0(struct BoardInfo *bi))
{
    struct CardData *card = getCardData(bi);
    LOCAL_SYSBASE();
    if (card->OpenPciBase) {
        LOCAL_OPENPCIBASE();
        if (card->board) {
            if (card->state && card->state->initialized) {
                radeon_resource_detach(bi);
                radeon_free_windows(bi);
                radeon_free_i2c(bi);
                radeon_write(bi, RADEON_GEN_INT_CNTL, 0);
                removePciVBlankInterrupt(bi);
                (void)radeon_wait_idle(bi);
                radeon_cp_stop(bi);
                radeon_write(bi, RADEON_CRTC_GEN_CNTL, card->state->savedCRTC);
                radeon_write(bi, RADEON_CRTC_EXT_CNTL, card->state->savedExtCRTC);
                radeon_write(bi, RADEON_SURFACE_CNTL, card->state->savedSurface);
                pci_write_config_word(PCI_COMMAND, card->state->savedCommand, card->board);
            }
            SetBoardAttrs(card->board, PRM_BoardOwner, (Tag)NULL, TAG_END);
        }
        CloseLibrary(card->OpenPciBase);
    }
    if (card->state) {
        struct RadeonState **link = &claimedCards;
        while (*link && *link != card->state)
            link = &(*link)->next;
        if (*link)
            *link = (*link)->next;
        FreeVec(card->state);
    }
    card->state       = NULL;
    card->board       = NULL;
    card->OpenPciBase = NULL;
    return TRUE;
}

extern "C" BOOL FindCard(__REGA0(struct BoardInfo *bi), __REGA1(CONST_STRPTR *toolTypes))
{
    struct CardData *card = getCardData(bi);
    LOCAL_SYSBASE();
    if (card->board)
        return TRUE;
    struct Library *OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION);
    if (!OpenPciBase)
        return FALSE;

    ULONG vendor = RADEON_VENDOR_ID, device = 0, slot = ~0UL, bus = ~0UL;
    parseToolTypes(bi, toolTypes, &device, &vendor, &slot, &bus);
    struct TagItem tags[5];
    ULONG n   = 0;
    tags[n++] = TagItem{PRM_Vendor, vendor};
    if (device)
        tags[n++] = TagItem{PRM_Device, device};
    if (slot != ~0UL)
        tags[n++] = TagItem{PRM_SlotNumber, slot};
    if (bus != ~0UL)
        tags[n++] = TagItem{PRM_BusNumber, bus};
    tags[n] = TagItem{TAG_END, 0};

    struct pci_dev *board = NULL;
    while ((board = FindBoardA(board, tags))) {
        struct Node *owner = NULL;
        ULONG id = 0, boardSlot = 0, boardBus = 0, boardFunction = 0;
        if (GetBoardAttrs(board, PRM_Device, (Tag)&id, PRM_BoardOwner, (Tag)&owner, TAG_END) != 2 || owner)
            continue;
        const struct RadeonDevice *entry = radeon_find_device(id);
        if (!entry || entry->device == RADEON_DEVICE_SENTINEL)
            continue;
        struct RadeonState *state = static_cast<RadeonState *>(AllocVec(sizeof(*state), MEMF_PUBLIC | MEMF_CLEAR));
        if (!state)
            break;
        card->boardNode.ln_Name = (char *)LibName;
        if (!SetBoardAttrs(board, PRM_BoardOwner, (Tag)&card->boardNode, TAG_END)) {
            FreeVec(state);
            continue;
        }
        state->device     = entry;
        state->boardInfo  = bi;
        state->next       = claimedCards;
        claimedCards      = state;
        card->state       = state;
        card->board       = board;
        card->OpenPciBase = OpenPciBase;
        GetBoardAttrs(board, PRM_SlotNumber, (Tag)&boardSlot, PRM_BusNumber, (Tag)&boardBus, PRM_FunctionNumber,
                      (Tag)&boardFunction, TAG_END);
        state->pciBus      = boardBus;
        state->pciSlot     = boardSlot;
        state->pciFunction = boardFunction;
        generateBoardName(card->boardName, "Radeon", boardBus, boardSlot);
        bi->BoardName              = card->boardName;
        bi->BoardType              = BT_Radeon;
        bi->GraphicsControllerType = GCT_ATIRV100;
        bi->PaletteChipType        = PCT_ATIRV100;
        return TRUE;
    }
    CloseLibrary(OpenPciBase);
    return FALSE;
}

extern "C" BOOL InitCard(__REGA0(struct BoardInfo *bi), __REGA1(CONST_STRPTR *toolTypes))
{
    struct CardData *card = getCardData(bi);
    if (!card->board || !card->OpenPciBase || !card->state)
        return FALSE;
    if (card->state->initialized)
        return TRUE;
    LOCAL_OPENPCIBASE();
    struct RadeonState *state = card->state;
    UBYTE *memory = NULL, *mmio = NULL, *rom = NULL;
    ULONG memorySize = 0, mmioSize = 0, romSize = 0;
    if (GetBoardAttrs(card->board, PRM_MemoryAddr0, (Tag)&memory, PRM_MemorySize0, (Tag)&memorySize, PRM_MemoryAddr2,
                      (Tag)&mmio, PRM_MemorySize2, (Tag)&mmioSize, TAG_END) != 4 ||
        !memory || !mmio || memorySize < RADEON_MIN_VRAM_SIZE || mmioSize < RADEON_MIN_MMIO_SIZE) {
        releaseCard(bi);
        return FALSE;
    }
    GetBoardAttrs(card->board, PRM_ROM_Address, (Tag)&rom, PRM_ROM_Size, (Tag)&romSize, TAG_END);
    state->savedCommand = pci_read_config_word(PCI_COMMAND, card->board);
    pci_write_config_word(PCI_COMMAND, state->savedCommand | PCI_COMMAND_MEMORY, card->board);
    bi->RegisterBase           = mmio;
    bi->MemoryIOBase           = mmio;
    bi->MemorySpaceBase        = memory;
    bi->MemorySpaceSize        = memorySize;
    bi->MemorySize             = 0;
    bi->MemoryBase             = memory;
    state->framebufferSize     = memorySize;
    state->framebufferPhysical = (ULONG)pci_logic_to_physic_addr(memory, card->board);
    state->savedCRTC           = radeon_read(bi, RADEON_CRTC_GEN_CNTL);
    state->savedExtCRTC        = radeon_read(bi, RADEON_CRTC_EXT_CNTL);
    state->savedSurface        = radeon_read(bi, RADEON_SURFACE_CNTL);
    state->initialized         = TRUE;
    setCacheMode(bi, mmio, mmioSize, MAPP_IO | MAPP_CACHEINHIBIT, CACHEFLAGS);
    setCacheMode(bi, memory, memorySize, MAPP_CACHEINHIBIT | MAPP_IMPRECISE | MAPP_NONSERIALIZED, CACHEFLAGS);
    if (!radeon_init_chip(bi, rom, romSize)) {
        releaseCard(bi);
        return FALSE;
    }
    if (state->cp.active && !radeon_resource_attach(bi)) {
        releaseCard(bi);
        return FALSE;
    }
    installPciVBlankInterrupt(bi, toolTypes);
    return TRUE;
}

/* Release BoardInfo-owned resources on the final CloseLibrary, while the
 * caller still owns the records. A delayed Expunge must not dereference them.
 */
void radeon_cleanup(struct CardBase *base)
{
    (void)base;
    while (claimedCards)
        releaseCard(claimedCards->boardInfo);
}
