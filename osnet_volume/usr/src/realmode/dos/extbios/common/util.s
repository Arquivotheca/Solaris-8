;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc.  All rights reserved.
;
; ident	"@(#)util.s	1.11	95/10/27 SMI\n"
;
; Solaris Primary Boot Subsystem - BIOS Extension Driver Framework
;===========================================================================
; Provides minimal INT 13h services for MDB devices during Solaris
; primary boot sequence.
;
; File: util.s
;
; This file contains miscellaneous routines that are used throughout
; the MDB driver code base.
;
;	Static Name Aliases
;
	TITLE   util

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

	.386
EXTERN _putchar:NEAR
EXTERN _puthex:NEAR
	PUBLIC  _mycs
_mycs   PROC    NEAR
	mov     ax, cs
	ret
_mycs   ENDP

	PUBLIC  _myds
_myds   PROC    NEAR
	mov     ax, ds
	ret
_myds   ENDP

	PUBLIC  _intr_disable
_intr_disable PROC    NEAR
	cli
	ret
_intr_disable ENDP

	PUBLIC  _intr_enable
_intr_enable PROC    NEAR
	sti
	ret
_intr_enable ENDP

	PUBLIC  _outb
_outb   PROC    NEAR
	push    bp
	mov     bp, sp
	mov     dx, 4[bp]
	mov     al, 6[bp]
	out     dx, al
	pop     bp
	ret
_outb   ENDP

	PUBLIC  _outw
_outw   PROC    NEAR
	push    bp
	mov     bp, sp
	mov     dx, 4[bp]
	mov     ax, 6[bp]
	out     dx, ax
	pop     bp
	ret
_outw   ENDP

DATA16	EQU	66H

	PUBLIC	_outl
_outl	PROC	NEAR
	push	bp
	mov	bp, sp
	db	DATA16
	xor	dx, dx
	mov	dx, 4[bp]
	mov	ax, 8[bp]
	db	DATA16
	shl	ax, 16
	mov	ax, 6[bp]
	db	DATA16
	out	dx, ax
	pop	bp
	ret
_outl	ENDP

	PUBLIC  _inb
_inb    PROC    NEAR
	push    bp
	mov     bp, sp
	mov     dx, 4[bp]
	in      al, dx
	xor     ah, ah
	pop     bp
	ret
_inb    ENDP

	PUBLIC  _inw
_inw    PROC    NEAR
	push    bp
	mov     bp, sp
	mov     dx, 4[bp]
	in      ax, dx
	pop     bp
	ret
_inw    ENDP

	PUBLIC  _inl
_inl    PROC    NEAR
	push    bp
	mov     bp, sp
	db	DATA16
	xor 	dx, dx
	mov     dx, 4[bp]
	db	DATA16
	in	ax, dx
	mov	dx, ax
	db	DATA16
	shr	ax, 16
	xchg	ax, dx
	pop     bp
	ret
_inl    ENDP

;       bcopy(far *dest, far *src, short bytecount)
	PUBLIC  _bcopy
_bcopy  PROC NEAR
	push    bp
	mov     bp, sp
	push    si
	push    di
	push    ds
	push    es

	cld
	mov     si, 8[bp]
	mov     ds, 10[bp]
	mov     di, 4[bp]
	mov     es, 6[bp]
	mov     cx, 12[bp]
	rep     movsb

	pop     es
	pop     ds
	pop     di
	pop     si
	pop     bp
	ret
_bcopy  ENDP

	PUBLIC  _putptr
_putptr PROC NEAR
	push    bp
	mov     bp, sp
	mov     ax, 6[bp]
	push    ax
	call    _puthex
	pop     ax
	xor     ah, ah
	mov     al, ':'
	push    ax
	call    _putchar
	pop     ax
	mov     ax, 4[bp]
	push    ax
	call    _puthex
	pop     ax
	pop     bp
	ret
_putptr ENDP

	PUBLIC	_milliseconds
_milliseconds PROC NEAR
         push	bp
         mov bp, sp
         mov ax, 4[bp]
         mov cx, 1000         ;convert units to microseconds
         mul cx
         mov cx, dx
         mov dx, ax
         mov ah, 86h
         int 15h
         pop	bp
	 ret
_milliseconds ENDP

	PUBLIC	_microseconds
_microseconds PROC NEAR
	 mov cx, 1
         push	bp
         mov bp, sp
         mov ax, 4[bp]
         mul cx
         mov cx, dx
         mov dx, ax
         mov ah, 86h
         int 15h
         pop	bp
	 ret
_microseconds ENDP

_TEXT	ENDS
END
