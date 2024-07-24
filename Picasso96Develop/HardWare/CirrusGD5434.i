	include	exec/types.i
	include	exec/nodes.i

CIRRUS34NAME	MACRO
		dc.b	'CirrusGD5434'
		ENDM

***********************************************************

MOUSEBUFFERSIZE		EQU	MAXSPRITEWIDTH*MAXSPRITEHEIGHT*4	; Software Sprite (4 = max: TrueAlpha)
SPRITEBUFFERSIZE	EQU	1024					; Hardware Sprite
MISCBUFFERSIZE		EQU	32*1024-2*MOUSEBUFFERSIZE-2*SPRITEBUFFERSIZE	; 32 KB Reserved Memory Total!

 STRUCTURE	RSVMEM,0
	STRUCT	rm_MiscBuffer,MISCBUFFERSIZE
	STRUCT	rm_MouseSaveBuffer,MOUSEBUFFERSIZE
	STRUCT	rm_MouseImageBuffer,MOUSEBUFFERSIZE
	STRUCT	rm_HardWareSprite1,SPRITEBUFFERSIZE
	STRUCT	rm_HardWareSprite2,SPRITEBUFFERSIZE
	LABEL	RESERVEDMEMORY

***********************************************************

gbi_CurrentMemoryMode		EQU	gbi_ChipData+0*4
gbi_SpriteBank			EQU	gbi_ChipData+1*4
gbi_DACModes			EQU	gbi_ChipData+11*4 ;MSB: lower,LSB top
										    
***********************************************************
MISCOUTPUTR	EQU	$3CC	;
MISCOUTPUTW	EQU	$3C2	;
INPUTSTATUS0	EQU	$3C2	;
INPUTSTATUS1	EQU	$3DA	;
FEATURECONTROLR	EQU	$3CA	;
FEATURECONTROLW	EQU	$3DA	;
VIDEOSYSENABLE	EQU	$3C3	; $46E8
DISPMODECONTROL	EQU	$3D8	;
PALETTEADDRESS	EQU	$3C8	;
PALETTEDATA	EQU	$3C9	;
ATNTMODECONTROL	EQU	$3DE	;
HERCULESCOMPAT	EQU	$3BF	;
***********************************************************

MAX_BLTHEIGHT	EQU	1024
MAX_BLTWIDTH	EQU	8184
MAX_BLTPITCH	EQU	8184

***********************************************************
TSI	EQU	$3C4	;
TSD	EQU	$3C5	;
***********************************************************
	ENUM
	EITEM	TS_SynchronousReset
	EITEM	TS_TSMode
	EITEM	TS_WritePlaneMask
	EITEM	TS_FontSelect
	EITEM	TS_MemoryMode
	EITEM	TS_Dummy1
	EITEM	TS_UnlockAllExtensions
	EITEM	TS_ExtendedSequencerMode
	EITEM	TS_EEPROMControl
	EITEM	TS_ScratchPad0
	EITEM	TS_ScratchPad1
	EITEM	TS_VCLK0Numerator
	EITEM	TS_VCLK1Numerator
	EITEM	TS_VCLK2Numerator
	EITEM	TS_VCLK3Numerator
	EITEM	TS_DRAMControl
	EITEM	TS_GraphicsCursorXPosition
	EITEM	TS_GraphicsCursorYPosition
	EITEM	TS_GraphicsCursorAttributes
	EITEM	TS_GraphicsCursorPattern
	EITEM	TS_ScratchPad2
	EITEM	TS_ScratchPad3
	EITEM	TS_PerformanceTuning
	EITEM	TS_ConfigReadback
	EITEM	TS_SignatureGeneratorControl
	EITEM	TS_SignatureGeneratorResultLow
	EITEM	TS_SignatureGeneratorResultHigh
	EITEM	TS_VCLK0Denominator
	EITEM	TS_VCLK1Denominator
	EITEM	TS_VCLK2Denominator
	EITEM	TS_VCLK3Denominator
	EITEM	TS_MCLKSelect

