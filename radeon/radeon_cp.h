#ifndef RADEON_CP_H
#define RADEON_CP_H

/* Primary command queue. The aperture uses 127 dwords, leaving its last slot unused, even when
 * the internal queue and its read/write pointer fields are wider on R200.
 */
constexpr auto RADEON_CP_APERTURE_WORDS        = 127u;
constexpr auto RADEON_CP_APERTURE_BYTES        = (RADEON_CP_APERTURE_WORDS * sizeof(ULONG));
constexpr auto RADEON_CP_R100_POINTER_BITS     = 8u;
constexpr auto RADEON_CP_R200_POINTER_BITS     = 9u;
constexpr auto RADEON_CP_MICROCODE_WORDS       = 256u;
constexpr auto RADEON_CP_PACKET_REGISTER_SHIFT = 2u;
constexpr auto RADEON_CP_PACKET_COUNT_SHIFT    = 16u;
constexpr auto RADEON_CP_PACKET3_TYPE          = (3UL << 30);
constexpr auto RADEON_CP_PACKET3_OPCODE_SHIFT  = 8u;
constexpr ULONG radeon_packet0(ULONG reg, ULONG count)
{
    return (reg >> RADEON_CP_PACKET_REGISTER_SHIFT) | ((count - 1) << RADEON_CP_PACKET_COUNT_SHIFT);
}
constexpr ULONG radeon_packet3(ULONG opcode, ULONG count)
{
    return RADEON_CP_PACKET3_TYPE | (opcode << RADEON_CP_PACKET3_OPCODE_SHIFT) |
           ((count - 1) << RADEON_CP_PACKET_COUNT_SHIFT);
}

struct RadeonCommandProcessor
{
    BOOL active;
    UWORD pointerBits;
    ULONG capacity;
    ULONG available;
    ULONG apertureOffset;
};

struct BoardInfo;
BOOL radeon_cp_start(struct BoardInfo *bi);
void radeon_cp_stop(struct BoardInfo *bi);
BOOL radeon_cp_submit(struct BoardInfo *bi, const ULONG *words, ULONG count);
BOOL radeon_cp_register(struct BoardInfo *bi, ULONG reg, ULONG value);
void radeon_cp_load_microcode(struct BoardInfo *bi);
#endif
