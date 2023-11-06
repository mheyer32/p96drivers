************************************************************************
*  sublibrary to picasso96
************************************************************************

        MACHINE 68020

        NOLIST

        include lvo/exec.i
        include exec/libraries.i
        include exec/resident.i
        include exec/initializers.i
        include exec/alerts.i
        include exec/ables.i
        include graphics/rastport.i

        include boardinfo.i
        include settings.i

        include macros.i
        include CirrusGD5434.i

        include CirrusGD5434.chip_rev.i

        LIST

NAME            MACRO
                CIRRUS34NAME
                ENDM

;-----------------------------------------------

Start:                                          ; just in case we are executed
        moveq   #-1,d0
        rts

;-----------------------------------------------

RomTagPri       EQU     0

RomTag:
        dc.w    RTC_MATCHWORD
        dc.l    RomTag
        dc.l    EndCode
        dc.b    RTF_AUTOINIT
        dc.b    VERSION
        dc.b    NT_LIBRARY
        dc.b    RomTagPri
        dc.l    LibName
        dc.l    IDString
        dc.l    InitTable

;-----------------------------------------------

LibName:                NAME
                dc.b    '.chip',0
IDString:               VSTRING
VERString:              VERSTAG
                even

;-----------------------------------------------

InitTable:
        dc.l    chip_SIZEOF                     ; size of library base data space (LIB_POSSIZE)
        dc.l    funcTable                       ; pointer to function initializers
        dc.l    dataTable                       ; pointer to data initializers
        dc.l    initRoutine                     ; routine to run

;-----------------------------------------------

funcTable:
        dc.l    Open
        dc.l    Close
        dc.l    Expunge
        dc.l    Null

        dc.l    InitGC                          ; the only externaly visible function!

        dc.l    -1

;-----------------------------------------------

dataTable:
        INITBYTE        LN_TYPE,NT_LIBRARY
        INITBYTE        LN_PRI,-60              ; no one looks for it, so put it behind the rest
        INITLONG        LN_NAME,LibName
        INITBYTE        LIB_FLAGS,LIBF_SUMUSED!LIBF_CHANGED
        INITWORD        LIB_VERSION,VERSION
        INITWORD        LIB_REVISION,REVISION
        INITLONG        LIB_IDSTRING,IDString
        dc.l            0

;-----------------------------------------------

initRoutine:                                    ; d0.l: library base, a0: segment list

        move.l  d0,a1
        move.l  a6,chip_ExecBase(a1)
        move.l  a0,chip_SegmentList(a1)
        rts

*****************************************************************
Open:
* a6:   library
* d0:   version
*****************************************************************
        addq.w  #1,LIB_OPENCNT(a6)              ; mark us as having another opener
        bclr    #LIBB_DELEXP,chip_Flags(a6)     ; prevent delayed expunges
        move.l  a6,d0
        rts

*****************************************************************
Close:
* a6:   library
*****************************************************************
        moveq   #0,d0                           ; set the return value
        subq.w  #1,LIB_OPENCNT(a6)              ; mark us as having one fewer openers
        bne     .end                            ; see if there is anyone left with us open

        btst    #LIBB_DELEXP,chip_Flags(a6)     ; see if we have a delayed expunge pending
        beq     .end
        bsr     Expunge
.end:
        rts

*****************************************************************
Expunge:
* a6:   library
*****************************************************************
        PUSH    d2/a5/a6
        move.l  a6,a5
        move.l  chip_ExecBase(a5),a6

        tst.w   LIB_OPENCNT(a5)                 ; see if anyone has us open
        beq     .not_open

        bset    #LIBB_DELEXP,chip_Flags(a5)     ; it is still open.  set the delayed expunge flag
        moveq   #0,d0
        bra     .end

.not_open:
        move.l  chip_SegmentList(a5),d2         ; go ahead and get rid of us.  Store our seglist in d2

        move.l  a5,a1                           ; unlink from library list
        CALL    Remove

        moveq   #0,d0                           ; free our memory
        move.l  a5,a1
        move.w  LIB_NEGSIZE(a5),d0
        sub.l   d0,a1
        add.w   LIB_POSSIZE(a5),d0
        CALL    FreeMem

        move.l  d2,d0                           ; set up our return value

.end:
        POP     d2/a5/a6
        rts

*****************************************************************
Null:
*****************************************************************
        moveq   #0,d0
        rts


*****************************************************************
InitGC:
* a0:   struct BoardInfo
*****************************************************************
        PUSH    a2
        move.l  a0,a2

; first, init type and chip dependent flags
        move.l  #GCT_CirrusGD5434,gbi_GraphicsControllerType(a2)
        move.l  #PCT_CirrusGD5434,gbi_PaletteChipType(a2)
        or.l    #BIF_NOMEMORYMODEMIX|BIF_DBLSCANDBLSPRITEY|BIF_DBLCLOCKHALFSPRITEX|BIF_VGASCREENSPLIT,gbi_Flags(a2)
        or.w    #RGBFF_PLANAR|RGBFF_CHUNKY|RGBFF_B8G8R8A8|RGBFF_B8G8R8|RGBFF_R5G6B5PC|RGBFF_R5G5B5PC,gbi_RGBFormats(a2)
        or.w    #RGBFF_PLANAR|RGBFF_B8G8R8|RGBFF_R8G8B8,gbi_SoftSpriteFlags(a2)

	;; this chip supports at most 1MB planar memory, that is 256K per plane
	move.l #256*1024,gbi_MaxPlanarMemory(a2)
	clr.w gbi_SpriteBank(a2)
	
; now, write pointers to the chip functions into boardinfo
; SetGC uses modeinfo to set display timing
        lea     SetGC(pc),a1
        move.l  a1,gbi_SetGC(a2)                ; mandatory
; SetPanning sets view origin e.g. for overscanned displays
        lea     SetPanning(pc),a1
        move.l  a1,gbi_SetPanning(a2)           ; mandatory
; CalculateBytesPerRow calculates how many bytes a display row for a given mode has
        lea     CalculateBytesPerRow(pc),a1
        move.l  a1,gbi_CalculateBytesPerRow(a2) ; mandatory
; CalculateMemory calculates the physical memory address to a given logical address
        lea     CalculateMemory(pc),a1
        move.l  a1,gbi_CalculateMemory(a2)      ; mandatory
; GetCompatibleFormats returns a mask of compatible RGBFormats to a supplied RGBFormat
        lea     GetCompatibleFormats(pc),a1
        move.l  a1,gbi_GetCompatibleFormats(a2) ; mandatory

; Cirrus has internal DAC, so put this function here
; SetDAC sets DAC mode (CLUT, HiColor15 etc)
        lea     SetDAC(pc),a1
        move.l  a1,gbi_SetDAC(a2)               ; mandatory, may be overwritten by board
; SetColorArray sets colors from boardinfo->CLUT
        lea     SetColorArray(pc),a1
        move.l  a1,gbi_SetColorArray(a2)        ; mandatory
; SetDPMSLevel sets degrees of power management
        lea     SetDPMSLevel(pc),a1
        move.l  a1,gbi_SetDPMSLevel(a0)         ; optional, useful if chip offers it

; misc standard VGA features
        lea     SetDisplay(pc),a1
        move.l  a1,gbi_SetDisplay(a2)           ; mandatory
        lea     SetMemoryMode(pc),a1
        move.l  a1,gbi_SetMemoryMode(a2)        ; mandatory
        lea     SetWriteMask(pc),a1
        move.l  a1,gbi_SetWriteMask(a2)         ; mandatory
        lea     SetReadPlane(pc),a1
        move.l  a1,gbi_SetReadPlane(a2)         ; mandatory
        lea     SetClearMask(pc),a1
        move.l  a1,gbi_SetClearMask(a2)         ; mandatory
        lea     WaitVerticalSync(pc),a1
        move.l  a1,gbi_WaitVerticalSync(a2)     ; mandatory
        lea     GetVSyncState(pc),a1
        move.l  a1,gbi_GetVSyncState(a2)        ; mandatory

	lea	SetScreenSplit(pc),a1
	move.l	a1,gbi_SetSplitPosition(a2) 	; optional
	
; WaitBlitter waits until blitter is ready
        movea.l gbi_ExecBase(a2),a1
        btst    #AFB_68040,AttnFlags+1(a1)
        bne     .goodcpu
        btst    #AFB_68030,AttnFlags+1(a1)
        beq     .goodcpu
        lea     WaitBlitter030(pc),a1
        bra     .continue
.goodcpu:
        lea     WaitBlitter(pc),a1
.continue:
        move.l  a1,gbi_WaitBlitter(a2)          ; mandatory

; blitter functions (optional)
; BlitRect uses blitter to move rectangle
        lea     BlitRect(pc),a1
        move.l  a1,gbi_BlitRect(a2)             ; optional
; InvertRect uses blitter to invert rectangle
        lea     InvertRect(pc),a1
        move.l  a1,gbi_InvertRect(a2)           ; optional
; FillRect uses blitter to fill rectangle
        lea     FillRect(pc),a1
        move.l  a1,gbi_FillRect(a2)             ; optional
; BlitPattern uses blitter to color expand a b/w pattern repeatedly
        lea     BlitPattern(pc),a1
        move.l  a1,gbi_BlitPattern(a2)          ; optional
; BlitTemplate uses blitter to color expand bit plane for Text() and Area...()
        lea     BlitTemplate(pc),a1
        move.l  a1,gbi_BlitTemplate(a2)         ; optional
; BlitPlanar2Chunky uses blitter to blit a rectangle of a planar bitmap into a chunky one
        lea     BlitPlanar2Chunky(pc),a1
        move.l  a1,gbi_BlitPlanar2Chunky(a2)    ; optional
; BlitRectNoMaskComplete uses blitter to blit source to destination without mask with any minterm
        lea     BlitRectNoMaskComplete(pc),a1
        move.l  a1,gbi_BlitRectNoMaskComplete(a2)       ; optional

        or.l    #BIF_BLITTER,gbi_Flags(a2)

; hardware sprite functions
; SetSprite activates hardware sprite
        lea     SetSprite(pc),a1
        move.l  a1,gbi_SetSprite(a2)            ; mandatory when offering hardware sprite
; SetSpritePosition sets hardware sprite position
        lea     SetSpritePosition(pc),a1
        move.l  a1,gbi_SetSpritePosition(a2)    ; mandatory when offering hardware sprite
; SetSpriteImage sets hardware sprite image
        lea     SetSpriteImage(pc),a1
        move.l  a1,gbi_SetSpriteImage(a2)       ; mandatory when offering hardware sprite
; SetSpriteColor sets hardware sprite colors
        lea     SetSpriteColor(pc),a1
        move.l  a1,gbi_SetSpriteColor(a2)       ; mandatory when offering hardware sprite

        lea     EnableSoftSprite(pc),a1
        move.l  a1,gbi_EnableSoftSprite(a2)     ; optional

        or.l    #BIF_HARDWARESPRITE,gbi_Flags(a2)

; clock functions
        lea     SetClock(pc),a1
        move.l  a1,gbi_SetClock(a2)
        lea     ResolvePixelClock(pc),a1
        move.l  a1,gbi_ResolvePixelClock(a2)
        lea     GetPixelClock(pc),a1
        move.l  a1,gbi_GetPixelClock(a2)

        move.l  gbi_Flags(a2),d0
        btst    #BIB_OVERCLOCK,d0
        bne     .overclocked
.normal:
        move.l  #PLANARCLOCKS,gbi_PixelClockCount+PLANAR*4(a2)
        move.l  #CHUNKYCLOCKS,gbi_PixelClockCount+CHUNKY*4(a2)
        move.l  #HIGHCLOCKS,gbi_PixelClockCount+HICOLOR*4(a2)
        move.l  #TRUECLOCKS,gbi_PixelClockCount+TRUECOLOR*4(a2)
        move.l  #ALPHACLOCKS,gbi_PixelClockCount+TRUEALPHA*4(a2)
        bra     .clocks_ok
.overclocked:
        move.l  #PLANAROVER,gbi_PixelClockCount+PLANAR*4(a2)
        move.l  #CHUNKYOVER,gbi_PixelClockCount+CHUNKY*4(a2)
        move.l  #HIGHOVER,gbi_PixelClockCount+HICOLOR*4(a2)
        move.l  #TRUEOVER,gbi_PixelClockCount+TRUECOLOR*4(a2)
        move.l  #ALPHAOVER,gbi_PixelClockCount+TRUEALPHA*4(a2)
.clocks_ok:
        move.w  #6,gbi_BitsPerCannon(a2)

; Cirrus 5434 has 9 bits for horizontal timings
        move.w  #(1<<9)-1,d0
        lsl.w   #3,d0
        move.w  d0,gbi_MaxHorValue+PLANAR*2(a2)
        move.w  d0,gbi_MaxHorValue+CHUNKY*2(a2)
        move.w  d0,gbi_MaxHorValue+HICOLOR*2(a2)
        move.w  d0,gbi_MaxHorValue+TRUECOLOR*2(a2)
        move.w  d0,gbi_MaxHorValue+TRUEALPHA*2(a2)

; Cirrus 5434 has 10 bits for vertical timings  (+ DoubleVertical)
        move.w  #(1<<11)-1,d0
        move.w  d0,gbi_MaxVerValue+PLANAR*2(a2)
        move.w  d0,gbi_MaxVerValue+CHUNKY*2(a2)
        move.w  d0,gbi_MaxVerValue+HICOLOR*2(a2)
        move.w  d0,gbi_MaxVerValue+TRUECOLOR*2(a2)
        move.w  d0,gbi_MaxVerValue+TRUEALPHA*2(a2)
; maximum allowed BitMap sizes. Fill out all depths made available in gbi_RGBFormats
        move.w  #1600,gbi_MaxHorResolution+PLANAR*2(a2)
        move.w  #1600,gbi_MaxVerResolution+PLANAR*2(a2)
        move.w  #1600,gbi_MaxHorResolution+CHUNKY*2(a2)
        move.w  #1600,gbi_MaxVerResolution+CHUNKY*2(a2)
        move.w  #1280,gbi_MaxHorResolution+HICOLOR*2(a2)
        move.w  #1280,gbi_MaxVerResolution+HICOLOR*2(a2)
        move.w  #800,gbi_MaxHorResolution+TRUECOLOR*2(a2)
        move.w  #800,gbi_MaxVerResolution+TRUECOLOR*2(a2)
        move.w  #1024,gbi_MaxHorResolution+TRUEALPHA*2(a2)
        move.w  #1024,gbi_MaxVerResolution+TRUEALPHA*2(a2)

        ; init gbi_CurrentMemoryMode
        move.l  #-1,gbi_CurrentMemoryMode(a2)

; now finally init the chip to something like 640x480x8 in about 31 kHz
; consult your chip manufacturer's manual
        move.l  gbi_RegisterBase(a2),a0

; wake up
        move.b  #$10,$46E8(a0)                  ; Adapter sleep: Setup
        move.b  #$01,$102(a0)                   ; POS102: Video enable
        move.b  #$08,$46E8(a0)                  ; Adapter sleep: Enable

        move.b  #$0f,MISCOUTPUTW(a0)            ; VCLK3, MemEnable, Color

        SetTSc  #$12,TS_UnlockAllExtensions     ; Enable extension registers
        SetTSc  #$00,TS_ConfigReadback          ; Enable normal DAC writes
        SetTSc  #$01,TS_TSMode
        SetTSc  #$38,TS_DRAMControl             ; 2MB setting
        SetTSc  #$03,TS_SynchronousReset        ; restart sequencer
        SetTSc  #$ff,TS_WritePlaneMask
        SetTSc  #$00,TS_FontSelect
        SetTSc  #$0e,TS_MemoryMode
        SetTSc  #$80,TS_ExtendedSequencerMode
        SetTSc  #$80,TS_EEPROMControl
        SetTSc  #$54,TS_PerformanceTuning       ; Bus delay & fifo setting
        SetTSc  #$02,TS_SignatureGeneratorControl
        SetTSc  #$65,TS_VCLK3Numerator
        SetTSc  #$3b,TS_VCLK3Denominator

        move.b  #$ff,DACD(a0)                   ; Pixel mask

; init Cirrus hardware sprite
        SetTSc  #$04,TS_GraphicsCursorAttributes

        SetTSc  #60,TS_GraphicsCursorPattern
        SetTSc  #$1c,TS_MCLKSelect              ; 50.113 MHz
        move.l  #50113630,gbi_MemoryClock(a2)

        SetCRTCc        #$5f,CRTC_HorizontalTotal
        SetCRTCc        #$4f,CRTC_HorizontalDisplayEnd
        SetCRTCc        #$50,CRTC_HorizontalBlankStart
        SetCRTCc        #$82,CRTC_HorizontalBlankEnd
        SetCRTCc        #$54,CRTC_HorizontalSyncStart
        SetCRTCc        #$80,CRTC_HorizontalSyncEnd
        SetCRTCc        #$bf,CRTC_VerticalTotal
        SetCRTCc        #$1f,CRTC_OverflowLow
        SetCRTCc        #$00,CRTC_PresetRowScan
        SetCRTCc        #$40,CRTC_MaximumRowAddress
        SetCRTCc        #$00,CRTC_CursorStartRowAddress
        SetCRTCc        #$00,CRTC_CursorEndRowAddress
        SetCRTCc        #$00,CRTC_LinearStartingAddressMiddle
        SetCRTCc        #$00,CRTC_LinearStartingAddressLow
        SetCRTCc        #$00,CRTC_CursorAddressMiddle
        SetCRTCc        #$00,CRTC_CursorAddressLow
        SetCRTCc        #$9c,CRTC_VerticalSyncStart
        SetCRTCc        #$0e,CRTC_VerticalSyncEnd
        SetCRTCc        #$8f,CRTC_VerticalDisplayEnd
        SetCRTCc        #$50,CRTC_RowOffset
        SetCRTCc        #$00,CRTC_UnderlineRowAddress
        SetCRTCc        #$96,CRTC_VerticalBlankStart
        SetCRTCc        #$b9,CRTC_VerticalBlankEnd
        SetCRTCc        #$c3,CRTC_CRTCMode
        SetCRTCc        #$ff,CRTC_LineCompare
        SetCRTCc        #$00,CRTC_InterlaceEnd
        SetCRTCc        #$00,CRTC_MiscellaneousControl
        SetCRTCc        #$82,CRTC_ExtendedDisplayControls
        SetCRTCc        #$00,CRTC_SyncAdjustGenlock
        SetCRTCc        #$40,CRTC_OverlayExtendedControl

        SetGDCc #$00,GDC_SetReset
        SetGDCc #$00,GDC_EnableSetReset
        SetGDCc #$00,GDC_ColorCompare
        SetGDCc #$00,GDC_DataRotateFunctionSelect
        SetGDCc #$00,GDC_ReadPlaneSelect
        SetGDCc #$00,GDC_GDCMode
        SetGDCc #$00,GDC_Offset0
        SetGDCc #$ec,GDC_Offset1
        SetGDCc #$01,GDC_Miscellaneous
        SetGDCc #$0f,GDC_ColorCare
        SetGDCc #$ff,GDC_BitMask
        SetGDCc #$28,GDC_GDCModeExtensions

; fill attribute controller's registers
        ResetATC

        moveq   #0,d0
.ATC_PaletteRAM_loop:
        move.b  d0,ATCI(a0)
        move.b  d0,ATCD(a0)
        addq.w  #1,d0
        cmp.b   #$f,d0
        ble     .ATC_PaletteRAM_loop

        ResetATC

        SetATC  #$21,ATC_ModeControl
        SetATC  #$00,ATC_OverscanColor
        SetATC  #$0f,ATC_ColorPlaneEnable
        SetATC  #$00,ATC_HorizontalPixelPanning
        SetATC  #$00,ATC_ColorSelect

        SetGDCc #$04,GDC_BLTStartStatus         ; Reset BLT engine

        POP     a2
        rts


