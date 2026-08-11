#ifndef REG_ACCESS_HPP
#define REG_ACCESS_HPP

/*
 * Policy-templated register apertures for m68k P96 drivers.
 * Include after chip *config.h / common.h (needs UBYTE/UWORD/ULONG, SWAP*,
 * REGISTER_OFFSET / MMIOREGISTER_OFFSET, flushWrites, D()).
 */

#include "common.h"
#include "vga_regs.hpp"

enum class RegEndian
{
    NoSwap,
    Swap
};
enum class RegLog
{
    Quiet,
    Verbose
};

template <typename T, RegEndian E>
struct EndianOps;

template <typename T>
struct EndianOps<T, RegEndian::NoSwap>
{
    static INLINE T in(T v) { return v; }
    static INLINE T out(T v) { return v; }
    static INLINE T dbg(T v) { return v; }
};

template <>
struct EndianOps<UBYTE, RegEndian::Swap>
{
    static INLINE UBYTE in(UBYTE v) { return v; }
    static INLINE UBYTE out(UBYTE v) { return v; }
    static INLINE UBYTE dbg(UBYTE v) { return v; }
};

template <typename T>
struct EndianOps<T, RegEndian::Swap>
{
    static INLINE T in(T v) { return swap(v); }
    static INLINE T out(T v) { return swap(v); }
    static INLINE T dbg(T v) { return v; }
};

template <typename T>
static INLINE T regLoadRaw(volatile T *p)
{
    flushWrites();
    T v = *p;
    asm volatile("" ::"r"(v));
    return v;
}

template <RegLog L>
struct RegLogOps;

template <>
struct RegLogOps<RegLog::Quiet>
{
    template <typename T>
    static INLINE void read(const char *, T)
    {
    }
    template <typename T>
    static INLINE void write(const char *, T)
    {
    }
};

template <>
struct RegLogOps<RegLog::Verbose>
{
    template <typename T>
    static INLINE void read(const char *name, T v)
    {
#ifdef DBG
        D(VERBOSE, "R %s -> 0x%08lx\n", name, (ULONG)v);
#else
        (void)name;
        (void)v;
#endif
    }
    template <typename T>
    static INLINE void write(const char *name, T v)
    {
#ifdef DBG
        D(VERBOSE, "W %s <- 0x%08lx\n", name, (ULONG)v);
#else
        (void)name;
        (void)v;
#endif
    }
};

/* Byte-offset register aperture (VGA / S3-style). */
template <RegEndian E, LONG BaseOff, RegLog L>
struct RegAperture
{
    volatile UBYTE *base;

    explicit RegAperture(volatile UBYTE *b) : base(b) {}

    template <typename T>
    INLINE T readOff(LONG off
#ifdef DBG
                     ,
                     const char *name = 0
#endif
    ) const
    {
        T v = EndianOps<T, E>::in(regLoadRaw((volatile T *)(base + (off - BaseOff))));
#ifdef DBG
        RegLogOps<L>::read(name ? name : "?", EndianOps<T, E>::dbg(v));
#endif
        return v;
    }

    template <typename T>
    INLINE void writeOff(LONG off, T v
#ifdef DBG
                         ,
                         const char *name = 0
#endif
    ) const
    {
#ifdef DBG
        RegLogOps<L>::write(name ? name : "?", EndianOps<T, E>::dbg(v));
#endif
        *(volatile T *)(base + (off - BaseOff)) = EndianOps<T, E>::out(v);
    }

    template <typename T>
    INLINE void writeMaskOff(LONG off, T mask, T val
#ifdef DBG
                             ,
                             const char *name = 0
#endif
    ) const
    {
        T regValue = regLoadRaw((volatile T *)(base + (off - BaseOff)));
        T m        = EndianOps<T, E>::out(mask);
        T bits     = EndianOps<T, E>::out(val);
        regValue   = (regValue & ~m) | (m & bits);
#ifdef DBG
        RegLogOps<L>::write(name ? name : "?", EndianOps<T, E>::dbg(EndianOps<T, E>::in(regValue)));
#endif
        *(volatile T *)(base + (off - BaseOff)) = regValue;
    }

