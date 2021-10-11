!	.asciz	"@(#)base.il 1.2 99/08/10 SMI"
!	Copyright (C) 1999 by Sun Microsystems, Inc.

	.inline	__add_set,0
	.volatile
	faddd	%f0,%f2,%f0		! compute result and leave it for return
	st	%fsr,[%sp+0x87f]
	.nonvolatile
	ld	[%sp+0x87f],%o0
	and	%o0,0x1f,%o0
	movrnz	%o0,1,%o0
	st	%o0,[%o2]		! store eround
	.end

	.inline	__mul_set,0
	.volatile
	fmuld	%f0,%f2,%f0		! compute result and leave it for return
	st	%fsr,[%sp+0x87f]
	.nonvolatile
	ld	[%sp+0x87f],%o0
	and	%o0,0x1f,%o0
	movrnz	%o0,1,%o0
	st	%o0,[%o2]		! store eround
	.end

	.inline	__div_set,0
	.volatile
	fdivd	%f0,%f2,%f0
	st	%fsr,[%sp+0x87f]
	.nonvolatile
	ld	[%sp+0x87f],%o0
	and	%o0,0x1f,%o0
	movrnz	%o0,1,%o0
	st	%o0,[%o2]		! store eround
	.end

	.inline	__abs_ulp,0
	st	%f1,[%sp+0x87f]
	ld	[%sp+0x87f],%o1
	xor	%o1,1,%o1		! flip lsb
	st	%o1,[%sp+0x87f]
	fmovs	%f0,%f2
	ld	[%sp+0x87f],%f3		! f2:3 gets twiddled lsb
	fsubd	%f0,%f2,%f0		! difference is an ulp
	fabsd	%f0,%f0			! absolute value of ulp
	.end

	.inline	__umac,0
!	sllx	%o0,48,%o0
!	sllx	%o1,48,%o1
!	srlx	%o0,48,%o0
!	srlx	%o1,48,%o1
	umul	%o0,%o1,%o0
	add	%o0,%o2,%o0
	srl	%o0,0,%o0
	.end

	.inline	__dabs,0
	fabsd	%f0,%f0
	.end

	.inline	min_normal,0
	set	0x00100000,%o0
	set	0x0,%o1
	std	%o0,[%sp+0x87f]
	ldd	[%sp+0x87f],%f0
	.end

	.inline	max_normal,0
	set	0x7fefffff,%o0
	set	0xffffffff,%o1
	std	%o0,[%sp+0x87f]
	ldd	[%sp+0x87f],%f0
	.end

	.inline	signaling_nan,0
	set	0x7ff00000,%o0
	set	0x1,%o1
	std	%o0,[%sp+0x87f]
	ldd	[%sp+0x87f],%f0
	.end

	.inline	__arint,0
	set	0x43300000,%o0
	set	0x0,%o1
	std	%o0,[%sp+0x87f]
	ldd	[%sp+0x87f],%f2
	fabsd	%f0,%f4
	fcmpd	%f0,%f4
	faddd	%f4,%f2,%f0
	fsubd	%f0,%f2,%f0
	fbue	2f
	nop
	fnegd	%f0,%f0
2:
	.end

	.inline	__carry_propagate_two,0
	ba	3f
	nop
1:
	add	%o0,%o2,%o2
	sth	%o2,[%o1]
	add	%o1,2,%o1
	srl	%o2,16,%o0
3:
	tst	%o0
	lduh	[%o1],%o2
	bne	1b
	nop
	.end
