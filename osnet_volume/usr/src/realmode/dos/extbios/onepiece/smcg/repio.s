;
; Copyright (c) 1997 Sun Microsystems, Inc.
; All rights reserved.
;
; ident "@(#)repio.s	1.3	97/03/11 SMI\n"
;
; File: repio.s
;
; This file contains I/O routines used by some of the SMC drivers.
;
	TITLE   repio

	.286            ; masm rejects "rep insw" etc without this

_TEXT   SEGMENT  BYTE PUBLIC 'CODE'
_TEXT   ENDS
_DATA   SEGMENT  WORD PUBLIC 'DATA'
_DATA   ENDS
CONST   SEGMENT  WORD PUBLIC 'CONST'
CONST   ENDS
_BSS    SEGMENT  WORD PUBLIC 'BSS'
_BSS    ENDS
DGROUP  GROUP   CONST,  _BSS,   _DATA
	ASSUME  CS: _TEXT, DS: NOTHING, SS: NOTHING, ES: NOTHING
_TEXT      SEGMENT

;
;       repinsw (ushort port, char far *buffer, ushort count);
;
;               reads "count" words from "port" and stores them in "buffer".
;

	PUBLIC  _repinsw
_repinsw   PROC    NEAR
	push    bp
	mov     bp, sp
	push    di
	push    es
	mov     dx, 4[bp]
	mov     di, 6[bp]
	mov     es, 8[bp]
	mov     cx, 10[bp]
	rep     insw
	pop     es
	pop     di
	pop     bp
	ret
_repinsw   ENDP

;
;       repoutsw (ushort port, char far *buffer, ushort count);
;
;               writes "count" words to "port" from "buffer".
;

	PUBLIC  _repoutsw
_repoutsw   PROC    NEAR
	push    bp
	mov     bp, sp
	push    si
	push    ds
	mov     dx, 4[bp]
	mov     si, 6[bp]
	mov     ds, 8[bp]
	mov     cx, 10[bp]
	rep     outsw
	pop     ds
	pop     si
	pop     bp
	ret
_repoutsw   ENDP
	
;DATA16  EQU     66H
;
;	PUBLIC  _outl
;_outl   PROC    NEAR
;	push    bp
;	mov     bp, sp
;	db      DATA16
;	xor     dx, dx
;	mov     dx, 4[bp]
;	mov     ax, 8[bp]
;	db      DATA16
;	shl     ax, 16
;	mov     ax, 6[bp]
;	db      DATA16
;	out     dx, ax
;	pop     bp
;	ret
;_outl   ENDP

_TEXT ENDS

END
