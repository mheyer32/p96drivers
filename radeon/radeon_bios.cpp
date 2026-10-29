#include "radeon_driver.hpp"

using namespace RadeonReg;

/* COMBIOS tables contain little-endian values even on a big-endian host.
 * Every operand is checked before use; a truncated script must not turn
 * into a write to an arbitrary register or an unbounded hardware wait.
 */
constexpr auto BIOS_REGISTER_TABLE       = 0x0c;
constexpr auto BIOS_PANEL_TABLE          = 0x34;
constexpr auto BIOS_PLL_INIT_TABLE       = 0x46;
constexpr auto BIOS_MEMORY_TABLE         = 0x48;
constexpr auto BIOS_MEMORY_REG_TABLE     = 0x4e;
constexpr auto BIOS_PLL_FINAL_TABLE      = 0x52;
constexpr auto BIOS_EXTENDED_TABLE       = 0x5e;
constexpr auto BIOS_EXTENDED_VERSION     = 7;
constexpr auto BIOS_EXTENDED_FIRST       = 5;
constexpr auto BIOS_EXTENDED_SECOND      = 3;
constexpr auto BIOS_REGISTER_MASK        = 0x1fff;
constexpr auto BIOS_REGISTER_OP_SHIFT    = 13;
constexpr auto BIOS_PLL_OP_SHIFT         = 6;
constexpr auto BIOS_PLL_ADDRESS_MASK     = 0x3f;
constexpr auto BIOS_PLL_BYTE_BITS        = 8;
constexpr auto BIOS_POLL_PLL             = 8;
constexpr auto BIOS_POLL_MEMORY          = 9;
constexpr auto BIOS_PLL_POLL_COUNT       = 1000;
constexpr auto BIOS_SHORT_DELAY_US       = 150;
constexpr auto BIOS_LONG_DELAY_US        = 15000;
constexpr auto BIOS_MEMORY_END           = 0xff;
constexpr auto BIOS_MEMORY_WAIT          = 0x0f;
constexpr auto BIOS_MEMORY_MODE_SHIFT    = 24;
constexpr auto BIOS_MEMORY_MODE_MASK     = 0xff000000UL;
constexpr auto BIOS_MEMORY_SIZE_SHIFT    = 20;
constexpr auto BIOS_MEMORY_PROBE_PATTERN = 0xaa55aa55UL;
constexpr auto BIOS_MEMORY_RESET_SKIP    = 3;
constexpr auto BIOS_PANEL_VERSION        = 3;
constexpr auto BIOS_PANEL_ENABLED        = 5;
constexpr auto BIOS_PANEL_PLL            = 0x12;

enum BIOSRegisterOperation
{
    BIOS_WRITE_REGISTER,
    BIOS_WRITE_REGISTER_ALT,
    BIOS_MASK_REGISTER,
    BIOS_MASK_REGISTER_ALT,
    BIOS_DELAY,
    BIOS_POLL
};
enum BIOSPLLOperation
{
    BIOS_PLL_WRITE,
    BIOS_PLL_MASK_BYTE,
    BIOS_PLL_SPECIAL,
    BIOS_PLL_SPECIAL_ALT
};
enum BIOSPLLSpecial
{
    BIOS_PLL_NOP,
    BIOS_PLL_SHORT_DELAY,
    BIOS_PLL_LONG_DELAY,
    BIOS_PLL_WAIT_READY,
    BIOS_PLL_WAIT_DLL,
    BIOS_PLL_RELEASE_RESET
};

struct RadeonROM
{
    const UBYTE *bytes;
    ULONG size;
};

static BOOL contains(const struct RadeonROM *rom, ULONG offset, ULONG size)
{
    return offset <= rom->size && size <= rom->size - offset;
}

static ULONG word(const struct RadeonROM *rom, ULONG offset)
{
    return (ULONG)rom->bytes[offset] | ((ULONG)rom->bytes[offset + 1] << 8);
}

