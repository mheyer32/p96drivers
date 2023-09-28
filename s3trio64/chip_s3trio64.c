#include "chip_s3trio64.h"

#define __NOLIBBASE__

#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/prometheus.h>

#include <SDI_stdarg.h>

#ifndef DBG
#define D(...)
#else
#define D(...) KPrintF(__VA_ARGS__)
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

static inline u32 abs_diff(u32 a, u32 b)
{
  return (a > b) ? (a - b) : (b - a);
}

struct svga_pll
{
  u16 m_min;
  u16 m_max;
  u16 n_min;
  u16 n_max;
  u16 r_min;
  u16 r_max; /* r_max < 32 */
  u32 f_vco_min;
  u32 f_vco_max;
  u32 f_base;
};

// f_wanted is in Khz
int svga_compute_pll(const struct svga_pll *pll, u32 f_wanted, u16 *m, u16 *n,
                     u16 *r)
{
  u16 am, an, ar;
  u32 f_vco, f_current, delta_current, delta_best;

#ifdef DBG
  KPrintF("ideal frequency: %ld kHz\n", (unsigned int)f_wanted);
#endif

  ar = pll->r_max;
  f_vco = f_wanted << ar;

  /* overflow check */
  if ((f_vco >> ar) != f_wanted)
    return -1;

  /* It is usually better to have greater VCO clock
     because of better frequency stability.
     So first try r_max, then r smaller. */
  while ((ar > pll->r_min) && (f_vco > pll->f_vco_max)) {
    ar--;
    f_vco = f_vco >> 1;
  }

  /* VCO bounds check */
  if ((f_vco < pll->f_vco_min) || (f_vco > pll->f_vco_max))
    return -1;

  delta_best = 0xFFFFFFFF;
  *m = 0;
  *n = 0;
  *r = ar;

  am = pll->m_min;
  an = pll->n_min;

  while ((am <= pll->m_max) && (an <= pll->n_max)) {
    f_current = (pll->f_base * am) / an;
    delta_current = abs_diff(f_current, f_vco);

    if (delta_current < delta_best) {
      delta_best = delta_current;
      *m = am;
      *n = an;
    }

    if (f_current <= f_vco) {
      am++;
    } else {
      an++;
    }
  }

  f_current = (pll->f_base * *m) / *n;
#ifdef DBG1
  KPrintF("found frequency: %ld kHz (VCO %ld kHz)\n", (int)(f_current >> ar),
          (int)f_current);
  KPrintF("m = %ld n = %ld r = %ld\n", (unsigned int)*m, (unsigned int)*n,
          (unsigned int)*r);
#endif

  return (f_current >> ar);
}

static const struct svga_pll s3_pll = {3, 129,   3,      33,   0,
                                       3, 35000, 240000, 14318};

ULONG SetMemoryClock(struct BoardInfo *bi, ULONG clockHz)
{
  REGBASE();

  u16 m, n, r;
  u8 regval;

#ifdef DBG
  KPrintF("original Hz: %ld\n", clockHz);
#endif

  int currentKhz = svga_compute_pll(&s3_pll, clockHz / 1000, &m, &n, &r);
  if (currentKhz < 0) {
    KPrintF("cannot set requested pixclock, keeping old value\n");
    return clockHz;
  }

  /* Set S3 clock registers */
  W_SR(0x10, (n - 2) | (r << 5));
  W_SR(0x11, m - 2);

  LOCAL_DOSBASE();
  Delay(1);

  /* Activate clock - write 0, 1, 0 to seq/15 bit 5 */
  regval = R_SR(0x15); /* | 0x80; */
  W_SR(0x15, regval & ~(1 << 5));
  W_SR(0x15, regval | (1 << 5));
  W_SR(0x15, regval & ~(1 << 5));

  return currentKhz * 1000;
}

static UWORD CalculateBytesPerRow(__REGA0(struct BoardInfo *bi),
                                  __REGD0(UWORD width), __REGD1(UWORD height),
                                  __REGA1(struct ModeInfo *mi),
                                  __REGD7(RGBFTYPE format))
{
  switch (format) {
  case RGBFB_NONE:
    width = width >> 3;
    break;
  case RGBFB_R8G8B8:
  case RGBFB_B8G8R8:
    width = width * 3;
    break;
  case RGBFB_A8R8G8B8:
  case RGBFB_A8B8G8R8:
  case RGBFB_R8G8B8A8:
  case RGBFB_B8G8R8A8:
    width = width * 4;
    break;
  case RGBFB_R5G6B5PC:
  case RGBFB_R5G5B5PC:
  case RGBFB_R5G6B5:
  case RGBFB_R5G5B5:
  case RGBFB_B5G6R5PC:
  case RGBFB_B5G5R5PC:
  case RGBFB_YUV422CGX:
  case RGBFB_YUV422:
  case RGBFB_YUV422PC:
  case RGBFB_YUV422PA:
  case RGBFB_YUV422PAPC:
    width = width * 2;
    break;
  default:
    /* RGBFB_CLUT:  width = width */
    break;
  }
  if (width >= 0x1ff8) {
    width = 0;
  }
  return width;
}

static void ASM SetColorArray(__REGA0(struct BoardInfo *bi),
                              __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
  REGBASE();
  LOCAL_SYSBASE();

  UWORD bppDiff = 8 - bi->BitsPerCannon;

  // This may noty be interrupted, so DAC_WR_AD remains set throughout the
  // function
  Disable();

  W_REG(DAC_WR_AD, startIndex);

  struct CLUTEntry *entry = &bi->CLUT[startIndex];

  // Do not print these individual register writes as it takes ages
  for (UWORD c = 0; c < count; ++c) {
    writeReg(RegBase, DAC_DATA, entry->Red >> bppDiff);
    writeReg(RegBase, DAC_DATA, entry->Green >> bppDiff);
    writeReg(RegBase, DAC_DATA, entry->Blue >> bppDiff);
    ++entry;
  }

  Enable();

  return;
}

