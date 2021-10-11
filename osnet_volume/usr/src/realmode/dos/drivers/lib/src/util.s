;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)util.s	1.2	97/03/10 SMI\n"
;
;
; Realmode driver utility functions.
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

; Instruction prefix for overriding data size
DATA32	EQU	66H

	.386
;
;	unsigned short get_code_selector(void)
;
;	return the present value of the CS segment register
;
	PUBLIC  _get_code_selector
_get_code_selector   PROC    NEAR
	mov     ax, cs
	ret
_get_code_selector   ENDP

;
;	unsigned short get_data_selector(void)
;
;	return the present value of the DS segment register
;
	PUBLIC  _get_data_selector
_get_data_selector   PROC    NEAR
	mov     ax, ds
	ret
_get_data_selector   ENDP

;	ushort splhi(void);
;
;	Disable processor interrupts.  Return the previous state.
;
	PUBLIC  _splhi
_splhi	PROC    NEAR
        pushf
        cli
        pop     ax
        and     ax,200h
	ret
_splhi	ENDP

;	ushort splx(ushort);
;
;	Set the processor interrupt flag according to the argument.
;	Return the previous state.
;
        PUBLIC  _splx
_splx	PROC    NEAR
        push    bp
        mov     bp, sp
        pushf
        mov	ax, [bp-2]
        mov     cx, 4[bp]
        and     cx, 200h
        or      [bp-2], cx
        popf
	and	ax, 200h
        pop     bp
        ret
_splx	ENDP

;
;	void outb(unsigned short port, unsigned char data)
;
;	Write 'data' byte to I/O address 'port'
;
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

;
;	void outw(unsigned short port, unsigned short data)
;
;	Write 'data' word to I/O address 'port'
;
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

;
;	void outl(unsigned short port, unsigned long data)
;
;	Write 'data' long to I/O address 'port'
;
	PUBLIC	_outl
_outl	PROC	NEAR
	push	bp
	mov	bp, sp
	db	DATA32
	xor	dx, dx
	mov	dx, 4[bp]
	mov	ax, 8[bp]
	db	DATA32
	shl	ax, 16
	mov	ax, 6[bp]
	db	DATA32
	out	dx, ax
	pop	bp
	ret
_outl	ENDP

;
;	unsigned char inb(unsigned short port)
;
;	Read and return a byte from the byte-wide I/O register 'port'
;
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

;
;	unsigned short inw(unsigned short port)
;
;	Read and return a word from the 16-bit-wide I/O register 'port'
;
	PUBLIC  _inw
_inw    PROC    NEAR
	push    bp
	mov     bp, sp
	mov     dx, 4[bp]
	in      ax, dx
	pop     bp
	ret
_inw    ENDP

;
;	unsigned short inl(unsigned short port)
;
;	Read and return a long word from the 32-bit-wide I/O register 'port'
;
	PUBLIC  _inl
_inl    PROC    NEAR
	push    bp
	mov     bp, sp
	db	DATA32
	xor 	dx, dx
	mov     dx, 4[bp]
	db	DATA32
	in	ax, dx
	mov	dx, ax
	db	DATA32
	shr	ax, 16
	xchg	ax, dx
	pop     bp
	ret
_inl    ENDP

_TEXT	ENDS
END
