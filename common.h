#ifndef COMMON_H
#define COMMON_H

#include <SDI_compiler.h>
#include <exec/types.h>
#include <mmu/context.h>
#include <proto/exec.h>

#include <boardinfo.h>
// FIXME: copy header into common location
// #include "endian.h"

#define ALWAYS  0       // Always print when DEBUG is enabled
#define ERROR   ALWAYS  // Function failed, not recoverable
#define WARN    5       // Function failed, but is recoverable
#define INFO    10      // Informational messages
#define VERBOSE 15      // Verbose output
#define CHATTY  20      // Very verbose output

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
// Helper macro to allow call DFUNC with just one argument (and __VA_ARGS__
// being empty)
#define VA_ARGS(...) , ##__VA_ARGS__
#define DFUNC(level, fmt, ...)                                             \
    if (debugLevel >= (level)) {                                           \
        myPrintF("%s:%ld: " fmt, __func__, __LINE__ VA_ARGS(__VA_ARGS__)); \
    }
#endif

// The offsets allow for using signed 16bit indexed addressing be used
#if !defined(REGISTER_OFFSET) || !defined(MMIOREGISTER_OFFSET)
#pragma GCC error "REGISTER_OFFSET or MMIOREGISTER_OFFSET not defined"
#endif

#define STRINGIFY(x) #x

#define SEQX     0x3C4  // Access SRxx registers
#define SEQ_DATA 0x3C5

#define CRTC_IDX  0x3D4  // Access CRxx registers
#define CRTC_DATA 0x3D5

#define GRC_ADR  0x3CE
#define GRC_DATA 0x3CF

#define ATR_AD     0x3C0
#define ATR_DATA_W 0x3C0
#define ATR_DATA_R 0x3C1

#define DAC_WR_AD 0x3C8
#define DAC_DATA  0x3C9

#define LOCAL_SYSBASE()        struct ExecBase *SysBase = bi->ExecBase
#define LOCAL_PROMETHEUSBASE() struct Library *PrometheusBase = getCardData(bi)->PrometheusBase
#define LOCAL_OPENPCIBASE()    struct Library *OpenPciBase = getCardData(bi)->OpenPciBase
// #define LOCAL_DOSBASE() struct Library *DOSBase = getChipData(bi)->DOSBase

static inline ULONG swapl(ULONG value)
{
    // endian swap value
    value = ((value & 0xFFFF0000) >> 16) | ((value & 0x0000FFFF) << 16);
    value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
    return value;
}

static inline UWORD swapw(UWORD value)
{
    // endian swap value
    value = (value & 0xFF00) >> 8 | (value & 0x00FF) << 8;
    return value;
}

#if BIGENDIAN_MMIO
#define SWAPW(x) x
#define SWAPL(x) x
#else
#define SWAPW(x) swapw(x)
#define SWAPL(x) swapl(x)
#endif

#ifdef BIGENDIAN_IO
#define SWAPW_IO(x) x
#define SWAPL_IO(x) x
#else
#define SWAPW_IO(x) swapw(x)
#define SWAPL_IO(x) swapl(x)
#endif

#define BIT(x) (1 << (x))

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
typedef struct OptionRomHeader
{
    UWORD signature;     // 0x0000: Signature (should be 0xAA55)
    UBYTE reserved[22];  // 0x0002: Reserved (usually 0, may contain PCI data structure pointer)
    UWORD pcir_offset;   // 0x0018: Pointer to PCI Data Structure (offset within the ROM)
} OptionRomHeader_t;

typedef struct PCI_DataStructure
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

static INLINE REGARGS volatile UBYTE *getLegacyBase(const struct BoardInfo *bi)
{
    return bi->RegisterBase;
}

static INLINE REGARGS volatile UBYTE *getMMIOBase(const struct BoardInfo *bi)
{
    return bi->MemoryIOBase;
}

static inline void flushWrites()
{
    asm volatile("nop");
}

