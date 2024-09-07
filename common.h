#ifndef COMMON_H
#define COMMON_H

#include <SDI_compiler.h>
#include <clib/debug_protos.h>
#include <proto/exec.h>

#include <boardinfo.h>
// FIXME: copy header into common location
#include "endian.h"

#ifndef DBG
#define D(...)
#define DFUNC(...)
#define LOCAL_DEBUGLEVEL(x)
#else
extern int debugLevel;
#define LOCAL_DEBUGLEVEL(level) int debugLevel = level;

extern void myPrintF(const char *fmt, ...);

#define D(level, ...)            \
    if (debugLevel >= (level)) { \
        myPrintF(__VA_ARGS__);    \
    }
// Helper macro to allow call DFUNC with just one argument (and __VA_ARGS__
// being empty)
#define VA_ARGS(...) , ##__VA_ARGS__
#define DFUNC(level, fmt, ...)                              \
    if (debugLevel >= (level)) {                            \
        myPrintF("%s: " fmt, __func__ VA_ARGS(__VA_ARGS__)); \
    }
#endif

// The offsets allow for using signed 16bit indexed addressing be used
#define REGISTER_OFFSET 0x8000
#define MMIOREGISTER_OFFSET 0x8000

#define SEQX 0x3C4  // Access SRxx registers
#define SEQ_DATA 0x3C5

#define CRTC_IDX 0x3D4  // Access CRxx registers
#define CRTC_DATA 0x3D5

#define GRC_ADR 0x3CE
#define GRC_DATA 0x3CF

#define ATR_AD 0x3C0
#define ATR_DATA_W 0x3C0
#define ATR_DATA_R 0x3C1

#define DAC_WR_AD 0x3C8
#define DAC_DATA 0x3C9

#define CardPrometheusBase CardData[0]
#define CardPrometheusDevice CardData[1]

#define LOCAL_SYSBASE() struct ExecBase *SysBase = bi->ExecBase
#define LOCAL_PROMETHEUSBASE() struct Library *PrometheusBase = (struct Library *)(bi->CardPrometheusBase)
// #define LOCAL_DOSBASE() struct Library *DOSBase = getChipData(bi)->DOSBase

#if BIGENDIAN_MMIO
#define SWAPW(x) x
#define SWAPL(x) x
#else
#define SWAPW(x) swapw(x)
#define SWAPL(x) swapl(x)
#endif

#define BIT(x) (1 << (x))
typedef enum BlitterOp
{
    None,
    FILLRECT,
    INVERTRECT,
    BLITRECT,
    BLITRECTNOMASKCOMPLETE,
    BLITTEMPLATE,
    BLITPLANAR2CHUNKY,
    LINE

} BlitterOp_t;

// Remember: all in Little Endian!
typedef struct OptionROMHeader
{
    UWORD signature;       // 0x0000: Signature (should be 0xAA55)
    UBYTE reserved[22];    // 0x0002: Reserved (usually 0, may contain PCI data structure pointer)
    UWORD pcir_offset;  // 0x0018: Pointer to PCI Data Structure (offset within the ROM)
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
    USHORT r_min; // post divider log2
    USHORT r_max; // post divider log2
    ULONG f_vco_min;
    ULONG f_vco_max;
    ULONG f_base;
};

// f_wanted is in Khz
int svga_compute_pll(const struct svga_pll *pll, ULONG f_wanted_khz, USHORT *m, USHORT *n, USHORT *r);

void delayMicroSeconds(ULONG us);
void deleyMilliSeconds(ULONG ms);

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

static inline struct ChipData *getChipData(struct BoardInfo *bi)
{
    return (struct ChipData *)&bi->ChipData[0];
}

static INLINE REGARGS volatile UBYTE *getLegacyBase(const struct BoardInfo *bi)
{
    return bi->RegisterBase;
}

static INLINE REGARGS volatile UBYTE *getMMIOBase(const struct BoardInfo *bi)
{
    return bi->MemoryIOBase;
}

static inline UBYTE REGARGS readReg(volatile UBYTE *regbase, UWORD reg)
{
    return regbase[reg - REGISTER_OFFSET];
}

static inline void REGARGS writeReg(volatile UBYTE *regbase, UWORD reg, UBYTE value)
{
    regbase[reg - REGISTER_OFFSET] = value;
}

static inline UWORD REGARGS readRegW(volatile UBYTE *regbase, UWORD reg)
{
    UWORD value = swapw(*(volatile UWORD *)(regbase + (reg - REGISTER_OFFSET)));
    asm volatile("" ::"r"(value));

    D(10, "R 0x%.4lx -> 0x%04lx\n", (LONG)reg, (LONG)value);

    return value;
}

static inline void REGARGS writeRegW(volatile UBYTE *regbase, UWORD reg, UWORD value)
{
    D(10, "W 0x%.4lx <- 0x%04lx\n", (LONG)reg, (LONG)value);

    *(volatile UWORD *)(regbase + (reg - REGISTER_OFFSET)) = swapw(value);
}

