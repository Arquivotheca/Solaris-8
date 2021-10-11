;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.

; GetBiosType:
;
;	Checks the bios type of the host machine in an attempt to determine
;	what sort of expansion bus is present.  Returns ...
;
;		-1   ...  If this is an MCA bios (implying that the expan-
;			  sion bus is micro-channel).
;
;		 1   ...  If this is an EISA bios (implying an EISA expansion
;			  bus).
;
;		 0   ...  If neither (caller may assume ISA expansion bus).

_text	segment	word public 'CODE'
	db	"@(#)biostype.s 1.5     95/03/23 SMI\n"

public	_GetBiosType
_GetBiosType proc

	push	es		; Save some registers
	push	bx
	mov	ah,0C0h		; Ask mca bios to identify itself
	int	15h
	mov	dl,es:[bx+5]	; Pick up bus type byte from ROM
	mov	ax,-1		; Assume it's MCA
	and	dl,2		; Did we guess correctly?
	jne	short fin	; .. branch if so

	mov	ax,0F000h	; Load segment identifier for ROM
	mov	es,ax
	xor	ax,ax		; Assume we'll fail the EISA test
	mov	bx,0FFD9h	; Check for "EISA" signature at the proper
	cmp	es:[bx],"IE"	; .. ROM location
	jne	short fin
	cmp	es:[bx+2],"AS"
	jne	short fin	; Nope, return 0
	inc	ax		; Yep, return 1

fin:	pop	bx
	pop	es
	retf

_GetBiosType	endp

;  IsPCI:
;
;	Returns a non-zero value if BIOS function 1A/B1 recognizes a PCI
;	bus

public	_IsPCI
_IsPCI	proc

	mov	ax, 0B101h
	int	1Ah
	jc	short no
	or	ah, ah
	jnz	short no
	cmp	dx, "CP"
	jne	short no
	mov	cl, 16
	db	66h
	shr	dx, cl
	cmp	dx, " I"
	jne	short no
	mov	ax, 1
	retf

no:	xor	ax, ax
	retf

_IsPCI	endp
_text	ends
	end