***********************************************************
SetGC:
* set mode according to ModeInfo
***********************************************************
* a0:   struct BoardInfo
* a1:   struct ModeInfo
* d0:   BOOL Border
***********************************************************
        PUSH    d2-d7/a2

        move.l  a0,a2
        move.l  a1,gbi_ModeInfo(a2)
        moveq   #0,d4
        move.w  d0,gbi_Border(a2)
        sne.b   d4                   ;keep the border flag in the low-byte
        move.l  gbi_RegisterBase(a2),a0
                                     ;d4 = 0xff: draw a border, d4=0x00: draw no border
        moveq   #0,d6
        moveq   #0,d7

        moveq   #0,d5
        move.w  gmi_HorTotal(a1),d5
        move.w  gmi_Width(a1),d3
        move.b  gmi_Flags(a1),d2

        move.b  #CRTC_CRTCMode,CRTCI(a0)
        move.b  CRTCD(a0),d0
        and.b   #~(1<<2),d0

; Cirrus needs this for modes higher than 1024 lines
        and.b   #~GMF_DOUBLEVERTICAL,d2
        btst    #GMB_INTERLACE,d2
        bne     .interlace

        btst    #GMB_DOUBLESCAN,d2
        beq.s .regular                                  ; thor: experimental
        cmp.w   #512,gmi_VerTotal(a1)
        bge.s .doubletiming1
.regular:
        cmp.w   #1024,gmi_VerTotal(a1)
        blt     .vertical_value_ok
.doubletiming1:
        or.b    #GMF_DOUBLEVERTICAL,d2
.double_vertical:
        or.b    #(1<<2),d0                              ; Multiply Vertical Registers by Two
.interlace:
        moveq   #1,d7
.vertical_value_ok:
        move.b  d0,CRTCD(a0)
        move.b  d2,gmi_Flags(a1)

        move.b  gmi_Depth(a1),d1
        cmp.b   #4,d1
        ble     .pixel_nibble
        cmp.b   #8,d1
        ble     .pixel_byte
        cmp.b   #16,d1
        ble     .pixel_word
        cmp.b   #24,d1
        ble     .pixel_triplebyte

; 32 bit mode adjustments
.pixel_doubleword:
        move.b  #TS_ExtendedSequencerMode,TSI(a0)
        or.b    #(1<<0)|(4<<1),TSD(a0)                  ; 256 color enable & enable crtc clock divider
        and.b   #~(3<<1),TSD(a0)                        ; to 32 bit/pixel
        bra     .set_border_direct

; 24 bit mode adjustments
.pixel_triplebyte:
        move.b  #TS_ExtendedSequencerMode,TSI(a0)
        or.b    #(1<<0)|(2<<1),TSD(a0)                  ; 256 color enable & enable crtc clock divider
        and.b   #~(5<<1),TSD(a0)                        ; to 24 bit/pixel
        bra     .set_border_direct

; 15/16 bit mode adjustments
.pixel_word:
        move.b  #TS_ExtendedSequencerMode,TSI(a0)
        or.b    #(1<<0)|(3<<1),TSD(a0)                  ; 256 color enable & enable crtc clock divider
        and.b   #~(4<<1),TSD(a0)                        ; to 16 bit/pixel

.set_border_direct:
        move.b  #CRTC_ExtendedDisplayControls,CRTCI(a0)
        move.b  CRTCD(a0),d0
        and.b   #~(1<<5),d0
        or.b    #(1<<7),d0                              ;always draw a border
        move.w  #$8000,d4                               ;indicate to erase the border
        move.b  d0,CRTCD(a0)
        bra     .pixel_end

; 8 bit mode adjustments
.pixel_byte:
        move.b  #TS_ExtendedSequencerMode,TSI(a0)
        or.b    #(1<<0),TSD(a0)                         ; 256 color enable
        and.b   #~(7<<1),TSD(a0)                        ; disable crtc clock divider
        btst    #GMB_DOUBLECLOCK,d2
        beq     .pixel_clut
        or.b    #(3<<1),TSD(a0)                         ; doubleclock
        lsr.w   #1,d3
        lsr.w   #1,d5
        moveq   #1,d6
        bra     .pixel_clut

; 4 bit mode adjustments
.pixel_nibble:
        move.b  #TS_ExtendedSequencerMode,TSI(a0)
        and.b   #~(7<<1),TSD(a0)                        ; disable crtc clock divider
        and.b   #~(1<<0),TSD(a0)                        ; 256 color disable

; 4 and 8 bit common parts
.pixel_clut:
        move.b  #CRTC_ExtendedDisplayControls,CRTCI(a0)
        move.b  CRTCD(a0),d0
        ;and.b   #~(1<<7),d0
        ;or.b    #(1<<5),d0
        ;tst.w   d4
        ;beq     .no_border_nibble
        ;tst.w   gmi_VerBlankSize(a1)
        ;beq     .no_border_nibble
        ;tst.w   gmi_HorBlankSize(a1)
        ;beq     .no_border_nibble
        and.b   #~(1<<5),d0
        or.b    #(1<<7),d0                              ;always draw a border
;.no_border_nibble:
        ori.w   #$8000,d4                               ;indicate to erase the border if not set
        move.b  d0,CRTCD(a0)

; common for all modes
.pixel_end:
        moveq   #0,d1
        move.b  gmi_Depth(a1),d1
        cmp.b   #4,d1
        ble     .performance_nibble

        add.l   #1,d1
        lsr.w   #3,d1
;       lsr.w   #2,d1

; now calculate the optimal FIFO value, based on the memory clock and
; the requested pixel clock
; raw bandwidth guess:
; MemoryClock * MemoryBusWidth / NumberOfFIFOValues
;
; we reduce the MemoryBusWidth by some percent to get a safety margin.
; play with the value of 62 below
        mulu.l  gmi_PixelClock(a1),d1
        move.l  gbi_MemoryClock(a2),d0
        lsr.l   #1,d0                   ; prevent value overflows due to the multiplication
        mulu.l  #62,d0                  ; 62 (64 bit bus bandwidth minus some percent)
        lsr.l   #7,d0
        divu.l  d0,d1

        add.l   #1,d1
        cmp.l   #15,d1
        ble     .performance_all
        moveq   #0,d1
        bra     .performance_all
.performance_nibble:
        moveq   #13,d1                  ; PerformanceTuning is fix for planar
.performance_all:
        move.b  #TS_PerformanceTuning,TSI(a0)
        move.b  TSD(a0),d0
        and.b   #%11110000,d0
        and.b   #%00001111,d1
        or.b    d1,d0
        move.b  d0,TSD(a0)


        move.w  d3,d0
        lsr.w   #3,d0
        tst.w   d4                      ;do we have a border ?
        beq.s   .cutatedge              ;if not, do not extend for scrolling
        tst.b   d4                      ;did we force a border?
        beq.s   .extend_border          ;if so, extend by one character
.cutatedge:
        subq.w  #1,d0                   ;if no border, cut at the real end
.extend_border:
        move.b  #CRTC_HorizontalDisplayEnd,CRTCI(a0)
        move.b  d0,CRTCD(a0)

**********************
vertical_display_end:
        move.w  gmi_Height(a1),d1
        btst    #GMB_DOUBLESCAN,d2
        beq     .not_doublescan
        add.w   d1,d1
.not_doublescan:
        lsr.w   d7,d1
        subq.w  #1,d1

        move.b  #CRTC_VerticalDisplayEnd,CRTCI(a0)
        move.b  d1,CRTCD(a0)

        move.b  #CRTC_OverflowLow,CRTCI(a0)
        move.b  CRTCD(a0),d0

        and.b   #%10111101,d0
        btst    #8,d1
        beq     .not8
        or.b    #%00000010,d0
.not8:
        btst    #9,d1
        beq     .not9
        or.b    #%01000000,d0
.not9:
        move.b  d0,CRTCD(a0)

*************************************
horizontal_total:
        move.w  d5,d1
        lsr.w   #3,d1
        subq.w  #5,d1
        move.b  #CRTC_HorizontalTotal,CRTCI(a0)
        move.b  d1,CRTCD(a0)

        move.w  d5,d0
        lsr.w   #4,d0
        move.b  #CRTC_InterlaceEnd,CRTCI(a0)
        move.b  d0,CRTCD(a0)

*************************************
horizontal_blank:
        moveq   #0,d1
        tst.b   d4
        beq     .no_border_at_left
        move.w  gmi_HorBlankSize(a1),d1
        lsr.w   d6,d1
.no_border_at_left:
        add.w   d3,d1
        lsr.w   #3,d1
        subq.w  #1,d1

        move.b  #CRTC_HorizontalBlankStart,CRTCI(a0)
        move.b  d1,CRTCD(a0)

        moveq   #0,d1
        tst.w   d4
        beq     .no_border_at_right
        tst.b   d4
        beq     .blank_at_zero
        move.w  gmi_HorBlankSize(a1),d1
        beq     .blank_at_zero
        lsr.w   d6,d1
        neg.w   d1
.no_border_at_right
        add.w   d5,d1
        lsr.w   #3,d1
        subq.w  #1,d1
.blank_at_zero:

        move.b  #CRTC_MiscellaneousControl,CRTCI(a0)
        move.b  CRTCD(a0),d0
        and.b   #%11001111,d0
        btst    #7,d1
        beq     .not7
        bset    #5,d0
.not7:
        btst    #6,d1
        beq     .not6
        bset    #4,d0
.not6:
        move.b  d0,CRTCD(a0)

        move.b  #CRTC_HorizontalSyncEnd,CRTCI(a0)
        move.b  CRTCD(a0),d0
        bclr    #7,d0
        btst    #5,d1
        beq     .not5
        bset    #7,d0
.not5:
        move.b  d0,CRTCD(a0)

        and.b   #%00011111,d1
        move.b  #CRTC_HorizontalBlankEnd,CRTCI(a0)
        move.b  CRTCD(a0),d0
        and.b   #%11100000,d0
        or.b    d1,d0
        move.b  d0,CRTCD(a0)

*************************************
horizontal_sync:
        move.w  gmi_HorSyncStart(a1),d1
        move.b  #CRTC_SyncAdjustGenlock,CRTCI(a0)
        move.b  CRTCD(a0),d0
        and.b   #%11111000,d0
        and.b   #%00000111,d1
        or.b    d1,d0
        move.b  d0,CRTCD(a0)

        move.w  gmi_HorSyncStart(a1),d1
        lsr.w   d6,d1
        add.w   d3,d1
        lsr.w   #3,d1
        move.b  #CRTC_HorizontalSyncStart,CRTCI(a0)
        move.b  d1,CRTCD(a0)

        move.w  gmi_HorSyncSize(a1),d0
        lsr.w   d6,d0
        lsr.w   #3,d0
        add.w   d0,d1

        move.b  #CRTC_HorizontalSyncEnd,CRTCI(a0)
        move.b  CRTCD(a0),d0
        and.b   #%11100000,d0
        and.b   #%00011111,d1
        or.b    d1,d0
        move.b  d0,CRTCD(a0)

*************************************
horizontal_sync_skew:
        move.b  gmi_HorSyncSkew(a1),d1

        move.b  #CRTC_HorizontalSyncEnd,CRTCI(a0)
        move.b  CRTCD(a0),d0
        lsl.b   #5,d1
        and.b   #%01100000,d1
        and.b   #%10011111,d0
        or.b    d1,d0
        move.b  d0,CRTCD(a0)

*************************************
horizontal_enable_skew:
        move.b  gmi_HorEnableSkew(a1),d1

        move.b  #CRTC_HorizontalBlankEnd,CRTCI(a0)
        move.b  CRTCD(a0),d0
        lsl.b   #5,d1
        and.b   #%01100000,d1
        and.b   #%10011111,d0
        or.b    d1,d0
        move.b  d0,CRTCD(a0)

*************************************
vertical_total:
        move.w  gmi_VerTotal(a1),d1
        btst    #GMB_DOUBLESCAN,d2
        beq     .not_doublescan
        add.w   d1,d1
.not_doublescan:
        lsr.w   d7,d1
        subq.w  #2,d1

        move.b  #CRTC_VerticalTotal,CRTCI(a0)
        move.b  d1,CRTCD(a0)

        move.b  #CRTC_OverflowLow,CRTCI(a0)
        move.b  CRTCD(a0),d0
        and.b   #%11011110,d0
        btst    #8,d1
        beq     .not8
        bset    #0,d0
.not8:
        btst    #9,d1
        beq     .not9
        bset    #5,d0
.not9:
        move.b  d0,CRTCD(a0)

*************************************
vertical_blank:
        move.w  gmi_Height(a1),d1
        btst    #GMB_DOUBLESCAN,d2
        beq     .not_doublescan1
        add.w   d1,d1
.not_doublescan1:
        tst.b   d4
        beq     .no_border_at_bottom
        add.w   gmi_VerBlankSize(a1),d1
.no_border_at_bottom
        lsr.w   d7,d1
        subq.w  #1,d1

        move.b  #CRTC_VerticalBlankStart,CRTCI(a0)
        move.b  d1,CRTCD(a0)

        move.b  #CRTC_OverflowLow,CRTCI(a0)
        move.b  CRTCD(a0),d0
        bclr    #3,d0
        btst    #8,d1
        beq     .not8
        bset    #3,d0
.not8:
        move.b  d0,CRTCD(a0)

        move.b  #CRTC_MaximumRowAddress,CRTCI(a0)
        move.b  CRTCD(a0),d0
        bclr    #5,d0
        btst    #9,d1
        beq     .not9
        bset    #5,d0
.not9:
        move.b  d0,CRTCD(a0)

        move.w  gmi_VerTotal(a1),d1
        btst    #GMB_DOUBLESCAN,d2
        beq     .not_doublescan
        add.w   d1,d1
.not_doublescan:
        tst.w   d4
        beq     .no_border_at_top
        tst.b   d4
        beq     .setvborderzero
        move.w  gmi_VerBlankSize(a1),d0
        bne     .nonzerosized
.setvborderzero:
        moveq   #0,d1
        beq     .havevblank
.nonzerosized:
        sub.w   d0,d1
.no_border_at_top
        lsr.w   d7,d1
        subq.w  #1,d1
.havevblank:
        move.b  #CRTC_MiscellaneousControl,CRTCI(a0)
        move.b  CRTCD(a0),d0
        and.b   #%00111111,d0
        btst    #9,d1
        beq     .not_9
        bset    #7,d0
.not_9:
        btst    #8,d1
        beq     .not_8
        bset    #6,d0
.not_8:
        move.b  d0,CRTCD(a0)

        move.b  #CRTC_VerticalBlankEnd,CRTCI(a0)
        move.b  d1,CRTCD(a0)

*************************************
vertical_sync:
        move.w  gmi_Height(a1),d1
        btst    #GMB_DOUBLESCAN,d2
        beq     .not_doublescan
        add.w   d1,d1
.not_doublescan:
        add.w   gmi_VerSyncStart(a1),d1
        lsr.w   d7,d1

        move.b  #CRTC_VerticalSyncStart,CRTCI(a0)
        move.b  d1,CRTCD(a0)

        move.b  #CRTC_OverflowLow,CRTCI(a0)
        move.b  CRTCD(a0),d0
        and.b   #%01111011,d0
        btst    #8,d1
        beq     .not8
        bset    #2,d0
.not8:
        btst    #9,d1
        beq     .not9
        bset    #7,d0
.not9:
        move.b  d0,CRTCD(a0)

        lsl.w   d7,d1
        add.w   gmi_VerSyncSize(a1),d1
        lsr.w   d7,d1

        move.b  #CRTC_VerticalSyncEnd,CRTCI(a0)
        move.b  CRTCD(a0),d0
        and.b   #%00001111,d1
        and.b   #%11110000,d0
        or.b    d1,d0
        move.b  d0,CRTCD(a0)

*************************************
interlace:
        move.b  #CRTC_MiscellaneousControl,CRTCI(a0)
        move.b  CRTCD(a0),d0

        moveq   #0,d1
        bclr    #0,d0
        btst    #GMB_INTERLACE,d2
        beq     .no
        bset    #0,d0
        move.w  gmi_HorSyncStart(a1),d1
        add.w   d3,d1
        lsr.w   #4,d1
.no:
        move.b  d0,CRTCD(a0)

        move.b  #CRTC_InterlaceEnd,CRTCI(a0)
        move.b  d1,CRTCD(a0)

*************************************
doublescan:
        move.b  #CRTC_MaximumRowAddress,CRTCI(a0)
        move.b  CRTCD(a0),d0

        bclr    #7,d0
        btst    #GMB_DOUBLESCAN,d2
        beq     .no
        bset    #7,d0
.no:
        move.b  d0,CRTCD(a0)

*************************************
polarity:

        move.b  MISCOUTPUTR(a0),d0
        and.b   #%00111111,d0

        btst    #GMB_HPOLARITY,d2
        beq     .not_horizontal
        bset    #6,d0
.not_horizontal:
        btst    #GMB_VPOLARITY,d2
        beq     .not_vertical
        bset    #7,d0
.not_vertical:
        move.b  d0,MISCOUTPUTW(a0)

*************************************
        move.l  (gbi_ExecBase,a2),a1
        DISABLE a1,NOFETCH              ; no disturbance while modifying ATC registers

*************************************
        ResetATC

*************************************
bordercolor:

        move.b  ATCI(a0),d0
        and.b   #(1<<5),d0
        or.b    #ATC_OverscanColor,d0
        move.b  d0,ATCI(a0)
        move.b  #0,ATCD(a0)

*************************************
        ENABLE  a1,NOFETCH

*************************************
        POP     d2-d7/a2
        rts


************************************************************************
SetPanning:
************************************************************************
* Set view origin for overscan displays
* a0:   struct BoardInfo
* a1:   UBYTE *Memory
* d0:   UWORD Width
* d1:   WORD XOffset
* d2:   WORD YOffset
* d7:   RGBFTYPE Format
************************************************************************

        PUSH    d2-d3

        move.w  d1,gbi_XOffset(a0)
        move.w  d2,gbi_YOffset(a0)

        suba.l  gbi_MemoryBase(a0),a1

        cmp.l   #RGBFB_MaxFormats,d7
        bcc     .byte

        move.w  (.formats,pc,d7.w*2),d3
        jmp     (.formats,pc,d3.w)

.formats:
        dc.w    .nibble-.formats
        dc.w    .byte-.formats
        dc.w    .triplebyte-.formats
        dc.w    .triplebyte-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .doubleword-.formats
        dc.w    .doubleword-.formats
        dc.w    .doubleword-.formats
        dc.w    .doubleword-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .byte-.formats
        dc.w    .byte-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats

.nibble:
        ext.l   d1
        muls    d0,d2
        add.l   d1,d2
        lsr.l   #3,d2                   ; Relative startaddress
        lsr.w   #4,d0                   ; Offset
        and.b   #$07,d1                 ; Pixelpanning
        add.l   a1,d2                   ; Planar special
        bra     .all_formats

.doubleword:
        ext.l   d1
        muls    d0,d2
        add.l   d1,d2
        lsl.l   #2,d2                   ; Relative startaddress
        lsr.w   #2,d0                   ; Offset (!)
        and.b   #$01,d1                 ; Pixelpanning
        bra     .non_planar_formats

.triplebyte:
        move.w  d0,d3
        add.w   d0,d0
        add.w   d3,d0
        move.w  d1,d3
        add.w   d1,d1
        add.w   d3,d1
        ext.l   d1
        muls    d0,d2
        add.l   d1,d2                   ; Relative startaddress
        lsr.w   #3,d0                   ; Offset
        and.b   #$07,d1                 ; Pixelpanning
        bra     .non_planar_formats

