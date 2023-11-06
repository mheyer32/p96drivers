************************************************************************
*  sublibrary to picasso96
************************************************************************

	MACHINE	68020

	NOLIST

	incdir	include:
	include	lvo/exec.i
	include	lvo/expansion.i
	include	exec/libraries.i
	include	exec/resident.i
	include	exec/initializers.i
	include	exec/alerts.i
	include	exec/ables.i
	include	libraries/configvars.i
	include	hardware/intbits.i

	incdir	/privateinclude
	include	boardinfo.i
	include	settings.i

	include	macros.i
	include	/CirrusGD5434/CirrusGD5434.i

	include	PiccoloSD64.card_rev.i

	LIST

NAME		MACRO
		dc.b	'PiccoloSD64'
		ENDM

;-----------------------------------------------

Start:						; just in case we are executed
	moveq	#-1,d0
	rts

;-----------------------------------------------

RomTagPri	EQU	0

RomTag:
	dc.w	RTC_MATCHWORD
	dc.l	RomTag
	dc.l	EndCode
	dc.b	RTF_AUTOINIT
	dc.b	VERSION
	dc.b	NT_LIBRARY
	dc.b	RomTagPri
	dc.l	LibName
	dc.l	IDString
	dc.l	InitTable

;-----------------------------------------------

CardName:		NAME
		dc.b	0
LibName:		NAME
		dc.b	'.card',0
IDString:		VSTRING
VERString:		VERSTAG
ExpansionName:	dc.b	'expansion.library',0
ChipName:	dc.b	'picasso96/'
		CIRRUS34NAME
		dc.b	'.chip',0
		even

;-----------------------------------------------

InitTable:
	dc.l	card_SIZEOF			; size of library base data space
	dc.l	funcTable			; pointer to function initializers
	dc.l	dataTable			; pointer to data initializers
	dc.l	initRoutine			; routine to run

;-----------------------------------------------

funcTable:
	dc.l	Open
	dc.l	Close
	dc.l	Expunge
	dc.l	Null

	dc.l	FindCard
	dc.l	InitCard

	dc.l	-1

;-----------------------------------------------

dataTable:
	INITBYTE	LN_TYPE,NT_LIBRARY
	INITBYTE	LN_PRI,-50
	INITLONG	LN_NAME,LibName
	INITBYTE	LIB_FLAGS,LIBF_SUMUSED!LIBF_CHANGED
	INITWORD	LIB_VERSION,VERSION
	INITWORD	LIB_REVISION,REVISION
	INITLONG	LIB_IDSTRING,IDString
	INITLONG	card_Name,CardName
	dc.l		0

;-----------------------------------------------

initRoutine:					; d0.l: library base, a0: segment list
	PUSH	a5

	movea.l	d0,a5
	move.l	a6,(card_ExecBase,a5)
	move.l	a0,(card_SegmentList,a5)

	lea	(ExpansionName,pc),a1
	moveq	#0,d0
	CALL	OpenLibrary
	move.l	d0,(card_ExpansionBase,a5)
	bne	.got_expansion

	ALERT	AG_OpenLib!AO_ExpansionLib

.got_expansion:
	move.l	a5,d0
	POP	a5
	rts


************************************************************************
Open:
* a6:	library
* d0:	version
************************************************************************

	addq.w	#1,(LIB_OPENCNT,a6)		; mark us as having another opener
	bclr	#LIBB_DELEXP,(card_Flags,a6)	; prevent delayed expunges
	move.l	a6,d0
	rts


************************************************************************
Close:
* a6:	library
************************************************************************

	moveq	#0,d0				; set the return value
	subq.w	#1,(LIB_OPENCNT,a6)		; mark us as having one fewer openers
	bne	.end				; see if there is anyone left with us open

	btst	#LIBB_DELEXP,(card_Flags,a6)	; see if we have a delayed expunge pending
	beq	.end
	bsr	Expunge
.end:
	rts


************************************************************************
Expunge:
* a6:	library
************************************************************************

	PUSH	d2/a5/a6
	movea.l	a6,a5
	movea.l	(card_ExecBase,a5),a6

	tst.w	(LIB_OPENCNT,a5)		; see if anyone has us open
	beq	.not_open

	bset	#LIBB_DELEXP,(card_Flags,a5)	; it is still open.  set the delayed expunge flag
	moveq	#0,d0
	bra	.end

.not_open:
	move.l	(card_SegmentList,a5),d2	; go ahead and get rid of us.  Store our seglist in d2

	movea.l	a5,a1				; unlink from library list
	CALL	Remove

	movea.l	(card_ExpansionBase,a5),a1
	CALL	CloseLibrary

	moveq	#0,d0				; free our memory
	movea.l	a5,a1
	move.w	(LIB_NEGSIZE,a5),d0
	suba.l	d0,a1
	add.w	(LIB_POSSIZE,a5),d0
	CALL	FreeMem

	move.l	d2,d0				; set up our return value

