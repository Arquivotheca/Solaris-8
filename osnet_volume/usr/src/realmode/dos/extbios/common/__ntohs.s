;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc.  All rights reserved.
;
; ident	"@(#)__ntohs.s	1.6	94/05/23 SMI\n"
;
; Solaris Primary Boot Subsystem - BIOS Extension Driver Support Routines
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	__ntohs  (__ntohs.s)
;
;   Calling Syntax:	postswap = __ntohs(preswap)
;
;   Description:	swap low/high-order bytes within a 16-bit short
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

;       Read char from keyboard
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

