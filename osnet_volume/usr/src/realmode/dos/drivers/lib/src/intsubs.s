;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)intsubs.s	1.2	97/03/10 SMI\n"
; 
;
;	Device interrupt support code for realmode drivers.
;
	TITLE   intsubs

_TEXT	SEGMENT  BYTE PUBLIC 'CODE'
_TEXT	ENDS
_DATA	SEGMENT  WORD PUBLIC 'DATA'
	EXTRN	_stack: word
	EXTRN	_stack_size: word
	extrn	_putvec: word
_DATA	ENDS
CONST	SEGMENT  WORD PUBLIC 'CONST'
CONST	ENDS
_BSS	SEGMENT  WORD PUBLIC 'BSS'
_BSS	ENDS
DGROUP	GROUP	CONST,	_BSS,	_DATA
	ASSUME  CS: _TEXT, DS: _DATA, SS: nothing, ES: nothing

_TEXT   SEGMENT
	rtnsave	word	0	
	argsave	word	0
	sosave	word	0
	sssave	word	0		
	flgstor	word	0

	.386		; Allow assembly of 386-specific instructions

;	void save_ds(void)
;
;	Routine to set up data for use by vec_wrap

	PUBLIC	_save_ds
_save_ds PROC	NEAR
	mov	word ptr cs:[privateds], ds
	ret
privateds	dw	0
_save_ds ENDP



; _vec_wrap
;
; Common code used to set up the stack and save registers. This
; stub is called from device interrupt handlers and network
; service interrupt handlers.
;
; Caller should push the code segment offset of the interrupt
; subroutine and a single argument then jump to _vec_wrap.
; 
; NOTE: this function doesn't support bios chaining.
	
	public	_vec_wrap
_vec_wrap	proc	near
		
	pop	cs:argsave
	pop	cs:rtnsave
	push	ds
	push	bp
	mov	bp, sp
	
;	Check current stack segment against ours. If it's the same just
;	run on the current stack. otherwise set up a normal stack frame
	
	push	ax
	mov	ax, ss
	cmp	ax, word ptr cs:[privateds]
	
;	Need to restore ax here in case we switch stacks and lose
;	easy access to it.
	
	pop	ax
	
	je	leave_stk

	mov	cs:sosave, sp
	mov	cs:sssave, ss
	mov	ss, word ptr cs:[privateds]
	mov	sp, offset _stack
	add	sp, ss:_stack_size

	push	1

	jmp	start_stack

leave_stk:

	push	0
	
start_stack:
	sti	
	
	mov	ds, word ptr cs:[privateds]
	ASSUME	ds:DGROUP

	mov	_putvec, 0
	
;	Save the registers such that they appear as arguments
	
	push	es
	mov	es, word ptr cs:[privateds]
	push	di
	push	si
	push	dx
	push	cx
	push	bx
	push	ax

	push	word ptr cs:argsave
	call	cs:rtnsave
	pop	ax		;; discard the argument
	
;	Restore the (possibly modified) registers
	pop	ax
	pop	bx
	pop	cx
	pop	dx
	pop	si
	pop	di
	pop	es

	cli
	pop	flgstor
	cmp	flgstor, 1
	jne	no_restore

	mov	ss, cs:sssave
	mov	sp, cs:sosave

no_restore:
       
	pop	bp
	pop	ds
	iret
_vec_wrap	endp
		
_TEXT	ENDS
END
