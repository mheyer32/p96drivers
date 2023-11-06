*  Picasso96.i -- include File
*  (C) Copyright 1996 Alexander Kneer & Tobias Abt
*  All Rights Reserved.
*
************************************************************************
	IFND	LIBRARIES_PICASSO96_I
LIBRARIES_PICASSO96_I	SET	1

************************************************************************
* includes
*
	IFND EXEC_TYPES_I
	INCLUDE "exec/types.i"
	ENDC

	IFND EXEC_NODES_I
	INCLUDE "exec/nodes.i"
	ENDC

	IFND UTILITY_TAGITEM_I
	INCLUDE "utility/tagitem.i"
	ENDC

************************************************************************
* This is the name of the library
*
P96NAME	MACRO
	dc.b "Picasso96API.library",0
	ENDM

************************************************************************
* Types for RGBFormat used
*
	ENUM	0
	EITEM	RGBFB_NONE	; no valid RGB format (should not happen)
	EITEM	RGBFB_CLUT	; palette mode, set colors with LeadRGB32, etc.
	EITEM	RGBFB_R8G8B8	; TrueColor RGB (8 bit each)
	EITEM	RGBFB_B8G8R8	; TrueColor BGR (8 bit each)
	EITEM	RGBFB_R5G6B5PC	; HiColor16 (5 bit R, 6 bit G, 5 bit B), format: gggbbbbbrrrrrggg
	EITEM	RGBFB_R5G5B5PC	; HiColor15 (5 bit each), format: gggbbbbb0rrrrrgg
	EITEM	RGBFB_A8R8G8B8	; 4 Byte TrueColor ARGB (A unused alpha channel)
	EITEM	RGBFB_A8B8G8R8	; 4 Byte TrueColor ABGR (A unused alpha channel)
	EITEM	RGBFB_R8G8B8A8	; 4 Byte TrueColor RGBA (A unused alpha channel)
	EITEM	RGBFB_B8G8R8A8	; 4 Byte TrueColor BGRA (A unused alpha channel)
	EITEM	RGBFB_R5G6B5	; HiColor16 (5 bit R, 6 bit G, 5 bit B), format: rrrrrggggggbbbbb
	EITEM	RGBFB_R5G5B5	; HiColor15 (5 bit each), format: 0rrrrrgggggbbbbb
	EITEM	RGBFB_B5G6R5PC	; HiColor16 (5 bit R, 6 bit G, 5 bit B), format: gggrrrrrbbbbbggg
	EITEM	RGBFB_B5G5R5PC	; HiColor15 (5 bit each), format: gggrrrrr0bbbbbbgg

	; By now, the following formats are for use with a hardware window only
	; (bitmap operations may be implemented incompletely)

	EITEM	RGBFB_YUV422CGX	; 2 Byte TrueColor YUV (CCIR recommendation CCIR601).
				; Each two-pixel unit is stored as one longword
				; containing luminance (Y) for each of the two pixels,
				; and chrominance (U,V) for alternate pixels.
				; The missing chrominance values are generated by
				; interpolation. (Y0-V0-Y1-U0)
	EITEM	RGBFB_YUV411	; 1 Byte TrueColor ACCUPAK. Four adjacent pixels form
				; a packet of 5 bits Y (luminance) each pixel and 6 bits
				; U and V (chrominance) shared by the four pixels
	EITEM	RGBFB_YUV411PC	; 1 Byte TrueColor ACCUPAK byte swapped (1234 -> 4321)
	EITEM	RGBFB_YUV422	; 2 Byte TrueColor YUV (CCIR recommendation CCIR601).
				; Each two-pixel unit is stored as one longword
				; containing luminance (Y) for each of the two pixels,
				; and chrominance (U,V) for alternate pixels.
				; The missing chrominance values are generated by
				; interpolation. (Y1-U0-Y0-V0)
	EITEM	RGBFB_YUV422PC	; 2 Byte TrueColor CCIR601 byte swapped (V0-Y0-U0-Y1)
	EITEM	RGBFB_YUV422PA	; 2 Byte TrueColor CCIR601 for use with YUV12 planar
				; assist mode on Cirrus Logic base graphics chips.
				; (Y0-Y1-V0-U0)
	EITEM	RGBFB_YUV422PAPC; 2 Byte TrueColor YUV12 byte swapped (U0-V0-Y1-Y0)

	EITEM	RGBFB_MaxFormats

