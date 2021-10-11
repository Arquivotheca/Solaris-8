;
; Copyright (c) 1999 by Sun Microsystems, Inc.
; All rights reserved.
;

; ident	"@(#)rl_shift.s	1.3	99/01/31 SMI\n"

	.MODEL SMALL, C, NEARSTACK
	.386
	.CODE			;code segment begins here

PUBLIC  _aNlshr
PUBLIC	_aNulshr

;[]------------------------------------------------------------[]
;| right shift routines. The only difference between these	|
;| routines is that one uses the 'sar' instruction vers 'shr'	|
;|								|
;| cx has the shift value					|
;| ax has low16 bits and dx has the high16.			|
;[]------------------------------------------------------------[]
_aNulshr:
	and	DWORD PTR ecx, 0ffffh	; ... clear upper 16 bits of junk
	and	DWORD PTR edx, 0ffffh	; ... ditto
	shl	DWORD PTR edx, 16	; ... prepare to or in values
	or	DWORD PTR eax, DWORD PTR edx
	shr	DWORD PTR eax, cl	; ... 32 bit op
	mov	DWORD PTR edx, DWORD PTR eax	; ... prepare result
	shr	DWORD PTR edx, 16	; ... dx value
	ret

_aNlshr:			;product = aNrshl ( )

	and	DWORD PTR ecx, 0ffffh	; ... clear upper 16 bits of junk
	and	DWORD PTR edx, 0ffffh	; ... ditto
	shl	DWORD PTR edx, 16	; ... prepare to or in values
	or	DWORD PTR eax, DWORD PTR edx
	sar	DWORD PTR eax, cl	; ... 32 bit op
	mov	DWORD PTR edx, DWORD PTR eax	; ... prepare result
	shr	DWORD PTR edx, 16	; ... dx value
	ret

	END


