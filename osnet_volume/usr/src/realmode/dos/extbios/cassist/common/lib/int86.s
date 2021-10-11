;
;  ident "int86.s 1.3       96/08/19 SMI\n"
; 
	.MODEL SMALL, C, NEARSTACK
	.386
	.CODE

EXTERN tmpAX : WORD
;[]------------------------------------------------------------[]
;|	_int86(intr #, in regs, out regs)			|
;|	This routine is based on the _int86 and _int86x 	|
;|	routines found in Microsofts RunTime library.		|
;|	Currently only supports int 10h, int 15h and int 16h	|
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

	cmp	WORD PTR [bp], 10h
	jne	not_int_10		; check another function
	int	10h
	jmp	comp_int

not_int_10:
	cmp	WORD PTR [bp], 15h
	jne	not_int_15		; check another function
	int	15h
	jmp	comp_int

not_int_15:
	cmp	WORD PTR [bp], 16h
	jne	bad_call		; check another function

	int	16h			; bios console character input
	pushf
	pop	bx			; return flags in bx for int 16h
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
	pop	ax
	and	ax, 1
	mov	12[di], ax
	
	mov	tmpAX, ax
	pop	bp
	pop	es
	popa
	mov	ax, tmpAX
	ret

	END
