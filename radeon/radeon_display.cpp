#include "radeon_driver.hpp"

#define __NOLIBBASE__
#include <proto/exec.h>

using namespace RadeonReg;

constexpr auto RADEON_DAC_INDEXED              = 2u;
constexpr auto RADEON_DAC_RGB15                = 3u;
constexpr auto RADEON_DAC_RGB16                = 4u;
constexpr auto RADEON_DAC_RGB24                = 5u;
constexpr auto RADEON_DAC_RGB32                = 6u;
constexpr auto RADEON_CLOCK_DIVIDER_TERMINATOR = 0u;
constexpr auto RADEON_PLL_UPDATE_POLLS         = 10000u;

/* Nesting keeps DAC and timing changes blanked until the outer operation
 * completes. Releasing an already unblanked display has no effect.
 */
static void blank_display(struct BoardInfo *bi, BOOL blank)
{
    struct ChipData *chip = getChipData(bi);
    if (blank) {
        if (!chip->blankDepth++)
            radeon_update(bi, RADEON_CRTC_EXT_CNTL, 0, RADEON_CRTC_BLANK_MASK);
    } else if (chip->blankDepth && !--chip->blankDepth) {
        radeon_update(bi, RADEON_CRTC_EXT_CNTL, RADEON_CRTC_BLANK_MASK, 0);
    }
}

BOOL ASM RadeonDriver::setSwitch(__REGD0(BOOL on))
{
    BoardInfo *bi = this;
    /* The Radeon has no switch for the Amiga chipset's video output. */
    (void)bi;
    (void)on;
    return FALSE;
}

static BOOL ASM RadeonSetSwitch(__REGA0(struct BoardInfo *bi), __REGD0(BOOL on))
{
    return asRadeon(bi)->setSwitch(on);
}

void ASM RadeonDriver::setColorArray(__REGD0(UWORD start), __REGD1(UWORD count))
{
    BoardInfo *bi = this;
    ULONG end     = minu((ULONG)start + count, RADEON_PALETTE_ENTRIES);
    for (ULONG index = start; index < end; ++index) {
        const struct CLUTEntry *entry = &bi->CLUT[index];
        ULONG color                   = ((ULONG)entry->Red << RADEON_COLOR_RED_SHIFT) |
                                        ((ULONG)entry->Green << RADEON_COLOR_GREEN_SHIFT) | entry->Blue;
        radeon_write(bi, RADEON_PALETTE_INDEX, (index << RADEON_FIELD_HIGH_SHIFT) | index);
        radeon_write(bi, RADEON_PALETTE_DATA, color);
        (void)radeon_read(bi, RADEON_PALETTE_DATA);
    }
}

static void ASM RadeonSetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD start), __REGD1(UWORD count))
{
    asRadeon(bi)->setColorArray(start, count);
}

void ASM RadeonDriver::setDAC(__REGD0(UWORD border), __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi     = this;
    RGBFTYPE format   = AS_RGBF(formatValue);
    ULONG pixelFormat = 0;
    ULONG directColor = RADEON_FP_DIRECT_COLOR;
    (void)border;
    if (format != RGBFB_CLUT) {
        /* True-colour modes use an identity ramp through the palette. */
        for (ULONG index = 0; index < RADEON_PALETTE_ENTRIES; ++index) {
            ULONG redBlue = (index << RADEON_COLOR_RED_SHIFT) | index;
            radeon_write(bi, RADEON_PALETTE_INDEX, redBlue);
            radeon_write(bi, RADEON_PALETTE_DATA, redBlue | (index << RADEON_COLOR_GREEN_SHIFT));
            (void)radeon_read(bi, RADEON_PALETTE_DATA);
        }
    }
    switch (format) {
    case RGBFB_CLUT:
        pixelFormat = RADEON_DAC_INDEXED;
        break;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        pixelFormat = RADEON_DAC_RGB24;
        break;
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G5B5:
    case RGBFB_B5G5R5PC:
        pixelFormat = RADEON_DAC_RGB15;
        directColor = 0;
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G6B5:
    case RGBFB_B5G6R5PC:
        pixelFormat = RADEON_DAC_RGB16;
        directColor = 0;
        break;
    case RGBFB_A8R8G8B8:
    case RGBFB_A8B8G8R8:
    case RGBFB_R8G8B8A8:
    case RGBFB_B8G8R8A8:
        pixelFormat = RADEON_DAC_RGB32;
        break;
    default:
        break;
    }
    radeon_update(bi, RADEON_DAC_CNTL, RADEON_DAC_MASK_ALL, 0);
    radeon_update(bi, RADEON_FP_GEN_CNTL, RADEON_FP_DIRECT_COLOR, directColor);
    ULONG control = (radeon_read(bi, RADEON_CRTC_GEN_CNTL) & ~RADEON_CRTC_PIXEL_FORMAT_MASK) |
                    (pixelFormat << RADEON_CRTC_PIXEL_FORMAT_SHIFT);
    blank_display(bi, TRUE);
    if (radeon_read(bi, RADEON_CRTC_GEN_CNTL) != control)
        radeon_write(bi, RADEON_CRTC_GEN_CNTL, control);
    blank_display(bi, FALSE);
}

