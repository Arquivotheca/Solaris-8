; Copyright (c) 1997, by Sun Microsystems, Inc.
; All rights reserved.
; ident "us_div.s 1.4	97/04/29 SMI"

    .MODEL SMALL, C, NEARSTACK
    .386

    .CODE			 ;code segment begins here

;[]------------------------------------------------------------[]
;|	_aNuldiv(long, long)					|
;|	This is an internal compiler call			|
;[]------------------------------------------------------------[]
PUBLIC  _aNuldiv
_aNuldiv:	         	 	;quotient = us_div ( )
	push	bp
	mov	bp, sp
	push	bx
	push	si
	mov	ax, WORD PTR 10[bp]
	or	ax, ax
	jnz	L12D9
	mov	cx, WORD PTR 8[bp]
	mov	ax, WORD PTR 6[bp]
	xor	dx, dx
	div	cx
	mov	bx, ax
	mov	ax, WORD PTR 4[bp]
	div	cx
	mov	dx, bx
	jmp	L1311
L12D9:	mov	cx, ax
	mov	bx, WORD PTR 8[bp]
	mov	dx, WORD PTR 6[bp]
	mov	ax, WORD PTR 4[bp]
L12E4:	shr	cx, 1
	rcr	bx, 1
	shr	dx, 1
	rcr	ax, 1
	or	cx, cx
	jnz	L12E4
	div	bx
	mov	si, ax
	mul	WORD PTR 10[bp]
	xchg	ax, cx
	mov	ax, WORD PTR 8[bp]
	mul	si
	add	dx, cx
	jb	L130D
	cmp	dx, WORD PTR 6[bp]
	ja	L130D
	jb	L130E
	cmp	ax, WORD PTR 4[bp]
	jbe	L130E
L130D:	dec	si
L130E:	xor	dx, dx
	xchg	ax, si
L1311:	pop	si
	pop	bx
	pop	bp
	ret	8
	
PUBLIC	_aNNaldiv
PUBLIC	_aNldiv
_aNNaldiv:
_aNldiv:
	push	bp
	mov	bp, sp
	push	di
	push	si
	push	bx
	xor	di, di
	mov	ax, WORD PTR 6[bp]
	or	ax, ax
	jge	L1296
	inc	di
	mov	dx, WORD PTR 4[bp]
	neg	ax
	neg	dx
	sbb	ax, 00
	mov	WORD PTR 6[bp], ax
	mov	WORD PTR 4[bp], dx
L1296:	mov	ax, WORD PTR 10[bp]
	or	ax, ax
	jge	L12AE
	inc	di
	mov	dx, WORD PTR 8[bp]
	neg	ax
	neg	dx
	sbb	ax, 00
	mov	WORD PTR 10[bp], ax
	mov	WORD PTR 8[bp], dx
L12AE:	or	ax, ax
	jnz	L12C7
	mov	cx, WORD PTR 8[bp]
	mov	ax, WORD PTR 6[bp]
	xor	dx, dx
	div	cx
	mov	bx, ax
	mov	ax, WORD PTR 4[bp]
	div	cx
	mov	bx, dx
	jmp	L12FF
L12C7:	mov	bx, ax
	mov	cx, WORD PTR 8[bp]
	mov	dx, WORD PTR 6[bp]
	mov	ax, WORD PTR 4[bp]
L12D2:	shr	bx, 1
	rcr	cx, 1
	shr	dx, 1
	rcr	ax, 1
	or	bx, bx
	jnz	L12D2
	div	cx
	mov	si, ax
	mul	WORD PTR 10[bp]
	xchg	ax, cx
	mov	ax, WORD PTR 8[bp]
	mul	si
	add	dx, cx
	jb	L12FB
	cmp	dx, WORD PTR 6[bp]
	ja	L12FB
	jb	L12FC
	cmp	ax, WORD PTR 4[bp]
	jbe	L12FC
L12FB:	dec	si
L12FC:	xor	dx, dx
	xchg	ax, si
L12FF:	dec	di
	jnz	L1309
	neg	dx
	neg	ax
	sbb	dx, 00
L1309:	pop	bx
	pop	si
	pop	di
	pop	bp
	ret	8
	_data	segment
tmpAX	dw	(0)
tmpDX	dw	(0)
	_data	ends
    

	END


