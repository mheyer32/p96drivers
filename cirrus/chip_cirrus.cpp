#include "chip_cirrus.h"
#include "edid_common.h"

#define __NOLIBBASE__

#include <clib/debug_protos.h>
#include <debuglib.h>
#include <exec/types.h>
#include <graphics/rastport.h>

#include <exec/interrupts.h>
#include <libraries/openpci.h>
#include <libraries/pcitags.h>
#include <proto/exec.h>
#include <proto/openpci.h>

#include <SDI_stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DBG
#if defined(TESTEXE)
int debugLevel = VERBOSE;
#else
int debugLevel = VERBOSE;
#endif
#endif

#ifndef TESTEXE
extern const char LibName[]     = "CirrusGD542x.chip";
extern const char LibIdString[] = "Cirrus Logic CL-GD542x Picasso96 chip driver version 1.0";

#ifndef LIB_VERSION
#define LIB_VERSION 1
#endif
#ifndef LIB_REVISION
#define LIB_REVISION 0
#endif
extern const UWORD LibVersion  = LIB_VERSION;
extern const UWORD LibRevision = LIB_REVISION;
#endif

#define MIN_PLLCLOCK_KHZ 24000
#define REF_FREQ_KHZ     14318

/* SRF[4:3] bus width / installed DRAM; bank bit only on non-Alpine. */
static ULONG cirrusMemSizeFromSrf(UBYTE srf, ChipFamily_t family)
{
    ULONG mem;

    switch (srf & 0x18) {
    case 0x08:
        mem = 512UL * 1024;
        break;
    case 0x10:
        mem = 1024UL * 1024;
        break;
    case 0x18:
        mem = 2048UL * 1024;
        break;
    default:
        mem = 1024UL * 1024;
        break;
    }

    if (family != GD5430 && family != GD5432 && family != GD5440 && (srf & 0x80))
        mem *= 2;

    return mem;
}

/* VBIOS program_srf_dram_control: SRF &= 0x67; SRF |= bits (bits in {0x08,0x10,0x18,0x98}). */
static UBYTE cirrusSrfDramBitsForSize(ULONG size, ChipFamily_t family)
{
    if (size >= 4UL * 1024 * 1024 && family != GD5430 && family != GD5432 && family != GD5440)
        return 0x98;
    if (size >= 2UL * 1024 * 1024)
        return 0x18;
    if (size >= 1UL * 1024 * 1024)
        return 0x10;
    return 0x08;
}

static void settleSrf(void)
{
    volatile ULONG n = 0x800;
    while (n--)
        ;
}

static void programSrfDramControl(VgaIo vga, UBYTE dramBits)
{
    UBYTE sr1f = vga.readSR(0x1F);
    UBYTE srf  = vga.readSR(0x0F);
    /* Alpine: bank bit (0x80) is 5434/36-only; keep FIFO (0x20) when set. */
    vga.writeSR(0x0F, (UBYTE)((srf & 0x67) | (dramBits & 0x98)));
    vga.writeSR(0x1F, sr1f);
    settleSrf();
}

/* SRF[4:3] latches into DRAM timing only at HBLANK — CRTC must be scanning. */
static void waitDisplayBlankEdges(VgaIo vga, int edges)
{
    for (int i = 0; i < edges; ++i) {
        ULONG t = 200000;
        while ((vga.readB(VgaReg::INPUT_STATUS1) & 1) && --t)
            ;
        t = 200000;
        while (!(vga.readB(VgaReg::INPUT_STATUS1) & 1) && --t)
            ;
    }
}

static void startCrtcForDramLatch(VgaIo vga)
{
    /* Unlock CR0–7 */
    vga.writeCR(0x11, (UBYTE)(vga.readCR(0x11) & ~0x80));

    /* SDK CirrusGD5434 ~640x480 @ 31 kHz */
    vga.writeCR(0x00, 0x5F);
    vga.writeCR(0x01, 0x4F);
    vga.writeCR(0x02, 0x50);
    vga.writeCR(0x03, 0x82);
    vga.writeCR(0x04, 0x54);
    vga.writeCR(0x05, 0x80);
    vga.writeCR(0x06, 0xBF);
    vga.writeCR(0x07, 0x1F);
    vga.writeCR(0x08, 0x00);
    vga.writeCR(0x09, 0x40);
    vga.writeCR(0x10, 0x9C);
    vga.writeCR(0x11, 0x0E); /* keep unlocked */
    vga.writeCR(0x12, 0x8F);
    vga.writeCR(0x13, 0x50);
    vga.writeCR(0x14, 0x00);
    vga.writeCR(0x15, 0x96);
    vga.writeCR(0x16, 0xB9);
    vga.writeCR(0x17, 0xC3);
    vga.writeCR(0x18, 0xFF);
    vga.writeCR(0x1A, 0x00);
    vga.writeCR(0x1B, 0x82);

    vga.writeGR(0x0E, 0x00); /* power management off */
    vga.writeGR(0x0B, 0x20); /* Alpine 16K granularity (cirrusfb) */
}

/*
 * CL-GD5440 VBIOS 1.07 init_extended_sequencer_memory / pre_seq @ 094E merge @ 097C.
 * Without this, cold BAR0 stays unclaimed (ROM still works).
 */
static void initExtendedSequencerMemory(VgaIo vga)
{
    vga.writeSR(0x16, (UBYTE)(vga.readSR(0x16) | 0x50));

    vga.writeSR(0x08, (UBYTE)((vga.readSR(0x08) & 0xBF) | 0x4A));
    vga.writeSR(0x0B, 0x2B);
    vga.writeSR(0x1B, 0x5B);
    vga.writeSR(0x0C, 0x2F);
    vga.writeSR(0x1C, 0x42);
    vga.writeSR(0x0D, 0x1F);
    vga.writeSR(0x1D, 0x00);
    vga.writeSR(0x0F, 0x71);
    vga.writeSR(0x16, 0x71);
    vga.writeSR(0x1F, 0x1C);
    vga.writeSR(0x17, (UBYTE)((vga.readSR(0x17) & 0xFE) | 0x01));

    vga.writeSR(0x1F, (UBYTE)((vga.readSR(0x1F) & 0xC0) | 0x1C));

    if ((vga.readSR(0x17) & 0x38) == 0x20)
        vga.writeSR(0x16, (UBYTE)((vga.readSR(0x16) & 0xAF) | 0xC0));

    vga.writeSR(0x09, 0x44);
    vga.writeSR(0x0A, 0x30);
    vga.writeSR(0x14, 0x08);
    vga.writeSR(0x15, 0x01);

    /* finalize @ 0987 with merge stream after DAT_13CE */
    vga.writeSR(0x07, 0x00);
    vga.writeSR(0x0F, (UBYTE)(vga.readSR(0x0F) & 0x9F));
    vga.writeSR(0x0E, 0x01);
    vga.writeSR(0x1E, 0x00);
    vga.writeGR(0x0B, (UBYTE)(vga.readGR(0x0B) & 0xC0));
    vga.writeCR(0x19, 0x05);
    vga.writeCR(0x1A, 0xFF);
    vga.writeCR(0x1B, 0xFF);
    vga.writeB(VgaReg::DAC_PEL_MASK, 0xFF);
    vga.writeSR(0x16, (UBYTE)((vga.readSR(0x16) & 0xF0) | 0x01));
}