static void ASM RadeonSetDAC(__REGA0(struct BoardInfo *bi), __REGD0(UWORD border), __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->setDAC(border, format);
}

void ASM RadeonDriver::setGC(__REGA1(struct ModeInfo *mode), __REGD0(BOOL border))
{
    BoardInfo *bi    = this;
    ULONG scan       = (mode->Flags & GMF_DOUBLESCAN) ? 2u : 1u;
    ULONG horizontal = ((ULONG)(mode->Width / RADEON_H_CHAR_PIXELS) - 1u) << RADEON_FIELD_HIGH_SHIFT;
    ULONG htotal     = ((mode->HorTotal / RADEON_H_CHAR_PIXELS - 1u) & RADEON_H_TOTAL_MASK) |
                       (horizontal & (RADEON_H_DISPLAY_MASK << RADEON_FIELD_HIGH_SHIFT));
    ULONG hwidth     = minu(mode->HorSyncSize / RADEON_H_CHAR_PIXELS, RADEON_H_SYNC_WIDTH_MAX);
    ULONG hsync      = ((hwidth ? hwidth : 1u) << RADEON_FIELD_HIGH_SHIFT) | ((ULONG)mode->Width + mode->HorSyncStart);
    ULONG vtotal =
        ((mode->VerTotal * scan - 1u) & UINT16_MAX) | ((mode->Height * scan - 1u) << RADEON_FIELD_HIGH_SHIFT);
    ULONG vsync = ((mode->Height * scan + mode->VerSyncStart) & RADEON_V_SYNC_START_MASK) |
                  ((ULONG)mode->VerSyncSize << RADEON_FIELD_HIGH_SHIFT);
    (void)border;
    if (mode->Flags & GMF_HPOLARITY)
        hsync |= RADEON_SYNC_POLARITY;
    if (mode->Flags & GMF_VPOLARITY)
        vsync |= RADEON_SYNC_POLARITY;
    blank_display(bi, TRUE);
    radeon_update(bi, RADEON_CRTC_GEN_CNTL, RADEON_CRTC_SCAN_MASK,
                  ((mode->Flags & GMF_DOUBLESCAN) >> GMB_DOUBLESCAN) | (mode->Flags & GMF_INTERLACE));
    radeon_write(bi, RADEON_CRTC_H_TOTAL_DISP, htotal);
    radeon_write(bi, RADEON_FP_H_TOTAL_DISP, htotal);
    radeon_write(bi, RADEON_CRTC_H_SYNC_STRT_WID, hsync);
    radeon_write(bi, RADEON_FP_H_SYNC_STRT_WID, hsync);
    radeon_write(bi, RADEON_CRTC_V_TOTAL_DISP, vtotal);
    radeon_write(bi, RADEON_FP_V_TOTAL_DISP, vtotal);
    radeon_write(bi, RADEON_CRTC_V_SYNC_STRT_WID, vsync);
    radeon_write(bi, RADEON_FP_V_SYNC_STRT_WID, vsync);
    radeon_write(bi, RADEON_FP_HORZ_STRETCH, horizontal);
    radeon_write(bi, RADEON_FP_VERT_STRETCH, ((ULONG)mode->Height - 1u) << RADEON_FP_V_SIZE_SHIFT);
    bi->ModeInfo = mode;
    bi->Border   = TRUE;
    blank_display(bi, FALSE);
}

