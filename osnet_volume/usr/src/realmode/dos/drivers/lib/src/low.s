;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)low.s	1.3	97/03/10 SMI\n"
; 
; Realmode driver support code.
;
;   File name:		low.s
;
;   Function names:	newvect   - INT 13h handler 
;			bef_ident - execute an extended BIOS BEF_IDENT call
;
;
;	Static Name Aliases
;
	TITLE   low


	_TEXT	segment  word public 'code'   ; Various encantations required
	_TEXT	ends                          ; .. by the assembler
	const	segment  word public 'const'
	const	ends
	_DATA	segment  word public 'data'

BEF_READ        equ	2
BEF_IDENT	equ	0F8h
BEF_MAGIC	equ	0BEF1h

	EXTRN   _oldvect: dword
	EXTRN	_stack: word
	EXTRN	_stack_size: word

	_DATA	ends
	_BSS	segment  word public 'bss'
	_BSS	ends

DGROUP	group   const, _BSS, _DATA
	assume  cs:_TEXT, ds:_DATA, es:nothing

_TEXT   SEGMENT
	EXTRN   _main_service: near

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
;	_newvect is the INT 13h entry point for realmode drivers.  It saves
;	the current context, switches to driver context, calls the driver
;	service routine main_service, restores the original context then
;	either returns to the caller or passes the interrupt to the next
;	interrupt handler in the chain.  Returns from main_service are
;	interpreted as follows:
;
;		0	pass on to the next handler in the chain
;		> 0	driver handled the request with no error
;		< 0	driver attempted the request but an error occurred
;
	sub     sp, 4         ;    allow room for long return
	push    bp
	mov     bp, sp
	push    ds

	mov     cs:sosave, sp
	mov     cs:sssave, ss
	
;	Calculate the data segment for the driver	
	mov	cs:mydata, cs
	sub	cs:mydata, seg _newvect
	add	cs:mydata, seg _oldvect
	
	mov     ss, cs:mydata
	mov     sp, offset _stack
	add	sp, ss:_stack_size
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

	call    _main_service

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

;	int bef_ident(ushort code, char far **name, char far **info);
;
;	Call the INT 13h BEF_IDENT function for device code 'code'
;	and return 0 for success, 1 for failure.  If successful, pass
;	back far pointers to the driver name and the bdev_info structure.
;
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