; legacy
RGBFB_Y4U2V2		EQU	RGBFB_YUV422CGX
RGBFB_Y4U1V1		EQU	RGBFB_YUV411

RGBFF_NONE		EQU	(1<<RGBFB_NONE)
RGBFF_CLUT		EQU	(1<<RGBFB_CLUT)
RGBFF_R8G8B8		EQU	(1<<RGBFB_R8G8B8)
RGBFF_B8G8R8		EQU	(1<<RGBFB_B8G8R8)
RGBFF_R5G6B5PC		EQU	(1<<RGBFB_R5G6B5PC)
RGBFF_R5G5B5PC		EQU	(1<<RGBFB_R5G5B5PC)
RGBFF_A8R8G8B8		EQU	(1<<RGBFB_A8R8G8B8)
RGBFF_A8B8G8R8		EQU	(1<<RGBFB_A8B8G8R8)
RGBFF_R8G8B8A8		EQU	(1<<RGBFB_R8G8B8A8)
RGBFF_B8G8R8A8		EQU	(1<<RGBFB_B8G8R8A8)
RGBFF_R5G6B5		EQU	(1<<RGBFB_R5G6B5)
RGBFF_R5G5B5		EQU	(1<<RGBFB_R5G5B5)
RGBFF_B5G6R5PC		EQU	(1<<RGBFB_B5G6R5PC)
RGBFF_B5G5R5PC		EQU	(1<<RGBFB_B5G5R5PC)
RGBFF_YUV422CGX		EQU	(1<<RGBFB_YUV422CGX)
RGBFF_YUV411		EQU	(1<<RGBFB_YUV411)
RGBFF_YUV411PC		EQU	(1<<RGBFB_YUV411PC)
RGBFF_YUV422		EQU	(1<<RGBFB_YUV422)
RGBFF_YUV422PC		EQU	(1<<RGBFB_YUV422PC)
RGBFF_YUV422PA		EQU	(1<<RGBFB_YUV422PA)
RGBFF_YUV422PAPC	EQU	(1<<RGBFB_YUV422PAPC)

; legacy
RGBFF_Y4U2V2		EQU	RGBFF_YUV422CGX
RGBFF_Y4U1V1		EQU	RGBFF_YUV411

RGBFF_HICOLOR		EQU	(RGBFF_R5G6B5PC|RGBFF_R5G5B5PC|RGBFF_R5G6B5|RGBFF_R5G5B5|RGBFF_B5G6R5PC|RGBFF_B5G5R5PC)
RGBFF_TRUECOLOR		EQU	(RGBFF_R8G8B8|RGBFF_B8G8R8)
RGBFF_TRUEALPHA		EQU	(RGBFF_A8R8G8B8|RGBFF_A8B8G8R8|RGBFF_R8G8B8A8|RGBFF_B8G8R8A8)

************************************************************************
* Flags for p96AllocBitMap
*
BMF_USERPRIVATE		EQU	$8000

************************************************************************
* Attributes for p96GetBitMapAttr
*
	ENUM	0
	EITEM	P96BMA_WIDTH
	EITEM	P96BMA_HEIGHT
	EITEM	P96BMA_DEPTH
	EITEM	P96BMA_MEMORY
	EITEM	P96BMA_BYTESPERROW
	EITEM	P96BMA_BYTESPERPIXEL
	EITEM	P96BMA_BITSPERPIXEL
	EITEM	P96BMA_RGBFORMAT
	EITEM	P96BMA_ISP96
	EITEM	P96BMA_ISONBOARD
	EITEM	P96BMA_BOARDMEMBASE
	EITEM	P96BMA_BOARDIOBASE
	EITEM	P96BMA_BOARDMEMIOBASE

************************************************************************
* Attributes for p96GetModeIDAttr
*
	ENUM	0
	EITEM	P96IDA_WIDTH
	EITEM	P96IDA_HEIGHT
	EITEM	P96IDA_DEPTH
	EITEM	P96IDA_BYTESPERPIXEL
	EITEM	P96IDA_BITSPERPIXEL
	EITEM	P96IDA_RGBFORMAT
	EITEM	P96IDA_ISP96
	EITEM	P96IDA_BOARDNUMBER
	EITEM	P96IDA_STDBYTESPERROW
	EITEM	P96IDA_BOARDNAME
	EITEM	P96IDA_COMPATIBLEFORMATS
	EITEM	P96IDA_VIDEOCOMPATIBLE
	EITEM	P96IDA_PABLOIVCOMPATIBLE
	EITEM	P96IDA_PALOMAIVCOMPATIBLE

