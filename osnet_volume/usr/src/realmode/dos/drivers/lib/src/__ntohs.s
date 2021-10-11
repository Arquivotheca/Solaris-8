;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)__ntohs.s	1.1	97/01/17 SMI\n"
; 
; Solaris Primary Boot Subsystem - BIOS Extension Driver Support Routines
;===========================================================================
; Provides minimal services for the real mode environment that the operating 
; system would normally supply.
;
;   Function name:	__ntohs  (__ntohs.s)
;
;   Calling Syntax:	postswap = __ntohs(preswap)
;
;   Description:	convert network data to host data.  On x86 that means
;			swap low/high-order bytes within a 16-bit short
;
;	Static Name Aliases
;
	TITLE   ntohs

_TEXT	SEGMENT  BYTE PUBLIC 'CODE'
_TEXT	ENDS
_DATA	SEGMENT  WORD PUBLIC 'DATA'
_DATA	ENDS
CONST	SEGMENT  WORD PUBLIC 'CONST'
CONST	ENDS
_BSS	SEGMENT  WORD PUBLIC 'BSS'
_BSS	ENDS
DGROUP	GROUP	CONST,	_BSS,	_DATA
	ASSUME  CS: _TEXT, DS: NOTHING, SS: NOTHING, ES: NOTHING

_TEXT   SEGMENT

	PUBLIC  ___ntohs
___ntohs PROC    NEAR
	push	bp
	mov	bp, sp
	mov	ax, [bp+4]
	xchg	ah, al
	pop	bp
	ret
___ntohs ENDP

_TEXT	ENDS
END

