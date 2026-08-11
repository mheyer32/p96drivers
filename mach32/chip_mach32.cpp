#include "chip_mach32.h"
#include "common.h"

#define __NOLIBBASE__

#include <exec/types.h>
#include <graphics/rastport.h>
#include <libraries/pcitags.h>
#include <libraries/openpci.h>
#include <proto/openpci.h>

/* Keep this file buildable on old native compilers (no <stdint.h>).
 * clangd parses on the host where pointers may be wider than ULONG. */
#if defined(__clang__) && !defined(__m68k__)
typedef unsigned long ptrint_t;
#else
typedef ULONG ptrint_t;
#endif

/******************************************************************************/

#ifndef MACH32_EMBEDDED_CHIP
extern const char LibName[]     = "ATIMach32.chip";
extern const char LibIdString[] = "ATI Mach32 Picasso96 chip driver";

#ifndef LIB_VERSION
#define LIB_VERSION 1
#endif
#ifndef LIB_REVISION
#define LIB_REVISION 0
#endif
extern const UWORD LibVersion  = LIB_VERSION;
extern const UWORD LibRevision = LIB_REVISION;
#endif

/******************************************************************************/

#if defined(DBG) && !defined(MACH32_EMBEDDED_CHIP)
int debugLevel = INFO;
#endif

void ASM Mach32Driver::waitBlitter()
{
    DFUNC(VERBOSE, "\n");
    this->waitFifo(16);
    Mach32IoQ io = ioQ();
    while (io.testW(IoReg::id_EXT_GE_STATUS, BIT(13))) {
        /* wait for GE idle */
    }
}

static INLINE ULONG getMemoryOffset(struct BoardInfo *bi, APTR memory)
{
    return (ULONG)memory - (ULONG)bi->MemoryBase;
}

/* 8514/A CRT: horizontal in units of 8 pixels; vertical in lines (see DISP_CNTL Y_CONTROL). */
static INLINE UWORD toChars(UWORD pixels)
{
    return (pixels + 7) >> 3;
}

/*
 * Vertical amounts from ModeInfo are in logical scanlines. Interlace halves them.
 *
 * Double-scan modes: do not multiply here. DISP_CNTL DOUBLE_SCAN makes the CRT Y counter
 * visit each logical line twice (bit D11 interleaved as LSB: 0, 0x800, 1, 0x801, … —
 * REG688000-15 §9-1). Program V_TOTAL / V_DISP / sync in that logical space; the chip
 * emits two physical scanlines per logical line. Multiplying by 2 here as well would
 * double-apply line doubling (bad sync vs ModeInfo).
 */
static INLINE UWORD toScanLinesY(UWORD y, UWORD modeFlags)
{
    // if (modeFlags & GMF_INTERLACE) {
    //     y /= 2;
    // }
    return y;
}

/*
 * Mach32 vertical timing registers are expressed in terms of the (potentially non-linear)
 * CRTC Y counter. With DISP_CNTL.Y_CONTROL set to 01b ("normal"), the counter uses the
 * SKIP_2 representation (REG688000-15 §8-10..8-12).
 */
static INLINE UWORD encodeSkip2Y(UWORD linear)
{
    return ((linear << 1) & 0x0FF8) | (linear & 0x0003) | ((linear & 0x0080) >> 5);
}

static INLINE ULONG encodeSkip2YL(ULONG linear)
{
    return ((linear << 1) & 0x0FF8) | (linear & 0x0003) | ((linear & 0x0080) >> 5);
}

void ASM Mach32Driver::setWriteMask(__REGD0(UBYTE mask))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "mask=0x%02lx\n", (ULONG)mask);
    (void)bi;
    (void)mask;
}

void ASM Mach32Driver::setClearMask(__REGD0(UBYTE mask))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "mask=0x%02lx\n", (ULONG)mask);
    (void)bi;
    (void)mask;
}

void ASM Mach32Driver::setReadPlane(__REGD0(UBYTE mask))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "mask=0x%02lx\n", (ULONG)mask);
    (void)bi;
    (void)mask;
}

void ASM Mach32Driver::setSpriteColor(__REGD0(UBYTE idx), __REGD1(UBYTE r), __REGD2(UBYTE g), __REGD3(UBYTE b),
                                      __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "idx=%ld r=%ld g=%ld b=%ld fmt=%ld\n", (ULONG)idx, (ULONG)r, (ULONG)g, (ULONG)b, (ULONG)fmt);

    DRIVER_LOCALS(bi);

    switch (fmt) {
    case RGBFB_NONE:
    case RGBFB_CLUT:
        if (idx == 0)
            io.writeB(IoReg::id_CURSOR_COLOR_0, (UBYTE)(17 + idx));
        else if (idx == 2)
            io.writeB(IoReg::id_CURSOR_COLOR_1, (UBYTE)(17 + idx));
        break;
    default: {
        UBYTE wr = r, wb = b;

        /* Bt481 RGB 888: cursor shifts B,G,R; DAC expects R,G,B. */
        if (fmt == RGBFB_R8G8B8 || fmt == RGBFB_R8G8B8A8 || fmt == RGBFB_A8R8G8B8) {
            wr = b;
            wb = r;
        }

        if (idx == 0) {
            io.writeB(IoReg::id_CURSOR_COLOR_0, wb);
            io.writeW(IoReg::id_EXT_CURSOR_COLOR_0, (UWORD)(((UWORD)wr << 8) | g));
        } else if (idx == 2) {
            io.writeB(IoReg::id_CURSOR_COLOR_1, wb);
            io.writeW(IoReg::id_EXT_CURSOR_COLOR_1, (UWORD)(((UWORD)wr << 8) | g));
        }
        break;
    }
    }
}

void ASM Mach32Driver::setSpriteImage(__REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "fmt=%ld\n", (ULONG)fmt);
    (void)fmt;
    packAtiHwCursorImage(bi);
}

BOOL ASM Mach32Driver::setSprite(__REGD0(BOOL show), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "show=%ld fmt=%ld\n", (ULONG)show, (ULONG)fmt);

    /* REG688000 §9-78: CURSOR_OFFSET_LO/HI are the offset to the cursor definition in **DWORDs**
     * (32-bit units, i.e. byte_offset / 4) from the start of display memory — not bytes or QWs. */
    ULONG byteOff = (ULONG)bi->MouseImageBuffer - (ULONG)bi->MemoryBase;
    ULONG offDW   = byteOff >> 2;

    DRIVER_LOCALS(bi);
    io.writeW(IoReg::id_CURSOR_OFFSET_LO, (UWORD)(offDW & 0xFFFF));
    UWORD hi = (UWORD)((offDW >> 16) & 0xF);
    if (show) {
        hi |= CURSOR_ENA;
    }
    io.writeW(IoReg::id_CURSOR_OFFSET_HI, hi);

    if (show) {
        this->setSpriteColor(0, bi->CLUT[17].Red, bi->CLUT[17].Green, bi->CLUT[17].Blue, fmt);
        this->setSpriteColor(1, bi->CLUT[18].Red, bi->CLUT[18].Green, bi->CLUT[18].Blue, fmt);
        this->setSpriteColor(2, bi->CLUT[19].Red, bi->CLUT[19].Green, bi->CLUT[19].Blue, fmt);
    }

    return TRUE;
}

void ASM Mach32Driver::setSpritePosition(__REGD0(WORD xpos), __REGD1(WORD ypos), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "x=%ld y=%ld fmt=%ld\n", (LONG)xpos, (LONG)ypos, (ULONG)fmt);

    bi->MouseX = xpos;
    bi->MouseY = ypos;

    WORD spriteX = xpos - bi->XOffset;
    WORD spriteY = ypos - bi->YOffset + bi->YSplit;

    WORD offsetX = 0;
    if (spriteX < 0) {
        if (spriteX > -64)
            offsetX = -spriteX;
        else
            offsetX = 64;
        spriteX = 0;
    }
    WORD offsetY = 0;
    if (spriteY < 0) {
        if (spriteY > -64)
            offsetY = -spriteY;
        else
            offsetY = 64;
        spriteY = 0;
    }

    if (bi->ModeInfo && (bi->ModeInfo->Flags & GMF_DOUBLESCAN)) {
        // spriteY *= 2;
    }

    DRIVER_LOCALS(bi);
    // The specs say that the horizontal position is in units of 8 pixels, but in reality this is not true.
    io.writeW(IoReg::id_HORZ_CURSOR_POSN, spriteX & 0x7FF);
    io.writeW(IoReg::id_VERT_CURSOR_POSN, spriteY & 0x0FFF);
    io.writeW(IoReg::id_HORZ_CURSOR_OFFSET, offsetX & 0x3F);
    io.writeW(IoReg::id_VERT_CURSOR_OFFSET, offsetY & 0x3F);
}

void ASM Mach32Driver::setGC(__REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    DRIVER_LOCALS(this);
    if (mi == NULL)
        return;

    ModeInfo = mi;
    Border   = border;

    DFUNC(INFO,
          "SetGC %lux%u HT=%u HB=%u HS=%u HW=%u VT"
          "=%u VB=%u VS=%u VW=%u border=%d\n",
          (ULONG)mi->Width, (ULONG)mi->Height, (ULONG)mi->HorTotal, (ULONG)mi->HorBlankSize, (ULONG)mi->HorSyncStart,
          (ULONG)mi->HorSyncSize, (ULONG)mi->VerTotal, (ULONG)mi->VerBlankSize, (ULONG)mi->VerSyncStart,
          (ULONG)mi->VerSyncSize, (int)border);

    drv->waitBlitter();

    io.writeW(IoReg::id_DISP_CNTL, CRT_RESET | Y_CONTROL_NORMAL);

    UWORD modeFlags = mi->Flags;

    UWORD hTotalChars = toChars(mi->HorTotal) - 1;
    UWORD hDispChars  = toChars(mi->Width) - 1;
    UWORD hSyncStart  = toChars(mi->Width + mi->HorSyncStart) - 1;
    UWORD hSyncWid    = toChars(mi->HorSyncSize) & 0x1F;
    if (modeFlags & GMF_HPOLARITY) {
        hSyncWid |= BIT(5);
    }

    /* Encode linear line counts to SKIP_2 Y-counter representation (DISP_CNTL.Y_CONTROL=01b). */
    UWORD vTotal     = encodeSkip2Y(toScanLinesY(mi->VerTotal, modeFlags) - 1);
    UWORD vDisp      = encodeSkip2Y(toScanLinesY(mi->Height, modeFlags) - 1);
    UWORD vSyncStart = encodeSkip2Y(toScanLinesY(mi->VerSyncStart + mi->Height, modeFlags) - 1);
    UWORD vSyncWid   = toScanLinesY(mi->VerSyncSize, modeFlags) & 0x1F;
    if (modeFlags & GMF_VPOLARITY) {
        vSyncWid |= BIT(5);
    }

    io.writeW(IoReg::id_H_DISP, (hDispChars & 0xFF) | (hTotalChars << 8));
    io.writeW(IoReg::id_H_TOTAL, hTotalChars & 0xFF);
    io.writeW(IoReg::id_H_SYNC_STRT, hSyncStart & 0xFF);
    io.writeW(IoReg::id_H_SYNC_WID, hSyncWid);

    io.writeW(IoReg::id_V_TOTAL, vTotal & 0x0FFF);
    io.writeW(IoReg::id_V_DISP, vDisp & 0x0FFF);
    io.writeW(IoReg::id_V_SYNC_STRT, vSyncStart & 0x0FFF);
    io.writeW(IoReg::id_V_SYNC_WID, vSyncWid);

    if (border) {
        UWORD hb = toChars(mi->HorBlankSize) & 0xF;
        UWORD vb = toScanLinesY(mi->VerBlankSize, modeFlags);
        if (vb > 255)
            vb = 255;
        io.writeW(IoReg::id_HORZ_OVERSCAN, (hb & 0xF) | ((hb & 0xF) << 4));
        io.writeW(IoReg::id_VERT_OVERSCAN, (vb & 0xFF) | ((vb & 0xFF) << 8));
    } else {
        io.writeW(IoReg::id_HORZ_OVERSCAN, 0);
        io.writeW(IoReg::id_VERT_OVERSCAN, 0);
    }

    /* Always leave CRT running — reset would stop VBlank IRQs P96 waits on. */
    UWORD disp = CRT_ENABLED | Y_CONTROL_NORMAL;

    if (modeFlags & GMF_DOUBLESCAN) {
        disp |= DOUBLE_SCAN_BIT;
    }
    if (modeFlags & GMF_INTERLACE) {
        disp |= INTERLACE_BIT;
    }

    io.writeW(IoReg::id_DISP_CNTL, disp);

    /* SetDisplay(FALSE): force blank via V_DISP=0 while CRT (and VBlank) keep running. */
    if (!(ChipFlags & 1))
        io.writeW(IoReg::id_V_DISP, 0);
}

void ASM Mach32Driver::setPanning(__REGA1(UBYTE *memory), __REGD0(UWORD width), __REGD3(UWORD height),
                                  __REGD1(WORD xoffset), __REGD2(WORD yoffset), __REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    DFUNC(INFO,
          "mem 0x%lx, width %ld, height %ld, xoffset %ld, yoffset %ld, "
          "format %ld\n",
          memory, (ULONG)width, (ULONG)height, (LONG)xoffset, (LONG)yoffset, (ULONG)format);

#ifndef NDEBUG
    if (width & 7) {
        DFUNC(ERROR, "Panning pitch not a multiple of 8\n");
        return;
    }
#endif

    LONG panOffset;
    ULONG pitch;
    ULONG memOffset;

    bi->XOffset = xoffset;
    bi->YOffset = yoffset;
    memOffset   = getMemoryOffset(bi, memory);

    UBYTE bpp = getBPP(format);
    panOffset = (yoffset * width + xoffset) * bpp;
    panOffset = (panOffset + memOffset) / 4;  // offset in 32bit words

    DRIVER_LOCALS(bi);
    io.writeW(IoReg::id_CRT_OFFSET_LO, panOffset & 0xFFFF);
    io.writeW(IoReg::id_CRT_OFFSET_HI, (panOffset >> 16));

    pitch = width / 8;  // pitch in 8 pixels
    // if (bi->ModeInfo && (bi->ModeInfo->Flags & GMF_DOUBLESCAN)) {
    //     pitch = -pitch;
    // }
    /* Linear scanout advances by CRT_PITCH once per physical raster line. DISP_CNTL
     * DOUBLE_SCAN (SetGC) shapes the 8514/A line counter for sync/blanking; it does
     * not by itself halve row-increment rate. Line doubling for the framebuffer is
     * usually tied to the VGA CRTC path (e.g. CR9 doublescan) or other fetch control. */
    io.writeW(IoReg::id_CRT_PITCH, pitch);

    D(VERBOSE, "panOffset 0x%lx, pitch %ld qwords\n", panOffset, (ULONG)pitch);
}

UWORD ASM Mach32Driver::calculateBytesPerRow(__REGD0(UWORD width), __REGD1(UWORD height), __REGA1(struct ModeInfo *mi),
                                             __REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "width=%lu height=%lu mi=0x%lx fmt=%ld\n", (ULONG)width, (ULONG)height, (ULONG)mi, (ULONG)format);
    (void)bi;
    (void)height;
    UBYTE bpp = getBPP(format);

    // Pitch needs to be 8 pixels aligned
    width     = (width + 7) & ~7;
    UWORD bpr = width * bpp;

    if (mi && (mi->Flags & GMF_DOUBLESCAN)) {
        /* Fake doublescan doubles pitch; GE_PITCH must still fit. */
        if ((ULONG)width * 2 > MACH32_MAX_PITCH_PIXELS) {
            return 0;
        }
        bpr <<= 1;
    }
    return bpr;
}
/* removed forward decl: FillRect (now FillRect method) */

APTR ASM Mach32Driver::allocCardMem(__REGD0(ULONG size), __REGD1(BOOL force), __REGD2(BOOL system),
                                    __REGD3(ULONG bytesperrow), __REGA1(struct ModeInfo *mi),
                                    __REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    APTR mem = getConstCardData(bi)->AllocCardMemDefault(bi, size, force, system, bytesperrow, mi, AS_RGBF(format));

    if (mi && (mi->Flags & GMF_DOUBLESCAN)) {
        struct RenderInfo ri;
        ri.Memory      = (APTR)((ULONG)mem + bytesperrow / 2);
        ri.BytesPerRow = bytesperrow;
        ri.RGBFormat   = AS_RGBF(format);
        this->waitBlitter();
        /* Bitmap size may differ from ModeInfo; derive from the allocated chunk. */
        this->fillRect(&ri, 0, 0, (bytesperrow / 2) / getBPP(format), size / bytesperrow, 0, 0xFF, AS_RGBF(format));
        this->waitBlitter();
    }

    return mem;
}

APTR ASM Mach32Driver::calculateMemory(__REGA1(APTR mem), __REGD0(struct RenderInfo *ri), __REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "mem=%p ri=%p fmt=%ld\n", mem, ri, (ULONG)format);
    (void)bi;
    (void)ri;
    (void)format;
    return mem;
}

ULONG ASM Mach32Driver::getCompatibleFormats(__REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "fmt=%ld\n", (ULONG)format);
    (void)bi;
    return MACH32_SUPPORTED_RGBFF;
}

