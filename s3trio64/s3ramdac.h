#ifndef S3RAMDAC_H
#define S3RAMDAC_H

#include <exec/types.h>
#include <boardinfo.h>

#define DAC_ENABLE_RS2() W_CR_MASK(0x55, 0x01, 0x01);  // Clear RS2 bit for direct register access
#define DAC_DISABLE_RS2() W_CR_MASK(0x55, 0x01, 0x00);  // Clear RS2 bit for direct register access

extern BOOL CheckForSDAC(struct BoardInfo *bi);
extern BOOL CheckForRGB524(struct BoardInfo *bi);
extern BOOL InitRGB524(struct BoardInfo *bi);

#endif // S3RAMDAC_H