SetTS	MACRO	Value, Index
	move.b	#\2,TSI(a0)
	move.b	\1,TSD(a0)
	ENDM

SetTSw	MACRO	Value, Index, Temp
	move.w	#(\2<<8),\3
	move.b	\1,\3
	move.w	\3,TSI(a0)
	ENDM

SetTSc	MACRO	ConstValue, Index
	move.w	\1|(\2<<8),TSI(a0)
	ENDM

***********************************************************
GDCI	EQU	$3CE	;
GDCD	EQU	$3CF	;
***********************************************************
	ENUM
	EITEM	GDC_SetReset
	EITEM	GDC_EnableSetReset
	EITEM	GDC_ColorCompare
	EITEM	GDC_DataRotateFunctionSelect
	EITEM	GDC_ReadPlaneSelect
	EITEM	GDC_GDCMode
	EITEM	GDC_Miscellaneous
	EITEM	GDC_ColorCare
	EITEM	GDC_BitMask
	EITEM	GDC_Offset0
	EITEM	GDC_Offset1
	EITEM	GDC_GDCModeExtensions
	EITEM	GDC_ColorKeyCompare
	EITEM	GDC_ColorKeyCompareMask
	EITEM	GDC_PowerManagement
	ENUM	$10
	EITEM	GDC_PixelBgColorByte1
	EITEM	GDC_PixelFgColorByte1
	EITEM	GDC_PixelBgColorByte2
	EITEM	GDC_PixelFgColorByte2
	EITEM	GDC_PixelBgColorByte3
	EITEM	GDC_PixelFgColorByte3
	ENUM	$20
	EITEM	GDC_BLTWidthLow
	EITEM	GDC_BLTWidthHigh
	EITEM	GDC_BLTHeightLow
	EITEM	GDC_BLTHeightHigh
	EITEM	GDC_BLTDstPitchLow
	EITEM	GDC_BLTDstPitchHigh
	EITEM	GDC_BLTSrcPitchLow
	EITEM	GDC_BLTSrcPitchHigh
	EITEM	GDC_BLTDstStartLow
	EITEM	GDC_BLTDstStartMiddle
	EITEM	GDC_BLTDstStartHigh
	EITEM	GDC_Dummy1
	EITEM	GDC_BLTSrcStartLow
	EITEM	GDC_BLTSrcStartMiddle
	EITEM	GDC_BLTSrcStartHigh
	EITEM	GDC_Dummy2
	EITEM	GDC_BLTMode
	EITEM	GDC_BLTStartStatus
	EITEM	GDC_BLTRasterOperation

GDC_PixelBgColorByte0	equ	GDC_SetReset
GDC_PixelFgColorByte0	equ	GDC_EnableSetReset

SetGDC	MACRO	Value, Index
	move.b	#\2,GDCI(a0)
	move.b	\1,GDCD(a0)
	ENDM

SetGDCw	MACRO	Value, Index, Temp
	move.w	#(\2<<8),\3
	move.b	\1,\3
	move.w	\3,GDCI(a0)
	ENDM

SetGDCc	MACRO	Value, Index
	move.w	\1|(\2<<8),GDCI(a0)
	ENDM

SetBLTSrcStart	MACRO	Address, RegisterBase
	move.b	#GDC_BLTSrcStartLow,GDCI(\2)
	move.b	\1,GDCD(\2)
	lsr.w	#8,\1
	move.b	#GDC_BLTSrcStartMiddle,GDCI(\2)
	move.b	\1,GDCD(\2)
	swap	\1
	and.b	#$3f,\1
	move.b	#GDC_BLTSrcStartHigh,GDCI(\2)
	move.b	\1,GDCD(\2)
	ENDM

