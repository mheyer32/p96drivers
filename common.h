#ifndef COMMON_H
#define COMMON_H

#include <SDI_compiler.h>
#include <exec/types.h>
#include <mmu/context.h>
#include <proto/exec.h>

#include <boardinfo.h>
/*
 * Older bebbo g++ ignored __asm("dn") on enum-typed parameters
 * ("attributes applied to 'RGBFTYPE' after definition"). Local amiga-gcc
 * 6.5 / 13.2 fix that; ULONG remains safe for unfixed toolchains.
 */
typedef ULONG RGBFTYPE_REG;
#define AS_RGBF(x)          static_cast<RGBFTYPE>(x)
#define P96_HOOK(field, fn) ((field) = reinterpret_cast<decltype(field)>(fn))

#define ALWAYS  0       // Always print when DEBUG is enabled
#define ERROR   ALWAYS  // Function failed, not recoverable
#define WARN    5       // Function failed, but is recoverable
#define INFO    10      // Informational messages
#define VERBOSE 15      // Verbose output
#define CHATTY  20      // Very verbose output
#define TELLALL 25      // Very verbose output

#ifndef DBG
#define D(...)
#define DFUNC(...)
#define LOCAL_DEBUGLEVEL(x)
#else
extern int debugLevel;
#define LOCAL_DEBUGLEVEL(level) int debugLevel = level;

extern void myPrintF(const char *fmt, ...);
extern void mySprintF(struct ExecBase *SysBase, char *outStr, const char *fmt, ...);

#define D(level, ...)            \
    if (debugLevel >= (level)) { \
        myPrintF(__VA_ARGS__);   \
    }