.word:
        ext.l   d1
        muls    d0,d2
        add.l   d1,d2
        lsl.l   #1,d2                   ; Relative startaddress
        lsr.w   #2,d0                   ; Offset
        and.b   #$03,d1                 ; Pixelpanning
        bra     .non_planar_formats

.byte:
        ext.l   d1
        muls    d0,d2
        add.l   d1,d2                   ; Relative startaddress
        lsr.w   #3,d0                   ; Offset
        and.b   #$07,d1                 ; Pixelpanning

        btst    #GMB_DOUBLECLOCK,([gbi_ModeInfo,a0],gmi_Flags)
        beq     .non_planar_formats

.doubleclock:
        lsr.b   #1,d1

.non_planar_formats:
        add.l   a1,d2
        lsr.l   #2,d2
        bclr    #0,d2

.all_formats:
        movea.l gbi_ExecBase(a0),a1
        movea.l gbi_RegisterBase(a0),a0

.linear_start_address:
        move.b  #CRTC_ExtendedDisplayControls,CRTCI(a0)
        move.b  CRTCD(a0),d3
        and.b   #%11110010,d3
        btst    #16,d2
        beq     .not16
        bset    #0,d3
.not16:
        btst    #17,d2
        beq     .not17
        bset    #2,d3
.not17:
        btst    #18,d2
        beq     .not18
        bset    #3,d3
.not18:
        move.b  d3,CRTCD(a0)

        move.b  #CRTC_OverlayExtendedControl,CRTCI(a0)
        move.b  CRTCD(a0),d3
        and.b   #%01111111,d3
        btst    #19,d2
        beq     .not19
        bset    #7,d3
.not19:
        move.b  d3,CRTCD(a0)

        move.w  d2,d3
        lsr.w   #8,d3

        move.b  #CRTC_LinearStartingAddressMiddle,CRTCI(a0)
        move.b  d3,CRTCD(a0)

.row_offset:
        move.b  #CRTC_ExtendedDisplayControls,CRTCI(a0)
        move.b  CRTCD(a0),d3
        and.b   #%11101111,d3
        btst    #8,d0
        beq     .not8
        bset    #4,d3
.not8:
        move.b  d3,CRTCD(a0)

        move.b  #CRTC_RowOffset,CRTCI(a0)
        move.b  d0,CRTCD(a0)            ; lower eight bits

.pixelpanning:
        DISABLE a1,NOFETCH              ; no disturbance while modifying ATC registers

        ResetATC

        move.b  ATCI(a0),d3
        and.b   #(1<<5),d3
        move.b  #ATC_HorizontalPixelPanning,ATCI(a0)
        or.b    #ATC_HorizontalPixelPanning,d3
        move.b  d1,ATCD(a0)
        move.b  d3,ATCI(a0)

        ENABLE  a1,NOFETCH

        move.b  #CRTC_LinearStartingAddressLow,CRTCI(a0)        ; use new address now
        move.b  d2,CRTCD(a0)

        POP     d2-d3
        rts


***********************************************************
CalculateBytesPerRow:
* calculate length of display line in bytes
***********************************************************
* a0:   struct BoardInfo
* d0:   UWORD Width
* d7:   RGBFTYPE RGBFormat
***********************************************************

        cmp.l   #RGBFB_MaxFormats,d7
        bcc     .end

        move.w  (.formats,pc,d7.l*2),d1
        jmp     (.formats,pc,d1.w)

.formats:
        dc.w    .nibble-.formats
        dc.w    .byte-.formats
        dc.w    .triplebyte-.formats
        dc.w    .triplebyte-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .doubleword-.formats
        dc.w    .doubleword-.formats
        dc.w    .doubleword-.formats
        dc.w    .doubleword-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .byte-.formats
        dc.w    .byte-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats

.triplebyte:
        move.w  d0,d1
        add.w   d0,d1
        add.w   d1,d0
        bra     .end

.nibble:
        lsr.w   #3,d0
        bra     .end

.doubleword:
        add.w   d0,d0
.word:
        add.w   d0,d0           ; linearstartingaddress
.byte:
.end:
	cmp.w	#$1000,d0	; maximal width for panning
	blo.s	.ok
	moveq	#0,d0		; this cannot work
.ok:
        rts


***********************************************************
CalculateMemory:
***********************************************************
* a0:   struct BoardInfo
* a1:   APTR Memory
* d7:   RGBFTYPE RGBFormat
***********************************************************
        move.l  a1,d0
        rts


*************************************
SetSprite:
* a0:   struct BoardInfo
* d0:   BOOL activate
* d7:   RGBFTYPE RGBFormat
*************************************
        move.l  gbi_RegisterBase(a0),a0
        move.b  #TS_GraphicsCursorAttributes,TSI(a0)
        move.b  TSD(a0),d1
        and.b   #%11111110,d1
        and.b   #%00000001,d0
        or.b    d0,d1
        move.b  d1,TSD(a0)
        rts


*************************************
SetSpritePosition:
SetSpriteImage:
* a0:   struct BoardInfo
* d7:   RGBFTYPE RGBFormat
*************************************
        PUSH    a2/d2-d7
        move.l  a0,a2

        movem.w (gbi_MouseX,a2),d4/d5
        sub.w   (gbi_XOffset,a2),d4
        sub.w   (gbi_YOffset,a2),d5
	move.w	d5,d7
	add.w   (gbi_YSplit,a2),d5

        move.l  gbi_RegisterBase(a2),a0
        move.l  gbi_ModeInfo(a2),d6
        move.l  d6,a1
        beq     .no_mode
        move.b  gmi_Flags(a1),d6

.no_mode:
        btst    #GMB_DOUBLESCAN,d6
        beq.s   .no_doublescan
        add.w   d5,d5
	add.w	d7,d7
.no_doublescan:

        btst    #GMB_DOUBLEVERTICAL,d6
        beq.s   .no_doublevert
        asr.w   #1,d5
	asr.w	#1,d7
.no_doublevert:

        btst    #GMB_DOUBLECLOCK,d6
        beq     .no_doubleclock
        asr.w   #1,d4
.no_doubleclock:

        moveq   #0,d2
        tst.w   d4
        bpl     .x_ok
        sub.w   d4,d2
        moveq   #0,d4
.x_ok:
        moveq   #0,d3
        tst.w   d5
        bpl     .y_ok
        sub.w   d5,d3
        moveq   #0,d5
.y_ok:
        moveq   #0,d0
        moveq   #0,d1
        move.l  gbi_Flags(a2),d6
        move.b  gbi_MouseWidth(a2),d0
        move.b  gbi_MouseHeight(a2),d1

	;; Check whether we can move the mouse down by moving the graphics down.
	tst.w	d3		;is there a positive offset
	bgt.s	.noshift
	tst.w	d7		;are we below the split position?
	bpl.s	.noshift	;if so, avoid the downshift

	neg.w	d7		;number of lines above the split
	cmp.w	d1,d7		;more than the mouse height?
	bge.s 	.keepheight
	move.w	d7,d1		;limit the mouse height by the number of lines above the split
.keepheight:
	
	moveq	#64,d3		;maximum amount of lines we can shift
	sub.w	d1,d3 		;number of lines we can shift (is already premultiplied)
	bge.s	.testclamp
	moveq	#0,d3
.testclamp:	
	cmp.w	d5,d3		;compare with Y position. Still room to shift?
	ble.s	.noclamp	;if lower than Y position, do not clamp the maximal shift
	move.w	d5,d3		;here the number of lines to shift is > Ypos, so adjust by at most this amount
.noclamp:
	sub.w	d3,d5		;adjust Y accordingly (shift up)
	neg.w	d3		;signal a fill-in offset
	
.noshift:
        btst   	#BIB_HIRESSPRITE,d6
        bne.s   .hires

        btst  	#BIB_BIGSPRITE,d6
        bne.s   .zoom

        bsr     RenderSpriteNormal
        bra.s   .done
.hires:
        bsr     RenderSpriteHires
        bra.s   .done
.zoom:
        bsr     RenderSpriteZoomed
.done:
        move.l  gbi_RegisterBase(a2),a0
	move.w	gbi_SpriteBank(a2),d0
	eor.b	#4,d0
	move.w	d0,gbi_SpriteBank(a2)
	or.w	#56|(TS_GraphicsCursorPattern<<8),d0
	move.w	d0,TSI(a0)

        and.w   #$7FF,d4
        and.w   #$7FF,d5
        lsl.w   #5,d4
        lsl.w   #5,d5
        or.w    #$10,d4
        or.w    #$11,d5
        rol.w   #8,d4
        rol.w   #8,d5
        move.w  d4,TSI(a0)      ;Cursor X
        move.w  d5,TSI(a0)      ;Cursor Y

        POP     a2/d2-d7
        rts
	
*************************************
RenderSpriteNormal:
* set sprite image for small sprites (i.e. user did not select "BIGSPRITE=Yes")
* a2:   struct BoardInfo
* d0.w: Width
* d1.w: Height
* d2.w: OffsetX
* d3.w: OffsetY (into the source data, i.e. positive values shift the sprite up)
*************************************
        PUSH    d2-d4/a3
        move.l  gbi_RegisterBase(a2),a0
        move.w  #(TS_MemoryMode<<8)|$0e,(TSI,a0)

        move.l  gbi_MemoryBase(a2),a0
        add.l   gbi_MemorySize(a2),a0
        add.l   #rm_HardWareSprite2,a0
	tst.w	gbi_SpriteBank(a2)
	beq.s	.bank2
	sub.l   #SPRITEBUFFERSIZE,a0
.bank2:	
	lea	SPRITEBUFFERSIZE(a0),a3	;end of the sprite data
	
	tst.w	d3		; a negative fill-in offset? Start at a later line (offset as given by d3)
	bpl.s	.regular
	neg.w	d3
	moveq	#0,d4
	subq.w	#1,d3
.initial_clear_loop:
        move.l  d4,(a0)+
        move.l  d4,(a0)+
        move.l  d4,(a0)+
        move.l  d4,(a0)+
        dbra    d3,.initial_clear_loop
	moveq	#0,d3
	
.regular:	
        sub.w   d3,d1           ; Start line
        cmp.w   #64,d1
        ble.s   .ok
        move.w  #64,d1
.ok:
        move.w  d2,d4

        move.l  gbi_MouseImage(a2),a1
	tst.w   a1
	beq.s	.plane_done
	lea	4(a1,d3.w*4),a1	;YOffset

        move.w  d1,d0
        subq.w  #1,d0
        bmi     .plane_done
.plane_loop:
        movem.w (a1)+,d2/d3

        lsl.w   d4,d3
        lsl.w   d4,d2
        move.w  d3,(a0)+
        move.w  #0,(a0)+
        move.l  #0,(a0)+
        move.w  d2,(a0)+
        move.w  #0,(a0)+
        move.l  #0,(a0)+
        dbra    d0,.plane_loop

.plane_done:
        moveq   #0,d1
.plane_clear_loop:
	cmp.l	a3,a0
	bhs.s	.plane_clear_done
        move.l  d1,(a0)+
        move.l  d1,(a0)+
        move.l  d1,(a0)+
        move.l  d1,(a0)+
        bra.s	.plane_clear_loop
.plane_clear_done:

        POP     d2-d4/a3
        rts

*************************************
RenderSpriteZoomed:
* set sprite image for big sprites (i.e. user did select "BIGSPRITE=Yes")
* a2:   struct BoardInfo
* d0.w: Width
* d1.w: Height
* d2.w: OffsetX
* d3.w: OffsetY
*************************************

        PUSH    d2-d7/a3-a4

        lsr.w   #1,d0
        lsr.w   #1,d1

        move.l  gbi_MemoryBase(a2),a0
        add.l   gbi_MemorySize(a2),a0
        add.l   #rm_HardWareSprite2,a0
	tst.w	gbi_SpriteBank(a2)
	beq.s	.bank2
	sub.l   #SPRITEBUFFERSIZE,a0
.bank2:	
        move.w  d2,d6           ; XOffset
	lea	SPRITEBUFFERSIZE(a0),a4	;end of the sprite data

	tst.w	d3		; a negative fill-in offset? Start at a later line (offset as given by d3)
	bpl.s	.regular
	neg.w	d3
	moveq	#0,d4
	subq.w	#1,d3
.initial_clear_loop:
        move.l  d4,(a0)+
        move.l  d4,(a0)+
        move.l  d4,(a0)+
        move.l  d4,(a0)+
        dbra    d3,.initial_clear_loop
	moveq	#0,d3
.regular:	
	
;**
;** offset calculation is wrong, d3 must
;** be divided by two to get the same units
;** as the position/offset data
;** Fixed.
;** 5.3.99, THOR
;**
        move.w  d3,d7           ; keep it
        lsr.w   #1,d3           ; calculate the offset
        sub.w   d3,d1           ; Start line
	ble	.plane_done
        move.w  d1,d0
        add.w   d1,d1
        lsr.w   #1,d7           ; carry implies shifted source data access
        bcc     .bit
        bset    #31,d1
        sub.w   #1,d1
.bit:
        move.w  d3,d7           ; YOffset
        lsl.w   #2,d7

;**
;** d0 is the loop counter, not d1.
;** corrected, 5.3.99, THOR
;**
        cmp.w   #32,d0
        ble.s   .ok
        move.w  #32,d0
.ok:
.plane:
        move.l  gbi_MouseImage(a2),a1
	tst.w   a1
	beq.s	.plane_done
        add.w   #4,a1
        add.w   d7,a1

        subq.w  #1,d0
        bmi     .plane_done
.plane_loop:
        move.w  (a1)+,d2                ;get data
        move.w  d2,d5
        lsr.w   #4,d2
        move.w  d2,d4
        lsr.w   #4,d2
        move.w  d2,d3
        lsr.w   #4,d2
        and.w   #15,d3
        and.w   #15,d4
        and.w   #15,d5
        move.b  .table(pc,d4.w),d4
        swap    d4
        move.b  .table(pc,d2.w),d4
        lsl.l   #8,d4
        move.b  .table(pc,d3.w),d4
        swap    d4
        move.b  .table(pc,d5.w),d4
        lsl.l   d6,d4
        move.l  d4,a3                   ;magnify by two

        move.w  (a1)+,d2                ;get data
        move.w  d2,d5
        lsr.w   #4,d2
        move.w  d2,d4
        lsr.w   #4,d2
        move.w  d2,d3
        lsr.w   #4,d2
        and.w   #15,d3
        and.w   #15,d4
        and.w   #15,d5
        move.b  .table(pc,d4.w),d4
        swap    d4
        move.b  .table(pc,d2.w),d4
        lsl.l   #8,d4
        move.b  .table(pc,d3.w),d4
        swap    d4
        move.b  .table(pc,d5.w),d4
        lsl.l   d6,d4                   ;ditto

        bclr    #31,d1                  ;round off?
        bne     .skip
        move.l  d4,(a0)+
        move.l  #0,(a0)+
        move.l  a3,(a0)+
        move.l  #0,(a0)+
.skip:
        move.l  d4,(a0)+
        move.l  #0,(a0)+
        move.l  a3,(a0)+
        move.l  #0,(a0)+
        dbra    d0,.plane_loop
;**
;** erase the remaining lines
;**

;**
;** the following failed if d1 was negative. This happens
;** if an autoscroll screen is scrolled downwards very fast
;** while holding the mouse pointer fixed at the drag bar
;** Fixed, THOR 5.4.99
;**
.plane_done:
        moveq   #0,d1
.plane_clear_loop:
	cmp.l	a4,a0
	bhs.s	.plane_clear_done
        move.l  d1,(a0)+
        move.l  d1,(a0)+
        move.l  d1,(a0)+
        move.l  d1,(a0)+
        bra.s	.plane_clear_loop
.plane_clear_done:

        POP     d2-d7/a3-a4
        rts
	
.table:
        dc.b    %00000000
        dc.b    %00000011
        dc.b    %00001100
        dc.b    %00001111
        dc.b    %00110000
        dc.b    %00110011
        dc.b    %00111100
        dc.b    %00111111
        dc.b    %11000000
        dc.b    %11000011
        dc.b    %11001100
        dc.b    %11001111
        dc.b    %11110000
        dc.b    %11110011
        dc.b    %11111100
        dc.b    %11111111

*************************************
RenderSpriteHires:
* set sprite image for hires sprites
* a2:   struct BoardInfo
* d0.w: Width
* d1.w: Height
* d2.w: OffsetX
* d3.w: OffsetY
*************************************

        PUSH    d2-d4/a3

        move.l  gbi_RegisterBase(a2),a0
        move.w  #(TS_MemoryMode<<8)|$0e,(TSI,a0)

        move.l  gbi_MemoryBase(a2),a0
        add.l   gbi_MemorySize(a2),a0
        add.l   #rm_HardWareSprite2,a0
	tst.w	gbi_SpriteBank(a2)
	beq.s	.bank2
	sub.l   #SPRITEBUFFERSIZE,a0
.bank2:	
	lea	SPRITEBUFFERSIZE(a0),a3	;end of the sprite data

	tst.w	d3		; a negative fill-in offset? Start at a later line (offset as given by d3)
	bpl.s	.regular
	neg.w	d3
	moveq	#0,d4
	subq.w	#1,d3
.initial_clear_loop:
        move.l  d4,(a0)+
        move.l  d4,(a0)+
        move.l  d4,(a0)+
        move.l  d4,(a0)+
        dbra    d3,.initial_clear_loop
	moveq	#0,d3
	
.regular:
        sub.w   d3,d1           ; Start line

        cmp.w   #64,d1
        ble     .ok
        move.w  #64,d1
.ok:

        move.w  d2,d4

        move.l  gbi_MouseImage(a2),a1
	tst.w   a1
	beq.s	.plane_done
	lea	8(a1,d3.w*8),a1

        move.w  d1,d0
        subq.w  #1,d0
        bmi     .plane_done
.plane_loop:
        movem.l (a1)+,d2/d3

        lsl.l   d4,d3
        lsl.l   d4,d2
        move.l  d3,(a0)+
        move.l  #0,(a0)+
        move.l  d2,(a0)+
        move.l  #0,(a0)+
        dbra    d0,.plane_loop

.plane_done:
        moveq   #0,d1
.plane_clear_loop:
	cmp.l	a3,a0
	bhs.s	.plane_clear_done
        move.l  d1,(a0)+
        move.l  d1,(a0)+
        move.l  d1,(a0)+
        move.l  d1,(a0)+
        bra.s	.plane_clear_loop
.plane_clear_done:

        POP     d2-d4/a3
        rts

*************************************
EnableSoftSprite:
* check whether a soft sprite is needed
* a0:   struct BoardInfo *
* d0:   FormatFlags (as bitmask)
* a1:   struct ModeInfo *
* returns TRUE in case a soft sprite
* is required.
*************************************
        and.w   gbi_SoftSpriteFlags(a0),d0
        bne.s   .forcesoftsprite
        move.l  a1,d0           ;mode info present?
        beq.s   .exit           ;if not, use hw sprite

        move.b  gmi_Flags(a1),d0
; Cirrus needs this for modes higher than 1024 lines
; but then has no reliable hardware sprite
        and.b   #~GMF_DOUBLEVERTICAL,d0
        btst    #GMB_INTERLACE,d0
        bne     .interlace

        btst    #GMB_DOUBLESCAN,d0
        beq.s   .regular
        cmp.w   #512,gmi_VerTotal(a1)
        bge.s   .forcesoftsprite
.regular:
        cmp.w   #1024,gmi_VerTotal(a1)
        blt     .vertical_value_ok
.forcesoftsprite:
        moveq   #1,d0
        bra.s   .exit
.vertical_value_ok:
.interlace:
        moveq   #0,d0
.exit:
        rts

*************************************
SetSpriteColor:
* set sprite colors
* a0:   struct BoardInfo
* d0.b: sprite color index (0-2)
* d1.b: red
* d2.b: green
* d3.b: blue
* d7:   RGBFTYPE RGBFormat
*************************************
        tst.b   d0
        beq     .color_0
        cmp.b   #2,d0
        bne     .end