SetBLTSrcStartW	MACRO	Address, RegisterBase, Temp
	move.l	\1,\3
	rol.l	#8,\3
	move.b	#GDC_BLTSrcStartHigh,\3
	rol.l	#8,\3
	and.b	#$3f,\3
	move.w	\3,GDCI(\2)
	move.b	#GDC_BLTSrcStartMiddle,\3
	rol.l	#8,\3
	move.w	\3,GDCI(\2)
	move.b	#GDC_BLTSrcStartLow,\3
	rol.l	#8,\3
	move.w	\3,GDCI(\2)
	ENDM

SetBLTDstStart	MACRO	Address, RegisterBase
	move.b	#GDC_BLTDstStartLow,GDCI(\2)
	move.b	\1,GDCD(\2)
	lsr.w	#8,\1
	move.b	#GDC_BLTDstStartMiddle,GDCI(\2)
	move.b	\1,GDCD(\2)
	swap	\1
	and.b	#$3f,\1
	move.b	#GDC_BLTDstStartHigh,GDCI(\2)
	move.b	\1,GDCD(\2)
	ENDM

SetBLTDstStartW	MACRO	Address, RegisterBase, Temp
	move.l	\1,\3
	rol.l	#8,\3
	move.b	#GDC_BLTDstStartHigh,\3
	rol.l	#8,\3
	and.b	#$3f,\3
	move.w	\3,GDCI(\2)
	move.b	#GDC_BLTDstStartMiddle,\3
	rol.l	#8,\3
	move.w	\3,GDCI(\2)
	move.b	#GDC_BLTDstStartLow,\3
	rol.l	#8,\3
	move.w	\3,GDCI(\2)
	ENDM

SetBLTSrcPitch	MACRO	Pitch, RegisterBase
	move.b	#GDC_BLTSrcPitchLow,GDCI(\2)
	move.b	\1,GDCD(\2)
	lsr.w	#8,\1
	and.b	#$1f,\1
	move.b	#GDC_BLTSrcPitchHigh,GDCI(\2)
	move.b	\1,GDCD(\2)
	ENDM

SetBLTSrcPitchW	MACRO	Pitch, RegisterBase, Temp
	move.w	\1,\3
	swap	\3
	move.b	#GDC_BLTSrcPitchHigh,\3
	rol.l	#8,\3
	and.b	#$1f,\3
	move.w	\3,GDCI(\2)
	move.b	#GDC_BLTSrcPitchLow,\3
	rol.l	#8,\3
	move.w	\3,GDCI(\2)
	ENDM

SetBLTDstPitch	MACRO	Pitch, RegisterBase
	move.b	#GDC_BLTDstPitchLow,GDCI(\2)
	move.b	\1,GDCD(\2)
	lsr.w	#8,\1
	and.b	#$1f,\1
	move.b	#GDC_BLTDstPitchHigh,GDCI(\2)
	move.b	\1,GDCD(\2)
	ENDM

SetBLTDstPitchW	MACRO	Pitch, RegisterBase, Temp
	move.w	\1,\3
	swap	\3
	move.b	#GDC_BLTDstPitchHigh,\3
	rol.l	#8,\3
	and.b	#$1f,\3
	move.w	\3,GDCI(\2)
	move.b	#GDC_BLTDstPitchLow,\3
	rol.l	#8,\3
	move.w	\3,GDCI(\2)
	ENDM

SetBLTWidth	MACRO	Width, RegisterBase
	subq.w	#1,\1
	move.b	#GDC_BLTWidthLow,GDCI(\2)
	move.b	\1,GDCD(\2)
	lsr.w	#8,\1
	and.b	#$1f,\1
	move.b	#GDC_BLTWidthHigh,GDCI(\2)
	move.b	\1,GDCD(\2)
	ENDM

SetBLTWidthW	MACRO	Width, RegisterBase, Temp
	move.w	\1,\3
	subq.w	#1,\3
	swap	\3
	move.b	#GDC_BLTWidthHigh,\3
	rol.l	#8,\3
	and.b	#$1f,\3
	move.w	\3,GDCI(\2)
	move.b	#GDC_BLTWidthLow,\3
	rol.l	#8,\3
	move.w	\3,GDCI(\2)
	ENDM