void ASM Mach32Driver::setDAC(__REGD0(UWORD region), __REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    DFUNC(INFO, "region=%lu fmt=%ld\n", (ULONG)region, (ULONG)format);
    (void)region;
    const RamdacOps_t *ops = getConstChipData(bi)->ramdacOps;

    UWORD dac8 = (format == RGBFB_CLUT && bi->BitsPerCannon == 8) ? DAC_8BIT_EN : 0;

    asMach32(bi)->writeExtGeConfigMask(DISPLAY_PIXEL_SIZE_MASK | PIXEL_WIDTH_MASK | _16_BIT_COLOR_MODE_MASK |
                                           _24_BIT_COLOR_CONFIG_MASK | _24_BIT_COLOR_ORDER_MASK | DAC_8BIT_EN_MASK |
                                           MULTIPLEX_PIXELS_MASK,
                                       PIXEL_WIDTH(1) | DISPLAY_PIXEL_SIZE | dac8);

    ops->setDac(bi, AS_RGBF(format));

    UWORD config = 0;

    switch (format) {
    case RGBFB_CLUT:
        config = PIXEL_WIDTH(1);
        break;
    case RGBFB_R5G5B5PC:
        config = PIXEL_WIDTH(2) | _16_BIT_COLOR_MODE(EXT_GE_16BIT_555);
        break;
    case RGBFB_R5G6B5PC:
        config = PIXEL_WIDTH(2) | _16_BIT_COLOR_MODE(EXT_GE_16BIT_565);
        break;
    case RGBFB_R8G8B8:
        config = PIXEL_WIDTH(3) | _24_BIT_COLOR_CONFIG(0) | _24_BIT_COLOR_ORDER(0);
        break;
    case RGBFB_B8G8R8:
        config = PIXEL_WIDTH(3) | _24_BIT_COLOR_CONFIG(0) | _24_BIT_COLOR_ORDER(1);
        break;
    case RGBFB_R8G8B8A8:
        config = PIXEL_WIDTH(3) | _24_BIT_COLOR_CONFIG(1) | _24_BIT_COLOR_ORDER(0);
        break;
    case RGBFB_B8G8R8A8:
        config = PIXEL_WIDTH(3) | _24_BIT_COLOR_CONFIG(1) | _24_BIT_COLOR_ORDER(1);
        break;
    case RGBFB_A8R8G8B8:
        config = PIXEL_WIDTH(3) | _24_BIT_COLOR_CONFIG(1) | _24_BIT_COLOR_ORDER(0);
        break;
    case RGBFB_A8B8G8R8:
        config = PIXEL_WIDTH(3) | _24_BIT_COLOR_CONFIG(1) | _24_BIT_COLOR_ORDER(1);
        break;
    default:
        break;
    }
    config |= DISPLAY_PIXEL_SIZE | dac8;

    asMach32(bi)->writeExtGeConfigMask(DISPLAY_PIXEL_SIZE_MASK | PIXEL_WIDTH_MASK | _16_BIT_COLOR_MODE_MASK |
                                           _24_BIT_COLOR_CONFIG_MASK | _24_BIT_COLOR_ORDER_MASK | DAC_8BIT_EN_MASK |
                                           MULTIPLEX_PIXELS_MASK,
                                       config);
}

void ASM Mach32Driver::setColorArray(__REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "startIndex %ld, count %ld\n", (ULONG)startIndex, (ULONG)count);

    if (bi->RGBFormat != RGBFB_CLUT)
        return;

    LOCAL_SYSBASE();

    const UBYTE bppDiff = 8 - bi->BitsPerCannon;
    Mach32IoQ dac       = ioQ();

    /* Must not be interrupted: W_INDEX stays set for the R/G/B stream. */
    Disable();

    dac.writeB(IoReg::id_DAC_W_INDEX, (UBYTE)startIndex);

    struct CLUTEntry *entry = &bi->CLUT[startIndex];

    for (UWORD c = 0; c < count; ++c) {
        dac.writeB(IoReg::id_DAC_DATA, entry->Red >> bppDiff);
        dac.writeB(IoReg::id_DAC_DATA, entry->Green >> bppDiff);
        dac.writeB(IoReg::id_DAC_DATA, entry->Blue >> bppDiff);
        ++entry;
    }

    Enable();
}

BOOL ASM Mach32Driver::setDisplay(__REGD0(BOOL state))
{
    BoardInfo *bi = this;
    DFUNC(INFO, "state=%ld\n", (ULONG)state);
    DRIVER_LOCALS(bi);

    UWORD disp = CRT_ENABLED | Y_CONTROL_NORMAL;
    if (bi->ModeInfo) {
        UWORD modeFlags = bi->ModeInfo->Flags;
        if (modeFlags & GMF_DOUBLESCAN)
            disp |= DOUBLE_SCAN_BIT;
        if (modeFlags & GMF_INTERLACE)
            disp |= INTERLACE_BIT;
    }
    io.writeW(IoReg::id_DISP_CNTL, disp);

    if (bi->ModeInfo) {
        if (state) {
            UWORD modeFlags = bi->ModeInfo->Flags;
            UWORD vDisp     = encodeSkip2Y(toScanLinesY(bi->ModeInfo->Height, modeFlags) - 1);
            io.writeW(IoReg::id_V_DISP, vDisp & 0x0FFF);
        } else {
            io.writeW(IoReg::id_V_DISP, 0);
        }
    }

    bi->ChipFlags = (bi->ChipFlags & ~1) | (state & 1);
    return TRUE;
}

void ASM Mach32Driver::setMemoryMode(__REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "fmt=%ld\n", (ULONG)format);
    (void)bi;
    (void)format;
}

LONG ASM Mach32Driver::resolvePixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG pixelClock),
                                         __REGD7(RGBFTYPE_REG RGBFormat))
{
    BoardInfo *bi = this;
    DFUNC(CHATTY, "mi=0x%lx target=%lu fmt=%ld\n", (ULONG)mi, pixelClock, (ULONG)RGBFormat);
    (void)bi;
    return ResolveModeInfoPixelClock(mi, pixelClock, AS_RGBF(RGBFormat));
}

ULONG ASM Mach32Driver::getPixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG index), __REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "index=%lu fmt=%ld\n", index, (ULONG)format);

    ULONG pixelClock = HzForClockIndexAsLogicalDotsPerSecond(index, AS_RGBF(format));

    D(VERBOSE, "Pixel clock for index %lu is %lu Hz (logical dots/s)\n", index, pixelClock);

    return pixelClock;
}

void ASM Mach32Driver::setClock()
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "\n");
    const RamdacOps_t *ops = getConstChipData(bi)->ramdacOps;
    ops->setClock(bi);
}

BOOL ASM Mach32Driver::getVSyncState(__REGD0(BOOL expected))
{
    BoardInfo *bi = this;
    (void)expected;
    DRIVER_LOCALS(bi);
    /*
     * Use live beam position vs programmed V_DISP (§9: blank asserts after V_DISP).
     */
    return (io.readW(IoReg::id_VERT_LINE_CNTR) & 0x7FF) > (io.readW(IoReg::id_R_V_DISP) & 0x0FFF);
}

BOOL ASM Mach32Driver::setInterrupt(__REGD0(BOOL state))
{
    BoardInfo *bi = this;
    DRIVER_LOCALS(bi);
    LOCAL_SYSBASE();
    Disable();

    /* Do not touch GE_RESET[15:14] (0 = no change). */
    if (state)
        io.writeW(IoReg::id_SUBSYS_CNTL, SUBSYS_VBLANK_ACK | SUBSYS_VBLANK_ENA);
    else
        io.writeW(IoReg::id_SUBSYS_CNTL, SUBSYS_VBLANK_ACK);

    Enable();
    return TRUE;
}

#if defined(TESTEXE)
static volatile ULONG hardVBlankEntries;
static volatile ULONG hardVBlankHandled;
#endif

/* OpenPCI/Exec interrupt server: is_Data (BoardInfo *) in a1.
 * Return non-zero if we handled this board's IRQ; else 0. Entry sets CCR.Z. */
ULONG __attribute__((noinline)) Mach32Driver::interruptServer()
{
    BoardInfo *bi = this;
    DRIVER_LOCALS(bi);

#if defined(TESTEXE)
    hardVBlankEntries++;
#endif

    if (!(ioQ.readW(IoReg::id_SUBSYS_STATUS) & SUBSYS_VBLANK_INT))
        return 0;

    /* Ack while keeping VBLANK_ENA so continuous IRQs keep firing. */
    ioQ.writeW(IoReg::id_SUBSYS_CNTL, SUBSYS_VBLANK_ACK | SUBSYS_VBLANK_ENA);

#if defined(TESTEXE)
    hardVBlankHandled++;
#endif
    {
        LOCAL_SYSBASE();
        Cause(&bi->SoftInterrupt);
    }
    return 1;
}

/**
 * Set DPMS (Display Power Management Signaling) level.
 *
 * Uses HORZ_OVERSCAN[15:13] (SYN_CONT_SEL/HSYN_CONT/VSYN_CONT) to force the HSYNC/VSYNC
 * pins to constant levels for monitor power-down.
 *
 * DPMS levels: DPMS_ON (0), DPMS_STANDBY (1), DPMS_SUSPEND (2), DPMS_OFF (3)
 */
void ASM Mach32Driver::setDPMSLevel(__REGD0(ULONG level))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "level=%ld\n", level);

    if (level > 3) {
        level = 3;
    }

    DRIVER_LOCALS(bi);

    /* Recreate the baseline overscan widths (and sync delay=0) based on current border setting. */
    UWORD base = 0;
    // if (bi->Border && bi->ModeInfo) {
    //     UWORD hb = (UWORD)(toChars(bi->ModeInfo->HorBlankSize) & 0xFu);
    //     base     = (UWORD)((hb & 0xFu) | ((hb & 0xFu) << 4));
    // }

    /*
     * Force sync pins to constant levels. A value of 1 corresponds to the "inactive" sync level.
     * This provides DPMS-style combinations without requiring the normal timing generator.
     */
    static const UWORD dpmsMask[4] = {
        0,                                    /* ON */
        SYN_CONT_SEL | HSYN_CONT,             /* STANDBY: HSYNC inactive, VSYNC active */
        SYN_CONT_SEL | VSYN_CONT,             /* SUSPEND: VSYNC inactive, HSYNC active */
        SYN_CONT_SEL | HSYN_CONT | VSYN_CONT, /* OFF: both inactive */
    };

    io.writeW(IoReg::id_HORZ_OVERSCAN, base | dpmsMask[level]);
}

void ASM Mach32Driver::waitVerticalSync(__REGD0(BOOL end))
{
    BoardInfo *bi = this;
    DFUNC(CHATTY, "\n");
    (void)end;
    (void)bi;
}

ULONG ASM Mach32Driver::getVBeamPos()
{
    BoardInfo *bi = this;
    DRIVER_LOCALS(bi);
    return io.readW(IoReg::id_VERT_LINE_CNTR) & 0x7FF;
}

// FIXME: refactor to unify with SetDAC
static void computeGEConfig(RGBFTYPE_REG fmt, UWORD *maskOut, UWORD *valOut)
{
    *maskOut  = DRAW_PIXEL_SIZE_MASK | PIXEL_WIDTH_MASK;
    UWORD cfg = 0;

    switch (fmt) {
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G6B5PC:
        cfg = PIXEL_WIDTH(2);
        break;
    default:
        cfg = PIXEL_WIDTH(1);
        break;
    }
    // Important: by setting this bit, we can set the draw engine's format differently from the display
    // format!
    cfg |= DRAW_PIXEL_SIZE;
    *valOut = cfg;
}

static INLINE void setBlitterFormat(BoardInfo_t *bi, RGBFTYPE_REG fmt)
{
    ChipData_t *cd = getChipData(bi);
    ULONG f        = (ULONG)fmt;

    if (cd->GEfmt == f) {
        return;
    }
    cd->GEfmt = f;

    asMach32(bi)->waitBlitter();
    UWORD emask, eval;
    computeGEConfig(fmt, &emask, &eval);

    DRIVER_LOCALS(bi);
    asMach32(bi)->writeExtGeConfigMask(emask, eval);

    if (fmt != RGBFB_CLUT && cd->GEmask != 0xFF) {
        cd->GEmask = 0xFF;
        ioNS.writeW(IoReg::id_WRT_MASK, 0xFFFF);
    }
}

static INLINE ULONG REGARGS penToColor(ULONG pen, RGBFTYPE_REG fmt)
{
    switch (fmt) {
    case RGBFB_B8G8R8:
    case RGBFB_B8G8R8A8:
    case RGBFB_A8B8G8R8:
        pen = swapl(pen);
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
        pen = swapw(pen);
        break;
    case RGBFB_CLUT:
        break;
    default:
        break;
    }
    return pen;
}

static INLINE void setGEWriteMask(BoardInfo_t *bi, UBYTE mask, RGBFTYPE_REG fmt, UBYTE waitFifoSlots)
{
    ChipData_t *cd = getChipData(bi);

    if (fmt == RGBFB_CLUT && cd->GEmask != mask) {
        cd->GEmask = mask;
        asMach32(bi)->waitFifo(waitFifoSlots + 1);
        DRIVER_LOCALS(bi);
        ioNS.writeW(IoReg::id_WRT_MASK, ((mask << 8) | mask));
    } else {
        asMach32(bi)->waitFifo(waitFifoSlots);
    }
}

/* SHADOW_SET[9:8] selects which GE_OFFSET/GE_PITCH shadow to load (REG688000-15 “Far-Blit”). */
#define SHADOW_SET_GE_PTR_SHIFT 8u
#define SHADOW_SET_GE_PTR_MASK  (3u << SHADOW_SET_GE_PTR_SHIFT)
#define SHADOW_SET_GE_PTR_BOTH  (0u << SHADOW_SET_GE_PTR_SHIFT)
#define SHADOW_SET_GE_PTR_DST   (1u << SHADOW_SET_GE_PTR_SHIFT)
#define SHADOW_SET_GE_PTR_SRC   (2u << SHADOW_SET_GE_PTR_SHIFT)

/* Far-Blit: load dst and src offset/pitch independently via SHADOW_SET[9:8]. */
static void myMemset(APTR dst, UBYTE val, ULONG len)
{
    UBYTE *p = (UBYTE *)dst;
    while (len--) {
        *p++ = val;
    }
}

static int myMemcmp(CONST_APTR a, CONST_APTR b, ULONG len)
{
    const UBYTE *p = (const UBYTE *)a;
    const UBYTE *q = (const UBYTE *)b;
    while (len--) {
        if (*p != *q)
            return (int)*p - (int)*q;
        p++;
        q++;
    }
    return 0;
}

static INLINE void setFarBlitBuffer(BoardInfo_t *bi, const struct RenderInfo *ri, RGBFTYPE_REG fmt, UWORD srcOrDst)
{
    ChipData_t *cd              = getChipData(bi);
    struct RenderInfo *cachedRi = NULL;
    UWORD shadowSel             = (srcOrDst + 1) << SHADOW_SET_GE_PTR_SHIFT;

    /* Keep draw engine's format in sync with pitch/offset programming. */
    setBlitterFormat(bi, fmt);

    cachedRi = &cd->srcDstRenderInfoCache[srcOrDst];
    if (myMemcmp(ri, cachedRi, sizeof(*cachedRi)) == 0) {
        return;
    }
    *cachedRi = *ri;

    UBYTE bppLog2  = getBPPLog2(fmt);
    UWORD pitch    = ri->BytesPerRow >> (bppLog2 + 3);
    ULONG offWords = getMemoryOffset(bi, ri->Memory) >> 2;

    asMach32(bi)->waitBlitter();
    DRIVER_LOCALS(bi);
    /* Select dst/src shadow for GE_OFFSET/GE_PITCH load. */
    io.writeW(IoReg::id_SHADOW_SET, shadowSel);
    io.writeW(IoReg::id_GE_PITCH, pitch);
    io.writeW(IoReg::id_GE_OFFSET_LO, offWords);
    io.writeW(IoReg::id_GE_OFFSET_HI, offWords >> 16);
}

static void drawRect(BoardInfo_t *bi, WORD x, WORD y, WORD width, WORD height)
{
    asMach32(bi)->waitFifo(5);
    DRIVER_LOCALS(bi);

    // io.writeW(IoReg::id_SRC_Y_DIR, 1); // FIXME: needed?
    io.writeW(IoReg::id_CUR_X, x);
    io.writeW(IoReg::id_CUR_Y, y);
    io.writeW(IoReg::id_DEST_X_START, x);
    io.writeW(IoReg::id_DEST_X_END, x + width);
    io.writeW(IoReg::id_DEST_Y_END, y + height);
    flushWrites();
}

void ASM Mach32Driver::fillRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                                __REGD3(WORD height), __REGD4(ULONG pen), __REGD5(UBYTE mask),
                                __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\npen %08lx, mask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)pen, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    UBYTE bpp = getBPP(fmt);
    if (bpp > 2) {
        DFUNC(INFO, "Fallback to FillRectDefault\n");
        bi->FillRectDefault(bi, ri, x, y, width, height, pen, mask, AS_RGBF(fmt));
        return;
    }

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != FILLRECT) {
        cd->GEOp       = FILLRECT;
        cd->GEdrawMode = 0xFF;

        asMach32(bi)->waitFifo(3);
        DRIVER_LOCALS(bi);

        io.writeW(IoReg::id_DP_CONFIG, DP_CONFIG_REPLACE);
        io.writeW(IoReg::id_ALU_FG_FN, 0x0027);
        io.writeW(IoReg::id_ALU_BG_FN, 0x0027);
    }

    if (cd->GEfgPen != pen) {
        cd->GEfgPen = pen;
        pen         = penToColor(pen, fmt);
        asMach32(bi)->waitFifo(1);
        DRIVER_LOCALS(bi);
        io.writeW(IoReg::id_FRGD_COLOR, pen);
    }

    setFarBlitBuffer(bi, ri, fmt, 0);
    setGEWriteMask(bi, mask, fmt, 0);
    drawRect(bi, x, y, width, height);
}

