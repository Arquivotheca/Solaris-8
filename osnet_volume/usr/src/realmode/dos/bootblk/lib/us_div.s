;
; Copyright (c) 1999 by Sun Microsystems, Inc.
; All rights reserved.
;

; ident	"@(#)us_div.s	1.2	99/01/31 SMI\n"

    .MODEL SMALL, C, NEARSTACK
    .386

    .CODE			 ;code segment begins here

;[]------------------------------------------------------------[]
;|	_aNuldiv(long, short)					|
;|	This is an internal compiler call			|
;[]------------------------------------------------------------[]
PUBLIC  _aNuldiv
EXTERN tmpAX : WORD
EXTERN tmpDX : WORD
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

	END