static ULONG probeFramebufferSize(BoardInfo_t *bi)
{
    LOCAL_SYSBASE();

    DFUNC(INFO, "Probing framebuffer memory size...\n");

    volatile UBYTE *memBase = (volatile UBYTE *)bi->MemoryBase;
    ChipFamily_t family     = (ChipFamily_t)getChipData(bi)->chipFamily;
    ULONG maxSize           = (family == GD5434 || family == GD5436 || family == GD5446 || family == GD5480)
                                  ? 4UL * 1024 * 1024
                                  : 2UL * 1024 * 1024;

    ULONG lastGood = 0;

    for (ULONG size = 512UL * 1024; size <= maxSize; size *= 2) {
        ULONG testOffset    = size - 256 - 4;
        ULONG uniquePattern = (ULONG)memBase + testOffset;
        ULONG patternHalf   = uniquePattern ^ 0xA5A5A5A5UL;

        ULONG original       = *(volatile ULONG *)(memBase + testOffset);
        ULONG originalAtHalf = *(volatile ULONG *)(memBase + size / 2);

        *(volatile ULONG *)(memBase + testOffset) = uniquePattern;
        *(volatile ULONG *)(memBase + size / 2)   = patternHalf;
        CacheClearU();

        ULONG readback  = *(volatile ULONG *)(memBase + testOffset);
        ULONG readbackH = *(volatile ULONG *)(memBase + size / 2);
        ULONG readback0 = *(volatile ULONG *)(memBase);

        *(volatile ULONG *)(memBase + testOffset) = original;
        *(volatile ULONG *)(memBase + size / 2)   = originalAtHalf;
        CacheClearU();

        if (readback != uniquePattern || readbackH != patternHalf) {
            DFUNC(INFO, "Memory boundary at %ld KB (last good %ld KB)\n", size / 1024, lastGood / 1024);
            return lastGood;
        }
        if (readback0 == uniquePattern || readback0 == patternHalf) {
            DFUNC(INFO, "Memory wraparound at %ld KB (last good %ld KB)\n", size / 1024, lastGood / 1024);
            return lastGood;
        }
        lastGood = size;
    }

    DFUNC(INFO, "Returning size: %ld KB\n", lastGood / 1024);
    return lastGood;
}

/* Linux drivers/video/fbdev/cirrusfb.c:bestclock — target freq in kHz */
static void cirrusBestClock(long freq, int *nom, int *den, int *div)
{
    int n, d;
    long h, diff;

    *nom = 0;
    *den = 0;
    *div = 0;

    if (freq < 8000)
        freq = 8000;

    diff = freq;

    for (n = 32; n < 128; n++) {
        int s = 0;

        d = (int)((14318L * n) / freq);
        if ((d >= 7) && (d <= 63)) {
            int temp = d;

            if (temp > 31) {
                s = 1;
                temp >>= 1;
            }
            h = ((14318L * n) / temp) >> s;
            h = h > freq ? h - freq : freq - h;
            if (h < diff) {
                diff = h;
                *nom = n;
                *den = temp;
                *div = s;
            }
        }
        d++;
        if ((d >= 7) && (d <= 63)) {
            if (d > 31) {
                s = 1;
                d >>= 1;
            }
            h = ((14318L * n) / d) >> s;
            h = h > freq ? h - freq : freq - h;
            if (h < diff) {
                diff = h;
                *nom = n;
                *den = d;
                *div = s;
            }
        }
    }
}

static ULONG cirrusComputeKhzFromPll(const CirrusPLLValue_t *e)
{
    if (!e->nom || !e->den)
        return 0;
    ULONG num = (ULONG)REF_FREQ_KHZ * (ULONG)e->nom;
    ULONG den = (ULONG)e->den;
    if (e->div)
        den <<= 1;
    if (!den)
        return 0;
    return num / den;
}

static ULONG cirrusBuildPllEntry(ULONG targetFreqKhz, CirrusPLLValue_t *out)
{
    int nom = 0, den = 0, div = 0;
    cirrusBestClock((long)targetFreqKhz, &nom, &den, &div);
    if (!nom || !den)
        return 0;

    out->nom = (UBYTE)nom;
    out->den = (UBYTE)den;
    out->div = (UBYTE)div;

    ULONG got      = cirrusComputeKhzFromPll(out);
    out->freq10khz = (UWORD)((got + 5) / 10);
    return got;
}

void CirrusDriver::writeHDR(UBYTE val)
{
    BoardInfo *bi = this;
    VgaIo vga = this->vga();
    (void)vga.readB(VgaReg::DAC_PEL_MASK);
    (void)vga.readB(VgaReg::DAC_PEL_MASK);
    (void)vga.readB(VgaReg::DAC_PEL_MASK);
    (void)vga.readB(VgaReg::DAC_PEL_MASK);
    vga.writeB(VgaReg::DAC_PEL_MASK, val);
    delayMicroSeconds(200);
}

