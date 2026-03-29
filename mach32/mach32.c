#include "chip_mach32.h"

#define __NOLIBBASE__

#if OPENPCI
#include <libraries/openpci.h>
#include <proto/openpci.h>
#endif
#include <libraries/pcitags.h>
#include <proto/exec.h>

#include <string.h>

#define PCI_DEVICE_MACH32 0x4158

/* No BAR tail reserved for register MMIO; full BAR0 treated as framebuffer for probe math. */
#define VRAM_PROBE_TAIL_RESERVE 0U

static ULONG misc_mem_size_bytes(ULONG misc_reg)
{
    static const ULONG sizes[] = { 512UL * 1024, 1024UL * 1024, 2048UL * 1024, 4096UL * 1024 };
    return sizes[(misc_reg >> 2) & 3u];
}

static BOOL probeFramebufferSize(BoardInfo_t *bi, ULONG barSize, ULONG candidate)
{
    LOCAL_SYSBASE();

    if (candidate < 512UL * 1024)
        return FALSE;

    ULONG vramTop = (ULONG)bi->MemoryBase + barSize - VRAM_PROBE_TAIL_RESERVE;

    volatile ULONG *framebuffer = (volatile ULONG *)bi->MemoryBase;
    volatile ULONG *highOffset  = framebuffer + (candidate >> 2) - 512 - 1;
    volatile ULONG *lowOffset   = framebuffer + (candidate >> 3);

    if ((ULONG)highOffset + sizeof(ULONG) > vramTop || (ULONG)lowOffset + sizeof(ULONG) > vramTop)
        return FALSE;

    framebuffer[0] = 0;
    *highOffset      = (ULONG)highOffset;
    *lowOffset       = (ULONG)lowOffset;

    CacheClearU();

    ULONG readbackHigh = *highOffset;
    ULONG readbackLow  = *lowOffset;
    ULONG readbackZero = framebuffer[0];

    framebuffer[0] = 0;
    CacheClearU();

    return (readbackHigh == (ULONG)highOffset && readbackLow == (ULONG)lowOffset && readbackZero == 0);
}

static void waitFifo(const BoardInfo_t *bi, UBYTE slots)
{
    (void)slots;
    /* TODO: poll GE_STAT / EXT_FIFO_STATUS per REG688000-15 §7-16 */
    (void)bi;
}

BOOL setupMMIO(BoardInfo_t *bi)
{
    (void)bi;
    /* Registers use PCI I/O (RegisterBase) only; no BAR MMIO window to establish. */
    return TRUE;
}

BOOL probe_memory_size(BoardInfo_t *bi)
{
    // LOCAL_SYSBASE();

    // ULONG barSize = bi->MemorySize;
    // if (barSize <= VRAM_PROBE_TAIL_RESERVE) {
    //     DFUNC(ERROR, "bar too small for VRAM probe\n");
    //     return FALSE;
    // }

    // ULONG misc     = W_MMIO(bi, MISC_OPTIONS);
    // ULONG aliasHint = misc_mem_size_bytes(misc);

    // static const ULONG trySizes[] = { 0x400000UL, 0x200000UL, 0x100000UL, 0x80000UL };
    // ULONG probed = 0;

    // for (unsigned i = 0; i < ARRAY_SIZE(trySizes); i++) {
    //     ULONG S = trySizes[i];
    //     if (S > barSize)
    //         continue;
    //     DFUNC(VERBOSE, "Mach32 VRAM probe trying %lu bytes (MISC_OPTIONS hint %lu)\n", S, aliasHint);
    //     if (probeFramebufferSize(bi, barSize, S)) {
    //         probed = S;
    //         break;
    //     }
    // }

    // if (!probed) {
    //     ULONG cap = minu(aliasHint, barSize - VRAM_PROBE_TAIL_RESERVE);
    //     cap         = (cap / 64UL) * 64UL;
    //     if (cap < 512UL * 1024)
    //         cap = 512UL * 1024;
    //     probed = cap;
    //     DFUNC(WARN, "Mach32 VRAM probe failed; using MISC_OPTIONS[3:2] / BAR cap → %lu bytes\n", probed);
    // }

    // if (probed <= VRAM_PROBE_TAIL_RESERVE) {
    //     DFUNC(ERROR, "probed size %lu invalid\n", probed);
    //     return FALSE;
    // }

    // ULONG usable = probed - VRAM_PROBE_TAIL_RESERVE;
    // usable       = minu(usable, barSize - VRAM_PROBE_TAIL_RESERVE);
    // usable       = (usable / 64UL) * 64UL;

    // if (usable + VRAM_PROBE_TAIL_RESERVE < 512UL * 1024) {
    //     DFUNC(ERROR, "usable VRAM %lu too small (need >= 512K)\n", usable);
    //     return FALSE;
    // }

    // bi->MemorySize = usable;
    // DFUNC(INFO, "Mach32 usable VRAM %luk (BAR %luk, MISC_OPTIONS raw 0x%lx)\n", bi->MemorySize / 1024UL, barSize / 1024UL,
    //       misc);

    return TRUE;
}

// APTR ASM AllocCardMem(__REGA0(struct BoardInfo *bi), __REGD0(ULONG size), __REGD1(BOOL force),
//                              __REGD2(BOOL system), __REGD3(ULONG bytesperrow), __REGA1(struct ModeInfo *mi),
//                              __REGD7(RGBFTYPE format))
// {
//     ULONG sz = size + 64;
//     return getConstCardData(bi)->AllocCardMemDefault(bi, sz, force, system, bytesperrow, mi, format);
// }
