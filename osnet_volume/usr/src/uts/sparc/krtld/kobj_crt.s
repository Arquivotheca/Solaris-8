/*
 * Copyright (c) 1986-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kobj_crt.s	1.5	96/09/28 SMI"

#include <sys/asm_linkage.h>
#include <sys/stack.h>
#include <sys/trap.h>

/* XXX No sharing here -- make this into two files */

#ifdef __sparcv9

/*
 * Exit routine from linker/loader to kernel.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
exitto(caddr_t entrypoint)
{}

#else	/* lint */

	ENTRY_NP(exitto)
	save	%sp, -SA(MINFRAME64), %sp
	set	romp, %o0			! pass the romp to the callee
	set	dbvec, %o1			! pass debug vector
	ldx	[%o1], %o1 
	set	ops, %o2			! pass the bootops
	ldx	[%o2], %o2
	jmpl	%i0, %o7
	ldx	[%o0], %o0
	/*  there is no return from here */
	SET_SIZE(exitto)

#endif	/* lint */

#else	/* __sparcv9 */

#if !defined(lint) && !defined(__lint)

	ENTRY_NP(.mul)
	mov	%o1, %y			! multiplicand to Y register
	andcc	%g0, %g0, %o4		! zero the partial product
					! and clear N and V conditions
	set	xx, %o5			! jump address after 31 interations
mul_longway:
	mulscc	%o4, %o0, %o4		! first iteration of 33
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	mulscc	%o4, %o0, %o4
	jmp	%o5
	mulscc	%o4, %o0, %o4		! 31st iteration

xx:
	sub	%g0, %o0, %o5		! negate the multiplicand
	mulscc	%o4, %o5, %o4		! 32nd iteration
	mulscc	%o4, %g0, %o4		! last iteration only shifts
	!
	! if you are not interested in detecting overflow,
	! remove the following branch (KEEP the rd!) and subcc
	!
	bge	1f			! skip negative overflow check
	rd	%y, %o0			! return least sig. bits of prod
	subcc	%o4, -1, %g0		! set Z if high order bits
					! are -1.
1:
	retl
	mov	%o4, %o1		! delay slot
	SET_SIZE(.mul)

/*
 * procedure to perform a 32 by 32 unsigned multiply.
 * pass the multiplicand into %o0, and the multiplier into %o1
 * the least significant 32 bits of the result will be returned in %o0,
 * and the most significant in %o1
 *
 * THE FOLLOWING IS NOT TRUE (yet):
 * This code indicates that overflow has occured, by leaving the Z condition
 * code clear. The following call sequence would be used if you wish to
 * deal with overflow:
 *
 *	 	call	.umul
 *		nop		( or set up last parameter here )
 *		bnz	overflow_code	(or tnz to overflow handler)
 */
	ENTRY_NP(.umul)
	mov	%o1, %y			! multiplicand to Y register
	andcc	%g0, %g0, %o4		! zero the partial product
					! and clear N and V conditions
					! bits of prod.
	!
	! long multiply
	!
umul_longway:
	! must detect and mask high-order bit of multiplicand,
	! since it will be (mis-)interpreted by the mulscc instruction
	! as a sign bit.
	addcc	%o0, %g0, %o2		! i.e. move, set cc
	bge	1f
	sethi	%hi(0x80000000), %o3
	andncc	%o0, %o3, %o0		! clear the bit, clear cc
1:
	set	uxx, %o5		! jump address after 31 interations
	b,a	mul_longway
uxx:
	mulscc	%o4, %o0, %o4		! 32nd iteration
	! hign-order correction
	! first, collect the outgoing carry
unsigned_correction:
	bl	1f
	mov	%o3, %o5		! (delay)
	mov	0, %o5			! %o5 has carry-out in high-order bit
1:	! now decide if we need to correct
	! consult saved copy of multiplicand, in %o2, to see if sign is set
	tst	%o2
	bpos	2f
	tst	%o5	! (delay) set SIGNED cc on carry-out we've collected
	! apply correction factor
	addcc	%o4, %o1, %o4
	! or-in carry-out
	bcc	1f
	tst	%o5	! (delay) set SIGNED cc on carry-out we've collected
	orcc	%o5, %o3, %o5 ! re-set SIGNED likewise
1:
2:
	mulscc	%o4, %g0, %o4		! last iteration collects carry-out
	!
	! should test for significant bits in the high-order word here
	!
	! beq	1f			! nothing here
	rd	%y, %o0			! return least sig. bits of prod
