/*
 * Copyright (c) 1994-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)hwmuldiv.s	1.5	95/08/15 SMI"

	.file	"hwmuldiv.s"

#if defined(PIC)

#include <sys/asm_linkage.h>
#include "PIC.h"

#include <sys/auxv.h>
#include <sys/archsystm.h>

/*
 * These routines use a pseudo-PLT for redirecting .mul and .div (etc.)
 * to use the hardware versions of the routines, when available.
 *
 * Note that only dynamically linked programs have access to the aux
 * vector, so static binaries always use the software routines.
 *
 * The simplest kind of PLTs might look like a table in the
 * data segment, and we do the PIC stuff to load the address of the
 * routine to jump to from the table.  However, this is expensive.
 * The next thought is to put some simple relocatable branch
 * instructions to the emulation routines in the data segment, then
 * overwrite them on the fly with branches to the hardware sequences e.g.
 *
 *	Before				After
 * r:
 *	mov	%o7, %g1		mov	%o7, %g1
 *	call	sw		 ---->	call	hw
 *	mov	%g1, %o7	|	mov	%g1, %o7
 *	call	hw	--------	call	hw
 *
 * Now once we've gone this far, then we might as well go all the way
 * (as below) - and copy the actual hardware instruction sequences over
 * whatever glue we use to vector off to the emulation routines.  Thus
 * in the case where we have the relevant hardware, it's as if we
 * dynamically linked directly to the right routine - this is as fast
 * as it can reasonably get.
 */

#define	VEC_ENTRY(r)				\
	.section	".hwmuldiv",#alloc,#write,#execinstr;	\
	.align	4;		 		\
	.type	r, #function;			\
	.global	r;				\
	.type	./**/r, #function;		\
./**/r:;					\
r:	mov	%o7, %g1;			\
	call	r/**/_sw;			\
	mov	%g1, %o7	! Must be 3 instructions of redirection

/*
 * Versions of .mul .umul .div .udiv .rem .urem written using the
 * appropriate SPARC instructions.  Only supported on later SPARC
 * implementations; so we dynamically switch to use them at run time.
 *
 * For .mul and .umul, clear the Z condition code if 32-bit overflow has
 * occurred to support cheesy applications that rely on this non-ABI-defined
 * behaviour.  See 1216117 for the ugly details.
 */

	VEC_ENTRY(.mul)
	smul	%o0, %o1, %o0
	rd	%y, %o1
	sra	%o0, 31, %o2
	retl
	cmp	%o1, %o2	! return with Z set if %y == (%o0 >> 31)
	SET_SIZE(.mul)

	VEC_ENTRY(.umul)
	umul	%o0, %o1, %o0
	rd	%y, %o1
	retl
	tst	%o1		! return with Z set if high order bits are zero
	SET_SIZE(.umul)

	VEC_ENTRY(.div)
	sra	%o0, 31, %o2
	wr	%g0, %o2, %y
	nop
	nop
	nop
	sdivcc	%o0, %o1, %o0
	bvs,a	1f
	xnor	%o0, %g0, %o0	! Corbett Correction Factor
1:	retl
	nop
	SET_SIZE(.div)

	VEC_ENTRY(.udiv)
	wr	%g0, %g0, %y
	nop
	nop
	retl
	udiv	%o0, %o1, %o0
	SET_SIZE(.udiv)

	VEC_ENTRY(.rem)
	sra	%o0, 31, %o4
	wr	%o4, %g0, %y
	nop
	nop
	nop
	sdivcc	%o0, %o1, %o2
	bvs,a	1f
	xnor	%o2, %g0, %o2	! Corbett Correction Factor
1:	smul	%o2, %o1, %o2
	retl
	sub	%o0, %o2, %o0
	SET_SIZE(.rem)

	VEC_ENTRY(.urem)
	wr	%g0, %g0, %y
	nop
	nop
	nop
	udiv	%o0, %o1, %o2
	umul	%o2, %o1, %o2
	retl
	sub	%o0, %o2, %o0
	SET_SIZE(.urem)

/*
 * Like ENTRY, but doesn't define a global symbol
 */
#define	LOCAL_ENTRY(n)		\
	.section ".text";	\
	.align	4;		\
	.type	n, #function;	\
