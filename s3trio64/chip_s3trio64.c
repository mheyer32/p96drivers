#include "chip_s3trio64.h"

#define __NOLIBBASE__

#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/prometheus.h>

#include <SDI_stdarg.h>

int debugLevel = 0;


#define SUBSYS_STAT 0x42E8  // Read
#define SUBSYS_CNTL 0x42E8  // Write
#define ADVFUNC_CNTL 0x4AE8

#define CUR_Y 0x82E8
#define CUR_Y2 0x82EA
#define CUR_X 0x86E8
#define CUR_X2 0x86EA
#define DESTY_AXSTP 0x8AE8
#define Y2_AXSTP2 0x8AEA
#define DESTX_DIASTP 0x8EE8
#define X2 0x8EEA
#define ERR_TERM 0x92E8
#define ERR_TERM2 0x92EA
#define MAJ_AXIS_PCNT 0x96E8
#define MAJ_AXIS_PCNT2 0x96EA
#define GP_STAT 0x9AE8  // Read-only

#define CMD 0x9AE8  // Write-only

#define CMD_ALWAYS 0x0001
#define CMD_ACROSS_PLANE 0x0002
#define CMD_NO_LASTPIXEL 0x0004
#define CMD_RADIAL_DRAW_DIR 0x0008
#define CMD_DRAW_PIXELS 0x0010
#define CMD_DRAW_DIR_MASK 0x00e0
#define CMD_BYTE_SWAP 0x1000

#define CMD_COMMAND_TYPE_MASK 0xe000
#define CMD_COMMAND_TYPE_SHIFT 13

#define CMD_TYPE_NOP (0b000 << CMD_COMMAND_TYPE_SHIFT)
#define CMD_TYPE_LINE (0b001 << CMD_COMMAND_TYPE_SHIFT)
#define CMD_TYPE_RECT_FILL (0b010 << CMD_COMMAND_TYPE_SHIFT)
#define CMD_TYPE_BLIT (0b110 << CMD_COMMAND_TYPE_SHIFT)
#define CMD_TYPE_PAT_BLIT (0b111 << CMD_COMMAND_TYPE_SHIFT)

#define CMD2 0x9AEA  // Write-only
#define CMD2_TRAPEZOID_DIR_MASK 0x00e0
#define CMD2_TRAPEZOID_DIR_SHIFT 5

#define SHORT_STROKE 0x9EE8
// These 5 are 32bit registers, which can be accessed either by two
// 16bit writes or 32bit writes when using IO programming (does not apply to
// MMIO)
#define BKGD_COLOR 0xA2E8
#define FRGD_COLOR 0xA6E8
#define WRT_MASK 0xAAE8
#define RD_MASK 0xAEE8
#define COLOR_CMP 0xB2E8

#define BKGD_MIX 0xB6E8
#define FRGD_MIX 0xBAE8

#define CLR_SRC_BKGD_COLOR (0b00 << 5)
#define CLR_SRC_FRGD_COLOR (0b01 << 5)
#define CLR_SRC_CPU (0b10 << 5)
#define CLR_SRC_MEMORY (0b11 << 5)

#define MIX_NOT_CURRENT 0b0000
#define MIX_ZERO 0b0001
#define MIX_ONE 0b0010
#define MIX_CURRENT 0b0011
#define MIX_NOT_NEW 0b0100
#define MIX_CURRENT_XOR_NEW 0b0101
#define MIX_NOT_CURRENT_XOR_NEW 0b0110
#define MIX_NEW 0b0111
#define MIX_NOT_CURRENT_OR_NOT_NEW 0b1000
#define MIX_CURRENT_OR_NOT_NEW 0b1001
#define MIX_NOT_CURRENT_OR_NEW 0b1010
#define MIX_CURRENT_OR_NEW 0b1011
#define MIX_CURRENT_AND_NEW 0b1100
#define MIX_NOT_CURRENT_AND_NEW 0b1101
#define MIX_CURRENT_AND_NOT_NEW 0b1110
#define MIX_NOT_CURRENT_AND_NOT_NEW 0b1111

#define RD_REG_DT 0xBEE8
// The following are accessible via RD_REG_DT, the number indicates the index
#define MIN_AXIS_PCNT 0x0
#define SCISSORS_T 0x1
#define SCISSORS_L 0x2
#define SCISSORS_B 0x3
#define SCISSORS_R 0x4
#define PIX_CNTL 0xA
#define MULT_MISC2 0xD
#define MULT_MISC 0xE
#define READ_SEL 0xF

#define PIX_TRANS 0xE2E8
#define PIX_TRANS_EXT 0xE2EA
#define PAT_Y 0xEAE8
#define PAT_X 0xEAEA

// Packed MMIO 32bit registers
#define ALT_CURXY 0x8100
#define ALT_CURXY2 0x8104
#define ALT_STEP 0x8108
#define ALT_STEP2 0x810C
#define ALT_ERR 0x8110
#define ALT_CMD 0x8118
#define ALT_MIX 0x8134
#define ALT_PCNT 0x8148
#define ALT_PAT 0x8168

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

  DFUNC(8, "ideal frequency: %ld kHz\n", (unsigned int)f_wanted);

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

  D(15, "found frequency: %ld kHz (VCO %ld kHz)\n", (int)(f_current >> ar),
          (int)f_current);
  D(15, "m = %ld n = %ld r = %ld\n", (unsigned int)*m, (unsigned int)*n,
          (unsigned int)*r);

  return (f_current >> ar);
}

static const struct svga_pll s3_pll = {3, 129,   3,      33,   0,
                                       3, 35000, 240000, 14318};