SetBLTHeight	MACRO	Height, RegisterBase
	subq.w	#1,\1
	move.b	#GDC_BLTHeightLow,GDCI(\2)
	move.b	\1,GDCD(\2)
	lsr.w	#8,\1
	and.b	#$03,\1
	move.b	#GDC_BLTHeightHigh,GDCI(\2)
	move.b	\1,GDCD(\2)
	ENDM

SetBLTHeightW	MACRO	Height, RegisterBase, Temp
	move.w	\1,\3
	subq.w	#1,\3
	swap	\3
	move.b	#GDC_BLTHeightHigh,\3
	rol.l	#8,\3
	and.b	#$03,\3
	move.w	\3,GDCI(\2)
	move.b	#GDC_BLTHeightLow,\3
	rol.l	#8,\3
	move.w	\3,GDCI(\2)
	ENDM

SetBLTROP	MACRO	ROP, RegisterBase
	move.w	#(GDC_BLTRasterOperation<<8)|\1,GDCI(\2)
	ENDM

SetBLTROPW	MACRO	ROP, RegisterBase, Temp
	move.w	#(GDC_BLTRasterOperation<<8),\3
	move.b	\1,\3
	move.w	\3,GDCI(\2)
	ENDM

SetBLTMode	MACRO	Mode, RegisterBase
	move.w	#(GDC_BLTMode<<8)|\1,GDCI(\2)
	ENDM

SetBLTModeW	MACRO	Mode, RegisterBase, Temp
	move.w	#(GDC_BLTMode<<8),\3
	move.b	\1,\3
	move.w	\3,GDCI(\2)
	ENDM

StartBLT	MACRO	RegisterBase
	nop
	move.w	#(GDC_BLTStartStatus<<8)|$0b,GDCI(\1)
	nop
	ENDM

WaitScreenBLT	MACRO	RegisterBase, Temp
	move.b	#GDC_BLTStartStatus,GDCI(\1)
	nop
	move.l	#2500000,\2		; Long timeout for screen-to-screen
.wait\@:
	btst	#3,GDCD(\1)
	beq	.done\@
	subq.l	#1,\2
	bgt	.wait\@
	move.b	#(1<<2),GDCD(\1)	; Reset stalled blitter
.done\@:
	ENDM

WaitSystemBLT	MACRO	RegisterBase, Temp
	move.b	#GDC_BLTStartStatus,GDCI(\1)
	nop
	move.w	#10000,\2		; Short timeout for system-to-screen
.wait\@:
	btst	#3,GDCD(\1)
	beq	.done\@
	subq.w	#1,\2
	bgt	.wait\@
	move.b	#(1<<2),GDCD(\1)	; Reset stalled blitter
.done\@:
	ENDM


BM_STANDARD	EQU	%00000000
BM_EXPANSION	EQU	%10000000
BM_PATTERN	EQU	%01000000
BM_BPP8		EQU	%00000000
BM_BPP16	EQU	%00010000
BM_BPP32	EQU	%00110000
BM_TRANSPARENCY	EQU	%00001000
BM_SYSTEM	EQU	%00000100
BM_REVERSE	EQU	%00000001

ROP_FALSE	EQU	$00
ROP_NOR		EQU	$90
ROP_ONLYDST	EQU	$50
ROP_NOTSRC	EQU	$d0
ROP_ONLYSRC	EQU	$09
ROP_NOTDST	EQU	$0b
ROP_EOR		EQU	$59
ROP_NAND	EQU	$da
ROP_AND		EQU	$05
ROP_NEOR	EQU	$95
ROP_DST		EQU	$06
ROP_NOTONLYSRC	EQU	$d6
ROP_SRC		EQU	$0d
ROP_NOTONLYDST	EQU	$ad
ROP_OR		EQU	$6d
ROP_TRUE	EQU	$0e

