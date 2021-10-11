;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc.  All rights reserved.
;
; ident	"@(#)low.s	1.6	94/05/23 SMI\n"
;
; Solaris Primary Boot Subsystem - BIOS Extension Driver Support Routines
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   File name:		low.s
;
;   Function names:	low       - preamble for setup/initmain
;			newvect   - INT 13h handler 
;			hidata    - preserves a driver's data segment
;
;   Calling Syntax:	see source below
;
;
;	Static Name Aliases
;
	TITLE   low


	_text	segment  word public 'code'   ; Various encantations required
	_text	ends                          ; .. by the assembler
	const	segment  word public 'const'
	const	ends
	_data	segment  word public 'data'

BEF_READ        equ	2
BEF_IDENT	equ	0F8h
BEF_MAGIC	equ	0BEF1h

	EXTRN   _oldvect: dword
	EXTRN	_stack: word
	EXTRN	_stacksize: word

	_data	ends
	_bss	segment  word public 'bss'
	_bss	ends

dgroup	group   const, _bss, _data
	assume  cs:_text, ds:_data, es:nothing

_TEXT   SEGMENT
	EXTRN   _resmain: near

PUBLIC MDBmark
PUBLIC resaddr
PUBLIC ressize
PUBLIC MDBcode
PUBLIC topmem
mydata  	WORD    0
sosave  	WORD    0
sssave  	WORD    0
MDBmark 	WORD	0BEF1h
resaddr		DWORD	0
ressize		BYTE	0
MDBcode		BYTE	0
topmem          WORD    0

	PUBLIC  _newvect
_newvect PROC FAR
;
;       INTERCEPTED BIOS ENTRY POINT.
;       PRESERVE ALL REGISTERS.
;
	sub     sp, 4         ;    allow room for long return
	push    bp
	mov     bp, sp
	push    ds

	mov     cs:sosave, sp
	mov     cs:sssave, ss
	mov     ss, cs:mydata
	mov     sp, offset _stack
	add	sp, ss:_stacksize
	sti

	push    ds
	mov     ds, cs:mydata
	ASSUME  ds: DGROUP

;       Save registers used here
	push    es
	mov     es, cs:mydata
	push    di
	push    si
	push    dx
	push    cx
	push    bx
	push    ax

	call    _resmain

	cmp     ax, 0
	je      passon
; WARNING.  Results of cmp are used again below.  Do not change flags.
	pop     ax
	pop     bx
	pop     cx
	pop     dx
	pop     si
	pop     di
	pop     es

	mov     ss, cs:sssave
	mov     sp, cs:sosave
; Next instruction uses flag set by earlier cmp.
	jg      success
	or      word ptr 10[bp], 1           ; turn on carry flag on stack
	jmp     common
success:
	and     word ptr 10[bp], 0FFFEH      ; turn off carry flag on stack
common:

	pop     ds
	pop     bp
	add     sp, 4
	iret

passon:
;       Set up pass-on address
	les     bx, _oldvect
	mov     ds, cs:sssave
	mov     ds:2[bp], bx
	mov     ds:4[bp], es

;       Restore registers
	pop     ax
	pop     bx
	pop     cx
	pop     dx
	pop     si
	pop     di
	pop     es

	mov     ss, cs:sssave
	mov     sp, cs:sosave

	pop     ds
	pop     bp
	cli
	ret              ; jump to pass-on address

_newvect ENDP

	PUBLIC  _hidata
_hidata PROC    NEAR
	push    bp
	mov     bp, sp
	mov     ax, 4[bp]
	mov     cs:mydata, ax
	pop     bp
	ret
_hidata ENDP

	PUBLIC  _bef_ident
_bef_ident PROC NEAR
	push    bp
	mov     bp, sp
	push    es
	push    bx
	mov     dl, 4[bp]
	mov     ah, BEF_IDENT
	stc
	int     13H
	mov     ax, 1
	jc      idfail
	cmp	dx, BEF_MAGIC
	jne	idfail
	mov     dx, bx
	mov     bx, 6[bp]
	mov     [bx], cx
	mov     2[bx], es
	mov     bx, 8[bp]
	mov     [bx], dx
	mov     2[bx], es
	xor     ax, ax
idfail:
	pop     bx
	pop     es
	pop     bp
	ret
_bef_ident ENDP
_TEXT	ENDS
END