!1:
	retl
	mov	%o4, %o1		! delay slot
	SET_SIZE(.umul)

/*
 * divison/remainder
 *
 * Input is:
 *	dividend -- the thing being divided
 * divisor  -- how many ways to divide
 * Important parameters:
 *	N -- how many bits per iteration we try to get
 *		as our current guess: 
 *	WORDSIZE -- how many bits altogether we're talking about:
 *		obviously: 
 * A derived constant:
 *	TOPBITS -- how many bits are in the top "decade" of a number:
 *		
 * Important variables are:
 *	Q -- the partial quotient under development -- initally 0
 *	R -- the remainder so far -- initially == the dividend
 *	ITER -- number of iterations of the main division loop will
 *		be required. Equal to CEIL( lg2(quotient)/2 )
 *		Note that this is log_base_(2^2) of the quotient.
 *	V -- the current comparand -- initially divisor*2^(ITER*2-1)
 * Cost:
 *	current estimate for non-large dividend is 
 *		CEIL( lg2(quotient) / 2 ) x ( 10 + 72/2 ) + C
 *	a large dividend is one greater than 2^(31-2 ) and takes a 
 *	different path, as the upper bits of the quotient must be developed 
 *	one bit at a time.
 */

/*
 * this is the recursive definition of how we develop quotient digits.
 * it takes three important parameters:
 *	$1 -- the current depth, 1<=$1<=2
 *	$2 -- the current accumulation of quotient bits
 *	2  -- max depth
 * We add a new bit to $2 and either recurse or 
 * insert the bits in the quotient.
 * Dynamic input:
 *	%o3 -- current remainder
 *	%o2 -- current quotient
 *	%o5 -- current comparand
 *	cc -- set on current value of %o3
 * Dynamic output:
 * %o3', %o2', %o5', cc'
 */

	ENTRY_NP(.rem)
	mov	1,%g4		! signify remainder
	orcc	%o1,%o0,%g0	! are either %o0 or %o1 negative
	bge	divide
	mov	%o0, %g1	! record sign of result in sign of %g1
	b,a	fixops

	ENTRY_NP(.urem)
	mov	1,%g4		! signify remainder
	b	divide		! next instruction needed in delay slot
	.empty			! keep assembler from complaining
	ENTRY_NP(.udiv)		! UNSIGNED DIVIDE
	mov	0, %g1		! result always positive
	b	divide		! next instruction needed in delay slot
	.empty			! keep assembler from complaining
	ENTRY_NP(.div)		! SIGNED DIVIDE
	mov	0, %g4		! signify divide
	orcc	%o1,%o0,%g0	! are either %o0 or %o1 negative
	bge	divide		! if not, skip this junk
	xor	%o1,%o0,%g1	! record sign of result in sign of %g1
fixops:
		tst	%o1
		bge	2f
		tst	%o0
	!	%o1 < 0
		bge	divide
		neg	%o1
	2:
	!	%o0 < 0
		neg	%o0
	!	FALL THROUGH

divide:
!	compute size of quotient, scale comparand
	orcc	%o1,%g0,%o5	! movcc	%o1,%o5
	te	ST_DIV0		! if %o1 = 0
	mov	%o0,%o3
	cmp     %o3,%o5
	blu     got_result ! if %o3<%o5 already, there's no point in continuing
	mov	0,%o2
	sethi	%hi(1<<(32-2 -1)),%g2
	cmp	%o3,%g2
	blu	not_really_big
	mov	0,%o4
	!
	! here, the %o0 is >= 2^(31-2) or so. We must be careful here, as
	! our usual 2-at-a-shot divide step will cause overflow and havoc. The
	! total number of bits in the result here is 2*%o4+%g3, where %g3 <= 2.
	! compute %o4, in an unorthodox manner: know we need to Shift %o5 into
	!	the top decade: so don't even bother to compare to %o3.
	1:
		cmp	%o5,%g2
		bgeu	3f
		mov	1,%g3
		sll	%o5,2,%o5
		b	1b
		inc	%o4
	! now compute %g3
	2:	addcc	%o5,%o5,%o5
		bcc	not_too_big ! bcc	not_too_big
		add	%g3,1,%g3
			!
			! here if the %o1 overflowed when Shifting
			! this means that %o3 has the high-order bit set
			! restore %o5 and subtract from %o3
			sll	%g2,2 ,%g2 ! high order bit
			srl	%o5,1,%o5 ! rest of %o5
			add	%o5,%g2,%o5
			b	do_single_div
			sub	%g3,1,%g3
	not_too_big:
	3:	cmp	%o5,%o3
		blu	2b
		nop
		be	do_single_div
		nop
	! %o5 > %o3: went too far: back up 1 step
	!	srl	%o5,1,%o5
	!	dec	%g3
	! do single-bit divide steps
	!
	! we have to be careful here. We know that %o3 >= %o5, so we can do the
	! first divide step without thinking. BUT, the others are conditional,
	! and are only done if %o3 >= 0. Because both %o3 and %o5 may have the high-
	! order bit set in the first step, just falling into the regular 
	! division loop will mess up the first time around. 
	! So we unroll slightly...
	do_single_div:
		deccc	%g3
		bl	end_regular_divide
		nop
		sub	%o3,%o5,%o3
		mov	1,%o2
		b,a	end_single_divloop
	single_divloop:
		sll	%o2,1,%o2
		bl	1f
		srl	%o5,1,%o5
		! %o3 >= 0
		sub	%o3,%o5,%o3
		b	2f
		inc	%o2
	1:	! %o3 < 0
		add	%o3,%o5,%o3
		dec	%o2
	2:	
	end_single_divloop:
		deccc	%g3
		bge	single_divloop
		tst	%o3
		b,a	end_regular_divide

