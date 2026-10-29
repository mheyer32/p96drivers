#include "radeon_driver.hpp"

#include "radeon_cp_microcode.h"

using namespace RadeonReg;

void radeon_cp_load_microcode(struct BoardInfo *bi)
{
    const struct RadeonDevice *device = getCardData(bi)->state->device;
    const struct RadeonMicrocode *program;
    switch (device->engine) {
    case RADEON_ENGINE_R100:
        program = r100_microcode;
        break;
    case RADEON_ENGINE_R200:
        program = r200_microcode;
        break;
    case RADEON_ENGINE_R300:
        program = r300_microcode;
        break;
    default:
        return;
    }
    radeon_write(bi, RADEON_CP_ME_RAM_ADDR, 0);
    for (ULONG i = 0; i < RADEON_CP_MICROCODE_WORDS; ++i) {
        radeon_write(bi, RADEON_CP_ME_RAM_DATAH, program[i].high);
        radeon_write(bi, RADEON_CP_ME_RAM_DATAL, program[i].low);
    }
}

/* Leave one queue entry empty to distinguish a full queue from an empty one.
 * Wrapped pointers are relative to the hardware-reported capacity, which is
 * independent of the size of the CPU aperture used to feed the queue.
 */
static ULONG cp_available(struct BoardInfo *bi)
{
    struct RadeonCommandProcessor *cp = &getCardData(bi)->state->cp;
    ULONG mask                        = (1UL << cp->pointerBits) - 1;
    ULONG read                        = radeon_read(bi, RADEON_CP_CSQ_STAT) & mask;
    ULONG write                       = (radeon_read(bi, RADEON_CP_CSQ_STAT) >> cp->pointerBits) & mask;
    if (write < read)
        return read - write - 1;
    ULONG occupied = write - read;
    return occupied < cp->capacity ? cp->capacity - occupied - 1 : 0;
}

BOOL radeon_cp_start(struct BoardInfo *bi)
{
    struct RadeonState *state         = getCardData(bi)->state;
    struct RadeonCommandProcessor *cp = &state->cp;
    if (cp->active)
        return !state->engineFault;
    if (!state->device || !state->device->engine || state->engineFault || !radeon_wait_idle(bi))
        return FALSE;
    cp->pointerBits =
        state->device->engine == RADEON_ENGINE_R100 ? RADEON_CP_R100_POINTER_BITS : RADEON_CP_R200_POINTER_BITS;
    radeon_cp_load_microcode(bi);
    /* Use the local framebuffer without AGP translation or host-memory
     * pointer writeback. Vertex coordinates are supplied by the CPU.
     */
    radeon_write(bi, RADEON_MC_AGP_LOCATION, 0);
    radeon_write(bi, RADEON_AGP_BASE, 0);
    radeon_write(bi, RADEON_AGP_COMMAND, 0);
    radeon_write(bi, RADEON_AIC_CNTL, 0);
    radeon_write(bi, RADEON_CACHE_CNTL,
                 state->device->engine == RADEON_ENGINE_R100 ? RADEON_CACHE_R100_MODE : RADEON_CACHE_R200_MODE);
    radeon_update(bi, RADEON_CP_BUS_CNTL, RADEON_CP_BUS_ACCESS_MASK, RADEON_CP_BUS_ACCESS_ENABLE);
    radeon_update(bi, RADEON_SE_CNTL_STATUS, 0, RADEON_TCL_BYPASS);
    if (state->device->engine != RADEON_ENGINE_R100) {
        radeon_write(bi, R200_SE_VAP_CNTL, r200MaxVertices(9));
        radeon_write(bi, R200_SE_VTE_CNTL, R200_VTX_XY_FMT | R200_VTX_Z_FMT);
        radeon_write(bi, R200_RB3D_DEPTHXY_OFFSET, 0);
        radeon_write(bi, R200_SE_TCL_OUTPUT_VTX_FMT_0, 0);
        radeon_write(bi, R200_SE_TCL_OUTPUT_VTX_FMT_1, 0);
    }
    if (state->device->engine != RADEON_ENGINE_R100) {
        radeon_write(bi, RADEON_RBBM_SOFT_RESET, RADEON_CP_R200_RESET_MASK);
        radeon_write(bi, RADEON_RBBM_SOFT_RESET, 0);
    }
    radeon_write(bi, RADEON_CP_RB_CNTL, RADEON_CP_NO_RPTR_WRITEBACK);
    radeon_write(bi, RADEON_CP_ME_CNTL, RADEON_CP_ME_RUN);
    radeon_write(bi, RADEON_CP_CSQ_CNTL, RADEON_CP_PRIMARY_PIO);
    /* Surface seven belongs to the hardware cursor. Linear rendering uses
     * the other surfaces without tiling or per-surface byte conversion.
     */
    for (ULONG surface = 0; surface < RADEON_CP_SURFACE_COUNT; ++surface)
        for (ULONG word = 0; word < RADEON_SURFACE_DESCRIPTOR_WORDS; ++word)
            radeon_write(bi,
                         static_cast<RadeonReg::Id>(RADEON_SURFACE0_LOWER_BOUND +
                                                    surface * RADEON_SURFACE_REGISTER_STRIDE + word * sizeof(ULONG)),
                         0);
    cp->capacity = radeon_read(bi, RADEON_CP_CSQ_CNTL) & ((1UL << cp->pointerBits) - 1);
    if (cp->capacity < 2) {
        radeon_write(bi, RADEON_CP_CSQ_CNTL, 0);
        return FALSE;
    }
    cp->available      = cp_available(bi);
    cp->apertureOffset = 0;
    cp->active         = TRUE;
    return TRUE;
}

BOOL radeon_cp_submit(struct BoardInfo *bi, const ULONG *words, ULONG count)
{
    struct RadeonState *state         = getCardData(bi)->state;
    struct RadeonCommandProcessor *cp = &state->cp;
    if (!count)
        return TRUE;
    if (!words || !cp->active || state->engineFault)
        return FALSE;
    while (count) {
        if (!cp->available) {
            ULONG poll;
            for (poll = 0; poll < RADEON_POLL_LIMIT; ++poll) {
                cp->available = cp_available(bi);
                if (cp->available)
                    break;
            }
            if (poll == RADEON_POLL_LIMIT) {
                state->engineFault = TRUE;
                return FALSE;
            }
        }
        ULONG batch = count < cp->available ? count : cp->available;
        cp->available -= batch;
        count -= batch;
        while (batch--) {
            radeon_write(bi, static_cast<RadeonReg::Id>(RADEON_CP_CSQ_APER_PRIMARY + cp->apertureOffset), *words++);
            cp->apertureOffset += sizeof(ULONG);
            if (cp->apertureOffset >= RADEON_CP_APERTURE_BYTES)
                cp->apertureOffset = 0;
        }
        if (!cp->available)
            cp->available = cp_available(bi);
    }
    return TRUE;
}

BOOL radeon_cp_register(struct BoardInfo *bi, ULONG reg, ULONG value)
{
    ULONG packet[] = {radeon_packet0(reg, 1), value};
    return radeon_cp_submit(bi, packet, ARRAY_SIZE(packet));
}

void radeon_cp_stop(struct BoardInfo *bi)
{
    struct RadeonCommandProcessor *cp = &getCardData(bi)->state->cp;
    if (!cp->active)
        return;
    (void)radeon_wait_idle(bi);
    radeon_write(bi, RADEON_CP_CSQ_CNTL, 0);
    cp->active    = FALSE;
    cp->available = 0;
}
