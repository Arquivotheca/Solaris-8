; 	@(#)int.s	1.2
; 
	.MODEL TINY, C, NEARSTACK
	.386
	.CODE

EXTERN tmpAX : WORD
;[]------------------------------------------------------------[]
;|	_int86(intr #, in regs, out regs)			|
;|	This routine is based on the _int86 and _int86x 	|
;|	routines found in Microsofts RunTime library. The strap	|
;|	program lives in one 64Kbyte segment so it doesn't need	|
;|	any segment pointers passed. It also only supports the	|
;|	following interrupts:					|
;|	10h - character output					|
;|	13h - disk I/O						|
;|	16h - character input					|
;[]------------------------------------------------------------[]	
PUBLIC _int86
_int86:
	pusha			; save all registers
	push	es		; save the segment register 
	push	bp

	mov	bp, sp		; get current stack to find args
	add	bp, 16h		; 16bytes for regs, 4 bytes for es & bp
				; and 2 bytes for return pc
	mov	ax, ds		; segment pointer for data
	mov	es, ax		; extra seg points to data seg

	mov	di, 2[bp]	; di = union _REGS inr
	mov	ax, 0[di]
	mov	bx, 2[di]
	mov	cx, 4[di]
	mov	dx, 6[di]

	cmp	WORD PTR [bp], 13h
	jne	not_int_13		; check another function, this is
					; the most common int called
	int	13h			; bios disk subsystem
	jb	disk_err
	mov	ax, 0			; everything's ok
	jmp	comp_int
disk_err:
	mov	ax, 1			; error on disk op
	jmp	comp_int

not_int_13:
	cmp	WORD PTR [bp], 10h
	jne	not_int_10		; check another function, second 
					; most common int called
	int	10h			; bios console character output
	jmp	comp_int

not_int_10:
	cmp	WORD PTR [bp], 16h
	jne	not_int_16		; check another function

	int	16h			; bios console character input
	jmp	comp_int

not_int_16:
	cmp	WORD PTR [bp], 14h
	jne	bad_call		; I goofed. this shouldn't happen
	int	14h			; bios serial i/o
	jmp	comp_int

bad_call:
	mov	ax, 1			; error return?

comp_int:
	mov	di, 4[bp]		; di = union _REGS outr
	mov	[di], ax
	mov	2[di], bx
	mov	4[di], cx
	mov	6[di], dx
	pushf
	pop	12[di]
	
	mov	tmpAX, ax
	pop	bp
	pop	es
	popa
	mov	ax, tmpAX
	ret

	END