void ASM Mach32Driver::invertRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                                  __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    UBYTE bpp = getBPP(fmt);
    if (bpp != 1 && bpp != 2) {
        DFUNC(INFO, "Fallback to InvertRectDefault\n");
        bi->InvertRectDefault(bi, ri, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != INVERTRECT) {
        cd->GEOp       = INVERTRECT;
        cd->GEdrawMode = 0xFF;

        asMach32(bi)->waitFifo(2);
        DRIVER_LOCALS(bi);

        io.writeW(IoReg::id_DP_CONFIG, DP_CONFIG_REPLACE);
        /* 8514/A path: foreground color source, NOT destination (REG688000-15 §8-24) */
        io.writeW(IoReg::id_FRGD_MIX, 0x0020);
    }

    setFarBlitBuffer(bi, ri, fmt, 0);
    setGEWriteMask(bi, mask, fmt, 0);
    drawRect(bi, x, y, width, height);
}

void ASM Mach32Driver::blitRect(__REGA1(struct RenderInfo *ri), __REGD0(WORD srcX), __REGD1(WORD srcY),
                                __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width), __REGD5(WORD height),
                                __REGD6(UBYTE mask), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nsx %ld, sy %ld, dx %ld, dy %ld, w %ld, h %ld\n"
          "mask 0x%lx fmt %ld\nri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt,
          (ULONG)ri->BytesPerRow, (ULONG)ri->Memory);

    UBYTE bpp = getBPP(fmt);
    if (bpp != 1 && bpp != 2) {
        DFUNC(INFO, "Fallback to BlitRectDefault\n");
        bi->BlitRectDefault(bi, ri, srcX, srcY, dstX, dstY, width, height, mask, AS_RGBF(fmt));
        return;
    }

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITRECT) {
        cd->GEOp       = BLITRECT;
        cd->GEdrawMode = 0xFF;

        asMach32(bi)->waitFifo(3);
        DRIVER_LOCALS(bi);
        io.writeW(IoReg::id_DP_CONFIG, DP_CONFIG_BLIT);
        /* COLOR_SRC must be blit source (11b) to read from VRAM, MIX=replace (REG688000-15 §8-24) */
        io.writeW(IoReg::id_FRGD_MIX, 0x0067);
        io.writeW(IoReg::id_BKGD_MIX, 0x0067);
    }

    setFarBlitBuffer(bi, ri, fmt, 0);
    setGEWriteMask(bi, mask, fmt, 10);

    DRIVER_LOCALS(bi);

    if ((dstY > srcY) || (dstY == srcY && dstX > srcX)) {
        /* Overlap: copy bottom-to-top, right-to-left */
        io.writeW(IoReg::id_SRC_X_DEST_X, srcX + width);
        io.writeW(IoReg::id_SRC_X_START, srcX + width);
        io.writeW(IoReg::id_SRC_Y_DEST_Y, srcY + height - 1);
        io.writeW(IoReg::id_SRC_X_END, srcX);
        io.writeW(IoReg::id_SRC_Y_DIR, 0);

        io.writeW(IoReg::id_CUR_X, dstX + width);
        io.writeW(IoReg::id_DEST_X_START, dstX + width);
        io.writeW(IoReg::id_CUR_Y, dstY + height - 1);
        io.writeW(IoReg::id_DEST_X_END, dstX);
        io.writeW(IoReg::id_DEST_Y_END, dstY - 1);
    } else {
        /* No overlap risk: copy top-to-bottom, left-to-right */
        io.writeW(IoReg::id_SRC_X_DEST_X, srcX);
        io.writeW(IoReg::id_SRC_X_START, srcX);
        io.writeW(IoReg::id_SRC_Y_DEST_Y, srcY);
        io.writeW(IoReg::id_SRC_X_END, srcX + width);
        io.writeW(IoReg::id_SRC_Y_DIR, 1);

        io.writeW(IoReg::id_CUR_X, dstX);
        io.writeW(IoReg::id_DEST_X_START, dstX);
        io.writeW(IoReg::id_CUR_Y, dstY);
        io.writeW(IoReg::id_DEST_X_END, dstX + width);
        io.writeW(IoReg::id_DEST_Y_END, dstY + height);
    }
    flushWrites();
}

/* FRGD_MIX / BKGD_MIX[7:5] = color source, [3:0] = mix function (REG688000-15 §8-24). */
#define CLR_SRC_BKGD_COLOR (0u << 5)
#define CLR_SRC_FRGD_COLOR (1u << 5)
#define CLR_SRC_CPU        (2u << 5)
#define CLR_SRC_MEMORY     (3u << 5)

/* P96 BlitRectNoMaskComplete: 4-bit minterm (B=source, C=destination). */
static const UBYTE minTermToMix[16] = {
    MIX_ZERO,                     // 0000
    MIX_NOT_CURRENT_AND_NOT_NEW,  // 0001
    MIX_CURRENT_AND_NOT_NEW,      // 0010
    MIX_NOT_NEW,                  // 0011
    MIX_NOT_CURRENT_AND_NEW,      // 0100
    MIX_NOT_CURRENT,              // 0101
    MIX_CURRENT_XOR_NEW,          // 0110
    MIX_NOT_CURRENT_OR_NOT_NEW,   // 0111
    MIX_CURRENT_AND_NEW,          // 1000
    MIX_NOT_CURRENT_XOR_NEW,      // 1001
    MIX_CURRENT,                  // 1010
    MIX_CURRENT_OR_NOT_NEW,       // 1011
    MIX_NEW,                      // 1100
    MIX_NOT_CURRENT_OR_NEW,       // 1101
    MIX_CURRENT_OR_NEW,           // 1110
    MIX_ONE,                      // 1111
};

void ASM Mach32Driver::blitRectNoMaskComplete(__REGA1(struct RenderInfo *sri), __REGA2(struct RenderInfo *dri),
                                              __REGD0(WORD srcX), __REGD1(WORD srcY), __REGD2(WORD dstX),
                                              __REGD3(WORD dstY), __REGD4(WORD width), __REGD5(WORD height),
                                              __REGD6(UBYTE opCode), __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nsx %ld, sy %ld, dx %ld, dy %ld, w %ld, h %ld\n"
          "minTerm 0x%lx fmt %ld\n"
          "sri->bytesPerRow %ld, sri->memory 0x%lx\n"
          "dri->bytesPerRow %ld, dri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)opCode, (ULONG)fmt,
          (ULONG)sri->BytesPerRow, (ULONG)sri->Memory, (ULONG)dri->BytesPerRow, (ULONG)dri->Memory);

    /* Mach32 GE blit path in this driver currently supports 8/16bpp modes only. */
    // FIXME: here we could still accelerate 24bpp modes by treating them as 32bpp with unused bits, but it would
    // require more extensive changes to the engine setup and coordinate calculations.
    UBYTE bpp = getBPP(fmt);
    if (bpp != 1 && bpp != 2) {
        DFUNC(INFO, "Fallback to BlitRectNoMaskCompleteDefault\n");
        bi->BlitRectNoMaskCompleteDefault(bi, sri, dri, srcX, srcY, dstX, dstY, width, height, opCode, AS_RGBF(fmt));
        return;
    }

    ChipData_t *cd = getChipData(bi);

    DRIVER_LOCALS(bi);
    if (cd->GEOp != BLITRECTNOMASKCOMPLETE) {
        cd->GEOp       = BLITRECTNOMASKCOMPLETE;
        cd->GEdrawMode = 0xFF; /* invalidate minterm cache */
        cd->GEmask     = 0xFF;

        asMach32(bi)->waitFifo(3);

        io.writeW(IoReg::id_DP_CONFIG, DP_CONFIG_BLIT);
        io.writeW(IoReg::id_ALU_BG_FN, MIX_ZERO);
        io.writeW(IoReg::id_WRT_MASK, 0xFFFF);
    }

    if (cd->GEdrawMode != opCode) {
        cd->GEdrawMode = opCode;

        asMach32(bi)->waitFifo(1);

        UWORD mix = minTermToMix[opCode & 0xF];
        io.writeW(IoReg::id_ALU_FG_FN, mix);
    }

    setFarBlitBuffer(bi, dri, fmt, 0);
    setFarBlitBuffer(bi, sri, fmt, 1);

    BOOL overlap = (sri->Memory == dri->Memory) && (sri->BytesPerRow == dri->BytesPerRow);

    asMach32(bi)->waitFifo(10);

    if (overlap && ((dstY > srcY) || (dstY == srcY && dstX > srcX))) {
        /* Overlap: copy bottom-to-top, right-to-left */
        io.writeW(IoReg::id_SRC_X_DEST_X, srcX + width);
        io.writeW(IoReg::id_SRC_X_START, srcX + width);
        io.writeW(IoReg::id_SRC_Y_DEST_Y, srcY + height - 1);
        io.writeW(IoReg::id_SRC_X_END, srcX);
        io.writeW(IoReg::id_SRC_Y_DIR, 0);

        io.writeW(IoReg::id_CUR_X, dstX + width);
        io.writeW(IoReg::id_DEST_X_START, dstX + width);
        io.writeW(IoReg::id_CUR_Y, dstY + height - 1);
        io.writeW(IoReg::id_DEST_X_END, dstX);
        io.writeW(IoReg::id_DEST_Y_END, dstY - 1);
    } else {
        /* No overlap risk: copy top-to-bottom, left-to-right */
        io.writeW(IoReg::id_SRC_X_DEST_X, srcX);
        io.writeW(IoReg::id_SRC_X_START, srcX);
        io.writeW(IoReg::id_SRC_Y_DEST_Y, srcY);
        io.writeW(IoReg::id_SRC_X_END, srcX + width);
        io.writeW(IoReg::id_SRC_Y_DIR, 1);

        io.writeW(IoReg::id_CUR_X, dstX);
        io.writeW(IoReg::id_DEST_X_START, dstX);
        io.writeW(IoReg::id_CUR_Y, dstY);
        io.writeW(IoReg::id_DEST_X_END, dstX + width);
        io.writeW(IoReg::id_DEST_Y_END, dstY + height);
    }

    flushWrites();
}

static INLINE void REGARGS setDrawMode(BoardInfo_t *bi, ULONG fgPen, ULONG bgPen, UBYTE drawMode, RGBFTYPE_REG fmt)
{
    ChipData_t *cd = getChipData(bi);
    if (cd->GEfgPen == fgPen && cd->GEbgPen == bgPen && cd->GEdrawMode == drawMode) {
        return;
    }

    cd->GEfgPen    = fgPen;
    cd->GEbgPen    = bgPen;
    cd->GEdrawMode = drawMode;

    ULONG fg = penToColor(fgPen, fmt);
    ULONG bg = penToColor(bgPen, fmt);

    UBYTE writeMode = (drawMode & COMPLEMENT) ? MIX_NOT_CURRENT : MIX_NEW;

    UBYTE fMix, bMix;
    if (drawMode & JAM2) {
        fMix = writeMode;
        bMix = writeMode;
    } else {
        fMix = writeMode;
        bMix = MIX_CURRENT;
    }

    UWORD fSrc = CLR_SRC_FRGD_COLOR;
    UWORD bSrc = CLR_SRC_BKGD_COLOR;
    if (drawMode & INVERSVID) {
        UBYTE tMix = fMix;
        fMix       = bMix;
        bMix       = tMix;

        UWORD tSrc = fSrc;
        fSrc       = bSrc;
        bSrc       = tSrc;
    }

    asMach32(bi)->waitFifo(4);
    DRIVER_LOCALS(bi);

    io.writeW(IoReg::id_FRGD_COLOR, fg);
    io.writeW(IoReg::id_BKGD_COLOR, bg);
    io.writeW(IoReg::id_FRGD_MIX, (UWORD)(fSrc | fMix));
    io.writeW(IoReg::id_BKGD_MIX, (UWORD)(bSrc | bMix));
}

void ASM Mach32Driver::blitTemplate(__REGA1(struct RenderInfo *ri), __REGA2(struct Template *tmpl), __REGD0(WORD x),
                                    __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                                    __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    UBYTE bpp = getBPP(fmt);
    if (bpp != 1 && bpp != 2) {
        DFUNC(INFO, "Fallback to BlitTemplateDefault\n");
        bi->BlitTemplateDefault(bi, ri, tmpl, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    DRIVER_LOCALS(bi);

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITTEMPLATE) {
        cd->GEOp       = BLITTEMPLATE;
        cd->GEdrawMode = 0xFF;

        asMach32(bi)->waitFifo(1);
        io.writeW(IoReg::id_DP_CONFIG, DP_CONFIG_TEMPLATE);
    }

    setFarBlitBuffer(bi, ri, fmt, 0);
    setDrawMode(bi, tmpl->FgPen, tmpl->BgPen, tmpl->DrawMode, fmt);
    setGEWriteMask(bi, mask, fmt, 1);

    /* Clip padding (and avoid CPU bit-rotation into left margin). */
    // io.writeW(IoReg::id_SCISSOR_LEFT, x);
    io.writeW(IoReg::id_SCISSOR_RIGHT, x + width - 1);

    /* 16 pixels per PIX_TRANS word. Round up width+offset to 16. */
    UWORD rol      = (UWORD)(tmpl->XOffset & 15);
    WORD blitWidth = (width + rol + 15) & ~15;

    /* Set up rectangle; writing DEST_Y_END kicks the engine. */
    drawRect(bi, x, y, blitWidth, height);

    WORD wordsPerLn    = blitWidth >> 4;
    ULONG fifoNeed     = (ULONG)wordsPerLn * (ULONG)height + 3u;
    UBYTE numFifoSlots = fifoNeed > 16u ? 16 : (UBYTE)fifoNeed;
    asMach32(bi)->waitFifo(numFifoSlots);
    WORD usedFifoSlots = 16 - numFifoSlots;

    const UBYTE *bitmap = (const UBYTE *)tmpl->Memory;
    bitmap += (tmpl->XOffset >> 4) * 2;
    WORD bitmapPitch = tmpl->BytesPerRow;

    for (WORD row = 0; row < height; ++row) {
        const UWORD *src = (UWORD *)bitmap;
        if (!rol) {
            for (WORD col = 0; col < wordsPerLn; ++col) {
                UWORD w = src[col];
                // We set DP_CONFIG_LSB_FIRST.
                ioNS.writeW(IoReg::id_PIX_TRANS, w);

                usedFifoSlots = (usedFifoSlots + 1) & 15;
                if (!usedFifoSlots) {
                    asMach32(bi)->waitFifo(16);
                }
            }
        } else {
            for (WORD col = 0; col < wordsPerLn; ++col) {
                UWORD w0 = src[col];
                UWORD w1 = src[col + 1];
                UWORD w  = (w0 << rol) | (w1 >> (16u - rol));
                // We set DP_CONFIG_LSB_FIRST.
                ioNS.writeW(IoReg::id_PIX_TRANS, w);

                usedFifoSlots = (usedFifoSlots + 1) & 15;
                if (!usedFifoSlots) {
                    asMach32(bi)->waitFifo(16);
                }
            }
        }
        bitmap += bitmapPitch;
    }

    if (!usedFifoSlots) {
        asMach32(bi)->waitFifo(1);
    }
    // io.writeW(IoReg::id_SCISSOR_LEFT, 0);
    io.writeW(IoReg::id_SCISSOR_RIGHT, 0x600);
    flushWrites();
}

static void performBlitPlanar2ChunkyBlits(BoardInfo_t *bi, WORD dstX, WORD dstY, WORD width, WORD height,
                                          const UBYTE *bitmap, WORD bmPitch, UWORD rol)
{
    rol &= 15u;
    /* 16 pixels per PIX_TRANS word. Round up width+offset to 16. */
    WORD blitWidth = (width + rol + 15) & ~15;
    drawRect(bi, dstX, dstY, blitWidth, height);

    WORD wordsPerLn    = blitWidth >> 4;
    ULONG fifoNeed     = (ULONG)wordsPerLn * (ULONG)height + 3u;
    UBYTE numFifoSlots = fifoNeed > 16u ? 16 : (UBYTE)fifoNeed;
    asMach32(bi)->waitFifo(numFifoSlots);
    WORD usedFifoSlots = 16 - numFifoSlots;

    DRIVER_LOCALS(bi);
    if ((ULONG)bitmap == 0) {
        // FIXME: use blitter fill instead
        for (WORD row = 0; row < height; ++row) {
            for (WORD col = 0; col < wordsPerLn; ++col) {
                ioNS.writeW(IoReg::id_PIX_TRANS, 0);
                usedFifoSlots = (usedFifoSlots + 1) & 15;
                if (!usedFifoSlots) {
                    asMach32(bi)->waitFifo(16);
                }
            }
        }
    } else if ((ULONG)bitmap == 0xFFFFFFFFu) {
        // FIXME: use blitter fill instead
        for (WORD row = 0; row < height; ++row) {
            for (WORD col = 0; col < wordsPerLn; ++col) {
                ioNS.writeW(IoReg::id_PIX_TRANS, 0xFFFF);
                usedFifoSlots = (usedFifoSlots + 1) & 15;
                if (!usedFifoSlots) {
                    asMach32(bi)->waitFifo(16);
                }
            }
        }
    } else {
        for (WORD row = 0; row < height; ++row) {
            const UWORD *src = (UWORD *)bitmap;
            if (!rol) {
                for (UWORD col = 0; col < wordsPerLn; ++col) {
                    UWORD w = src[col];
                    ioNS.writeW(IoReg::id_PIX_TRANS, w);

                    usedFifoSlots = (usedFifoSlots + 1) & 15;
                    if (!usedFifoSlots) {
                        asMach32(bi)->waitFifo(16);
                    }
                }
            } else {
                for (UWORD col = 0; col < wordsPerLn; ++col) {
                    UWORD w0 = src[col];
                    UWORD w1 = src[col + 1];
                    UWORD w  = (w0 << rol) | (w1 >> (16u - rol));
                    ioNS.writeW(IoReg::id_PIX_TRANS, w);

                    usedFifoSlots = (usedFifoSlots + 1) & 15;
                    if (!usedFifoSlots) {
                        asMach32(bi)->waitFifo(16);
                    }
                }
            }
            bitmap += bmPitch;
        }
    }
}

void ASM Mach32Driver::blitPlanar2Chunky(__REGA1(struct BitMap *bm), __REGA2(struct RenderInfo *ri),
                                         __REGD0(SHORT srcX), __REGD1(SHORT srcY), __REGD2(SHORT dstX),
                                         __REGD3(SHORT dstY), __REGD4(SHORT width), __REGD5(SHORT height),
                                         __REGD6(UBYTE minTerm), __REGD7(UBYTE mask))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nsrcX %ld, srcY %ld, dstX %ld, dstY %ld, w %ld, h %ld"
          "\nmask 0x%lx minTerm %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)srcX, (ULONG)srcY, (ULONG)dstX, (ULONG)dstY, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)minTerm,
          (ULONG)ri->BytesPerRow, (ULONG)ri->Memory);

    if (width < 64 || height < 64) {
        bi->BlitPlanar2ChunkyDefault(bi, bm, ri, srcX, srcY, dstX, dstY, width, height, minTerm, mask);
        return;
    }

    ChipData_t *cd = getChipData(bi);
    if (cd->GEOp != BLITPLANAR2CHUNKY) {
        cd->GEOp       = BLITPLANAR2CHUNKY;
        cd->GEdrawMode = 0xFF;
        cd->GEmask     = 0xFF;
        cd->GEfgPen    = ~0UL;
        cd->GEbgPen    = 0;

        asMach32(bi)->waitFifo(4);
        DRIVER_LOCALS(bi);
        io.writeW(IoReg::id_DP_CONFIG, DP_CONFIG_TEMPLATE);
        io.writeW(IoReg::id_WRT_MASK, 0xFFFF);
        io.writeW(IoReg::id_FRGD_COLOR, 0xFF);
        io.writeW(IoReg::id_BKGD_COLOR, 0x00);
    }

    UBYTE mix = minTermToMix[minTerm & 0xF];
    if (cd->GEdrawMode != minTerm) {
        cd->GEdrawMode = minTerm;
        asMach32(bi)->waitFifo(2);
        DRIVER_LOCALS(bi);
        io.writeW(IoReg::id_FRGD_MIX, (UWORD)(CLR_SRC_FRGD_COLOR | mix));
        io.writeW(IoReg::id_BKGD_MIX, (UWORD)(CLR_SRC_BKGD_COLOR | mix));
    }

    setFarBlitBuffer(bi, ri, RGBFB_CLUT, 0);

    asMach32(bi)->waitFifo(1);
    DRIVER_LOCALS(bi);

    /* Clip padding (and avoid CPU bit-rotation into left margin). */
    io.writeW(IoReg::id_SCISSOR_RIGHT, dstX + width - 1);

    WORD bmPitch        = bm->BytesPerRow;
    ULONG bmStartOffset = (ULONG)(srcY * bmPitch) + (ULONG)((srcX >> 4) * 2);
    UWORD rol           = (UWORD)(srcX & 15);

    for (UBYTE p = 0; p < bm->Depth; ++p) {
        UBYTE writeMask = (UBYTE)(1u << p);
        if (!(mask & writeMask)) {
            continue;
        }

        setGEWriteMask(bi, writeMask, RGBFB_CLUT, 0);

        const UBYTE *bitmap = (const UBYTE *)bm->Planes[p];
        if ((ULONG)bitmap != 0 && (ULONG)bitmap != 0xFFFFFFFFu) {
            bitmap += bmStartOffset;
        }
        performBlitPlanar2ChunkyBlits(bi, dstX, dstY, width, height, bitmap, bmPitch, rol);
    }

    asMach32(bi)->waitFifo(1);
    io.writeW(IoReg::id_SCISSOR_RIGHT, 0x600);
    flushWrites();
}