static void initPixelClockPLLTable(BoardInfo_t *bi)
{
    LOCAL_SYSBASE();

    ChipData_t *cd = getChipData(bi);

    UWORD maxFreq    = 135;
    UWORD minFreq    = 12;
    UWORD numEntries = (maxFreq - minFreq + 1) * 2;

    CirrusPLLValue_t *pllValues = (CirrusPLLValue_t *)AllocVec(sizeof(CirrusPLLValue_t) * numEntries, MEMF_PUBLIC);
    if (!pllValues) {
        DFUNC(ERROR, "Failed to allocate PLL table\n");
        return;
    }

    cd->pllValues    = pllValues;
    cd->numPllValues = 0;

    ULONG maxHiColorFreq   = 80000;
    ULONG maxTrueColorFreq = 50000;

    bi->PixelClockCount[PLANAR]    = 0;
    bi->PixelClockCount[HICOLOR]   = 0;
    bi->PixelClockCount[TRUECOLOR] = 0;
    bi->PixelClockCount[TRUEALPHA] = 0;
    bi->PixelClockCount[CHUNKY]    = 0;

    int lastValue = -1;
    for (UWORD i = 0; i < numEntries; ++i) {
        ULONG freq = (ULONG)minFreq * 1000 + (ULONG)i * 500;

        BOOL doubleClocking = (freq <= MIN_PLLCLOCK_KHZ);
        if (doubleClocking)
            freq *= 2;

        CirrusPLLValue_t *entry = &cd->pllValues[cd->numPllValues];
        ULONG currentKhz        = cirrusBuildPllEntry(freq, entry);

        if (doubleClocking)
            currentKhz /= 2;

        if (currentKhz > 0 && ((int)currentKhz != lastValue)) {
            lastValue        = (int)currentKhz;
            entry->freq10khz = (UWORD)((currentKhz + 5) / 10);
            cd->numPllValues++;

            bi->PixelClockCount[CHUNKY]++;
            if (currentKhz <= maxHiColorFreq) {
                bi->PixelClockCount[HICOLOR]++;
                if (currentKhz <= maxTrueColorFreq) {
                    bi->PixelClockCount[TRUECOLOR]++;
                    bi->PixelClockCount[TRUEALPHA]++;
                }
            }
        }
    }

    D(VERBOSE, "Initialized %ld Cirrus PLL entries\n", cd->numPllValues);

    if (cd->numPllValues == 0 && numEntries > 0) {
        cirrusBuildPllEntry(25175, &cd->pllValues[0]);
        cd->numPllValues = 1;
    }
}

BOOL ASM CirrusDriver::setDisplay(__REGD0(BOOL state))
{
    BoardInfo *bi = this;
    VgaIo vga = this->vga();
    vga.writeSRMask(0x01, 0x20, (~(UBYTE)state & 1) << 5);
    bi->ChipFlags = (bi->ChipFlags & ~1) | (state & 1);
    return TRUE;
}

BOOL ASM CirrusDriver::getVSyncState(__REGD0(BOOL expected))
{
    BoardInfo *bi = this;
    VgaIo vga = this->vga();
    (void)expected;
    return (vga.readB(VgaReg::INPUT_STATUS1) & 0x08) != 0;
}

ULONG ASM CirrusDriver::getVBeamPos()
{
    BoardInfo *bi = this;
    (void)bi;
    return 0;
}

LONG ASM CirrusDriver::resolvePixelClock(__REGA1(struct ModeInfo *mi),
                                  __REGD0(ULONG desiredPixelClock), __REGD7(RGBFTYPE_REG rgbFormat))
{
    BoardInfo *bi = this;
    (void)rgbFormat;
    DFUNC(VERBOSE, "desiredPixelClock=%ld Hz\n", desiredPixelClock);

    if (!mi)
        return desiredPixelClock;

    const ChipData_t *cd = getConstChipData(bi);
    if (!cd->pllValues || cd->numPllValues == 0) {
        DFUNC(ERROR, "PLL table not initialized\n");
        return desiredPixelClock;
    }

    UWORD targetFreq10khz = (UWORD)(desiredPixelClock / 10000);

    UWORD upper     = cd->numPllValues - 1;
    UWORD upperFreq = cd->pllValues[upper].freq10khz;
    UWORD lower     = 0;
    UWORD lowerFreq = cd->pllValues[lower].freq10khz;

    while (lower + 1 < upper) {
        UWORD middle     = (upper + lower) / 2;
        UWORD middleFreq = cd->pllValues[middle].freq10khz;

        if (targetFreq10khz > middleFreq) {
            lower     = middle;
            lowerFreq = middleFreq;
        } else {
            upper     = middle;
            upperFreq = middleFreq;
        }
    }

    if ((ULONG)targetFreq10khz - (ULONG)lowerFreq > (ULONG)upperFreq - (ULONG)targetFreq10khz) {
        lower     = upper;
        lowerFreq = upperFreq;
    }

    mi->PixelClock = (ULONG)lowerFreq * 10000;

    mi->Flags &= ~GMF_DOUBLECLOCK;
    if ((ULONG)lowerFreq * 10 <= MIN_PLLCLOCK_KHZ)
        mi->Flags |= GMF_DOUBLECLOCK;

    const CirrusPLLValue_t *pv = &cd->pllValues[lower];
    mi->pll1.Numerator         = pv->nom;
    mi->pll2.Denominator       = ((UWORD)pv->den << 8) | (pv->div & 1);

    return (LONG)lower;
}

ULONG ASM CirrusDriver::getPixelClock(__REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD7(RGBFTYPE_REG rgbFormat))
{
    BoardInfo *bi = this;
    (void)mi;
    (void)rgbFormat;
    const ChipData_t *cd = getConstChipData(bi);
    if (!cd->pllValues || index >= cd->numPllValues)
        return 0;
    return (ULONG)cd->pllValues[index].freq10khz * 10000;
}

void CirrusDriver::programVclk(UBYTE nom, UBYTE denRaw, UBYTE divBit)
{
    BoardInfo *bi = this;
    VgaIo vga = this->vga();

    int tmp = (int)denRaw << 1;
    if (divBit)
        tmp |= 1;

    ChipFamily_t fam = (ChipFamily_t)getChipData(bi)->chipFamily;
    if (fam == GD5434 || fam == GD5436 || fam == GD5446 || fam == GD5480)
        tmp |= 0x80;

    vga.writeSR(SR_VCLK_NUM, nom);
    vga.writeSR(SR_VCLK_DEN, (UBYTE)tmp);

    /* Select VCLK0 in SR1B[3:2] */
    vga.writeSR(0x1B, (UBYTE)(vga.readSR(0x1B) & ~(UBYTE)0x0C));
}

void ASM CirrusDriver::setClock()
{
    BoardInfo *bi = this;
    struct ModeInfo *mi = bi->ModeInfo;
    if (!mi) {
        DFUNC(ERROR, "ModeInfo is NULL\n");
        return;
    }
    VgaIo vga = this->vga();
    if (mi->Flags & GMF_DOUBLECLOCK)
        vga.writeSRMask(0x01, BIT(3), BIT(3));
    else
        vga.writeSRMask(0x01, BIT(3), 0);

    UBYTE nom = (UBYTE)mi->pll1.Numerator;
    UBYTE den = (UBYTE)(mi->pll2.Denominator >> 8);
    UBYTE div = (UBYTE)(mi->pll2.Denominator & 1);

    this->programVclk(nom, den, div);
}