static void ASM SetDAC(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE format))
{
  REGBASE();
  static const UBYTE DAC_ColorModes[] = {
      0x00,  // RGBFB_NONE
      0x00,  // RGBFB_CLUT
      0x70,  // RGBFB_R8G8B8
      0x70,  // RGBFB_B8G8R8
      0x50,  // RGBFB_R5G6B5PC
      0x30,  // RGBFB_R5G5B5PC
      0xd0,  // RGBFB_A8R8G8B8
      0xd0,  // RGBFB_A8B8G8R8
      0xd0,  // RGBFB_R8G8B8A8
      0xd0,  // RGBFB_B8G8R8A8
      0x50,  // RGBFB_R5G6B5
      0x30,  // RGBFB_R5G5B5
      0x50,  // RGBFB_B5G6R5PC
      0x30,  // RGBFB_B5G5R5PC
      0x00,  // RGBFB_YUV422CGX
      0x00,  // RGBFB_YUV411
      0x00,  // RGBFB_YUV411PC
      0x00,  // RGBFB_YUV422
      0x00,  // RGBFB_YUV422PC
      0x00,  // RGBFB_YUV422PA
      0x00,  // RGBFB_YUV422PAPC
  };

  UBYTE dacMode;

  if (format < RGBFB_MaxFormats) {
    if ((format == RGBFB_CLUT) &&
        ((bi->ModeInfo->Flags & GMF_DOUBLECLOCK) != 0)) {
      dacMode = 0x12;  // Why is  it setting a reserved bit 0x02 here?
    } else {
      dacMode = DAC_ColorModes[format];
    }
    W_CR_MASK(0x67, 0xF2, dacMode);
  }
  return;
}

static inline REGARGS UWORD ToScanLines(UWORD y, UWORD modeFlags)
{
  if (modeFlags & GMF_DOUBLESCAN)
    y *= 2;
  if (modeFlags & GMF_INTERLACE)
    y /= 2;
  return y;
}

static inline REGARGS UWORD AdjustBorder(UWORD x, BOOL border, UWORD defaultX)
{
  if (!border || x == 0)
    x = defaultX;
  return x;
}