    /* Like TST_IO_*: raw read & EndianOps::out(mask). */
    template <typename T>
    INLINE BOOL testOff(LONG off, T mask
#ifdef DBG
                        ,
                        const char *name = 0
#endif
    ) const
    {
        T raw = regLoadRaw((volatile T *)(base + (off - BaseOff)));
#ifdef DBG
        RegLogOps<L>::read(name ? name : "?", EndianOps<T, E>::dbg(EndianOps<T, E>::in(raw)));
#endif
        return (raw & EndianOps<T, E>::out(mask)) != 0;
    }

    INLINE BOOL testWOff(LONG off, UWORD mask
#ifdef DBG
                         ,
                         const char *name = 0
#endif
    ) const
    {
        return testOff<UWORD>(off, mask
#ifdef DBG
                              ,
                              name
#endif
        );
    }

    INLINE BOOL testLOff(LONG off, ULONG mask
#ifdef DBG
                         ,
                         const char *name = 0
#endif
    ) const
    {
        return testOff<ULONG>(off, mask
#ifdef DBG
                              ,
                              name
#endif
        );
    }
};

/*
 * ATI Mach64-style aperture: RegId is dword-index RegId.
 * Address = id * 4; never treat id as a byte offset.
 */
template <typename RegId, RegEndian E, LONG BaseOff, RegLog L>
struct AtiRegAperture
{
    volatile UBYTE *base;

    explicit AtiRegAperture(volatile UBYTE *b) : base(b) {}

    static INLINE LONG byteOff(RegId id) { return (LONG)id * 4; }

    template <typename T>
    INLINE T read(RegId id
#ifdef DBG
                  ,
                  const char *name = 0
#endif
    ) const
    {
        T v = EndianOps<T, E>::in(regLoadRaw((volatile T *)(base + (byteOff(id) - BaseOff))));
#ifdef DBG
        RegLogOps<L>::read(name ? name : regName(id), EndianOps<T, E>::dbg(v));
#endif
        return v;
    }

    template <typename T>
    INLINE void write(RegId id, T v
#ifdef DBG
                      ,
                      const char *name = 0
#endif
    ) const
    {
#ifdef DBG
        RegLogOps<L>::write(name ? name : regName(id), EndianOps<T, E>::dbg(v));
#endif
        *(volatile T *)(base + (byteOff(id) - BaseOff)) = EndianOps<T, E>::out(v);
    }

    INLINE UWORD readW(RegId id
#ifdef DBG
                       ,
                       const char *name = 0
#endif
    ) const
    {
        return read<UWORD>(id
#ifdef DBG
                           ,
                           name
#endif
        );
    }

    INLINE ULONG readL(RegId id
#ifdef DBG
                       ,
                       const char *name = 0
#endif
    ) const
    {
        return read<ULONG>(id
#ifdef DBG
                           ,
                           name
#endif
        );
    }

    INLINE void writeW(RegId id, UWORD v
#ifdef DBG
                       ,
                       const char *name = 0
#endif
    ) const
    {
        write<UWORD>(id, v
#ifdef DBG
                     ,
                     name
#endif
        );
    }

    INLINE void writeL(RegId id, ULONG v
#ifdef DBG
                       ,
                       const char *name = 0
#endif
    ) const
    {
        write<ULONG>(id, v
#ifdef DBG
                     ,
                     name
#endif
        );
    }

    INLINE void writeMaskL(RegId id, ULONG mask, ULONG val
#ifdef DBG
                           ,
                           const char *name = 0
#endif
    ) const
    {
        writeMask<ULONG>(id, mask, val
#ifdef DBG
                         ,
                         name
#endif
        );
    }

    /*
     * Like TST_MMIO_*: raw (unswapped) read & EndianOps::out(mask).
     * Constant masks fold to a pre-swapped immediate; the MMIO load stays unswapped.
     */
    template <typename T>
    INLINE BOOL test(RegId id, T mask
#ifdef DBG
                     ,
                     const char *name = 0
#endif
    ) const
    {
        T raw = regLoadRaw((volatile T *)(base + (byteOff(id) - BaseOff)));
#ifdef DBG
        RegLogOps<L>::read(name ? name : regName(id), EndianOps<T, E>::dbg(EndianOps<T, E>::in(raw)));
#endif
        return (raw & EndianOps<T, E>::out(mask)) != 0;
    }

