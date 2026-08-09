#ifndef REG_ACCESS_HPP
#define REG_ACCESS_HPP

/*
 * Policy-templated register apertures for m68k P96 drivers.
 * Include after chip *config.h / common.h (needs UBYTE/UWORD/ULONG, SWAP*,
 * REGISTER_OFFSET / MMIOREGISTER_OFFSET, flushWrites, D()).
 */

#include "common.h"

enum class RegEndian
{
    NoSwap,
    SwapIO,
    SwapMMIO
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
struct EndianOps<UBYTE, RegEndian::SwapIO>
{
    static INLINE UBYTE in(UBYTE v) { return v; }
    static INLINE UBYTE out(UBYTE v) { return v; }
    static INLINE UBYTE dbg(UBYTE v) { return v; }
};

template <>
struct EndianOps<UWORD, RegEndian::SwapIO>
{
    static INLINE UWORD in(UWORD v) { return SWAPW_IO(v); }
    static INLINE UWORD out(UWORD v) { return SWAPW_IO(v); }
    static INLINE UWORD dbg(UWORD v) { return v; }
};

template <>
struct EndianOps<ULONG, RegEndian::SwapIO>
{
    static INLINE ULONG in(ULONG v) { return SWAPL_IO(v); }
    static INLINE ULONG out(ULONG v) { return SWAPL_IO(v); }
    static INLINE ULONG dbg(ULONG v) { return v; }
};

template <>
struct EndianOps<UBYTE, RegEndian::SwapMMIO>
{
    static INLINE UBYTE in(UBYTE v) { return v; }
    static INLINE UBYTE out(UBYTE v) { return v; }
    static INLINE UBYTE dbg(UBYTE v) { return v; }
};

template <>
struct EndianOps<UWORD, RegEndian::SwapMMIO>
{
    static INLINE UWORD in(UWORD v) { return SWAPW(v); }
    static INLINE UWORD out(UWORD v) { return SWAPW(v); }
    static INLINE UWORD dbg(UWORD v) { return v; }
};

template <>
struct EndianOps<ULONG, RegEndian::SwapMMIO>
{
    static INLINE ULONG in(ULONG v) { return SWAPL(v); }
    static INLINE ULONG out(ULONG v) { return SWAPL(v); }
    static INLINE ULONG dbg(ULONG v) { return v; }
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
        RegLogOps<L>::read(name ? name : "?", EndianOps<T, E>::dbg(v));
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
        RegLogOps<L>::write(name ? name : "?", EndianOps<T, E>::dbg(v));
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
        RegLogOps<L>::read(name ? name : "?", EndianOps<T, E>::dbg(EndianOps<T, E>::in(raw)));
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
        RegLogOps<L>::write(name ? name : "?", EndianOps<T, E>::dbg(EndianOps<T, E>::in(regValue)));
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
        RegLogOps<L>::read(name ? name : "?", v);
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
        RegLogOps<L>::write(name ? name : "?", v);
#endif
        base[byteOff(id) + byteIndex - BaseOff] = v;
    }
};

/* Software shadow for write-only / unsafe-RMW registers. */
template <typename Win, typename RegId, typename T = ULONG>
struct CachedReg
{
    Win *win;
    RegId id;
    T shadow;

    CachedReg() : win(0), id(static_cast<RegId>(0)), shadow(0) {}
    CachedReg(Win *w, RegId i, T initial = 0) : win(w), id(i), shadow(initial) {}

    INLINE void write(T v)
    {
        shadow = v;
        win->template write<T>(id, shadow);
    }

    INLINE void writeMask(T mask, T val)
    {
        shadow = (shadow & ~mask) | (val & mask);
        win->template write<T>(id, shadow);
    }

    INLINE T get() const { return shadow; }
};

#endif /* REG_ACCESS_HPP */