static void ASM SetGC(__REGA0(struct BoardInfo *bi),
                      __REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
  REGBASE();

  UWORD interlaced;
  UBYTE depth;
  UBYTE modeFlags;
  UWORD hTotal;
  UWORD ScreenWidth;

  D("W %ld, H %ld, HTotal %ld, HBlankSize %ld, HSyncStart %ld, HSyncSize %ld, "
    "\nVTotal %ld, VBlankSize %ld,  VSyncStart %ld ,  VSyncSize %ld\n",
    (ULONG)mi->Width, (ULONG)mi->Height, (ULONG)mi->HorTotal,
    (ULONG)mi->HorBlankSize, (ULONG)mi->HorSyncStart, (ULONG)mi->HorSyncSize,
    (ULONG)mi->VerTotal, (ULONG)mi->VerBlankSize, (ULONG)mi->VerSyncStart,
    (ULONG)mi->VerSyncSize);

  bi->ModeInfo = mi;
  bi->Border = border;

  // Disable Clocking Doubling
  W_SR_MASK(0x15, 0x10, 0);
  W_SR_MASK(0x18, 0x80, 0);

  hTotal = mi->HorTotal;
  ScreenWidth = mi->Width;
  modeFlags = mi->Flags;
  interlaced = (modeFlags & GMF_INTERLACE) != 0;

  depth = mi->Depth;
  if (depth <= 8) {
    // Extended System Cont 1 Register (EX_SCTL_11 (CR50)
    // 00 = 1 byte (Default). This corresponds to a pixel length of 4 or 8
    // bits/pixel in bit 7 of the Subsystem Status register (42E8H)
    W_CR(0x50, 0x0);
    if ((border == 0) || ((mi->HorBlankSize == 0 || (mi->VerBlankSize == 0)))) {
      D("8-Bit Mode, NO Border\n");
      // Bit 5 BDR SEL - Blank/Border Select
      // 1 = BLANK is active during entire display inactive period (no border)
      W_CR_MASK(0x33, 0x20, 0x20);
    } else {
      D("8-Bit Mode, Border\n");
      // Bit 5 BDR SEL - Blank/Border Select
      // o = BLANK active time is defined by CR2 and CR3
      W_CR_MASK(0x33, 0x20, 0x0);
    }
    if (modeFlags & GMF_DOUBLECLOCK) {
      // CLKSVN Control 2 Register (SR15)
      // Bit 4 DCLK/2 - Divide DCLK by 2
      // Either this bit or bit 6 of this register must be set to 1 for clock
      // doubled RAMDAC operation (mode 0001).
      W_SR_MASK(0x15, 0x10, 0x10);
      // RAMDAC/CLKSVN Control Register (SR18)
      // Bit 7 CLKx2 - Enable clock doubled mode
      // 1 = RAMDAC clock doubled mode (0001) enabled
      // This bit must be set to 1 when mode 0001 is specified in bits 7-4
      // of CR67 or SRC. Either bit 4 or bit 6 of SR15 must also be set to 1.
      W_SR_MASK(0x18, 0x80, 0x80);
    }
  } else if (depth <= 16) {
    D("16-Bit Mode, No Border\n");
    // Bit 5 BDR SEL - Blank/Border Select
    // o = BLANK active time is defined by CR2 and CR3
    W_CR_MASK(0x33, 0x20, 0x0);
    // Extended System Cont 1 Register (EX_SCTL_11 (CR50)
    // 01 = 2 bytes. 16 bits/pixel
    W_CR(0x50, 0x10);

    //FIXME: what is this?
    hTotal = hTotal * 2;
    ScreenWidth = ScreenWidth * 2 + 6;
    border = 0;
  } else {
    D("24-Bit Mode, No Border\n");
    // Bit 5 BDR SEL - Blank/Border Select
    // 0 = BLANK active time is defined by CR2 and CR3
    W_CR_MASK(0x33, 0x20, 0x0);
    // Extended System Cont 1 Register (EX_SCTL_11 (CR50)
    // 11 = 4 bytes. 32 bits/pixel
    W_CR(0x50, 0x30);

    border = 0;
  }

#define ADJUST_HBORDER(x) AdjustBorder(x, border, 8)
#define ADJUST_VBORDER(y) AdjustBorder(y, border, 1);
#define TO_CLKS(x) ((x) >> 3)
#define TO_SCANLINES(y) ToScanLines((y), modeFlags)

  {
    // Horizontal Total (CRO)
    UWORD hTotalClk = TO_CLKS(hTotal) - 5;
    D("Horizontal Total %ld\n", (ULONG)hTotalClk);
    W_CR_OVERFLOW1(hTotalClk, 0x0, 0, 8, 0x5D, 0, 1);
    // FIXME: is this correct?
    {
      // Interlace Retrace Start Register (lL_RTSTART) (CR3C) ???
      W_CR(0x3c, hTotalClk >> 1);
    }
  }
  {
    // Horizontal Display End Register (H_D_END) (CR1)
    // One less than the total number of displayed characters
    // This register defines the number of character clocks for one line of the
    // active display. Bit 8 of this value is bit 1 of CR5D.
    UWORD hDisplayEnd = TO_CLKS(ScreenWidth) - 1;
    D("Display End %ld\n", (ULONG)hDisplayEnd);
    W_CR_OVERFLOW1(hDisplayEnd, 0x1, 0, 8, 0x5D, 1, 1);
  }

  UWORD hBorderSize = ADJUST_HBORDER(mi->HorBlankSize);
  UWORD hBlankStart = TO_CLKS(ScreenWidth + hBorderSize) - 1;
  {
    // AR11 register defines the overscan or border color displayed on the CRT
    // screen. The overscan color is displayed when both BLANK and DE (Display
    // Enable) signals are inactive.

    // Start Horizontal Blank Register (S_H_BLNKI (CR2))
    D("Horizontal Blank Start %ld\n", (ULONG)hBlankStart);
    W_CR_OVERFLOW1(hBlankStart, 0x2, 0, 8, 0x5d, 2, 1);
  }

  {
    // End Horizontal Blank Register (E_H_BLNKI (CR3)
    UWORD hBlankEnd = TO_CLKS(hTotal - hBorderSize) - 1;
    D("Horizontal Blank End %ld\n", (ULONG)hBlankEnd);
    //    W_CR_OVERFLOW2(hBlankEnd, 0x3, 0, 5, 0x5, 7, 1, 0x5d, 3, 1);
    W_CR_OVERFLOW1(hBlankEnd, 0x3, 0, 5, 0x5, 7, 1);
  }

  UWORD hSyncStart = TO_CLKS(mi->HorSyncStart + ScreenWidth);
  {
    // Start Horizontal Sync Position Register (S_H_SV _PI (CR4)
    D("HSync start %ld\n", (ULONG)hSyncStart);
    W_CR_OVERFLOW1(hSyncStart, 0x4, 0, 8, 0x5d, 4, 1);
  }

  UWORD endHSync = hSyncStart + TO_CLKS(mi->HorSyncSize);
  {
    // End Horizontal Sync Position Register (E_H_SY_P) (CR5)
    D("HSync End %ld\n", (ULONG)endHSync);
    W_CR_MASK(0x5, 0x1f, endHSync);
//    W_CR_OVERFLOW1(endHSync, 0x5, 0, 5, 0x5d, 5, 1);
  }

  // Start Display FIFO Register (DT _EX_POS) (CR3B)
  // FIFO filling cannot begin again
  // until the scan line position defined by the Start
  // Display FIFO register (CR3B), which is normally
  // programmed with a value 5 less than the value
  // programmed in CRO (horizontal total). This provides time during the
  // horizontal blanking period for RAM refresh and hardware cursor fetch.
  {
    UWORD startDisplayFifo = TO_CLKS(hTotal) - 5 - 5;
    if (endHSync > startDisplayFifo) {
      startDisplayFifo = endHSync + 1;
    }
    D("Start Display Fifo %ld\n", (ULONG)startDisplayFifo);
    W_CR_OVERFLOW1(startDisplayFifo, 0x3b, 0, 8, 0x5d, 6, 1);
  }

  {
    // Vertical Total (CR6)
    UWORD vTotal = TO_SCANLINES(mi->VerTotal) - 2;
    D("VTotal %ld\n", (ULONG)vTotal);
    W_CR_OVERFLOW3(vTotal, 0x6, 0, 8, 0x7, 0, 1, 0x7, 5, 1, 0x5e, 0, 1);
  }

  UWORD vBlankSize = ADJUST_VBORDER(mi->VerBlankSize);
  {
    // Vertical Display End register (CR12)
    UWORD vDisplayEnd = TO_SCANLINES(mi->Height) - 1;
    D("Vertical Display End %ld\n", (ULONG)vDisplayEnd);
    W_CR_OVERFLOW3(vDisplayEnd, 0x12, 0, 8, 0x7, 1, 1, 0x7, 6, 1, 0x5e, 1, 1);
  }

  {
    // Start Vertical Blank Register (SVB) (CR15)
    UWORD vBlankStart = mi->Height;
    if ((modeFlags & GMF_DOUBLESCAN) != 0) {
      vBlankStart = vBlankStart * 2;
    }
    // FIXME: the blankSize is unaffected by double scan, but affected by
    // interlaced?
    vBlankStart = ((vBlankStart + vBlankSize) >> interlaced) - 1;
    D("VBlank Start %ld\n", (ULONG)vBlankStart);
    W_CR_OVERFLOW3(vBlankStart, 0x15, 0, 8, 0x7, 3, 1, 0x9, 5, 1, 0x5e, 2, 1);
  }

  {
    // End Vertical Blank Register (EVB) (CR16)
    UWORD vBlankEnd = mi->VerTotal;
    if ((modeFlags & GMF_DOUBLESCAN) != 0) {
      vBlankEnd = vBlankEnd * 2;
    }
    vBlankEnd = ((vBlankEnd - vBlankSize) >> interlaced) - 1;
    D("VBlank End %ld\n", (ULONG)vBlankEnd);
    // FIXME: the blankSize is unaffected by double scan, but affected by
    // interlaced?
    W_CR(0x16, vBlankEnd);
  }

  UWORD vRetraceStart = TO_SCANLINES(mi->Height + mi->VerSyncStart);
  {
    // Vertical Retrace Start Register (VRS) (CR10)
    // FIXME: here VsyncStart is in lines, not scanlines, while mi->VerBlankSize
    // is in scanlines?
    D("VRetrace Start %ld\n", (ULONG)vRetraceStart);
    W_CR_OVERFLOW3(vRetraceStart, 0x10, 0, 8, 0x7, 2, 1, 0x7, 7, 1, 0x5e, 4, 1);
  }

  {
    // Vertical Retrace End Register (VRE) (CR11) Bits 3-0 VERTICAL RETRACE END
    // Value = least significant 4 bits of the scan line counter value at which
    // VSYNC goes in active. To obtain this value, add the desired VSYNC pulse
    // width in scan line units to the CR10 value, also in scan line units. The
    // 4 1east significant bits of this sum are programmed into this field.
    // This allows a maximum VSYNC pulse width of 15 scan line units.
    UWORD vRetraceEnd = vRetraceStart + TO_SCANLINES(mi->VerSyncSize);
    D("VRetrace End %ld\n", (ULONG)vRetraceEnd);
    W_CR_MASK(0x11, 0x0F, vRetraceEnd);
  }

  // Enable Interlace
  {
    UBYTE interlace = R_CR(0x42) & 0xdf;
    if ((modeFlags & GMF_INTERLACE) != 0) {
      interlace = interlace | 0x20;
    }
    W_CR(0x42, interlace);
  }

  // Enable Doublescan
  {
    UBYTE dblScan = R_CR(0x9) & 0x7f;
    if ((modeFlags & GMF_DOUBLESCAN) != 0) {
      dblScan = dblScan | 0x80;
    }
    W_CR(0x9, dblScan);
  }

  // Vsync/HSync polarity
  {
    UBYTE polarities = R_REG(0x3CC) & 0x3f;
    if ((modeFlags & GMF_HPOLARITY) != 0) {
      polarities = polarities | 0x40;
    }
    if ((modeFlags & GMF_VPOLARITY) != 0) {
      polarities = polarities | 0x80;
    }
    W_REG(0x3C2, polarities);
  }

//  {

//    static const ULONG mcyclesPerEntry = 9;
//    static const ULONG mcyclesPerPage = 2;
//    static const ULONG fifoEntries = 16;
//    static const ULONG fifoWidth = 64 /*Bits*/ /8; // Bytes // Trio in 1MB config has only 32bits width

//    ULONG entries = 1;
//    ULONG pageModeCycle = 10;



//    // Find M parameter for MCLK (how many clocks are given back to CPU memory
//    // access etc before handing it back to FIFO)
//    ULONG memClock = bi->MemoryClock;
//    UBYTE depth = mi->Depth;
//    if (depth <= 4) {
//      memClock /= 10;
//    } else if (depth <= 8) {
//      memClock /= 10;
//    } else if (depth <= 16) {
//      memClock /= 20;
//    } else {
//      memClock /= 41;
//    }
////    mclkM = ((memClk / (mi->PixelClock / 10000)) / 10) -
////             bi->MemoryClock / 2400000) - 9 >> 1;



//    // FIXME: check formula
//    memClock = (((memClock ) / (mi->PixelClock * 1000))) - bi->MemoryClock / 2400 000) - 9 >> 1;

//    // FIXME: the resulting value is 6 bit, but here we're throwing away the
//    // topmost bit, limiting us to just 32 cycles
//    if (memClock > 0x1f) {
//      memClock = 0x1f;
//    }
//    if (memClock < 0x3) {
//      memClock = 0x3;
//    }

//    // M PARAMETER
//    // 6-bit Value = maximum number of MCLKs that the LPB, CPU and Graphics
//    // Engine can use to access memory before giving up control of the memory
//    // bus. See Section 7.5 for more information. Bit 2 is the high order bit of
//    // this value.
//    // FIXME: on Trio64/32 this is a 5bit value, on Trio64+ 6bit
//    W_CR(0x54, memClock << 3);
//  }

    W_CR(0x54, 0x18);

  {
    // Extended Memory Control 3 Register (EXT-MCTL-3) (CR60)
    // Bits 7-0 N(DISP-FETCH-PAGE) - N Parameter
    // Value = Number of MCLKs allocated to Streams Processor FIFO filling
    // before control of the memory bus is relinquished. See Section 7.5 for
    // more information.
    W_CR(0x60, 0xff);
  }
  // Backward Compatibility 3 Register (BKWD_3) (CR34)
  // Bit 4 ENB SFF - Enable Start Display FIFO Fetch Register(CR3B)
  W_CR(0x34, 0x10);

  {
    LOCAL_SYSBASE();
    Disable();

    // Atttribute Controller Index register to AR11 while preserving "Bit 5 ENB
    // PLT - Enable Video Display"
    W_REG(ATR_AD, (R_REG(ATR_AD) & 0x20) | 0x11);
    // write AR11 = 0 Border Color Register
    W_REG(ATR_DATA_W, 0x0);

    Enable();
  }
}