static INLINE void REGARGS rotate8x8MonoPattern(UBYTE rows[8], UBYTE offX, UBYTE offY)
{
    if (offX & 7u) {
        UBYTE r = offX & 7u;
        for (UBYTE y = 0; y < 8; ++y) {
            UBYTE b = rows[y];
            rows[y] = (UBYTE)((b >> r) | (b << (8u - r)));
        }
    }

    if (offY & 7u) {
        UBYTE r = offY & 7u;
        UBYTE tmp[8];
        for (UBYTE y = 0; y < 8; ++y) {
            tmp[(y + r) & 7u] = rows[y];
        }
        for (UBYTE y = 0; y < 8; ++y) {
            rows[y] = tmp[y];
        }
    }
}

static INLINE UWORD REGARGS rotate16(UWORD v, UBYTE r)
{
    r &= 15u;
    if (!r) {
        return v;
    }
    return (UWORD)((v << r) | (v >> (16u - r)));
}

static void REGARGS BlitPatternNon8x8(BoardInfo_t *bi, struct RenderInfo *ri, struct Pattern *pattern, WORD x, WORD y,
                                      WORD width, WORD height, UBYTE mask, RGBFTYPE_REG fmt)
{
    setFarBlitBuffer(bi, ri, fmt, 0);

    DRIVER_LOCALS(bi);

    ChipData_t *cd = getChipData(bi);
    if (cd->GEOp != BLITTEMPLATE) {
        cd->GEOp       = BLITTEMPLATE;
        cd->GEdrawMode = 0xFF;

        asMach32(bi)->waitFifo(1);
        io.writeW(IoReg::id_DP_CONFIG, DP_CONFIG_TEMPLATE);
    }

    setDrawMode(bi, pattern->FgPen, pattern->BgPen, pattern->DrawMode, fmt);
    setGEWriteMask(bi, mask, fmt, 1);

    /* Clip padding from our 16px expansion. */
    io.writeW(IoReg::id_SCISSOR_RIGHT, x + width - 1);

    UWORD patternHeight = (UWORD)(1u << pattern->Size);
    const UWORD *src    = (const UWORD *)pattern->Memory;

    UBYTE pattOffX = pattern->XOffset;
    UWORD pattOffY = pattern->YOffset;

    WORD blitWidth = (width + 15) & ~15;
    drawRect(bi, x, y, blitWidth, height);

    WORD wordsPerLn    = blitWidth >> 4;
    ULONG fifoNeed     = (ULONG)wordsPerLn * (ULONG)height + 3u;
    UBYTE numFifoSlots = fifoNeed > 16u ? 16 : (UBYTE)fifoNeed;
    asMach32(bi)->waitFifo(numFifoSlots);
    WORD usedFifoSlots = 16 - numFifoSlots;

    for (WORD row = 0; row < height; ++row) {
        UWORD patRow = src[(pattOffY + row) & (patternHeight - 1u)];
        UWORD w      = rotate16(patRow, pattOffX);
        for (WORD col = 0; col < wordsPerLn; ++col) {
            ioNS.writeW(IoReg::id_PIX_TRANS, w);

            usedFifoSlots = (usedFifoSlots + 1) & 15;
            if (!usedFifoSlots) {
                asMach32(bi)->waitFifo(16);
            }
        }
    }

    if (!usedFifoSlots) {
        asMach32(bi)->waitFifo(1);
    }
    io.writeW(IoReg::id_SCISSOR_RIGHT, 0x600);
    flushWrites();
}

void ASM Mach32Driver::blitPattern(__REGA1(struct RenderInfo *ri), __REGA2(struct Pattern *pattern), __REGD0(WORD x),
                                   __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                                   __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE,
          "\nx %ld, y %ld, w %ld, h %ld\nmask 0x%lx fmt %ld\n"
          "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
          (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)mask, (ULONG)fmt, (ULONG)ri->BytesPerRow,
          (ULONG)ri->Memory);

    UBYTE bpp = getBPP(fmt);
    if (bpp > 2 || pattern->Size > 8) {
        bi->BlitPatternDefault(bi, ri, pattern, x, y, width, height, mask, AS_RGBF(fmt));
        return;
    }

    UWORD patternHeight = (UWORD)(1u << pattern->Size);
    const UWORD *src    = (const UWORD *)pattern->Memory;

    BOOL is8x8 = TRUE;
    for (UWORD i = 0; i < patternHeight; ++i) {
        UWORD row = src[i];
        // Left and right byte (8 pixels) must match, as well as vertically
        if ((UBYTE)row != (UBYTE)(row >> 8) || row != src[i & 7]) {
            is8x8 = FALSE;
            break;
        }
    }
    if (!is8x8) {
        BlitPatternNon8x8(bi, ri, pattern, x, y, width, height, mask, fmt);
        return;
    }

    ChipData_t *cd = getChipData(bi);

    if (cd->GEOp != BLITPATTERN) {
        cd->GEOp       = BLITPATTERN;
        cd->GEdrawMode = 0xFF;
        // Operations other that pattern blits will disturb the pattern registers, so we can't assume the pattern we
        // last uploaded is stil there.
        cd->patternCacheKey = 0xFFFFFFFFu;

        asMach32(bi)->waitFifo(2);
        DRIVER_LOCALS(bi);
        io.writeW(IoReg::id_DP_CONFIG, DP_CONFIG_MONO_PATTERN);
        // 8x8 Mono Pattern Enable
        io.writeW(IoReg::id_PATT_LENGTH, BIT(7));
    }

    setFarBlitBuffer(bi, ri, fmt, 0);
    setDrawMode(bi, pattern->FgPen, pattern->BgPen, pattern->DrawMode, fmt);
    setGEWriteMask(bi, mask, fmt, 0);

    UBYTE pattOffX = (UBYTE)((x - pattern->XOffset) & 7);
    UBYTE pattOffY = (UBYTE)((y - pattern->YOffset) & 7);

    ULONG cacheKey = 0x80000000UL | ((ULONG)pattern->Size) | ((ULONG)pattOffX << 8) | ((ULONG)pattOffY << 16);
    BOOL changed   = (cacheKey != cd->patternCacheKey);

    UBYTE rows[8] = {0};
    for (UBYTE i = 0; i < 8; ++i) {
        UBYTE srcRow = src[i & (patternHeight - 1)];
        rows[i]      = srcRow;
    }
    rotate8x8MonoPattern(rows, pattOffX, pattOffY);

    for (UBYTE i = 0; i < 8; ++i) {
        if (cd->patternCache[i] != rows[i]) {
            cd->patternCache[i] = rows[i];
            changed             = TRUE;
        }
    }

    if (changed) {
        cd->patternCacheKey = cacheKey;

        asMach32(bi)->waitFifo(5);
        DRIVER_LOCALS(bi);

        /* Load PATT_DATA_10..17 via PATT_DATA_INDEX, then enable 8x8 mono pattern mode (PATT_LENGTH[7]). */
        io.writeW(IoReg::id_PATT_DATA_INDEX, 0x10);
        /*
         * Empirically, some Mach32 variants appear to interpret the two bytes of each PATT_DATA word as two
         * successive 8-bit pattern rows in 8x8 mode (low byte first).
         * So pack two 8-bit rows per register word: low=row0, high=row1, etc.
         */
        UWORD *pattData = (UWORD *)cd->patternCache;
        for (UBYTE i = 0; i < 4; ++i) {
            ioNS.writeW(IoReg::id_PATT_DATA, pattData[i]);
        }
    }

    drawRect(bi, x, y, width, height);
}

void ASM Mach32Driver::drawLine(__REGA1(struct RenderInfo *ri), __REGA2(struct Line *line), __REGD0(UBYTE mask),
                                __REGD7(RGBFTYPE_REG fmt))
{
    BoardInfo *bi = this;
    DFUNC(VERBOSE, "\n");

    UBYTE bpp = getBPP(fmt);
    if (bpp > 2) {
        bi->DrawLineDefault(bi, ri, line, mask, AS_RGBF(fmt));
        return;
    }

    setFarBlitBuffer(bi, ri, fmt, 0);

    ChipData_t *cd = getChipData(bi);
    if (cd->GEOp != LINE) {
        cd->GEOp            = LINE;
        cd->lineMode        = 0xFF;
        cd->patternCacheKey = 0x0000;

        asMach32(bi)->waitFifo(2);
        DRIVER_LOCALS(bi);
        /* Disable special pre-clip modes by default. */
        io.writeW(IoReg::id_PATT_LENGTH, (UWORD)PATT_LENGTH_MONO16);
        io.writeW(IoReg::id_DP_CONFIG, DP_CONFIG_REPLACE);
    }

    setDrawMode(bi, line->FgPen, line->BgPen, line->DrawMode, fmt);
    setGEWriteMask(bi, mask, fmt, 0);

    DRIVER_LOCALS(bi);

    BOOL solid = (line->LinePtrn == 0xFFFFu);
    if (solid) {
        if (cd->lineMode != 1) {
            cd->lineMode = 1;
            asMach32(bi)->waitFifo(9);
            DRIVER_LOCALS(bi);
            asMach32(bi)->writeBee8(PIXEL_CNTL, MASK_BIT_SRC_ONE);
        }
    } else {
        UBYTE phase  = (UBYTE)((15u - (line->PatternShift & 15u)) & 15u);
        BOOL needPat = (cd->linePatternCache != line->LinePtrn);
        if (cd->lineMode != 0 || needPat) {
            cd->lineMode         = 0;
            cd->linePatternCache = line->LinePtrn;
            asMach32(bi)->waitFifo(11);
            DRIVER_LOCALS(bi);
            asMach32(bi)->writeBee8(PIXEL_CNTL, MASK_BIT_SRC_PATTEN);
            io.writeW(IoReg::id_PATT_DATA_INDEX, 0x10);
            ioNS.writeW(IoReg::id_PATT_DATA, line->LinePtrn);
            io.writeW(IoReg::id_PATT_INDEX, (UWORD)phase);
        } else {
            asMach32(bi)->waitFifo(8);
            io.writeW(IoReg::id_PATT_INDEX, (UWORD)phase);
        }
    }

    io.writeW(IoReg::id_CUR_X, line->X);
    io.writeW(IoReg::id_CUR_Y, line->Y);

    WORD absMAX = myabs(line->lDelta);
    WORD absMIN = myabs(line->sDelta);

    WORD axialStep = 2 * absMIN;
    io.writeW(IoReg::id_SRC_Y_DEST_Y, axialStep); /* DESTY_AXSTP */
    WORD diagStep = axialStep - 2 * absMAX;
    io.writeW(IoReg::id_SRC_X_DEST_X, diagStep); /* DESTX_DIASTP */
    WORD errTerm = axialStep - absMAX;
    io.writeW(IoReg::id_ERR_TERM, errTerm /*(UWORD)line->twoSDminusLD*/);
    UWORD octant = 0;
    if (line->dX > 0) {
        octant |= LINEDRAW_OPT_OCTANT_XDIR;
    }
    if (line->dY > 0) {
        octant |= LINEDRAW_OPT_OCTANT_YDIR;
    }
    if (!line->Horizontal) {
        octant |= LINEDRAW_OPT_OCTANT_YMAJ;
    }
    io.writeW(IoReg::id_LINEDRAW_OPT, octant); /* DIR_TYPE=0 (Bresenham/Octant), LAST_PEL_OFF=0 */
    UWORD count = line->Length;
    io.writeW(IoReg::id_BRES_COUNT, count); /* kick off the line drawing */
    flushWrites();
}

static ULONG probeFramebufferSize(BoardInfo_t *bi)
{
    volatile UBYTE *base = (volatile UBYTE *)bi->MemoryBase;

    LOCAL_SYSBASE();
    DRIVER_LOCALS(bi);

    ULONG lastSize = 0;

    for (int i = 0; i < 4; ++i) {
        ULONG size = 1 << (19 + i);

        io.writeMaskW(IoReg::id_MISC_OPTIONS, MEM_SIZE_ALIAS_MASK, MEM_SIZE_ALIAS(i));

        volatile ULONG *p0 = (volatile ULONG *)(base + 0);
        volatile ULONG *p1 = (volatile ULONG *)(base + lastSize);
        volatile ULONG *p2 = (volatile ULONG *)(base + size) - 1;  // last DWORD;
        ULONG p0Val        = (ULONG)(base + lastSize);
        *p0                = p0Val;
        *p1                = 0xCAFEBABE;
        *p2                = 0xBAADF00D;

        CacheClearU();

        ULONG p0chk = *p0;
        ULONG p1chk = *p1;
        ULONG p2chk = *p2;

        if ((lastSize && p0chk != p0Val) || p1chk != 0xCAFEBABE || p2chk != 0xBAADF00D) {
            DFUNC(INFO, "VRAM probe: pattern at offset %lu not visible (got 0x%08lX/ 0x%08lX)\n", (ULONG)size,
                  (ULONG)p0chk, (ULONG)p1chk);
            // Current size failed, return the last size which worked.
            if (i > 0) {
                // Reset memSize to last size that worked. I noticed that the MEM_SIZE_ALIAS needs to match
                // the size of the VRAM, it cannot be larger. Maybe this ergister is used to control the addressing
                // scheme for the physical chips on board.
                io.writeMaskW(IoReg::id_MISC_OPTIONS, MEM_SIZE_ALIAS_MASK, MEM_SIZE_ALIAS(i - 1));
            }
            return lastSize;
        }

        lastSize = size;
    }

    return lastSize;
}

