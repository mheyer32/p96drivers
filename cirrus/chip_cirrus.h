#ifndef CHIP_CIRRUS_H
#define CHIP_CIRRUS_H

#include "cirrus_common.h"
#include "cirrusconfig.h"

/* Sequencer indices (see Cirrus CL-GD542X TRM, ch. 9) */
#define SR_UNLOCK_EXTENSIONS 0x06
#define SR_UNLOCK_VALUE        0x12

#define SR_VCLK_NUM 0x1A
#define SR_VCLK_DEN 0x1E

/* CR27[7:2] device id field (TRM table 9.56) */
#define CR27_CHIP_ID 0x27

/* Standard VGA palette DAC I/O (TRM / IBM VGA) */
#define DAC_MASK  0x3C6
#define DAC_WR_AD 0x3C8
#define DAC_DATA  0x3C9

#endif