.color_1:
        moveq   #15,d0
.color_0:
        move.l  gbi_RegisterBase(a0),a0

        lsr.b   #2,d1
        lsr.b   #2,d2
        lsr.b   #2,d3
        move.b  d0,PALETTEADDRESS(a0)

        move.b  #TS_GraphicsCursorAttributes,TSI(a0)
        move.b  TSD(a0),d0
        or.b    #2,d0                           ; reveal extended DAC colors
        move.b  d0,TSD(a0)

        DELAY
        move.b  d1,PALETTEDATA(a0)
        nop
        DELAY
        move.b  d2,PALETTEDATA(a0)
        nop
        DELAY
        move.b  d3,PALETTEDATA(a0)
        nop

        and.b   #~2,d0                          ; hide extended DAC colors
        move.b  d0,TSD(a0)

.end:
        rts


*************************************
SetDAC:
* set sprite colors
* a0:   struct BoardInfo
* d7:   RGBFTYPE RGBFormat
*************************************
DACD    equ     $3c6

        cmp.l   #RGBFB_MaxFormats,d7
        bcc     .end
        cmp.l   #RGBFB_CLUT,d7
        bne     .not_clut
        move.l  gbi_ModeInfo(a0),a1
        btst    #GMB_DOUBLECLOCK,gmi_Flags(a1)
        beq     .not_doubleclocked
        move.b  #$4a,d0
        bra     .clock_doubled
.not_doubleclocked:
.not_clut:
        move.b  (.formats,pc,d7.l),d0
.clock_doubled:

        move.l  gbi_RegisterBase(a0),a0

        move.b  DACD(a0),d1
        DELAY
        move.b  #0,DACD(a0)
        DELAY
        tst.b   DACD(a0)
        DELAY
        tst.b   DACD(a0)
        DELAY
        tst.b   DACD(a0)
        DELAY
        tst.b   DACD(a0)
        DELAY
        move.b  d0,DACD(a0)     ; Hidden DAC
        DELAY
        move.b  d1,DACD(a0)     ; Pixel Mask
.end:   rts

.formats:
        dc.b    0,0,$e5,$e5,$e1,$e0
        dc.b    $e5,$e5,$e5,$e5,0,0
        dc.b    $e1,$e0
        dc.b    0,0,0,0,0,0,0

        cnop    0,4

*************************************
SetColorArray:
* set color palette
* a0:   struct BoardInfo
* d0.w: startindex
* d1.w: count
*************************************
        PUSH    d2/a6
        move.l  (gbi_ExecBase,a0),a6

        lea     gbi_CLUT(a0),a1
        moveq   #8,d2
        sub.w   gbi_BitsPerCannon(a0),d2
        move.l  gbi_RegisterBase(a0),a0

        DISABLE
        move.b  d0,PALETTEADDRESS(a0)

        add.w   d0,a1
        add.w   d0,a1
        add.w   d0,a1

        bra     .start

.loop:
        move.b  (a1)+,d0
        lsr.b   d2,d0
        move.b  d0,PALETTEDATA(a0)
        nop
        move.b  (a1)+,d0
        lsr.b   d2,d0
        move.b  d0,PALETTEDATA(a0)
        nop
        move.b  (a1)+,d0
        lsr.b   d2,d0
        move.b  d0,PALETTEDATA(a0)
        nop
.start:
        dbra    d1,.loop

        ENABLE
        POP     d2/a6
        rts


*************************************
SetDPMSLevel:
* sets degrees of power management
* a0:   struct BoardInfo
* d0.l: DPMS level
*************************************

        cmp.l   #DPMS_OFF,d0
        bgt     .end

        move.l  gbi_ModeInfo(a0),a1
        move.l  gbi_RegisterBase(a0),a0

        move.b  MISCOUTPUTR(a0),d1
        and.b   #%00111111,d1

        cmp.l   #DPMS_ON,d0
        bne     .special_polarity

        btst    #GMB_HPOLARITY,gmi_Flags(a1)
        beq     .not_horizontal
        bset    #6,d1
.not_horizontal:
        btst    #GMB_VPOLARITY,gmi_Flags(a1)
        beq     .not_vertical
        bset    #7,d1
.not_vertical:
.special_polarity:
        move.b  d1,MISCOUTPUTW(a0)

        move.b  #GDC_PowerManagement,GDCI(a0)
        move.b  GDCD(a0),d1
        and.b   #~6,d1
        or.b    (.table,pc,d0),d1
        move.b  d1,GDCD(a0)
.end:
        rts

.table:
        dc.b    0       ; DPMS_ON - Full operation
        dc.b    2       ; DPMS_STANDBY - Optional state of minimal power reduction
        dc.b    4       ; DPMS_SUSPEND - Significant reduction of power consumption
        dc.b    6       ; DPMS_OFF - Lowest level of power consumption


************************************************************************
WaitBlitter030:
* wait until blitter is ready (030 WriteAllocate Problem)
************************************************************************
* a0:   struct BoardInfo
************************************************************************
        PUSH    a2/a6
        movea.l gbi_ExecBase(a0),a6

        movea.l gbi_RegisterBase(a0),a0
        move.b  #GDC_BLTStartStatus,GDCI(a0)
        nop
        lea     GDCD(a0),a2
.wait:
        movea.l a2,a0
        moveq   #1,d0
        move.l  #CACRF_ClearD,d1
        CALL    CacheClearE
        btst    #3,(a2)
        bne     .wait

        POP     a2/a6
        rts


************************************************************************
WaitBlitter:
* wait until blitter is ready (020/040/060)
************************************************************************
* a0:   struct BoardInfo
************************************************************************

        movea.l gbi_RegisterBase(a0),a0
        WaitScreenBLT   a0,d0
        rts


************************************************************************
LargeBlit:
************************************************************************
* a0:   RegisterBase
* a1:   Destination Address (board relative)
* d2:   WORD width
* d3:   WORD height
* All registers are preserved
************************************************************************
        PUSH    d0-d3

        move.l  a1,d0
        SetBLTDstStartW d0,a0,d1
        move.w  d2,d0
        SetBLTWidthW    d0,a0,d1

        move.w  #MAX_BLTHEIGHT,d2       ; GD5434 maximum blit height
        cmp.w   d2,d3
        ble     .height_ok

        move.w  d2,d0
        SetBLTHeightW   d0,a0,d1

.loop:
        StartBLT        a0
        WaitScreenBLT   a0,d1

        sub.w   d2,d3
        cmp.w   d2,d3
        bgt     .loop

.height_ok:
        move.w  d3,d0
        SetBLTHeightW   d0,a0,d1
        StartBLT        a0

        POP     d0-d3
        rts


************************************************************************
BlitRect:
************************************************************************
* a0:   struct BoardInfo
* a1:   struct RenderInfo
* d0:   WORD SrcX
* d1:   WORD SrcY
* d2:   WORD DstX
* d3:   WORD DstY
* d4:   WORD SizeX
* d5:   WORD SizeY
* d6:   UBYTE Mask
* d7:   ULONG RGBFormat
************************************************************************
        PUSH    a2-a3/d2-d7

        cmp.l   #RGBFB_MaxFormats,d7
        bcc     .end

        movea.l a0,a3

        move.w  (.formats,pc,d7.l*2),d7
        jmp     (.formats,pc,d7.w)

.formats:
        dc.w    .end-.formats
        dc.w    .byte-.formats
        dc.w    .triplebyte-.formats
        dc.w    .triplebyte-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .doubleword-.formats
        dc.w    .doubleword-.formats
        dc.w    .doubleword-.formats
        dc.w    .doubleword-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .byte-.formats
        dc.w    .byte-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats

.doubleword:
        add.w   d0,d0
        add.w   d0,d0
        add.w   d2,d2
        add.w   d2,d2
        add.w   d4,d4
        add.w   d4,d4
        moveq   #-1,d6
        bra     .all_formats

.triplebyte:
        move.w  d0,d7
        add.w   d0,d0
        add.w   d7,d0
        move.w  d2,d7
        add.w   d2,d2
        add.w   d7,d2
        move.w  d4,d7
        add.w   d4,d4
        add.w   d7,d4
        moveq   #-1,d6
        bra     .all_formats

.word:
        add.w   d0,d0
        add.w   d2,d2
        add.w   d4,d4
        moveq   #-1,d6
        bra     .all_formats

.byte:
.all_formats:

        moveq   #0,d7

        cmp.w   d1,d3
        blt     .copybitmap_ok
        bgt     .copybitmap_add

        cmp.w   d0,d2
        blt     .copybitmap_ok

.copybitmap_add:
        add.w   d4,d0
        subq.w  #1,d0
        add.w   d5,d1
        subq.w  #1,d1
        add.w   d4,d2
        subq.w  #1,d2
        add.w   d5,d3
        subq.w  #1,d3
        moveq   #-1,d7

.copybitmap_ok:
        move.w  gri_BytesPerRow(a1),d7
        movea.l gri_Memory(a1),a1
        suba.l  gbi_MemoryBase(a3),a1   ; vga relative start address
        movea.l a1,a2
        adda.w  d0,a1
        muls    d7,d1
        adda.l  d1,a1                   ; Source start address
        adda.w  d2,a2
        muls    d7,d3
        adda.l  d3,a2                   ; Destination start address

        move.l  gbi_Flags(a0),d2
        and.l   #BIF_IGNOREMASK,d2
        bne     .no_mask

        cmp.b   #$ff,d6
        bne     .maskblit

.no_mask:
        movea.l gbi_RegisterBase(a3),a0
        moveq   #BM_STANDARD,d0
        tst.l   d7
        bpl     .mode_ok
        moveq   #BM_REVERSE,d0
.mode_ok:
        SetBLTModeW     d0,a0,d1
        move.w  d7,d0
        SetBLTSrcPitchW d0,a0,d1
        move.w  d7,d0
        SetBLTDstPitchW d0,a0,d1
        SetBLTROP       ROP_SRC,a0
        move.l  a1,d0
        SetBLTSrcStartW d0,a0,d1

        move.w  d4,d2
        move.w  d5,d3
        movea.l a2,a1
        bsr     LargeBlit
        bra     .end

.maskblit:
        PUSH    a4-a6

        movea.l gbi_MemoryBase(a3),a0
        adda.l  gbi_MemorySize(a3),a0
        lea     (rm_MiscBuffer,a0),a0
        moveq   #-1,d1
        move.l  d1,(a0)+
        move.l  d1,(a0)
        movea.l gbi_RegisterBase(a3),a0
        movea.l gbi_MemorySize(a3),a3
        lea     (rm_MiscBuffer,a3),a3
        SetGDCw d6,GDC_PixelFgColorByte0,d1

        move.l  a3,d2
        addq.l  #8,d2
        move.w  #GDC_BLTDstStartLow<<8,d1
        move.b  d2,d1
        movea.w d1,a4
        lsr.w   #8,d2
        move.w  #GDC_BLTDstStartMiddle<<8,d1
        move.b  d2,d1
        movea.w d1,a5
        swap    d2
        move.w  #GDC_BLTDstStartHigh<<8,d1
        move.b  d2,d1
        movea.w d1,a6

        move.l  #MISCBUFFERSIZE-8,d3
        divu    d4,d3                   ; Maxium blit height
        bvs     .cut
        cmp.w   #MAX_BLTHEIGHT,d3       ; GD5434 maxium blit height
        bls     .fits
.cut:
        move.w  #MAX_BLTHEIGHT,d3
.fits:

        tst.l   d7
        bpl     .incrementing
        suba.w  d4,a1
        addq.l  #1,a1
        adda.w  d7,a1
        suba.w  d4,a2
        addq.l  #1,a2
        adda.w  d7,a2
.incrementing:

        move.w  d4,d0
        SetBLTWidthW    d0,a0,d1
        SetBLTMode      BM_STANDARD,a0
        SetBLTROP       ROP_SRC,a0

        cmp.w   d3,d5                   ; Total height <= Maximum height?
        ble     .height_ok

        move.w  d7,d6
        mulu    d3,d6
        move.w  d3,d0
        SetBLTHeightW   d0,a0,d1
        bra     .y_loop

.y_loop_wait:
        WaitScreenBLT   a0,d1

.y_loop:
        tst.l   d7
        bpl     .pos
        suba.l  d6,a1
        suba.l  d6,a2
.pos:
        ; copy dst to temp
        move.w  d7,d0
        SetBLTSrcPitchW d0,a0,d1
        move.w  d4,d0
        SetBLTDstPitchW d0,a0,d1
        move.l  a2,d0
        SetBLTSrcStartW d0,a0,d1
        move.w  a4,GDCI(a0)
        move.w  a5,GDCI(a0)
        move.w  a6,GDCI(a0)             ; SetBLTDstStartW
        StartBLT        a0
        WaitScreenBLT   a0,d1

        ; neor src to temp
        move.l  a1,d0
        SetBLTSrcStartW d0,a0,d1
        move.w  a4,GDCI(a0)
        move.w  a5,GDCI(a0)
        move.w  a6,GDCI(a0)             ; SetBLTDstStartW
        SetBLTROP       ROP_NEOR,a0
        StartBLT        a0
        WaitScreenBLT   a0,d1

        ; or mask to temp
        move.l  a3,d0
        SetBLTSrcStartW d0,a0,d1
        move.w  a4,GDCI(a0)
        move.w  a5,GDCI(a0)
        move.w  a6,GDCI(a0)             ; SetBLTDstStartW
        SetBLTMode      BM_PATTERN|BM_EXPANSION,a0
        SetBLTROP       ROP_OR,a0
        StartBLT        a0
        WaitScreenBLT   a0,d1

        ; neor src to modified tmp
        move.l  a1,d0
        SetBLTSrcStartW d0,a0,d1
        move.w  a4,GDCI(a0)
        move.w  a5,GDCI(a0)
        move.w  a6,GDCI(a0)             ; SetBLTDstStartW
        SetBLTMode      BM_STANDARD,a0
        SetBLTROP       ROP_NEOR,a0
        StartBLT        a0
        WaitScreenBLT   a0,d1

        ; copy new dst back
        move.w  d4,d0
        SetBLTSrcPitchW d0,a0,d1
        move.w  d7,d0
        SetBLTDstPitchW d0,a0,d1
        move.l  a3,d0
        addq.l  #8,d0
        SetBLTSrcStartW d0,a0,d1
        move.l  a2,d0
        SetBLTDstStartW d0,a0,d1
        SetBLTROP       ROP_SRC,a0
        StartBLT        a0

        tst.l   d7
        bmi     .neg
        adda.l  d6,a1
        adda.l  d6,a2
.neg:

        sub.w   d3,d5                   ; Substract blit from rest height
        cmp.w   d3,d5                   ; Rest height >= blit height?
        bge     .y_loop_wait

        tst.w   d5
        ble     .done                   ; No rest height => All done

        WaitScreenBLT   a0,d1

.height_ok:
        move.w  d5,d3                   ; Blit height = Rest height
        move.w  d3,d0
        SetBLTHeightW   d0,a0,d1        ; Last chunk is smaller
        move.w  d7,d6
        mulu    d3,d6
        bra     .y_loop

.done:
        POP     a4-a6

.end:
        POP     a2-a3/d2-d7
        rts


************************************************************************

ROP_table:
        dc.b    ROP_FALSE       ; (-> FillRect)
        dc.b    ROP_NOR
        dc.b    ROP_ONLYDST     ; (~s /\ d)
        dc.b    ROP_NOTSRC
        dc.b    ROP_ONLYSRC     ; (s /\ ~d)
        dc.b    ROP_NOTDST      ; (-> InvertRect)
        dc.b    ROP_EOR
        dc.b    ROP_NAND
        dc.b    ROP_AND
        dc.b    ROP_NEOR
        dc.b    ROP_DST         ; (nop)
        dc.b    ROP_NOTONLYSRC  ; (~s \/ d)
        dc.b    ROP_SRC
        dc.b    ROP_NOTONLYDST  ; (s \/ ~d)
        dc.b    ROP_OR
        dc.b    ROP_TRUE        ; (-> FillRect)


************************************************************************
BlitRectNoMaskComplete:
************************************************************************
* a0:   struct BoardInfo
* a1:   struct RenderInfo *sri
* a2:   struct RenderInfo *dri
* d0:   WORD SrcX
* d1:   WORD SrcY
* d2:   WORD DstX
* d3:   WORD DstY
* d4:   WORD SizeX
* d5:   WORD SizeY
* d6:   UBYTE Opcode
* d7:   ULONG RGBFormat
************************************************************************
        PUSH    a2-a3/d2-d7

        cmp.l   #RGBFB_MaxFormats,d7
        bcc     .end

        move.l  a0,a3
        swap    d6                      ; opcode -> hiword

        move.w  (.formats,pc,d7.l*2),d7
        jmp     (.formats,pc,d7.w)

.formats:
        dc.w    .end-.formats
        dc.w    .byte-.formats
        dc.w    .triplebyte-.formats
        dc.w    .triplebyte-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .doubleword-.formats
        dc.w    .doubleword-.formats
        dc.w    .doubleword-.formats
        dc.w    .doubleword-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .byte-.formats
        dc.w    .byte-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats
        dc.w    .word-.formats

.triplebyte:
        move.w  d0,d7
        add.w   d0,d0
        add.w   d7,d0
        move.w  d2,d7
        add.w   d2,d2
        add.w   d7,d2
        move.w  d4,d7
        add.w   d4,d4
        add.w   d7,d4
        bra     .all_formats

.doubleword:
        add.w   d0,d0
        add.w   d2,d2
        add.w   d4,d4

.word:
        add.w   d0,d0
        add.w   d2,d2
        add.w   d4,d4
        bra     .all_formats

.byte:
;       bra     .all_formats

.all_formats:

        move.w  gri_BytesPerRow(a1),d6
        move.l  gri_Memory(a1),a1
        sub.l   gbi_MemoryBase(a3),a1   ; vga relative source address

        move.w  gri_BytesPerRow(a2),d7
        move.l  gri_Memory(a2),a2
        sub.l   gbi_MemoryBase(a3),a2   ; vga relative destination address

        cmp.l   a1,a2
        bne     .copybitmap_ok          ; simple case, different locations

        cmp.w   d1,d3
        blt     .copybitmap_ok
        bgt     .copybitmap_add

        cmp.w   d0,d2
        blt     .copybitmap_ok

.copybitmap_add:
        add.w   d5,d1
        subq.w  #1,d1

        add.w   d4,d0
        subq.w  #1,d0

        add.w   d0,a1
        muls    d6,d1
        add.l   d1,a1                   ; vga memory start address

        move.w  d3,d0
        add.w   d5,d0
        subq.w  #1,d0
        muls    d7,d0
        add.l   d0,a2
        add.w   d2,a2                   ; vga memory start address
        add.w   d4,a2
        sub.w   #1,a2

        moveq   #BM_REVERSE,d0          ; direction
        bra     .copycases_done

.copybitmap_ok:
        add.w   d0,a1
        muls    d6,d1
        add.l   d1,a1                   ; vga memory start address

        move.w  d3,d0
        muls    d7,d0
        add.l   d0,a2
        add.w   d2,a2                   ; vga memory start address

        moveq   #BM_STANDARD,d0         ; direction
.copycases_done:

        movea.l gbi_RegisterBase(a3),a0
        SetBLTModeW     d0,a0,d1
        move.w  d6,d0
        SetBLTSrcPitchW d0,a0,d1
        move.w  d7,d0
        SetBLTDstPitchW d0,a0,d1
        moveq   #0,d0
        swap    d6
        move.b  d6,d0
        move.b  (ROP_table,pc,d0.w),d0
        SetGDC  d0,GDC_BLTRasterOperation
        move.l  a1,d0
        SetBLTSrcStartW d0,a0,d1

        move.w  d4,d2
        move.w  d5,d3
        movea.l a2,a1
        bsr     LargeBlit

