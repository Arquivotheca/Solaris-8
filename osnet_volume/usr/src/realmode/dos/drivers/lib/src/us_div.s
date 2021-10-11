;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)us_div.s	1.2	97/03/10 SMI\n"
;
; Realmode driver C language support.  Calls to this routine are generated
; by the compiler.
;

    .MODEL SMALL, C, NEARSTACK
    .386

    .CODE			 ;code segment begins here

PUBLIC  _aNuldiv
_aNuldiv:	         	 	;quotient = us_div ( )

	pusha
	push	bp
	mov	bp, sp

	add	bp, 14h

	mov	ax, [bp]                ;load both halves of dividend
	mov	dx, 2[bp]
	mov	bx, 4[bp]		;load divisor

	or	bx, bx			;check for zerodivide condition
	jz	div_out

	xor	cx, cx			;break into two division operations

	cmp	dx, bx			;can bypass upper half division?
	jb	lowerdiv

	xchg	ax, cx			;save lower half of dividend
	xchg	dx, ax			;operate on upper half first
	div	bx			;perform unsigned word division

	xchg	ax, cx			;replace lower half of dividend

lowerdiv:

	div	bx

	mov	tmpDX, cx		;restore upper half of quotient
	mov	tmpAX, ax

	pop	bp
	popa

	mov	dx, tmpDX
	mov	ax, tmpAX

	ret	8

div_out:
	pop bp
	popa

	ret	8

	_DATA	segment
tmpAX	dw	(0)
tmpDX	dw	(0)
	_DATA	ends


	END