************************************************************************
* Tags for p96BestModeIDTagList
*
P96BIDTAG_Dummy			EQU	(TAG_USER+96)

P96BIDTAG_FormatsAllowed	EQU	(P96BIDTAG_Dummy+$0001)
P96BIDTAG_FormatsForbidden	EQU	(P96BIDTAG_Dummy+$0002)
P96BIDTAG_NominalWidth		EQU	(P96BIDTAG_Dummy+$0003)
P96BIDTAG_NominalHeight		EQU	(P96BIDTAG_Dummy+$0004)
P96BIDTAG_Depth			EQU	(P96BIDTAG_Dummy+$0005)
P96BIDTAG_VideoCompatible	EQU	(P96BIDTAG_Dummy+$0006)
P96BIDTAG_PabloIVCompatible	EQU	(P96BIDTAG_Dummy+$0007)
P96BIDTAG_PalomaIVCompatible	EQU	(P96BIDTAG_Dummy+$0008)

************************************************************************
* Tags for p96RequestModeIDTagList
*
P96MA_Dummy			EQU	(TAG_USER+$10000+96)

P96MA_MinWidth			EQU	(P96MA_Dummy+$0001)
P96MA_MinHeight			EQU	(P96MA_Dummy+$0002)
P96MA_MinDepth			EQU	(P96MA_Dummy+$0003)
P96MA_MaxWidth			EQU	(P96MA_Dummy+$0004)
P96MA_MaxHeight			EQU	(P96MA_Dummy+$0005)
P96MA_MaxDepth			EQU	(P96MA_Dummy+$0006)
P96MA_DisplayID			EQU	(P96MA_Dummy+$0007)
P96MA_FormatsAllowed		EQU	(P96MA_Dummy+$0008)
P96MA_FormatsForbidden		EQU	(P96MA_Dummy+$0009)
P96MA_WindowTitle		EQU	(P96MA_Dummy+$000a)
P96MA_OKText			EQU	(P96MA_Dummy+$000b)
P96MA_CancelText		EQU	(P96MA_Dummy+$000c)
P96MA_Window			EQU	(P96MA_Dummy+$000d)
P96MA_PubScreenName		EQU	(P96MA_Dummy+$000e)
P96MA_Screen			EQU	(P96MA_Dummy+$000f)
P96MA_VideoCompatible		EQU	(P96MA_Dummy+$0010)
P96MA_PabloIVCompatible		EQU	(P96MA_Dummy+$0011)
P96MA_PalomaIVCompatible	EQU	(P96MA_Dummy+$0012)

************************************************************************
* Tags for p96OpenScreenTagList
*
P96SA_Dummy			EQU	(TAG_USER+$20000+96)