static void ASM RadeonSetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mode), __REGD0(BOOL border))
{
    asRadeon(bi)->setGC(mode, border);
}

void ASM RadeonDriver::setPanning(__REGA1(UBYTE *memory), __REGD0(UWORD width), __REGD3(UWORD height), __REGD1(WORD x),
                                  __REGD2(WORD y), __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi   = this;
    RGBFTYPE format = AS_RGBF(formatValue);
    ULONG bytes     = radeon_bytes_per_pixel(format);
    UWORD pitch     = radeon_calculate_bytes_per_row(width, format);
    (void)height;
    if (!bytes)
        return;
    bi->XOffset = x;
    bi->YOffset = y;
    radeon_write(bi, RADEON_CRTC_PITCH, pitch / RADEON_H_CHAR_PIXELS / bytes);
    radeon_write(bi, RADEON_CRTC_OFFSET,
                 (ULONG)(memory - (UBYTE *)bi->MemorySpaceBase) + (LONG)y * pitch + (LONG)x * bytes);
    radeon_refresh_overlay(bi);
}

static void ASM RadeonSetPanning(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *memory), __REGD0(UWORD width),
                                 __REGD3(UWORD height), __REGD1(WORD x), __REGD2(WORD y), __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->setPanning(memory, width, height, x, y, format);
}

APTR ASM RadeonDriver::calculateMemory(__REGA1(APTR memory), __REGD0(struct RenderInfo *render),
                                       __REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi   = this;
    RGBFTYPE format = AS_RGBF(formatValue);
    (void)bi;
    (void)render;
    (void)format;
    return memory;
}

static APTR ASM RadeonCalculateMemory(__REGA0(struct BoardInfo *bi), __REGA1(APTR memory),
                                      __REGD0(struct RenderInfo *render), __REGD7(RGBFTYPE_REG format))
{
    return asRadeon(bi)->calculateMemory(memory, render, format);
}

ULONG ASM RadeonDriver::getCompatibleFormats(__REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi   = this;
    RGBFTYPE format = AS_RGBF(formatValue);
    (void)bi;
    (void)format;
    return ~(ULONG)RGBFF_PLANAR;
}

static ULONG ASM RadeonGetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE_REG format))
{
    return asRadeon(bi)->getCompatibleFormats(format);
}

BOOL ASM RadeonDriver::setDisplay(__REGD0(BOOL on))
{
    BoardInfo *bi = this;
    radeon_write(bi, RADEON_DAC_EXT_CNTL, on ? 0 : RADEON_DAC_FORCE_BLANK);
    return on;
}

static BOOL ASM RadeonSetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL on))
{
    return asRadeon(bi)->setDisplay(on);
}

void ASM RadeonDriver::setMemoryMode(__REGD7(RGBFTYPE_REG formatValue))
{
    BoardInfo *bi   = this;
    RGBFTYPE format = AS_RGBF(formatValue);
    ULONG swap      = 0;
    switch (format) {
    case RGBFB_A8R8G8B8:
    case RGBFB_R8G8B8A8:
    case RGBFB_YUV422:
        swap = RADEON_SURFACE_SWAP_32;
        break;
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        swap = RADEON_SURFACE_SWAP_16;
        break;
    default:
        break;
    }
    radeon_write(bi, RADEON_SURFACE_CNTL, swap);
}

static void ASM RadeonSetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE_REG format))
{
    asRadeon(bi)->setMemoryMode(format);
}

