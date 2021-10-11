; Copyright (c) 1996, by Sun Microsystems, Inc.
; All rights reserved.
; ident "us_mod.s 1.2	96/04/29 SMI"

	.MODEL SMALL, C, NEARSTACK
	.386
	.CODE			;code segment begins here

PUBLIC  _aNulrem

_aNulrem:			;remainder = us_mod ( )

	pusha
	push	bp
	
	mov	bp, sp
	add	bp, 14h

	mov	ax, [bp]	; load both halves of dividend
	mov	dx, 2[bp]
	mov	bx, 4[bp]	; load divisor

	or	bx, bx		; check for zerodivide condition
	jz	outtahere

	xor	cx, cx		; break into two division operations

	cmp	dx, bx		; can bypass upper half division?
	jb	lowermod

	xchg	ax, cx		; save lower half of dividend
	xchg	dx, ax		; operate on upper half first
	div	bx		; perform unsigned word division

	xchg	ax, cx		; replace lower half of dividend

lowermod:

	div	bx

	mov	tmpAX, dx	; remainder loaded in AX by divide operation
	
	pop	bp
	popa
	
	mov	ax, tmpAX
	ret	8

outtahere:

	pop	bp
	popa
	ret	8

	_data	segment
tmpAX	dw	(0)
	_data	ends
    

	END