UWORD ASM CirrusDriver::calculateBytesPerRow(__REGD0(UWORD width), __REGD1(UWORD height),
                                      __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE_REG rgbFormat))
{
    BoardInfo *bi = this;
    (void)height;
    if (mi) {
        if (width <= 512)
            width = (width + 7) & ~7;
        else if (width <= 640)
            width = 640;
        else if (width <= 800)
            width = 800;
        else if (width <= 1024)
            width = 1024;
        else if (width <= 1152)
            width = 1152;
        else if (width <= 1280)
            width = 1280;
        else if (width <= 1600)
            width = 1600;
        else
            return 0;
        return (width * getBPP(rgbFormat) + 7) & ~7;
    }
    return width * getBPP(rgbFormat);
}

APTR ASM CirrusDriver::calculateMemory(__REGA1(APTR mem), __REGD0(struct RenderInfo *ri),
                                __REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    (void)bi;
    (void)ri;
    (void)format;
    return mem;
}

ULONG ASM CirrusDriver::getCompatibleFormats(__REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    (void)bi;
    if (format == RGBFB_NONE)
        return 0;
    return RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;
}

void CirrusDriver::setSequencerPathWidth(RGBFTYPE_REG format)
{
    BoardInfo *bi = this;
    /* CL-GD5446 SR7[3:1] selects sequencer/CRTC clocking control (pixel datapath width). */
    if (getChipData(bi)->chipFamily != GD5446 && getChipData(bi)->chipFamily != GD5440) {
        return;
    }
    VgaIo vga = this->vga();

    UBYTE modeBits = 0; /* default 8-bpp */
    switch ((RGBFTYPE)format) {
    case RGBFB_CLUT:
    case RGBFB_NONE:
        modeBits = 0b000;
        break;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        modeBits = 0b010; /* 24-bpp */
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
    case RGBFB_B5G6R5PC:
    case RGBFB_B5G5R5PC:
        modeBits = 0b101; /* 16-bpp (also used for clock-doubled 8-bpp) */
        break;
    case RGBFB_A8R8G8B8:
    case RGBFB_B8G8R8A8:
    case RGBFB_R8G8B8A8:
    case RGBFB_A8B8G8R8:
        modeBits = 0b110; /* 32-bpp */
        break;
    default:
        modeBits = 0b000;
        break;
    }

    /* Preserve linear frame buffer enable bits [7:4]. */
    UBYTE sr7 = vga.readSR(0x07);
    sr7 &= (UBYTE) ~(0x0E);        /* clear [3:1] */
    sr7 |= (UBYTE)(modeBits << 1); /* set [3:1] */
    vga.writeSR(0x07, sr7);
}

void ASM CirrusDriver::setDAC(__REGD0(UWORD region), __REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    (void)region;
    DFUNC(INFO, "format=%ld\n", (ULONG)format);

    if (format >= RGBFB_MaxFormats) {
        DFUNC(ERROR, "Invalid format %ld\n", (ULONG)format);
        return;
    }

    ChipData_t *cd = getChipData(bi);
    cd->GEFormat   = (UBYTE)format;
    cd->GEbppLog2  = getBPPLog2((RGBFTYPE)format);

    this->setSequencerPathWidth(format);

    UBYTE hdr = 0x00;

    switch ((RGBFTYPE)format) {
    case RGBFB_CLUT:
    case RGBFB_NONE:
        hdr = 0x00;
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
        hdr = (bi->ModeInfo && (bi->ModeInfo->Flags & GMF_DOUBLECLOCK)) ? (UBYTE)0xE1 : (UBYTE)0xC1;
        break;
    case RGBFB_A8R8G8B8:
    case RGBFB_B8G8R8A8:
    case RGBFB_R8G8B8A8:
    case RGBFB_A8B8G8R8:
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        hdr = 0xC5;
        break;
    default:
        DFUNC(WARN, "SetDAC: format %ld — using 16bpp HDR\n", (ULONG)format);
        hdr = 0xC1;
        break;
    }

    this->writeHDR(hdr);
}

void ASM CirrusDriver::setColorArray(__REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    BoardInfo *bi = this;
    LOCAL_SYSBASE();
    Disable();
    VgaIo vga = this->vga();
    vga.writeB(VgaReg::DAC_WR_INDEX, startIndex);

    struct CLUTEntry *entry = &bi->CLUT[startIndex];
    for (UWORD c = 0; c < count; ++c) {
        vga.writeB(VgaReg::DAC_PEL_DATA, entry->Red);
        vga.writeB(VgaReg::DAC_PEL_DATA, entry->Green);
        vga.writeB(VgaReg::DAC_PEL_DATA, entry->Blue);
        ++entry;
    }

    if (startIndex == 0) {
        vga.readB(VgaReg::INPUT_STATUS1);
        if (bi->ModeInfo->Depth <= 8)
            vga.writeAR(0x11, 0);
        else
            vga.writeAR(0x11, (UBYTE)((bi->CLUT[0].Red & 0xE0) | ((bi->CLUT[0].Green >> 3) & 0x1C) | (bi->CLUT[0].Blue >> 6)));
        vga.writeB(VgaReg::ATTR_AD, 0x20);
    }
    Enable();
}

ULONG CirrusDriver::getMemoryOffset(APTR memory) const
{
    return (ULONG)memory - (ULONG)this->MemoryBase;
}

void ASM CirrusDriver::setPanning(__REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD3(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset),
                           __REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    VgaIo vga = this->vga();
    (void)height;

    bi->XOffset     = xoffset;
    bi->YOffset     = yoffset;
    ULONG memOffset = this->getMemoryOffset(memory);

    LONG panOffset;
    UWORD pitch;

    switch ((RGBFTYPE)format) {
    case RGBFB_NONE:
        pitch     = width >> 3;
        panOffset = (LONG)yoffset * (width >> 3) + (xoffset >> 3);
        break;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        pitch     = width * 3;
        panOffset = ((LONG)yoffset * width + xoffset) * 3;
        break;
    case RGBFB_A8R8G8B8:
    case RGBFB_A8B8G8R8:
    case RGBFB_R8G8B8A8:
    case RGBFB_B8G8R8A8:
        pitch     = width * 4;
        panOffset = ((LONG)yoffset * width + xoffset) * 4;
        break;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
    case RGBFB_B5G6R5PC:
    case RGBFB_B5G5R5PC:
        pitch     = width * 2;
        panOffset = ((LONG)yoffset * width + xoffset) * 2;
        break;
    default:
        pitch     = width;
        panOffset = (LONG)yoffset * width + xoffset;
        break;
    }

    pitch     = (pitch + 7) / 8;
    panOffset = (panOffset + (LONG)memOffset) / 4;

    vga.writeCROverflow2U((ULONG)panOffset, 0x0d, 0, 8, 0x0c, 0, 8, 0x1c, 0, 4);
    vga.writeCROverflow1((UWORD)pitch, 0x13, 0, 8, 0x1c, 4, 4);
}