static void ASM SetPanning(__REGA0(struct BoardInfo *bi),
                           __REGA1(UBYTE *memory), __REGD0(UWORD width),
                           __REGD4(UWORD height), __REGD1(WORD xoffset),
                           __REGD2(WORD yoffset), __REGD7(RGBFTYPE format))
{
  REGBASE();
  LOCAL_SYSBASE();

  D("SetPanning mem 0x%lx, width %ld, height %ld, xoffset %ld, yoffset %ld, "
    "format %ld\n",
    memory, (ULONG)width, (ULONG)height, (LONG)xoffset, (LONG)yoffset, (ULONG)format);

  LONG panOffset;
  UWORD pitch;
  ULONG memOffset;

  bi->XOffset = xoffset;
  bi->YOffset = yoffset;
  memOffset = (ULONG)memory - (ULONG)bi->MemoryBase;

  switch (format) {
  case RGBFB_NONE:
    pitch = width >> 3; // ?? can planar modes even be accessed?
    panOffset = (ULONG)yoffset * (width >> 3) + (xoffset >> 3);
    break;
  case RGBFB_R8G8B8:
  case RGBFB_B8G8R8:
  case RGBFB_A8R8G8B8:
  case RGBFB_A8B8G8R8:
  case RGBFB_R8G8B8A8:
  case RGBFB_B8G8R8A8:
    pitch = width * 4 ;
    panOffset = (yoffset * width + xoffset) * 4;
    break;
  case RGBFB_R5G6B5PC:
  case RGBFB_R5G5B5PC:
  case RGBFB_R5G6B5:
  case RGBFB_R5G5B5:
  case RGBFB_B5G6R5PC:
  case RGBFB_B5G5R5PC:
  case RGBFB_YUV422CGX:
  case RGBFB_YUV422:
  case RGBFB_YUV422PC:
  case RGBFB_YUV422PA:
  case RGBFB_YUV422PAPC:
    pitch = width * 2;
    panOffset = (yoffset * width + xoffset) * 2;
    break;
  default:
    // RGBFB_CLUT:
    pitch = width;
    panOffset = yoffset * width + xoffset;
    break;
  }

  pitch /= 8;
  panOffset = (panOffset + memOffset) / 4;

  D("panOffset 0x%lx, pitch %ld dwords\n", panOffset, pitch);
  // Start Address Low Register (STA(L)) (CRD)
  // Start Address High Register (STA(H)) (CRC)
  // Extended System Control 3 Register (EXT-SCTL-3)(CR69)
  W_CR_OVERFLOW2_ULONG(panOffset, 0xd, 0, 8, 0xc, 0, 8, 0x69, 0, 4);

  //  assert(pitchInDoublwWords < 0xFFFF);

  // Offset Register (SCREEN-OFFSET) (CR13)
  //  Bits 7-0 LOGICAL SCREEN WIDTH
  //      10-bit Value = quantity that is multiplied by 2 (word mode), 4
  //      (doubleword mode) or 8 (quadword mode) to specify the difference
  //      between the starting byte addresses of two consecutive scan lines.
  //      This register contains the least significant 8 bits of this value.
  //      The addressing mode is specified by bit 6 of CR14 and bit 3 of CR17.
  //      Setting bit 3 of CR31 to 1 forces doubleword mode.

  // This register specifies the amount to be added to the internal linear
  // counter when advancing from one screen row to the next. The addition is
  // performed whenever the internal row address counter advances past the
  // maximum row address value, indicating that all the scan lines in the
  // present row have been displayed. The Row Offset register is programmed in
  // terms of CPU-addressed words per scan line, counted as either words or
  // doublewords, depending on whether byte or word mode is in effect. If the
  // CRTC Mode register is set to select byte mode, the Row Offset register is
  // programmed with a word value. So for a 640-pixel (80-byte) wide graphics
  // display, a value of 80/2 = 40 (28 hex) would normally be programmed, where
  // 80 ts the number of bytes per scan line. If the CRTC Mode register is set
  // to select word mode, then the Row Offset register is programmed with a
  // doubleword, rather than a word, value. For instance, in 80-column text
  // mode, a value of 160/4=40 (28 hex) would be programmed, because from the
  // CPU-addressing side, each character requires 2 linear bytes (character code
  // byte and attribute byte), for a total of 160 (AO hex) bytes per row.

  W_CR_OVERFLOW1(pitch, 0x13, 0, 8, 0x51, 4, 2);

  Disable();

  R_REG(0x3DA);  // Reset AFF flip-flop // FIXME: why?

  Enable();
  return;
}

static APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi),
                                __REGA1(APTR mem), __REGD7(RGBFTYPE format))
{
  if (bi->ChipRevision & 0x40)  // Trio64+?
  {
    switch (format) {
    case RGBFB_A8R8G8B8:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
      // Redirect to Big Endian Linear Address Window. On the Prometheus, due to its hardware byteswapping,
      // this effectively makes the BE CPU writes appear as LE in memory
      return mem + 0x2000000;
      break;
    default:
      return mem;
      break;
    }
  }
  return mem;
}

static ULONG ASM GetCompatibleFormats(__REGA0(struct BoardInfo *bi),
                                      __REGD7(RGBFTYPE format))
{
  if (format == RGBFB_NONE)
    return (ULONG)0;

  return RGBFF_CLUT | RGBFF_HICOLOR | RGBFF_TRUECOLOR | RGBFF_TRUEALPHA;
}

static void ASM SetDisplay(__REGA0(struct BoardInfo *bi), __REGD0(BOOL state))
{
  // Clocking Mode Register (ClK_MODE) (SR1)
  REGBASE();
  W_SR_MASK(0x01, 0x20, (~(UBYTE)state & 1) << 5);
}

static ULONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi),
                                   __REGA1(struct ModeInfo *mi),
                                   __REGD0(ULONG pixelClock),
                                   __REGD7(RGBFTYPE RGBFormat))
{
  BOOL noOverClock;
  ULONG biFlags;

  biFlags = bi->Flags;
//  noOverClock = (biFlags & BIF_OVERCLOCK) == 0;

  mi->Flags = mi->Flags & ~GMF_ALWAYSBORDER;
  if (0x3ff < mi->VerTotal) {
    mi->Flags = mi->Flags | GMF_ALWAYSBORDER;
  }

  // Enable Double Clock for 8Bit modes when pixelclock exceeds 85Mhz
  mi->Flags &= ~GMF_DOUBLECLOCK;
  if (RGBFormat == RGBFB_CLUT || RGBFormat == RGBFB_NONE) {
    if (pixelClock > 85013001) {
      mi->Flags |= GMF_DOUBLECLOCK;
      pixelClock /= 2;
    }
  }
  if (mi->Height < 400) {
    mi->Flags |= GMF_DOUBLESCAN;
  }

  UWORD m, n, r;

#ifdef DBG1
  KPrintF("original Pixel Hz: %ld\n", pixelClock);
#endif

  int currentKhz = svga_compute_pll(&s3_pll, pixelClock / 1000, &m, &n, &r);
  if (currentKhz < 0) {
    KPrintF("cannot resolve requested pixclock\n");
    return 0;
  }
  if (mi->Flags & GMF_DOUBLECLOCK) {
    currentKhz *= 2;
  }
  mi->PixelClock = currentKhz * 1000;

#ifdef DBG1
  KPrintF("Pixelclock Hz: %ld\n\n", mi->PixelClock);
#endif

  // FIXME: would have to encode r here, too
  mi->pll1.Numerator = n;
  mi->pll2.Denominator = m;

  return currentKhz / 1000;  // use Mhz as "index"
}

