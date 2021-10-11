!       .asciz  "@(#)base.il	1.1"
!       Copyright (c) 1990 by Sun Microsystems, Inc.

	.inline	__add_set,3
	std	%o0,[%sp+0x48]		! save x
	ldd	[%sp+0x48],%f0
	std	%o2,[%sp+0x50]		! save y
	ldd	[%sp+0x50],%f2
	.volatile
	faddd	%f0,%f2,%f0		! compute result and leave it for return
	st	%fsr,[%sp+0x44]
	.nonvolatile
	ld	[%sp+0x44],%o0
	andcc	%o0,0x1f,%o0
	be	1f
	nop
	set	1,%o0
1:
	st	%o0,[%o4]		! store eround
	.end

	.inline	__mul_set,3
	std	%o0,[%sp+0x48]		! save x
	ldd	[%sp+0x48],%f0
	std	%o2,[%sp+0x50]		! save y
	ldd	[%sp+0x50],%f2
	.volatile
	fmuld	%f0,%f2,%f0		! compute result and leave it for return
	st	%fsr,[%sp+0x44]
	.nonvolatile
	ld	[%sp+0x44],%o0
	andcc	%o0,0x1f,%o0
	be	1f
	nop
	set	1,%o0
1:
	st	%o0,[%o4]		! store eround
	.end

	.inline	__div_set,3
	std	%o0,[%sp+0x48]		! save x
	ldd	[%sp+0x48],%f0
	std	%o2,[%sp+0x50]		! save y
	ldd	[%sp+0x50],%f2
	
	.volatile
	fdivd	%f0,%f2,%f0		! compute result and leave it for return
					! look out - -cg87 inserts fmovs which is OK
					! on -cg87 hardware - leaves cexc alone -
					! but bug on -cg89 - clears cexc!
	st	%fsr,[%sp+0x44]
	.nonvolatile
	ld	[%sp+0x44],%o0
		andcc	%o0,0x1f,%o0		! check cexc for cg89

	be	1f
	nop
	set	1,%o0
1:
	st	%o0,[%o4]		! store eround
	.end

	.inline __abs_ulp,2
	std	%o0,[%sp+0x48]          ! save x
        ldd     [%sp+0x48],%f0		! f0 gets x
	xor	%o1,1,%o1		! flip lsb
	st	%o1,[%sp+0x4c]
	fmovs	%f0,%f2
	ld 	[%sp+0x4c],%f3		! f2 gets twiddled lsb
	fsubd	%f0,%f2,%f0		! difference is an ulp
	fabss	%f0,%f0			! absolute value of ulp	
	.end

!__umac:
!	save	%sp,-96,%sp
!	sll	%i0,16,%i0
!	sll	%i1,16,%i1
!	srl	%i1,16,%o1
!	call	.umul,2
!	srl	%i0,16,%o0
!	add	%o0,%i2,%i0
!	ret
!	restore

	.inline	__umac,3
					! o0 and o1 contain short multiplier x and multiplicand y
					! o2 contains long addend

	wr	%o0, %y			! multiplier x to Y register
	addcc	%g0,%g0,%o0		! Clear o0 and condition codes.
	nop
	nop

	mulscc	%o0, %o1, %o0		! first iteration of 17
	mulscc	%o0, %o1, %o0		! second iteration of 17
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0
	mulscc	%o0, %o1, %o0		! sixteenth iteration of 17
	mulscc	%o0, %g0, %o0		! last iteration only shifts

	rd	%y, %o1
	sll	%o0, 16, %o0
	srl	%o1, 16, %o1
	or	%o1, %o0, %o0
 	add	%o0,%o2,%o0
	.end

        .inline __dabs,2
        sll	%o0,1,%o0
	srl	%o0,1,%o0	! Clear sign bit. 
	std     %o0,[%sp+0x48]  ! Double aligned part of shadow area.
        ldd     [%sp+0x48],%f0
        .end

        .inline min_normal,0
        set     0x00100000,%o0
        set     0x0,%o1
        std     %o0,[%sp+0x48]
        ldd     [%sp+0x48],%f0
        .end

        .inline max_normal,0
        set     0x7fefffff,%o0
        set     0xffffffff,%o1
        std     %o0,[%sp+0x48]
        ldd     [%sp+0x48],%f0
        .end

        .inline signaling_nan,0
        set     0x7ff00000,%o0
        set     0x1,%o1
        std     %o0,[%sp+0x48]
        ldd     [%sp+0x48],%f0
        .end

!	in _base_il.s
!        .inline	__four_digits_quick,2
!        sll     %o0,16,%o4
!        srl     %o4,16,%o4
!        srl     %o4,1,%o0
!        sethi   %hi(__four_digits_quick_table),%o2
!        or      %o2,%lo(__four_digits_quick_table),%o2 ! [internal]
!        or      %o0,3,%o0
!        add     %o0,%o2,%o0
!        ldsb    [%o0],%o3
!        and     %o4,7,%o4
!        add     %o4,%o3,%o3
!        sll     %o3,24,%o3
!        sra     %o3,24,%o3
!        mov     %o3,%o4
!        cmp     %o4,57
!        add     %o1,3,%o5
!        ble     2f
!        dec     %o0
!1:                                    ! [internal]
!        dec     10,%o4
!        stb     %o4,[%o5]
!        ldsb    [%o0],%o3
!        inc     %o3
!        sll     %o3,24,%o3
!        sra     %o3,24,%o3
!        mov     %o3,%o4
!        cmp     %o4,57
!        dec     %o5
!        bg      1b
!        dec     %o0
!2: 
!        stb     %o3,[%o5]
!        dec     %o5
!        cmp     %o5,%o1
!        blu     4f
!        nop
!3:                                    ! [internal]
!        ldsb    [%o0],%o2
!        stb     %o2,[%o5]
!        dec     %o5
!        cmp     %o5,%o1
!        bcc     3b
!        dec     %o0
!4:
!	.end

	.inline	__arint,2
	set	0x43300000,%o2		! load constant
	set	0,%o3
	std	%o2,[%sp+0x50]
	tst	%o0			! set condition codes
	sll	%o0,1,%o0
	ldd	[%sp+0x50],%f2
	srl	%o0,1,%o0		! clear sign
	std	%o0,[%sp+0x48]
	ldd	[%sp+0x48],%f0
	faddd	%f0,%f2,%f0
	fsubd	%f0,%f2,%f0
	bpos	2f
	nop
	fnegs	%f0,%f0
2:
	.end

        .inline __carry_propagate_two,2
        ba	3f
	nop
1:
        add     %o0,%o2,%o2
        sth     %o2,[%o1] 
        add     %o1,2,%o1
        srl     %o2,16,%o0
3:
        tst     %o0
        lduh    [%o1],%o2
        bne   	1b
	nop
        .end