static ULONG longword(const struct RadeonROM *rom, ULONG offset)
{
    return word(rom, offset) | (word(rom, offset + 2) << 16);
}

static BOOL poll_register(struct BoardInfo *bi, ULONG reg, ULONG mask, ULONG wanted, ULONG count, BOOL pll)
{
    while (count--) {
        ULONG value = pll ? radeon_pll_read(bi, reg) : radeon_read(bi, static_cast<RadeonReg::Id>(reg));
        if ((value & mask) == wanted)
            return TRUE;
    }
    return FALSE;
}

static BOOL register_table(struct BoardInfo *bi, const struct RadeonROM *rom, ULONG cursor)
{
    if (!cursor)
        return TRUE;
    while (contains(rom, cursor, sizeof(UWORD))) {
        ULONG instruction = word(rom, cursor);
        ULONG reg         = instruction & BIOS_REGISTER_MASK;
        ULONG operation   = instruction >> BIOS_REGISTER_OP_SHIFT;
        cursor += sizeof(UWORD);
        if (!instruction)
            return TRUE;
        switch (operation) {
        case BIOS_WRITE_REGISTER:
        case BIOS_WRITE_REGISTER_ALT:
            if (!contains(rom, cursor, sizeof(ULONG)) || (reg & 3))
                return FALSE;
            radeon_write(bi, static_cast<RadeonReg::Id>(reg), longword(rom, cursor));
            cursor += sizeof(ULONG);
            break;
        case BIOS_MASK_REGISTER:
        case BIOS_MASK_REGISTER_ALT:
            if (!contains(rom, cursor, 2 * sizeof(ULONG)) || (reg & 3))
                return FALSE;
            radeon_write(bi, static_cast<RadeonReg::Id>(reg),
                         (radeon_read(bi, static_cast<RadeonReg::Id>(reg)) & longword(rom, cursor)) |
                             longword(rom, cursor + sizeof(ULONG)));
            cursor += 2 * sizeof(ULONG);
            break;
        case BIOS_DELAY:
            if (!contains(rom, cursor, sizeof(UWORD)))
                return FALSE;
            delayMicroSeconds(word(rom, cursor));
            cursor += sizeof(UWORD);
            break;
        case BIOS_POLL: {
            if (!contains(rom, cursor, sizeof(UWORD)))
                return FALSE;
            ULONG count = word(rom, cursor);
            cursor += sizeof(UWORD);
            if (reg == BIOS_POLL_PLL && !poll_register(bi, RADEON_PLL_PWRMGT_CNTL, RADEON_MC_BUSY, 0, count, TRUE))
                return FALSE;
            if (reg == BIOS_POLL_MEMORY &&
                !poll_register(bi, RADEON_MC_STATUS, RADEON_MC_IDLE, RADEON_MC_IDLE, count, FALSE))
                return FALSE;
            break;
        }
        default:
            /* Reserved opcodes have no operands in the COMBIOS format. */
            break;
        }
    }
    return FALSE;
}