static void logMemoryInfo(BoardInfo_t *bi)
{
    DRIVER_LOCALS(bi);

    UWORD mem = io.readW(IoReg::id_MEM_CFG);
    UWORD sub = io.readW(IoReg::id_SUBSYS_STATUS);

    ULONG sel = mem & MEM_APERT_SEL_MASK;
    ULONG loc = (mem & MEM_APERT_LOC_MASK) >> MEM_APERT_LOC_SHIFT;

    const char *apStr[] = {"off", "1MB", "4MB"};

    D(ALWAYS, "MEM_CFG=0x%04lX: aperture %s, MEM_APERT_LOC=0x%02lx (1MB page index)\n", (ULONG)mem, apStr[sel], loc);
    D(ALWAYS, "SUBSYS_STATUS[MEM_SIZE strap]: %s\n", (sub & SUBSYS_MEMSIZE_BIT) ? "1M-class" : "512K-class");
    D(ALWAYS, "Framebuffer probe: populated VRAM ~%lu KB \n", bi->MemorySize / 1024UL);
}

/* P96 BoardInfo entry stubs */

static void ASM WaitBlitter(__REGA0(struct BoardInfo *bi))
{
    asMach32(bi)->waitBlitter();
}
static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    asMach32(bi)->setWriteMask(mask);
}
static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    asMach32(bi)->setClearMask(mask);
}
static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    asMach32(bi)->setReadPlane(mask);
}
static void ASM SetSpriteColor(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE idx), __REGD1(UBYTE r), __REGD2(UBYTE g),
                               __REGD3(UBYTE b), __REGD7(RGBFTYPE fmt))
{
    asMach32(bi)->setSpriteColor(idx, r, g, b, fmt);
}
static void ASM SetSpriteImage(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE fmt))
{
    asMach32(bi)->setSpriteImage(fmt);
}
static BOOL ASM SetSprite(__REGA0(struct BoardInfo *bi), __REGD0(BOOL show), __REGD7(RGBFTYPE fmt))
{
    return asMach32(bi)->setSprite(show, fmt);
}
static void ASM SetSpritePosition(__REGA0(struct BoardInfo *bi), __REGD0(WORD xpos), __REGD1(WORD ypos),
                                  __REGD7(RGBFTYPE fmt))
{
    asMach32(bi)->setSpritePosition(xpos, ypos, fmt);
}
static void ASM SetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    asMach32(bi)->setGC(mi, border);
}
static void ASM SetPanning(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD3(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset),
                           __REGD7(RGBFTYPE format))
{
    asMach32(bi)->setPanning(memory, width, height, xoffset, yoffset, format);
}
static UWORD ASM CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                      __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE format))
{
    return asMach32(bi)->calculateBytesPerRow(width, height, mi, format);
}
static APTR ASM AllocCardMem(__REGA0(struct BoardInfo *bi), __REGD0(ULONG size), __REGD1(BOOL force),
                             __REGD2(BOOL system), __REGD3(ULONG bytesperrow), __REGA1(struct ModeInfo *mi),
                             __REGD7(RGBFTYPE format))
{
    return asMach32(bi)->allocCardMem(size, force, system, bytesperrow, mi, format);
}
static APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi), __REGA1(APTR mem), __REGD0(struct RenderInfo *ri),
                                __REGD7(RGBFTYPE format))
{
    return asMach32(bi)->calculateMemory(mem, ri, format);
}
static ULONG ASM GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    return asMach32(bi)->getCompatibleFormats(format);
}
static void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD0(UWORD region), __REGD7(RGBFTYPE format))
{
    asMach32(bi)->setDAC(region, format);
}
static void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    asMach32(bi)->setColorArray(startIndex, count);
}
static BOOL ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    return asMach32(bi)->setDisplay(state);
}
static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    asMach32(bi)->setMemoryMode(format);
}
static LONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi),
                                  __REGD0(ULONG pixelClock), __REGD7(RGBFTYPE RGBFormat))
{
    return asMach32(bi)->resolvePixelClock(mi, pixelClock, RGBFormat);
}
static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD7(RGBFTYPE format))
{
    return asMach32(bi)->getPixelClock(mi, index, format);
}
static void ASM SetClock(__REGA0(struct BoardInfo *bi))
{
    asMach32(bi)->setClock();
}
static BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL expected))
{
    return asMach32(bi)->getVSyncState(expected);
}
static BOOL ASM SetInterrupt(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    return asMach32(bi)->setInterrupt(state);
}

extern "C" ULONG ASM interruptServer(__REGA1(struct BoardInfo *bi))
{
    return asMach32(bi)->interruptServer();
}
DEFINE_INTSERVER(interruptServerTrampoline, interruptServer);