static inline void REGARGS writeRegL(volatile UBYTE *regbase, UWORD reg, ULONG value)
{
    D(10, "W 0x%.4lx <- 0x%08lx\n", (LONG)reg, (LONG)value);

    *(volatile ULONG *)(regbase + (reg - REGISTER_OFFSET)) = swapl(value);
}

static inline ULONG REGARGS readRegL(volatile UBYTE *regbase, UWORD reg)
{
    ULONG value = swapl(*(volatile ULONG *)(regbase + (reg - REGISTER_OFFSET)));
    asm volatile("" ::"r"(value));

    D(10, "R 0x%.4lx -> 0x%08lx\n", (LONG)reg, (LONG)value);

    return value;
}

static inline UWORD REGARGS readMMIO_W(volatile UBYTE *mmiobase, UWORD regOffset)
{
    // This construct makes sure, the compiler doesn't take shortcuts
    // performing a byte access (for instance via btst) when the hardware really
    // requires register access as words
    UWORD value = SWAPW(*(volatile UWORD *)(mmiobase + (regOffset - MMIOREGISTER_OFFSET)));
    asm volatile("" ::"r"(value));
    return value;
}

static inline ULONG REGARGS readMMIO_L(volatile UBYTE *mmiobase, UWORD regOffset)
{
    // This construct makes sure, the compiler doesn't take shortcuts
    // performing a byte access (for instance via btst) when the hardware really
    // requires register access as words
    ULONG value = SWAPL(*(volatile ULONG *)(mmiobase + (regOffset - MMIOREGISTER_OFFSET)));
    asm volatile("" ::"r"(value));
    return value;
}

static inline void REGARGS writeMMIO_W(volatile UBYTE *mmiobase, UWORD regOffset, UWORD value)
{
    D(10, "W 0x%.4lx <- 0x%04lx\n", (LONG)regOffset, (LONG)value);
    *(volatile UWORD *)(mmiobase + (regOffset - MMIOREGISTER_OFFSET)) = SWAPW(value);
}

static inline void REGARGS writeMMIO_L(volatile UBYTE *mmiobase, UWORD regOffset, ULONG value)
{
    D(10, "W 0x%.4lx <- 0x%08lx\n", (LONG)regOffset, (LONG)value);

    *(volatile ULONG *)(mmiobase + (regOffset - MMIOREGISTER_OFFSET)) = SWAPL(value);
}

static inline UBYTE REGARGS readRegister(volatile UBYTE *regbase, UWORD reg)
{
    UBYTE value = readReg(regbase, reg);
    D(20, "R 0x%.4lx -> 0x%02lx\n", (LONG)reg, (LONG)value);

    return value;
}

static inline void REGARGS writeRegister(volatile UBYTE *regbase, UWORD reg, UBYTE value)
{
    writeReg(regbase, reg, value);

    D(10, "W 0x%.4lx <- 0x%02lx\n", (LONG)reg, (LONG)value);
}

static inline void REGARGS writeRegisterMask(volatile UBYTE *regbase, UWORD reg, UBYTE mask, UBYTE value)
{
    writeRegister(regbase, reg, (readRegister(regbase, reg) & ~mask) | (value & mask));
}

static inline UBYTE REGARGS readCRx(volatile UBYTE *regbase, UBYTE regIndex)
{
    writeReg(regbase, CRTC_IDX, regIndex);
    UBYTE value = readReg(regbase, CRTC_DATA);

    D(20, "R CR%.2lx -> 0x%02lx\n", (LONG)regIndex, (LONG)value);
    return value;
}