static BOOL pll_table(struct BoardInfo *bi, const struct RadeonROM *rom, ULONG cursor)
{
    if (!cursor)
        return TRUE;
    while (contains(rom, cursor, 1)) {
        ULONG instruction = rom->bytes[cursor++];
        ULONG index       = instruction & BIOS_PLL_ADDRESS_MASK;
        if (!instruction)
            return TRUE;
        switch (instruction >> BIOS_PLL_OP_SHIFT) {
        case BIOS_PLL_WRITE:
            if (!contains(rom, cursor, sizeof(ULONG)))
                return FALSE;
            radeon_pll_write(bi, index, longword(rom, cursor));
            cursor += sizeof(ULONG);
            break;
        case BIOS_PLL_MASK_BYTE: {
            if (!contains(rom, cursor, 3) || rom->bytes[cursor] >= sizeof(ULONG))
                return FALSE;
            ULONG shift = rom->bytes[cursor] * BIOS_PLL_BYTE_BITS;
            ULONG mask  = ~(UINT8_MAX << shift) | ((ULONG)rom->bytes[cursor + 1] << shift);
            ULONG value = (ULONG)rom->bytes[cursor + 2] << shift;
            radeon_pll_write(bi, index, (radeon_pll_read(bi, index) & mask) | value);
            cursor += 3;
            break;
        }
        default:
            switch (index) {
            case BIOS_PLL_SHORT_DELAY:
                delayMicroSeconds(BIOS_SHORT_DELAY_US);
                break;
            case BIOS_PLL_LONG_DELAY:
                delayMicroSeconds(BIOS_LONG_DELAY_US);
                break;
            case BIOS_PLL_WAIT_READY:
                if (!poll_register(bi, RADEON_PLL_PWRMGT_CNTL, RADEON_MC_BUSY, 0, BIOS_PLL_POLL_COUNT, TRUE))
                    return FALSE;
                break;
            case BIOS_PLL_WAIT_DLL:
                if (!poll_register(bi, RADEON_PLL_PWRMGT_CNTL, RADEON_DLL_READY, RADEON_DLL_READY, BIOS_PLL_POLL_COUNT,
                                   TRUE))
                    return FALSE;
                break;
            case BIOS_PLL_RELEASE_RESET: {
                ULONG control = radeon_pll_read(bi, RADEON_PLL_PWRMGT_CNTL);
                if (control & RADEON_MC_RESET) {
                    radeon_pll_write(bi, RADEON_PLL_PWRMGT_CNTL, control & ~RADEON_MC_RESET);
                    delayMicroSeconds(BIOS_LONG_DELAY_US);
                }
                break;
            }
            default:
                break;
            }
        }
    }
    return FALSE;
}

static BOOL memory_reset(struct BoardInfo *bi, const struct RadeonROM *rom, ULONG cursor)
{
    while (contains(rom, cursor, 1)) {
        ULONG command = rom->bytes[cursor++];
        if (command == BIOS_MEMORY_END)
            return TRUE;
        if (command == BIOS_MEMORY_WAIT) {
            if (!poll_register(bi, RADEON_MC_STATUS, RADEON_MC_IDLE, RADEON_MC_IDLE, RADEON_POLL_LIMIT, FALSE))
                return FALSE;
        } else {
            if (!contains(rom, cursor, sizeof(UWORD)))
                return FALSE;
            radeon_update(bi, RADEON_MEM_SDRAM_MODE_REG, 0, command << BIOS_MEMORY_MODE_SHIFT);
            ULONG mode = word(rom, cursor);
            radeon_update(bi, RADEON_MEM_SDRAM_MODE_REG, 0,
                          ((mode << 16) & BIOS_MEMORY_MODE_MASK) | (mode & UINT8_MAX));
            cursor += sizeof(UWORD);
        }
    }
    return FALSE;
}