static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi),
                               __REGA1(struct ModeInfo *mi),
                               __REGD0(ULONG index), __REGD7(RGBFTYPE format))
{
  UWORD m, n, r;
  ULONG pixelClockKhz = index * 1000;
  if (mi->Flags & GMF_DOUBLECLOCK) {
      pixelClockKhz /= 2;
  }
  int currentKhz = svga_compute_pll(&s3_pll, pixelClockKhz, &m, &n, &r);
  if (currentKhz < 0) {
    KPrintF("cannot resolve requested pixclock\n");
    return 0;
  }
  if (mi->Flags & GMF_DOUBLECLOCK) {
    currentKhz *= 2;
  }
  return currentKhz * 1000;
}

static void ASM SetClock(__REGA0(struct BoardInfo *bi))
{
  REGBASE();

  ULONG pixelClock = bi->ModeInfo->PixelClock;

  if (bi->ModeInfo->Flags & GMF_DOUBLECLOCK) {
    pixelClock /= 2;
  }

#ifdef DBG
  KPrintF("SetClock: %ld -> %ld\n", bi->ModeInfo->PixelClock, pixelClock);
#endif

  UWORD m, n, r;
  int currentKhz = svga_compute_pll(&s3_pll, pixelClock / 1000, &m, &n, &r);
  if (currentKhz < 0) {
    KPrintF("cannot resolve requested pixclock\n");
  }

  /* Set S3 DCLK clock registers */
  // Clock-Doubling will be enabled by SetGC
  W_SR(0x12, (n - 2) | (r << 5));
  W_SR(0x13, m - 2);

  {
    LOCAL_DOSBASE();
    Delay(1);
  }

  /* Activate clock - write 0, 1, 0 to seq/15 bit 5 */

  UBYTE regval = R_SR(0x15); /* | 0x80; */
  W_SR(0x15, regval & ~(1 << 5));
  W_SR(0x15, regval | (1 << 5));
  W_SR(0x15, regval & ~(1 << 5));
}

static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi),
                              __REGD7(RGBFTYPE format))
{
  REGBASE();
  if (bi->ChipCurrentMemFmt == format)
  {
    return;
  }
  bi->ChipCurrentMemFmt = format;

  if (bi->ChipRevision & 0x40)  // Trio64+?
  {
    switch (format) {
    case RGBFB_A8R8G8B8:
        // swap all the bytes within a double word
        W_CR_MASK(0x53, 0x06, 0x04);
      break;
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
      // Just swap the bytes within a word
      W_CR_MASK(0x53, 0x06, 0x02);
      break;
    default:
      W_CR_MASK(0x53, 0x06, 0x00);
      break;
    }
  }
  return;
}

static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
  // This function shall preserve all registers!
  // FIMXE: would have to implement asm prolog/epilog to save scratch registers
}

static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
}

static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
}

BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi), __REGD0(BOOL expected))
{
  REGBASE();
  return (R_REG(0x3DA) & 0x08) != 0;
}

static void WaitVerticalSync(__REGA0(struct BoardInfo *bi)) {}

static void ASM SetDPMSLevel(__REGA0(struct BoardInfo *bi), __REGD0(ULONG level))
{
  //  DPMS_ON,      /* Full operation                             */
  //  DPMS_STANDBY, /* Optional state of minimal power reduction  */
  //  DPMS_SUSPEND, /* Significant reduction of power consumption */
  //  DPMS_OFF      /* Lowest level of power consumption */

  static const UBYTE DPMSLevels[4] = {0x00, 0x90, 0x60, 0x50};
  REGBASE();
  W_REG_MASK(0xd, 0xF0, DPMSLevels[level]);

}


void ASM SetSplitPosition(__REGA0(struct BoardInfo *bi),__REGD0(SHORT splitPos))
{
  REGBASE();
  D("SplitPos: %ld\n", (ULONG)splitPos);
  bi->YSplit = splitPos;
  if (!splitPos)
  {
    splitPos = 0x7ff;
  }
  else
  {
    if (bi->ModeInfo->Flags & GMF_DOUBLESCAN)
    {
      splitPos *= 2;
    }
  }
  W_CR_OVERFLOW3((UWORD)splitPos, 0x18, 0, 8, 0x7, 4, 1, 0x9, 6, 1, 0x5e, 6, 1);
}