static INLINE REGARGS UWORD toScanLines(UWORD y, UWORD modeFlags)
{
    if (modeFlags & GMF_DOUBLESCAN)
        y *= 2;
    if (modeFlags & GMF_INTERLACE)
        y /= 2;
    return y;
}

static INLINE REGARGS UWORD adjustBorder(UWORD x, BOOL border, UWORD defaultX)
{
    if (!border || x == 0)
        x = defaultX;
    return x;
}

void ASM CirrusDriver::setGC(__REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    BoardInfo *bi = this;
    bi->ModeInfo = mi;
    bi->Border   = border;

    UWORD hTotal      = mi->HorTotal;
    UWORD screenWidth = mi->Width;
    UBYTE modeFlags   = mi->Flags;
    BOOL isInterlaced = (modeFlags & GMF_INTERLACE) ? 1 : 0;

#define ADJUST_HBORDER(x) adjustBorder((x), border, 8)
#define ADJUST_VBORDER(y) adjustBorder((y), border, 1);
#define TO_CLKS(x)        ((x) >> 3)
#define TO_SCANLINES(y)   toScanLines((y), modeFlags)
    VgaIo vga = this->vga();

    {
        UWORD hTotalClk = TO_CLKS(hTotal) - 5;
        vga.writeCROverflow1(hTotalClk, 0x00, 0, 8, 0x1B, 0, 1);
        vga.writeCROverflow1(hTotalClk >> 1, 0x19, 0, 8, 0x1B, 4, 1);
    }

    {
        UWORD hDisplayEnd = TO_CLKS(screenWidth) - 1;
        vga.writeCROverflow1(hDisplayEnd, 0x01, 0, 8, 0x1B, 1, 1);
    }

    UWORD hBorderSize = ADJUST_HBORDER(mi->HorBlankSize);
    {
        UWORD hBlankStart = TO_CLKS(screenWidth + hBorderSize);
        vga.writeCROverflow1(hBlankStart, 0x02, 0, 8, 0x1B, 2, 1);
    }

    {
        UWORD hBlankEnd = TO_CLKS(hTotal - hBorderSize) - 1;
        vga.writeCROverflow1(hBlankEnd, 0x03, 0, 5, 0x05, 7, 1);
    }

    {
        UWORD hSyncStart = TO_CLKS(screenWidth + mi->HorSyncStart);
        vga.writeCROverflow1(hSyncStart, 0x04, 0, 8, 0x1B, 3, 1);
    }

    {
        UWORD endHSync = TO_CLKS(screenWidth + mi->HorSyncStart + mi->HorSyncSize) - 1;
        vga.writeCRMask(0x05, 0x1f, endHSync);
    }

    {
        UWORD vTotal = TO_SCANLINES(mi->VerTotal) - 2;
        vga.writeCROverflow3(vTotal, 0x06, 0, 8, 0x07, 0, 1, 0x07, 5, 1, 0x1A, 0, 1);
    }

    {
        UWORD vDisplayEnd = TO_SCANLINES(mi->Height) - 1;
        vga.writeCROverflow3(vDisplayEnd, 0x12, 0, 8, 0x07, 1, 1, 0x07, 6, 1, 0x1A, 1, 1);
    }

    {
        UWORD vRetraceStart = TO_SCANLINES(mi->Height + mi->VerSyncStart);
        vga.writeCROverflow3(vRetraceStart, 0x10, 0, 8, 0x07, 2, 1, 0x07, 7, 1, 0x1A, 3, 1);
    }

    {
        UWORD vRetraceEnd = TO_SCANLINES(mi->Height + mi->VerSyncStart + mi->VerSyncSize) - 1;
        vga.writeCRMask(0x11, 0x0F, vRetraceEnd);
    }

    UWORD vBlankSize = ADJUST_VBORDER(mi->VerBlankSize);
    {
        UWORD vBlankStart = TO_SCANLINES(mi->Height + vBlankSize);
        vga.writeCROverflow3(vBlankStart, 0x15, 0, 8, 0x07, 3, 1, 0x09, 5, 1, 0x1A, 2, 1);
    }

    {
        UWORD vBlankEnd = TO_SCANLINES(mi->VerTotal - vBlankSize) - 1;
        vga.writeCR(0x16, vBlankEnd);
    }

    {
        UBYTE cr1a = vga.readCR(0x1A);
        if (isInterlaced)
            cr1a |= 1;
        else
            cr1a &= (UBYTE)~1;
        vga.writeCR(0x1A, cr1a);
    }

    {
        UBYTE dblScan = vga.readCR(0x9) & 0x7f;
        if ((modeFlags & GMF_DOUBLESCAN) != 0)
            dblScan |= 0x80;
        vga.writeCR(0x9, dblScan);
    }

    {
        UBYTE polarities = 0;
        if ((modeFlags & GMF_HPOLARITY) != 0)
            polarities |= 0x40;
        if ((modeFlags & GMF_VPOLARITY) != 0)
            polarities |= 0x80;
        vga.writeMiscMask(0xC0, polarities);
    }
#undef ADJUST_HBORDER
#undef ADJUST_VBORDER
#undef TO_CLKS
#undef TO_SCANLINES
}

void ASM CirrusDriver::setMemoryMode(__REGD7(RGBFTYPE_REG format))
{
    BoardInfo *bi = this;
    (void)bi;
    (void)format;
}

void ASM CirrusDriver::waitBlitter()
{
    BoardInfo *bi = this;
    (void)bi;
}

void ASM CirrusDriver::waitVerticalSync(__REGD0(BOOL waitForEnd))
{
    BoardInfo *bi = this;
    if (!(bi->ChipFlags & 1))
        return;
    VgaIo vga = this->vga();
    if (waitForEnd) {
        while (!(vga.readB(VgaReg::INPUT_STATUS1) & 0x08)) {
        }
        while (!(vga.readB(VgaReg::INPUT_STATUS1) & 0x01)) {
        }
    } else {
        while (!(vga.readB(VgaReg::INPUT_STATUS1) & 0x01)) {
        }
        while (!(vga.readB(VgaReg::INPUT_STATUS1) & 0x08)) {
        }
    }
}

void ASM CirrusDriver::setWriteMask(__REGD0(UBYTE mask))
{
    BoardInfo *bi = this;
    (void)bi;
    (void)mask;
}
void ASM CirrusDriver::setClearMask(__REGD0(UBYTE mask))
{
    BoardInfo *bi = this;
    (void)bi;
    (void)mask;
}
void ASM CirrusDriver::setReadPlane(__REGD0(UBYTE mask))
{
    BoardInfo *bi = this;
    (void)bi;
    (void)mask;
}