static INLINE UBYTE REGARGS readReg(volatile UBYTE *regbase, LONG reg)
{
    return regbase[reg - REGISTER_OFFSET];
}

static INLINE void REGARGS writeReg(volatile UBYTE *regbase, LONG reg, UBYTE value)
{
    regbase[reg - REGISTER_OFFSET] = value;
}

static INLINE UWORD REGARGS readRegW(volatile UBYTE *regbase, LONG reg, const char *regName)
{
    UWORD value = SWAPW_IO(*(volatile UWORD *)(regbase + (reg - REGISTER_OFFSET)));
    asm volatile("" ::"r"(value));

    D(VERBOSE, "R %s -> 0x%04lx\n", regName, (ULONG)value);

    return value;
}

static INLINE void REGARGS writeRegW(volatile UBYTE *regbase, LONG reg, UWORD value, const char *regName)
{
    D(VERBOSE, "W %s <- 0x%04lx\n", regName, (ULONG)value);

    *(volatile UWORD *)(regbase + (reg - REGISTER_OFFSET)) = SWAPW_IO(value);
}

static INLINE void REGARGS writeRegL(volatile UBYTE *regbase, LONG reg, ULONG value, const char *regName)
{
    D(VERBOSE, "W %s <- 0x%08lx\n", regName, (ULONG)value);

    *(volatile ULONG *)(regbase + (reg - REGISTER_OFFSET)) = SWAPL_IO(value);
}

static INLINE ULONG REGARGS readRegLNoSwap(volatile UBYTE *regbase, LONG reg, const char *regName)
{
    ULONG value = *(volatile ULONG *)(regbase + (reg - REGISTER_OFFSET));
    asm volatile("" ::"r"(value));

    D(VERBOSE, "R %s -> 0x%08lx\n", regName, value);

    return value;
}

static INLINE void REGARGS writeRegLNoSwap(volatile UBYTE *regbase, LONG reg, ULONG value, const char *regName)
{
    D(VERBOSE, "W %s <- 0x%08lx\n", regName, (LONG)swapl(value));
    *(volatile ULONG *)(regbase + (reg - REGISTER_OFFSET)) = value;
}

static INLINE ULONG REGARGS readRegL(volatile UBYTE *regbase, LONG reg, const char *regName)
{
    ULONG value = SWAPL_IO(*(volatile ULONG *)(regbase + (reg - REGISTER_OFFSET)));
    asm volatile("" ::"r"(value));

    D(VERBOSE, "R %s -> 0x%08lx\n", regName, value);

    return value;
}

static INLINE UWORD REGARGS readMMIO_W(volatile UBYTE *mmiobase, LONG regOffset, const char *regName)
{
    flushWrites();
    UWORD value = SWAPW(*(volatile UWORD *)(mmiobase + (regOffset - MMIOREGISTER_OFFSET)));
    // This construct makes sure, the compiler doesn't take shortcuts
    // performing a byte access (for instance via btst) when the hardware really
    // requires register access as words
    asm volatile("" ::"r"(value));

    D(VERBOSE, "R %s -> 0x%04lx\n", (ULONG)regName, (ULONG)value);

    return value;
}

static INLINE ULONG REGARGS readMMIO_L(volatile UBYTE *mmiobase, LONG regOffset, const char *regName)
{
    flushWrites();
    // This construct makes sure, the compiler doesn't take shortcuts
    // performing a byte access (for instance via btst) when the hardware really
    // requires register access as words
    ULONG value = SWAPL(*(volatile ULONG *)(mmiobase + (regOffset - MMIOREGISTER_OFFSET)));
    asm volatile("" ::"r"(value));

    D(VERBOSE, "R %s -> 0x%08lx\n", (ULONG)regName, value);

    return value;
}

static INLINE void REGARGS writeMMIO_W(volatile UBYTE *mmiobase, LONG regOffset, UWORD value, const char *regName)
{
    D(VERBOSE, "W %s <- 0x%04lx\n", regName, (LONG)value);
    *(volatile UWORD *)(mmiobase + (regOffset - MMIOREGISTER_OFFSET)) = SWAPW(value);
}

