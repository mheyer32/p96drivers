#ifndef CHIP_S3TRIO64_H
#define CHIP_S3TRIO64_H

#include <SDI_compiler.h>
#include <clib/debug_protos.h>

#include <boardinfo.h>
// FIXME: copy header into common location
#include <../PromLib/endian.h>

#define ChipRevision ChipData[0]
#define ChipDOSBase ChipData[1]
#define CardPrometheusBase CardData[0]

#define LOCAL_SYSBASE() struct ExecBase *SysBase = bi->ExecBase
#define LOCAL_PROMETHEUSBASE() \
  struct Library *PrometheusBase = (struct Library *)(bi->CardPrometheusBase)
#define LOCAL_DOSBASE() \
  struct Library *DOSBase = (struct Library *)(bi->ChipDOSBase)

// The offsets allow for using signed 16bit indexed addressing be used
#define REGISTER_OFFSET 0x8000
#define MMIOREGISTER_OFFSET 0x8000

#define SEQX 0x3C4  // Access SRxx registers
#define SEQ_DATA 0x3C5

#define CRTC_IDX 0x3D4  // Access CRxx registers
#define CRTC_DATA 0x3D5

#define GRC_ADR 0x3CE
#define GRC_DATA 0x3CF

#define ATR_AD 0x3C0
#define ATR_DATA_W 0x3C0
#define ATR_DATA_R 0x3C1

#define DAC_WR_AD 0x3C8
#define DAC_DATA 0x3C9

static INLINE REGARGS volatile UBYTE *getLegacyBase(const struct BoardInfo *bi)
{
  return bi->RegisterBase;
}

static INLINE REGARGS volatile UBYTE *getMMIOBase(const struct BoardInfo *bi)
{
  return bi->MemoryIOBase;
}

static inline UBYTE REGARGS readReg(volatile UBYTE *regbase, UWORD reg)
{
  return regbase[reg - REGISTER_OFFSET];
}

static inline void REGARGS writeReg(volatile UBYTE *regbase, UWORD reg,
                                    UBYTE value)
{
  regbase[reg - REGISTER_OFFSET] = value;
}

static inline UWORD REGARGS readRegW(volatile UBYTE *regbase, UWORD reg)
{
  return swapw(*(volatile UWORD *)(regbase + (reg - REGISTER_OFFSET)));
}

static inline void REGARGS writeRegW(volatile UBYTE *regbase, UWORD reg,
                                     UWORD value)
{
  *(volatile UWORD *)(regbase + (reg - REGISTER_OFFSET)) = swapw(value);
}

static inline UWORD REGARGS readRegMMIOW(volatile UBYTE *regbase, UWORD reg)
{
  return *(volatile UWORD *)(regbase + (reg - MMIOREGISTER_OFFSET));
}

static inline void REGARGS writeRegMMIOW(volatile UBYTE *regbase, UWORD reg,
                                         UWORD value)
{
  *(volatile UWORD *)(regbase + (reg - MMIOREGISTER_OFFSET)) = value;
}

static inline UBYTE REGARGS readRegister(volatile UBYTE *regbase, UWORD reg)
{
  UBYTE value = readReg(regbase, reg);
#ifdef DBG1
  KPrintF("R 0x%.4lx -> 0x%02lx\n", (LONG)reg, (LONG)value);
#endif
  return value;
}

static inline void REGARGS writeRegister(volatile UBYTE *regbase, UWORD reg,
                                         UBYTE value)
{
  writeReg(regbase, reg, value);
#ifdef DBG
  KPrintF("W 0x%.4lx <- 0x%02lx\n", (LONG)reg, (LONG)value);
#endif
}

static inline void REGARGS writeRegisterMask(volatile UBYTE *regbase, UWORD reg,
                                             UBYTE mask, UBYTE value)
{
  writeRegister(regbase, reg,
                (readRegister(regbase, reg) & ~mask) | (value & mask));
}

static inline UBYTE REGARGS readCRx(volatile UBYTE *regbase, UBYTE regIndex)
{
  writeReg(regbase, CRTC_IDX, regIndex);
  UBYTE value = readReg(regbase, CRTC_DATA);
#ifdef DBG1
  KPrintF("R CR%.2lx -> 0x%02lx\n", (LONG)regIndex, (LONG)value);
#endif
  return value;
}

