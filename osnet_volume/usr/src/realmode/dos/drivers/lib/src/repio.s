;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)repio.s	1.1	97/03/10 SMI\n"
;
; Repeat string I/O routines for realmode drivers.
;
;
;
;	Static Name Aliases
;
	TITLE   repio

	.286		; masm rejects "rep insw" etc without this

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
_TEXT      SEGMENT

; Data size override instruction
DATA16	EQU	66H

;
;
;	void repinsl(ushort port, char far *buffer, ushort count);
;
;		reads "count" longs from "port" and
;		stores them in "buffer".

	PUBLIC  _repinsl
_repinsl   PROC    NEAR
	push    bp
	mov     bp, sp
	push	di
	push	es
	mov     dx, 4[bp]
	mov	di, 6[bp]
	mov	es, 8[bp]
	mov	cx, 10[bp]
	db	DATA16
	rep	insw
	pop	es
	pop	di
	pop     bp
	ret
_repinsl   ENDP

;
;	void repoutsl(ushort port, char far *buffer, ushort count);
;
;		writes "count" longs to "port" from
;		"buffer".
;

	PUBLIC  _repoutsl
_repoutsl   PROC    NEAR
	push    bp
	mov     bp, sp
	push	si
	push	ds
	mov     dx, 4[bp]
	mov	si, 6[bp]
	mov	ds, 8[bp]
	mov	cx, 10[bp]
	db	DATA16
	rep	outsw
	pop	ds
	pop	si
	pop     bp
	ret
_repoutsl   ENDP

;
;
;	void repinsw(ushort port, char far *buffer, ushort count);
;
;		reads "count" 16-bit words from "port" and
;		stores them in "buffer".

	PUBLIC  _repinsw
_repinsw   PROC    NEAR
	push    bp
	mov     bp, sp
	push	di
	push	es
	mov     dx, 4[bp]
	mov	di, 6[bp]
	mov	es, 8[bp]
	mov	cx, 10[bp]
	rep	insw
	pop	es
	pop	di
	pop     bp
	ret
_repinsw   ENDP

;
;	void repoutsw(ushort port, char far *buffer, ushort count);
;
;		writes "count" 16-bit words to "port" from
;		"buffer".
;

	PUBLIC  _repoutsw
_repoutsw   PROC    NEAR
	push    bp
	mov     bp, sp
	push	si
	push	ds
	mov     dx, 4[bp]
	mov	si, 6[bp]
	mov	ds, 8[bp]
	mov	cx, 10[bp]
	rep	outsw
	pop	ds
	pop	si
	pop     bp
	ret
_repoutsw   ENDP

;
;	void repinsb(ushort port, char far *buffer, ushort count);
;
;		reads "count" bytes from "port" and
;		stores them in "buffer".

	PUBLIC  _repinsb
_repinsb   PROC    NEAR
	push    bp
	mov     bp, sp
	push	di
	push	es
	mov     dx, 4[bp]
	mov	di, 6[bp]
	mov	es, 8[bp]
	mov	cx, 10[bp]
	rep	insb
	pop	es
	pop	di
	pop     bp
	ret
_repinsb   ENDP

;
;	void repoutsb(ushort port, char far *buffer, ushort count);
;
;		writes "count" bytes to "port" from "buffer".
;

	PUBLIC  _repoutsb
_repoutsb   PROC    NEAR
	push    bp
	mov     bp, sp
	push	si
	push	ds
	mov     dx, 4[bp]
	mov	si, 6[bp]
	mov	ds, 8[bp]
	mov	cx, 10[bp]
	rep	outsb
	pop	ds
	pop	si
	pop     bp
	ret
_repoutsb   ENDP

_TEXT	ENDS
END