static void ASM SetDPMSLevel(__REGA0(struct BoardInfo *bi), __REGD0(ULONG level))
{
    asMach32(bi)->setDPMSLevel(level);
}
static void ASM WaitVerticalSync(__REGA0(struct BoardInfo *bi), __REGD0(BOOL end))
{
    asMach32(bi)->waitVerticalSync(end);
}
static ULONG ASM GetVBeamPos(__REGA0(struct BoardInfo *bi))
{
    return asMach32(bi)->getVBeamPos();
}
static void ASM FillRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                         __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(ULONG pen),
                         __REGD5(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asMach32(bi)->fillRect(ri, x, y, width, height, pen, mask, fmt);
}
static void ASM InvertRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                           __REGD1(WORD y), __REGD2(WORD width), __REGD3(WORD height), __REGD4(UBYTE mask),
                           __REGD7(RGBFTYPE fmt))
{
    asMach32(bi)->invertRect(ri, x, y, width, height, mask, fmt);
}
static void ASM BlitRect(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGD0(WORD srcX),
                         __REGD1(WORD srcY), __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                         __REGD5(WORD height), __REGD6(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asMach32(bi)->blitRect(ri, srcX, srcY, dstX, dstY, width, height, mask, fmt);
}
static void ASM BlitRectNoMaskComplete(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *sri),
                                       __REGA2(struct RenderInfo *dri), __REGD0(WORD srcX), __REGD1(WORD srcY),
                                       __REGD2(WORD dstX), __REGD3(WORD dstY), __REGD4(WORD width),
                                       __REGD5(WORD height), __REGD6(UBYTE opCode), __REGD7(RGBFTYPE fmt))
{
    asMach32(bi)->blitRectNoMaskComplete(sri, dri, srcX, srcY, dstX, dstY, width, height, opCode, fmt);
}
static void ASM BlitTemplate(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                             __REGA2(struct Template *tmpl), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                             __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asMach32(bi)->blitTemplate(ri, tmpl, x, y, width, height, mask, fmt);
}
static void ASM BlitPattern(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri),
                            __REGA2(struct Pattern *pattern), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD width),
                            __REGD3(WORD height), __REGD4(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asMach32(bi)->blitPattern(ri, pattern, x, y, width, height, mask, fmt);
}
static void ASM BlitPlanar2Chunky(__REGA0(struct BoardInfo *bi), __REGA1(struct BitMap *bm),
                                  __REGA2(struct RenderInfo *ri), __REGD0(SHORT srcX), __REGD1(SHORT srcY),
                                  __REGD2(SHORT dstX), __REGD3(SHORT dstY), __REGD4(SHORT width), __REGD5(SHORT height),
                                  __REGD6(UBYTE minTerm), __REGD7(UBYTE mask))
{
    asMach32(bi)->blitPlanar2Chunky(bm, ri, srcX, srcY, dstX, dstY, width, height, minTerm, mask);
}
static void ASM DrawLine(__REGA0(struct BoardInfo *bi), __REGA1(struct RenderInfo *ri), __REGA2(struct Line *line),
                         __REGD0(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
    asMach32(bi)->drawLine(ri, line, mask, fmt);
}
BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    DFUNC(ALWAYS, "\n");

    DRIVER_LOCALS(bi);

    {
        ChipData_t *cd      = getChipData(bi);
        cd->GEfgPen         = ~0UL;
        cd->GEbgPen         = 0;
        cd->GEmask          = 0xFF;
        cd->GEdrawMode      = 0xFF;
        cd->GEOp            = 0; /* BlitterOp_t None */
        cd->GEfmt           = ~0;
        cd->fifoSlotsCached = 0xffff; /* no free slots known yet */
        cd->patternCacheKey = 0xFFFFFFFFu;
        for (int i = 0; i < 8; ++i) {
            cd->patternCache[i] = 0;
        }
        cd->lineMode = 0xFF;
        myMemset(&cd->srcDstRenderInfoCache, 0, sizeof(cd->srcDstRenderInfoCache));
    }

    bi->GraphicsControllerType = GCT_ATIRV100;
    bi->PaletteChipType        = PCT_Unknown;
    bi->ChipFlags |= 1; /* display on until SetDisplay(FALSE) */
    bi->Flags |= BIF_GRANTDIRECTACCESS | BIF_BLITTER | BIF_HARDWARESPRITE;
    bi->RGBFormats = MACH32_SUPPORTED_RGBFF;

    getCardData(bi)->AllocCardMemDefault = bi->AllocCardMem;
    P96_HOOK(bi->AllocCardMem, AllocCardMem);
    P96_HOOK(bi->SetGC, SetGC);
    P96_HOOK(bi->SetPanning, SetPanning);
    P96_HOOK(bi->CalculateBytesPerRow, CalculateBytesPerRow);
    P96_HOOK(bi->CalculateMemory, CalculateMemory);
    P96_HOOK(bi->GetCompatibleFormats, GetCompatibleFormats);
    P96_HOOK(bi->SetDAC, SetDAC);
    P96_HOOK(bi->SetColorArray, SetColorArray);
    P96_HOOK(bi->SetDisplay, SetDisplay);
    P96_HOOK(bi->SetMemoryMode, SetMemoryMode);
    P96_HOOK(bi->SetWriteMask, SetWriteMask);
    P96_HOOK(bi->SetReadPlane, SetReadPlane);
    P96_HOOK(bi->SetClearMask, SetClearMask);
    P96_HOOK(bi->ResolvePixelClock, ResolvePixelClock);
    P96_HOOK(bi->GetPixelClock, GetPixelClock);
    P96_HOOK(bi->SetClock, SetClock);
    P96_HOOK(bi->SetDPMSLevel, SetDPMSLevel);
    P96_HOOK(bi->WaitVerticalSync, WaitVerticalSync);
    P96_HOOK(bi->GetVSyncState, GetVSyncState);
    P96_HOOK(bi->SetInterrupt, SetInterrupt);
    bi->HardInterrupt.is_Code = (void (*)())interruptServerTrampoline;

    P96_HOOK(bi->SetSprite, SetSprite);
    P96_HOOK(bi->SetSpritePosition, SetSpritePosition);
    P96_HOOK(bi->SetSpriteImage, SetSpriteImage);
    P96_HOOK(bi->SetSpriteColor, SetSpriteColor);
    P96_HOOK(bi->WaitBlitter, WaitBlitter);
    P96_HOOK(bi->BlitRect, BlitRect);
    P96_HOOK(bi->BlitRectNoMaskComplete, BlitRectNoMaskComplete);
    P96_HOOK(bi->InvertRect, InvertRect);
    P96_HOOK(bi->FillRect, FillRect);
    P96_HOOK(bi->BlitTemplate, BlitTemplate);
    P96_HOOK(bi->BlitPlanar2Chunky, BlitPlanar2Chunky);
    P96_HOOK(bi->DrawLine, DrawLine);
    P96_HOOK(bi->BlitPattern, BlitPattern);
    bi->MaxBMWidth  = 1536;
    bi->MaxBMHeight = 1536;

    bi->BitsPerCannon          = 6;
    bi->MaxHorValue[PLANAR]    = MACH32_MAX_PITCH_PIXELS;
    bi->MaxHorValue[CHUNKY]    = MACH32_MAX_PITCH_PIXELS;
    bi->MaxHorValue[HICOLOR]   = MACH32_MAX_PITCH_PIXELS;
    bi->MaxHorValue[TRUECOLOR] = MACH32_MAX_PITCH_PIXELS;
    bi->MaxHorValue[TRUEALPHA] = MACH32_MAX_PITCH_PIXELS;

    bi->MaxVerValue[PLANAR]    = 2047;
    bi->MaxVerValue[CHUNKY]    = 2047;
    bi->MaxVerValue[HICOLOR]   = 2047;
    bi->MaxVerValue[TRUECOLOR] = 2047;
    bi->MaxVerValue[TRUEALPHA] = 2047;

    // CUR_X/Y and DST_X/y are defined as:
    // 1. The Current X Position register is used to detennine the starting X coordinate of all
    //    drawing operations. Values written to this register will be considered to be in the
    //    range (-512 .. 1535).
    // 2. Bit 12 which is normally reserved when performing 8514-compatible drawing
    //    operations is unused when perfonning extended drawing operations.
    bi->MaxHorResolution[PLANAR]    = 1536;
    bi->MaxVerResolution[PLANAR]    = 1536;
    bi->MaxHorResolution[CHUNKY]    = 1536;
    bi->MaxVerResolution[CHUNKY]    = 1536;
    bi->MaxHorResolution[HICOLOR]   = 1536;
    bi->MaxVerResolution[HICOLOR]   = 1536;
    bi->MaxHorResolution[TRUECOLOR] = 1536;
    bi->MaxVerResolution[TRUECOLOR] = 1536;
    bi->MaxHorResolution[TRUEALPHA] = 1536;
    bi->MaxVerResolution[TRUEALPHA] = 1536;

    bi->PixelClockCount[PLANAR]    = PIXEL_CLOCK_INDEX_COUNT;
    bi->PixelClockCount[CHUNKY]    = PIXEL_CLOCK_INDEX_COUNT;
    bi->PixelClockCount[HICOLOR]   = PIXEL_CLOCK_INDEX_COUNT;
    bi->PixelClockCount[TRUECOLOR] = PIXEL_CLOCK_INDEX_COUNT;
    bi->PixelClockCount[TRUEALPHA] = PIXEL_CLOCK_INDEX_COUNT;

    io.writeW(IoReg::id_SCRATCH_PAD0, 0xCCCC);
    UWORD scratch0 = io.readW(IoReg::id_SCRATCH_PAD0);
    io.writeW(IoReg::id_SCRATCH_PAD0, 0x5555);
    UWORD scratch1 = io.readW(IoReg::id_SCRATCH_PAD0);
    if (scratch0 != 0xCCCC || scratch1 != 0x5555) {
        DFUNC(ERROR, "Scratch pad test failed: read 0x%04X and 0x%04X\n", scratch0, scratch1);
        return FALSE;
    }

    // dumpMach32Eeprom(bi);

    UWORD chipId     = io.readW(IoReg::id_CHIP_ID);
    char chipName[3] = {0};
    chipName[1]      = (chipId & 0x1F) + 0x41;
    chipName[0]      = ((chipId >> 5) & 0x1F) + 0x41;
    ULONG chipClass  = (chipId >> 10) & 3;
    ULONG revision   = (chipId >> 12) & 0xF;

    D(ALWAYS, "Chip Version detected: Mach32%s, revision %ld, class 0x%lx\n", chipName, revision, chipClass);

    UWORD configStat1 = io.readW(IoReg::id_CONFIG_STATUS_1);
    ULONG vgaEnabled  = !(configStat1 & BIT(0));
    ULONG busType     = (configStat1 >> 1) & 7;
    ULONG memType     = (configStat1 >> 4) & 7;
    ULONG chipEnabled = !(configStat1 & BIT(7));
    ULONG dacType     = (configStat1 >> 9) & 0x7;

    D(ALWAYS, "CONFIG_STATUS_1: VGA %s, bus type 0x%lx, mem type 0x%lx, chip %s, DAC type 0x%lx\n",
      vgaEnabled ? "enabled" : "disabled", busType, memType, chipEnabled ? "enabled" : "disabled", dacType);

    // DISABLE_VGA, DLY_LATCH_ENA, 16_BIT_IO
    io.writeMaskW(IoReg::id_MISC_OPTIONS, BIT(4) | BIT(5) | BIT(7), BIT(4) | BIT(5) | BIT(7));

    // These are some magic values I got from the BIOS that largely get rid of screen corruption
    io.writeW(IoReg::id_MISC_OPTIONS, (io.readW(IoReg::id_MISC_OPTIONS) & 0x7F) | 0x9080);
    io.writeW(IoReg::id_LOCAL_CNTL, (io.readW(IoReg::id_LOCAL_CNTL) & 0x380) | 0x1401);
    io.writeMaskW(IoReg::id_PCI_CNTL, 0x00FF, 0x00C0);           // enable TARGET_ABORT_EN and PCI_DAC_DLY
    io.writeMaskW(IoReg::id_MAX_WAITSTATES, (UWORD)~0xFBFF, 0);  // reset magic bit
    io.writeMaskW(IoReg::id_MISC_OPTIONS, 0xFF00, 0xF000);       //
    io.writeMaskW(IoReg::id_LOCAL_CNTL, BIT(2), BIT(2));         // Enable SHORT_CAS_PULSE_EN

    UWORD configStat2 = io.readW(IoReg::id_CONFIG_STATUS_2);

    // "Unlock" the Shadow registers. This is not in any way described in the documentation and
    // Not described in the TRM; required for correct CRTC programming.
    // Without it, one cannot program the CRTC correctly and won't get proper display timings.
    io.writeW(IoReg::id_SHADOW_SET, 1);
    io.writeW(IoReg::id_SHADOW_CTL, 0);
    io.writeW(IoReg::id_SHADOW_SET, 2);
    io.writeW(IoReg::id_SHADOW_CTL, 0);
    io.writeW(IoReg::id_SHADOW_SET, 0);

    io.writeW(IoReg::id_CLOCK_SEL, PASS_THROUGH_DISABLE | CLK_SEL(0x4) | CLK_DIV | VFIFO_DEPTH(6));

    // Enable Memory Access
    UWORD memCfg = io.readW(IoReg::id_MEM_CFG);
    D(ALWAYS, "PCI memory aperture at 0x%08lX\n", (ULONG)(memCfg >> 4) << 20);
    io.writeMaskW(IoReg::id_MEM_CFG, 0x03, 0x02);  // Enable 4MB aperture
    // io.writeMaskW(IoReg::id_APERTURE_CNTL, BIT(10) | BIT(11), BIT(10) | BIT(11));  // Zero WaitState write access
    io.writeB(IoReg::id_APERTURE_CNTL, 0);
    io.writeW(IoReg::id_MEM_BNDRY, 0);

    if (!InitRAMDAC(bi, (DACType)dacType)) {
        DFUNC(ERROR, "RAMDAC initialization failed\n");
        return FALSE;
    }

    // Reset Graphics Engine GE, disable Interrupts
    io.writeW(IoReg::id_SUBSYS_CNTL, 0x800f);
    delayMilliSeconds(5);
    io.writeW(IoReg::id_SUBSYS_CNTL, 0x400f);

    io.readW(IoReg::id_SUBSYS_STATUS);

    /* GE_X_CONTROL[1:0] = 10 (reserved), GE_Y_CONTROL[3:2] = 01 (linear) — REG688000-15 §8-17 */
    asMach32(bi)->writeBee8(MEM_CNTL, 0x6);

    asMach32(bi)->writeBee8(SCISSORS_T, 0);
    asMach32(bi)->writeBee8(SCISSORS_L, 0);
    asMach32(bi)->writeBee8(SCISSORS_B, 0x600);
    asMach32(bi)->writeBee8(SCISSORS_R, 0x600);
    io.writeW(IoReg::id_SCISSOR_TOP, 0);
    io.writeW(IoReg::id_SCISSOR_LEFT, 0);
    io.writeW(IoReg::id_SCISSOR_BOTTOM, 0x600);
    io.writeW(IoReg::id_SCISSOR_RIGHT, 0x600);
    asMach32(bi)->writeBee8(PIXEL_CNTL, MASK_BIT_SRC_ONE);

    io.writeW(IoReg::id_DP_CONFIG, DP_CONFIG_REPLACE);
    io.writeW(IoReg::id_ALU_FG_FN, 7);
    io.writeW(IoReg::id_DEST_CMP_FN, 0);
    io.writeW(IoReg::id_WRT_MASK, 0xFFFF);

    {
        ULONG probed = probeFramebufferSize(bi);
        if (!probed) {
            DFUNC(ERROR, "Failed to determine framebuffer size\n");
            return FALSE;
        }
        bi->MemorySize = probed;
    }

    /* 64×64 @ 2 bpp = 1024 bytes. Reserve at end of linear aperture. The hardware cursor base
     * must be DWORD-aligned: CURSOR_OFFSET is programmed in DWORDs from display base (see
     * writeCursorAddress). Aligning the framebuffer end to 64 bytes keeps it DWORD-aligned. */
    const ULONG maxSpriteBuffersSize = (64UL * 64UL * 2UL / 8UL);
    ULONG fbLen                      = bi->MemorySize - maxSpriteBuffersSize;
    fbLen &= ~(63UL); /* 64-byte (and thus DWORD) aligned; required for CURSOR_OFFSET in DWORDs */
    bi->MemorySize       = fbLen;
    bi->MouseImageBuffer = bi->MemoryBase + bi->MemorySize;

    logMemoryInfo(bi);

    DFUNC(ALWAYS, "Initialization complete\n");
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(TESTEXE) && (!defined(__clang__) || defined(__m68k__))

#include <libraries/openpci.h>
#include <proto/dos.h>
#include <proto/openpci.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <settings.h>

extern struct UtilityBase *UtilityBase;

#define PCI_VENDOR_ATI    0x1002
#define PCI_DEVICE_MACH32 0x4158

struct Library *OpenPciBase = NULL;
extern struct Library *DOSBase;

static volatile ULONG softVBlankCount;

static void ASM SoftVBlankCount(__REGA1(ULONG *count))
{
    (*count)++;
}

/* Count VBLANK_INT edges with ENA=0 (no INTA). Separates CRT events from PCI delivery. */
static void testVBlankPoll(BoardInfo_t *bi)
{
    DRIVER_LOCALS(bi);
    LOCAL_SYSBASE();

    Disable();
    io.writeW(IoReg::id_SUBSYS_CNTL, SUBSYS_VBLANK_ACK);
    Enable();

    ULONG edges     = 0;
    ULONG lineMin   = 0x7FFu;
    ULONG lineMax   = 0;
    ULONG syncSeen  = 0;
    ULONG syncClear = 0;

    D(ALWAYS, "VBlank poll: CRT edges (ENA=0) for 2s...\n");
    for (ULONG i = 0; i < 2000; ++i) {
        UWORD st   = ioQ.readW(IoReg::id_SUBSYS_STATUS);
        UWORD line = ioQ.readW(IoReg::id_VERT_LINE_CNTR) & 0x7FF;
        UWORD ds   = ioQ.readW(IoReg::id_DISP_STATUS);

        if (line < lineMin)
            lineMin = line;
        if (line > lineMax)
            lineMax = line;
        if (ds & DISP_STATUS_VERT_SYNC)
            syncSeen++;
        else
            syncClear++;

        if (st & SUBSYS_VBLANK_INT) {
            edges++;
            ioQ.writeW(IoReg::id_SUBSYS_CNTL, SUBSYS_VBLANK_ACK);
        }
        delayMilliSeconds(1);
    }

    D(ALWAYS,
      "VBlank poll: %lu VBLANK_INT edges, VERT_LINE_CNTR min=%lu max=%lu, "
      "DISP_STATUS VERT_SYNC hi=%lu lo=%lu, SUBSYS_STATUS=0x%04lx\n",
      edges, lineMin, lineMax, syncSeen, syncClear, (ULONG)io.readW(IoReg::id_SUBSYS_STATUS));
    if (lineMin == lineMax)
        D(ERROR, "VBlank poll: VERT_LINE_CNTR frozen — CRT not scanning\n");
    if (edges < 50)
        D(ERROR, "VBlank poll: too few VBLANK_INT edges (expected ~120 @60Hz)\n");
}

/* Register PCI VBlank server, enable chip IRQ, count SoftInterrupts for 2s. */
static void testVBlankInterrupt(BoardInfo_t *bi, struct pci_dev *board)
{
    LOCAL_SYSBASE();
    softVBlankCount   = 0;
    hardVBlankEntries = 0;
    hardVBlankHandled = 0;

    {
        ULONG pin = 0, line = 0;
        GetBoardAttrs(board, PRM_InterruptPin, (Tag)&pin, PRM_InterruptLine, (Tag)&line, TAG_END);
        D(ALWAYS, "VBlank IRQ test: PCI INT pin=%lu line=%lu HardInt.is_Code=%p\n", pin, line,
          bi->HardInterrupt.is_Code);
    }

    /* Chip-side first: does VBLANK_INT latch without host IRQ? */
    testVBlankPoll(bi);

    bi->SoftInterrupt.is_Node.ln_Type = NT_INTERRUPT;
    bi->SoftInterrupt.is_Node.ln_Pri  = 0;
    bi->SoftInterrupt.is_Node.ln_Name = (char *)"TestMach32SoftVBlank";
    bi->SoftInterrupt.is_Data         = (APTR)&softVBlankCount;
    bi->SoftInterrupt.is_Code         = (void (*)())SoftVBlankCount;

    bi->HardInterrupt.is_Node.ln_Type = NT_INTERRUPT;
    bi->HardInterrupt.is_Node.ln_Pri  = 0;
    bi->HardInterrupt.is_Node.ln_Name = (char *)"TestMach32VBlank";
    bi->HardInterrupt.is_Data         = bi;
    /* is_Code set by InitChip */

    /*
     * Probe with CPU IRQs off: ENA=1 must still latch VBLANK_INT in STATUS.
     * If it never sets, chip/ENA programming is wrong. If it sets here but PCI
     * softints stay 0, INTA is not reaching the host (or the hard ISR is dead).
     */
    {
        DRIVER_LOCALS(bi);
        ULONG waited = 0;
        UWORD st     = 0;

        Disable();
        io.writeW(IoReg::id_SUBSYS_CNTL, SUBSYS_VBLANK_ACK);
        io.writeW(IoReg::id_SUBSYS_CNTL, SUBSYS_VBLANK_ACK | SUBSYS_VBLANK_ENA);
        for (; waited < 100; ++waited) {
            st = ioQ.readW(IoReg::id_SUBSYS_STATUS);
            if (st & SUBSYS_VBLANK_INT)
                break;
            /* Busy-wait ~1ms without Enable() so OpenPCI cannot ACK. */
            for (volatile ULONG spin = 0; spin < 5000; ++spin)
                ;
        }
        io.writeW(IoReg::id_SUBSYS_CNTL, SUBSYS_VBLANK_ACK); /* ENA=0 again before Enable */
        Enable();

        D(ALWAYS, "VBlank IRQ test: Disable+ENA latch %s after ~%lums (STATUS=0x%04lx)\n",
          (st & SUBSYS_VBLANK_INT) ? "OK" : "FAIL", waited, (ULONG)st);
    }

    /* OpenPCI: server may run immediately — chip must not assert INTA yet. */
    Disable();
    {
        DRIVER_LOCALS(bi);
        io.writeW(IoReg::id_SUBSYS_CNTL, SUBSYS_VBLANK_ACK);
    }
    Enable();

    if (!pci_add_intserver(&bi->HardInterrupt, board)) {
        D(ERROR, "VBlank IRQ test: pci_add_intserver failed\n");
        return;
    }

    bi->Flags |= BIF_VBLANKINTERRUPT;
    hardVBlankEntries = 0;
    hardVBlankHandled = 0;
    softVBlankCount   = 0;
    bi->SetInterrupt(bi, TRUE);

    {
        DRIVER_LOCALS(bi);
        D(ALWAYS, "VBlank IRQ test: SUBSYS_STATUS=0x%04lx — counting softints 2s...\n",
          (ULONG)io.readW(IoReg::id_SUBSYS_STATUS));
    }

    delayMilliSeconds(2000);

    ULONG count   = softVBlankCount;
    ULONG hardEnt = hardVBlankEntries;
    ULONG hardOk  = hardVBlankHandled;
    UWORD stEnd;
    {
        DRIVER_LOCALS(bi);
        stEnd = io.readW(IoReg::id_SUBSYS_STATUS);
    }
    bi->SetInterrupt(bi, FALSE);
    {
        DRIVER_LOCALS(bi);
        io.writeW(IoReg::id_SUBSYS_CNTL, SUBSYS_VBLANK_ACK);
    }
    pci_rem_intserver(&bi->HardInterrupt, board);
    bi->Flags &= ~BIF_VBLANKINTERRUPT;

    D(ALWAYS, "VBlank IRQ test: %lu softints (~%lu Hz), hard entries=%lu handled=%lu, STATUS end=0x%04lx\n", count,
      count / 2, hardEnt, hardOk, (ULONG)stEnd);
    if (hardEnt == 0) {
        D(ERROR, "VBlank IRQ test: hard ISR never entered — PCI INTA not delivered\n");
    } else if (hardOk == 0) {
        D(ERROR, "VBlank IRQ test: hard ISR entered but never saw VBLANK_INT\n");
    } else if (count < 50) {
        D(ERROR, "VBlank IRQ test: hard ISR OK but softints low (Cause/SoftInterrupt?)\n");
    }
}

/* VBLANK/S — args buffer must be long-aligned (static); stack LONGs broke ReadArgs. */
static const char testArgsTemplate[] = "VBLANK/S";
static LONG testArgs[1];

/* Lives for the whole test run; SetGC/SetPanning keep pointers into ModeInfo. */
static struct ModeInfo s_mode640x480;

/*
 * Byte-wide stores: on BE hosts + PCI VRAM, byte enables may not map 1:1 to linear
 * chunky order (you often see 16-bit lane duplication in a hex dump).
 */
static void testFillPattern8bppBytes(BoardInfo_t *bi, UWORD width, UWORD height)
{
    volatile UBYTE *mem = (volatile UBYTE *)bi->MemoryBase;
    UWORD bpr           = bi->CalculateBytesPerRow(bi, width, height, bi->ModeInfo, RGBFB_CLUT);

    /* First 16 lines: horizontal 0-255 ramp (repeating) to judge palette precision */
    UWORD gradientRows = 16;
    if (gradientRows > height)
        gradientRows = height;
    for (UWORD y = 0; y < gradientRows; y++) {
        for (UWORD x = 0; x < width; x++) {
            mem[(ULONG)y * (ULONG)bpr + (ULONG)x] = (UBYTE)(x & 0xFF);
        }
    }

    for (UWORD y = gradientRows; y < height; y++) {
        for (UWORD x = 0; x < width; x++) {
            mem[(ULONG)y * (ULONG)bpr + (ULONG)x] = (UBYTE)(x ^ y);
        }
    }
}

static UBYTE readPixel8(BoardInfo_t *bi, UWORD bpr, UWORD x, UWORD y)
{
    volatile UBYTE *mem = (volatile UBYTE *)bi->MemoryBase;
    return mem[(ULONG)y * (ULONG)bpr + (ULONG)x];
}

static int verifyRect8(BoardInfo_t *bi, UWORD bpr, UWORD rx, UWORD ry, UWORD rw, UWORD rh, UBYTE expected,
                       const char *label)
{
    int errors = 0;
    /* Sample a few points: corners + center */
    static const char *posNames[] = {"TL", "TR", "BL", "BR", "center"};
    UWORD xs[]                    = {rx, (UWORD)(rx + rw - 1), rx, (UWORD)(rx + rw - 1), (UWORD)(rx + rw / 2)};
    UWORD ys[]                    = {ry, ry, (UWORD)(ry + rh - 1), (UWORD)(ry + rh - 1), (UWORD)(ry + rh / 2)};

    for (int i = 0; i < 5; i++) {
        UBYTE got = readPixel8(bi, bpr, xs[i], ys[i]);
        UBYTE bg  = (UBYTE)(xs[i] ^ ys[i]);
        if (got != expected) {
            D(0,
              "  FAIL %s %s (%ld,%ld): expected 0x%02lx, got 0x%02lx (bg was 0x%02lx, ~bg=0x%02lx, bg^pen=0x%02lx)\n",
              label, posNames[i], (ULONG)xs[i], (ULONG)ys[i], (ULONG)expected, (ULONG)got, (ULONG)bg, (ULONG)(UBYTE)~bg,
              (ULONG)(UBYTE)(bg ^ expected));
            errors++;
        }
    }
    if (!errors)
        D(0, "  PASS %s: all 5 samples = 0x%02lx\n", label, (ULONG)expected);
    return errors;
}

static void makeSolidLine(struct Line *line, WORD x0, WORD y0, WORD x1, WORD y1, UBYTE drawMode, ULONG fgPen)
{
    memset(line, 0, sizeof(*line));

    WORD dx = (WORD)(x1 - x0);
    WORD dy = (WORD)(y1 - y0);

    line->X  = x0;
    line->Y  = y0;
    line->dX = dx;
    line->dY = dy;

    WORD adx = myabs(dx);
    WORD ady = myabs(dy);

    line->Horizontal = (adx >= ady);
    line->lDelta     = line->Horizontal ? dx : dy;
    line->sDelta     = line->Horizontal ? dy : dx;

    line->Length = (UWORD)((UWORD)myabs(line->lDelta) + 1u);

    /* Segment start error term (REG688000-15 §9-37). */
    WORD absMAX        = myabs(line->lDelta);
    WORD absMIN        = myabs(line->sDelta);
    line->twoSDminusLD = (WORD)(2 * absMIN - absMAX - ((dx > 0) ? 1 : 0));

    line->LinePtrn     = 0xFFFF;
    line->PatternShift = 0;
    line->FgPen        = fgPen;
    line->BgPen        = 0;
    line->DrawMode     = drawMode;
    line->Xorigin      = x0;
    line->Yorigin      = y0;
}

static void makePatternLine(struct Line *line, WORD x0, WORD y0, WORD x1, WORD y1, UBYTE drawMode, ULONG fgPen,
                            ULONG bgPen, UWORD linePtrn, UWORD patternShift)
{
    memset(line, 0, sizeof(*line));

    WORD dx = (WORD)(x1 - x0);
    WORD dy = (WORD)(y1 - y0);

    line->X  = x0;
    line->Y  = y0;
    line->dX = dx;
    line->dY = dy;

    WORD adx = myabs(dx);
    WORD ady = myabs(dy);

    line->Horizontal = (adx >= ady);
    line->lDelta     = line->Horizontal ? dx : dy;
    line->sDelta     = line->Horizontal ? dy : dx;
    line->Length     = (UWORD)((UWORD)myabs(line->lDelta) + 1u);

    WORD absMaj        = myabs(line->lDelta);
    WORD absMin        = myabs(line->sDelta);
    line->twoSDminusLD = (WORD)(2 * absMin - absMaj - ((dx > 0) ? 1 : 0));

    line->LinePtrn     = linePtrn;
    line->PatternShift = (UWORD)(patternShift & 15u);
    line->FgPen        = fgPen;
    line->BgPen        = bgPen;
    line->DrawMode     = drawMode;
    line->Xorigin      = x0;
    line->Yorigin      = y0;
}

static int verifyPixel8(BoardInfo_t *bi, UWORD bpr, UWORD x, UWORD y, UBYTE expected, const char *label)
{
    UBYTE got = readPixel8(bi, bpr, x, y);
    if (got != expected) {
        D(0, "  FAIL %s (%ld,%ld): expected 0x%02lx, got 0x%02lx\n", label, (ULONG)x, (ULONG)y, (ULONG)expected,
          (ULONG)got);
        return 1;
    }
    D(0, "  PASS %s (%ld,%ld): 0x%02lx\n", label, (ULONG)x, (ULONG)y, (ULONG)got);
    return 0;
}

static UBYTE expected8x8MonoPen(const UWORD patt[8], UWORD baseX, UWORD baseY, UWORD x, UWORD y, UBYTE fg, UBYTE bg)
{
    UBYTE row = (UBYTE)patt[(y - baseY) & 7u];
    /* MSB = leftmost pixel (matches Template tests). */
    UBYTE bit = (UBYTE)(row >> (7u - ((x - baseX) & 7u))) & 1u;
    return bit ? fg : bg;
}

static void testBlitPattern8x8Mono(BoardInfo_t *bi)
{
    struct RenderInfo ri;
    memset(&ri, 0, sizeof(ri));
    ri.Memory      = bi->MemoryBase;
    ri.BytesPerRow = bi->CalculateBytesPerRow(bi, 640, 480, bi->ModeInfo, RGBFB_CLUT);
    ri.RGBFormat   = RGBFB_CLUT;

    if (!bi->BlitPattern) {
        D(0, "TestMach32: BlitPattern not installed; skipping pattern test\n");
        return;
    }

    /* 8x8 mono pattern: 16-bit rows with identical bytes -> triggers 8x8 detection. */
    static UWORD pat8x8[8] = {
        0x8080, 0x4040, 0x2020, 0x1010, 0x0808, 0x0404, 0x0202, 0x0101,
    };

    struct Pattern patt;
    patt.Memory   = pat8x8;
    patt.Size     = 3; /* 8 rows */
    patt.DrawMode = JAM2;
    patt.FgPen    = 0xFF;
    patt.BgPen    = 0x00;
    patt.XOffset  = 0;
    patt.YOffset  = 0;

    /* Paint a solid backdrop so 0 bits are obvious. */
    bi->FillRect(bi, &ri, 40, 40, 160, 160, 0x33, 0xFF, RGBFB_CLUT);
    bi->WaitBlitter(bi);

    WORD x = 64, y = 64, w = 96, h = 96;
    D(0, "TestMach32: BlitPattern — 8x8 mono detect (JAM2 fg=0xFF bg=0x00)\n");
    bi->BlitPattern(bi, &ri, &patt, x, y, w, h, 0xFF, RGBFB_CLUT);
    bi->WaitBlitter(bi);

    UWORD bpr = ri.BytesPerRow;
    struct
    {
        UWORD x, y;
    } samples[] = {
        {64, 64}, {65, 64}, {71, 64}, {64, 65}, {70, 70}, {71, 71},
    };

    for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        UWORD sx = samples[i].x, sy = samples[i].y;
        UBYTE got = readPixel8(bi, bpr, sx, sy);
        UBYTE exp = expected8x8MonoPen(pat8x8, (UWORD)x, (UWORD)y, sx, sy, 0xFF, 0x00);
        if (got != exp) {
            D(0, "  FAIL BlitPattern sample (%lu,%lu): expected 0x%02lx, got 0x%02lx\n", (ULONG)sx, (ULONG)sy,
              (ULONG)exp, (ULONG)got);
        } else {
            D(0, "  PASS BlitPattern sample (%lu,%lu): 0x%02lx\n", (ULONG)sx, (ULONG)sy, (ULONG)got);
        }
    }
}

