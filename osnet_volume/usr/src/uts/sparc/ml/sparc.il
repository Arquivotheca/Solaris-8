!
! Copyright (c) 1992-1998 by Sun Microsystems, Inc.
! All rights reserved.
!
#ident	"@(#)sparc.il	1.8	99/01/05 SMI"
!
! In-line functions for sparc kernels.
!

! return current thread pointer

	.inline	threadp,0
	.register %g7, #scratch
	mov	%g7, %o0
	.end

! return caller

	.inline	caller,0
	mov	%i7, %o0
	.end

! return callee

	.inline	callee,0
	mov	%o7, %o0
	.end

! needed by krtld

	.inline doflush,0
	andn	%o0, 3, %o0
	flush	%o0
	.end
!
! extern long long load_double(long long *);
!
        .inline load_double,4
        ldd     [%o0],%o0
        .end

!
! extern long long store_double(long long, long long *);
!
        .inline store_double,12
        std     %o0,[%o2]
        .end

! Reorder args from .umul to put them into 'long long' ordering
! for 64-bit multiply emulation on old hardware

	.inline	__umul32x32to64,8
	call	.umul,2
	nop
	mov	%o0, %o2
	mov	%o1, %o0
	mov	%o2, %o1
	.end

! Generates divide-by-zero trap from C for 64-bit divide emulation
! on old hardware.  In the kernel, this should cause a plain panic
! (though the old code used to silently return zero)

	.inline	__raise_divide_by_zero,8
	ta	2
	clr	%o0
	clr	%o1
	.end
