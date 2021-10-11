; Copyright (c) 1993, 1994 Sun Microsystems, Inc.  All rights reserved.
;
; ident	"@(#)setup.s	1.9	95/05/16 SMI\n"
;
; Solaris Primary Boot Subsystem - BIOS Extension Driver Support Routines
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   File name:		setup.s
;
;   Function names:	setup     - preamble for initmain
;			bef_ident - issues the INT 13h, function F8h call
;			reboot    - issues an INT 19h
;			hang      - the black hole......
;
;   Calling Syntax:	see source below
;
;
;	Static Name Aliases
;
	TITLE   setup
	.DOSSEG			; get _edata and _end for BSS clearing

CONST	SEGMENT  WORD PUBLIC 'CONST'
CONST	ENDS
_BSS	SEGMENT  WORD PUBLIC 'BSS'
_BSS	ENDS
DGROUP   GROUP CONST, _BSS, _DATA

BEF_READ        equ	2
BEF_IDENT	equ	0F8h
BEF_MAGIC	equ	0BEF1h


_DATA      SEGMENT   WORD PUBLIC 'DATA'
_DATA      ENDS

_BSS      SEGMENT
sustack	  db 2048 dup (?)
esustk	  label word
_BSS      ENDS

_TEXT      SEGMENT   WORD PUBLIC 'CODE'

	ASSUME  CS: _TEXT, DS: NOTHING, SS: NOTHING, ES: NOTHING
	EXTRN   _initmain: near

	PUBLIC  _setup	
_setup  PROC    FAR
;       Set up segment registers and a local stack
	mov     ax, cs
	add     ax, seg esustk
	sub     ax, seg _setup
	mov     ds, ax
	mov     es, ax

;	Clear BSS (from _edata to _end)
	EXTRN	__edata:BYTE
	EXTRN 	__end:BYTE
	mov	di, offset DGROUP:__edata
	mov	cx, offset DGROUP:__end
	sub	cx, di
	xor	ax, ax
	cld
	rep	stosb		; clear it all to 0

;       Pop return address off old stack
	pop     cx
	pop     dx

	mov	ax, ds
	mov     ss, ax
	lea     sp, esustk
	ASSUME  DS: _DATA, SS: _DATA, ES: _DATA

;       Push return address on local stack
	push    dx
	push    cx

	call    _initmain

	PUBLIC  exit
exit::
	ret
_setup  ENDP

;       Non-resident utility routines go here
;
;       bef_ident(unsigned short dev, char far **contstr, char far **devstr)
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

;       int bef_read0(unsigned short dev, int count, char *buffer)
;
;	Read "count" sectors from start of device "dev" into "buffer".
;	Return 1 for success, 0 for failure.
;
	PUBLIC  _bef_ident
_bef_read0 PROC NEAR
	push    bp
	mov     bp, sp
	push    es
	push    bx
	push	ds
	pop	es
	mov     ah, BEF_READ
	mov	al, 6[bp]
	mov     dl, 4[bp]
	mov	bx, 8[bp]
	xor	dh, dh
	mov	cx, 1
	int     13H
	mov     ax, 0
	jc      rdfail
	mov     ax, 1
rdfail:
	pop     bx
	pop     es
	pop     bp
	ret
_bef_read0 ENDP

	PUBLIC  _reboot
_reboot PROC    NEAR
	int     19H
_reboot ENDP

	PUBLIC  _hang
_hang   PROC NEAR
hang:
	jmp     hang
_hang   ENDP

_TEXT	ENDS
END