.end:
	POP	d2/a5/a6
	rts


************************************************************************
Null:
************************************************************************

	moveq	#0,d0
	rts


MANUFACTURER	EQU	2195


************************************************************************
FindCard:
************************************************************************
* a0:	APTR BoardInfo
************************************************************************

	PUSH	a2-a3/a6

	movea.l	a0,a2
	movea.l	(card_ExpansionBase,a6),a6

	suba.l	a0,a0
.find_memory_loop:
	move.l	#MANUFACTURER,d0
	moveq	#10,d1
	CALL	FindConfigDev
	tst.l	d0
	beq	.no_memory_found
	movea.l	d0,a0
	bclr	#CDB_CONFIGME,(cd_Flags,a0)
	beq	.find_memory_loop
	move.l	(cd_BoardAddr,a0),(gbi_MemoryBase,a2)

	move.l	(cd_BoardSize,a0),d0	; Board size in bytes
	cmp.l	#$01000000,d0		; Always 16MB in Zorro-III
	bne	.got_board_size		; 2 or 4 MB in Zorro-II

	lea	(cd_Rom,a0),a1		; ExpansionRom structure
	move.b	(er_Type,a1),d1		; Type byte
	subq.l	#1,d1			; Substract 1 (modulo 8)
	and.l	#ERT_MEMMASK,d1		; Extract memory size bits
	moveq	#1,d0			; Basis: 1 slot
	lsl.w	d1,d0			; Size in slots
	swap	d0			; Size in bytes

.got_board_size:
	move.l	d0,(gbi_MemorySize,a2)	; Save preliminary(!) size

	suba.l	a0,a0
.find_registers_loop:
	move.l	#MANUFACTURER,d0
	moveq	#11,d1
	CALL	FindConfigDev
	tst.l	d0
	beq	.no_registers_found
	movea.l	d0,a0
	bclr	#CDB_CONFIGME,(cd_Flags,a0)
	beq	.find_registers_loop
	move.l	(cd_BoardAddr,a0),(gbi_RegisterBase,a2)

	moveq	#-1,d0
.no_registers_found:
.no_memory_found:
	POP	a2-a3/a6
	rts


************************************************************************
InitCard:
************************************************************************
* a0:	APTR BoardInfo
************************************************************************

	PUSH	d3-d4/a2-a6
	movea.l	(card_ExecBase,a6),a5

	movea.l	a0,a2

	lea	(ChipName,pc),a1
	moveq	#0,d0
	movea.l	a5,a6
	CALL	OpenLibrary
	move.l	d0,(gbi_ChipBase,a2)
	beq	.end

	lea	(gbi_HardInterrupt,a2),a1
	lea	(InterruptCode,pc),a0
	move.l	a0,(IS_CODE,a1)
	moveq	#INTB_EXTER,d0
	movea.l	a5,a6
	CALL	AddIntServer

	ori.l	#BIF_VBLANKINTERRUPT,(gbi_Flags,a2)

; wakeup
	movea.l	(gbi_RegisterBase,a2),a0
	adda.l	#$8000,a0
	move.b	#$1c,(a0)		; Reset
	move.w	#500,d0
.l1:
	DELAY
	dbra	d0,.l1
	moveq	#$0c,d0
	move.b	d0,(a0)			; Init board control register
	move.b	d0,(gbi_MoniSwitch,a2)	; Save board control register
	move.w	#500,d0
.l2:
	DELAY
	dbra	d0,.l2

	movea.l	(gbi_ChipBase,a2),a6
	movea.l	a2,a0
	CALL	InitChip
	movea.l	a5,a6

	; as on piccolo boards red and blue lines are exchanged
	; we remove the formats supplied by the chip and insert
	; the correct ones instead

	andi.w	#~(RGBFF_B8G8R8|RGBFF_B8G8R8A8|RGBFF_R5G6B5PC|RGBFF_R5G5B5PC),(gbi_RGBFormats,a2)
	ori.w	#RGBFF_R8G8B8|RGBFF_R8G8B8A8|RGBFF_B5G6R5PC|RGBFF_B5G5R5PC,(gbi_RGBFormats,a2)

	lea	(CardName,pc),a1
	move.l	a1,(gbi_BoardName,a2)
	move.l	#BT_PiccoloSD64,(gbi_BoardType,a2)
	lea	(SetSwitch,pc),a1
	move.l	a1,(gbi_SetSwitch,a2)
	lea	(SetColorArray,pc),a1
	move.l	a1,(gbi_SetColorArray,a2)
	lea	(SetSpriteColor,pc),a1
	move.l	a1,(gbi_SetSpriteColor,a2)
	lea	(SetInterrupt,pc),a1
	move.l	a1,(gbi_SetInterrupt,a2)

	move.l	(gbi_MemoryBase,a2),(gbi_MemorySpaceBase,a2)
	ori.l	#BIF_CACHEMODECHANGE,(gbi_Flags,a2)

	movea.l	(gbi_RegisterBase,a2),a0
	move.l	(gbi_MemoryBase,a2),d0
	swap	d0
	and.b	#$10,d0			; (There is no 1MB SD64 anyhow...)
	or.b	#$81,d0
	SetTS	d0,TS_ExtendedSequencerMode

	move.l	(gbi_MemorySize,a2),d0
	cmp.l	#$00200000,d0
	beq	.has2MB			; 2MB-jumper closed
	cmp.l	#$00400000,d0
	beq	.test4MB		; 2MB-jumper open
	bra	.has0MB			; Unexpected memory size