.end:
        POP     a2-a3/d2-d7
        rts


************************************************************************
FillRect:
************************************************************************
* a0:   struct BoardInfo
* a1:   struct RenderInfo
* d0:   WORD X
* d1:   WORD Y
* d2:   WORD Width
* d3:   WORD Height
* d4:   ULONG Pen
* d5:   UBYTE Mask
* d7:   ULONG RGBFormat
************************************************************************

        PUSH    d2/d4-d6/a2

        cmp.l   #RGBFB_MaxFormats,d7
        bcc     .fallback

        move.w  (.formats,pc,d7.l*2),d6
        jmp     (.formats,pc,d6.w)

.formats:
        dc.w    .fallback-.formats      ; NONE=PLANAR   [SD64] 
        dc.w    .byte-.formats          ; CLUT=CHUNKY   (SD64)
        dc.w    .triplebyte-.formats    ; R8G8B8        (SD64)
        dc.w    .triplebyte-.formats    ; B8G8R8        (Pixel64)
        dc.w    .word-.formats  ; R5G6B5PC      (Pixel64)
        dc.w    .word-.formats  ; R5G5B5PC      (Pixel64)
        dc.w    .fallback-.formats      ; A8R8G8B8
        dc.w    .fallback-.formats      ; A8B8G8R8
        dc.w    .doubleword-.formats    ; R8G8B8A8      (SD64)
        dc.w    .doubleword-.formats    ; B8G8R8A8      (Pixel64)
        dc.w    .fallback-.formats      ; R5G6B5
        dc.w    .fallback-.formats      ; R5G5B5
        dc.w    .word-.formats          ; B5G6R5PC      (SD64)
        dc.w    .word-.formats          ; B5G6R5PC      (SD64)
        dc.w    .fallback-.formats      ; YUV422CGX
        dc.w    .fallback-.formats      ; YUV411
        dc.w    .fallback-.formats      ; YUV411PC
        dc.w    .fallback-.formats      ; YUV422
        dc.w    .fallback-.formats      ; YUV422PC
        dc.w    .fallback-.formats      ; YUV422PA
        dc.w    .fallback-.formats      ; YUV422PAPC

****************************************

.fallback:
        POP     d2/d4-d6/a2
        move.l  gbi_FillRectDefault(a0),-(sp)
        rts

****************************************

.triplebyte:
        move.w  d0,d6
        add.w   d0,d0
        add.w   d6,d0
        move.w  d2,d6
        add.w   d2,d2
        add.w   d6,d2

        move.l  d4,d6
        lsr.l   #8,d6
        cmp.w   d4,d6                   ; R = G = B?
        bne     .special_blit

        move.l  #((BM_PATTERN|BM_EXPANSION)<<8)|ROP_SRC,d6
        bra     .start_blit             ; Fake 8bit blit

.special_blit:
        movea.l gri_Memory(a1),a2
        adda.w  d0,a2
        move.w  gri_BytesPerRow(a1),d0  ; Screen pitch
        muls    d0,d1
        adda.l  d1,a2                   ; Absolute startaddress
        movea.l a2,a1
        suba.l  gbi_MemoryBase(a0),a1   ; Relative startaddress

        move.l  d4,d1   ; -bgr
        rol.w   #8,d4   ; -brg
        swap    d4      ; rg-b
        lsl.w   #8,d4   ; rgb-
        move.b  d1,d4   ; rgbr
        move.l  d4,d5   ; rgbr
        lsl.l   #8,d5   ; gbr-
        lsr.l   #8,d1   ; --bg
        move.b  d1,d5   ; gbrg
        move.l  d5,d6   ; gbrg
        lsl.l   #8,d6   ; brg-
        lsr.l   #8,d1   ; ---b
        move.b  d1,d6   ; brgb

        move.w  d2,d1
        subq.w  #4,d1
        bmi     .rest3
        move.l  d4,(a2)+
        subq.w  #4,d1
        bmi     .rest2
        move.l  d5,(a2)+
        subq.w  #4,d1
        bmi     .rest1
        move.l  d6,(a2)+
        tst.w   d1
        beq     .start_special
        subq.w  #4,d1
        bmi     .rest3
        move.l  d4,(a2)+
        subq.w  #4,d1
        bmi     .rest2
        move.l  d5,(a2)+
        subq.w  #4,d1
        bmi     .rest1
        move.l  d6,(a2)+
        bra     .start_special

.rest3:
        swap    d4
        move.w  d4,(a2)+
        rol.l   #8,d4
        move.b  d4,(a2)
        bra     .start_special

.rest2:
        swap    d5
        move.w  d5,(a2)
        bra     .start_special

.rest1:
        rol.l   #8,d6
        move.b  d6,(a2)

.start_special:
        move.w  d1,d4                   ; Rest width
        subq.w  #1,d3                   ; Rest height
        bgt     .prepare_blit
        tst.w   d4
        bgt     .prepare_blit
        bra     .done                   ; No blits needed

.prepare_blit:
        movea.l a0,a2
        movea.l gbi_RegisterBase(a2),a0
        move.w  d0,d6                   ; Screen pitch
        SetBLTMode      BM_STANDARD,a0
        SetBLTROP       ROP_SRC,a0
        moveq   #0,d0
        SetBLTSrcPitchW d0,a0,d1        ; Repeat source pixels
        move.l  a1,d0
        SetBLTSrcStartW d0,a0,d1

        tst.w   d4
        ble     .rect_blit              ; Line complete

.line_blit:
        move.l  a1,d0
        add.l   #24,d0
        SetBLTDstStartW d0,a0,d1

        ext.l   d4
        divs.w  #24,d4
        tst.w   d4
        beq     .rest_blit              ; Rest width < 8 pixels

        moveq   #24,d0
        SetBLTDstPitchW d0,a0,d1
        moveq   #24,d0
        SetBLTWidthW    d0,a0,d1
        SetBLTHeightW   d4,a0,d1
        StartBLT        a0
        WaitScreenBLT   a0,d1

.rest_blit:
        swap    d4
        tst.w   d4
        beq     .rect_blit              ; Line complete

        SetBLTWidthW    d4,a0,d1
        moveq   #1,d0
        SetBLTHeightW   d0,a0,d1
        StartBLT        a0
        WaitScreenBLT   a0,d1

.rect_blit:
        tst.w   d3
        beq     .done                   ; Rest height = 0

        move.w  d6,d0
        SetBLTDstPitchW d0,a0,d1
        adda.w  d6,a1
        bsr     LargeBlit

.done:
        addq.w  #1,d3                   ; Original height
        bra     .end

****************************************

.doubleword:
        move.l  #((BM_PATTERN|BM_EXPANSION|BM_BPP32)<<8)|ROP_SRC,d6
        add.w   d0,d0
        add.w   d0,d0
        add.w   d2,d2
        add.w   d2,d2
        bra     .start_blit

****************************************

.word:
        move.l  #((BM_PATTERN|BM_EXPANSION|BM_BPP16)<<8)|ROP_SRC,d6
        add.w   d0,d0
        add.w   d2,d2
        bra     .start_blit

****************************************

.byte:
        move.l  #((BM_PATTERN|BM_EXPANSION)<<8)|ROP_SRC,d6

        cmp.b   #$ff,d5
        beq     .start_blit

        and.b   d5,d4
        beq     .clear_pen
        cmp.b   d5,d4
        beq     .solid_pen
        bra     .other_pen

.clear_pen:
        move.b  #ROP_ONLYDST,d6
        move.b  d5,d4
        bra     .start_blit

.solid_pen:
        move.b  #ROP_OR,d6
        bra     .start_blit

.other_pen:
        bset    #31,d6                  ; Set mask flag
        move.b  #ROP_OR,d6
;       bra     .start_blit

****************************************

.start_blit:
        movea.l gri_Memory(a1),a2
        adda.w  d0,a2
        move.w  gri_BytesPerRow(a1),d0  ; Screen pitch
        muls    d0,d1
        adda.l  d1,a2
        movea.l a2,a1
        movea.l gbi_MemoryBase(a0),a2
        suba.l  a2,a1                   ; Relative startaddress

        adda.l  gbi_MemorySize(a0),a2
        lea     (rm_MiscBuffer,a2),a2
        moveq   #-1,d1
        move.l  d1,(a2)+                ; Solid pattern for expansion
        move.l  d1,(a2)

        movea.l a0,a2
        movea.l gbi_RegisterBase(a0),a0

        SetBLTDstPitchW d0,a0,d1
        move.w  d6,d0
        lsr.w   #8,d0
        SetBLTModeW     d0,a0,d1
        move.l  gbi_MemorySize(a2),d0
        add.l   #rm_MiscBuffer,d0
        SetBLTSrcStartW d0,a0,d1

        tst.l   d6
        bpl     .main_blit

.mask_blit:
        SetBLTROP       ROP_ONLYDST,a0
        SetGDCw d5,GDC_PixelFgColorByte0,d1
        bsr     LargeBlit
        WaitScreenBLT   a0,d1

.main_blit:
        SetBLTROPW      d6,a0,d1

        btst    #4+8,d6                 ; TrueAlpha or HiColor?
        beq     .expand_byte
        btst    #5+8,d6                 ; TrueAlpha?
        beq     .expand_word

.expand_doubleword:
        SetGDCw d4,GDC_PixelFgColorByte3,d7
        lsr.l   #8,d4
        SetGDCw d4,GDC_PixelFgColorByte2,d7
        lsr.l   #8,d4

.expand_word:
        SetGDCw d4,GDC_PixelFgColorByte1,d1
        lsr.l   #8,d4

.expand_byte:
        SetGDCw d4,GDC_PixelFgColorByte0,d1

        bsr     LargeBlit

.end:
        POP     d2/d4-d6/a2
        rts


************************************************************************
InvertRect:
************************************************************************
* a0:   struct BoardInfo
* a1:   struct RenderInfo
* d0:   WORD X
* d1:   WORD Y
* d2:   WORD Width
* d3:   WORD Height
* d4:   UBYTE Mask
* d7:   ULONG RGBFormat
************************************************************************

        PUSH    d2/d4-d5/a2

        cmp.l   #RGBFB_MaxFormats,d7
        bcc     .fallback

        move.w  (.formats,pc,d7.l*2),d5
        jmp     (.formats,pc,d5.w)

.formats:
        dc.w    .fallback-.formats      ; NONE=PLANAR   [SD64] 
        dc.w    .byte-.formats          ; CLUT=CHUNKY   (SD64)
        dc.w    .triplebyte-.formats    ; R8G8B8        (SD64)
        dc.w    .triplebyte-.formats    ; B8G8R8        (Pixel64)
        dc.w    .word16-.formats        ; R5G6B5PC      (Pixel64)
        dc.w    .word15-.formats        ; R5G5B5PC      (Pixel64)
        dc.w    .fallback-.formats      ; A8R8G8B8
        dc.w    .fallback-.formats      ; A8B8G8R8
        dc.w    .doubleword-.formats    ; R8G8B8A8      (SD64)
        dc.w    .doubleword-.formats    ; B8G8R8A8      (Pixel64)
        dc.w    .fallback-.formats      ; R5G6B5
        dc.w    .fallback-.formats      ; R5G5B5
        dc.w    .word16-.formats        ; B5G6R5PC      (SD64)
        dc.w    .word15-.formats        ; B5G5R5PC      (SD64)
        dc.w    .fallback-.formats      ; YUV422CGX
        dc.w    .fallback-.formats      ; YUV411
        dc.w    .fallback-.formats      ; YUV411PC
        dc.w    .fallback-.formats      ; YUV422
        dc.w    .fallback-.formats      ; YUV422PC
        dc.w    .fallback-.formats      ; YUV422PA
        dc.w    .fallback-.formats      ; YUV422PAPC

****************************************

.fallback:
        POP     d2/d4-d5/a2
        move.l  gbi_InvertRectDefault(a0),-(sp)
        rts

****************************************

.triplebyte:
        move.w  d0,d5
        add.w   d0,d0
        add.w   d5,d0
        move.w  d2,d5
        add.w   d2,d2
        add.w   d5,d2

        move.w  #BM_PATTERN|BM_EXPANSION,d5
        moveq   #-1,d4
        bra     .start_blit             ; Fake 8bit color expansion blit

****************************************

.doubleword:
        add.w   d0,d0
        add.w   d0,d0
        add.w   d2,d2
        add.w   d2,d2
        
        move.w  #BM_PATTERN|BM_EXPANSION|BM_BPP32,d5
        move.l  #$ffffff00,d4
        bra     .start_blit

****************************************

.word15:
        move.w  #$ff7f,d4
        bra     .word

.word16:
        moveq   #-1,d4
;       bra     .word

.word:
        add.w   d0,d0
        add.w   d2,d2
        
        move.w  #BM_PATTERN|BM_EXPANSION|BM_BPP16,d5
        bra     .start_blit

****************************************

.byte:
        move.w  #BM_PATTERN|BM_EXPANSION,d5
;       bra     .start_blit

****************************************

.start_blit:
        movea.l gri_Memory(a1),a2
        adda.w  d0,a2
        move.w  gri_BytesPerRow(a1),d0  ; Screen pitch
        muls    d0,d1
        adda.l  d1,a2
        movea.l a2,a1
        movea.l gbi_MemoryBase(a0),a2
        suba.l  a2,a1                   ; Relative startaddress

        adda.l  gbi_MemorySize(a0),a2
        lea     (rm_MiscBuffer,a2),a2
        moveq   #-1,d1
        move.l  d1,(a2)+                ; Solid pattern for expansion
        move.l  d1,(a2)

        movea.l a0,a2
        movea.l gbi_RegisterBase(a0),a0

        SetBLTDstPitchW d0,a0,d1
        SetBLTROP       ROP_EOR,a0
        SetBLTModeW     d5,a0,d1
        move.l  gbi_MemorySize(a2),d0
        add.l   #rm_MiscBuffer,d0
        SetBLTSrcStartW d0,a0,d1

        btst    #4,d5                   ; TrueAlpha or HiColor?
        beq     .expand_byte
        btst    #5,d5                   ; TrueAlpha?
        beq     .expand_word

.expand_doubleword:
        SetGDCw d4,GDC_PixelFgColorByte3,d7
        lsr.l   #8,d4
        SetGDCw d4,GDC_PixelFgColorByte2,d7
        lsr.l   #8,d4

.expand_word:
        SetGDCw d4,GDC_PixelFgColorByte1,d1
        lsr.l   #8,d4

.expand_byte:
        SetGDCw d4,GDC_PixelFgColorByte0,d1

        bsr     LargeBlit

        POP     d2/d4-d5/a2
        rts


************************************************************************
BlitPattern:
************************************************************************
* a0:   struct BoardInfo
* a1:   struct RenderInfo
* a2:   struct Pattern
* d0:   WORD X
* d1:   WORD Y
* d2:   WORD Width
* d3:   WORD Height
* d4:   UBYTE Mask
* d7:   ULONG RGBFormat
************************************************************************

        cmpi.b  #3,pat_Size(a2)
        bgt     .fallback               ; Height > 8 not supported

        PUSH    d5-d6/a3

        move.l  pat_Memory(a2),a3
        move.b  pat_Size(a2),d5
        moveq   #1,d6
        lsl.l   d5,d6                   ; Height in lines (1, 2, 4 or 8)

.checkloop:
        move.b  (a3)+,d5
        cmp.b   (a3)+,d5
        bne     .fallback1              ; Real width > 8 not supported
        subq.l  #1,d6
        bgt     .checkloop

        cmp.l   #RGBFB_MaxFormats,d7
        bcc     .fallback

        jmp     ([.formats,pc,d7.l*4])
.formats:
        dc.l    .fallback1      ; NONE          [SD64]
        dc.l    .byte           ; CLUT          (SD64)
        dc.l    .fallback1      ; R8G8B8        [SD64]
        dc.l    .fallback1      ; B8G8R8        [Pixel64]
        dc.l    .word16         ; R5G6B5PC      (Pixel64)
        dc.l    .word15pc               ; R5G5B5PC      (Pixel64)
        dc.l    .doubleword32axxx       ; A8R8G8B8
        dc.l    .doubleword32axxx       ; A8B8G8R8
        dc.l    .doubleword32xxxa       ; R8G8B8A8      (SD64)
        dc.l    .doubleword32xxxa       ; B8G8R8A8      (Pixel64)
        dc.l    .word16         ; R5G6B5
        dc.l    .word15         ; R5G5B5
        dc.l    .word16         ; B5G6R5PC      (SD64)
        dc.l    .word15pc               ; B5G5R5PC      (SD64)
        dc.l    .word16         ; YUV422CGX
        dc.l    .byte           ; YUV411
        dc.l    .byte           ; YUV411PC
        dc.l    .word16         ; YUV422
        dc.l    .word16         ; YUV422PC
        dc.l    .word16         ; YUV422PAPC
        dc.l    .word16         ; YUV422PAPC

.fallback1:
        POP     d5-d6/a3

.fallback:
        move.l  gbi_BlitPatternDefault(a0),-(sp)
        rts

*************************************
.doubleword32axxx:
        PUSH    d2-d4/d7/a4-a6
*************************************
        move.l  #$00ffffff,d6
        bra     .doubleword

*************************************
.doubleword32xxxa:
        PUSH    d2-d4/d7/a4-a6
*************************************
        move.l  #$ffffff00,d6
;       bra     .doubleword

.doubleword:
        add.w   d0,d0
        add.w   d0,d0
        add.w   d2,d2
        add.w   d2,d2
        move.w  #BM_PATTERN|BM_EXPANSION|BM_BPP32,d7
        bra     .all_formats

*************************************
.word15:
        PUSH    d2-d4/d7/a4-a6
*************************************
        move.l  #$7fff7fff,d6
        bra     .word

*************************************
.word15pc:
        PUSH    d2-d4/d7/a4-a6
*************************************
        move.l  #$ff7fff7f,d6
        bra     .word

*************************************
.word16:
        PUSH    d2-d4/d7/a4-a6
*************************************
        move.l  #$ffffffff,d6
;       bra     .word

.word:
        add.w   d0,d0
        add.w   d2,d2
        move.w  #BM_PATTERN|BM_EXPANSION|BM_BPP16,d7
        bra     .all_formats

*************************************
.byte:
        PUSH    d2-d4/d7/a4-a6
*************************************
        moveq   #0,d6
        move.b  d4,d6
        move.w  #BM_PATTERN|BM_EXPANSION,d7
;       bra     .all_formats

.all_formats:
        movea.l a0,a3
        move.w  gri_BytesPerRow(a1),d4
        movea.l gri_Memory(a1),a6
        movea.l gbi_RegisterBase(a3),a0
        movea.l gbi_MemoryBase(a3),a4
        suba.l  a4,a6
        movea.l gbi_MemorySize(a3),a3
        lea     (rm_MiscBuffer,a3),a3
        adda.l  a3,a4
        mulu    d4,d1
        adda.l  d1,a6
        adda.w  d0,a6                   ; Destination start address

        SetBLTDstPitchW d4,a0,d0        ; Not modified by blitter operation

        move.b  pat_DrawMode(a2),d5     ; DrawMode -> d5.w
        swap    d5
        bclr    #31,d5                  ; Clear extra-run flag
        bclr    #30,d5                  ; Clear extra-pattern flag

        btst    #1+16,d5                ; COMPLEMENT
        beq     .not_complement

.complement:
        SetBLTROP       ROP_EOR,a0
        move.l  d6,d0
        moveq   #0,d1
        btst    #0+16,d5                ; JAM2
        beq     .start_expansion
        move.l  d6,d1
        bra     .start_expansion

.not_complement:
        btst    #4,d7
        bne     .no_mask                ; 16 or 32 bpp
        cmp.b   #$ff,d6
        beq     .no_mask                ; 8 bpp, but no zeros in mask

