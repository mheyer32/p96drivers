#ifndef P96_DRIVER_HPP
#define P96_DRIVER_HPP

#include "common.h"

#include <type_traits>

/*
 * Method-only view of BoardInfo. No virtuals, no extra data — sizeof must match
 * BoardInfo so P96-allocated boards can be static_cast to P96Driver*.
 *
 * Runtime polymorphism stays on BoardInfo’s function-pointer hooks (InitChip /
 * P96_HOOK). A C++ vtable would add a vptr (breaking the overlay) and a second
 * indirection on top of bi->Hook; do not add virtual methods here.
 */
class P96Driver : public BoardInfo
{
   public:
    volatile UBYTE *ioBase() const { return RegisterBase; }
    volatile UBYTE *mmioBase() const { return MemoryIOBase; }
};

static_assert(sizeof(P96Driver) == sizeof(BoardInfo), "P96Driver must not grow BoardInfo");
static_assert(std::is_standard_layout<P96Driver>::value, "P96Driver must be standard layout");

static INLINE P96Driver *asDriver(BoardInfo *bi)
{
    return static_cast<P96Driver *>(bi);
}

static INLINE const P96Driver *asDriver(const BoardInfo *bi)
{
    return static_cast<const P96Driver *>(bi);
}

#endif /* P96_DRIVER_HPP */
