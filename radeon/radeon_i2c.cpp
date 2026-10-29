#include "radeon_driver.hpp"

using namespace RadeonReg;

constexpr auto RADEON_I2C_RISE_TIME = 40u;
constexpr auto RADEON_I2C_FALL_TIME = 100u;

static void __stdargs set_sda(struct RadeonI2CBus *bus, BOOL high)
{
    radeon_update(bus->board, bus->gpio, RADEON_GPIO_DATA_DRIVE, high ? 0 : RADEON_GPIO_DATA_DRIVE);
    (void)radeon_read(bus->board, bus->gpio);
}

static void __stdargs set_scl(struct RadeonI2CBus *bus, BOOL high)
{
    radeon_update(bus->board, bus->gpio, RADEON_GPIO_CLOCK_DRIVE, high ? 0 : RADEON_GPIO_CLOCK_DRIVE);
    (void)radeon_read(bus->board, bus->gpio);
}

static BOOL __stdargs get_sda(struct RadeonI2CBus *bus)
{
    return !!(radeon_read(bus->board, bus->gpio) & RADEON_GPIO_DATA_INPUT);
}

static BOOL __stdargs get_scl(struct RadeonI2CBus *bus)
{
    return !!(radeon_read(bus->board, bus->gpio) & RADEON_GPIO_CLOCK_INPUT);
}

void radeon_install_i2c(struct BoardInfo *bi)
{
    struct RadeonP96Extension *extension = radeon_p96_extension(bi);
    if (!extension || !RADEON_P96_HAS(extension, i2cBuses))
        return;
    LOCAL_SYSBASE();
    struct RadeonState *state = getCardData(bi)->state;
    struct RadeonI2CBus *buses =
        static_cast<RadeonI2CBus *>(AllocVec(sizeof(*buses) * RADEON_DDC_PORT_COUNT, MEMF_PUBLIC | MEMF_CLEAR));
    if (!buses)
        return;
    static const struct
    {
        RadeonReg::Id reg;
        const char *name;
    } ports[]                        = {{RADEON_GPIO_DVI_DDC, "dvi"},
                                        {RADEON_GPIO_VGA_DDC, "vga"},
                                        {RADEON_GPIO_CRT2_DDC, "crt2"},
                                        {RADEON_GPIO_MONID, "monid"}};
    extension->i2cBuses.mlh_Head     = (struct MinNode *)&extension->i2cBuses.mlh_Tail;
    extension->i2cBuses.mlh_Tail     = NULL;
    extension->i2cBuses.mlh_TailPred = (struct MinNode *)&extension->i2cBuses.mlh_Head;
    for (ULONG i = 0; i < RADEON_DDC_PORT_COUNT; ++i) {
        struct RadeonI2CBus *bus = &buses[i];
        bus->node.ln_Name        = (char *)ports[i].name;
        bus->setSDA              = set_sda;
        bus->getSDA              = get_sda;
        bus->setSCL              = set_scl;
        bus->getSCL              = get_scl;
        bus->riseTime            = RADEON_I2C_RISE_TIME;
        bus->fallTime            = RADEON_I2C_FALL_TIME;
        bus->board               = bi;
        bus->gpio                = ports[i].reg;
        state->savedGPIO[i]      = radeon_read(bi, bus->gpio);
        /* I2C is open drain: outputs remain zero and output-enable drives
         * low. Releasing output-enable lets the connector pull-ups go high.
         */
        radeon_update(
            bi, bus->gpio,
            RADEON_GPIO_DATA_DRIVE | RADEON_GPIO_CLOCK_DRIVE | RADEON_GPIO_DATA_OUTPUT | RADEON_GPIO_CLOCK_OUTPUT, 0);
        AddTail((struct List *)&extension->i2cBuses, &bus->node);
    }
    state->i2cBuses = buses;
}

void radeon_free_i2c(struct BoardInfo *bi)
{
    struct RadeonState *state = getCardData(bi)->state;
    if (!state->i2cBuses)
        return;
    LOCAL_SYSBASE();
    for (ULONG i = 0; i < RADEON_DDC_PORT_COUNT; ++i) {
        struct RadeonI2CBus *bus = &state->i2cBuses[i];
        Remove(&bus->node);
        radeon_write(bi, bus->gpio, state->savedGPIO[i]);
    }
    FreeVec(state->i2cBuses);
    state->i2cBuses = NULL;
}