static INLINE void REGARGS writeMMIO_L(volatile UBYTE *mmiobase, LONG regOffset, ULONG value, const char *regName)
{
    D(VERBOSE, "W %s <- 0x%08lx\n", regName, (LONG)value);

    *(volatile ULONG *)(mmiobase + (regOffset - MMIOREGISTER_OFFSET)) = SWAPL(value);
}

static INLINE UBYTE REGARGS readRegister(volatile UBYTE *regbase, LONG reg, const char *regName)
{
    UBYTE value = readReg(regbase, reg);
    D(VERBOSE, "R %s -> 0x%02lx\n", regName, (LONG)value);

    return value;
}

static INLINE void REGARGS writeRegister(volatile UBYTE *regbase, LONG reg, UBYTE value, const char *regName)
{
    D(VERBOSE, "W %s <- 0x%02lx\n", regName, (LONG)value);
    writeReg(regbase, reg, value);
}

static INLINE void REGARGS writeRegisterMask(volatile UBYTE *regbase, LONG reg, UBYTE mask, UBYTE value,
                                             const char *regName)
{
    writeRegister(regbase, reg, (readRegister(regbase, reg, regName) & ~mask) | (value & mask), regName);
}

static INLINE UBYTE REGARGS readCRx(volatile UBYTE *regbase, UBYTE regIndex)
{
    writeReg(regbase, CRTC_IDX, regIndex);
    UBYTE value = readReg(regbase, CRTC_DATA);

    D(VERBOSE, "R CR%lX -> 0x%02lx\n", (LONG)regIndex, (LONG)value);
    return value;
}

