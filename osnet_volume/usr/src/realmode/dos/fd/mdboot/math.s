	.MODEL TINY, C, NEARSTACK
	.386
	.CODE			;code segment begins here

EXTERN tmpAX : WORD
EXTERN tmpDX : WORD

PUBLIC  _aNulrem

_aNulrem:			;remainder = us_mod ( )

	pusha
	push	bp
	
	mov	bp, sp
	add	bp, 14h

;	a simple divide may overflow (see bug 4297325).  This algorithm
; 	forms a % b by asserting that b < 64K, and then forming
; 	(a >> 16) % b, taking that remainder and << 16, or'ing
; 	in a & 0xFFFF, and dividing again to get the remainder; this
; 	final remainder is equivalent to a % b.
; 	Proving this is difficult.
; 	Restating:
;	a % b = ((((a >> 16) % b) << 16) | (a & 0xFFFF)) % b

	mov	ax, 2[bp]	; load upper half of dividend
	mov	dx, 0		; 
	mov	bx, 4[bp]	; load divisor

	div	bx		; dx:ax = rem:quo of upper half
	mov	ax, [bp]	; dx:ax = rem of upper:low div
	div	bx		; dx:ax = rem of whole div:garbage

	mov	tmpAX, dx	; remainder to AX (eventually)
	
	pop	bp
	popa
	
	mov	ax, tmpAX
	xor	dx, dx
	ret	8

outtahere:

	pop	bp
	popa
	ret	8

;[]------------------------------------------------------------[]
;|	_aNuldiv(long, short)					|
;|	This is an internal compiler call			|
;[]------------------------------------------------------------[]
PUBLIC  _aNuldiv
_aNuldiv:	         	 	;quotient = us_div ( )

	pusha
	push	bp
	mov	bp, sp

	add	bp, 14h

	mov	ax, [bp]                ;load both halves of dividend
	mov	dx, 2[bp]
	mov	bx, 4[bp]		;load divisor

	div	bx

	mov	tmpDX, dx		;restore upper half of quotient
	mov	tmpAX, ax
     
	pop	bp
	popa
     
	mov	dx, tmpDX
	mov	ax, tmpAX

	ret	8

	END