***********************************************************
ATCI	EQU	$3C0	;
ATCR	EQU	$3C1	;
ATCD	EQU	ATCI
***********************************************************
	ENUM
	EITEM	ATC_PaletteRAM0
	EITEM	ATC_PaletteRAM1
	EITEM	ATC_PaletteRAM2
	EITEM	ATC_PaletteRAM3
	EITEM	ATC_PaletteRAM4
	EITEM	ATC_PaletteRAM5
	EITEM	ATC_PaletteRAM6
	EITEM	ATC_PaletteRAM7
	EITEM	ATC_PaletteRAM8
	EITEM	ATC_PaletteRAM9
	EITEM	ATC_PaletteRAMa
	EITEM	ATC_PaletteRAMb
	EITEM	ATC_PaletteRAMc
	EITEM	ATC_PaletteRAMd
	EITEM	ATC_PaletteRAMe
	EITEM	ATC_PaletteRAMf
	EITEM	ATC_ModeControl
	EITEM	ATC_OverscanColor
	EITEM	ATC_ColorPlaneEnable
	EITEM	ATC_HorizontalPixelPanning
	EITEM	ATC_ColorSelect

	BITDEF	ATC,PaletteRAMAddressSource,5

ResetATC	MACRO
		tst.b	INPUTSTATUS1(a0)	; set ATC to indexed mode
		nop
		ENDM

SetATC	MACRO
	move.b	#(\2|ATCF_PaletteRAMAddressSource),ATCI(a0)
	move.b	\1,ATCD(a0)
	ENDM

***********************************************************
CRTCI	EQU	$3D4	;
CRTCD	EQU	$3D5	;
***********************************************************
	ENUM
	EITEM	CRTC_HorizontalTotal
	EITEM	CRTC_HorizontalDisplayEnd
	EITEM	CRTC_HorizontalBlankStart
	EITEM	CRTC_HorizontalBlankEnd
	EITEM	CRTC_HorizontalSyncStart
	EITEM	CRTC_HorizontalSyncEnd
	EITEM	CRTC_VerticalTotal
	EITEM	CRTC_OverflowLow
	EITEM	CRTC_PresetRowScan
	EITEM	CRTC_MaximumRowAddress
	EITEM	CRTC_CursorStartRowAddress
	EITEM	CRTC_CursorEndRowAddress
	EITEM	CRTC_LinearStartingAddressMiddle
	EITEM	CRTC_LinearStartingAddressLow
	EITEM	CRTC_CursorAddressMiddle
	EITEM	CRTC_CursorAddressLow
	EITEM	CRTC_VerticalSyncStart
	EITEM	CRTC_VerticalSyncEnd
	EITEM	CRTC_VerticalDisplayEnd
	EITEM	CRTC_RowOffset
	EITEM	CRTC_UnderlineRowAddress
	EITEM	CRTC_VerticalBlankStart
	EITEM	CRTC_VerticalBlankEnd
	EITEM	CRTC_CRTCMode
	EITEM	CRTC_LineCompare
	EITEM	CRTC_InterlaceEnd
	EITEM	CRTC_MiscellaneousControl
	EITEM	CRTC_ExtendedDisplayControls
	EITEM	CRTC_SyncAdjustGenlock
	EITEM	CRTC_OverlayExtendedControl
	ENUM	$22
	EITEM	CRTC_DataLatchesReadback
	EITEM	CRTC_Dummy1
	EITEM	CRTC_ATCToggleReadback
	EITEM	CRTC_PartStatus
	EITEM	CRTC_ATCIndexReadback
	EITEM	CRTC_ID

SetCRTC	MACRO	Value, Index
	move.b	#\2,CRTCI(a0)
	move.b	\1,CRTCD(a0)
	ENDM

SetCRTCc	MACRO	Value, Index
	move.w	\1|(\2<<8),CRTCI(a0)
	ENDM

SetCRTCw	MACRO	Value, Index, Temp
	move.w	#(\2<<8),\3
	move.b	\1,\3
	move.w	\3,CRTCI(a0)
	ENDM