static void testDrawLineClut8bpp(BoardInfo_t *bi)
{
    if (!bi->DrawLine) {
        D(0, "TestMach32: DrawLine not installed; skipping line test\n");
        return;
    }

    struct RenderInfo ri;
    memset(&ri, 0, sizeof(ri));
    ri.Memory      = bi->MemoryBase;
    ri.BytesPerRow = bi->CalculateBytesPerRow(bi, 640, 480, bi->ModeInfo, RGBFB_CLUT);
    ri.RGBFormat   = RGBFB_CLUT;
    UWORD bpr      = ri.BytesPerRow;

    /* Ensure a known background under the test area. */
    bi->FillRect(bi, &ri, 16, 260, 200, 120, 0x11, 0xFF, RGBFB_CLUT);
    bi->WaitBlitter(bi);

    D(0, "TestMach32: DrawLine — solid lines (fg=0xEE)\n");

    struct Line line;

    /* Horizontal line. */
    makeSolidLine(&line, 20, 280, 100, 280, JAM2, 0xEE);
    bi->DrawLine(bi, &ri, &line, 0xFF, RGBFB_CLUT);
    bi->WaitBlitter(bi);
    verifyPixel8(bi, bpr, 60, 280, 0xEE, "DrawLine horiz mid");

    /* Vertical line. */
    makeSolidLine(&line, 120, 270, 120, 340, JAM2, 0xEE);
    bi->DrawLine(bi, &ri, &line, 0xFF, RGBFB_CLUT);
    bi->WaitBlitter(bi);
    verifyPixel8(bi, bpr, 120, 305, 0xEE, "DrawLine vert mid");

    /* 45-degree diagonal. */
    makeSolidLine(&line, 30, 300, 80, 350, JAM2, 0xEE);
    bi->DrawLine(bi, &ri, &line, 0xFF, RGBFB_CLUT);
    bi->WaitBlitter(bi);
    verifyPixel8(bi, bpr, 55, 325, 0xEE, "DrawLine diag mid");

    /* JAM2 horizontal stipple: 0xAAAA (MSB first along +X), fg/bg match fill so zeros stay visible. */
    D(0, "TestMach32: DrawLine — patterned horizontal (JAM2 fg=0xEE bg=0x11, LinePtrn=0xAAAA)\n");
    makePatternLine(&line, 24, 315, 55, 315, JAM2, 0xEE, 0x11, 0xAAAAu, 0);
    bi->DrawLine(bi, &ri, &line, 0xFF, RGBFB_CLUT);
    bi->WaitBlitter(bi);
    /* First pixel along +X: MSB of 0xAAAA = 1 -> fg; second: 0 -> bg */
    verifyPixel8(bi, bpr, 24, 315, 0xEE, "DrawLine patt horiz bit0");
    verifyPixel8(bi, bpr, 25, 315, 0x11, "DrawLine patt horiz bit1");
}

/*
 * Hardware FillRect + InvertRect test.  The CPU XOR pattern stays visible as the
 * background; GE fills only cover partial regions so you can see both CPU and GE
 * output side by side.  Identity palette: pen value == grey level.
 */
