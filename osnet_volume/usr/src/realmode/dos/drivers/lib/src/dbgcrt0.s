;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)dbgcrt0.s	1.3	97/03/27 SMI\n"
; 
;  Debug C Startup for realmode drivers:
;
;  This file is built into debug versions of real mode drivers for use with
;  the special debug program befdebug.  It supplies an entry point (_start)
;  and transfers control to a previously-installed copy of befdebug.  The
;  crt0 object file must still appear first in the link command because
;  befdebug invokes the driver by finding it at the start of the executable.
;  dbgcrt0.obj must be linked in somewhere later in the object file list.
;
	title   dbgcrt0
	.DOSSEG

	_text	segment  word public 'code'   ; Various incantations required
	_text	ends                          ; .. by the assembler
	const	segment  word public 'const'
	const	ends
	_data	segment  word public 'data'
	EXTRN	_stack: word
	EXTRN	_stack_size: word
	EXTRN	__end: word
signature db	"BEFDEBUG"
sign_msg  db	"BEFDEBUG is not installed.",0ah,0
befdbg	dword	0
	_data	ends

	_bss	segment  word public 'bss'
	_bss ends

;	This stack segment defines a temporary stack that
;	will be set up by DOS when it executes the program.
;	Without this, the stack would be set to cs:0 which
;	means that debuggers will clobber a few bytes at the
;	end of the 64K code segment.  In a driver with
;	more than 64K of code + data that would be within
;	the data segment.  The number of bytes can vary from
;	debugger to debugger but will be at least 6.
;
	stack	segment word stack 'stack'
__tstk	dw	16 dup (?)
	stack	ends

DGROUP	group   const, _bss, _data, stack
	assume  cs:_text, ds:nothing, es:nothing

_text	segment

extrn	_printf:near
	public	_start		; >> ENTRY POINT <<
_start	proc near
	nop                        ; A couple of no-ops just to make it a
	nop                        ; .. bit easier to find the load point
				   ; .. in a hex dump

;	Establish segment addressing and set up stack
	mov	cx, cs
	add     cx, seg _stack
	sub     cx, seg _start
	mov	ds, cx
	assume	ds:_data
	mov	dx, OFFSET _stack
	add	dx, _stack_size
	mov	ss, cx
	mov	sp, dx
	
;	BEFDEBUG advertises its entry point at vector FB.
;	Check for befdebug signature to determine whether befdebug has
;	been loaded.  We use the same interrupt vector for finding
;	befdebug as is used for network driver service because we know
;	it is available.  Network drivers will tend to seize it back, but
;	befdebug will restore it after use.
	xor	bx, bx
	mov	es, bx
	mov	bx, 3ech
	les	bx, dword ptr es:[bx]
	mov	ax, word ptr signature
	cmp	ax, es:[bx+2]
	jne	bad_sign
	mov	ax, word ptr signature+2
	cmp	ax, es:[bx+4]
	jne	bad_sign
	mov	ax, word ptr signature+4
	cmp	ax, es:[bx+6]
	jne	bad_sign
	mov	ax, word ptr signature+6
	cmp	ax, es:[bx+8]
	jne	bad_sign
	
;	Save the INT FB vector contents ready for transferring
;	to befdebug
	mov	word ptr befdbg, bx
	mov	word ptr befdbg+2, es

;	Pass the address of _end in ds:ax
	lea	ax, __end

;	Next two instructions are equivalent to INT FB but
;	work better with some debuggers which make it hard
;	to follow software interrupts.
	pushf
	call	dword ptr [befdbg]

;	Re-establish segment addressing and set up stack.
;	befdebug doesn't preserve context because our
;	stack gets reused anyway.
	mov	cx, cs
	add     cx, seg _stack
	sub     cx, seg _start
	mov	ds, cx
	assume	ds:_data
	mov	dx, OFFSET _stack
	add	dx, _stack_size
	mov	ss, cx
	mov	sp, dx
	jmp	_dos_exit
	
bad_sign:
	lea	ax, sign_msg
	push	ax
	call	_printf
	pop	ax
	
_dos_exit:
	mov	ax, 4c00h	; 4c = exit, 0 = exit code
	int	21h		; exit to DOS
_start	endp
_text	ends
	end	_start		; register "_start" as program entry point