BOOL InitChipL(__REGA0(struct BoardInfo *bi))
{
  REGBASE();
  MMIOBASE();
  LOCAL_PROMETHEUSBASE();
  LOCAL_SYSBASE();

  bi->ChipDOSBase = (ULONG) OpenLibrary(DOSNAME, 0);
  if (!bi->ChipDOSBase) {
    return FALSE;
  }

  bi->GraphicsControllerType = GCT_S3Trio64;
  bi->PaletteChipType = PCT_S3Trio64;
  bi->Flags = bi->Flags | BIF_NOMEMORYMODEMIX | BIF_BORDERBLANK |
              BIF_NOP2CBLITS | BIF_NOBLITTER | BIF_GRANTDIRECTACCESS | BIF_VGASCREENSPLIT;
  // Trio64 supports BGR_8_8_8_X 24bit, R5G5B5 and R5G6B5 modes.
  // Prometheus does byte-swapping for writes to memory, so if we're writing a 32bit register
  // filled with XRGB, the written memory order will be BGRX
  bi->RGBFormats = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;


  bi->SetGC = SetGC;
  bi->SetPanning = SetPanning;
  bi->CalculateBytesPerRow = CalculateBytesPerRow;
  bi->CalculateMemory = CalculateMemory;
  bi->GetCompatibleFormats = GetCompatibleFormats;
  bi->SetDAC = SetDAC;
  bi->SetColorArray = SetColorArray;
  bi->SetDisplay = SetDisplay;
  bi->SetMemoryMode = SetMemoryMode;
  bi->SetWriteMask = SetWriteMask;
  bi->SetReadPlane = SetReadPlane;
  bi->SetClearMask = SetClearMask;
  bi->ResolvePixelClock = ResolvePixelClock;
  bi->GetPixelClock = GetPixelClock;
  bi->SetClock = SetClock;

  // VSYNC
  bi->WaitVerticalSync = WaitVerticalSync;
  bi->GetVSyncState = GetVSyncState;

  // DPMS
  bi->SetDPMSLevel = SetDPMSLevel;

  // VGA Splitscreen
  bi->SetSplitPosition = SetSplitPosition;

  // Blitter acceleration
  //  bi->WaitBlitter = (_func_38.conflict *)&WaitBlitter;
  //  bi->BlitRect = BlitRect;
  //  bi->InvertRect = InvertRect;
  //  bi->FillRect = FillRect;
  //  bi->BlitTemplate = BlitTemplate;
  //  bi->BlitPlanar2Chunky = BlitPlanar2Chunky;
  //  bi->BlitRectNoMaskComplete = BlitRectNoMaskComplete;
  //  bi->DrawLine = (_func_55.conflict *)&DrawLine;
  //  bi->Reserved1Default = (_func_64.conflict *)&Reserved1Default;

  //  PLANAR,
  //      CHUNKY,
  //      HICOLOR,
  //      TRUECOLOR,
  //      TRUEALPHA,

  bi->PixelClockCount[PLANAR] = 0;
  bi->PixelClockCount[CHUNKY] =
      135;  // > 80Mhz can be achieved via Double Clock mode
  bi->PixelClockCount[HICOLOR] = 80;
  bi->PixelClockCount[TRUECOLOR] = 50;
  bi->PixelClockCount[TRUEALPHA] = 50;

  //  if ((bi->Flags & BIB_OVERCLOCK) != 0) {
  //    bi->PixelClockCount[2] = bi->PixelClockCount[2] + 100;
  //    bi->PixelClockCount[3] = bi->PixelClockCount[3] + 100;
  //    bi->PixelClockCount[4] = bi->PixelClockCount[4] + 100;
  //  }

  bi->BitsPerCannon = 6;
  bi->MaxHorValue[PLANAR] = 4088;
  bi->MaxHorValue[CHUNKY] = 4088;
  bi->MaxHorValue[HICOLOR] = 8176;
  bi->MaxHorValue[TRUECOLOR] = 16352;
  bi->MaxHorValue[TRUEALPHA] = 16352;

  bi->MaxVerValue[PLANAR] = 2047;
  bi->MaxVerValue[CHUNKY] = 2047;
  bi->MaxVerValue[HICOLOR] = 2047;
  bi->MaxVerValue[TRUECOLOR] = 2047;
  bi->MaxVerValue[TRUEALPHA] = 2047;

  bi->MaxHorResolution[PLANAR] = 1600;
  bi->MaxVerResolution[PLANAR] = 1600;

  bi->MaxHorResolution[CHUNKY] = 1600;
  bi->MaxVerResolution[CHUNKY] = 1600;

  bi->MaxHorResolution[HICOLOR] = 1280;
  bi->MaxVerResolution[HICOLOR] = 1280;

  bi->MaxHorResolution[TRUECOLOR] = 1280;
  bi->MaxVerResolution[TRUECOLOR] = 1280;

  bi->MaxHorResolution[TRUEALPHA] = 1280;
  bi->MaxVerResolution[TRUEALPHA] = 1280;

  if (bi->ChipRevision & 0x40) {
    /* Chip wakeup Trio64+*/
    W_REG(0x3C3, 0x01);
  } else {
    /* Chip wakeup Trio64/32*/
    W_REG(0x3C3, 0x10);
    W_REG(0x102, 0x01);
    W_REG(0x3C3, 0x08);
  }

  // Color-Emulation, 0x3D4/5 for CR_IDX/DATA
  W_REG(0x3C2, 0x63);

  // Unlock S3 registers
  W_CR(0x38, 0x48);
  W_CR(0x39, 0xa5);

  UBYTE chipRevision = R_CR(0x2F);
  BOOL isTrio64Plus = (chipRevision & 0x40) != 0;
  if (isTrio64Plus) {
    BOOL LPBMode = (R_CR(0x6F) & 0x01) == 0;
    CONST_STRPTR modeString =
        (LPBMode ? "Local Peripheral Bus (LPB)" : "Compatibility");
    KPrintF("Chip is Trio64+ (Rev %ld) in %s mode\n",
            (ULONG)chipRevision & 0x0f, modeString);

    // We can support byte-swapped formats on this chip via the Big Linear Adressing Window
    bi->RGBFormats |= RGBFF_A8R8G8B8 | RGBFF_R5G6B5 | RGBFF_R5G5B5;
  } else {
    KPrintF("Chip is Trio64/32 (Rev %ld)\n", (ULONG)chipRevision);
  }
  // Input Status ? Register (STATUS_O)
  KPrintF("Monitor is %s present\n", (R_REG(0x3C2) & 0x10 ? "NOT" : ""));

  /* The Enhanced Graphics Command register group is unlocked
     by setting bit 0 of the System Configuration register (CR40) to 1.
     After that, bitO of4AE8H must be setto 1 to enable Enhanced mode functions.
      */
  W_CR_MASK(0x40, 0x1, 0x1);

  /* Now that we enabled enhanced mode register access;
   * Enable enhanced mode functions,  write lower byte of 0x4AE8
   * WARNING: DO NOT ENABLE MMIO WITH BIT 5 HERE.
   * This bit will be OR'd into CR53 and thus makes into impossible to setup "new MMIO only" mode on Trio64+.
   * This is despite the docs claiming Bit 5 is "reserved" on there.
   */
  W_REG(0x4AE8, 0x05);

    /* This field contains the upper 6 bits (19-14) of the CPU base address,
     allowing accessing of up to 4 MBytes of display memory via 64K pages.
     When a non-zero value is programmed in this field, bits 3-0 of CR35
     and 3-2 of CR51 (the old CPU base address bits) are ignored.
     Bit 0 of CR31 must be set to 1 to enable this field.  */
  W_CR(0x6a, 0x0);

  W_SR(0x8, 0x6);
  W_SR(0x1, 0x1);
  W_SR(0x0, 0x3);
  W_SR(0x2, 0xff);
  W_SR(0x3, 0x0);
  W_SR(0x4, 0x2);

  // FIXME: this has memory setting implications potentially only valid for the
  // Cybervision
  W_SR(0xa, 0xc0);
  W_SR(0x18, 0xc0);

  // Init RAMDAC
  W_REG(0x3C6, 0xff);

  ULONG clock = bi->MemoryClock;
  if (clock < 54000000) {
    clock = 54000000;
  }
  if (65000000 < clock) {
    clock = 65000000;
  }

  clock = SetMemoryClock(bi, clock);
  bi->MemoryClock = clock;

  W_CR(0x0, 0x5f);
  W_CR(0x1, 0x4f);
  W_CR(0x2, 0x50);
  W_CR(0x3, 0x82);
  W_CR(0x4, 0x54);
  W_CR(0x5, 0x80);
  W_CR(0x6, 0xbf);
  W_CR(0x7, 0x1f);
  W_CR(0x8, 0x0);
  W_CR(0x9, 0x40);
  W_CR(0xa, 0x0);
  W_CR(0xb, 0x0);
  W_CR(0xc, 0x0);
  W_CR(0xd, 0x0);
  W_CR(0xe, 0x0);
  W_CR(0xf, 0x0);
  W_CR(0x10, 0x9c);

  W_CR(0x11, 0xe); // 5 DRAM refresh cycles, unlock CR0/CR7, Diable Vertical Interrupt

  W_CR(0x12, 0x8f);

  // Offset Register (SCREEN-OFFSET) (CR13)
  // Specifies the logical screen width (pitch). Bits 5-4 of CR51 are extension
  // bits 9-8 for this value. If these bits are OOb, bit 2 of
  // CR43 is extension bit 8 of this register.
  // 10-bit Value = quantity that is multiplied by 2 (word mode), 4 (doubleword
  // mode) or 8 (quadword mode) to specify the difference between the starting
  // byte addresses of two consecutive scan lines. This register contains the
  // least significant 8 bits of this

  W_CR(0x13, 0x50);  // == 160, menaing 640byte in double word mode

  W_CR(0x14, 0x0);  // Underline Location Register (ULL) (CR14) (affects address
                    // counting)
  W_CR(0x15, 0x96);  // Start Vertical Blank Register (SVB) (CR15)
  W_CR(0x16, 0xb9);  // End Vertical Blank Register (EVB) (CR16)
  W_CR(0x17, 0xe3);  // Byte Adressing mode, V/HSync pulses enabled
  W_CR(0x18, 0xff);  // Line compare register

  W_CR(0x31, 0x04);   // Enhanced memory mapping, 16bit VGA bus(?)
//  W_CR(0x31, 0x0c);   // Enhanced memory mapping, Doubleword mode

  W_CR(0x32, 0x10);

  // BackWard Compatibility 2 Register (BKWD_2) (CR33)
  // Bit 1 DIS VDE - Disable Vertical Display End Extension Bits Write
  // Protection
  //  0 = VDE protection enabled
  //  1 = Disables the write protect setting of the bit 7 of CR11 on bits 1
  //      and 6 of CR7
  W_CR(0x33, 0x02);
  /* Miscellaneous 1 (CR3A)
   * Bits 1-0 REF-CNT - Alternate Refresh Count Control
        01 = Refresh Count 1
     Bit 2 ENS RFC - Enable Alternate Refresh Count Control
       1 = Alternate refresh count control (bits 1-0) is enabled
     Bit 4 ENH 256 - Enable 8 Bits/Pixel or Greater Color Enhanced Mode
     Bit 5 HST DFW - Enable High Speed Text Font Writing */
  W_CR(0x3a, 0x35);
  // Start Display FIFO Fetch (CR3B)
  W_CR(0x3b, 0x5a);
  // Extended Mode Register (EXT_MODE) (CR43)
  W_CR(0x43, 0x00);

  // Enable 4MB Linear Address Window (LAW)
  W_CR(0x58, 0x13);
  if (isTrio64Plus)
  {
    // Enable Trio64+ "New MMIO" only and byte swapping in the Big Endian window
    W_CR_MASK(0x53, 0x3E, 0x0c);

  }
  else
  {
    D("setup compatible MMIO\n");
    // Enable Trio64 old style MMIO. This hardcodes the MMIO range to 0xA8000 physical address.
    // Need to make sure, nothing else sits there
    W_CR_MASK(0x53, 0x38, 0x30);

    // LAW start address
    ULONG physAddress = Prm_GetPhysicalAddress(bi->MemoryBase);
    if (physAddress & 0x3FFFFF) {
      KPrintF("WARNING: card's base address is not 4MB aligned!\n");
    }
    W_CR_MASK(0x5a, physAddress >> 16, 0x7F);
    // Upper address bits may  not be touched as they would result in shifting the PCI window
//    W_CR_MASK(0x59, physAddress >> 24);
  }

  // MCLK M Parameter
  W_CR(0x54, 0x70);

  W_CR(0x60, 0xff);

  W_CR(0x5d, 0x0);
  W_CR(0x5e, 0x40);
  W_GR(0x0, 0x0);
  W_GR(0x1, 0x0);
  W_GR(0x2, 0x0);
  W_GR(0x3, 0x0);
  W_GR(0x4, 0x0);
  W_GR(0x5, 0x40);
  W_GR(0x6, 0x1);
  W_GR(0x7, 0xf);
  W_GR(0x8, 0xff);

  // Reset AFF to index register selection
  R_REG(0x3DA);

  for (int p = 0; p < 16; ++p) {
    /* The attribute controller registers are located atthe same byte I/O
       address for writing address and data. An internal address flip-flop (AFF)
       controls the selection of either the attribute index or data registers.
       To initialize the address flip-flop (AFF), an I/O read is issued at
       address 3BAH or 3DAH. This presets the address flip-flop to select the
       index register. After the index register has been loaded by an I/O write
       to address 3COH, AFF toggles and the next 1/0 write loads the data
       register. Every I/O write to address 3COH toggles this address flip-flop.
       However, it does not toggle for I/O reads at address 3COH or 3C1 H. The
       Attribute Controller Index register is read at 3COH, and the Attribute
       Controller Data register is read
       at address 3C1 H.  */
    W_AR(p, p);
  }
  W_AR(0x30, 0x61);
  W_AR(0x31, 0x0);
  W_AR(0x32, 0xf);
  W_AR(0x33, 0x0);
  W_AR(0x34, 0x0);
  /* Read Register Data Register RD_REG_DT
     Write a value with register-index in top bits and register value in bottom
     bits */
  // Write conservative scissors
  W_REG_W(0xBEE8, 0x1000);
  W_REG_W(0xBEE8, 0x2000);
  W_REG_W(0xBEE8, 0x3fff);
  W_REG_W(0xBEE8, 0x4fff);

  /* Enable PLL load */
  W_MISC_MASK(0x0c, 0x0c);

  return TRUE;
}
