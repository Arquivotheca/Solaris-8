
! Copyright (c) 1989, 1997, by Sun Microsystems, Inc.
! All rights reserved.

.ident  "@(#)_base_il.s	1.12	97/05/16 SMI"

#include <sys/asm_linkage.h>
#include "SYS.h"
#include "synonyms.h"

	.seg	".text"

#ifdef NO_C_CODE
	.global	__four_digits_quick_table

	ENTRY(__four_digits_quick)
	sll	%o0,16,%o4
	srl	%o4,16,%o4
	srl	%o4,1,%o0
#ifdef PIC
	PIC_SETUP(o5)
	sethi	%hi(__four_digits_quick_table), %g5
	or	%g5, %lo(__four_digits_quick_table), %g5
	ldn	[%g5 + %o5], %g5
	ldn	[%g5], %o2
#else
	setn	__four_digits_quick_table, %o5, %o2
#endif
	or	%o0,3,%o0
	add	%o0,%o2,%o0
	ldsb	[%o0],%o3
	and	%o4,7,%o4
	add	%o4,%o3,%o3
	sll	%o3,24,%o3
	sra	%o3,24,%o3
	mov	%o3,%o4
	cmp	%o4,57
	add	%o1,3,%o5
	ble	L77004
	dec	%o0
LY1:					! [internal]
	dec	10,%o4
	stb	%o4,[%o5]
	ldsb	[%o0],%o3
	inc	%o3
	sll	%o3,24,%o3
	sra	%o3,24,%o3
	mov	%o3,%o4
	cmp	%o4,57
	dec	%o5
	bg	LY1
	dec	%o0
L77004:
	stb	%o3,[%o5]
	dec	%o5
	cmp	%o5,%o1
	blu	L77008
	nop
LY2:					! [internal]
	ldsb	[%o0],%o2
	stb	%o2,[%o5]
	dec	%o5
	cmp	%o5,%o1
	bcc	LY2
	dec	%o0
L77008:
	retl
	nop				! [internal]

	SET_SIZE(__four_digits_quick)

	ENTRY(__umac)
        save    %sp,-SA(MINFRAME),%sp
        sll     %i0,16,%o0
        srl     %o0,16,%o0
        sll     %i1,16,%o1
        srl     %o1,16,%o1
	smul	%o0,%o1,%o0
        add     %o0,%i2,%i0
        ret
        restore

	SET_SIZE(__umac)

	ENTRY(__carry_propagate_two)
        tst     %o0
        bne,a   1f
        lduh    [%o1],%o2
        retl
	nop
1:
        add     %o0,%o2,%o2
        sth     %o2,[%o1]
        add     %o1,2,%o1
        srl     %o2,16,%o0
        tst     %o0
        bne,a   1b
        lduh    [%o1],%o2
	retl
	nop

	SET_SIZE(__carry_propagate_two)
#endif /* NO_C_CODE */
!
!	Relocated from _ieee_il4.s
!
	ENTRY(__get_ieee_flags)
	st	%fsr,[%o0]
	st	%g0,[%sp+SAVE_OFFSET]
	ld	[%sp+SAVE_OFFSET],%fsr
	nop
	retl
	nop

	SET_SIZE(__get_ieee_flags)
!
!	Relocated from _ieee_il4.s
!
	ENTRY(__set_ieee_flags)
	ld	[%o0],%fsr
	nop
	retl
	nop

	SET_SIZE(__set_ieee_flags)
#ifdef NO_C_CODE
!
!	Relocated from _ieee_il4.s
!
	ENTRY(__multiply_base_two_vector)
        save    %sp,-SA(MINFRAME),%sp
!i0=n, i1=px, i2=py, i3=product
	sll	%i0,16,%i0
	srl	%i0,16,%i0
	mov	0,%l1		! l1=acc
	add	%i0,%i0,%i0
	subcc	%i0,2,%i0
	bl	3f
	mov	0,%l0		! carry
1:
	lduh	[%i1],%o0
	
					! o0 and o1 contain short multiplier x and multiplicand y
					! o2 contains long addend

	wr	%o0, %y			! multiplier x to Y register
	lduh	[%i2+%i0],%o1
	addcc	%g0,%g0,%o0		! Clear o0 and condition codes.
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
	mulscc	%o0, %o1, %o0		! 16th iteration
	mulscc	%o0, %g0, %o0		! last iteration only shifts

	rd	%y, %o1
	sll	%o0, 16, %o0
	srl	%o1, 16, %o1
 	addcc	%o0,%l1,%l1		! acc += hi p ;
	addx	%l0,%g0,%l0		! propagate carry
 	addcc	%o1,%l1,%l1		! acc += lo p ;
	addx	%l0,%g0,%l0		! propagate carry
	subcc	%i0,2,%i0
	bge,a	1b
	add	%i1,2,%i1
3:
	sth	%l1,[%i3]
	srl	%l1,16,%l1
	sth	%l1,[%i3+2]
	sth	%l0,[%i3+4]
	ret
	restore

	SET_SIZE(__multiply_base_two_vector)
#endif /* NO_C_CODE */