n:

#define	RETL_INSN	0x81c3e008

/*
 * This is the core of the patch routine for fixing the
 * vectors above to contain the inlined hardware instructions.
 *
 * At entry, %o4 contains the address of the top of
 * the vector and %o5 contains the address of the beginning of
 * the hardware instructions.  We copy from the latter to the
 * former until we hit the instruction after the 'retl'.
 *
 * Pretty vile, eh?
 *
 * XXX	Note that our .fini section doesn't undo the state created
 *	by our .init section.
 */
	LOCAL_ENTRY(.__patch_vec)
	save	%sp, -SA(MINFRAME), %sp
	mov	%i4, %l0		! & beginning of vector
	mov	%i5, %l1		! & first hwinsn in vector
	set	RETL_INSN, %l3		! terminating insn pattern
	ld	[%l1], %l2
1:	inc	4, %l1			! src++
	st	%l2, [%l0]
	iflush	%l0
	inc	4, %l0			! dst++
	cmp	%l3, %l2		! stored a 'retl' ?
	bne	1b
	ld	[%l1], %l2
	st	%l2, [%l0]		! store last insn
	iflush	%l0
	ret
	restore
	SET_SIZE(.__patch_vec)
	
/*
 * %l7 contains the address of the GOT.
 * Depends on 'redirect-to-emulation' taking precisely 3 insns.
 * We use a local symbol to avoid binding confusion.
 */
#define	PATCH_VEC(nm)				\
	sethi	%hi(./**/nm), %o4;		\
	or	%o4, %lo(./**/nm), %o4;		\
	ld	[%o4 + %l7], %o4;		\
	call	.__patch_vec;			\
	add	%o4, 0xc, %o5

/*
 * We get control here when libc's .init section is fired up
 */
	.global	environ
	LOCAL_ENTRY(.__sparc_init)
	save	%sp, -SA(MINFRAME), %sp
	PIC_SETUP(l7)
	set	environ, %o0
	ld	[%o0 + %l7], %o0
	ld	[%o0], %o0
!
!	1181257	Some older runtime environments, e.g. that provided by
!		by 'acomp' from the 5.0 cross-root, don't set up their
!		'environ' correctly at the time we get to run.
!		Quietly give up on trying to 'optimize' them.
!
	tst	%o0
	beq	5f
	nop
!
!	for (cpp = environ; *cpp++; )
!		;
!	/* A pity we can't get to auxv faster than this */
!
	ld	[%o0], %o1
2:	inc	4, %o0
	tst	%o1
	bne,a	2b
	ld	[%o0], %o1
!
!	The aux vector begins at %o0 and looks like this:
!
!	AT_BASE		voffset
!	...
!	AT_SUN_HWCAP	auxv_hwcap_flags	<= this is what we want
!	...
!	AT_NULL					<= terminator
!
3:	ld	[%o0], %o1
	cmp	%o1, AT_NULL
	be	5f
	cmp	%o1, AT_SUN_HWCAP
	bne,a	3b
	inc	8, %o0
!
!	If AV_SPARC_HWMUL_32x32 is set, revector multiply; if
!	AV_SPARC_HWDIV_32x32 is set, revector divide; if both are
!	set, revector remainder too.
!
	ld	[%o0 + 4], %o0	! flags
	set	AV_SPARC_HWMUL_32x32, %o2
	andcc	%o0, %o2, %g0	! flags & AV_SPARC_HWMUL_32x32
	be	4f
	set	AV_SPARC_HWDIV_32x32, %o3
	PATCH_VEC(.mul)
	PATCH_VEC(.umul)
4:	andcc	%o0, %o3, %g0	! flags & AV_SPARC_HWDIV_32x32
	be	5f
	nop
	PATCH_VEC(.div)
	PATCH_VEC(.udiv)
	andcc	%o0, %o2, %g0	! flags & AV_SPARC_HWMUL_32x32
	be	5f
	nop
	PATCH_VEC(.rem)
	PATCH_VEC(.urem)
5:	ret
	restore
	SET_SIZE(.__sparc_init)

	.section ".init"
	.align	4
	call	.__sparc_init
	nop
#endif	/* PIC */