not_really_big:
1:	
	sll	%o5,2,%o5
	cmp	%o5,%o3
	bleu	1b
	inccc	%o4
	be	got_result
	dec	%o4
do_regular_divide:

!	do the main division iteration
	tst	%o3
!	fall through into divide loop
divloop:
	sll	%o2,2,%o2
		!depth 1, accumulated bits 0 
	bl	L.1.4
	srl	%o5,1,%o5
	! remainder is positive
	subcc	%o3,%o5,%o3
			!depth 2, accumulated bits 1
	bl	L.2.5
	srl	%o5,1,%o5
	! remainder is positive
	subcc	%o3,%o5,%o3
		b	9f
		add	%o2, (1*2+1), %o2
	
L.2.5:	! remainder is negative
	addcc	%o3,%o5,%o3
		b	9f
		add	%o2, (1*2-1), %o2
	
L.1.4:	! remainder is negative
	addcc	%o3,%o5,%o3
			!depth 2, accumulated bits -1
	bl	L.2.3
	srl	%o5,1,%o5
	! remainder is positive
	subcc	%o3,%o5,%o3
		b	9f
		add	%o2, (-1*2+1), %o2
	
L.2.3:	! remainder is negative
	addcc	%o3,%o5,%o3
		b	9f
		add	%o2, (-1*2-1), %o2

9:
end_regular_divide:
	deccc	%o4
	bge	divloop
	tst	%o3
	
	tst	%g4
	bz	divout
	tst	%o3


	bl,a    got_result
        add     %o3,%o1,%o3
	b,a	remresult
divout:
	bl,a	got_result
	dec	%o2


got_result:
	tst	%g4
	bz	divresult
	.empty			! keep assembler from complaining
remresult:
        tst     %g1
        bl,a    1f
        neg     %o3     ! remainder <- -%o3
1:
        retl
        mov     %o3,%o0 ! remainder <-  %o3
	SET_SIZE(.urem)
	SET_SIZE(.rem)

divresult:
	bl,a	1f
	neg	%o2	! quotient  <- -%o2
1:
	retl
	mov	%o2,%o0	! quotient  <-  %o2
	SET_SIZE(.udiv)
	SET_SIZE(.div)
#endif	/* !defined(lint) && !defined(__lint) */

/*
 * Exit routine from linker/loader to kernel.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
exitto(caddr_t entrypoint)
{}

#else	/* lint */

	ENTRY_NP(exitto)
	save	%sp, -SA(MINFRAME), %sp

	sethi	%hi(romp), %o0			! pass the romp to the callee
	sethi	%hi(dbvec), %o1			! pass debug vector
	ld	[%o1 + %lo(dbvec)], %o1 	!
	sethi	%hi(ops), %o2			! pass bootops
	ld	[%o2 + %lo(ops)], %o2		!
	jmpl	%i0, %o7			! call thru register to the standalone
	ld	[%o0 + %lo(romp)], %o0
	/*  there is no return from here */
	SET_SIZE(exitto)

#endif	/* lint */

#endif	/* __sparcv9 */