static inline void REGARGS writeCRx(volatile UBYTE *regbase, UBYTE regIndex,
                                    UBYTE value)
{
  writeReg(regbase, CRTC_IDX, regIndex);
  writeReg(regbase, CRTC_DATA, value);
// FIXME: this doesn't work. I was hoping to write CRTC_IDX and CRTC_DATA in one
// go
//  writeRegW(regbase, CRTC_IDX, (regIndex << 8) | value );
#ifdef DBG
  KPrintF("W CR%.2lx <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
#endif
}

static inline void REGARGS writeCRxMask(volatile UBYTE *regbase, UBYTE regIndex,
                                        UBYTE mask, UBYTE value)
{
  UBYTE regvalue = (readCRx(regbase, regIndex) & ~mask) | (value & mask);
  // Keep index register from previous read
  writeReg(regbase, CRTC_DATA, regvalue);
#ifdef DBG
  KPrintF("W CR%.2lx <- 0x%02lx\n", (LONG)regIndex, (LONG)regvalue);
#endif
}

static inline UBYTE REGARGS readSRx(volatile UBYTE *regbase, UBYTE regIndex)
{
  writeReg(regbase, SEQX, regIndex);
  UBYTE value = readReg(regbase, SEQ_DATA);
#ifdef DBG1
  KPrintF("R SR%2lx -> 0x%02lx\n", (LONG)regIndex, (LONG)value);
#endif
  return value;
}

static inline void REGARGS writeSRx(volatile UBYTE *regbase, UBYTE regIndex,
                                    UBYTE value)
{
  writeReg(regbase, SEQX, regIndex);
  writeReg(regbase, SEQ_DATA, value);
#ifdef DBG
  KPrintF("W SR%2lx <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
#endif
}

static inline void REGARGS writeSRxMask(volatile UBYTE *regbase, UBYTE regIndex,
                                        UBYTE mask, UBYTE value)
{
  writeSRx(regbase, regIndex,
           (readSRx(regbase, regIndex) & ~mask) | (value & mask));
}

static inline UBYTE REGARGS readGRx(volatile UBYTE *regbase, UBYTE regIndex)
{
  writeReg(regbase, GRC_ADR, regIndex);
  UBYTE value = readReg(regbase, GRC_DATA);
#ifdef DBG
  KPrintF("R GR%2lx -> 0x%02lx\n", (LONG)regIndex, (LONG)value);
#endif
  return value;
}

static inline void REGARGS writeGRx(volatile UBYTE *regbase, UBYTE regIndex,
                                    UBYTE value)
{
  writeReg(regbase, GRC_ADR, regIndex);
  writeReg(regbase, GRC_DATA, value);
#ifdef DBG
  KPrintF("W GR%2lx <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
#endif
}

static inline UBYTE REGARGS readARx(volatile UBYTE *regbase, UBYTE regIndex)
{
  writeReg(regbase, ATR_AD, regIndex);
  UBYTE value = readReg(regbase, ATR_DATA_R);
#ifdef DBG1
  KPrintF("R AR%2lx -> 0x%lx\n", (LONG)regIndex, (LONG)value);
#endif
  return value;
}

static inline void REGARGS writeARx(volatile UBYTE *regbase, UBYTE regIndex,
                                    UBYTE value)
{
  // This disables the "Enable video display" bit, but only when it is disabled,
  // the Attribute Controller registers may be accessed
  writeReg(regbase, ATR_AD, regIndex);
  writeReg(regbase, ATR_DATA_W, value);
#ifdef DBG
  KPrintF("W AR%2lx <- 0x%02lx\n", (LONG)regIndex, (LONG)value);
#endif
}

static inline void REGARGS writeMISC_OUT(volatile UBYTE *regbase, UBYTE mask,
                                         UBYTE value)
{
  UBYTE misc = (readRegister(regbase, 0x3CC) & ~mask) | (value & mask);
  writeRegister(regbase, 0x3C2, misc);
}

#define REGBASE() volatile UBYTE *RegBase = getLegacyBase(bi)
#define MMIOBASE() volatile UBYTE *MMIOBase = getMMIOBase(bi)

#define R_CR(reg) readCRx(RegBase, reg)
#define W_CR(reg, value) writeCRx(RegBase, reg, value)
#define W_CR_MASK(reg, mask, value) writeCRxMask(RegBase, reg, mask, value)

#define R_SR(reg) readSRx(RegBase, reg)
#define W_SR(reg, value) writeSRx(RegBase, reg, value)
#define W_SR_MASK(reg, mask, value) writeSRxMask(RegBase, reg, mask, value)

#define R_GR(reg) readGRx(RegBase, reg)
#define W_GR(reg, value) writeGRx(RegBase, reg, value)

#define R_AR(reg) readARx(RegBase, reg)
#define W_AR(reg, value) writeARx(RegBase, reg, value)

#define R_REG(reg) readRegister(RegBase, reg)
#define W_REG(reg, value) writeRegister(RegBase, reg, value)
#define W_REG_MASK(reg, mask, value) \
  writeRegisterMask(RegBase, reg, mask, value)

#define R_REG_W(reg) readRegW(RegBase, reg)
#define W_REG_W(reg, value) writeRegW(RegBase, reg, value)

#define R_REG_W_MMIO(reg) readRegMMIOW(MMIOBase, reg)
#define W_REG_W_MMIO(reg, value) writeRegMMIOW(MMIOBase, reg, value)

#define W_MISC_MASK(mask, value) writeMISC_OUT(RegBase, mask, value)

#define _W_CR_OF(value, reg, bitPos, numBits)                \
  if (numBits < 8) {                                         \
    UWORD mask_W_CR_OF = ((1 << (numBits)) - 1) << (bitPos); \
    UWORD val_W_CR_OF = ((value) << (bitPos));               \
    W_CR_MASK(reg, mask_W_CR_OF, val_W_CR_OF);               \
  } else {                                                   \
    W_CR(reg, value);                                        \
  }

#define W_CR_OVERFLOW1(value, reg, bitPos1, numBits1, overflowReg, bitPos2, \
                       numBits2)                                            \
  do {                                                                      \
    UWORD val_W_CR_OVERFLOW1 = value;                                       \
    _W_CR_OF(val_W_CR_OVERFLOW1, reg, bitPos1, numBits1);                   \
    val_W_CR_OVERFLOW1 >>= numBits1;                                        \
    _W_CR_OF(val_W_CR_OVERFLOW1, overflowReg, bitPos2, numBits2);           \
  } while (0);

#define W_CR_OVERFLOW2(value, reg, bitPos1, numBits1, overflowReg, bitPos2, \
                       numBits2, extOverflowReg, bitPos3, numBits3)         \
  do {                                                                      \
    _W_CR_OF(value, reg, bitPos1, numBits1);                                \
    _W_CR_OF((value >> numBits1), overflowReg, bitPos2, numBits2);          \
    _W_CR_OF((value >> (numBits1 + numBits2)), extOverflowReg, bitPos3,     \
             numBits3);                                                     \
  } while (0);

#define W_CR_OVERFLOW2_ULONG(value, reg, bitPos1, numBits1, overflowReg,  \
                             bitPos2, numBits2, extOverflowReg, bitPos3,  \
                             numBits3)                                    \
  do {                                                                    \
    ULONG val_W_CR_OVERFLOW2_LONG = value;                                \
    _W_CR_OF(val_W_CR_OVERFLOW2_LONG, reg, bitPos1, numBits1);            \
    val_W_CR_OVERFLOW2_LONG >>= numBits1;                                 \
    _W_CR_OF(val_W_CR_OVERFLOW2_LONG, overflowReg, bitPos2, numBits2);    \
    val_W_CR_OVERFLOW2_LONG >>= numBits2;                                 \
    _W_CR_OF(val_W_CR_OVERFLOW2_LONG, extOverflowReg, bitPos3, numBits3); \
  } while (0);

// FIXME: can this be done vargs style?
#define W_CR_OVERFLOW3(value, reg, bitPos1, numBits1, overflowReg, bitPos2, \
                       numBits2, extOverflowReg, bitPos3, numBits3,         \
                       extOverflowReg2, bitPos4, numBits4)                  \
  do {                                                                      \
    UWORD val_W_CR_OVERFLOW3 = value;                                       \
    _W_CR_OF(val_W_CR_OVERFLOW3, reg, bitPos1, numBits1);                   \
    val_W_CR_OVERFLOW3 >>= numBits1;                                        \
    _W_CR_OF(val_W_CR_OVERFLOW3, overflowReg, bitPos2, numBits2);           \
    val_W_CR_OVERFLOW3 >>= numBits2;                                        \
    _W_CR_OF(val_W_CR_OVERFLOW3, extOverflowReg, bitPos3, numBits3);        \
    val_W_CR_OVERFLOW3 >>= numBits3;                                        \
    _W_CR_OF(val_W_CR_OVERFLOW3, extOverflowReg2, bitPos4, numBits4);       \
  } while (0);

BOOL InitChipL(__REGA0(struct BoardInfo *bi));

#endif