.mask:
        btst    #0+16,d5                ; JAM2
        bne     .mask_jam2

.mask_jam1:
        move.l  pat_FgPen(a2),d0
        and.l   d6,d0
        moveq   #0,d1

        tst.l   d0
        beq     .clear_jam1
        cmp.l   d6,d0
        beq     .solid_jam1
        bra     .other_jam1

.clear_jam1:
        move.l  d6,d0
        SetBLTROP       ROP_ONLYDST,a0
        bra     .start_byte

.solid_jam1:
        move.l  d6,d0
        SetBLTROP       ROP_OR,a0
        bra     .start_byte

.other_jam1:
        bset    #31,d5                  ; Set extra-run flag
        swap    d0
        or.l    d0,d6
        bra     .startblit

.mask_jam2:
        bset    #31,d5                  ; Set extra-run flag
        move.l  pat_FgPen(a2),d0
        and.l   d6,d0
        move.l  pat_BgPen(a2),d1
        and.l   d6,d1

        tst.l   d0
        beq     .clear_jam2
        cmp.l   d6,d0
        beq     .solid_jam2
        bra     .other_jam2

.clear_jam2:
        cmp.l   d6,d1
        bne     .other_jam2

        move.l  d6,d0
        lsl.l   #8,d0
        swap    d0
        or.l    d0,d6
        bra     .startblit

.solid_jam2:
        tst.l   d1
        bne     .other_jam2

        move.l  d6,d0
        lsl.l   #8,d6
        swap    d0
        or.l    d0,d6
        bra     .startblit

.other_jam2:
        bset    #30,d5                  ; Set extra-pattern flag
        moveq   #-1,d4                  ; Solid pattern for clear run
        move.l  d4,(a4)+                ; Write extra pattern
        move.l  d4,(a4)+
        lsl.w   #8,d1
        move.b  d0,d1
        swap    d1
        or.l    d1,d6
        bra     .startblit

.no_mask:
        SetBLTROP       ROP_SRC,a0
        btst    #0+16,d5                ; JAM2
        beq     .jam1

.jam2:
        move.l  pat_FgPen(a2),d0
        move.l  pat_BgPen(a2),d1
        and.l   d6,d0
        and.l   d6,d1
        bra     .start_expansion

.jam1:
        or.w    #BM_TRANSPARENCY,d7
        move.l  pat_FgPen(a2),d0
        and.l   d6,d0
        btst    #4,d7
        beq     .duplicate_byte         ; 8 bpp
        btst    #5,d7
        beq     .duplicate_word         ; 16 bpp
        bra     .fullcolor              ; 32 bpp

.duplicate_byte:
        move.b  d0,d4
        lsl.l   #8,d0
        move.b  d4,d0

.duplicate_word:
        move.w  d0,d4
        swap    d0
        move.w  d4,d0

.fullcolor:
        move.l  d0,d1
        not.l   d1
        bra     .start_doubleword       ; Set all 8 color registers

.start_expansion:
        btst    #4,d7
        beq     .start_byte             ; 8 bpp
        btst    #5,d7
        beq     .start_word             ; 16 bpp

.start_doubleword:
        SetGDCw d0,GDC_PixelFgColorByte3,d4
        SetGDCw d1,GDC_PixelBgColorByte3,d4
        lsr.l   #8,d0
        lsr.l   #8,d1
        SetGDCw d0,GDC_PixelFgColorByte2,d4
        SetGDCw d1,GDC_PixelBgColorByte2,d4
        lsr.l   #8,d0
        lsr.l   #8,d1

.start_word:
        SetGDCw d0,GDC_PixelFgColorByte1,d4
        SetGDCw d1,GDC_PixelBgColorByte1,d4
        lsr.l   #8,d0
        lsr.l   #8,d1

.start_byte:
        SetGDCw d0,GDC_PixelFgColorByte0,d4
        SetGDCw d1,GDC_PixelBgColorByte0,d4

.startblit:
        SetBLTModeW     d7,a0,d4

        movea.l pat_Memory(a2),a1
        move.b  pat_Size(a2),d1
        move.l  #$00010001,d4
        lsl.l   d1,d4                   ; Height in lines (two copies)

        move.w  d4,d5
        lsl.w   #1,d5
        subq.w  #1,d5                   ; Overflow mask

        swap    d2                      ; Save width
        swap    d3                      ; Save height
        move.w  pat_XOffset(a2),d2
        move.w  pat_YOffset(a2),d3
        and.w   #$0007,d2               ; XOffset in bits
        lsl.w   #1,d3
        and.w   d5,d3                   ; YOffset in source bytes
        
        cmp.w   #8,d4                   ; Maximum height
        blt     .looping2               ; Height is 4, 2 or 1 lines

.looping1:
        lsl.l   #8,d0                   ; Make room in 1st pattern longword
        move.b  (a1,d3.w),d0            ; Read one line of eight pixels
        rol.b   d2,d0                   ; Rotate by XOffset
        addq.w  #2,d3                   ; Move to next source line
        and.w   d5,d3                   ; Reset to first line on overflow
        subq.w  #1,d4
        cmp.w   #4,d4
        bgt     .looping1

.looping2:
        lsl.l   #8,d1                   ; Make room in 2nd pattern longword
        move.b  (a1,d3.w),d1            ; Read one line of eight pixels
        rol.b   d2,d1                   ; Rotate by XOffset
        addq.w  #2,d3                   ; Move to next source line
        and.w   d5,d3                   ; Reset to first line on overflow
        subq.w  #1,d4
        bgt     .looping2

        swap    d4                      ; Restore height
        cmp.w   #8,d4
        beq     .complete
        cmp.w   #4,d4
        beq     .duplicate_pattern_long
        cmp.w   #2,d4
        beq     .duplicate_pattern_word
        
.duplicate_pattern_byte:
        move.b  d1,d0
        lsl.w   #8,d1
        move.b  d0,d1

.duplicate_pattern_word:
        move.w  d1,d0
        swap    d1
        move.w  d0,d1

.duplicate_pattern_long:
        move.l  d1,d0

.complete:
        btst    #2+16,d5                ; INVERSVID
        beq     .write_pattern
        not.l   d0                      ; Invert pattern
        not.l   d1

.write_pattern:
        move.l  d0,(a4)+                ; Write main pattern
        move.l  d1,(a4)+

        swap    d2                      ; Width
        swap    d3                      ; Height
        movea.l a6,a1                   ; Destination

        btst    #31,d5                  ; Check extra-run flag
        beq     .main_blit

        SetGDCw d6,GDC_PixelFgColorByte0,d4
        ror.l   #8,d6
        SetGDCw d6,GDC_PixelBgColorByte0,d4
        ror.l   #8,d6
        SetBLTROP       ROP_ONLYDST,a0
        move.l  a3,d0
        SetBLTSrcStartW d0,a0,d4
        bsr     LargeBlit
        WaitScreenBLT   a0,d4

        SetGDCw d6,GDC_PixelFgColorByte0,d4
        ror.l   #8,d6
        SetGDCw d6,GDC_PixelBgColorByte0,d4
        ror.l   #8,d6
        SetBLTROP       ROP_OR,a0
        btst    #30,d5
        beq     .main_blit
        addq.l  #8,a3

.main_blit:
        move.l  a3,d0
        SetBLTSrcStartW d0,a0,d4
        bsr     LargeBlit

        POP     d2-d4/d7/a4-a6
        POP     d5-d6/a3
        rts


************************************************************************
BlitTemplate:
************************************************************************
* a0:   struct BoardInfo
* a1:   struct RenderInfo
* a2:   struct Template
* d0:   WORD X
* d1:   WORD Y
* d2:   WORD Width
* d3:   WORD Height
* d4:   UBYTE Mask
* d7:   ULONG RGBFormat
************************************************************************

        cmp.l   #RGBFB_MaxFormats,d7
        bcc     .fallback

        jmp     ([.formats,pc,d7.l*4])
.formats:
        dc.l    .fallback               ; NONE          [SD64]
        dc.l    .byte           ; CLUT          (SD64)
        dc.l    .fallback               ; R8G8B8        [SD64]
        dc.l    .fallback               ; B8G8R8        [Pixel64]
        dc.l    .word16         ; R5G6B5PC      (Pixel64)
        dc.l    .word15pc               ; R5G5B5PC      (Pixel64)
        dc.l    .doubleword32axxx       ; A8R8G8B8
        dc.l    .doubleword32axxx       ; A8B8G8R8
        dc.l    .doubleword32xxxa       ; R8G8B8A8      (SD64)
        dc.l    .doubleword32xxxa       ; B8G8R8A8      (Pixel64)
        dc.l    .word16         ; R5G6B5
        dc.l    .word15         ; R5G5B5
        dc.l    .word16         ; B5G6R5PC      (SD64)
        dc.l    .word15pc               ; B5G5R5PC      (SD64)
        dc.l    .word16         ; YUV422CGX
        dc.l    .byte           ; YUV411
        dc.l    .byte           ; YUV411PC
        dc.l    .word16         ; YUV422
        dc.l    .word16         ; YUV422PC
        dc.l    .word16         ; YUV422PAPC
        dc.l    .word16         ; YUV422PAPC

.fallback:
        move.l  gbi_BlitTemplateDefault(a0),-(sp)
        rts

*************************************
.doubleword32axxx:
        PUSH    a3-a6/d2-d7
*************************************
        move.l  #$00ffffff,d4
        bra     .doubleword

*************************************
.doubleword32xxxa:
        PUSH    a3-a6/d2-d7
*************************************
        move.l  #$ffffff00,d4
;       bra     .doubleword

.doubleword:
        move.w  d2,d5
        add.w   d0,d0
        add.w   d0,d0
        add.w   d5,d5
        add.w   d5,d5
        move.w  #BM_EXPANSION|BM_BPP32,d7
        bra     .all_formats

*************************************
.word15:
        PUSH    a3-a6/d2-d7
*************************************
        move.l  #$00007fff,d4
        bra     .word

*************************************
.word15pc:
        PUSH    a3-a6/d2-d7
*************************************
        move.l  #$0000ff7f,d4
        bra     .word

*************************************
.word16:
        PUSH    a3-a6/d2-d7
*************************************
        move.l  #$0000ffff,d4
;       bra     .word

.word:
        move.w  d2,d5
        add.w   d0,d0
        add.w   d5,d5
        move.w  #BM_EXPANSION|BM_BPP16,d7
        bra     .all_formats

*************************************
.byte:
        PUSH    a3-a6/d2-d7
*************************************
        and.l   #$000000ff,d4
        move.w  d2,d5
        move.w  #BM_EXPANSION,d7
;       bra     .all_formats

.all_formats:
        and.l   #$0000ffff,d3           ; Clear flags
        move.l  gbi_Flags(a0),d6
        btst    #BIB_SYSTEM2SCREENBLITS,d6
        beq     .check_done
        bset    #17,d3                  ; Set system-to-screen flag

.check_done:
        move.w  gri_BytesPerRow(a1),d6
        movea.l gri_Memory(a1),a6
        movea.l gbi_MemoryBase(a0),a4
        suba.l  a4,a6
        movea.l gbi_MemorySize(a0),a3
        lea     (rm_MiscBuffer,a3),a3
        addq.l  #8,a3
        adda.l  a3,a4
        mulu    d6,d1
        adda.l  d1,a6
        adda.w  d0,a6                   ; Destination start address
        movea.w d6,a5
        movea.l gbi_RegisterBase(a0),a0

        SetBLTDstPitchW d6,a0,d0

        add.w   #7,d2
        lsr.w   #3,d2                   ; Translate width in bitmap bytes

        swap    d2
        move.b  tmp_DrawMode(a2),d2     ; DrawMode -> d2.b
        lsl.w   #8,d2
        move.b  tmp_XOffset(a2),d2      ; XOffset -> d2.b
        swap    d2
        and.l   #$3fffffff,d2           ; Abuse two DrawMode bits as flags

        SetBLTWidthW    d5,a0,d0

        btst    #1+24,d2                ; COMPLEMENT
        beq     .not_complement

.complement:
        SetBLTROP       ROP_EOR,a0
        move.l  d4,d0
        moveq   #0,d1                   ; Complement is always jam1
        bra     .all_drawmodes

.not_complement:
        btst    #4,d7
        bne     .no_mask                ; 16 bpp
        btst    #5,d7
        bne     .no_mask                ; 32 bpp
        cmp.b   #$ff,d4
        beq     .no_mask                ; 8 bpp, but no zeros in mask

.mask:
        btst    #0+24,d2                ; JAM2
        bne     .mask_jam2

.mask_jam1:
        move.l  tmp_FgPen(a2),d0
        and.l   d4,d0
        moveq   #0,d1

        tst.l   d0
        beq     .clear_jam1
        cmp.l   d4,d0
        beq     .solid_jam1
        bra     .other_jam1

.clear_jam1:
        move.l  d4,d0
        SetBLTROP       ROP_ONLYDST,a0
        bra     .all_drawmodes

.solid_jam1:
        move.l  d4,d0
        SetBLTROP       ROP_OR,a0
        bra     .all_drawmodes

.other_jam1:
        bset    #31,d2                  ; Set extra-run flag
        bra     .all_drawmodes

.mask_jam2:
        bset    #31,d2                  ; Set extra-run flag
        move.l  tmp_FgPen(a2),d0
        and.l   d4,d0
        move.l  tmp_BgPen(a2),d1
        and.l   d4,d1

        tst.l   d0
        beq     .clear_jam2
        cmp.l   d4,d0
        beq     .solid_jam2
        bra     .other_jam2

.clear_jam2:
        cmp.l   d4,d1
        bne     .other_jam2

        moveq   #0,d0
        move.l  d4,d1
        bra     .all_drawmodes

.solid_jam2:
        tst.l   d1
        bne     .other_jam2

        move.l  d4,d0
        moveq   #0,d1
        lsl.l   #8,d4
        bra     .all_drawmodes

.other_jam2:
        bset    #30,d2                  ; Set pattern-clear flag
        moveq   #-1,d5                  ; Solid pattern for extra run
        subq.l  #8,a4
        move.l  d5,(a4)+                ; Write pattern
        move.l  d5,(a4)+
        bra     .all_drawmodes

.no_mask:
        SetBLTROP       ROP_SRC,a0
        btst    #0+24,d2                ; JAM2
        beq     .jam1

.jam2:
        move.l  tmp_FgPen(a2),d0
        and.l   d4,d0
        move.l  tmp_BgPen(a2),d1
        and.l   d4,d1
        bra     .all_drawmodes

.jam1:
        or.b    #BM_TRANSPARENCY,d7
        move.l  tmp_FgPen(a2),d0
        and.l   d4,d0

        btst    #5,d7
        bne     .fullcolor              ; 32 bpp
        btst    #4,d7
        bne     .duplicate_word         ; 16 bpp

.duplicate_byte:
        move.b  d0,d5
        lsl.l   #8,d0
        move.b  d5,d0

.duplicate_word:
        move.w  d0,d5
        swap    d0
        move.w  d5,d0

.fullcolor:
        move.l  d0,d1
        not.l   d1
        bra     .set_high               ; Set all 8 color registers

.all_drawmodes:
        btst    #4,d7
        beq     .save_low               ; 8 bpp
        btst    #5,d7
        beq     .set_middle             ; 16 bpp

.set_high:
        SetGDCw d0,GDC_PixelFgColorByte3,d5
        lsr.l   #8,d0
        SetGDCw d1,GDC_PixelBgColorByte3,d5
        lsr.l   #8,d1
        SetGDCw d0,GDC_PixelFgColorByte2,d5
        lsr.l   #8,d0
        SetGDCw d1,GDC_PixelBgColorByte2,d5
        lsr.l   #8,d1

.set_middle:
        SetGDCw d0,GDC_PixelFgColorByte1,d5
        lsr.l   #8,d0
        SetGDCw d1,GDC_PixelBgColorByte1,d5
        lsr.l   #8,d1

.save_low:
        swap    d4
        move.b  d1,d4
        lsl.w   #8,d4
        move.b  d0,d4

        SetBLTDstStartW a6,a0,d5

        move.w  tmp_BytesPerRow(a2),d1
        movea.l tmp_Memory(a2),a1

        btst    #2+24,d2                ; INVERSVID
        beq     .not_invers
        bset    #16,d3

.not_invers:
        btst    #17,d3                  ; Check system-to-screen flag
        beq     .calc
        or.w    #BM_SYSTEM,d7
        bra     .cut

.calc
        move.l  #MISCBUFFERSIZE-8,d5
        divu    d2,d5                   ; Maximum blit height
        bvs     .cut
        cmp.w   #MAX_BLTHEIGHT,d5       ; Chip limit
        bls     .fits
.cut:
        move.w  #MAX_BLTHEIGHT,d5
.fits:

        SetBLTModeW     d7,a0,d6

        swap    d7
        move.w  d3,d7                   ; Rest height = Total height -> d7.w
        move.w  d5,d3                   ; Blit height = Maximum height -> d3

        move.w  a5,d5
        mulu    d3,d5
        movea.l d5,a5                   ; Maximum blit offset

        cmp.w   d3,d7                   ; Total height <= Maximum height?
        ble     .height_ok              ; Process in one single run

        SetBLTHeightW   d3,a0,d5

.loop:
        btst    #17,d3                  ; Check system-to-screen flag
        bne     .do_blit

        move.l  d2,d0
        swap    d0                      ; XOffset
        exg     a4,a0                   ; Video memory address
        bsr     ExpansionWrite
        exg     a0,a4

.do_blit:
        tst.l   d2                      ; Check extra-run flag
        bpl     .main_blit

.mask_blit:
        swap    d4
        SetGDCw d4,GDC_PixelFgColorByte0,d5
        ror.l   #8,d4
        SetGDCw d4,GDC_PixelBgColorByte0,d5
        ror.l   #8,d4
        SetBLTROP       ROP_ONLYDST,a0

        btst    #30,d2
        beq     .expansion_blit

.pattern_blit:
        move.l  a3,d0
        subq.l  #8,d0
        SetBLTSrcStartW d0,a0,d5
        SetBLTMode      BM_PATTERN|BM_EXPANSION,a0

        StartBLT        a0
        WaitScreenBLT   a0,d5

        swap    d7
        SetBLTModeW     d7,a0,d5
        swap    d7
        bra     .mask_blit_done

.expansion_blit:
        SetBLTSrcStartW a3,a0,d5

        StartBLT        a0
        btst    #17,d3                  ; Check system-to-screen flag
        beq     .wait_mask

        move.l  d2,d0
        swap    d0                      ; XOffset
        exg     a4,a0                   ; Video memory address
        bsr     ExpansionWrite
        exg     a0,a4
        WaitSystemBLT   a0,d5
        bra     .mask_blit_done

.wait_mask:
        WaitScreenBLT   a0,d5

.mask_blit_done:
        SetBLTROP       ROP_OR,a0
        SetBLTDstStartW a6,a0,d5        ; Reset startaddress for second run

.main_blit:
        SetGDCw d4,GDC_PixelFgColorByte0,d5
        ror.w   #8,d4
        SetGDCw d4,GDC_PixelBgColorByte0,d5
        ror.w   #8,d4
        SetBLTSrcStartW a3,a0,d5

        StartBLT        a0
        btst    #17,d3                  ; Check system-to-screen flag
        beq     .wait_main

        move.l  d2,d0
        swap    d0                      ; XOffset
        exg     a4,a0                   ; Video memory address
        bsr     ExpansionWrite
        exg     a0,a4
        WaitSystemBLT   a0,d5
        bra     .main_blit_done

.wait_main:
        WaitScreenBLT   a0,d5

.main_blit_done:
        adda.l  a5,a6
        move.w  d1,d5
        mulu    d3,d5
        adda.l  d5,a1                   ; Shift source address

        sub.w   d3,d7                   ; Substract blit from rest height
        cmp.w   d3,d7                   ; Rest height >= Blit height?
        bge     .loop

        tst.w   d7
        ble     .end                    ; Exit