void ASM RadeonDriver::setDPMSLevel(__REGD0(ULONG level))
{
    BoardInfo *bi = this;
    radeon_update(bi, RADEON_DISP_PWR_MAN, RADEON_FP_POWER_MASK, level << RADEON_FP_POWER_SHIFT);
    radeon_update(bi, RADEON_FP_GEN_CNTL, RADEON_FP_PANEL_ENABLE, level == DPMS_ON ? RADEON_FP_PANEL_ENABLE : 0);
}

static void ASM RadeonSetDPMSLevel(__REGA0(struct BoardInfo *bi), __REGD0(ULONG level))
{
    asRadeon(bi)->setDPMSLevel(level);
}

void ASM RadeonDriver::waitVerticalSync(__REGD0(BOOL toggle))
{
    BoardInfo *bi = this;
    (void)toggle;
    radeon_write(bi, RADEON_GEN_INT_STATUS, RADEON_VSYNC_INTERRUPT);
    for (ULONG poll = 0; poll < RADEON_POLL_LIMIT; ++poll)
        if (radeon_read(bi, RADEON_GEN_INT_STATUS) & RADEON_VSYNC_INTERRUPT)
            break;
}

static void ASM RadeonWaitVerticalSync(__REGA0(struct BoardInfo *bi), __REGD0(BOOL toggle))
{
    asRadeon(bi)->waitVerticalSync(toggle);
}

BOOL ASM RadeonDriver::getVSyncState(__REGD0(BOOL toggle))
{
    BoardInfo *bi = this;
    /* P96 uses the VBlank interrupt or WaitVerticalSync on this card.
     * This callback does not report an instantaneous sync state.
     */
    (void)bi;
    (void)toggle;
    return FALSE;
}

static BOOL ASM RadeonGetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL toggle))
{
    return asRadeon(bi)->getVSyncState(toggle);
}

ULONG radeon_pll_read(struct BoardInfo *bi, ULONG index)
{
    radeon_update(bi, RADEON_CLOCK_INDEX, RADEON_PLL_INDEX_MASK, index);
    return radeon_read(bi, RADEON_CLOCK_DATA);
}

void radeon_pll_write(struct BoardInfo *bi, ULONG index, ULONG value)
{
    radeon_update(bi, RADEON_CLOCK_INDEX, RADEON_PLL_INDEX_MASK, index | RADEON_PLL_WRITE_ENABLE);
    radeon_write(bi, RADEON_CLOCK_DATA, value);
}

