;
; Copyright (c) 1993, 1994, 1999 by Sun Microsystems, Inc.
; All rights reserved.
;
;
; ident	"@(#)dssubs.s	1.9	99/10/21 SMI\n"
;
;	Static Name Aliases
;
	TITLE   dssubs

_TEXT	SEGMENT  BYTE PUBLIC 'CODE'
_TEXT	ENDS
_DATA	SEGMENT  WORD PUBLIC 'DATA'
	EXTRN	_stack: word
	EXTRN	_stacksize: word
	extrn	_putvec: word
_DATA	ENDS
CONST	SEGMENT  WORD PUBLIC 'CONST'
CONST	ENDS
_BSS	SEGMENT  WORD PUBLIC 'BSS'
_BSS	ENDS
DGROUP	GROUP	CONST,	_BSS,	_DATA
	ASSUME  CS: _TEXT, DS: _data, SS: nothing, ES: nothing

.386

_TEXT   SEGMENT
	extrn	_new_driver_interface: near
	extrn	_AdapterInterrupt: near
	sosave	word	0
	sssave	word	0
	flgstor	word	0

	PUBLIC	_save_ds
_save_ds PROC	NEAR
	push	ax
	mov	ax, ds
	mov	word ptr cs:[privateds], ax
	pop	ax
	ret
privateds	dw	0
_save_ds ENDP

	PUBLIC	_set_ds
_set_ds PROC	NEAR
	push	ax
	mov	ax, word ptr cs:[privateds]
	mov	ds, ax
	pop	ax
	ret
_set_ds ENDP

;; _bios_intercept -- 
;; general interface to the driver which supports things like open,
;; close, read, and write. this is a private interface developed by
;; SunSoft.

	public	_bios_intercept
_bios_intercept	proc	far
	cli
	push	ds
	push	bp
	mov	bp, sp

	;; check current stack segment against ours. If its the same just
	;; run on the current stack. otherwise setup a normal stack frame
	;;
	;; bios_intercept and intr_vec share the same stack and they
	;; both check to make sure whether they are already on before
	;; switching it.  This is okay right now because they don't switch
	;; to some other stacks.  However, if new_driver_interface somehow
	;; needs to switch stack in the future, then they can't share the
	;; same stack again.
	;;

	push	ax
	mov	ax, ss
	cmp	ax, word ptr cs:[privateds]

	;; need to restore ax here in case we switch stacks and lose
	;; easy access to it.

	pop	ax

	je	bios_leave_stk

	mov	cs:sosave, sp
	mov	cs:sssave, ss
	mov	ss, word ptr cs:[privateds]
	mov	sp, offset _stack
	add	sp, ss:_stacksize

	push	1

	jmp	bios_start_stack

bios_leave_stk:

	push	0

bios_start_stack:
	sti

	;; need to setup the data segment now. it would be possible to setup
	;; the data segment above when we setup the stack and assume that
	;; if the stack segment is ours that so is the data segment. i've
	;; decided against that because what if we change model sizes so that
	;; we have multiple data segments and a single stack.

	mov	ds, word ptr cs:[privateds]
	ASSUME	ds:DGROUP

	mov	_putvec, 0

	;; squirrel away registers. this is more than saving the registers
	;; though. new_driver_interface uses the fact that they're stored
	;; on the stack as arguments.

	push	es
	mov	es, word ptr cs:[privateds]
	push	di
	push	si
	push	dx
	push	cx
	push	bx
	push	ax

	call	_new_driver_interface

	;; now restore the registers. it's possible that new_driver_interface
	;; has modified some of these.

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
	jne	bios_no_restore

	mov	ss, cs:sssave
	mov	sp, cs:sosave

bios_no_restore:

	;; clear the carry bit in the flags to notify the caller that
	;; everything is okay. stack at this point is:
	;;
	;; +-------+----+----+----+----+
	;; | flags | cs | ip | ds | bp |
	;; +-------+----+----+----+----+
	;;                          ^--- bp points here

	and	word ptr 8[bp], 0fffeh
	pop	bp
	pop	ds
	iret
_bios_intercept	endp

; intr_vec --
; the network framework has stored this far pointer routine into one
; of the hardware interrupt vectors.

	public	_intr_vec
_intr_vec	proc	far
	cli

; NOTE: this function doesn't support bios chaining.

	;; Interrupt may be taken at the time when boot.bin is calling
	;; the BIOS to do the bios wait function.  On some of the newer
	;; BIOSes, the bios wait function uses 32-bit EBP. Since boot.bin
	;; has the stack segment descriptor setup with the B-flag on (use
	;; 32-bit stack pointer ESP), make sure the upper 16 bits of EBP
	;; are zapped before continue.
	push	ebp
	xor	ebp, ebp
	push	ds

	;; check current stack segment against ours. If its the same just
	;; run on the current stack. otherwise setup a normal stack frame

	push	ax
	mov	ax, ss
	cmp	ax, word ptr cs:[privateds]

	;; need to restore ax here in case we switch stacks and lose
	;; easy access to it.

	pop	ax

	je	intr_leave_stk

	mov	cs:sosave, sp
	mov	cs:sssave, ss
	mov	ss, word ptr cs:[privateds]
	mov	sp, offset _stack
	add	sp, ss:_stacksize

	push	1

	jmp	intr_start_stack

intr_leave_stk:

	push	0

intr_start_stack:
	sti

	;; need to setup the data segment now. it would be possible to setup
	;; the data segment above when we setup the stack and assume that
	;; if the stack segment is ours that so is the data segment. i've
	;; decided against that because what if we change model sizes so that
	;; we have multiple data segments and a single stack.

	mov	ds, word ptr cs:[privateds]
	ASSUME	ds:DGROUP

	mov	_putvec, 0

	;; squirrel away registers. this is more than saving the registers
	;; though.

	push	es
	mov	es, word ptr cs:[privateds]
	push	di
	push	si
	push	dx
	push	cx
	push	bx
	push	ax

	call	_AdapterInterrupt

	;; now restore the registers. it's possible that AdapterInterrupt
	;; has modified some of these.

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
	jne	intr_no_restore

	mov	ss, cs:sssave
	mov	sp, cs:sosave

intr_no_restore:
	pop	ds
	pop	ebp
	iret
_intr_vec	endp

_TEXT	ENDS
END