    INLINE BOOL testW(RegId id, UWORD mask
#ifdef DBG
                      ,
                      const char *name = 0
#endif
    ) const
    {
        return test<UWORD>(id, mask
#ifdef DBG
                           ,
                           name
#endif
        );
    }

    INLINE BOOL testL(RegId id, ULONG mask
#ifdef DBG
                      ,
                      const char *name = 0
#endif
    ) const
    {
        return test<ULONG>(id, mask
#ifdef DBG
                           ,
                           name
#endif
        );
    }

    template <typename T>
    INLINE void writeMask(RegId id, T mask, T val
#ifdef DBG
                          ,
                          const char *name = 0
#endif
    ) const
    {
        T regValue = regLoadRaw((volatile T *)(base + (byteOff(id) - BaseOff)));
        T m        = EndianOps<T, E>::out(mask);
        T bits     = EndianOps<T, E>::out(val);
        regValue   = (regValue & ~m) | (m & bits);
#ifdef DBG
        RegLogOps<L>::write(name ? name : regName(id), EndianOps<T, E>::dbg(EndianOps<T, E>::in(regValue)));
#endif
        *(volatile T *)(base + (byteOff(id) - BaseOff)) = regValue;
    }

    INLINE UBYTE readB(RegId id, WORD byteIndex
#ifdef DBG
                       ,
                       const char *name = 0
#endif
    ) const
    {
        UBYTE v = regLoadRaw(base + (byteOff(id) + byteIndex - BaseOff));
#ifdef DBG
        RegLogOps<L>::read(name ? name : regName(id), v);
#endif
        return v;
    }

    INLINE void writeB(RegId id, WORD byteIndex, UBYTE v
#ifdef DBG
                       ,
                       const char *name = 0
#endif
    ) const
    {
#ifdef DBG
        RegLogOps<L>::write(name ? name : regName(id), v);
#endif
        base[byteOff(id) + byteIndex - BaseOff] = v;
    }
};

/*
 * Absolute-address aperture (Mach32 port I/O, S3/VGA MMIO): RegId value is the
 * hardware index/port address; byte address = id - BaseOff (REGISTER_OFFSET /
 * MMIOREGISTER_OFFSET). Same API shape as AtiRegAperture.
 */
template <typename RegId, RegEndian E, LONG BaseOff, RegLog L>
struct AbsRegAperture
{
    volatile UBYTE *base;

    explicit AbsRegAperture(volatile UBYTE *b) : base(b) {}

    static INLINE LONG byteOff(RegId id) { return (LONG)id; }

    template <typename T>
    INLINE T read(RegId id
#ifdef DBG
                  ,
                  const char *name = 0
#endif
    ) const
    {
        T v = EndianOps<T, E>::in(regLoadRaw((volatile T *)(base + (byteOff(id) - BaseOff))));
#ifdef DBG
        RegLogOps<L>::read(name ? name : regName(id), EndianOps<T, E>::dbg(v));
#endif
        return v;
    }

    template <typename T>
    INLINE void write(RegId id, T v
#ifdef DBG
                      ,
                      const char *name = 0
#endif
    ) const
    {
#ifdef DBG
        RegLogOps<L>::write(name ? name : regName(id), EndianOps<T, E>::dbg(v));
#endif
        *(volatile T *)(base + (byteOff(id) - BaseOff)) = EndianOps<T, E>::out(v);
    }

    INLINE UWORD readW(RegId id
#ifdef DBG
                       ,
                       const char *name = 0
#endif
    ) const
    {
        return read<UWORD>(id
#ifdef DBG
                           ,
                           name
#endif
        );
    }

    INLINE ULONG readL(RegId id
#ifdef DBG
                       ,
                       const char *name = 0
#endif
    ) const
    {
        return read<ULONG>(id
#ifdef DBG
                           ,
                           name
#endif
        );
    }

    INLINE UBYTE readB(RegId id
#ifdef DBG
                       ,
                       const char *name = 0
#endif
    ) const
    {
        return read<UBYTE>(id
#ifdef DBG
                           ,
                           name
#endif
        );
    }