.height_ok:
        move.w  d7,d3                   ; Blit height = rest height
        SetBLTHeightW   d3,a0,d5
        bra     .loop

.end:
        POP     a3-a6/d2-d7
        rts


************************************************************************


DepthMask_table:
        dc.b    %00000000
        dc.b    %00000001
        dc.b    %00000011
        dc.b    %00000111
        dc.b    %00001111
        dc.b    %00011111
        dc.b    %00111111
        dc.b    %01111111
        dc.b    %11111111,0,0,0

        BITDEF  OP,CLEARBEFORE,15
        BITDEF  OP,INVERTBEFORE,14
        BITDEF  OP,INVERTAFTER,13
        BITDEF  OP,AND,12

Opcode_table:
        dc.w    -1                              ; ROP_FALSE (-> FillRect)
        dc.w    ROP_OR|OPF_INVERTAFTER          ; ROP_NOR
        dc.w    ROP_ONLYDST                     ; ROP_ONLYDST (~s /\ d)
        dc.w    ROP_NOTONLYSRC|OPF_AND|OPF_CLEARBEFORE  ; ROP_NOTSRC
        dc.w    ROP_AND|OPF_AND|OPF_INVERTBEFORE        ; ROP_ONLYSRC (s /\ ~d)
        dc.w    -1                              ; ROP_NOTDST (-> InvertRect)
        dc.w    ROP_EOR                         ; ROP_EOR
        dc.w    ROP_AND|OPF_AND|OPF_INVERTAFTER ; ROP_NAND
        dc.w    ROP_AND|OPF_AND                 ; ROP_AND
        dc.w    ROP_NEOR|OPF_AND                ; ROP_NEOR
        dc.w    -1                              ; ROP_DST (nop)
        dc.w    ROP_NOTONLYSRC|OPF_AND          ; ROP_NOTONLYSRC (~s \/ d)
        dc.w    ROP_OR|OPF_CLEARBEFORE          ; ROP_SRC
        dc.w    ROP_OR|OPF_INVERTBEFORE         ; ROP_NOTONLYDST (s \/ ~d)
        dc.w    ROP_OR                          ; ROP_OR
        dc.w    -1                              ; ROP_TRUE (-> FillRect)


************************************************************************
BlitPlanar2Chunky:
************************************************************************
* a0    APTR BoardInfo
* a1    APTR BitMap
* a2    APTR RenderInfo
* d0    WORD SrcX,
* d1    WORD SrcY,
* d2    WORD DstX,
* d3    WORD DstY,
* d4    WORD SizeX,
* d5    WORD SizeY,
* d6    UBYTE MinTerm);
* d7    UBYTE Mask);
************************************************************************

PLANARBUFFER    equ     (MISCBUFFERSIZE-16)/9/8*8
CHUNKYBUFFER    equ     PLANARBUFFER*8

        PUSH    d2-d7/a2-a6
        movea.l a1,a5                   ; BitMap -> a5
        movea.l gbi_MemorySize(a0),a4
        lea     (rm_MiscBuffer,a4),a4
        movea.l gbi_MemoryBase(a0),a3
        movea.l gri_Memory(a2),a6       ; Display address
        suba.l  a3,a6
        adda.w  d2,a6
        move.w  gri_BytesPerRow(a2),d2
        mulu    d2,d3
        adda.l  d3,a6                   ; Blit Start Address -> a6

        moveq   #0,d3                   ; Clear flags -> d3.w
;       move.b  d7,d3
;       ror.l   #8,d3                   ; full mask (not anded with bm->Depth value)

        move.l  gbi_Flags(a0),d2
        btst    #BIB_SYSTEM2SCREENBLITS,d2
        beq     .check_done
        bset    #17,d3                  ; Set system-to-screen flag

.check_done:
        move.w  d4,d2                   ; Width -> d2.w
        move.w  d5,d3                   ; Height -> d3.w

        adda.l  a3,a4                   ; Absolute pattern address
        moveq   #-1,d4
        move.l  d4,(a4)+                ; Solid pattern
        move.l  d4,(a4)+
        moveq   #0,d4
        move.l  d4,(a4)+                ; Empty pattern
        move.l  d4,(a4)+
        suba.l  a3,a4                   ; Relative chunky buffer address

        movea.l gbi_RegisterBase(a0),a0
        SetBLTWidthW    d2,a0,d5        ; Same for all blits
        SetBLTROP       ROP_SRC,a0      ; For first chunk
        SetBLTMode      BM_STANDARD,a0

        add.w   #7,d2
        lsr.w   #3,d2                   ; Translate into bitmap bytes

        move.w  d0,d4
        move.w  d1,d5
        and.w   #$000f,d0               ; XOffset -> d0.b [7:0]
        swap    d0

        move.w  gri_BytesPerRow(a2),d1  ; Screen BytesPerRow -> d1.w
        swap    d1
        move.w  bm_BytesPerRow(a5),d1   ; Bitmap BytesPerRow -> d1.w

        muls    d1,d5
        lsr.w   #4,d4
        movea.l d5,a2
        adda.w  d4,a2
        adda.w  d4,a2                   ; Bitplane start offset -> a2

        moveq   #0,d4
        move.b  bm_Depth(a5),d4
        and.b   (DepthMask_table,pc,d4.w),d7

        swap    d7
        ext.w   d6
        move.w  (Opcode_table,pc,d6.w*2),d7
        cmp.w   #-1,d7
        beq     .end
        swap    d7

        move.b  d7,d4
        lsl.w   #8,d7
        move.b  d4,d7                   ; Backup mask -> d7.b

        lea     bm_Planes(a5),a5

        move.l  #PLANARBUFFER,d4
        divu    d2,d4                   ; Maximum blit height
        bvs     .cut
        cmp.w   #MAX_BLTHEIGHT,d4       ; GD542X maximum blit height
        bls     .fits
.cut:
        move.w  #MAX_BLTHEIGHT,d4
.fits:

        swap    d2
        move.w  d3,d2                   ; Rest height = Total height -> d2.w
        move.w  d4,d3                   ; Blit height = Maximum height -> d3.w

        cmp.w   d3,d2                   ; Total height <= Maximum height?
        ble     .height_ok              ; Process in one single run

        SetBLTHeightW   d3,a0,d5        ; Chunk height
        bra     .blit_chunks_loop

.blit_chunks_loop_wait:
        WaitScreenBLT   a0,d5

.blit_chunks_loop:
        swap    d2                      ; Blit width in bytes
        move.w  d7,d4
        lsr.w   #8,d4
        move.b  d4,d7                   ; Restore mask

        SetBLTSrcStartW a6,a0,d5
        move.l  d1,d4
        swap    d4
        SetBLTSrcPitchW d4,a0,d5        ; Relevant for copy blit only
        SetBLTDstStartW a4,a0,d5
        move.w  d2,d4
        lsl.w   #3,d4                   ; Buffer pitch
        SetBLTDstPitchW d4,a0,d5        ; Same for all blits but the last
        StartBLT        a0
        WaitScreenBLT   a0,d5

        btst    #OPB_CLEARBEFORE+16,d7
        bne     .clear_before

        btst    #OPB_INVERTBEFORE+16,d7
        beq     .nothing_before

.invert_before:
        SetBLTROP       ROP_EOR,a0
        bra     .do_before

.clear_before:
        SetBLTROP       ROP_ONLYDST,a0

.do_before:
;       move.l  d3,d4
;       rol.l   #8,d4
        move.w  d7,d4
        lsr.w   #8,d4
        SetGDCw d4,GDC_PixelFgColorByte0,d5
        moveq   #-16,d4                 ; Solid pattern offset
        add.l   a4,d4
        SetBLTSrcStartW d4,a0,d5
        SetBLTDstStartW a4,a0,d5
        SetBLTMode      BM_EXPANSION|BM_PATTERN,a0
        StartBLT        a0
        WaitScreenBLT   a0,d5

.nothing_before:
        swap    d7
        SetBLTROPW      d7,a0,d4
        swap    d7

        btst    #OPB_AND+16,d7
        beq     .no_and
        move.l  #$fffeffff,d6           ; Blit mask -> d6
        bra     .no_or

.no_and:
        move.l  #$00000001,d6           ; Blit mask -> d6

.no_or:
        clr.w   d0                      ; Offset in bitplanes array -> d0.w

.blit_planes_loop:
        lsr.b   #1,d7
        bcc     .plane_done             ; Plane not involved

        movea.l a5,a1                   ; Bitplanes array pointer
        adda.w  d0,a1
        move.l  (a1),d4
        beq     .empty_plane
        cmp.l   #-1,d4
        beq     .full_plane

        movea.l d4,a1                   ; Plane pointer
        adda.l  a2,a1                   ; + Offset (start in bitmap)

        btst    #17,d3                  ; Check system-to-screen flag
        beq     .copy_and_expand

        moveq   #0,d4
        SetBLTMode      BM_EXPANSION|BM_SYSTEM,a0
        bra     .blit_plane

.copy_and_expand:
        adda.l  #CHUNKYBUFFER,a4        ; Relative planar buffer address
        adda.l  a3,a4                   ; Absolute planar buffer address
        swap    d0
        exg     a0,a4
        bsr     ExpansionWrite
        exg     a0,a4
        swap    d0
        suba.l  a3,a4                   ; Relative planar buffer address
        move.l  a4,d4
        suba.l  #CHUNKYBUFFER,a4        ; Relative chunky buffer address
        SetBLTMode      BM_EXPANSION,a0
        bra     .blit_plane

.empty_plane:
        btst    #OPB_AND+16,d7
        beq     .plane_done             ; This dummy plane is a NOP
        moveq   #-8,d4                  ; Empty pattern offset
        bra     .dummy_plane

.full_plane:
        btst    #OPB_AND+16,d7
        bne     .plane_done             ; This dummy plane is a NOP
        moveq   #-16,d4                 ; Solid pattern offset

.dummy_plane:
        add.l   a4,d4
        SetBLTMode      BM_EXPANSION|BM_PATTERN,a0

.blit_plane:
        SetBLTSrcStartW d4,a0,d5
        SetBLTDstStartW a4,a0,d5
        SetGDCw d6,GDC_PixelFgColorByte0,d5
        swap    d6
        SetGDCw d6,GDC_PixelBgColorByte0,d5
        swap    d6
        StartBLT        a0
        tst.l   d4                      ; System-to-screen blit?
        bne     .wait_plane

        adda.l  a3,a4                   ; Absolute video memory address
        swap    d0
        exg     a0,a4
        bsr     ExpansionWrite
        exg     a0,a4
        swap    d0
        suba.l  a3,a4                   ; Relative video memory address
        WaitSystemBLT   a0,d5
        bra     .plane_done

.wait_plane:
        WaitScreenBLT   a0,d5

.plane_done:
        addq.w  #4,d0
        rol.l   #1,d6                   ; shift plane mask
        tst.b   d7                      ; any more planes ?
        bne     .blit_planes_loop

        btst    #OPB_INVERTAFTER+16,d7
        beq     .nothing_after

;       move.l  d3,d4
;       rol.l   #8,d4
        move.w  d7,d4
        lsr.w   #8,d4
        SetGDCw d4,GDC_PixelFgColorByte0,d5
        moveq   #-16,d4                 ; Solid pattern offset
        add.l   a4,d4
        SetBLTSrcStartW d4,a0,d5
        SetBLTDstStartW a4,a0,d5
        SetBLTROP       ROP_EOR,a0
        SetBLTMode      BM_EXPANSION|BM_PATTERN,a0
        StartBLT        a0
        WaitScreenBLT   a0,d5

.nothing_after:
        SetBLTSrcStartW a4,a0,d5
        move.w  d2,d4
        lsl.w   #3,d4                   ; Buffer pitch
        SetBLTSrcPitchW d4,a0,d5
        SetBLTDstStartW a6,a0,d5
        move.l  d1,d4
        swap    d4
        SetBLTDstPitchW d4,a0,d5
        SetBLTROP       ROP_SRC,a0
        SetBLTMode      BM_STANDARD,a0
        StartBLT        a0

        move.w  d1,d4                   ; Bitmap BytesPerRow
        mulu    d3,d4
        adda.l  d4,a2                   ; Shift Src offset to next chunk

        move.l  d1,d4
        swap    d4                      ; Screen BytesPerRow
        mulu    d3,d4
        adda.l  d4,a6                   ; Shift Dst address to next chunk

        swap    d2                      ; Rest height
        sub.w   d3,d2                   ; Substract blit from rest height
        cmp.w   d3,d2                   ; Rest height >= Blit height?
        bge     .blit_chunks_loop_wait  ; Blit next chunk

        tst.w   d2                      ; Rest height > 0?
        ble     .end                    ; No rest height => All done

        WaitScreenBLT   a0,d5

.height_ok:
        move.w  d2,d3                   ; Blit height = rest height
        SetBLTHeightW   d3,a0,d5        ; Last chunk is smaller
        bra     .blit_chunks_loop       ; Blit last chunk

.end:
        POP     d2-d7/a2-a6
        rts


************************************************************************
ExpansionWrite:
* Internal routine to write or blit planar data
************************************************************************
* a0:   APTR Video memory address
* a1:   APTR Source startaddress (word-aligned)
* d0:   BYTE XOffset (0..15)
* d1:   WORD BytesPerRow
* d2:   WORD Width (in bitmap bytes)
* d3:   LONG Height & Flags
* All registers are preserved
************************************************************************

        PUSH    d0-d7/a0-a6

        movea.w d3,a3                   ; Height -------------------> a3
        move.b  d0,d3
        ext.w   d3
        move.w  d2,d0

        btst    #16,d3
        sne     d4
        ext.w   d4
        ext.l   d4                      ; Toggle -------------------> d4

        ext.l   d2                      ; Clear internal flags -----> d2.w
        subq.w  #1,d2
        and.w   #$3,d2
        addq.w  #1,d2
        lsl.w   #3,d2
        movea.w d2,a6                   ; Bit address shift at eol -> a6

        add.w   d3,d2
        neg.w   d2
        add.w   #64,d2
        and.w   #$1f,d2
        moveq   #-1,d6
        moveq   #-1,d7
        lsr.l   d3,d6                   ; Left clipping mask -------> d6
        lsl.l   d2,d7                   ; Right clipping mask ------> d7

        move.w  d3,d5
        addq.w  #7,d5
        lsr.w   #3,d5
        add.w   d5,d0
        movea.w d0,a5                   ; Width --------------------> a5

        move.w  d0,d5
        subq.w  #1,d5
        btst    #1,d5
        beq     .longaligned
        bset    #30,d2                  ; Set extralong flag
.longaligned:

        move.w  d1,d5
        addq.w  #3,d0
        and.w   #~3,d0
        neg.w   d0
        add.w   d5,d0
        movea.w d0,a4                   ; Bitmap pitch remainder ---> a4

        subq.w  #1,d3
        and.w   #$1f,d3
        addq.w  #1,d3                   ; Buffer offset ------------> d3.w

        move.w  #32,d2
        sub.w   d3,d2                   ; Data offset --------------> d2.w

        moveq   #0,d5                   ; Buffer -------------------> d5

.yloop:
        move.l  a1,d0
        btst    #1,d0
        beq     .startrow               ; Longword-aligned row

        add.w   #16,d2
        move.w  #32,d3
        and.w   #$1f,d2                 ; Shift data offset
        sub.w   d2,d3
        bchg    #31,d2                  ; Test & toggle shift flag
        bne     .shiftback

        subq.l  #2,a1                   ; Last longword boundary
        not.w   d6
        swap    d6                      ; Left-shift left clipping mask
        btst    #30,d2                  ; One longword more in this line?
        bne     .append

        not.w   d7
        swap    d7                      ; Right-shift right clipping mask
        bra     .startrow

.append:
        swap    d7                      ; Left-shift right clipping mask
        not.w   d7
        addq.l  #2,a5                   ; Raise width
        subq.l  #4,a4                   ; Reduce pitch remainder
        bra     .startrow

.shiftback:
        addq.l  #2,a1                   ; Next longword boundary
        swap    d6
        not.w   d6                      ; Right-Shift left clipping mask
        btst    #30,d2                  ; One longword less in this line?
        bne     .shrink

        swap    d7
        not.w   d7                      ; Left-shift right clipping mask
        bra     .startrow

.shrink:
        not.w   d7
        swap    d7                      ; Right-shift right clipping mask
        subq.l  #2,a5                   ; Reduce width
        addq.l  #4,a4                   ; Raise pitch remainder

.startrow:
        movea.l a1,a2
        adda.l  a5,a2
        move.l  (a1)+,d0
        and.l   d6,d0                   ; Cut off unused bits
        move.l  d6,d1
        lsl.l   d3,d1
        bcs     .xloop                  ; Longword complete

        cmpa.l  a2,a1
        blt     .prepare

        and.l   d7,d0
        lsl.l   d3,d0                   ; Shift into position
        or.l    d0,d5                   ; Append on buffer
        bra     .continue               ; End of line

.prepare:
        lsl.l   d3,d0                   ; Shift into position
        or.l    d0,d5                   ; Append on buffer
        move.l  (a1)+,d0                ; Read from Bitmap

.xloop:
        cmpa.l  a2,a1                   ; Compare with lineend
        bge     .xexit                  ; Last longword read

        move.l  d0,d1                   ; Copy data
        lsr.l   d2,d0                   ; Part to process
        lsl.l   d3,d1                   ; Part to save
        or.l    d5,d0                   ; Assemble complete longword
        eor.l   d4,d0                   ; Toggle
        move.l  d0,(a0)                 ; Write into video memory

        btst    #17,d3                  ; Check system-to-screen flag
        bne     .conti1
        addq.l  #4,a0

.conti1:
        move.l  d1,d5                   ; Save remaining part
        move.l  (a1)+,d0                ; Read from Bitmap
        bra     .xloop

.xexit:
        and.l   d7,d0                   ; Cut off unused bits
        move.l  d7,d1
        lsl.l   d3,d1
        bcs     .complete2

        lsr.l   d2,d0
        or.l    d0,d5                   ; Append buffer
        bra     .continue

.complete2:
        move.l  d0,d1                   ; Copy data
        lsr.l   d2,d0                   ; Part to process
        lsl.l   d3,d1                   ; Part to save
        or.l    d5,d0                   ; Assemble complete longword
        eor.l   d4,d0
        move.l  d0,(a0)                 ; Write into video memory

        btst    #17,d3                  ; Check system-to-screen flag
        bne     .conti2
        addq.l  #4,a0

.conti2:
        move.l  d1,d5                   ; Save remaining part

.continue:
        add.w   a6,d2                   ; Raise offset
        move.w  #32,d3
        and.w   #$1f,d2
        subq.l  #1,a3                   ; Done with one line
        sub.w   d2,d3
        adda.l  a4,a1                   ; Add BytesPerRow remainder
        cmpa.l  #0,a3
        bgt     .yloop

        and.w   #$1f,d3                 ; Also clears C-flag
        lsl.l   d3,d6                   ; Check for remainder
        bcs     .remainder
        bmi     .noremainder

.remainder:
        eor.l   d4,d5
        move.l  d5,(a0)                 ; Write incomplete longword

.noremainder:
        POP     d0-d7/a0-a6
        rts


*************************************************************************************
SetDisplay:
* a0:   struct BoardInfo
* d0:   BOOL state
*************************************************************************************
        move.l  gbi_RegisterBase(a0),a0
        not.b   d0
        and.w   #1,d0
        lsl.b   #5,d0
        move.b  #TS_TSMode,TSI(a0)
        move.b  TSD(a0),d1
        and.b   #%11011111,d1
        or.b    d0,d1
        move.b  d1,TSD(a0)
        rts