.test4MB:
	movea.l	(gbi_RegisterBase,a2),a0
	move.b	#TS_DRAMControl,(TSI,a0)
	ori.b	#(1<<7),(TSD,a0)	; Enable second bank for testing
	movea.l	(gbi_ExecBase,a2),a6
	movea.l	(gbi_MemoryBase,a2),a3	; First two megabytes
	movea.l	a3,a4
	adda.l	#$00200000,a4		; Second two megabytes
	move.l	#$2bad2bad,d3
	move.l	#$fab4fab4,d4
	move.l	d3,(a3)
	CALL	CacheClearU		; Make sure to write through
	nop				; Flush write buffer
	move.l	d4,(a4)
	CALL	CacheClearU		; Make sure to write through
	nop				; Flush write buffer
	cmp.l	(a3),d4
	beq	.has2MB			; Second bank wraparound
	cmp.l	(a3),d3
	bne	.has0MB			; First bank invalid
	cmp.l	(a4),d4
	bne	.has2MB			; Second bank invalid
	bra	.has4MB

.has2MB:
	movea.l	(gbi_RegisterBase,a2),a0
	move.b	#TS_DRAMControl,(TSI,a0)
	andi.b	#~(1<<7),(TSD,a0)	; Disable second RAM bank
	move.l	#$200000-RESERVEDMEMORY,(gbi_MemorySize,a2)
	move.l	#$200000,(gbi_MemorySpaceSize,a2)
	bra	.done			; 2 MB minus cursors etc.

.has4MB:
	move.l	#$400000-RESERVEDMEMORY,(gbi_MemorySize,a2)
	move.l	#$400000,(gbi_MemorySpaceSize,a2)
	bra	.done			; 4 MB minus cursors etc.

.has0MB:
	moveq	#0,d0			; Houston, we have a problem...
	bra	.end

.done:
* Software sprite blitter acceleration
;	tst.w	(gbi_SoftSpriteFlags,a2)
;	beq	.ok_end
	move.l	(gbi_Flags,a2),d0
	btst	#BIB_BLITTER,d0
	beq	.ok_end
	
	bset	#BIB_HASSPRITEBUFFER,d0
	move.l	d0,(gbi_Flags,a2)
	move.l	(gbi_MemoryBase,a2),d0
	add.l	(gbi_MemorySize,a2),d0
	add.l	#rm_MouseSaveBuffer,d0
	move.l	d0,(gbi_MouseSaveBuffer,a2)
	add.l	#rm_MouseImageBuffer-rm_MouseSaveBuffer,d0
	move.l	d0,(gbi_MouseImageBuffer,a2)

.ok_end:
	moveq	#-1,d0
.end:
	POP	d3-d4/a2-a6
	rts


************************************************************************
SetSwitch:
************************************************************************
* a0:	APTR BoardInfo
* d0.w:	BOOL State
************************************************************************

	move.b	(gbi_MoniSwitch,a0),d1	; Previous state
	and.b	#~(1<<5),d1		; Clear Active bit

	tst.w	d0
	beq	.switch

	or.b	#(1<<5),d1		; Set Active bit

.switch:
	move.b	(gbi_MoniSwitch,a0),d0
	movea.l	(gbi_RegisterBase,a0),a1
	adda.l	#$8000,a1
	move.b	d1,(a1)			; Activate new state
	move.b	d1,(gbi_MoniSwitch,a0)	; Remember new state
	and.w	#(1<<5),d0
	ror.w	#5,d0			; Return old switch state
	rts