/* P96 BoardInfo entry stubs */

static BOOL ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
    return asCirrus(bi)->setDisplay(state);
}
static BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL expected))
{
    return asCirrus(bi)->getVSyncState(expected);
}
static ULONG ASM GetVBeamPos(__REGA0(struct BoardInfo *bi))
{
    return asCirrus(bi)->getVBeamPos();
}
static LONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi),
                                  __REGD0(ULONG desiredPixelClock), __REGD7(RGBFTYPE rgbFormat))
{
    return asCirrus(bi)->resolvePixelClock(mi, desiredPixelClock, rgbFormat);
}
static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(ULONG index),
                               __REGD7(RGBFTYPE rgbFormat))
{
    return asCirrus(bi)->getPixelClock(mi, index, rgbFormat);
}
static void ASM SetClock(__REGA0(struct BoardInfo *bi))
{
    asCirrus(bi)->setClock();
}
static UWORD ASM CalculateBytesPerRow(__REGA0(struct BoardInfo *bi), __REGD0(UWORD width), __REGD1(UWORD height),
                                      __REGA1(struct ModeInfo *mi), __REGD7(RGBFTYPE rgbFormat))
{
    return asCirrus(bi)->calculateBytesPerRow(width, height, mi, rgbFormat);
}
static APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi), __REGA1(APTR mem), __REGD0(struct RenderInfo *ri),
                                __REGD7(RGBFTYPE format))
{
    return asCirrus(bi)->calculateMemory(mem, ri, format);
}
static ULONG ASM GetCompatibleFormats(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    return asCirrus(bi)->getCompatibleFormats(format);
}
static void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD0(UWORD region), __REGD7(RGBFTYPE format))
{
    asCirrus(bi)->setDAC(region, format);
}
static void ASM SetColorArray(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    asCirrus(bi)->setColorArray(startIndex, count);
}
static void ASM SetPanning(__REGA0(struct BoardInfo *bi), __REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD3(UWORD height), __REGD1(WORD xoffset), __REGD2(WORD yoffset),
                           __REGD7(RGBFTYPE format))
{
    asCirrus(bi)->setPanning(memory, width, height, xoffset, yoffset, format);
}
static void ASM SetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    asCirrus(bi)->setGC(mi, border);
}
static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
    asCirrus(bi)->setMemoryMode(format);
}
static void ASM WaitBlitter(__REGA0(struct BoardInfo *bi))
{
    asCirrus(bi)->waitBlitter();
}
static void ASM WaitVerticalSync(__REGA0(struct BoardInfo *bi), __REGD0(BOOL waitForEnd))
{
    asCirrus(bi)->waitVerticalSync(waitForEnd);
}
static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    asCirrus(bi)->setWriteMask(mask);
}
static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    asCirrus(bi)->setClearMask(mask);
}
static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
    asCirrus(bi)->setReadPlane(mask);
}

BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    DFUNC(ALWAYS, "Cirrus GD54xx InitChip\n");

    LOCAL_SYSBASE();

    ChipData_t *cd      = getChipData(bi);
    ChipFamily_t family = (ChipFamily_t)cd->chipFamily;
    VgaIo vga           = asCirrus(bi)->vga();
    CirrusIo io         = asCirrus(bi)->io();

    /* VBIOS chipWakeup PCI path (46E8=0x16/0x0E + 4AE8). */
    io.writeB(IoReg::ADAPTER_SLEEP, 0x16);
    io.writeB(IoReg::POS102, 0x01);
    io.writeB(IoReg::ADAPTER_SLEEP, 0x0E);
    io.writeB(IoReg::ADAPTER_SLEEP2, 0x00);
    vga.writeB(VgaReg::VGA_ENABLE, 0x01);

    /* VBIOS: SR16=0x58 before unlock, then extended seq / DRAM timing. */
    vga.writeSR(0x16, 0x58);
    vga.writeSR(0x06, 0x12);
    initExtendedSequencerMemory(vga);

    /* MISC: VCLK3 + RAM enable + color (SDK); CRTC needs a live VCLK for HBLANK. */
    vga.writeB(VgaReg::MISC_OUT_W, 0x0F);

    vga.writeSR(0x01, BIT(5) | BIT(0));
    vga.writeSR(0x00, 0x03);
    vga.writeSR(0x02, 0xFF);
    vga.writeSR(0x03, 0x00);
    vga.writeSR(0x04, 0x0E);
    vga.writeSR(0x18, 0x02);
    /* VCLK3 like SDK (after VBIOS clock table). */
    vga.writeSR(0x0E, 0x65);
    vga.writeSR(0x1E, 0x3B);
    bi->MemoryClock = 50113630;

    startCrtcForDramLatch(vga);

    /*
     * SRF[4:3] only takes effect at HBLANK. 0x38 = 64-bit/2MB + CRT FIFO (SDK).
     * Alpine has no bank bit — do not use 0x98.
     */
    vga.writeSR(0x0F, 0x38);
    vga.writeSR(0x1F, 0x1C);
    waitDisplayBlankEdges(vga, 4);
    D(INFO, "After CRTC+SRF latch: SRF=0x%02lx INPUT_STATUS1=0x%02lx\n", (ULONG)vga.readSR(0x0F),
      (ULONG)vga.readB(VgaReg::INPUT_STATUS1));

    UBYTE chipId     = vga.readCR(0x27) >> 2;
    UBYTE classId    = vga.readCR(0x28);
    BOOL ddc2Support = (classId == 0x01 || classId == 0x03);

    D(INFO, "Detected chip ID: 0x%02lX == %ld, class 0x%02lX\n", (ULONG)chipId, (ULONG)chipId, (ULONG)classId);
    D(INFO, "Using chip family: %s\n", getChipFamilyName(family));

    {
        LOCAL_OPENPCIBASE();
        struct pci_dev *board = getCardData(bi)->board;
        ULONG bar0            = pci_read_config_long(PCI_BASE_ADDRESS_0, board);
        ULONG rom             = pci_read_config_long(PCI_ROM_ADDRESS, board);
        UWORD command         = pci_read_config_word(PCI_COMMAND, board);

        D(INFO, "BAR0: 0x%08lx, ROM: 0x%08lx, COMMAND 0x%04lx\n", bar0, rom, (ULONG)command);

        pci_write_config_long(PCI_BASE_ADDRESS_0, bar0, board);
        pci_write_config_word(PCI_COMMAND, command | PCI_COMMAND_MEMORY | PCI_COMMAND_IO, board);

        /* ROM responds ⇒ PCI memory decode works; keep enabled until LFB proven. */
        if (rom & PCI_ROM_ADDRESS_ENABLE) {
            APTR romHost = pci_physic_to_logic_addr((APTR)(rom & PCI_ROM_ADDRESS_MASK), board);
            D(INFO, "ROM host 0x%08lx\n", (ULONG)romHost);
            if (romHost) {
                UWORD sig = *(volatile UWORD *)romHost;
                D(INFO, "ROM[0] = 0x%04lx %s\n", (ULONG)sig,
                  (sig == 0xAA55 || sig == 0x55AA) ? "(BIOS sig)" : "");
            }
        }
    }

    vga.writeGR(0x33, 0x00);
    vga.writeGR(0x05, 0x40);
    vga.writeGR(0x06, 0x05);
    vga.writeGR(0x08, 0xFF);
    vga.writeGR(0x0B, 0x20);
    vga.writeGR(0x0E, 0x00);
    vga.writeB(VgaReg::DAC_PEL_MASK, 0xFF);

    /* Linear aperture: SR7[7:4]≠0 + packed. Alpine defaults from cirrusfb. */
    {
        static const UBYTE srfTry[] = {0x38, 0x18, 0x10, 0x08};
        static const UBYTE sr7Try[] = {0xA1, 0xA0, 0x80, 0xF0, 0xE1};
        volatile ULONG *fb          = (volatile ULONG *)bi->MemoryBase;
        BOOL barOk                  = FALSE;
        ULONG r;

        LOCAL_OPENPCIBASE();
        ULONG rom = pci_read_config_long(PCI_ROM_ADDRESS, getCardData(bi)->board);
        if (rom & PCI_ROM_ADDRESS_ENABLE) {
            pci_write_config_long(PCI_ROM_ADDRESS, rom & ~PCI_ROM_ADDRESS_ENABLE,
                                  getCardData(bi)->board);
            D(INFO, "PCI ROM decode disabled before LFB probe\n");
        }

        for (unsigned si = 0; !barOk && si < sizeof(srfTry); ++si) {
            vga.writeSR(0x0F, srfTry[si]);
            waitDisplayBlankEdges(vga, 2);
            for (unsigned i = 0; i < sizeof(sr7Try); ++i) {
                vga.writeSR(0x07, sr7Try[i]);
                r = fb[0];
                D(INFO, "BAR0 SRF=0x%02lx SR7=0x%02lx read 0x%08lx\n", (ULONG)srfTry[si],
                  (ULONG)sr7Try[i], r);
                if (r == 0xFFFFFFFFUL)
                    continue;
                fb[0] = 0xA5A5A5A5UL;
                CacheClearU();
                r = fb[0];
                D(INFO, "BAR0 write/read 0x%08lx %s\n", r, r == 0xA5A5A5A5UL ? "OK" : "FAIL");
                if (r == 0xA5A5A5A5UL) {
                    barOk = TRUE;
                    break;
                }
            }
        }

        D(INFO, "SR7=0x%02lx MISC=0x%02lx SR4=0x%02lx SRF=0x%02lx SR1F=0x%02lx SR16=0x%02lx GR0B=0x%02lx\n",
          (ULONG)vga.readSR(0x07), (ULONG)vga.readB(VgaReg::MISC_OUT_R), (ULONG)vga.readSR(0x04),
          (ULONG)vga.readSR(0x0F), (ULONG)vga.readSR(0x1F), (ULONG)vga.readSR(0x16),
          (ULONG)vga.readGR(0x0B));

        if (!barOk) {
            DFUNC(ERROR, "Linear framebuffer not responding at 0x%08lx\n", (ULONG)bi->MemoryBase);
            return FALSE;
        }
    }

    ULONG maxTry    = (family == GD5434 || family == GD5436 || family == GD5446 || family == GD5480)
                          ? 4UL * 1024 * 1024
                          : 2UL * 1024 * 1024;
    UBYTE srfBefore = vga.readSR(0x0F);
    programSrfDramControl(vga, cirrusSrfDramBitsForSize(maxTry, family));
    D(INFO, "SRF before size probe: 0x%02lx -> 0x%02lx\n", (ULONG)srfBefore, (ULONG)vga.readSR(0x0F));

    ULONG probed = probeFramebufferSize(bi);
    if (!probed)
        probed = 512UL * 1024;

    programSrfDramControl(vga, cirrusSrfDramBitsForSize(probed, family));
    bi->MemorySize = probed;
    D(INFO, "VRAM probe %ld KB, SRF now 0x%02lx (decode %ld KB)\n", probed / 1024,
      (ULONG)vga.readSR(0x0F), cirrusMemSizeFromSrf(vga.readSR(0x0F), family) / 1024);

    /* Alpine: MMIO in last 256 bytes of linear aperture (after VRAM is known good). */
    if (family == GD5430 || family == GD5436 || family == GD5440) {
        vga.writeSRMask(0x17, BIT(6) | BIT(2), BIT(6) | BIT(2));
        vga.writeGR(0x06, (UBYTE)((vga.readGR(0x06) & ~(0x3 << 2)) | (0b01 << 2)));
    }

    vga.writeGR(0x00, 0x00);
    vga.writeGR(0x01, 0x00);
    vga.writeGR(0x02, 0x00);
    vga.writeGR(0x03, 0x00);
    vga.writeGR(0x04, 0x00);
    vga.writeGR(0x05, 0x00);

    // Enable Graphics Mode
    vga.writeGR(0x06, (UBYTE)((vga.readGR(0x06) & ~(0x03)) | ((0x01) & (0x03))));
    vga.writeGR(0x07, 0x0F);
    vga.writeGR(0x08, 0xFF);

    vga.writeCR(0x08, 0x00);
    vga.writeCR(0x09, 0x00);
    vga.writeCR(0x0A, 0x00);
    vga.writeCR(0x0B, 0x00);
    vga.writeCR(0x0C, 0x00);
    vga.writeCR(0x0D, 0x00);
    vga.writeCR(0x0E, 0x00);
    vga.writeCR(0x0F, 0x00);
    vga.writeCR(0x11, 0x20);
    vga.writeCR(0x11, 0x30);
    vga.writeCR(0x1C, 0x00);
    vga.writeCR(0x13, 0x50);
    vga.writeCR(0x14, BIT(6));
    vga.writeCR(0x17, BIT(7) | BIT(6) | BIT(5) | BIT(0));
    vga.writeCR(0x18, 0xff);

    vga.readB(VgaReg::INPUT_STATUS1);
    vga.writeB(VgaReg::ATTR_AD, 0x00);
    vga.readB(VgaReg::INPUT_STATUS1);
    {
        int p;
        for (p = 0; p < 16; ++p)
            vga.writeAR((UBYTE)p, (UBYTE)p);
        vga.writeAR(0x10, 0x61);
        vga.readB(VgaReg::INPUT_STATUS1);
        vga.writeB(VgaReg::ATTR_AD, 0x20);
    }

    vga.writeB(VgaReg::DAC_PEL_MASK, 0xFF);

    vga.writeMiscMask(0x0F, 0x0F);

    switch (cd->chipFamily) {
    case GD5446:
    case GD5440:
        bi->GraphicsControllerType = GCT_CirrusGD5446;
        bi->PaletteChipType        = PCT_CirrusGD5446;
        break;
    case GD5434:
        bi->GraphicsControllerType = GCT_CirrusGD5434;
        bi->PaletteChipType        = PCT_CirrusGD5434;
        break;
    default:
        bi->GraphicsControllerType = GCT_CirrusGD542x;
        bi->PaletteChipType        = PCT_CirrusGD542x;
        break;
    }
    bi->Flags |= BIF_GRANTDIRECTACCESS | BIF_VGASCREENSPLIT;

    bi->RGBFormats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;

    initPixelClockPLLTable(bi);