static void testFillRectClut8bpp(BoardInfo_t *bi)
{
    struct RenderInfo ri;

    memset(&ri, 0, sizeof(ri));
    ri.Memory      = bi->MemoryBase;
    ri.BytesPerRow = bi->CalculateBytesPerRow(bi, 640, 480, bi->ModeInfo, RGBFB_CLUT);
    ri.RGBFormat   = RGBFB_CLUT;
    UWORD bpr      = ri.BytesPerRow;

    if (bi->FillRect == NULL) {
        D(0, "TestMach32: FillRect not installed; skipping GE fill test\n");
        return;
    }

    /* Verify XOR background is intact before GE fills */
    UBYTE bgSample = readPixel8(bi, bpr, 100, 100);
    D(0, "TestMach32: pre-fill bg check at (100,100): got 0x%02lx, expected 0x%02lx (x^y)\n", (ULONG)bgSample,
      (ULONG)(UBYTE)(100 ^ 100));

    D(0, "TestMach32: FillRect — two nested rects on XOR background (0x80 / 0xFF)\n");

    bi->FillRect(bi, &ri, 80, 60, 480, 360, 0x80, 0xFF, RGBFB_CLUT);
    bi->WaitBlitter(bi);
    verifyRect8(bi, bpr, 80, 60, 480, 360, 0x80, "FillRect pen=0x80");

    bi->FillRect(bi, &ri, 160, 120, 320, 240, 0xFF, 0xFF, RGBFB_CLUT);
    bi->WaitBlitter(bi);
    verifyRect8(bi, bpr, 160, 120, 320, 240, 0xFF, "FillRect pen=0xFF");

    /* Verify outer fill region (between the two rects) still has pen 0x80 */
    verifyRect8(bi, bpr, 80, 60, 80, 60, 0x80, "outer region still 0x80");

    /* Verify untouched background outside all fills */
    UBYTE bgAfter = readPixel8(bi, bpr, 10, 10);
    D(0, "  bg after fills at (10,10): got 0x%02lx, expected 0x%02lx (x^y)\n", (ULONG)bgAfter, (ULONG)(UBYTE)(10 ^ 10));

    if (bi->InvertRect) {
        D(0, "TestMach32: InvertRect — 200x100 in white region (0xFF -> 0x00 = black)\n");
        bi->InvertRect(bi, &ri, 220, 190, 200, 100, 0xFF, RGBFB_CLUT);
        bi->WaitBlitter(bi);
        verifyRect8(bi, bpr, 220, 190, 200, 100, 0x00, "InvertRect 0xFF->0x00");
    }

    if (bi->BlitRect) {
        /* Forward blit (dstY < srcY): copy 0x80 region to bottom of screen */
        D(0, "TestMach32: BlitRect — fwd copy 100x60 from (80,60) to (10,400)\n");
        bi->BlitRect(bi, &ri, 80, 60, 10, 400, 100, 60, 0xFF, RGBFB_CLUT);
        bi->WaitBlitter(bi);
        verifyRect8(bi, bpr, 10, 400, 100, 60, 0x80, "BlitRect fwd 0x80");

        /* Overlapping blit: shift the 0x80 region down by 20 pixels */
        D(0, "TestMach32: BlitRect — overlap 100x60 from (80,60) to (80,80)\n");
        bi->BlitRect(bi, &ri, 80, 60, 80, 80, 100, 60, 0xFF, RGBFB_CLUT);
        bi->WaitBlitter(bi);
        verifyRect8(bi, bpr, 80, 80, 100, 60, 0x80, "BlitRect overlap dst");

        /* Reverse blit (dstY > srcY): copy 0xFF region to far corner */
        D(0, "TestMach32: BlitRect — rev copy 80x50 from (200,130) to (540,10)\n");
        bi->BlitRect(bi, &ri, 200, 130, 540, 10, 80, 50, 0xFF, RGBFB_CLUT);
        bi->WaitBlitter(bi);
        verifyRect8(bi, bpr, 540, 10, 80, 50, 0xFF, "BlitRect rev 0xFF");
    }

    if (bi->BlitTemplate) {
        D(0, "TestMach32: BlitTemplate — DrawMode/XOffset/odd-width coverage\n");

        /* 32x32 mono pattern: MSB = left. */
        static ULONG template32[32] = {
            (ULONG)0b00000000000000100000000000000000, (ULONG)0b00000000000000100000000000000000,
            (ULONG)0b00000000000000100000000000000000, (ULONG)0b00000000000000100000000000000000,
            (ULONG)0b00000000001111011000000000000000, (ULONG)0b00000011111111111110000000000000,
            (ULONG)0b00001111111111111111110000000000, (ULONG)0b00011111111111111111111100000000,
            (ULONG)0b00111111111111111111111111000000, (ULONG)0b01111110000111111100001111110000,
            (ULONG)0b01111110000111111100001111110000, (ULONG)0b00011111111111111111111111100000,
            (ULONG)0b00001111111111111111111111100000, (ULONG)0b00000111111111111111111111100000,
            (ULONG)0b00000111111111111111111111100000, (ULONG)0b00000111111111111111111111100000,
            (ULONG)0b00000111110011111111100111100000, (ULONG)0b00000111111100000000011111100000,
            (ULONG)0b00000111111111111111111111100000, (ULONG)0b00000111111111111111111111100000,
            (ULONG)0b00111111111111111111111111000000, (ULONG)0b00111111111111111111111111000000,
            (ULONG)0b00000001111111111111111110000000, (ULONG)0b00000000111111111111111100000000,
            (ULONG)0b00000011111111111100000000000000, (ULONG)0b10101010101010101010101010101010,
            (ULONG)0b01010101010101010101010101010101, (ULONG)0b11111111111111111111111111111111,
            (ULONG)0b00000000000000000000000000000000, (ULONG)0b11001100110011001100110011001100,
            (ULONG)0b00110011001100110011001100110011,
        };

        /* FgPen = 1 (red), BgPen = 0 (blue). */
#define BLIT_TMPL_PEN_FG 1
#define BLIT_TMPL_PEN_BG 0

        /* Simple 32x32 blits at y=0 (position drift regression). */
        for (int i = 7; i < 16; i++) {
            struct Template tmpl;
            tmpl.Memory      = template32;
            tmpl.BytesPerRow = 4;
            tmpl.XOffset     = 0;
            tmpl.DrawMode    = JAM2;
            tmpl.FgPen       = BLIT_TMPL_PEN_FG;
            tmpl.BgPen       = BLIT_TMPL_PEN_BG;

            bi->FillRect(bi, &ri, i * 32 + i, 0, 32, 32, 50, 0xFF, RGBFB_CLUT);
            bi->BlitTemplate(bi, &ri, &tmpl, i * 32 + i, 0, 32, 32, 0xFF, RGBFB_CLUT);
            bi->WaitBlitter(bi);
        }

        /* DrawMode matrix + XOffset roll (width 24 to exercise padding/scissor). */
        for (int i = 0; i < 4; i++) {
            WORD y    = (WORD)(200 + i * 40);
            WORD xoff = 5;

            /* JAM1: 1->Fg, 0->D (keep background). */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = (UWORD)(i * 2);
                tmpl.DrawMode    = JAM1;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_FG; /* avoid transparent bg edge-cases */
                bi->FillRect(bi, &ri, 32 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                bi->BlitTemplate(bi, &ri, &tmpl, 32 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
                bi->WaitBlitter(bi);
            }

            /* JAM2: 1->Fg, 0->Bg. */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = (UWORD)(i * 2);
                tmpl.DrawMode    = JAM2;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_BG;
                bi->FillRect(bi, &ri, 96 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                bi->BlitTemplate(bi, &ri, &tmpl, 96 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
                bi->WaitBlitter(bi);
            }

            /* COMPLEMENT: flip where pattern is 1. */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = (UWORD)(i * 2);
                tmpl.DrawMode    = COMPLEMENT;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_FG;
                bi->FillRect(bi, &ri, 144 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                bi->BlitTemplate(bi, &ri, &tmpl, 144 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
                bi->WaitBlitter(bi);
            }

            /* JAM1 | INVERSVID. */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = (UWORD)(i * 2);
                tmpl.DrawMode    = JAM1 | INVERSVID;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_FG;
                bi->FillRect(bi, &ri, 192 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                bi->BlitTemplate(bi, &ri, &tmpl, 192 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
                bi->WaitBlitter(bi);
            }

            /* JAM2 | INVERSVID. */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = (UWORD)(i * 2);
                tmpl.DrawMode    = JAM2 | INVERSVID;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_BG;
                bi->FillRect(bi, &ri, 240 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                bi->BlitTemplate(bi, &ri, &tmpl, 240 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
                bi->WaitBlitter(bi);
            }

            /* COMPLEMENT | INVERSVID. */
            {
                struct Template tmpl;
                tmpl.Memory      = template32;
                tmpl.BytesPerRow = 4;
                tmpl.XOffset     = (UWORD)(i * 2);
                tmpl.DrawMode    = COMPLEMENT | INVERSVID;
                tmpl.FgPen       = BLIT_TMPL_PEN_FG;
                tmpl.BgPen       = BLIT_TMPL_PEN_BG;
                bi->FillRect(bi, &ri, 288 + xoff, y, 32, 32, 50, 0xFF, RGBFB_CLUT);
                bi->BlitTemplate(bi, &ri, &tmpl, 288 + xoff, y, 24, 32, 0xFF, RGBFB_CLUT);
                bi->WaitBlitter(bi);
            }
        }

#undef BLIT_TMPL_PEN_FG
#undef BLIT_TMPL_PEN_BG

        /* Regression: 32x32 1px frame + edge-case width near right border. */
        {
            static ULONG templateFrame[32];
            for (int row = 0; row < 32; row++) {
                templateFrame[row] = (row == 0 || row == 31) ? 0xFFFFFFFFUL : 0x80000001UL;
            }
            struct Template tmpl;
            tmpl.Memory      = templateFrame;
            tmpl.BytesPerRow = 4;
            tmpl.XOffset     = 0;
            tmpl.DrawMode    = JAM2;
            tmpl.FgPen       = 0x00;
            tmpl.BgPen       = 0xFF;

            bi->FillRect(bi, &ri, 422, 262, 32, 32, 0xFF, 0xFF, RGBFB_CLUT);
            bi->BlitTemplate(bi, &ri, &tmpl, 422, 262, 32, 32, 0xFF, RGBFB_CLUT);
            bi->WaitBlitter(bi);

            bi->FillRect(bi, &ri, 627, 262, 20, 32, 0xFF, 0xFF, RGBFB_CLUT);
            bi->BlitTemplate(bi, &ri, &tmpl, 627, 262, 20, 32, 0xFF, RGBFB_CLUT);
            bi->WaitBlitter(bi);
        }

        /* 46x11: non-32 width, non-dword row size. */
        {
            static UBYTE template46x11[11][8]; /* 6 bytes per row used, 8 for alignment */
            for (int row = 0; row < 11; row++) {
                if (row == 0 || row == 10) {
                    template46x11[row][0] = 0xFF;
                    template46x11[row][1] = 0xFF;
                    template46x11[row][2] = 0xFF;
                    template46x11[row][3] = 0xFF;
                    template46x11[row][4] = 0xFF;
                    template46x11[row][5] = 0x3F; /* 46 bits: low 6 bits of byte 5 */
                } else {
                    template46x11[row][0] = 0x01;
                    template46x11[row][1] = 0x00;
                    template46x11[row][2] = 0x00;
                    template46x11[row][3] = 0x00;
                    template46x11[row][4] = 0x00;
                    template46x11[row][5] = 0x20; /* bit 45 = last column */
                }
            }
            struct Template tmpl46;
            tmpl46.Memory      = template46x11;
            tmpl46.BytesPerRow = 6;
            tmpl46.XOffset     = 0;
            tmpl46.DrawMode    = JAM2;
            tmpl46.FgPen       = 0x00;
            tmpl46.BgPen       = 0xFF;

            bi->FillRect(bi, &ri, 94, 57, 46, 11, 0xFF, 0xFF, RGBFB_CLUT);
            bi->BlitTemplate(bi, &ri, &tmpl46, 94, 57, 46, 11, 0xFF, RGBFB_CLUT);
            bi->WaitBlitter(bi);
        }
    }
}

/* Linear scan of probeFramebufferSize() range; PCI BAR is often larger (e.g. 4 MB decode vs 1 MB RAM). */
#define VRAM_TEST_LOG_MAX 32

/* Full VRAM scan is slow on target; keep disabled for fast iteration. */
#ifndef TESTMACH32_ENABLE_MEMTEST
#define TESTMACH32_ENABLE_MEMTEST 0
#endif

static INLINE UBYTE vramTestBytePat(ULONG off)
{
    return (UBYTE)(off ^ (off >> 8) ^ (off >> 16) ^ (off >> 24) ^ 0x5Au);
}

static INLINE UWORD vramTestWordPat(ULONG off)
{
    ULONG i = off >> 1;
    return (UWORD)(i ^ (i >> 8) ^ (i >> 16) ^ 0xACE1u);
}

static INLINE ULONG vramTestDwordPat(ULONG off)
{
    ULONG i = off >> 2;
    return i ^ (i << 11) ^ (i >> 7) ^ 0xCAFEBABEu;
}

static int testVramScanBytes(UBYTE *base, ULONG len, int *logBudget)
{
    int err           = 0;
    volatile UBYTE *m = (volatile UBYTE *)base;

    for (ULONG off = 0; off < len; off++) {
        UBYTE w = vramTestBytePat(off);
        m[off]  = w;
        flushWrites();
        UBYTE r = m[off];
        if (r != w) {
            err++;
            if (*logBudget > 0) {
                D(ERROR, "  VRAM byte off 0x%08lX: wrote 0x%02lX read 0x%02lX\n", off, (ULONG)w, (ULONG)r);
                (*logBudget)--;
            }
        }
    }
    return err;
}

static int testVramScanWords(UBYTE *base, ULONG len, int *logBudget)
{
    int err          = 0;
    ULONG lenAligned = len & ~1UL;
    volatile UWORD *m;

    for (ULONG off = 0; off + 2UL <= lenAligned; off += 2UL) {
        UWORD w = vramTestWordPat(off);
        m       = (volatile UWORD *)(base + off);
        *m      = w;
        flushWrites();
        UWORD r = *m;
        if (r != w) {
            err++;
            if (*logBudget > 0) {
                D(ERROR, "  VRAM word off 0x%08lX: wrote 0x%04lX read 0x%04lX\n", off, (ULONG)w, (ULONG)r);
                (*logBudget)--;
            }
        }
    }
    return err;
}

static int testVramScanDwords(UBYTE *base, ULONG len, int *logBudget)
{
    int err          = 0;
    ULONG lenAligned = len & ~3UL;
    volatile ULONG *m;

    for (ULONG off = 0; off + 4UL <= lenAligned; off += 4UL) {
        ULONG w = vramTestDwordPat(off);
        m       = (volatile ULONG *)(base + off);
        *m      = w;
        flushWrites();
        ULONG r = *m;
        if (r != w) {
            err++;
            if (*logBudget > 0) {
                D(ERROR, "  VRAM dword off 0x%08lX: wrote 0x%08lX read 0x%08lX XOR 0x%08lX\n", off, w, r, w ^ r);
                (*logBudget)--;
            }
        }
    }
    return err;
}

static int testVramByteRoundtrip(BoardInfo_t *bi, ULONG vramBytes)
{
    int logLeft = VRAM_TEST_LOG_MAX;
    int errors  = 0;

    if (vramBytes == 0) {
        D(WARN, "TestMach32: VRAM UBYTE scan skipped (zero probe length)\n");
        return 0;
    }

    D(INFO, "TestMach32: VRAM UBYTE full scan [0 .. 0x%08lX) (%lu bytes, probed VRAM)\n", vramBytes, vramBytes);
    errors = testVramScanBytes(bi->MemoryBase, vramBytes, &logLeft);

    if (errors == 0) {
        D(INFO, "TestMach32: VRAM UBYTE scan — all %lu bytes match\n", vramBytes);
    } else {
        D(ERROR, "TestMach32: VRAM UBYTE scan — %d mismatches (first %d logged)\n", errors,
          VRAM_TEST_LOG_MAX - logLeft);
    }

    return errors;
}

static int testVramWordRoundtrip(BoardInfo_t *bi, ULONG vramBytes)
{
    int logLeft = VRAM_TEST_LOG_MAX;
    int errors  = 0;
    ULONG words = vramBytes >> 1;

    if (vramBytes < 2UL) {
        D(WARN, "TestMach32: VRAM UWORD scan skipped (probed length < 2 bytes)\n");
        return 0;
    }

    D(INFO, "TestMach32: VRAM UWORD full scan [0 .. 0x%08lX) (%lu words, probed VRAM)\n", vramBytes & ~1UL, words);
    errors = testVramScanWords(bi->MemoryBase, vramBytes, &logLeft);

    if (errors == 0) {
        D(INFO, "TestMach32: VRAM UWORD scan — all %lu words match\n", words);
    } else {
        D(ERROR, "TestMach32: VRAM UWORD scan — %d mismatches (first %d logged)\n", errors,
          VRAM_TEST_LOG_MAX - logLeft);
    }

    return errors;
}

static int testVramLongwordRoundtrip(BoardInfo_t *bi, ULONG vramBytes)
{
    int logLeft  = VRAM_TEST_LOG_MAX;
    int errors   = 0;
    ULONG dwords = vramBytes >> 2;

    if (vramBytes < 4UL) {
        D(WARN, "TestMach32: VRAM ULONG scan skipped (probed length < 4 bytes)\n");
        return 0;
    }

    D(INFO, "TestMach32: VRAM ULONG full scan [0 .. 0x%08lX) (%lu dwords, probed VRAM)\n", vramBytes & ~3UL, dwords);
    errors = testVramScanDwords(bi->MemoryBase, vramBytes, &logLeft);

    if (errors == 0) {
        D(INFO, "TestMach32: VRAM ULONG scan — all %lu dwords match\n", dwords);
    } else {
        D(ERROR, "TestMach32: VRAM ULONG scan — %d mismatches (first %d logged)\n", errors,
          VRAM_TEST_LOG_MAX - logLeft);
    }

    return errors;
}

static void fillModeInfo640x480(struct ModeInfo *mi)
{
    memset(mi, 0, sizeof(*mi));
    mi->Width  = 640;
    mi->Height = 480;
    mi->Depth  = 8;
    /* VGA 640x480 @ ~60 Hz, ~25.175 MHz; Hor/Ver fields match Mach64 / programCrtc conventions */
    mi->HorTotal     = 800;
    mi->HorBlankSize = 160;
    mi->HorSyncStart = 16;
    mi->HorSyncSize  = 96;
    mi->VerTotal     = 525;
    mi->VerBlankSize = 45;
    mi->VerSyncStart = 10;
    mi->VerSyncSize  = 2;
}

static void setup640x480Screen(struct BoardInfo *bi)
{
    static struct ModeInfo mode;
    mode.Width        = 640;
    mode.Height       = 480;
    mode.Depth        = 8;
    mode.Flags        = GMF_HPOLARITY | GMF_VPOLARITY;
    mode.HorTotal     = 800;
    mode.HorBlankSize = 0;
    mode.HorSyncStart = 16;
    mode.HorSyncSize  = 96;
    mode.VerTotal     = 525;
    mode.VerBlankSize = 0;
    mode.VerSyncStart = 10;
    mode.VerSyncSize  = 2;
    mode.PixelClock   = 25175000;

    bi->RGBFormat = RGBFB_CLUT;
    bi->Depth     = 8;

    bi->SetMemoryMode(bi, RGBFB_CLUT);
    bi->SetDAC(bi, 0, RGBFB_CLUT);

    bi->ResolvePixelClock(bi, &mode, 25175000UL, RGBFB_CLUT);
    bi->ModeInfo = &mode;

    bi->SetClock(bi);
    bi->SetGC(bi, &mode, TRUE);

    {
        DFUNC(0, "SetColorArray\n");
        UBYTE colors[256 * 3];
        for (int c = 0; c < 256; c++) {
            bi->CLUT[c].Red   = c;
            bi->CLUT[c].Green = c;
            bi->CLUT[c].Blue  = c;
        }

        /* Two colors for BlitTemplate DrawMode tests: index 0 = blue, index 1 = red. */
        bi->CLUT[0].Red   = 0;
        bi->CLUT[0].Green = 0;
        bi->CLUT[0].Blue  = 255;
        bi->CLUT[1].Red   = 255;
        bi->CLUT[1].Green = 0;
        bi->CLUT[1].Blue  = 0;

        bi->SetColorArray(bi, 0, 256);

        /* Readback a few palette entries to verify DAC precision */
        DRIVER_LOCALS(bi);
        UBYTE testEntries[] = {1, 63, 64, 127, 128, 129, 200, 255};
        for (int i = 0; i < (int)(sizeof(testEntries) / sizeof(testEntries[0])); i++) {
            UBYTE idx = testEntries[i];
            io.writeB(IoReg::id_DAC_R_INDEX, idx);
            delayMicroSeconds(2);
            UBYTE r = io.readB(IoReg::id_DAC_DATA);
            UBYTE g = io.readB(IoReg::id_DAC_DATA);
            UBYTE b = io.readB(IoReg::id_DAC_DATA);
            D(0, "  palette[%ld] readback: R=%ld G=%ld B=%ld (expected %ld)\n", (ULONG)idx, (ULONG)r, (ULONG)g,
              (ULONG)b, (ULONG)idx);
        }
    }

    bi->SetPanning(bi, bi->MemoryBase, 640, 480, 0, 0, RGBFB_CLUT);
    bi->SetDisplay(bi, TRUE);
}

#if defined(TESTEXE) && !defined(MACH32_EMBEDDED_CHIP)
static void onSigInt(int dummy)
{
    (void)dummy;
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
        OpenPciBase = NULL;
    }
    abort();
}

int main(void)
{
    signal(SIGINT, onSigInt);

    int rval              = EXIT_FAILURE;
    BOOL vblankTest       = FALSE;
    struct RDArgs *rdargs = NULL;

    if (!(OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION))) {
        D(ERROR, "TestMach32: cannot open openpci.library v%ld+\n", MIN_OPENPCI_VERSION);
        return EXIT_FAILURE;
    }

    testArgs[0] = 0;
    rdargs      = ReadArgs((STRPTR)testArgsTemplate, testArgs, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR) "TestMach32");
        CloseLibrary(OpenPciBase);
        OpenPciBase = NULL;
        return EXIT_FAILURE;
    }
    vblankTest = testArgs[0] ? TRUE : FALSE;
    FreeArgs(rdargs);
    rdargs = NULL;

    D(0, "TestMach32: looking for Mach32...\n");

    struct pci_dev *board     = NULL;
    struct TagItem findTags[] = {{PRM_Vendor, PCI_VENDOR_ATI}, {PRM_Device, PCI_DEVICE_MACH32}, {TAG_END, 0}};

    while ((board = FindBoardA(board, findTags)) != NULL) {
        ULONG Device = 0, Revision = 0, Memory0Size = 0;
        APTR Memory0 = NULL, legacyIOBase = NULL;

        GetBoardAttrs(board, PRM_Device, (Tag)&Device, PRM_Revision, (Tag)&Revision, PRM_MemoryAddr0, (Tag)&Memory0,
                      PRM_MemorySize0, (Tag)&Memory0Size, PRM_LegacyIOSpace, (Tag)&legacyIOBase, TAG_END);

        if (Device != PCI_DEVICE_MACH32) {
            continue;
        }

        D(0, "TestMach32: Mach32 PCI device rev %lu, BAR0 %p size %lu\n", Revision, Memory0, Memory0Size);

        pci_write_config_word(PCI_COMMAND,
                              (UWORD)(pci_read_config_word(PCI_COMMAND, board) | PCI_COMMAND_MEMORY | PCI_COMMAND_IO),
                              board);

        /* BoardInfo is large — keep off the default CLI stack (see TestS3 / TestMach64). */
        static struct BoardInfo boardInfo;
        memset(&boardInfo, 0, sizeof(boardInfo));
        struct BoardInfo *bi = &boardInfo;

        bi->ExecBase                  = SysBase;
        bi->UtilBase                  = (struct Library *)UtilityBase;
        getCardData(bi)->OpenPciBase  = OpenPciBase;
        getCardData(bi)->board        = board;
        getCardData(bi)->legacyIOBase = (volatile UBYTE *)legacyIOBase + REGISTER_OFFSET;
        bi->RegisterBase              = (UBYTE *)legacyIOBase + REGISTER_OFFSET;
        bi->MemoryBase                = (UBYTE *)Memory0;
        bi->MemorySize                = Memory0Size;

        if (Memory0Size > 0) {
            setCacheMode(bi, (UBYTE *)Memory0, Memory0Size, MAPP_CACHEINHIBIT | MAPP_IMPRECISE | MAPP_NONSERIALIZED,
                         CACHEFLAGS);
        }

        if (!InitChip(bi)) {
            D(ERROR, "TestMach32: InitChip failed\n");
            goto done;
        }

        D(INFO, "TestMach32: VRAM linear scans use probe=%lu KB \n", bi->MemorySize / 1024UL);

        D(INFO, "TestMach32: programming 640x480 CLUT / VGA timings + identity palette\n");
        setup640x480Screen(bi);

        if (vblankTest)
            testVBlankInterrupt(bi, board);

        D(INFO, "TestMach32: VRAM XOR byte fill at %p (CPU)\n", bi->MemoryBase);
        testFillPattern8bppBytes(bi, 640, 480);
        bi->WaitBlitter(bi);

        testFillRectClut8bpp(bi);
        testBlitPattern8x8Mono(bi);
        testDrawLineClut8bpp(bi);

        if (TESTMACH32_ENABLE_MEMTEST) {
            const ULONG vramScanBytes = bi->MemorySize;

            int memErr = testVramLongwordRoundtrip(bi, vramScanBytes);
            memErr += testVramWordRoundtrip(bi, vramScanBytes);
            memErr += testVramByteRoundtrip(bi, vramScanBytes);

            if (memErr) {
                D(ERROR, "TestMach32: VRAM byte/word/dword full-scan test(s) failed\n");
                rval = EXIT_FAILURE;
            } else {
                rval = EXIT_SUCCESS;
            }
        } else {
            D(INFO, "TestMach32: VRAM full-scan tests disabled\n");
            rval = EXIT_SUCCESS;
        }
        goto done;
    }

    D(ERROR, "TestMach32: no PCI Mach32 (0x4158) found\n");

done:
    if (OpenPciBase) {
        CloseLibrary(OpenPciBase);
        OpenPciBase = NULL;
    }
    return rval;
}
#endif
#endif /* TESTEXE */
