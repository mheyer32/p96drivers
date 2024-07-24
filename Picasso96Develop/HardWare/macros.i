	IFND  PRIVATE_MACROS_I
PRIVATE_MACROS_I SET 1


***************************************
CALL	MACRO	;FunctionName
	jsr	_LVO\1(a6)
	ENDM

PUSHM	MACRO	;Registers
	movem.l	\1,-(sp)
	ENDM

POPM	MACRO	;Registers
	movem.l	(sp)+,\1
	ENDM

PUSH	MACRO	;Registers
	movem.l	\1,-(sp)
	ENDM

POP	MACRO	;Registers
	movem.l	(sp)+,\1
	ENDM

TOP	MACRO	;Registers
	movem.l	(sp),\1
	ENDM

DELAY	MACRO
	tst.b	$bfe001
	ENDM

* macro to produce multiple instances of code blocks
REPEAT	MACRO
	IFNE	\1
	\2
	REPEAT	(\1-1),<\2>
	ENDC
	ENDM

***************************************

_intena	EQU	$dff09A

***************************************
	ENDC  ;PRIVATE_MACROS_I