#ifndef TESTEXE
    queryEDID(bi);
#endif

    bi->MaxBMWidth    = 2048;
    bi->MaxBMHeight   = 2048;
    bi->BitsPerCannon = 8;

    bi->MaxHorValue[PLANAR] = bi->MaxHorValue[CHUNKY] = bi->MaxHorValue[HICOLOR] = bi->MaxHorValue[TRUECOLOR] =
        bi->MaxHorValue[TRUEALPHA]                                               = 4093;
    bi->MaxVerValue[PLANAR] = bi->MaxVerValue[CHUNKY] = bi->MaxVerValue[HICOLOR] = bi->MaxVerValue[TRUECOLOR] =
        bi->MaxVerValue[TRUEALPHA]                                               = 2047;
    bi->MaxHorResolution[PLANAR] = bi->MaxVerResolution[PLANAR] = 2048;
    bi->MaxHorResolution[CHUNKY] = bi->MaxVerResolution[CHUNKY] = 2048;
    bi->MaxHorResolution[HICOLOR] = bi->MaxVerResolution[HICOLOR] = 2048;
    bi->MaxHorResolution[TRUECOLOR] = bi->MaxVerResolution[TRUECOLOR] = 2048;
    bi->MaxHorResolution[TRUEALPHA] = bi->MaxVerResolution[TRUEALPHA] = 2048;

    P96_HOOK(bi->SetGC, SetGC);
    P96_HOOK(bi->SetPanning, SetPanning);
    P96_HOOK(bi->CalculateBytesPerRow, CalculateBytesPerRow);
    P96_HOOK(bi->CalculateMemory, CalculateMemory);
    P96_HOOK(bi->GetCompatibleFormats, GetCompatibleFormats);
    P96_HOOK(bi->SetDAC, SetDAC);
    P96_HOOK(bi->SetColorArray, SetColorArray);
    P96_HOOK(bi->SetDisplay, SetDisplay);
    P96_HOOK(bi->SetMemoryMode, SetMemoryMode);
    P96_HOOK(bi->ResolvePixelClock, ResolvePixelClock);
    P96_HOOK(bi->GetPixelClock, GetPixelClock);
    P96_HOOK(bi->SetClock, SetClock);
    P96_HOOK(bi->SetWriteMask, SetWriteMask);
    P96_HOOK(bi->SetReadPlane, SetReadPlane);
    P96_HOOK(bi->SetClearMask, SetClearMask);
    P96_HOOK(bi->GetVSyncState, GetVSyncState);
    P96_HOOK(bi->GetVBeamPos, GetVBeamPos);
    P96_HOOK(bi->WaitVerticalSync, WaitVerticalSync);

    P96_HOOK(bi->WaitBlitter, WaitBlitter);
    bi->FillRect               = bi->FillRectDefault;
    bi->InvertRect             = bi->InvertRectDefault;
    bi->BlitRectNoMaskComplete = bi->BlitRectNoMaskCompleteDefault;
    bi->BlitRect               = bi->BlitRectDefault;
    bi->BlitTemplate           = bi->BlitTemplateDefault;
    bi->BlitPlanar2Chunky      = bi->BlitPlanar2ChunkyDefault;
    bi->DrawLine               = bi->DrawLineDefault;
    bi->BlitPattern            = bi->BlitPatternDefault;

    (void)cd;
    return TRUE;
}

#ifdef TESTEXE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct UtilityBase *UtilityBase;

static struct BoardInfo boardInfo = {0};

static void sigIntHandler(int dummy)
{
    (void)dummy;
    abort();
}

int main(int argc, char **argv)
{
    signal(SIGINT, sigIntHandler);

    BOOL doEdid = FALSE;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "edid") == 0 || strcmp(argv[i], "EDID") == 0)
            doEdid = TRUE;
    }

    int rval = EXIT_FAILURE;
    memset(&boardInfo, 0, sizeof(boardInfo));
    struct BoardInfo *bi = &boardInfo;
    bi->ExecBase         = SysBase;
    bi->UtilBase         = (struct Library *)UtilityBase;

    struct Library *OpenPciBase = NULL;
    CardData_t *card            = getCardData(bi);
    struct pci_dev *board       = NULL;

    OpenPciBase = OpenLibrary("openpci.library", MIN_OPENPCI_VERSION);
    if (!OpenPciBase) {
        DFUNC(ERROR, "openpci.library missing\n");
        goto exit;
    }

    card->OpenPciBase = OpenPciBase;

    board = FindBoard(NULL, PRM_Vendor, VENDOR_ID_CIRRUS, TAG_END);
    if (!board) {
        DFUNC(ERROR, "No Cirrus PCI device found\n");
        goto exit;
    }
    card->board = board;

    if (!initRegisterAndMemoryBases(bi)) {
        DFUNC(ERROR, "initRegisterAndMemoryBases failed\n");
        goto exit;
    }

    if (!InitChip(bi)) {
        DFUNC(ERROR, "InitChip failed\n");
        goto exit;
    }

    if (doEdid) {
        DFUNC(INFO, "TestCirrus: EDID read (DDC2B / SR8)\n");
        queryEDID(bi);
    }
    rval = EXIT_SUCCESS;
exit:
    if (OpenPciBase)
        CloseLibrary(OpenPciBase);
    return rval;
}

#endif

#ifdef __cplusplus
}
#endif