P96SA_Left			EQU	(P96SA_Dummy+$0001)
P96SA_Top			EQU	(P96SA_Dummy+$0002)
P96SA_Width			EQU	(P96SA_Dummy+$0003)
P96SA_Height			EQU	(P96SA_Dummy+$0004)
P96SA_Depth			EQU	(P96SA_Dummy+$0005)
P96SA_DetailPen			EQU	(P96SA_Dummy+$0006)
P96SA_BlockPen			EQU	(P96SA_Dummy+$0007)
P96SA_Title			EQU	(P96SA_Dummy+$0008)
P96SA_Colors			EQU	(P96SA_Dummy+$0009)
P96SA_ErrorCode			EQU	(P96SA_Dummy+$000a)
P96SA_Font			EQU	(P96SA_Dummy+$000b)
P96SA_SysFont			EQU	(P96SA_Dummy+$000c)
P96SA_Type			EQU	(P96SA_Dummy+$000d)
P96SA_BitMap			EQU	(P96SA_Dummy+$000e)
P96SA_PubName			EQU	(P96SA_Dummy+$000f)
P96SA_PubSig			EQU	(P96SA_Dummy+$0010)
P96SA_PubTask			EQU	(P96SA_Dummy+$0011)
P96SA_DisplayID			EQU	(P96SA_Dummy+$0012)
P96SA_DClip			EQU	(P96SA_Dummy+$0013)
P96SA_ShowTitle			EQU	(P96SA_Dummy+$0014)
P96SA_Behind			EQU	(P96SA_Dummy+$0015)
P96SA_Quiet			EQU	(P96SA_Dummy+$0016)
P96SA_AutoScroll		EQU	(P96SA_Dummy+$0017)
P96SA_Pens			EQU	(P96SA_Dummy+$0018)
P96SA_SharePens			EQU	(P96SA_Dummy+$0019)
P96SA_BackFill			EQU	(P96SA_Dummy+$001a)
P96SA_Colors32			EQU	(P96SA_Dummy+$001b)
P96SA_VideoControl		EQU	(P96SA_Dummy+$001c)
P96SA_RGBFormat			EQU	(P96SA_Dummy+$001d)
P96SA_NoSprite			EQU	(P96SA_Dummy+$001e)
P96SA_NoMemory			EQU	(P96SA_Dummy+$001f)
P96SA_RenderFunc		EQU	(P96SA_Dummy+$0020)
P96SA_SaveFunc			EQU	(P96SA_Dummy+$0021)
P96SA_UserData			EQU	(P96SA_Dummy+$0022)
P96SA_Alignment			EQU	(P96SA_Dummy+$0023)
P96SA_FixedScreen		EQU	(P96SA_Dummy+$0024)
P96SA_Exclusive			EQU	(P96SA_Dummy+$0025)
P96SA_ConstantBytesPerRow	EQU	(P96SA_Dummy+$0026)
P96SA_ConstantByteSwapping	EQU	(P96SA_Dummy+$0027)

************************************************************************

MODENAMELENGTH	EQU	48

 STRUCTURE P96Mode,LN_SIZE
	STRUCT	p96m_Description,MODENAMELENGTH
	UWORD	p96m_Width
	UWORD	p96m_Height
	UWORD	p96m_Depth
	ULONG	p96m_DisplayID
	LABEL	p96m_SIZEOF

************************************************************************

 STRUCTURE RenderInfo,0
	APTR	gri_Memory
	WORD	gri_BytesPerRow
	UWORD	gri_pad
	ULONG	gri_RGBFormat
	LABEL	gri_SIZEOF

************************************************************************

 STRUCTURE TrueColorInfo,0
 	ULONG	tci_PixelDistance
 	ULONG	tci_BytesPerRow
	APTR	tci_RedData
	APTR    tci_GreenData
	APTR    tci_BlueData
	LABEL	tci_SIZEOF

************************************************************************
* Tags for p96OpenScreenTagList
*
P96PIP_Dummy			EQU	(TAG_USER+$30000+96)

P96PIP_SourceFormat		EQU	(P96PIP_Dummy+1)	; RGBFTYPE (I) 
P96PIP_SourceBitMap		EQU	(P96PIP_Dummy+2)	; struct BitMap * (G) 
P96PIP_SourceRPort		EQU	(P96PIP_Dummy+3)	; struct RastPort * (G) 
P96PIP_SourceWidth		EQU	(P96PIP_Dummy+4)	; ULONG (I) 
P96PIP_SourceHeight		EQU	(P96PIP_Dummy+5)	; ULONG (I) 
P96PIP_Type			EQU	(P96PIP_Dummy+6)	; ULONG (I) default: PIPT_MemoryWindow 
P96PIP_ErrorCode		EQU	(P96PIP_Dummy+7)	; LONG* (I) 
P96PIP_Brightness		EQU	(P96PIP_Dummy+8)	; ULONG (IGS) default: 0 
P96PIP_Left			EQU	(P96PIP_Dummy+9)	; ULONG (IS) default: 0 
P96PIP_Top			EQU	(P96PIP_Dummy+10)	; ULONG (IS) default: 0 
P96PIP_Width			EQU	(P96PIP_Dummy+11)	; ULONG (IS) default: inner width of window 
P96PIP_Height			EQU	(P96PIP_Dummy+12)	; ULONG (IS) default: inner height of window 
P96PIP_Relativity		EQU	(P96PIP_Dummy+13)	; ULONG (IS) default: PIPRel_Width|PIPRel_Height 
P96PIP_Colors			EQU	(P96PIP_Dummy+14)	; struct ColorSpec * (IS)
								; ti_Data is an array of struct ColorSpec,
								; terminated by ColorIndex = -1.  Specifies
								; initial screen palette colors.
								; Also see P96PIP_Colors32.
								; This only works with CLUT PIPs on non-CLUT
								; screens. For CLUT PIPs on CLUT screens the
								; PIP colors share the screen palette.