    INLINE void writeW(RegId id, UWORD v
#ifdef DBG
                       ,
                       const char *name = 0
#endif
    ) const
    {
        write<UWORD>(id, v
#ifdef DBG
                     ,
                     name
#endif
        );
    }

    INLINE void writeL(RegId id, ULONG v
#ifdef DBG
                       ,
                       const char *name = 0
#endif
    ) const
    {
        write<ULONG>(id, v
#ifdef DBG
                     ,
                     name
#endif
        );
    }

    INLINE void writeB(RegId id, UBYTE v
#ifdef DBG
                       ,
                       const char *name = 0
#endif
    ) const
    {
        write<UBYTE>(id, v
#ifdef DBG
                     ,
                     name
#endif
        );
    }

    template <typename T>
    INLINE void writeMask(RegId id, T mask, T val
#ifdef DBG
                          ,
                          const char *name = 0
#endif
    ) const
    {
        T regValue = regLoadRaw((volatile T *)(base + (byteOff(id) - BaseOff)));
        T m        = EndianOps<T, E>::out(mask);
        T bits     = EndianOps<T, E>::out(val);
        regValue   = (regValue & ~m) | (m & bits);
#ifdef DBG
        RegLogOps<L>::write(name ? name : regName(id), EndianOps<T, E>::dbg(EndianOps<T, E>::in(regValue)));
#endif
        *(volatile T *)(base + (byteOff(id) - BaseOff)) = regValue;
    }

    INLINE void writeMaskB(RegId id, UBYTE mask, UBYTE val
#ifdef DBG
                           ,
                           const char *name = 0
#endif
    ) const
    {
        writeMask<UBYTE>(id, mask, val
#ifdef DBG
                         ,
                         name
#endif
        );
    }

    INLINE void writeMaskW(RegId id, UWORD mask, UWORD val
#ifdef DBG
                           ,
                           const char *name = 0
#endif
    ) const
    {
        writeMask<UWORD>(id, mask, val
#ifdef DBG
                         ,
                         name
#endif
        );
    }

    INLINE void writeMaskL(RegId id, ULONG mask, ULONG val
#ifdef DBG
                           ,
                           const char *name = 0
#endif
    ) const
    {
        writeMask<ULONG>(id, mask, val
#ifdef DBG
                         ,
                         name
#endif
        );
    }

    template <typename T>
    INLINE BOOL test(RegId id, T mask
#ifdef DBG
                     ,
                     const char *name = 0
#endif
    ) const
    {
        T raw = regLoadRaw((volatile T *)(base + (byteOff(id) - BaseOff)));
#ifdef DBG
        RegLogOps<L>::read(name ? name : regName(id), EndianOps<T, E>::dbg(EndianOps<T, E>::in(raw)));
#endif
        return (raw & EndianOps<T, E>::out(mask)) != 0;
    }

    INLINE BOOL testW(RegId id, UWORD mask
#ifdef DBG
                      ,
                      const char *name = 0
#endif
    ) const
    {
        return test<UWORD>(id, mask
#ifdef DBG
                           ,
                           name
#endif
        );
    }

    INLINE BOOL testL(RegId id, ULONG mask
#ifdef DBG
                      ,
                      const char *name = 0
#endif
    ) const
    {
        return test<ULONG>(id, mask
#ifdef DBG
                           ,
                           name
#endif
        );
    }

    /* Raw (pre-endian) word read — Mach32 FIFO poll / QI path. */
    INLINE UWORD readWRaw(RegId id) const { return regLoadRaw((volatile UWORD *)(base + (byteOff(id) - BaseOff))); }

    /* Raw long write — legacy W_MMIO_NOSWAP_L. */
    INLINE void writeLRaw(RegId id, ULONG v
#ifdef DBG
                          ,
                          const char *name = 0
#endif
    ) const
    {
#ifdef DBG
        RegLogOps<L>::write(name ? name : regName(id), EndianOps<ULONG, E>::dbg(EndianOps<ULONG, E>::in(v)));
#endif
        *(volatile ULONG *)(base + (byteOff(id) - BaseOff)) = v;
    }
};

