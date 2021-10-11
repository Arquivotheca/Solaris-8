; Copyright (c) 1997, by Sun Microsystems, Inc.
; All rights reserved.
; ident "ul_mul.s 1.4	97/04/29 SMI"

	.MODEL SMALL, C, NEARSTACK
	.386
	.CODE			;code segment begins here

PUBLIC	_aNNalmul
PUBLIC	_aNlmul
PUBLIC  _aNulmul
_aNulmul:			;product = ul_mul ( )
_aNlmul:
_aNNalmul:
	push	bp
	mov	bp, sp
	mov	ax, WORD PTR 6[bp]
	mov	cx, WORD PTR 10[bp]
	or	cx, ax
	mov	cx, WORD PTR 8[bp]
	jnz	L1329
	mov	ax, WORD PTR 4[bp]
	mul	cx
	pop	bp
	ret	8
L1329:	push	bx
	mul	cx
	mov	bx, ax
	mov	ax, WORD PTR 4[bp]
	mul	WORD PTR 10[bp]
	add	bx, ax
	mov	ax, WORD PTR 4[bp]
	mul	cx
	add	dx, bx
	pop	bx
	pop	bp
	ret	8
	
	END