static INLINE void REGARGS writeCRx(volatile UBYTE *regbase, UBYTE regIndex, UBYTE value)
{
    writeReg(regbase, CRTC_IDX, regIndex);
    writeReg(regbase, CRTC_DATA, value);
    // FIXME: this doesn't work. I was hoping to write CRTC_IDX and CRTC_DATA in
    // one go
    //  writeRegW(regbase, CRTC_IDX, (regIndex << 8) | value );

    D(VERBOSE, "W CR%lX <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
}

static INLINE void REGARGS writeCRxMask(volatile UBYTE *regbase, UBYTE regIndex, UBYTE mask, UBYTE value)
{
    UBYTE regvalue = (readCRx(regbase, regIndex) & ~mask) | (value & mask);
    // Keep index register from previous read
    writeReg(regbase, CRTC_DATA, regvalue);

    D(VERBOSE, "W CR%lX <- 0x%02lx\n", (LONG)regIndex, (LONG)regvalue);
}

static INLINE UBYTE REGARGS readSRx(volatile UBYTE *regbase, UBYTE regIndex)
{
    writeReg(regbase, SEQX, regIndex);
    UBYTE value = readReg(regbase, SEQ_DATA);

    D(VERBOSE, "R SR%lX -> 0x%02lx\n", (LONG)regIndex, (LONG)value);

    return value;
}

static INLINE void REGARGS writeSRx(volatile UBYTE *regbase, UBYTE regIndex, UBYTE value)
{
    writeReg(regbase, SEQX, regIndex);
    writeReg(regbase, SEQ_DATA, value);

    D(VERBOSE, "W SR%lX <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
}

static INLINE void REGARGS writeSRxMask(volatile UBYTE *regbase, UBYTE regIndex, UBYTE mask, UBYTE value)
{
    writeReg(regbase, SEQX, regIndex);
    UBYTE regvalue = (readReg(regbase, SEQ_DATA) & ~mask) | (value & mask);
    writeReg(regbase, SEQ_DATA, regvalue);

    D(VERBOSE, "W SR%lX <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
    //  writeSRx(regbase, regIndex,
    //           (readSRx(regbase, regIndex) & ~mask) | (value & mask));
}

static INLINE UBYTE REGARGS readGRx(volatile UBYTE *regbase, UBYTE regIndex)
{
    writeReg(regbase, GRC_ADR, regIndex);
    UBYTE value = readReg(regbase, GRC_DATA);

    D(VERBOSE, "R GR%lX -> 0x%02lx\n", (LONG)regIndex, (LONG)value);
    return value;
}

static INLINE void REGARGS writeGRx(volatile UBYTE *regbase, UBYTE regIndex, UBYTE value)
{
    writeReg(regbase, GRC_ADR, regIndex);
    writeReg(regbase, GRC_DATA, value);

    D(VERBOSE, "W GR%lX <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
}

static INLINE UBYTE REGARGS readARx(volatile UBYTE *regbase, UBYTE regIndex)
{
    writeReg(regbase, ATR_AD, regIndex);
    UBYTE value = readReg(regbase, ATR_DATA_R);

    D(VERBOSE, "R AR%lX -> 0x%lx\n", (LONG)regIndex, (LONG)value);
    return value;
}

static INLINE void REGARGS writeARx(volatile UBYTE *regbase, UBYTE regIndex, UBYTE value)
{
    // This disables the "Enable video display" bit, but only when it is disabled,
    // the Attribute Controller registers may be accessed
    writeReg(regbase, ATR_AD, regIndex);
    writeReg(regbase, ATR_DATA_W, value);

    D(VERBOSE, "W AR%lX <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
}

static INLINE void REGARGS writeMISC_OUT(volatile UBYTE *regbase, UBYTE mask, UBYTE value)
{
    UBYTE misc = (readRegister(regbase, 0x3CC, "MISC_OUT_3CC") & ~mask) | (value & mask);
    writeRegister(regbase, 0x3C2, misc, "MISC_OUT_3C2");
}

#define REGBASE()  volatile UBYTE *RegBase = getLegacyBase(bi)
#define MMIOBASE() volatile UBYTE *MMIOBase = getMMIOBase(bi)

#define R_CR(reg)                   readCRx(RegBase, reg)
#define W_CR(reg, value)            writeCRx(RegBase, reg, value)
#define W_CR_MASK(reg, mask, value) writeCRxMask(RegBase, reg, mask, value)

#define R_SR(reg)                   readSRx(RegBase, reg)
#define W_SR(reg, value)            writeSRx(RegBase, reg, value)
#define W_SR_MASK(reg, mask, value) writeSRxMask(RegBase, reg, mask, value)

#define R_GR(reg)        readGRx(RegBase, reg)
#define W_GR(reg, value) writeGRx(RegBase, reg, value)

#define R_AR(reg)        readARx(RegBase, reg)
#define W_AR(reg, value) writeARx(RegBase, reg, value)

#define R_REG(reg)                   readRegister(RegBase, reg, #reg)
#define W_REG(reg, value)            writeRegister(RegBase, reg, value, #reg)
#define W_REG_MASK(reg, mask, value) writeRegisterMask(RegBase, reg, mask, value, #reg)

#define R_IO_W(reg)        readRegW(RegBase, reg, #reg)
#define R_IO_L(reg)        readRegL(RegBase, reg, #reg)
#define W_IO_W(reg, value) writeRegW(RegBase, reg, value, #reg)
#define W_IO_L(reg, value) writeRegL(RegBase, reg, value, #reg)

#define R_MMIO_W(reg)        readMMIO_W(MMIOBase, reg, #reg)
#define R_MMIO_L(reg)        readMMIO_L(MMIOBase, reg, #reg)
#define W_MMIO_W(reg, value) writeMMIO_W(MMIOBase, reg, value, #reg)
#define W_MMIO_L(reg, value) writeMMIO_L(MMIOBase, reg, value, #reg)

#define W_MISC_MASK(mask, value) writeMISC_OUT(RegBase, mask, value)

#define _W_CR_OF(value, reg, bitPos, numBits)                    \
    if (numBits < 8) {                                           \
        UWORD mask_W_CR_OF = ((1 << (numBits)) - 1) << (bitPos); \
        UWORD val_W_CR_OF  = ((value) << (bitPos));              \
        W_CR_MASK(reg, mask_W_CR_OF, val_W_CR_OF);               \
    } else {                                                     \
        W_CR(reg, value);                                        \
    }

#define W_CR_OVERFLOW1(value, reg, bitPos1, numBits1, overflowReg, bitPos2, numBits2) \
    do {                                                                              \
        UWORD val_W_CR_OVERFLOW1 = value;                                             \
        _W_CR_OF(val_W_CR_OVERFLOW1, reg, bitPos1, numBits1);                         \
        val_W_CR_OVERFLOW1 >>= numBits1;                                              \
        _W_CR_OF(val_W_CR_OVERFLOW1, overflowReg, bitPos2, numBits2);                 \
    } while (0);

#define W_CR_OVERFLOW2(value, reg, bitPos1, numBits1, overflowReg, bitPos2, numBits2, extOverflowReg, bitPos3, \
                       numBits3)                                                                               \
    do {                                                                                                       \
        _W_CR_OF(value, reg, bitPos1, numBits1);                                                               \
        _W_CR_OF((value >> numBits1), overflowReg, bitPos2, numBits2);                                         \
        _W_CR_OF((value >> (numBits1 + numBits2)), extOverflowReg, bitPos3, numBits3);                         \
    } while (0);

#define W_CR_OVERFLOW2_ULONG(value, reg, bitPos1, numBits1, overflowReg, bitPos2, numBits2, extOverflowReg, bitPos3, \
                             numBits3)                                                                               \
    do {                                                                                                             \
        ULONG val_W_CR_OVERFLOW2_LONG = value;                                                                       \
        _W_CR_OF(val_W_CR_OVERFLOW2_LONG, reg, bitPos1, numBits1);                                                   \
        val_W_CR_OVERFLOW2_LONG >>= numBits1;                                                                        \
        _W_CR_OF(val_W_CR_OVERFLOW2_LONG, overflowReg, bitPos2, numBits2);                                           \
        val_W_CR_OVERFLOW2_LONG >>= numBits2;                                                                        \
        _W_CR_OF(val_W_CR_OVERFLOW2_LONG, extOverflowReg, bitPos3, numBits3);                                        \
    } while (0);

// FIXME: can this be done vargs style?
#define W_CR_OVERFLOW3(value, reg, bitPos1, numBits1, overflowReg, bitPos2, numBits2, extOverflowReg, bitPos3, \
                       numBits3, extOverflowReg2, bitPos4, numBits4)                                           \
    do {                                                                                                       \
        UWORD val_W_CR_OVERFLOW3 = value;                                                                      \
        _W_CR_OF(val_W_CR_OVERFLOW3, reg, bitPos1, numBits1);                                                  \
        val_W_CR_OVERFLOW3 >>= numBits1;                                                                       \
        _W_CR_OF(val_W_CR_OVERFLOW3, overflowReg, bitPos2, numBits2);                                          \
        val_W_CR_OVERFLOW3 >>= numBits2;                                                                       \
        _W_CR_OF(val_W_CR_OVERFLOW3, extOverflowReg, bitPos3, numBits3);                                       \
        val_W_CR_OVERFLOW3 >>= numBits3;                                                                       \
        _W_CR_OF(val_W_CR_OVERFLOW3, extOverflowReg2, bitPos4, numBits4);                                      \
    } while (0);

static INLINE int makeDWORD(short hi, short lo)
{
    int res;
    __asm __volatile(
        "swap %0 \n"
        "move.w %2,%0"
        : "=&d"(res)
        : "0"(hi), "g"(lo)
        : "cc");
    return res;

    //    return hi << 16 | lo;
}

static inline UBYTE getBPP(RGBFTYPE format)
{
    // FIXME: replace with fixed table?
    switch (format) {
    case RGBFB_CLUT:
        return 1;
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        return 2;
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

static inline UBYTE getBPPLog2(RGBFTYPE format)
{
    // FIXME: replace with fixed table?
    switch (format) {
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