/*
 * Standard VGA port aperture: flat DAC/MISC/INPUT_STATUS1 access plus CR/SR/GR/AR
 * index/data helpers. Shared by S3, AT3D, and any VGA-compatible chip.
 */
template <LONG BaseOff, RegLog L>
struct VgaAperture : protected AbsRegAperture<VgaReg::Id, RegEndian::Swap, BaseOff, L>
{
    using Base = AbsRegAperture<VgaReg::Id, RegEndian::Swap, BaseOff, L>;
    using Base::base;
    using Base::readB;
    using Base::writeB;
    using Base::writeMaskB;

    explicit VgaAperture(volatile UBYTE *b) : Base(b) {}

    INLINE UBYTE readCR(UBYTE idx) const
    {
        storeB(VgaReg::CRTC_INDEX, idx);
        UBYTE value = loadB(VgaReg::CRTC_VALUE);
        D(VERBOSE, "R CR%lX -> 0x%02lx\n", (LONG)idx, (LONG)value);
        return value;
    }

    INLINE void writeCR(UBYTE idx, UBYTE value) const
    {
        storeB(VgaReg::CRTC_INDEX, idx);
        storeB(VgaReg::CRTC_VALUE, value);
        D(VERBOSE, "W CR%lX <- 0x%02lx\n", (LONG)idx, (LONG)value);
    }

    INLINE void writeCRMask(UBYTE idx, UBYTE mask, UBYTE value) const
    {
        UBYTE regvalue = (readCR(idx) & ~mask) | (value & mask);
        storeB(VgaReg::CRTC_VALUE, regvalue);
        D(VERBOSE, "W CR%lX <- 0x%02lx\n", (LONG)idx, (LONG)regvalue);
    }

    INLINE UBYTE readSR(UBYTE idx) const
    {
        storeB(VgaReg::SEQ_INDEX, idx);
        UBYTE value = loadB(VgaReg::SEQ_VALUE);
        D(VERBOSE, "R SR%lX -> 0x%02lx\n", (LONG)idx, (LONG)value);
        return value;
    }

    INLINE void writeSR(UBYTE idx, UBYTE value) const
    {
        storeB(VgaReg::SEQ_INDEX, idx);
        storeB(VgaReg::SEQ_VALUE, value);
        D(VERBOSE, "W SR%lX <- 0x%02lx\n", (LONG)idx, (LONG)value);
    }

    INLINE void writeSRMask(UBYTE idx, UBYTE mask, UBYTE value) const
    {
        storeB(VgaReg::SEQ_INDEX, idx);
        UBYTE regvalue = (loadB(VgaReg::SEQ_VALUE) & ~mask) | (value & mask);
        D(VERBOSE, "W SR%lX <- 0x%02lx\n", (LONG)idx, (ULONG)regvalue);
        storeB(VgaReg::SEQ_VALUE, regvalue);
    }

    INLINE UBYTE readGR(UBYTE idx) const
    {
        storeB(VgaReg::GRC_INDEX, idx);
        UBYTE value = loadB(VgaReg::GRC_VALUE);
        D(VERBOSE, "R GR%lX -> 0x%02lx\n", (LONG)idx, (LONG)value);
        return value;
    }

    INLINE void writeGR(UBYTE idx, UBYTE value) const
    {
        storeB(VgaReg::GRC_INDEX, idx);
        storeB(VgaReg::GRC_VALUE, value);
        D(VERBOSE, "W GR%lX <- 0x%02lx\n", (LONG)idx, (LONG)value);
    }

    INLINE UBYTE readAR(UBYTE idx) const
    {
        storeB(VgaReg::ATTR_AD, idx);
        UBYTE value = loadB(VgaReg::ATTR_DATA_R);
        D(VERBOSE, "R AR%lX -> 0x%lx\n", (LONG)idx, (LONG)value);
        return value;
    }

    INLINE void writeAR(UBYTE idx, UBYTE value) const
    {
        storeB(VgaReg::ATTR_AD, (UBYTE)(idx | 0x20));
        storeB(VgaReg::ATTR_DATA_W, value);
        D(VERBOSE, "W AR%lX <- 0x%02lx\n", (LONG)idx, (LONG)value);
    }