ULONG SetMemoryClock(struct BoardInfo *bi, ULONG clockHz)
{
  REGBASE();

  u16 m, n, r;
  u8 regval;

  DFUNC(10, "original Hz: %ld\n", clockHz);

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

static inline UBYTE getBPP(RGBFTYPE format)
{
  // FIXME: replace with fixed table?
  switch (format) {
  case RGBFB_CLUT:
    return 1;
    break;
  case RGBFB_R5G6B5PC:
  case RGBFB_R5G5B5PC:
  case RGBFB_R5G6B5:
  case RGBFB_R5G5B5:
    return 2;
    break;
  case RGBFB_A8R8G8B8:
  case RGBFB_B8G8R8A8:
    return 4;
    break;
  default:
    // fallthrough
    break;
  }
  return 0;
}

static UWORD CalculateBytesPerRow(__REGA0(struct BoardInfo *bi),
                                  __REGD0(UWORD width), __REGD1(UWORD height),
                                  __REGA1(struct ModeInfo *mi),
                                  __REGD7(RGBFTYPE format))
{
  // Make the bytes per row compatible with the Graphics Engine's presets
  if (width <= 640) {
    width = 640;
  } else if (width <= 800) {
    width = 800;
  } else if (width <= 1024) {
    width = 1024;
  } else if (width <= 1152) {
    width = 1152;
  } else if (width <= 1280) {
    width = 1280;
  } else if (width <= 1600) {
    width = 1600;
  } else if (width <= 2048) {
    width = 2048;
  } else {
    return 0;
  }

  UBYTE bpp = getBPP(format);

  UWORD bytesPerRow = width * bpp;
  // I believe the Graphics Engine can hadle only 1MB at a time
  ULONG maxHeight = 1 * 1024 * 1024 / bytesPerRow;

  if (height > maxHeight) {
    return 0;
  }
  return bytesPerRow;
}

static void ASM SetColorArray(__REGA0(struct BoardInfo *bi),
                              __REGD0(UWORD startIndex), __REGD1(UWORD count))
{
  REGBASE();
  LOCAL_SYSBASE();

  DFUNC(5, "startIndex %ld, count %ld\n", (ULONG)startIndex, (ULONG)count);

  // FIXME: this should be a constant for the Trio, no need to make it dynamic
  const UBYTE bppDiff = 2;  // 8 - bi->BitsPerCannon;

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

  DFUNC(5, "\n");

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
      dacMode = 0x10;
    } else {
      dacMode = DAC_ColorModes[format];
    }
    W_CR_MASK(0x67, 0xF0, dacMode);
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

  DFUNC(5,
      "W %ld, H %ld, HTotal %ld, HBlankSize %ld, HSyncStart %ld, HSyncSize "
      "%ld, "
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
    if ((border == 0) || ((mi->HorBlankSize == 0 || (mi->VerBlankSize == 0)))) {
      D(6, "8-Bit Mode, NO Border\n");
      // Bit 5 BDR SEL - Blank/Border Select
      // 1 = BLANK is active during entire display inactive period (no border)
      W_CR_MASK(0x33, 0x20, 0x20);
    } else {
      D(6, "8-Bit Mode, Border\n");
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
    D(6, "16-Bit Mode, No Border\n");
    // Bit 5 BDR SEL - Blank/Border Select
    // o = BLANK active time is defined by CR2 and CR3
    W_CR_MASK(0x33, 0x20, 0x0);

    // FIXME: what is this?
    hTotal = hTotal * 2;
    ScreenWidth = ScreenWidth * 2 + 6;
    border = 0;
  } else {
    D(6, "24-Bit Mode, No Border\n");
    // Bit 5 BDR SEL - Blank/Border Select
    // 0 = BLANK active time is defined by CR2 and CR3
    W_CR_MASK(0x33, 0x20, 0x0);
    border = 0;
  }

#define ADJUST_HBORDER(x) AdjustBorder(x, border, 8)
#define ADJUST_VBORDER(y) AdjustBorder(y, border, 1);
#define TO_CLKS(x) ((x) >> 3)
#define TO_SCANLINES(y) ToScanLines((y), modeFlags)

  {
    // Horizontal Total (CRO)
    UWORD hTotalClk = TO_CLKS(hTotal) - 5;
    D(6, "Horizontal Total %ld\n", (ULONG)hTotalClk);
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
    D(6, "Display End %ld\n", (ULONG)hDisplayEnd);
    W_CR_OVERFLOW1(hDisplayEnd, 0x1, 0, 8, 0x5D, 1, 1);
  }

  UWORD hBorderSize = ADJUST_HBORDER(mi->HorBlankSize);
  UWORD hBlankStart = TO_CLKS(ScreenWidth + hBorderSize) - 1;
  {
    // AR11 register defines the overscan or border color displayed on the CRT
    // screen. The overscan color is displayed when both BLANK and DE (Display
    // Enable) signals are inactive.

    // Start Horizontal Blank Register (S_H_BLNKI (CR2))
    D(6, "Horizontal Blank Start %ld\n", (ULONG)hBlankStart);
    W_CR_OVERFLOW1(hBlankStart, 0x2, 0, 8, 0x5d, 2, 1);
  }

  {
    // End Horizontal Blank Register (E_H_BLNKI (CR3)
    UWORD hBlankEnd = TO_CLKS(hTotal - hBorderSize) - 1;
    D(6, "Horizontal Blank End %ld\n", (ULONG)hBlankEnd);
    //    W_CR_OVERFLOW2(hBlankEnd, 0x3, 0, 5, 0x5, 7, 1, 0x5d, 3, 1);
    W_CR_OVERFLOW1(hBlankEnd, 0x3, 0, 5, 0x5, 7, 1);
  }

  UWORD hSyncStart = TO_CLKS(mi->HorSyncStart + ScreenWidth);
  {
    // Start Horizontal Sync Position Register (S_H_SV _PI (CR4)
    D(6, "HSync start %ld\n", (ULONG)hSyncStart);
    W_CR_OVERFLOW1(hSyncStart, 0x4, 0, 8, 0x5d, 4, 1);
  }

  UWORD endHSync = hSyncStart + TO_CLKS(mi->HorSyncSize);
  {
    // End Horizontal Sync Position Register (E_H_SY_P) (CR5)
    D(6, "HSync End %ld\n", (ULONG)endHSync);
    W_CR_MASK(0x5, 0x1f, endHSync);
    //    W_CR_OVERFLOW1(endHSync, 0x5, 0, 5, 0x5d, 5, 1);
  }

  // Start Display FIFO Register (DT _EX_POS) (CR3B)
  // FIFO filling cannot begin again
  // until the scan line position defined by the Start
  // Display FIFO register (CR3B), which is normally
  // programmed with a value 5 less than the value
  // programmed in CRO (horizontal total). This provides time during the
  // horizontal blanking period for RAM refresh and hardware cursor fetch.
  {
    UWORD startDisplayFifo = TO_CLKS(hTotal) - 5 - 5;
    if (endHSync > startDisplayFifo) {
      startDisplayFifo = endHSync + 1;
    }
    D(6, "Start Display Fifo %ld\n", (ULONG)startDisplayFifo);
    W_CR_OVERFLOW1(startDisplayFifo, 0x3b, 0, 8, 0x5d, 6, 1);
  }

  {
    // Vertical Total (CR6)
    UWORD vTotal = TO_SCANLINES(mi->VerTotal) - 2;
    D(6, "VTotal %ld\n", (ULONG)vTotal);
    W_CR_OVERFLOW3(vTotal, 0x6, 0, 8, 0x7, 0, 1, 0x7, 5, 1, 0x5e, 0, 1);
  }

  UWORD vBlankSize = ADJUST_VBORDER(mi->VerBlankSize);
  {
    // Vertical Display End register (CR12)
    UWORD vDisplayEnd = TO_SCANLINES(mi->Height) - 1;
    D(6, "Vertical Display End %ld\n", (ULONG)vDisplayEnd);
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
    D(6, "VBlank Start %ld\n", (ULONG)vBlankStart);
    W_CR_OVERFLOW3(vBlankStart, 0x15, 0, 8, 0x7, 3, 1, 0x9, 5, 1, 0x5e, 2, 1);
  }

  {
    // End Vertical Blank Register (EVB) (CR16)
    UWORD vBlankEnd = mi->VerTotal;
    if ((modeFlags & GMF_DOUBLESCAN) != 0) {
      vBlankEnd = vBlankEnd * 2;
    }
    vBlankEnd = ((vBlankEnd - vBlankSize) >> interlaced) - 1;
    D(6, "VBlank End %ld\n", (ULONG)vBlankEnd);
    // FIXME: the blankSize is unaffected by double scan, but affected by
    // interlaced?
    W_CR(0x16, vBlankEnd);
  }

  UWORD vRetraceStart = TO_SCANLINES(mi->Height + mi->VerSyncStart);
  {
    // Vertical Retrace Start Register (VRS) (CR10)
    // FIXME: here VsyncStart is in lines, not scanlines, while mi->VerBlankSize
    // is in scanlines?
    D(6, "VRetrace Start %ld\n", (ULONG)vRetraceStart);
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
    D(6, "VRetrace End %ld\n", (ULONG)vRetraceEnd);
    W_CR_MASK(0x11, 0x0F, vRetraceEnd);
  }

  // Enable Interlace
  {
    UBYTE interlace = R_CR(0x42) & 0xdf;
    if (interlaced) {
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
  //    static const ULONG fifoWidth = 64 /*Bits*/ /8; // Bytes // Trio in 1MB
  //    config has only 32bits width

  //    ULONG entries = 1;
  //    ULONG pageModeCycle = 10;

  //    // Find M parameter for MCLK (how many clocks are given back to CPU
  //    memory
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
  //    memClock = (((memClock ) / (mi->PixelClock * 1000))) - bi->MemoryClock /
  //    2400 000) - 9 >> 1;

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
  //    // Engine can use to access memory before giving up control of the
  //    memory
  //    // bus. See Section 7.5 for more information. Bit 2 is the high order
  //    bit of
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

    R_REG(0x3DA);
    // write AR11 = 0 Border Color Register
    W_AR(0x11, 0);

    // Re-enable video out
    W_REG(ATR_AD, 0x20);
    R_REG(0x3DA);

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

  DFUNC(5,
      "mem 0x%lx, width %ld, height %ld, xoffset %ld, yoffset %ld, "
      "format %ld\n",
      memory, (ULONG)width, (ULONG)height, (LONG)xoffset, (LONG)yoffset,
      (ULONG)format);

  LONG panOffset;
  UWORD pitch;
  ULONG memOffset;

  bi->XOffset = xoffset;
  bi->YOffset = yoffset;
  memOffset = (ULONG)memory - (ULONG)bi->MemoryBase;

  switch (format) {
  case RGBFB_NONE:
    pitch = width >> 3;  // ?? can planar modes even be accessed?
    panOffset = (ULONG)yoffset * (width >> 3) + (xoffset >> 3);
    break;
  case RGBFB_R8G8B8:
  case RGBFB_B8G8R8:
  case RGBFB_A8R8G8B8:
  case RGBFB_A8B8G8R8:
  case RGBFB_R8G8B8A8:
  case RGBFB_B8G8R8A8:
    pitch = width * 4;
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

  D(5, "panOffset 0x%lx, pitch %ld dwords\n", panOffset, (ULONG)pitch);
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

  // Bits 5-4 of CR51 are extension bits 9-8 of this register. If these bits are
  // OOb, bit 2 of CR43 is extension bit 8 of this register.
  //  W_CR_MASK(0x43, 0x04, (pitch >> 6) & 0x04);

  Disable();

  R_REG(0x3DA);  // Reset AFF flip-flop // FIXME: why?

  Enable();
  return;
}

static APTR ASM CalculateMemory(__REGA0(struct BoardInfo *bi),
                                __REGA1(APTR mem), __REGD7(RGBFTYPE format))
{
  if (getChipData(bi)->Revision & 0x40)  // Trio64+?
  {
    switch (format) {
    case RGBFB_A8R8G8B8:
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
      // Redirect to Big Endian Linear Address Window. On the Prometheus, due to
      // its hardware byteswapping, this effectively makes the BE CPU writes
      // appear as LE in memory
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

  DFUNC(5, " state %ld\n", (ULONG)state);

  W_SR_MASK(0x01, 0x20, (~(UBYTE)state & 1) << 5);
  //  R_REG(0x3DA);
  //  W_REG(ATR_AD, 0x20);
  //  R_REG(0x3DA);
}

static ULONG ASM ResolvePixelClock(__REGA0(struct BoardInfo *bi),
                                   __REGA1(struct ModeInfo *mi),
                                   __REGD0(ULONG pixelClock),
                                   __REGD7(RGBFTYPE RGBFormat))
{
  mi->Flags &= ~GMF_ALWAYSBORDER;
  if (0x3ff < mi->VerTotal) {
    mi->Flags |= GMF_ALWAYSBORDER;
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

  D(15, "original Pixel Hz: %ld\n", pixelClock);

  int currentKhz = svga_compute_pll(&s3_pll, pixelClock / 1000, &m, &n, &r);
  if (currentKhz < 0) {
    D(0, "cannot resolve requested pixclock\n");
    return 0;
  }
  if (mi->Flags & GMF_DOUBLECLOCK) {
    currentKhz *= 2;
  }
  mi->PixelClock = currentKhz * 1000;

  D(15, "Pixelclock Hz: %ld\n\n", mi->PixelClock);

  // FIXME: would have to encode r here, too
  mi->pll1.Numerator = n;
  mi->pll2.Denominator = m;

  return currentKhz / 1000;  // use Mhz as "index"
}

static ULONG ASM GetPixelClock(__REGA0(struct BoardInfo *bi),
                               __REGA1(struct ModeInfo *mi),
                               __REGD0(ULONG index), __REGD7(RGBFTYPE format))
{
  DFUNC(5, "\n");

  ULONG pixelClockKhz = index * 1000;
  if (mi->Flags & GMF_DOUBLECLOCK) {
    pixelClockKhz /= 2;
  }

  UWORD m, n, r;

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

  DFUNC(5, "\n");

  ULONG pixelClock = bi->ModeInfo->PixelClock;

  if (bi->ModeInfo->Flags & GMF_DOUBLECLOCK) {
    pixelClock /= 2;
  }

  D(6, "SetClock: %ld -> %ld\n", bi->ModeInfo->PixelClock, pixelClock);

  UWORD m, n, r;
  int currentKhz = svga_compute_pll(&s3_pll, pixelClock / 1000, &m, &n, &r);
  if (currentKhz < 0) {
    D(0, "cannot resolve requested pixclock\n");
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

static void ASM SetMemoryModeInternal(__REGA0(struct BoardInfo *bi),
                                      __REGD7(RGBFTYPE format))
{
  REGBASE();

  //  DFUNC("\n");
  if (getChipData(bi)->Revision & 0x40)  // Trio64+?
  {
    if (getChipData(bi)->MemFormat == format) {
      return;
    }
    getChipData(bi)->MemFormat = format;

    // Setup the linear window CPU access such that the below formats will be
    // converted to the actual framebuffer format on write/read
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

static void ASM SetMemoryMode(__REGA0(struct BoardInfo *bi),
                              __REGD7(RGBFTYPE format))
{
  __asm __volatile("\t movem.l d0-d1/a0-a1,-(sp)\n"
                   : /* no result */
                   :
                   : "sp");

  SetMemoryModeInternal(bi, format);

  __asm __volatile("\t movem.l (sp)+,d0-d1/a0-a1\n"
                   : /* no result */
                   :
                   : "d0", "d1", "a0", "a1", "sp");
}

static void ASM SetWriteMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
  __asm __volatile("\t movem.l d0-d1/a0-a1,-(sp)\n"
                   : /* no result */
                   :
                   : "sp");

  //  SetWriteMaskInternal(bi, format);

  __asm __volatile("\t movem.l (sp)+,d0-d1/a0-a1\n"
                   : /* no result */
                   :
                   : "d0", "d1", "a0", "a1", "sp");
}

static void ASM SetClearMask(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
}

static void ASM SetReadPlane(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE mask))
{
}

static BOOL ASM GetVSyncState(__REGA0(struct BoardInfo *bi),
                              __REGD0(BOOL expected))
{
  REGBASE();
  return (R_REG(0x3DA) & 0x08) != 0;
}

static void WaitVerticalSync(__REGA0(struct BoardInfo *bi)) {}

static void ASM SetDPMSLevel(__REGA0(struct BoardInfo *bi),
                             __REGD0(ULONG level))
{
  //  DPMS_ON,      /* Full operation                             */
  //  DPMS_STANDBY, /* Optional state of minimal power reduction  */
  //  DPMS_SUSPEND, /* Significant reduction of power consumption */
  //  DPMS_OFF      /* Lowest level of power consumption */

  static const UBYTE DPMSLevels[4] = {0x00, 0x90, 0x60, 0x50};

  REGBASE();
  W_REG_MASK(0xd, 0xF0, DPMSLevels[level]);
}

static void ASM SetSplitPosition(__REGA0(struct BoardInfo *bi),
                                 __REGD0(SHORT splitPos))
{
  REGBASE();
  DFUNC(5, "%ld\n", (ULONG)splitPos);

  bi->YSplit = splitPos;
  if (!splitPos) {
    splitPos = 0x7ff;
  } else {
    if (bi->ModeInfo->Flags & GMF_DOUBLESCAN) {
      splitPos *= 2;
    }
  }
  W_CR_OVERFLOW3((UWORD)splitPos, 0x18, 0, 8, 0x7, 4, 1, 0x9, 6, 1, 0x5e, 6, 1);
}

static void ASM SetSpritePosition(__REGA0(struct BoardInfo *bi),
                                  __REGD0(WORD xpos), __REGD1(WORD ypos),
                                  __REGD7(RGBFTYPE fmt))
{
  DFUNC(5, "\n");
  REGBASE();

  bi->MouseX = xpos;
  bi->MouseY = ypos;

  WORD spriteX = xpos - bi->XOffset;
  WORD spriteY = ypos - bi->YOffset + bi->YSplit;
  if (bi->ModeInfo->Flags & GMF_DOUBLESCAN) {
    spriteY *= 2;
  }

  WORD offsetX = 0;
  if (spriteX < 0) {
    offsetX = -spriteX;
    spriteX = 0;
  }
  WORD offsetY = 0;
  if (spriteY < 0) {
    offsetY = -spriteY;
    spriteY = 0;
  }

  D(5, "SpritePos X: %ld 0x%lx, Y: %ld 0x%lx\n", (LONG)spriteX, (ULONG)spriteX,
    (LONG)spriteY, (ULONG)spriteY);
  // should we be able to handle negative values and use the offset registers
  // for that?
  W_CR_OVERFLOW1(spriteX, 0x47, 0, 8, 0x46, 0, 8);
  W_CR_OVERFLOW1(spriteY, 0x49, 0, 8, 0x48, 0, 8);
  W_CR(0x4e, offsetX & 63);
  W_CR(0x4f, offsetY & 63);
}

static void ASM SetSpriteImage(__REGA0(struct BoardInfo *bi),
                               __REGD7(RGBFTYPE fmt))
{
  DFUNC(5, "\n");

  // FIXME: need to set temporary memory format?
  // No, MouseImage should be in little endian window and not affected

  const UWORD *image = bi->MouseImage + 2;
  UWORD *cursor = (UWORD *)bi->MouseImageBuffer;
  for (UWORD y = 0; y < bi->MouseHeight; ++y) {
    // first 16 bit
    UWORD plane0 = *image++;
    UWORD plane1 = *image++;

    UWORD andMask = ~plane0;  // AND mask
    UWORD xorMask = plane1;   // XOR mask
    *cursor++ = andMask;
    *cursor++ = xorMask;
    // padding, should result in  screen color
    for (UWORD p = 0; p < 3; ++p) {
      *cursor++ = 0xFFFF;
      *cursor++ = 0x0000;
    }
  }
  // Pad the rest of the cursor image
  for (UWORD y = bi->MouseHeight; y < 64; ++y) {
    for (UWORD p = 0; p < 4; ++p) {
      *cursor++ = 0xFFFF;
      *cursor++ = 0x0000;
    }
  }
}

static void ASM SetSpriteColor(__REGA0(struct BoardInfo *bi),
                               __REGD0(UBYTE index), __REGD1(UBYTE red),
                               __REGD2(UBYTE green), __REGD3(UBYTE blue),
                               __REGD7(RGBFTYPE fmt))
{
  DFUNC(5, "Index %ld, Red %ld, Green %ld, Blue %ld\n", (ULONG)index, (ULONG)red,
        (ULONG)green, (ULONG)blue);
  REGBASE();
  LOCAL_SYSBASE();

  if (index != 0 && index != 2)
    return;

  UBYTE reg = 0;
  if (index == 0) {
    reg = 0x4B;
  } else {
    reg = 0x4A;
  }

  R_CR(0x45);  // Reset "Graphics Cursor Stack"
  switch (fmt) {
  case RGBFB_NONE:
  case RGBFB_CLUT: {
    UBYTE paletteEntry;
    if (index == 0) {
      paletteEntry = 17;  // Cursor Palette entry is fixed
    } else {
      paletteEntry = 19;
    }
    W_CR(reg, paletteEntry);
    W_REG(CRTC_DATA, paletteEntry);
    W_REG(CRTC_DATA, paletteEntry);
  } break;
  case RGBFB_B8G8R8A8:
  case RGBFB_A8R8G8B8: {
    W_CR(reg, blue);  // No Conversion needed for 24bit RGB
    W_REG(CRTC_DATA, green);
    W_REG(CRTC_DATA, red);
  } break;
  case RGBFB_R5G5B5PC:
  case RGBFB_R5G5B5: {
    UBYTE a =
        (blue >> 3) |
        ((green << 2) & 0xe);  // 16bit, just need to write the first two byte
    UBYTE b = (green >> 5) | ((red >> 1) & ~0x3);
    W_CR(reg, a);
    W_REG(CRTC_DATA, b);
  } break;
  case RGBFB_R5G6B5PC:
  case RGBFB_R5G6B5: {
    UBYTE a =
        (blue >> 3) | ((green << 3) &
                       0xe);  // // 16bit, just need to write the first two byte
    UBYTE b = (green >> 5) | (red & 0xf8);
    W_CR(reg, a);
    W_REG(CRTC_DATA, b);
  } break;
  }
}

static BOOL ASM SetSprite(__REGA0(struct BoardInfo *bi), __REGD0(BOOL activate),
                          __REGD7(RGBFTYPE RGBFormat))
{
  DFUNC(5, "\n");
  REGBASE();

  W_CR(0x45, activate ? 0x01 : 0x00);

  if (activate) {
    SetSpriteColor(bi, 0, bi->CLUT[17].Red, bi->CLUT[17].Green,
                   bi->CLUT[17].Blue, bi->RGBFormat);
    SetSpriteColor(bi, 1, bi->CLUT[18].Red, bi->CLUT[18].Green,
                   bi->CLUT[18].Blue, bi->RGBFormat);
    SetSpriteColor(bi, 2, bi->CLUT[19].Red, bi->CLUT[19].Green,
                   bi->CLUT[19].Blue, bi->RGBFormat);
  }

  return TRUE;
}

static inline void ASM WaitBlitter(__REGA0(struct BoardInfo *bi))
{
  REGBASE();
  //  LOCAL_SYSBASE();

  //  D("Waiting for blitter...");
  // FIXME: ideally you'd want this interrupt driven. I.e. sleep until the HW
  // interrupt indicates its done. Otherwise, whats the point of having the
  // blitter run asynchronous to the CPU?
  // FIXME AS a debug measure, Here we're waiting for the GE to finish AND all
  // FIFO slots to clear
  //  while ((R_REG_W(GP_STAT) & ((1<<9)|(1<<10))) != 1<<10)) {
  //  };

  while (R_REG_W(GP_STAT) & (1 << 9)) {
  };

  //  D("done\n");
}

static inline void REGARGS WaitFifo(struct BoardInfo *bi, BYTE numSlots)
{
  REGBASE();

//  assert(numSlots <= 13);

  // The FIFO bits are split into two groups, 7-0 and 15-11
  // Bit 7=0 (means 13 slots are available, bit 15 represents at least 5 slots available)

  BYTE testBit = 7 - (numSlots - 1);
  testBit &= 0xF; // handle wrap-around

  while (R_REG_W(GP_STAT) & (1 << testBit)) {
  };
}

#define MByte(x) ((x) * (1024 * 1024))
static inline void REGARGS getGESegmentAndOffset(ULONG memOffset,
                                                 WORD bytesPerRow, UBYTE bpp,
                                                 UWORD *segment, UWORD *xoffset,
                                                 UWORD *yoffset)
{
  *segment = (memOffset >> 20) & 7;

  ULONG srcOffset = memOffset & 0xFFFFF;
  *yoffset = srcOffset / bytesPerRow;
  *xoffset = (srcOffset % bytesPerRow) / bpp;

#ifdef DBG
  if (*segment > 0) {
    KPrintF("segment %ld, xoff %ld, yoff %ld, memoffset 0x%08lx\n",
            (ULONG)*segment, (ULONG)*xoffset, (ULONG)*yoffset, memOffset);
  }
#endif
}

static inline BOOL setCR50(struct BoardInfo *bi, UWORD bytesPerRow, UBYTE bpp)
{
  REGBASE();

  if (getChipData(bi)->GEbytesPerRow == bytesPerRow &&
      getChipData(bi)->GEbpp == bpp) {
    return TRUE;
  }
  getChipData(bi)->GEbytesPerRow = bytesPerRow;
  getChipData(bi)->GEbpp = bpp;

  WaitBlitter(bi);

  UWORD width = bytesPerRow / bpp;
  UBYTE CR31_1 = 0;
  UBYTE CR50_76_0 = 0;
  // Make the bytes per row compatible with the Graphics Engine's presets
  if (width == 640) {
    CR50_76_0 = 0b01000000;
  } else if (width == 800) {
    CR50_76_0 = 0b10000000;
  } else if (width == 1024) {
    CR50_76_0 = 0b00000000;
  } else if (width == 1152) {
    CR50_76_0 = 0b00000001;
  } else if (width == 1280) {
    CR50_76_0 = 0b11000000;
  } else if (width == 1600) {
    CR50_76_0 = 0b10000001;
  } else if (width == 2048) {
    CR31_1 = (1 << 1);
    CR50_76_0 = 0b00000000;
  } else {
    DFUNC(0,
        "Width unsupported by Graphics Engine, choosing unaccelerated  path\n");
    return FALSE;  // reserved
  }

  W_CR_MASK(0x50, 0xF1, CR50_76_0 | ((bpp - 1) << 4));
  W_CR_MASK(0x31, (1 << 1), CR31_1);

  return TRUE;
}

// struct RenderInfo {
//  APTR			Memory;
//  WORD			BytesPerRow;
//  WORD			pad;
//  RGBFTYPE		RGBFormat;
//};

static inline ULONG REGARGS getMemoryOffset(struct BoardInfo *bi, APTR memory)
{
  ULONG offset = (ULONG)memory - (ULONG)bi->MemoryBase;
  return offset;
}

#define TOP_LEFT (0b101 << 5)
#define TOP_RIGHT (0b100 << 5)
#define BOTTOM_LEFT (0b001 << 5)
#define BOTTOM_RIGHT (0b000 << 5)

static void ASM FillRect(__REGA0(struct BoardInfo *bi),
                         __REGA1(struct RenderInfo *ri), __REGD0(WORD x),
                         __REGD1(WORD y), __REGD2(WORD width),
                         __REGD3(WORD height), __REGD4(ULONG pen),
                         __REGD5(UBYTE mask), __REGD7(RGBFTYPE fmt))
{
  DFUNC(5,
      "\nx %ld, y %ld, w %ld, h %ld\npen %08lx, mask 0x%lx fmt %ld\n"
      "ri->bytesPerRow %ld, ri->memory 0x%lx\n",
      (ULONG)x, (ULONG)y, (ULONG)width, (ULONG)height, (ULONG)pen, (ULONG)mask,
      (ULONG)fmt, (ULONG)ri->BytesPerRow, (ULONG)ri->Memory);

  REGBASE();
  MMIOBASE();
  //  LOCAL_SYSBASE();

  UBYTE bpp = getBPP(fmt);
  if (!bpp) {
    bi->FillRectDefault(bi, ri, x, y, width, height, pen, mask, fmt);
    return;
  }

  if (!setCR50(bi, ri->BytesPerRow, bpp)) {
    bi->FillRectDefault(bi, ri, x, y, width, height, pen, mask, fmt);
    return;
  }
  UWORD seg;
  UWORD xoffset;
  UWORD yoffset;
  getGESegmentAndOffset(getMemoryOffset(bi, ri->Memory), ri->BytesPerRow, bpp,
                        &seg, &xoffset, &yoffset);

  x += xoffset;
  y += yoffset;

#ifdef DBG
  if ((x > (1 << 11)) || (y > (1 << 11))) {
    KPrintF("X %ld or Y %ld out of range\n", (ULONG)x, (ULONG)y);
  }
#endif

  if (getChipData(bi)->GEOp != RectFill) {
    getChipData(bi)->GEOp = RectFill;

    WaitFifo(bi, 13);
    // Set MULT_MISC first so that
    // "Bit 4 RSF - Select Upper Word in 32 Bits/Pixel Mode" is set to 0 and
    // Bit 9 CMR 32B - Select 32-Bit Command Registers
    W_BEE8(MULT_MISC, (1 << 9));

    W_BEE8(PIX_CNTL, 0x0000);
    W_REG_W(FRGD_MIX, CLR_SRC_FRGD_COLOR | MIX_NEW);
    //FIXME: set mask according to  'mask' parameter for CLUT modes
    W_REG_L(WRT_MASK, 0xFFFFFFFF);
  }
  else
  {
    WaitFifo(bi, 8);
  }

  // This could/should get chached as well
  W_BEE8(MULT_MISC2, seg << 4);

  W_MMIO_PACKED(ALT_CURXY, (x << 16) | y);
  W_MMIO_PACKED(ALT_PCNT, ((width - 1) << 16) | (height - 1));

  // Extended Memory Control 4 Register (EXT-MCTL-4) (CR61)
  //    Bits 6-5 BIG ENDIAN - Big Endian Data Bye Swap (image writes only)
  //        00 = No swap (Default)
  //        01 = Swap bytes within each word
  //        10 = Swap all bytes in doublewords (bytes reversed)
  //        11 = Reserved
  switch (fmt) {
  case RGBFB_A8R8G8B8:
    // swap all the bytes within a double word
    //      W_CR(0x61, 0b10<<5);
    //      W_CR_MASK(0x54, 0x3, 0x2);
    pen = swapl(pen);
    break;
  case RGBFB_R5G6B5:
  case RGBFB_R5G5B5:
    // FIXME: The FillRect doc says that Pen will be a 16bit value, but it
    // seems its actually 32 with both words having the same value?
    // Just swap the bytes within a word
    //      W_CR(0x61, 0b01<<5);
    //      W_CR_MASK(0x54, 0x3, 0x1);
    pen = swapl(pen);
    break;
  case RGBFB_CLUT:
    pen |= (pen << 8) | (pen << 16) | (pen << 24);
    break;
  default:
    //      W_CR(0x61, 0b00<<5);
    //      W_CR_MASK(0x54, 0x3, 0x0);
    break;
  }

  pen = swapl(pen);

  W_REG_L(FRGD_COLOR, pen);

  UWORD cmd = CMD_ALWAYS | CMD_TYPE_RECT_FILL | CMD_DRAW_PIXELS | TOP_LEFT;

  // CMD_BYTE_SWAP has effect on short stroke renddering only?
  //  switch (fmt) {
  //  case RGBFB_A8R8G8B8:
  //  case RGBFB_R5G6B5:
  //  case RGBFB_R5G5B5:
  //    // Just swap the bytes within a word
  //    cmd |= CMD_BYTE_SWAP;
  //  }

  W_REG_W(CMD, cmd);

  //  WaitBlitter(bi);
}

BOOL InitChipL(__REGA0(struct BoardInfo *bi))
{
  REGBASE();
  MMIOBASE();
  LOCAL_PROMETHEUSBASE();
  LOCAL_SYSBASE();

  getChipData(bi)->DOSBase = (ULONG)OpenLibrary(DOSNAME, 0);
  if (!getChipData(bi)->DOSBase) {
    return FALSE;
  }

  bi->GraphicsControllerType = GCT_S3Trio64;
  bi->PaletteChipType = PCT_S3Trio64;
  bi->Flags = bi->Flags | BIF_NOMEMORYMODEMIX | BIF_BORDERBLANK |
              BIF_NOP2CBLITS | BIB_BLITTER | BIF_GRANTDIRECTACCESS |
              BIF_VGASCREENSPLIT | BIF_HASSPRITEBUFFER | BIF_HARDWARESPRITE;
  // Trio64 supports BGR_8_8_8_X 24bit, R5G5B5 and R5G6B5 modes.
  // Prometheus does byte-swapping for writes to memory, so if we're writing a
  // 32bit register filled with XRGB, the written memory order will be BGRX
  bi->RGBFormats =
      RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B8G8R8A8;

  // We don't support these modes, but if we did, they would not allow for a HW
  // sprite
  bi->SoftSpriteFlags = RGBFF_B8G8R8 | RGBFF_R8G8B8;

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

  // Mouse Sprite
  bi->SetSprite = SetSprite;
  bi->SetSpritePosition = SetSpritePosition;
  bi->SetSpriteImage = SetSpriteImage;
  bi->SetSpriteColor = SetSpriteColor;

  // Blitter acceleration
  bi->WaitBlitter = WaitBlitter;
  //  bi->BlitRect = BlitRect;
  //  bi->InvertRect = InvertRect;
  bi->FillRect = FillRect;
  //  bi->BlitTemplate = BlitTemplate;
  //  bi->BlitPlanar2Chunky = BlitPlanar2Chunky;
  //  bi->BlitRectNoMaskComplete = BlitRectNoMaskComplete;
  //  bi->DrawLine = (_func_55.conflict *)&DrawLine;

  DFUNC(15,
      "WaitBlitter 0x%08lx\nBlitRect 0x%08lx\nInvertRect 0x%08lx\nFillRect "
      "0x%08lx\n"
      "BlitTemplate 0x%08lx\n BlitPlanar2Chunky 0x%08lx\n"
      "BlitRectNoMaskComplete 0x%08lx\n DrawLine 0x%08lx\n",
      bi->WaitBlitter, bi->BlitRect, bi->InvertRect, bi->FillRect,
      bi->BlitTemplate, bi->BlitPlanar2Chunky, bi->BlitRectNoMaskComplete,
      bi->DrawLine);

  bi->PixelClockCount[PLANAR] = 0;
  bi->PixelClockCount[CHUNKY] =
      135;  // > 80Mhz can be achieved via Double Clock mode
  bi->PixelClockCount[HICOLOR] = 80;
  bi->PixelClockCount[TRUECOLOR] = 50;
  bi->PixelClockCount[TRUEALPHA] = 50;

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

  if (getChipData(bi)->Revision & 0x40) {
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
    D(0, "Chip is Trio64+ (Rev %ld) in %s mode\n", (ULONG)chipRevision & 0x0f,
      modeString);

    // We can support byte-swapped formats on this chip via the Big Linear
    // Adressing Window
    bi->RGBFormats |= RGBFF_A8R8G8B8 | RGBFF_R5G6B5 | RGBFF_R5G5B5;
  } else {
    D(0, "Chip is Trio64/32 (Rev %ld)\n", (ULONG)chipRevision);
  }

  /* The Enhanced Graphics Command register group is unlocked
     by setting bit 0 of the System Configuration register (CR40) to 1.
     After that, bitO of4AE8H must be setto 1 to enable Enhanced mode functions.
      */
  W_CR_MASK(0x40, 0x1, 0x1);

  /* Now that we enabled enhanced mode register access;
   * Enable enhanced mode functions,  write lower byte of 0x4AE8
   * WARNING: DO NOT ENABLE MMIO WITH BIT 5 HERE.
   * This bit will be OR'd into CR53 and thus makes into impossible to setup
   * "new MMIO only" mode on Trio64+. This is despite the docs claiming Bit 5 is
   * "reserved" on there.
   */
  W_REG(ADVFUNC_CNTL, 0x01);

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
  //  W_SR(0xa, 0xc0);
  //  W_SR(0x18, 0xc0);

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

  // 5 DRAM refresh cycles, unlock CR0/CR7, disable Vertical Interrupt
  W_CR(0x11, 0xe);

  W_CR(0x12, 0x8f);

  // Offset Register (SCREEN-OFFSET) (CR13)
  // Specifies the logical screen width (pitch). Bits 5-4 of CR51 are extension
  // bits 9-8 for this value. If these bits are OOb, bit 2 of
  // CR43 is extension bit 8 of this register.
  // 10-bit Value = quantity that is multiplied by 2 (word mode), 4 (doubleword
  // mode) or 8 (quadword mode) to specify the difference between the starting
  // byte addresses of two consecutive scan lines. This register contains the
  // least significant 8 bits of this

  W_CR(0x13, 0x50);  // == 160, meaning 640byte in double word mode

  // Underline Location Register (ULL) (CR14) (affects address
  // counting)
  //  Bit 5 CNT BY4 - Select Count by 4 Mode
  //      0= The memory address counter depends on bit 3 of CR17 (count by 2)
  //      1 = The memory address counter is incremented every four character
  //      clocks
  //              The CNT BY4 bit is used when double word addresses are used.
  //  Bit 6 DBLWD MODE - Select Doubleword Mode
  //      0 = The memory addresses are byte or word addresses
  //      1 = The memory addresses are doubleword addresses
  //
  W_CR(0x14, 0x40);

  W_CR(0x15, 0x96);  // Start Vertical Blank Register (SVB) (CR15)
  W_CR(0x16, 0xb9);  // End Vertical Blank Register (EVB) (CR16)

  //  CRTC Mode Control Register (CRT _MO) (CR17
  //  Bit 3 CNT BY2 - Select Word Mode
  //  0= Memory address counter is clocked with the character clock input, and
  //  byte mode addressing for the video memory is selected 1 = Memory address
  //  counter is clocked by the character clock input divided by 2, and word
  //  mode addressing for the video memory is selected Bit 6 BYTE MODE - Select
  //  Byte Addressing Mode
  //      0 = Word mode shifts all memory address counter bits down one bit, and
  //      the most
  //          significant bit of the counter appears on the least significant
  //          bit of the memory address output
  //      1 = Byte address mode
  W_CR(0x17, 0xC3);  // Byte Adressing mode, V/HSync pulses enabled

  W_CR(0x18, 0xff);  // Line compare register

  // Memory Configuration Register (MEM_CNFG) (eR31)
  // Bit 3 ENH MAP - Use Enhanced Mode Memory Mapping
  // 0= Force IBM VGA mapping for memory accesses
  // 1 = Force Enhanced Mode mappings
  // Setting this bit to 1 overrides the settings of bit 6 of CR14 and bit 3 of
  // CR17 and causes the use of doubleword memory addressing mode. Also, the
  // function of bits 3- 2 of GR6 is overridden with a fixed 64K map at AOOOOH.
  W_CR(0x31, 0x08);  // Enhanced memory mapping, Doubleword mode

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

  // Extended System Control 2 Register (EX_SCTL_2) (CR51)
  W_CR(0x51, 0x00);

  // Enable 4MB Linear Address Window (LAW)
  W_CR(0x58, 0x13);
  if (isTrio64Plus) {
    // Enable Trio64+ "New MMIO" only and byte swapping in the Big Endian window
    D(5, "setup New MMIO\n");
    W_CR_MASK(0x53, 0x3E, 0x0c);
  } else {
    D(5, "setup compatible MMIO\n");
    // Enable Trio64 old style MMIO. This hardcodes the MMIO range to 0xA8000
    // physical address. Need to make sure, nothing else sits there
    W_CR_MASK(0x53, 0x10, 0x10);

    // LAW start address
    ULONG physAddress = Prm_GetPhysicalAddress(bi->MemoryBase);
    if (physAddress & 0x3FFFFF) {
      D(0, "WARNING: card's base address is not 4MB aligned!\n");
    }
    W_CR_MASK(0x5a, physAddress >> 16, 0x7F);
    // Upper address bits may  not be touched as they would result in shifting
    // the PCI window
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

  // Enable writing attribute pallette registers, disable video
  R_REG(0x3DA);
  W_REG(ATR_AD, 0x0);

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

  // Enable video
  R_REG(0x3DA);
  W_REG(ATR_AD, 0x20);

  /* Enable PLL load */
  W_MISC_MASK(0x0c, 0x0c);

  // Just some diagnostics
#ifdef DBG
  UBYTE memType = (R_CR(0x36) >> 2) & 3;
  switch (memType) {
  case 0b00:
    D(1, "1-cycle EDO\n");
    break;
  case 0b10:
    D(1, "2-cycle EDO\n");
    break;
  case 0b11:
    D(1, "FPM\n");
    break;
  default:
    D(0, "unknown memory type\n");
  }
#endif

  // Determine memory size of the card (typically 1-2MB, but can be up to 4MB)
  bi->MemorySize = 0x400000;
  volatile ULONG *framebuffer = (volatile ULONG *)bi->MemoryBase;
  framebuffer[0] = 0;
  while (bi->MemorySize) {
    D(1, "Probing memory size %ld\n", bi->MemorySize);

    // Enable Linear Addressing Window LAW
    {
      UBYTE LAWSize = 0;
      UBYTE MemSize = 0;
      if (bi->MemorySize >= 0x400000) {
        LAWSize = 0b11;
        MemSize = 0b000;
      } else if (bi->MemorySize >= 0x200000) {
        LAWSize = 0b10;  // 2MB
        MemSize = 0b100;
      } else {
        LAWSize = 0b01;  // 1MB
        MemSize = 0b110;
      }
      W_CR_MASK(0x36, 0xE0, MemSize << 5);
      W_CR_MASK(0x58, 0x13, LAWSize | 0x10);
    }

    CacheClearU();

    // Probe the last and the first longword for the current segment,
    // as well as offset 0 to check for wrap arounds
    volatile ULONG *highOffset = framebuffer + (bi->MemorySize >> 2) - 1;
    volatile ULONG *lowOffset = framebuffer + (bi->MemorySize >> 3);
    // Probe  memory
    *framebuffer = 0;
    *highOffset = (ULONG)highOffset;
    *lowOffset = (ULONG)lowOffset;

    CacheClearU();

    ULONG readbackHigh = *highOffset;
    ULONG readbackLow = *lowOffset;
    ULONG readbackZero = *framebuffer;

    D(5, "S3Trio64: probing memory at 0x%lx ?= 0x%lx; 0x%lx ?= 0x%lx, 0x0 ?= "
      "0x%lx\n",
      highOffset, readbackHigh, lowOffset, readbackLow, readbackZero);

    if (readbackHigh == (ULONG)highOffset && readbackLow == (ULONG)lowOffset &&
        readbackZero == 0) {
      break;
    }
    // reduce available memory size
    bi->MemorySize >>= 1;
  }

  D(1, "S3Trio64: memorySize %ldmb\n", bi->MemorySize / (1024 * 1024));

  // Input Status ? Register (STATUS_O)
  D(1, "Monitor is %s present\n", ((R_REG(0x3C2) & 0x10) ? "" : "NOT"));

  // Two sprite images, each 64x64*2 bits
  const ULONG maxSpriteBuffersSize = (64 * 64 * 2 / 8) * 2;

  // take sprite image data off the top of the memory
  // sprites can be placed at segment boundaries of 1kb
  bi->MemorySize = (bi->MemorySize - maxSpriteBuffersSize) & ~(1024 - 1);
  bi->MouseImageBuffer = bi->MemoryBase + bi->MemorySize;
  bi->MouseSaveBuffer =
      bi->MemoryBase + bi->MemorySize + maxSpriteBuffersSize / 2;

  // Start Address in terms of 1024byte segments
  W_CR_OVERFLOW1((bi->MemorySize >> 10), 0x4d, 0, 8, 0x4c, 0, 4);
  // Sprite image offsets
  W_CR(0x4e, 0);
  W_CR(0x4f, 0);
  // Reset cursor position
  W_CR(0x46, 0);
  W_CR(0x47, 0);
  W_CR(0x48, 0);
  W_CR(0x49, 0);

  // Write conservative scissors
  W_REG_W(0xBEE8, 0x1000);
  W_REG_W(0xBEE8, 0x2000);
  W_REG_W(0xBEE8, 0x3fff);
  W_REG_W(0xBEE8, 0x4fff);

  W_BEE8(MULT_MISC, 0);
  W_BEE8(MULT_MISC2, 0);
  // Init GE write/read masks
  W_REG_W(WRT_MASK, 0xFFFF);
  W_REG_W(WRT_MASK, 0xFFFF);
  W_REG_W(RD_MASK, 0xFFFF);
  W_REG_W(RD_MASK, 0xFFFF);
  W_REG_W(COLOR_CMP, 0x0);
  W_REG_W(COLOR_CMP, 0x0);

  W_REG_W(FRGD_MIX, CLR_SRC_FRGD_COLOR | MIX_NEW);
  W_REG_W(BKGD_MIX, CLR_SRC_BKGD_COLOR | MIX_NEW);

  W_REG_W(CMD, CMD_ALWAYS|CMD_TYPE_NOP);

  return TRUE;
}