static BOOL memory_size(struct BoardInfo *bi, const struct RadeonROM *rom, ULONG cursor)
{
    struct RadeonState *state = getCardData(bi)->state;
    while (contains(rom, cursor, 2)) {
        ULONG halfSize = (ULONG)rom->bytes[cursor] << BIOS_MEMORY_SIZE_SHIFT;
        cursor += 2;
        if (!halfSize)
            return TRUE;
        /* A BAR may expose less than the installed RAM. Never probe beyond
         * the CPU mapping, even if the firmware lists larger candidates.
         */
        if (halfSize > state->framebufferSize / 2)
            continue;
        volatile ULONG *lower = (volatile ULONG *)((UBYTE *)bi->MemorySpaceBase + halfSize - sizeof(ULONG));
        volatile ULONG *upper = (volatile ULONG *)((UBYTE *)bi->MemorySpaceBase + 2 * halfSize - sizeof(ULONG));
        ULONG savedLower = *lower, savedUpper = *upper;
        radeon_write(bi, RADEON_CONFIG_MEMSIZE, halfSize);
        *lower       = 0;
        *upper       = BIOS_MEMORY_PROBE_PATTERN;
        BOOL aliases = *lower == BIOS_MEMORY_PROBE_PATTERN;
        *upper       = savedUpper;
        *lower       = savedLower;
        if (!aliases) {
            bi->MemorySize = 2 * halfSize;
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL panel_init(struct BoardInfo *bi, const struct RadeonROM *rom, ULONG panel)
{
    if (!panel || !radeon_read(bi, RADEON_FP_GEN_CNTL))
        return TRUE;
    if (!contains(rom, panel, BIOS_PANEL_PLL + sizeof(ULONG)))
        return FALSE;
    if (rom->bytes[panel] != BIOS_PANEL_VERSION || !rom->bytes[panel + BIOS_PANEL_ENABLED])
        return TRUE;
    radeon_update(bi, RADEON_FP_HORZ_STRETCH, ~(RADEON_H_DISPLAY_MASK << RADEON_FIELD_HIGH_SHIFT), 0);
    radeon_update(bi, RADEON_FP_VERT_STRETCH, ~(RADEON_V_SYNC_START_MASK << RADEON_FP_V_SIZE_SHIFT), 0);
    radeon_write(bi, RADEON_TMDS_PLL_CNTL, longword(rom, panel + BIOS_PANEL_PLL));
    radeon_update(bi, RADEON_TMDS_TRANSMITTER_CNTL, RADEON_TMDS_PLL_ENABLE | RADEON_TMDS_PLL_RESET, 0);
    return TRUE;
}

BOOL radeon_init_rom(struct BoardInfo *bi, const UBYTE *bytes, ULONG size)
{
    struct RadeonROM rom = {bytes, size};
    if (!bytes || !contains(&rom, 0, RADEON_ROM_HEADER_POINTER + sizeof(UWORD)) ||
        word(&rom, 0) != RADEON_ROM_SIGNATURE)
        return radeon_read(bi, RADEON_CONFIG_MEMSIZE) != 0;
    ULONG header = word(&rom, RADEON_ROM_HEADER_POINTER);
    if (!header || !contains(&rom, header, BIOS_EXTENDED_TABLE + sizeof(UWORD)))
        return FALSE;
    /* These chips use COMBIOS. An ATOM command table is a different bytecode
     * language and must never be interpreted as register-write commands.
     */
    if (bytes[header + 4] == 'A' && bytes[header + 5] == 'T' && bytes[header + 6] == 'O' && bytes[header + 7] == 'M')
        return FALSE;
    if (!register_table(bi, &rom, word(&rom, header + BIOS_REGISTER_TABLE)) ||
        !pll_table(bi, &rom, word(&rom, header + BIOS_PLL_INIT_TABLE)) ||
        !register_table(bi, &rom, word(&rom, header + BIOS_MEMORY_REG_TABLE)))
        return FALSE;
    ULONG memory = word(&rom, header + BIOS_MEMORY_TABLE);
    if (memory) {
        ULONG end = memory;
        while (contains(&rom, end, 1) && bytes[end])
            ++end;
        if (!contains(&rom, end, BIOS_MEMORY_RESET_SKIP) || !memory_reset(bi, &rom, end + BIOS_MEMORY_RESET_SKIP) ||
            !memory_size(bi, &rom, memory))
            return FALSE;
    }
    if (bytes[header] > BIOS_EXTENDED_VERSION) {
        ULONG extended = word(&rom, header + BIOS_EXTENDED_TABLE);
        if (extended) {
            if (!contains(&rom, extended, BIOS_EXTENDED_FIRST + sizeof(UWORD)) ||
                !register_table(bi, &rom, word(&rom, extended + BIOS_EXTENDED_FIRST)) ||
                !register_table(bi, &rom, word(&rom, extended + BIOS_EXTENDED_SECOND)))
                return FALSE;
        }
    }
    return pll_table(bi, &rom, word(&rom, header + BIOS_PLL_FINAL_TABLE)) &&
           panel_init(bi, &rom, word(&rom, header + BIOS_PANEL_TABLE));
}
