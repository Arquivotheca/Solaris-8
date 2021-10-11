;
; Copyright (c) 1999 by Sun Microsystems, Inc.
; All rights reserved.
;

; ident	"@(#)ul_mul.s	1.3	99/01/31 SMI\n"

	.MODEL SMALL, C, NEARSTACK
	.386
	.CODE			;code segment begins here

PUBLIC  _aNulmul
PUBLIC	_aNlmul
_aNulmul:			;product = ul_mul ( )
_aNlmul:
	push	bp
	push	bx

	mov	bp, sp
	add	bp, 6h

;	mov	ax, [bp]	;load both halves of dividend
;	mov	dx, 2[bp]
;	mov	bx, 4[bp]

;	mul	bx		;perform unsigned long multiplication

	xor	DWORD PTR eax, DWORD PTR eax
	mov	DWORD PTR eax, DWORD PTR [bp]
	xor	DWORD PTR ebx, DWORD PTR ebx
	mov	bx, 4[bp]

	mul	DWORD PTR ebx
	mov	DWORD PTR edx, DWORD PTR eax
	shr	DWORD PTR edx, 16

	pop	bx
	pop	bp

	ret	8

	END


