static inline void REGARGS writeCRx(volatile UBYTE *regbase, UBYTE regIndex, UBYTE value)
{
    writeReg(regbase, CRTC_IDX, regIndex);
    writeReg(regbase, CRTC_DATA, value);
    // FIXME: this doesn't work. I was hoping to write CRTC_IDX and CRTC_DATA in
    // one go
    //  writeRegW(regbase, CRTC_IDX, (regIndex << 8) | value );

    D(10, "W CR%.2lx <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
}

static inline void REGARGS writeCRxMask(volatile UBYTE *regbase, UBYTE regIndex, UBYTE mask, UBYTE value)
{
    UBYTE regvalue = (readCRx(regbase, regIndex) & ~mask) | (value & mask);
    // Keep index register from previous read
    writeReg(regbase, CRTC_DATA, regvalue);

    D(10, "W CR%.2lx <- 0x%02lx\n", (LONG)regIndex, (LONG)regvalue);
}

static inline UBYTE REGARGS readSRx(volatile UBYTE *regbase, UBYTE regIndex)
{
    writeReg(regbase, SEQX, regIndex);
    UBYTE value = readReg(regbase, SEQ_DATA);

    D(10, "R SR%2lx -> 0x%02lx\n", (LONG)regIndex, (LONG)value);

    return value;
}

static inline void REGARGS writeSRx(volatile UBYTE *regbase, UBYTE regIndex, UBYTE value)
{
    writeReg(regbase, SEQX, regIndex);
    writeReg(regbase, SEQ_DATA, value);

    D(10, "W SR%2lx <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
}

static inline void REGARGS writeSRxMask(volatile UBYTE *regbase, UBYTE regIndex, UBYTE mask, UBYTE value)
{
    writeReg(regbase, SEQX, regIndex);
    UBYTE regvalue = (readReg(regbase, SEQ_DATA) & ~mask) | (value & mask);
    writeReg(regbase, SEQ_DATA, regvalue);

    D(10, "W SR%2lx <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
    //  writeSRx(regbase, regIndex,
    //           (readSRx(regbase, regIndex) & ~mask) | (value & mask));
}

static inline UBYTE REGARGS readGRx(volatile UBYTE *regbase, UBYTE regIndex)
{
    writeReg(regbase, GRC_ADR, regIndex);
    UBYTE value = readReg(regbase, GRC_DATA);

    D(20, "R GR%2lx -> 0x%02lx\n", (LONG)regIndex, (LONG)value);
    return value;
}

static inline void REGARGS writeGRx(volatile UBYTE *regbase, UBYTE regIndex, UBYTE value)
{
    writeReg(regbase, GRC_ADR, regIndex);
    writeReg(regbase, GRC_DATA, value);

    D(10, "W GR%2lx <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
}

static inline UBYTE REGARGS readARx(volatile UBYTE *regbase, UBYTE regIndex)
{
    writeReg(regbase, ATR_AD, regIndex);
    UBYTE value = readReg(regbase, ATR_DATA_R);

    D(20, "R AR%2lx -> 0x%lx\n", (LONG)regIndex, (LONG)value);
    return value;
}

static inline void REGARGS writeARx(volatile UBYTE *regbase, UBYTE regIndex, UBYTE value)
{
    // This disables the "Enable video display" bit, but only when it is disabled,
    // the Attribute Controller registers may be accessed
    writeReg(regbase, ATR_AD, regIndex);
    writeReg(regbase, ATR_DATA_W, value);

    D(10, "W AR%2lx <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
}

static inline void REGARGS writeMISC_OUT(volatile UBYTE *regbase, UBYTE mask, UBYTE value)
{
    UBYTE misc = (readRegister(regbase, 0x3CC) & ~mask) | (value & mask);
    writeRegister(regbase, 0x3C2, misc);
}

#define REGBASE() volatile UBYTE *RegBase = getLegacyBase(bi)
#define MMIOBASE() volatile UBYTE *MMIOBase = getMMIOBase(bi)

#define R_CR(reg) readCRx(RegBase, reg)
#define W_CR(reg, value) writeCRx(RegBase, reg, value)
#define W_CR_MASK(reg, mask, value) writeCRxMask(RegBase, reg, mask, value)

#define R_SR(reg) readSRx(RegBase, reg)
#define W_SR(reg, value) writeSRx(RegBase, reg, value)
#define W_SR_MASK(reg, mask, value) writeSRxMask(RegBase, reg, mask, value)

#define R_GR(reg) readGRx(RegBase, reg)
#define W_GR(reg, value) writeGRx(RegBase, reg, value)

#define R_AR(reg) readARx(RegBase, reg)
#define W_AR(reg, value) writeARx(RegBase, reg, value)

#define R_REG(reg) readRegister(RegBase, reg)
#define W_REG(reg, value) writeRegister(RegBase, reg, value)
#define W_REG_MASK(reg, mask, value) writeRegisterMask(RegBase, reg, mask, value)

#define R_IO_W(reg) readRegW(RegBase, reg)
#define R_IO_L(reg) readRegL(RegBase, reg)
#define W_IO_W(reg, value) writeRegW(RegBase, reg, value)
#define W_IO_L(reg, value) writeRegL(RegBase, reg, value)

#define R_MMIO_W(reg) readMMIO_W(MMIOBase, reg)
#define R_MMIO_L(reg) readMMIO_L(MMIOBase, reg)
#define W_MMIO_W(reg, value) writeMMIO_W(MMIOBase, reg, value)
#define W_MMIO_L(reg, value) writeMMIO_L(MMIOBase, reg, value)

#define W_MISC_MASK(mask, value) writeMISC_OUT(RegBase, mask, value)

#define _W_CR_OF(value, reg, bitPos, numBits)                    \
    if (numBits < 8) {                                           \
        UWORD mask_W_CR_OF = ((1 << (numBits)) - 1) << (bitPos); \
        UWORD val_W_CR_OF = ((value) << (bitPos));               \
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

static inline int makeDWORD(short hi, short lo)
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

BOOL InitChip(__REGA0(struct BoardInfo *bi));

#endif  // COMMON_H