************************************************************************
SetColorArray:
************************************************************************
* a0:	APTR BoardInfo
* d0:	WORD StartIndex
* d1:	WORD Count
************************************************************************
* overwrite standard function for clut modes as red and blue
* lines exchanged on piccolo boards
************************************************************************

	PUSH	d2/a6
	movea.l	(gbi_ExecBase,a0),a6

	lea	(gbi_CLUT,a0),a1
	moveq	#8,d2
	sub.w	(gbi_BitsPerCannon,a0),d2
	movea.l	(gbi_RegisterBase,a0),a0

	DISABLE
	move.b	d0,(PALETTEADDRESS,a0)

	adda.w	d0,a1
	adda.w	d0,a1
	adda.w	d0,a1

	bra	.start

.loop:
	move.b	(2,a1),d0		; blue value
	lsr.b	d2,d0
	move.b	d0,(PALETTEDATA,a0)
	nop
	move.b	(1,a1),d0		; green value
	lsr.b	d2,d0
	move.b	d0,(PALETTEDATA,a0)
	nop
	move.b	(0,a1),d0		; red value
	lsr.b	d2,d0
	move.b	d0,(PALETTEDATA,a0)
	nop
	addq.l	#3,a1
.start:
	dbra	d1,.loop

	ENABLE
	POP	d2/a6
	rts


************************************************************************
SetSpriteColor:
************************************************************************
* a0:	APTR BoardInfo
* d0:	BYTE Index
* d1:	BYTE Red
* d2:	BYTE Green
* d3:	BYTE Blue
* d7:	RGBFTYPE RGBFormat
************************************************************************
* overwrite standard function for clut modes as red and blue
* lines exchanged on piccolo boards
************************************************************************

	tst.b	d0
	beq	.color_0
	cmp.b	#2,d0
	bne	.end
.color_1:
	moveq	#15,d0
.color_0:
	movea.l	(gbi_RegisterBase,a0),a0

	lsr.b	#2,d1
	lsr.b	#2,d2
	lsr.b	#2,d3
	move.b	d0,(PALETTEADDRESS,a0)

	move.b	#TS_GraphicsCursorAttributes,(TSI,a0)
	move.b	(TSD,a0),d0
	or.b	#2,d0			; reveal extended DAC colors
	move.b	d0,(TSD,a0)

	DELAY
	move.b	d3,(PALETTEDATA,a0)	; blue value
	nop
	DELAY
	move.b	d2,(PALETTEDATA,a0)	; green value
	nop
	DELAY
	move.b	d1,(PALETTEDATA,a0)	; red value
	nop

	and.b	#~2,d0			; hide extended DAC colors
	move.b	d0,(TSD,a0)

.end:
	rts


************************************************************************
SetInterrupt:
************************************************************************
* a0:	APTR BoardInfo
* d0.w:	BOOL State
************************************************************************

	swap	d0
	move.b	(gbi_MoniSwitch,a0),d1	; Previous card state
	movea.l	(gbi_RegisterBase,a0),a1
	move.b	#CRTC_VerticalSyncEnd,(CRTCI,a1)
	move.b	(CRTCD,a1),d0		; Previous chip state

	and.b	#~(1<<6),d1		; Card interrupt disable
	or.b	#(1<<5),d0
	and.b	#~(1<<4),d0		; Chip interrupt disable

	btst	#16,d0
	beq	.switch

	or.b	#(1<<6),d1		; Card interrupt enable
	and.b	#~(1<<5),d0
	or.b	#(1<<4),d0		; Chip interrupt enable

.switch:
	move.b	d0,(CRTCD,a1)		; Activate new chip state
	adda.l	#$8000,a1
	move.b	d1,(a1)			; Activate new card state
	move.b	d1,(gbi_MoniSwitch,a0)	; Remember new card state
	rts


************************************************************************
InterruptCode:
* a1:	APTR BoardInfo
************************************************************************

	movea.l	(gbi_RegisterBase,a1),a6
	move.b	(INPUTSTATUS0,a6),d0
	and.b	#(1<<7),d0		; Interrupt caused by this chip?
	beq	.no_interrupt_pending	; Foreign interrupt; Z flag set

.interrupt_pending:
	move.b	(CRTCI,a6),d1		; Save CRTC index
	move.b	#CRTC_VerticalSyncEnd,(CRTCI,a6)
	move.b	(CRTCD,a6),d0
	and.b	#~(1<<4),d0		; Clear interrupt
	move.b	d0,(CRTCD,a6)
	or.b	#(1<<4),d0		; Allow next interrupt
	move.b	d0,(CRTCD,a6)
	move.b	d1,(CRTCI,a6)		; Restore CRTC index

	movea.l	(gbi_ExecBase,a1),a6
	lea	(gbi_SoftInterrupt,a1),a1
	CALL	Cause
	moveq	#1,d0			; Clear Z flag

.no_interrupt_pending:
	rts


************************************************************************

EndCode:
   END