void ASM RadeonDriver::setClock()
{
    BoardInfo *bi = this;
    /* Hardware encodings are not logarithms: the PLL also supports odd
     * post-divisors. Select the first divisor whose VCO is in range.
     */
    static const struct
    {
        ULONG divisor, encoding;
    } divisors[]          = {{1, 0}, {2, 1}, {4, 2}, {8, 3}, {3, 4}, {16, 5}, {6, 6}, {12, 7}};
    struct ChipData *chip = getChipData(bi);
    ULONG requested       = bi->ModeInfo->PixelClock / RADEON_CLOCK_UNIT_HZ;
    ULONG vco = 0, encoding = 0;
    if (!chip->referenceClock)
        return;
    for (ULONG i = 0; i < ARRAY_SIZE(divisors); ++i) {
        vco = requested * divisors[i].divisor;
        if (vco >= chip->minPLL && vco <= chip->maxPLL) {
            encoding = divisors[i].encoding;
            break;
        }
    }
    ULONG feedback = (vco * chip->referenceDivider + chip->referenceClock / 2u) / chip->referenceClock;
    radeon_pll_write(bi, RADEON_VCLK_ECP_CNTL, radeon_pll_read(bi, RADEON_VCLK_ECP_CNTL) & ~RADEON_PLL_SOURCE_MASK);
    radeon_pll_write(bi, RADEON_PPLL_CNTL,
                     radeon_pll_read(bi, RADEON_PPLL_CNTL) | RADEON_PPLL_RESET | RADEON_PPLL_ATOMIC_ENABLE);
    radeon_update(bi, RADEON_CLOCK_INDEX, 0, RADEON_PLL_DIV_SEL);
    radeon_pll_write(bi, RADEON_PPLL_REF_DIV,
                     (radeon_pll_read(bi, RADEON_PPLL_REF_DIV) & ~RADEON_PPLL_REF_MASK) | chip->referenceDivider);
    radeon_pll_write(bi, RADEON_PPLL_DIV_3,
                     (radeon_pll_read(bi, RADEON_PPLL_DIV_3) & ~(RADEON_PPLL_FEEDBACK_MASK | RADEON_PPLL_POST_MASK)) |
                         feedback | (encoding << RADEON_FIELD_HIGH_SHIFT));
    for (ULONG poll = 0; poll < RADEON_POLL_LIMIT; ++poll)
        if (!(radeon_pll_read(bi, RADEON_PPLL_REF_DIV) & RADEON_PPLL_ATOMIC_UPDATE))
            break;
    radeon_pll_write(bi, RADEON_PPLL_REF_DIV, radeon_pll_read(bi, RADEON_PPLL_REF_DIV) | RADEON_PPLL_ATOMIC_UPDATE);
    for (ULONG poll = 0; poll < RADEON_PLL_UPDATE_POLLS; ++poll)
        if (!(radeon_pll_read(bi, RADEON_PPLL_REF_DIV) & RADEON_PPLL_ATOMIC_UPDATE))
            break;
    radeon_pll_write(bi, RADEON_HTOTAL_CNTL, 0);
    radeon_pll_write(
        bi, RADEON_PPLL_CNTL,
        radeon_pll_read(bi, RADEON_PPLL_CNTL) & ~(RADEON_PPLL_RESET | RADEON_PPLL_SLEEP | RADEON_PPLL_ATOMIC_ENABLE));
    delayMicroSeconds(RADEON_PPLL_SETTLE_US);
    radeon_pll_write(bi, RADEON_VCLK_ECP_CNTL,
                     (radeon_pll_read(bi, RADEON_VCLK_ECP_CNTL) & ~RADEON_PLL_SOURCE_MASK) | RADEON_PLL_SOURCE_PPLL);
}

static void ASM RadeonSetClock(__REGA0(struct BoardInfo *bi))
{
    asRadeon(bi)->setClock();
}

BOOL ASM RadeonDriver::setInterrupt(__REGD0(BOOL enabled))
{
    BoardInfo *bi = this;
    LOCAL_SYSBASE();
    Disable();
    if (enabled) {
        radeon_update(bi, RADEON_GEN_INT_STATUS, 0, RADEON_VBLANK_INTERRUPT);
        radeon_update(bi, RADEON_GEN_INT_CNTL, 0, RADEON_VBLANK_INTERRUPT);
    } else {
        radeon_update(bi, RADEON_GEN_INT_CNTL, RADEON_VBLANK_INTERRUPT, 0);
    }
    Enable();
    return !enabled;
}

static BOOL ASM RadeonSetInterrupt(__REGA0(struct BoardInfo *bi), __REGD0(BOOL enabled))
{
    return asRadeon(bi)->setInterrupt(enabled);
}

ULONG ASM RadeonDriver::interruptServer()
{
    BoardInfo *bi = this;
    if (!(radeon_read(bi, RADEON_GEN_INT_CNTL) & RADEON_VBLANK_INTERRUPT) ||
        !(radeon_read(bi, RADEON_GEN_INT_STATUS) & RADEON_VBLANK_INTERRUPT))
        return 0;
    LOCAL_SYSBASE();
    radeon_update(bi, RADEON_GEN_INT_CNTL, RADEON_VBLANK_INTERRUPT, 0);
    radeon_update(bi, RADEON_GEN_INT_STATUS, 0, RADEON_VBLANK_INTERRUPT);
    Cause(&bi->SoftInterrupt);
    return 1;
}

extern "C" ULONG ASM RadeonVBlank(__REGA1(struct BoardInfo *bi))
{
    return asRadeon(bi)->interruptServer();
}

DEFINE_INTSERVER(RadeonVBlankServer, RadeonVBlank);

