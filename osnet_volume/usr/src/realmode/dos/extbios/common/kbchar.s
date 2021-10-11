;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc.  All rights reserved.
;
; ident	"@(#)kbchar.s	1.6	94/05/23 SMI\n"
;
; Solaris Primary Boot Subsystem - BIOS Extension Driver Support Routines
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	kbchar  (kbchar.s)
;
;   Calling Syntax:	keycode = kbchar ()
;
;   Description:	no input argument; reads keyboard buffer and
;			returns character code in AX.
;
;	Static Name Aliases

	TITLE   kbchar

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

	PUBLIC  _kbchar
_kbchar PROC    NEAR		; Read char from keyboard
	xor     ah, ah
	int     16H
	xor     ah, ah
	ret
_kbchar ENDP

_TEXT	ENDS
END