/* GNU ,##__VA_ARGS__ eats the comma when the varargs list is empty (C and C++). */
#define DFUNC(level, fmt, ...)                                             \
    if (debugLevel >= (level)) {                                           \
        myPrintF("%s:%ld: " fmt, __func__, (long)__LINE__, ##__VA_ARGS__); \
    }
#endif

#define STRINGIFX(x) #x
#define STRINGIFY(x) STRINGIFX(x)

/* Standard VGA ports: see vga_regs.hpp (VgaReg / VgaIo). */

#define LOCAL_SYSBASE()     struct ExecBase *SysBase = bi->ExecBase
#define LOCAL_UTILITYBASE() struct Library *UtilityBase = bi->UtilBase
#if OPENPCI
#define LOCAL_OPENPCIBASE() struct Library *OpenPciBase = getCardData(bi)->OpenPciBase
#endif

/* BoardInfo.CardFlags — private; RTG does not touch these.
 * P96 monitor tooltype BLACKLEVEL=Black|Pedestal (same as S3ViRGE.chip).
 * Unset / Pedestal → clear bit (DAC blanking pedestal on). Black → set bit (0 IRE). */
#define CFF_BLACKLEVEL_BLACK (1UL << 0)
#define CFF_VBLANK_INTSERVER (1UL << 1) /* VBlank HardInterrupt registered (OpenPCI or PORTS) */
// #define LOCAL_DOSBASE() struct Library *DOSBase = getChipData(bi)->DOSBase

/*
 * OpenPCI/Exec interrupt servers: return with CCR.Z clear if this board's IRQ
 * was handled, Z set otherwise. Loading d0 alone is not enough — C epilogues
 * (addq etc.) clobber CCR. Wrap the C body:
 *
 *   ULONG ASM FooHandler(__REGA1(struct BoardInfo *bi)) { ...; return handled; }
 *   DEFINE_INTSERVER(Foo, FooHandler);
 *   bi->HardInterrupt.is_Code = (void (*)())Foo;
 *
 * Non-zero handler return → Z clear; zero → Z set.
 * Scratch for IS_CODE: d0-d1/a0-a1/a5-a6 — trampoline preserves the rest.
 */
#define DEFINE_INTSERVER(entry, handler) \
    extern "C" void entry(void);         \
    asm(".section .text." #entry         \
        ",\"ax\"\n"                      \
        "	.align	2\n"                 \
        "	.globl	_" #entry            \
        "\n"                             \
        "_" #entry                       \
        ":\n"                            \
        "	jsr	_" #handler              \
        "\n"                             \
        "	tst.l	d0\n"                \
        "	rts\n")

// FIXME: IDK if this is a good idea. Often times the compiler decides to promote something to int
// and then this becomes ambiguous and may emit code that we don't want.
// So we should probably just stick with the swapw/swapl functions instead?
static inline ULONG swap(ULONG value)
{
    // endian swap value
    value = ((value & 0xFFFF0000) >> 16) | ((value & 0x0000FFFF) << 16);
    value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
    return value;
}

static inline UWORD swap(UWORD value)
{
    // endian swap value
    value = (value & 0xFF00) >> 8 | (value & 0x00FF) << 8;
    return value;
}

static inline UWORD swapw(UWORD value)
{
    return swap(value);
}

static inline ULONG swapl(ULONG value)
{
    return swap(value);
}

// #if BIGENDIAN_MMIO
// #define SWAPW(x) x
// #define SWAPL(x) x
// #else
// #define SWAPW(x) swapw(x)
// #define SWAPL(x) swapl(x)
// #endif

// #if BIGENDIAN_IO
// #define SWAPW_IO(x) x
// #define SWAPL_IO(x) x
// #else
// #define SWAPW_IO(x) swapw(x)
// #define SWAPL_IO(x) swapl(x)
// #endif

#define BIT(x)          (1 << (x))
#define TESTBIT(x, bit) (((x) & BIT(bit)) != 0)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define STATIC_ASSERT(COND, MSG)    typedef char static_assertion_##MSG[(COND) ? 1 : -1]
#define SIZEOF_MEMBER(type, member) (sizeof(((type *)0)->member))

static inline ULONG maxu(ULONG x, ULONG y)
{
    return (x < y) ? y : x;
}

static inline ULONG minu(ULONG x, ULONG y)
{
    return (x < y) ? x : y;
}

static inline ULONG ceilDivu(ULONG x, ULONG y)
{
    if (!y)
        return 0;
    return (x + y - 1) / y;
}

static inline int max(int x, int y)
{
    return (x < y) ? y : x;
}

static inline int min(int x, int y)
{
    return (x < y) ? x : y;
}

static inline int ceilDiv(int x, int y)
{
    return (x + y - 1) / y;
}

static inline int numBits(ULONG x)
{
    int bits = 0;
    while (x) {
        ++bits;
        x >>= 1;
    }
    return bits;
}

typedef enum BlitterOp
{
    None,
    FILLRECT,
    INVERTRECT,
    BLITRECT,
    BLITRECTNOMASKCOMPLETE,
    BLITTEMPLATE,
    BLITPLANAR2CHUNKY,
    BLITPATTERN,
    LINE

} BlitterOp_t;

// Remember: all in Little Endian!
typedef struct __attribute__((packed)) OptionRomHeader
{
    UWORD signature;     // 0x0000: Signature (should be 0xAA55)
    UBYTE reserved[22];  // 0x0002: Reserved (usually 0, may contain PCI data structure pointer)
    UWORD pcir_offset;   // 0x0018: Pointer to PCI Data Structure (offset within the ROM)
} OptionRomHeader_t;

typedef struct __attribute__((packed)) PCI_DataStructure
{
    UBYTE signature[4];            // 0x0000: Signature ('PCIR')
    UWORD vendor_id;               // 0x0004: Vendor ID (from PCI Configuration Space)
    UWORD device_id;               // 0x0006: Device ID (from PCI Configuration Space)
    UWORD vital_product_data_ptr;  // 0x0008: Pointer to Vital Product Data (VPD) (if used, otherwise 0)
    UWORD length;                  // 0x000A: Length of the PCI Data Structure in bytes
    UBYTE revision;                // 0x000C: Revision level of the code/data in the ROM
    UBYTE class_code[3];           // 0x000D: Class Code (same as PCI Configuration Space)
    UWORD image_length;            // 0x0010: Image length in 512-byte units
    UWORD code_revision;           // 0x0012: Revision level of the code
    UBYTE code_type;               // 0x0014: Code Type (e.g., 0x00 for x86, 0x01 for Open Firmware, etc.)
    UBYTE indicator;               // 0x0015: Indicator (0x80 = last image, 0x00 = more images follow)
    UWORD reserved;                // 0x0016: Reserved (typically 0)
} PciRomData_t;

struct svga_pll
{
    USHORT m_min;
    USHORT m_max;
    USHORT n_min;
    USHORT n_max;
    USHORT r_min;  // post divider log2
    USHORT r_max;  // post divider log2
    ULONG f_vco_min;
    ULONG f_vco_max;
    ULONG f_base;
};

// f_wanted is in Khz
int svga_compute_pll(const struct svga_pll *pll, ULONG f_wanted_khz, USHORT *m, USHORT *n, USHORT *r);

void delayMicroSeconds(ULONG us);
void delayMilliSeconds(ULONG ms);

// can be used as mask
#define CACHEFLAGS (MAPP_IO | MAPP_CACHEINHIBIT | MAPP_NONSERIALIZED | MAPP_IMPRECISE | MAPP_COPYBACK)

BOOL setCacheMode(struct BoardInfo *bi, APTR from, ULONG size, ULONG flags, ULONG mask);

/******************************************************************************/
static inline ULONG abs_diff(ULONG a, ULONG b)
{
    return (a > b) ? (a - b) : (b - a);
}

static inline WORD myabs(WORD x)
{
    WORD result;
    result = (x < 0) ? (-x) : x;
    return (result);
}

typedef struct BoardInfo BoardInfo_t;

static INLINE struct ChipData *getChipData(struct BoardInfo *bi)
{
    return (struct ChipData *)&bi->ChipData[0];
}

static INLINE const struct ChipData *getConstChipData(const struct BoardInfo *bi)
{
    return (const struct ChipData *)&bi->ChipData[0];
}

static INLINE struct CardData *getCardData(struct BoardInfo *bi)
{
    return (struct CardData *)&bi->CardData[0];
}

static INLINE const struct CardData *getConstCardData(const struct BoardInfo *bi)
{
    return (const struct CardData *)&bi->CardData[0];
}

static INLINE REGARGS volatile UBYTE *getIOBase(const struct BoardInfo *bi)
{
    return bi->RegisterBase;
}

static INLINE REGARGS volatile UBYTE *getMMIOBase(const struct BoardInfo *bi)
{
    return bi->MemoryIOBase;
}

static inline void flushWrites()
{
    /* Drain 68060 store buffer before a subsequent serialized/IO/VRAM read. */
    asm volatile("nop" ::: "memory");
}

// make a DWORD from two shorts. Make sure, hi and lo are not the same variable!
static INLINE int makeDWORD(short hi, short lo)
{
    int res;
    __asm __volatile(
        "swap %0\n\t"
        "move.w %2,%0"
        : "=&d"(res)
        : "0"(hi), "g"(lo)
        : "cc");
    return res;
    //    return hi << 16 | lo;
}

static INLINE int copyToUpper(short hilo)
{
    int res, tmp;
    __asm __volatile(
        "move.w %0,%1\n\t"
        "swap %0\n\t"
        "move.w %1,%0\n\t"
        : "=&d"(res), "=d"(tmp)
        : "0"(hilo)
        : "cc");
    return res;
    //    return hilo << 16 | hilo;
}

// Move lowest byte of a into lowest byte of b
static INLINE unsigned int moveb(unsigned char a, unsigned int b)
{
    unsigned int res;
    __asm __volatile("move.b %2,%0" : "=d"(res) : "0"(b), "dmi"(a) : "cc");
    return res;
}

// Move lowest byte of a into lowest byte of b
static INLINE unsigned int movew(unsigned short a, unsigned int b)
{
    unsigned int res;
    __asm __volatile("move.w %2,%0" : "=d"(res) : "0"(b), "dmi"(a) : "cc");
    return res;
}

static inline UBYTE getBPP(ULONG format)
{
    // FIXME: replace with fixed table?
    switch ((RGBFTYPE)format) {
    case RGBFB_CLUT:
        return 1;
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        return 2;
        break;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        return 3;
        break;
    case RGBFB_A8R8G8B8:
    case RGBFB_B8G8R8A8:
    case RGBFB_R8G8B8A8:
    case RGBFB_A8B8G8R8:
        return 4;
        break;
    default:
        // fallthrough
        break;
    }
    return 0;
}

static inline UBYTE getBPPLog2(ULONG format)
{
    // FIXME: replace with fixed table?
    switch ((RGBFTYPE)format) {
    case RGBFB_CLUT:
        return 0;
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        return 1;
        break;
    case RGBFB_A8R8G8B8:
    case RGBFB_B8G8R8A8:
    case RGBFB_R8G8B8A8:
    case RGBFB_A8B8G8R8:
        return 2;
        break;
    default:
        // fallthrough
        break;
    }
    return 0;
}

static inline UWORD revertBitsW(UWORD word)
{
    // Convert sprite data to "LSB are leftmost"
    word = ((word & 0xFF00) >> 8) | ((word & 0x00FF) << 8);
    word = ((word & 0xF0F0) >> 4) | ((word & 0x0F0F) << 4);
    word = ((word & 0xCCCC) >> 2) | ((word & 0x3333) << 2);
    word = ((word & 0xAAAA) >> 1) | ((word & 0x5555) << 1);
    return word;
}

/* Spread 16 plane bits into odd positions of a ULONG (0b…abcdefgh → 0a0b0c…). */
static inline ULONG spreadBits(UWORD word)
{
    ULONG x = word;
    x       = (x | (x << 8)) & 0x00FF00FFUL;
    x       = (x | (x << 4)) & 0x0F0F0F0FUL;
    x       = (x | (x << 2)) & 0x33333333UL;
    x       = (x | (x << 1)) & 0x55555555UL;
    return x;
}

/* Duplicate each bit (0b…abc → 0b…aabbcc). Used by BIGSPRITE (ATI after reverse, S3 as-is). */
static inline ULONG expandBits2x(UWORD word)
{
    ULONG x = spreadBits(word);
    return x | (x << 1);
}

/* ATI Mach32/64 and AT3D: 64x64 hardware cursor, 2 bpp (AND/XOR), transparent = 0b10 */
#define ATI_CURSOR_TRANSPARENT 0xAAAAAAAAUL

static inline ULONG combineAtiCursor16(UWORD andBits, UWORD xorBits)
{
    return swap((spreadBits(andBits) << 1) | spreadBits(xorBits));
}

static inline ULONG packAtiCursor16(UWORD plane0, UWORD plane1)
{
    return combineAtiCursor16(revertBitsW(~plane0), revertBitsW(plane1));
}

static inline void packAtiHwCursorImage(struct BoardInfo *bi)
{
    ULONG *cursor = (ULONG *)bi->MouseImageBuffer;
    UWORD height  = bi->MouseHeight;
    if (height > 64)
        height = 64;

    if (bi->Flags & BIF_HIRESSPRITE) {
        const ULONG *image = (const ULONG *)bi->MouseImage + 2;
        for (UWORD y = 0; y < height; ++y) {
            ULONG plane0 = *image++;
            ULONG plane1 = *image++;
            *cursor++    = packAtiCursor16((UWORD)(plane0 >> 16), (UWORD)(plane1 >> 16));
            *cursor++    = packAtiCursor16((UWORD)plane0, (UWORD)plane1);
            *cursor++    = ATI_CURSOR_TRANSPARENT;
            *cursor++    = ATI_CURSOR_TRANSPARENT;
        }
    } else if (bi->Flags & BIF_BIGSPRITE) {
        /* MouseHeight is already the on-screen (doubled) size; source rows are half. */
        UWORD srcH = height >> 1;
        if (srcH > 32)
            srcH = 32;
        const UWORD *image = bi->MouseImage + 2;
        for (UWORD y = 0; y < srcH; ++y) {
            UWORD plane0 = *image++;
            UWORD plane1 = *image++;
            /* Reverse once, then duplicate in LSB-left domain (low 16 = left pixels). */
            ULONG andBits = expandBits2x(revertBitsW(~plane0));
            ULONG xorBits = expandBits2x(revertBitsW(plane1));
            ULONG row[4];
            row[0] = combineAtiCursor16((UWORD)andBits, (UWORD)xorBits);
            row[1] = combineAtiCursor16((UWORD)(andBits >> 16), (UWORD)(xorBits >> 16));
            row[2] = ATI_CURSOR_TRANSPARENT;
            row[3] = ATI_CURSOR_TRANSPARENT;
            for (UWORD r = 0; r < 2; ++r) {
                *cursor++ = row[0];
                *cursor++ = row[1];
                *cursor++ = row[2];
                *cursor++ = row[3];
            }
        }
        height = srcH * 2;
    } else {
        const UWORD *image = bi->MouseImage + 2;
        for (UWORD y = 0; y < height; ++y) {
            UWORD plane0 = *image++;
            UWORD plane1 = *image++;
            *cursor++    = packAtiCursor16(plane0, plane1);
            *cursor++    = ATI_CURSOR_TRANSPARENT;
            *cursor++    = ATI_CURSOR_TRANSPARENT;
            *cursor++    = ATI_CURSOR_TRANSPARENT;
        }
    }
    for (UWORD y = height; y < 64; ++y) {
        for (UWORD p = 0; p < 4; ++p)
            *cursor++ = ATI_CURSOR_TRANSPARENT;
    }
}

// Apparently the mix modes can be shared between S3 cards and ATI Mach64
#define MIX_NOT_CURRENT             0b0000
#define MIX_ZERO                    0b0001
#define MIX_ONE                     0b0010
#define MIX_CURRENT                 0b0011
#define MIX_NOT_NEW                 0b0100
#define MIX_CURRENT_XOR_NEW         0b0101
#define MIX_NOT_CURRENT_XOR_NEW     0b0110
#define MIX_NEW                     0b0111
#define MIX_NOT_CURRENT_OR_NOT_NEW  0b1000
#define MIX_CURRENT_OR_NOT_NEW      0b1001
#define MIX_NOT_CURRENT_OR_NEW      0b1010
#define MIX_CURRENT_OR_NEW          0b1011
#define MIX_CURRENT_AND_NEW         0b1100
#define MIX_NOT_CURRENT_AND_NEW     0b1101
#define MIX_CURRENT_AND_NOT_NEW     0b1110
#define MIX_NOT_CURRENT_AND_NOT_NEW 0b1111

#endif  // COMMON_H