****************************************************************************************
SetMemoryMode:
* a0:   struct BoardInfo
* d7:   RGBFTYPE RGBFormat
****************************************************************************************
        cmp.l   gbi_CurrentMemoryMode(a0),d7
        bne     .doit
        rts

.doit:
        PUSH    d0
        move.l  d7,gbi_CurrentMemoryMode(a0)
        cmp.l   #RGBFB_PLANAR,d7
        sne     d0
;       and.w   #1,d0
;       lsl.b   #3,d0
        and.w   #8,d0
        or.w    #(TS_MemoryMode<<8)|$06,d0      ; enable/disable chain 4
        move.w  d0,([gbi_RegisterBase,a0],TSI)
        POP     d0
        nop
        rts

;       PUSH    a0/d0-d1
;       move.l  (gbi_RegisterBase,a0),a0

;       and.b   #1,d0
;       move.b  #TS_ExtendedSequencerMode,TSI(a0)
;       move.b  TSD(a0),d1
;       and.b   #%11111110,d1
;       or.b    d0,d1
;       move.b  d1,TSD(a0)

;       move.b  #TS_MemoryMode,TSI(a0)
;       move.b  d0,d1
;       lsl.b   #3,d0
;       or.b    #$06,d0                         ; enable/disable chain 4
;       move.b  d0,TSD(a0)

;       POP     a0/d0-d1
;       rts


***************************************************************************************
SetWriteMask:
* a0:   struct BoardInfo
* d0:   UBYTE mask
***************************************************************************************
        PUSH    a0
        move.l  gbi_RegisterBase(a0),a0

        move.b  #TS_WritePlaneMask,TSI(a0)
        move.b  d0,TSD(a0)
        nop
        POP     a0
        rts


***************************************************************************************
SetReadPlane:
* a0:   struct BoardInfo
* d0:   UBYTE Plane
***************************************************************************************
        and.b   #3,d0
        move.l  gbi_RegisterBase(a0),a0

        SetGDCw d0,GDC_ReadPlaneSelect,d1
        nop
        rts


***************************************************************************************
SetClearMask:
* a0:   struct BoardInfo
* d0:   UBYTE mask
***************************************************************************************
        move.b  d0,gbi_ClearMask(a0)
        move.l  gbi_RegisterBase(a0),a0

        move.w  #(GDC_SetReset<<8),GDCI(a0)
        nop
        SetGDCw d0,GDC_EnableSetReset,d1
        nop
        rts


************************************************************************
WaitVerticalSync:
************************************************************************
* a0:   APTR BoardInfo
* d0:   BOOL End
************************************************************************

MAXPOLL EQU     100000

        move.l  gbi_RegisterBase(a0),a0
        lea     INPUTSTATUS1(a0),a0
        tst.b   d0
        beq     .sync_start

.sync_end:
        move.l  #MAXPOLL,d1
.wait_end:
        move.b  (a0),d0
        and.b   #(1<<3),d0
        beq     .done
        subq.l  #1,d1
        bne     .wait_end
        rts

.sync_start:
        move.l  #MAXPOLL,d1
.wait_start:
        move.b  (a0),d0
        and.b   #(1<<3),d0
        bne     .done
        subq.l  #1,d1
        bne     .wait_start
.done:
        rts


*****************************************************************
GetVSyncState:
* a0:   struct BoardInfo
* d0:   BOOL ExpectedState
*****************************************************************
        move.l  gbi_RegisterBase(a0),a0
        btst    #3,INPUTSTATUS1(a0)
        sne     d0
        extb.l  d0
        rts

************************************************************************
SetScreenSplit:
************************************************************************
* a0:   struct BoardInfo
* d0:	YPosition for the screen split	
************************************************************************

	move.w  d0,(gbi_YSplit,a0)
	subq.w	#1,d0		;the register is the split position-1

	move.l	gbi_ModeInfo(a0),a1
        btst    #GMB_DOUBLESCAN,gmi_Flags(a1)
        beq.s .regular
	lsl.w	#1,d0
	or.w 	#1,d0
.regular:
	btst    #GMB_DOUBLEVERTICAL,gmi_Flags(a1)
        beq.s   .no_doublevert
	lsr.w	#1,d0
.no_doublevert:
	move.w #$3ff,d1		;maximum split position
	move.l  gbi_ExecBase(a0),a1
	cmp.w d1,d0		;beyond maximum position
	blo.s .okclip
	move.w d1,d0
.okclip:	
	move.l	gbi_RegisterBase(a0),a0

        DISABLE a1,NOFETCH              ; no disturbance while modifying registers
	
	;; bit 9 of the screen split is in CRTC_MaximumRowAddress, $9
	move.b  #CRTC_MaximumRowAddress,CRTCI(a0)
        move.b  CRTCD(a0),d1
        bclr    #6,d1
        btst    #9,d0
        beq.s   .not9
        bset    #6,d1
.not9:
        move.b  d1,CRTCD(a0)
	
	;; bit 8 of the screen split is in CRTC_OverflowLow, $7
	move.b  #CRTC_OverflowLow,CRTCI(a0)
        move.b  CRTCD(a0),d1
        bclr    #4,d1
        btst    #8,d0
        beq.s   .not8
        bset    #4,d1
.not8:
        move.b  d1,CRTCD(a0)

	;; the remaining bits are in CRTC_LineCompare, $18
	move.b  #CRTC_LineCompare,CRTCI(a0)
	move.b	d0,CRTCD(a0)
	
        ENABLE a1,NOFETCH              ; no disturbance while modifying registers
	
	rts
	
************************************************************************
SetClock:
************************************************************************
* a0:   struct BoardInfo
************************************************************************
        move.l  gbi_ModeInfo(a0),a1
        move.l  gbi_RegisterBase(a0),a0

.numerator:
        move.w  #TS_VCLK3Numerator<<8,d1
        move.b  gmi_Numerator(a1),d1
        move.w  d1,TSI(a0)

.denominator:
        move.w  #TS_VCLK3Denominator<<8,d1
        move.b  gmi_Denominator(a1),d1
        cmp.b   #31,d1
        ble     .shift
        and.b   #%00111111,d1
        or.b    #1,d1                   ; Post scaler
        bra     .write
.shift:
        lsl.b   #1,d1
.write:
        move.w  d1,TSI(a0)
        rts


************************************************************************
ResolvePixelClock:
************************************************************************
* a0:   struct BoardInfo
* a1:   struct ModeInfo
* d0.l: pixelclock
* d7:   RGBFormat
************************************************************************
        PUSH    d2-d4
        moveq   #0,d2
        move.w  (.num_clocks,pc,d7.l*2),d2
        move.l  gbi_Flags(a0),d1
        btst    #BIB_OVERCLOCK,d1
        beq     .not_overclocked
        move.w  (.num_overclocks,pc,d7.l*2),d2
.not_overclocked:

        move.l  d0,d1
        moveq   #4,d4
        lea     PixelClocks,a0
        moveq   #0,d3
        move.b  (.clock_multipliers,pc,d7.l),d3
        cmp.b   #3,d3
        bne     .normal_mode
.truecolor_mode:
        add.l   d0,d1
        add.l   d0,d1
        moveq   #12,d4
        lea     TruePixelClocks,a0
.normal_mode:

        moveq   #0,d0
.loop:
        cmp.l   (a0),d1
        adda.l  d4,a0
        beq     .found
        blt     .adjust
        add.l   #1,d0
        cmp.l   d2,d0
        blt     .loop

        subq.l  #1,d0
        bra     .found
.adjust:
        tst.l   d0
        beq     .found
        suba.l  d4,a0
        move.l  (a0),d2
        suba.l  d4,a0
        add.l   (a0),d2
        sub.l   d1,d2
        sub.l   d1,d2
        bmi     .less
.first:
        move.l  (a0),d1
        subq.l  #1,d0
        bra     .found
.less:
        adda.l  d4,a0
        move.l  (a0),d1
.found:

        and.b   #~GMF_DOUBLECLOCK,gmi_Flags(a1)
        cmp.l   #RGBFB_CHUNKY,d7
        bne     .no_doubleclock
        cmp.l   #PLANARCLOCKS,d0
        blt     .no_doubleclock
        or.b    #GMF_DOUBLECLOCK,gmi_Flags(a1)
.no_doubleclock:

        and.b   #~GMF_DOUBLEVERTICAL,gmi_Flags(a1)

        btst    #GMB_DOUBLESCAN,gmi_Flags(a1)           ;thor: experimental
        bne.s .regular
        cmp.w   #512,gmi_VerTotal(a1)
        bge.s .doublevert
.regular:
        cmp.w   #1024,gmi_VerTotal(a1)
        blt     .vertical_value_ok
.doublevert:
        or.b    #GMF_DOUBLEVERTICAL,gmi_Flags(a1)
.vertical_value_ok:

        divu.l  d3,d1
        move.l  d1,gmi_PixelClock(a1)
        move.l  d0,d4
        cmp.b   #3,d3
        bne     .normal
        mulu    #3,d4
        move.b  TrueNumerators(pc,d4.l),gmi_Numerator(a1)
        move.b  TrueDenominators(pc,d4.l),gmi_Denominator(a1)
        bra     .end
.normal:
        move.b  Numerators(pc,d4.l),gmi_Numerator(a1)
        move.b  Denominators(pc,d4.l),gmi_Denominator(a1)
.end:
        POP     d2-d4
        rts

.num_clocks:
        dc.w    PLANARCLOCKS,CHUNKYCLOCKS,  TRUECLOCKS,  TRUECLOCKS
        dc.w      HIGHCLOCKS,  HIGHCLOCKS, ALPHACLOCKS, ALPHACLOCKS
        dc.w     ALPHACLOCKS, ALPHACLOCKS,  HIGHCLOCKS,  HIGHCLOCKS
        dc.w      HIGHCLOCKS,  HIGHCLOCKS,  HIGHCLOCKS,PLANARCLOCKS
.num_overclocks:
        dc.w      PLANAROVER,  CHUNKYOVER,    TRUEOVER,    TRUEOVER
        dc.w        HIGHOVER,    HIGHOVER,   ALPHAOVER,   ALPHAOVER
        dc.w       ALPHAOVER,   ALPHAOVER,    HIGHOVER,    HIGHOVER
        dc.w        HIGHOVER,    HIGHOVER,    HIGHOVER,  PLANAROVER
.clock_multipliers:
        dc.b    1,1,3,3,1,1,1,1,1,1,1,1,1,1,1,1
        cnop    0,4


************************************************************************
GetPixelClock:
************************************************************************
* a0:   struct BoardInfo
* a1:   struct ModeInfo
* d0.l: index
* d7:   RGBFormat
************************************************************************
        moveq   #0,d1
        move.b  (.clock_dividers,pc,d7.l),d1
        mulu    d1,d0
        lea     PixelClocks,a0
        cmp.b   #3,d1
        bne     .normal
        lea     TruePixelClocks,a0
.normal:
        move.l  (a0,d0.l*4),d0
        divu.l  d1,d0
        rts

.clock_dividers:
        dc.b    1,1,3,3,1,1,1,1,1,1,1,1,1,1,1,1


************************************************************************

PLANARCLOCKS    EQU     151
CHUNKYCLOCKS    EQU     201
HIGHCLOCKS      EQU     151
TRUECLOCKS      EQU     38
ALPHACLOCKS     EQU     71

PLANAROVER      EQU     171
CHUNKYOVER      EQU     251
HIGHOVER        EQU     171
TRUEOVER        EQU     61
ALPHAOVER       EQU     81

Numerators:
        dc.b      7, 11, 43, 45, 47,  7, 49, 49, 45, 63
        dc.b     22, 67, 19, 53, 32, 71, 39, 31, 69, 79
        dc.b     81, 63, 22, 47, 83, 91, 45, 41, 57, 89
        dc.b     54, 57, 69, 37, 83, 73, 45,123, 81,103
TrueNumerators:
        dc.b     44, 49, 13, 11, 38, 59, 53,117, 19, 53
        dc.b     22,119, 78, 51, 31, 55, 69, 78, 79, 80
        dc.b     95, 82, 63, 29, 44, 95,127, 79, 83, 59
        dc.b     22, 54, 45, 94,105, 73, 57,105, 89, 83
        dc.b    101, 67, 57,115, 69,125, 37, 71, 83,118
        dc.b     73, 31, 90, 75,127, 12, 81, 94,103, 54
        dc.b     88, 93, 98,116, 13, 48, 22,102, 76, 18
        dc.b     59,119,106, 65,117, 33, 19,110, 53, 34
        dc.b     44, 64,119, 15,126, 81, 51, 77, 31, 26
        dc.b    110, 58, 69,123, 43, 92,109,126,127,111
        dc.b     95, 45,113, 74, 63,121, 29, 35, 88, 59
        dc.b     95,125, 18,127, 79, 55, 43, 68, 87, 25
        dc.b     44,120, 89,115, 45, 84, 26,111,105, 33
        dc.b     73, 20,114,101, 61,109, 89,117, 83,125
        dc.b     21,127, 99, 78, 57, 93,115, 94,109, 73
        dc.b     22, 81, 37,119,127, 30, 83, 91, 99,107
        dc.b    123, 54, 31,109, 86, 55, 71,111,127, 24
        dc.b    121, 97, 81,122, 49, 41, 33, 91,108,117
        dc.b    109,101, 93, 34,111, 77,103, 69, 26,113
        dc.b     96,114, 44, 53, 71, 98,107,125, 27,127
        dc.b    118, 82,119,101, 83, 37, 65, 28,103, 47
        dc.b     66

Denominators:
        dc.b     10, 15, 56, 56, 56,  8, 54, 52, 46, 62
        dc.b     21, 62, 17, 46, 27, 58, 31, 24, 52, 58
        dc.b     58, 44, 15, 31, 54, 58, 28, 25, 34, 52
        dc.b     31, 32, 38, 20, 44, 38, 23, 62, 40, 50
TrueDenominators:
        dc.b     21, 23,  6,  5, 17, 26, 23, 50,  8, 22
        dc.b      9, 48, 31, 20, 12, 21, 26, 29, 29, 29
        dc.b     34, 29, 22, 10, 15, 32, 42, 26, 27, 19
        dc.b      7, 17, 14, 29, 32, 22, 17, 31, 26, 24
        dc.b     29, 19, 16, 32, 19, 34, 10, 19, 22, 31
        dc.b     19,  8, 23, 19, 32,  3, 20, 23, 25, 13
        dc.b     21, 22, 23, 27,  3, 11,  5, 23, 17,  4
        dc.b     13, 26, 23, 14, 25,  7,  4, 23, 11,  7
        dc.b      9, 13, 24,  3, 25, 16, 10, 15,  6,  5
        dc.b     21, 11, 13, 23,  8, 17, 20, 23, 23, 20
        dc.b     17,  8, 20, 13, 11, 21,  5,  6, 15, 10
        dc.b     16, 21,  3, 21, 13,  9,  7, 11, 14,  4
        dc.b      7, 19, 14, 18,  7, 13,  4, 17, 16,  5
        dc.b     11,  3, 17, 15,  9, 16, 13, 17, 12, 18
        dc.b      3, 18, 14, 11,  8, 13, 16, 13, 15, 10
        dc.b      3, 11,  5, 16, 17,  4, 11, 12, 13, 14
        dc.b     16,  7,  4, 14, 11,  7,  9, 14, 16,  3
        dc.b     15, 12, 10, 15,  6,  5,  4, 11, 13, 14
        dc.b     13, 12, 11,  4, 13,  9, 12,  8,  3, 13
        dc.b     11, 13,  5,  6,  8, 11, 12, 14,  3, 14
        dc.b     13,  9, 13, 11,  9,  4,  7,  3, 11,  5
        dc.b      7

PixelClocks:
        dc.l     10022726, 10499999, 10994317, 11505680, 12017044
        dc.l     12528407, 12992423, 13492131, 14006915, 14549118
        dc.l     14999998, 15472872, 16002672, 16497033, 16969695
        dc.l     17527427, 18013194, 18494316, 18999123, 19502349
        dc.l     19996079, 20501030, 20999997, 21708208, 22007573
        dc.l     22464731, 23011361, 23481815, 24004008, 24506116
        dc.l     24941346, 25504258, 25998801, 26488633, 27009294
        dc.l     27505977, 28013830, 28405422, 28994314, 29495451
TruePixelClocks:
        dc.l     29999996, 30503949, 31022723, 31499996, 32005344
        dc.l     32491255, 32994067, 33504541, 34005677, 34493797
        dc.l     34999996, 35497155, 36026388, 36511359, 36988632
        dc.l     37499995, 37998247, 38510967, 39004697, 39498428
        dc.l     40006679, 40485888, 41002061, 41522722, 41999995
        dc.l     42507097, 43295449, 43505239, 44015146, 44461717
        dc.l     44999994, 45481278, 46022721, 46410652, 46981528
        dc.l     47510325, 48008015, 48497061, 49012232, 49517039
        dc.l     49866765, 50490424, 51008516, 51455959, 51997601
        dc.l     52640368, 52977266, 53504778, 54018588, 54501459
        dc.l     55011955, 55482947, 56027661, 56519132, 56825277
        dc.l     57272720, 57988629, 58517779, 58990902, 59475517
        dc.l     59999992, 60526852, 61007897, 61515144, 62045447
        dc.l     62479331, 62999992, 63498016, 64010687, 64431810
        dc.l     64982509, 65533208, 65988134, 66477264, 67009082
        dc.l     67499991, 68011355, 68478252, 68987595, 69545446
        dc.l     69999991, 70489502, 70994309, 71590900, 72163627
        dc.l     72485786, 73022718, 73499991, 73977263, 74454536
        dc.l     74999990, 75495858, 75996494, 76571137, 76960217
        dc.l     77486621, 78034081, 78438725, 79061255, 79465899
        dc.l     80013359, 80539762, 80897717, 81503486, 82004122
        dc.l     82499990, 83045444, 83522717, 83999989, 84477262
        dc.l     85014194, 85227262, 85909080, 86590898, 87010478
        dc.l     87499989, 87954534, 88512385, 88977261, 89488625
        dc.l     89999989, 90430611, 91022716, 91477261, 92045443
        dc.l     92517471, 93068170, 93489293, 93963056, 94499988
        dc.l     95020649, 95454533, 96016031, 96409079, 97045442
        dc.l     97542601, 98024463, 98542768, 99034078, 99431806
        dc.l    100227260,101022714,101249987,101528913,102017032
        dc.l    102430057,102911919,103531455,104045441,104522714
        dc.l    104999987,105433871,105954532,106491464,106965227
        dc.l    107386350,108037176,108579532,109038448,109431804
        dc.l    110071009,110454531,110965895,111477259,111942135
        dc.l    112499986,112954531,113522713,113650554,114545440
        dc.l    115499985,115738622,115977258,116454531,116931803
        dc.l    117409076,118124985,118450398,118951034,119659076
        dc.l    120052432,120511348,121053704,121704530,122255229
        dc.l    122499984,122897712,123494302,124090893,124458026
        dc.l    124958662,125559425,125999984,126477257,127073847
        dc.l    127561967,127670438,127840893,128863620,129886347
        dc.l    129965018,130454529,131066417,131466925,132045438
        dc.l    132443165,132954529,133636347,134070231,134590892
        dc.l    134999983


************************************************************************
GetCompatibleFormats:
************************************************************************
* a0: struct BoardInfo *bi
* d7: RGBFTYPE RGBFormat
* for the CirrusGD5434, due to the 1MB planar limitation, only one
* planar screen is allowed on the board at any time, all other formats
* can coexist without any problems
************************************************************************

        cmp.l   #RGBFB_PLANAR,d7
        beq     .planar

.chunky:
        move.l  #~RGBFF_PLANAR,d0
        bra     .end

.planar:
        move.l  #RGBFF_PLANAR,d0

.end:
        rts


************************************************************************

EndCode:
   END
