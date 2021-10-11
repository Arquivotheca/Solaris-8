!
! Copyright (c) 1996, by Sun Microsystems, Inc.
! All rights reserved.
!
!	@(#)muldiv64.il	1.1	97/04/05 SMI

! Reorder args from .umul to put them into 'long long' ordering

	.inline	__umul32x32to64,8
	call	.umul,2
	nop
	mov	%o0, %o2
	mov	%o1, %o0
	mov	%o2, %o1
	.end

! Generates divide-by-zero trap from C

	.inline	__raise_divide_by_zero,8
	ta	2
	clr	%o0
	clr	%o1
	.end