void radeon_init_display(struct BoardInfo *bi)
{
    static const RadeonReg::Id disableRegisters[] = {
        RADEON_OVR_CLR,        RADEON_OVR_WID_LEFT_RIGHT, RADEON_OVR_WID_TOP_BOTTOM, RADEON_OV0_SCALE_CNTL,
        RADEON_MPP_TB_CONFIG,  RADEON_MPP_GP_CONFIG,      RADEON_RBBM_SOFT_RESET,    RADEON_SUBPIC_CNTL,
        RADEON_VIPH_CONTROL,   RADEON_I2C_CNTL_1,         RADEON_GEN_INT_CNTL,       RADEON_CAP0_TRIG_CNTL,
        RADEON_CAP1_TRIG_CNTL, RADEON_CRTC_OFFSET,        RADEON_CRTC_OFFSET_CNTL};
    /* Establish a known 640x480 indexed mode before P96 selects a screen.
     * Keep its timing record on the stack only while programming the CRTC.
     */
    struct ModeInfo startup = {0};
    startup.Width           = 640;
    startup.Height          = 480;
    startup.Depth           = 8;
    startup.Flags           = GMF_HPOLARITY | GMF_VPOLARITY;
    startup.HorTotal        = 800;
    startup.VerTotal        = 528;
    startup.HorSyncStart    = 16;
    startup.VerSyncStart    = 2;
    startup.HorSyncSize     = 64;
    startup.VerSyncSize     = 8;
    startup.PixelClock      = 25180000;
    blank_display(bi, TRUE);
    for (ULONG i = 0; i < ARRAY_SIZE(disableRegisters); ++i)
        radeon_write(bi, disableRegisters[i], 0);
    radeon_write(bi, RADEON_CRTC_GEN_CNTL, RADEON_CRTC_DISPLAY_ENABLE);
    radeon_update(bi, RADEON_CRTC_EXT_CNTL, ~RADEON_CRTC_BLANK_MASK, RADEON_CRTC_CRT_ENABLE);
    radeon_update(bi, RADEON_DAC_CNTL, ~RADEON_DAC_RANGE_MASK, RADEON_DAC_MASK_ALL | RADEON_DAC_8BIT_ENABLE);
    bi->RGBFormat = RGBFB_CLUT;
    RadeonSetPanning(bi, bi->MemoryBase, startup.Width, startup.Height, 0, 0, RGBFB_CLUT);
    RadeonSetGC(bi, &startup, FALSE);
    RadeonSetDAC(bi, FALSE, RGBFB_CLUT);
    RadeonSetClock(bi);
    bi->ModeInfo = NULL;
    blank_display(bi, FALSE);
}

void radeon_install_display(struct BoardInfo *bi)
{
    P96_HOOK(bi->SetSwitch, RadeonSetSwitch);
    P96_HOOK(bi->SetColorArray, RadeonSetColorArray);
    P96_HOOK(bi->SetDAC, RadeonSetDAC);
    P96_HOOK(bi->SetGC, RadeonSetGC);
    P96_HOOK(bi->SetPanning, RadeonSetPanning);
    P96_HOOK(bi->CalculateMemory, RadeonCalculateMemory);
    P96_HOOK(bi->GetCompatibleFormats, RadeonGetCompatibleFormats);
    P96_HOOK(bi->SetDisplay, RadeonSetDisplay);
    P96_HOOK(bi->SetClock, RadeonSetClock);
    P96_HOOK(bi->SetMemoryMode, RadeonSetMemoryMode);
    P96_HOOK(bi->SetDPMSLevel, RadeonSetDPMSLevel);
    P96_HOOK(bi->WaitVerticalSync, RadeonWaitVerticalSync);
    P96_HOOK(bi->GetVSyncState, RadeonGetVSyncState);
    P96_HOOK(bi->SetInterrupt, RadeonSetInterrupt);
    /* Exec's generic code pointer omits the interrupt ABI: A1 is is_Data
     * and D0 reports whether this server handled the interrupt.
     */
    bi->HardInterrupt.is_Code         = (VOID (*)())RadeonVBlankServer;
    bi->HardInterrupt.is_Data         = bi;
    bi->HardInterrupt.is_Node.ln_Name = const_cast<char *>("Radeon vertical blank");
}