    INLINE void writeMiscMask(UBYTE mask, UBYTE value) const
    {
        UBYTE misc = (readB(VgaReg::MISC_OUT_R) & ~mask) | (value & mask);
        writeB(VgaReg::MISC_OUT_W, misc);
    }

    INLINE void writeCROverflow1(UWORD value, UBYTE reg, UBYTE bitPos1, UBYTE numBits1, UBYTE overflowReg,
                                 UBYTE bitPos2, UBYTE numBits2) const
    {
        writeCROfField(value, reg, bitPos1, numBits1);
        writeCROfField(value >> numBits1, overflowReg, bitPos2, numBits2);
    }

    INLINE void writeCROverflow2(UWORD value, UBYTE reg, UBYTE bitPos1, UBYTE numBits1, UBYTE overflowReg,
                                 UBYTE bitPos2, UBYTE numBits2, UBYTE extOverflowReg, UBYTE bitPos3,
                                 UBYTE numBits3) const
    {
        writeCROfField(value, reg, bitPos1, numBits1);
        writeCROfField(value >> numBits1, overflowReg, bitPos2, numBits2);
        writeCROfField(value >> (numBits1 + numBits2), extOverflowReg, bitPos3, numBits3);
    }

    INLINE void writeCROverflow2U(ULONG value, UBYTE reg, UBYTE bitPos1, UBYTE numBits1, UBYTE overflowReg,
                                  UBYTE bitPos2, UBYTE numBits2, UBYTE extOverflowReg, UBYTE bitPos3,
                                  UBYTE numBits3) const
    {
        writeCROfField(value, reg, bitPos1, numBits1);
        value >>= numBits1;
        writeCROfField(value, overflowReg, bitPos2, numBits2);
        value >>= numBits2;
        writeCROfField(value, extOverflowReg, bitPos3, numBits3);
    }

    INLINE void writeCROverflow3(UWORD value, UBYTE reg, UBYTE bitPos1, UBYTE numBits1, UBYTE overflowReg,
                                 UBYTE bitPos2, UBYTE numBits2, UBYTE extOverflowReg, UBYTE bitPos3, UBYTE numBits3,
                                 UBYTE extOverflowReg2, UBYTE bitPos4, UBYTE numBits4) const
    {
        writeCROfField(value, reg, bitPos1, numBits1);
        value >>= numBits1;
        writeCROfField(value, overflowReg, bitPos2, numBits2);
        value >>= numBits2;
        writeCROfField(value, extOverflowReg, bitPos3, numBits3);
        value >>= numBits3;
        writeCROfField(value, extOverflowReg2, bitPos4, numBits4);
    }

   private:
    INLINE void storeB(VgaReg::Id port, UBYTE v) const { base[(LONG)port - BaseOff] = v; }

    INLINE UBYTE loadB(VgaReg::Id port) const
    {
        flushWrites();
        return base[(LONG)port - BaseOff];
    }

    INLINE void writeCROfField(ULONG value, UBYTE reg, UBYTE bitPos, UBYTE numBits) const
    {
        if (numBits < 8) {
            UBYTE m = ((1 << numBits) - 1) << bitPos;
            UBYTE v = (UBYTE)(value << bitPos);
            writeCRMask(reg, m, v);
        } else {
            writeCR(reg, (UBYTE)value);
        }
    }
};

/* Software shadow for write-only / unsafe-RMW registers. Win is stored by value
 * (AbsRegAperture is a pointer wrapper). shadow points at durable state (e.g. CardData). */
template <typename Win, typename RegId, typename T = ULONG>
struct CachedReg
{
    Win win;
    RegId id;
    T *shadow;

    CachedReg(Win w, RegId i, T *s) : win(w), id(i), shadow(s) {}

    INLINE void write(T v)
    {
        *shadow = v;
        win.template write<T>(id, *shadow);
    }

    INLINE void writeMask(T mask, T val) { write((T)((*shadow & ~mask) | (val & mask))); }

    INLINE T get() const { return *shadow; }
};

#endif /* REG_ACCESS_HPP */