P96PIP_Colors32			EQU	(P96PIP_Dummy+15)	; ULONG* (IS)
								; Tag to set the palette colors at 32 bits-per-gun.
								; ti_Data is a pointer * to a table to be passed to
								; the graphics.library/LoadRGB32() function.
								; This format supports both runs of color
								; registers and sparse registers.  See the
								; autodoc for that function for full details.
								; Any color set here has precedence over
								; the same register set by P96PIP_Colors.
								; This only works with CLUT PIPs on non-CLUT
								; screens. For CLUT PIPs on CLUT screens the
								; PIP colors share the screen palette.
P96PIP_NoMemory			EQU	(P96PIP_Dummy+16)
P96PIP_RenderFunc		EQU	(P96PIP_Dummy+17)
P96PIP_SaveFunc			EQU	(P96PIP_Dummy+18)
P96PIP_UserData			EQU	(P96PIP_Dummy+19)
P96PIP_Alignment		EQU	(P96PIP_Dummy+20)
P96PIP_ConstantBytesPerRow	EQU	(P96PIP_Dummy+21)
P96PIP_AllowCropping		EQU	(P96PIP_Dummy+22)
P96PIP_InitialIntScaling	EQU	(P96PIP_Dummy+23)
P96PIP_ClipLeft			EQU	(P96PIP_Dummy+24)
P96PIP_ClipTop			EQU	(P96PIP_Dummy+25)
P96PIP_ClipWidth		EQU	(P96PIP_Dummy+26)
P96PIP_ClipHeight		EQU	(P96PIP_Dummy+27)
P96PIP_ConstantByteSwapping	EQU	(P96PIP_Dummy+28)

	ENUM	0
	EITEM	PIPT_MemoryWindow	; default
	EITEM	PIPT_VideoWindow
	EITEM	PIPT_NUMTYPES

P96PIPT_MemoryWindow	EQU	PIPT_MemoryWindow
P96PIPT_VideoWindow	EQU	PIPT_VideoWindow

PIPRel_Right		EQU	1	; P96PIP_Left is relative to the right side (negative value)
PIPRel_Bottom		EQU	2	; P96PIP_Top is relative to the bottom (negative value)
PIPRel_Width		EQU	4	; width of PIP changes relative to window sizing
PIPRel_Height		EQU	8	; height of PIP changes relative to window sizing

PIPERR_NOMEMORY		EQU (1)	; couldn't get normal memory
PIPERR_ATTACHFAIL	EQU (2)	; Failed to attach to a screen
PIPERR_NOTAVAILABLE	EQU (3) ; PIP not available for other reason
PIPERR_OUTOFPENS	EQU (4) ; couldn't get a free pen for occlusion
PIPERR_BADDIMENSIONS	EQU (5)	; type, width, height or format invalid
PIPERR_NOWINDOW		EQU (6)	; couldn't open window
PIPERR_BADALIGNMENT	EQU (7)	; specified alignment is not ok
PIPERR_CROPPED		EQU (8)	; pip would be cropped, but isn't allowed to

************************************************************************
* Tags for P96GetRTGDataTagList

P96RD_Dummy			EQU	(TAG_USER+$40000+96)
P96RD_NumberOfBoards		EQU	(P96RD_Dummy+1)

************************************************************************
* Tags for P96GetBoardDataTagList

P96BD_Dummy			EQU	(TAG_USER+$50000+96)
P96BD_BoardName			EQU	(P96BD_Dummy+1)
P96BD_ChipName			EQU	(P96BD_Dummy+2)
P96BD_TotalMemory		EQU	(P96BD_Dummy+4)
P96BD_FreeMemory		EQU	(P96BD_Dummy+5)
P96BD_LargestFreeMemory		EQU	(P96BD_Dummy+6)
P96BD_MonitorSwitch		EQU	(P96BD_Dummy+7)
P96BD_RGBFormats		EQU	(P96BD_Dummy+8)
P96BD_MemoryClock		EQU	(P96BD_Dummy+9)

************************************************************************
	ENDC
************************************************************************
