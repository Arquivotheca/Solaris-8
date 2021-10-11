/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)invoke.s	1.9	99/02/09 SMI"

/*
 * Kernel function call invocation for 64-bit sun4u systems
 */


#ifndef lint
#include <sys/asm_linkage.h>
#include <sys/privregs.h>
#include <sys/stack.h>

#include "sparc.h"
#endif


#ifndef lint
	.section	RODATA
	.align		8

loadxargs:
	.xword	xarg00
	.xword	xarg01
	.xword	xarg02
	.xword	xarg03
	.xword	xarg04
	.xword	xarg05
	.xword	xarg06

loadargs:
	.xword	arg00
	.xword	arg01
	.xword	arg02
	.xword	arg03
	.xword	arg04
	.xword	arg05
	.xword	arg06
#endif


#ifdef lint
/*ARGSUSED*/
unsigned long
kernel_invoke(unsigned long (*func)(unsigned long, ...),
	      unsigned long argc,
	      unsigned long argv[]) { return (*func)(argv[0]); }
#else
	ENTRY(kernel_invoke)

	save	    %sp, -SA(MINFRAME), %sp	! standard frame

	call	    readreg		! read g6 for later restoration
	mov	    Reg_G6, %o0
	mov	    %o0, %l6

	call	    readreg		! read g7 for later restoration
	mov	    Reg_G7, %o0
	mov	    %o0, %l7

	call	    readreg		! read sp to determine stack type
	mov	    Reg_SP, %o0

	andcc	    %o0, 1, %g0		! test for 32-bit stack
	bz,pn	    %xcc, call32
	sub	    %i1, 6, %i1		! first 6 args are in registers

	 /*
	  * 64-bit kernel invocation
	  */
	brgz,pn	    %i1, push64		! check for stack args
	sllx	    %i1, 3, %i1		! convert xwords to bytes

	setx	    loadxargs+48, %l1, %l0
	ldx	    [%l0 + %i1], %l0
	jmp	    %l0			! switch on number of arguments
	nop				! (from 0 to 6)

push64:
	add	    %i1, 8, %g1		! offset to get the rounding right
	and	    %g1, -STACK_ALIGN, %g1	! round
	sub	    %sp, %g1, %sp	! allocate add'l stack space


	/*
	 * At this point, we've allocated needed stack space for
	 * args, and adjusted %i1 to be a count of the total bytes
	 * being passed on the stack.
	 *
	 * Note that %i1 >= 8, and (%i1 % 8) == 0
	 *
	 * In the loop below, we bias the pointers by -8 since
	 * %i1 == 8 in the last iteration.
	 */

	add	    %i2, 5*8, %l0	! address of argv[5]
	add	    %sp, STACK_BIAS+MINFRAME-8, %l1
					! address of arg space - 8

	ldx	    [%l0 + %i1], %l2	! last arg
1:
	stx	    %l2, [%l1 + %i1]
	subcc	    %i1, 8, %i1
	bnz,a,pt    %xcc, 1b		! stop when %i1 is 0
	ldx	    [%l0 + %i1], %l2	! next arg down

xarg06:	ldx	    [%i2 + 5*8], %o5
xarg05:	ldx	    [%i2 + 4*8], %o4
xarg04:	ldx	    [%i2 + 3*8], %o3
xarg03:	ldx	    [%i2 + 2*8], %o2
xarg02:	ldx	    [%i2 + 1*8], %o1
xarg01:	ldx	    [%i2 + 0*8], %o0
xarg00:
	mov	    %g6, %l0
	mov	    %g7, %l1

	mov	    %l6, %g6		! Restore THREAD_REG and PROC_REG
	jmpl	    %i0, %o7		! for kernel call
	mov	    %l7, %g7

	mov	    %l0, %g6
	mov	    %l1, %g7

	ret
	restore	    %g0, %o0, %o0



	 /*
	  * 32-bit kernel invocation
	  *
	  * %i1 has already been adjusted down by 6, and the
	  * condition flags set according to the result.
	  *
	  * Life's a bit tricky here.  We're going to construct a 32-bit
	  * call frame, convert %sp to a 32-bit stack pointer, then
	  * invoke our (32-bit) kernel function.  This means we're going
	  * to have a mixed mode stack.  We rely on the OBP spill/fill
	  * trap handlers to take care of this properly; the kernel's
	  * won't by default.  Remember, sun4u kadb uses the OBP trap
	  * table...
	  *
	  */
call32:
	call	    readreg		! need to know the saved %pstate
	mov	    Reg_PSR, %o0	! for later
	srlx	    %o0, TSTATE_PSTATE_SHIFT, %l4
	and	    %l4, TSTATE_PSTATE_MASK, %l4

	brgz,pn	    %i1, push32		! check for stack args
	sllx	    %i1, 2, %i1		! convert words to bytes

	setx	    loadargs+48, %l1, %l0
	add	    %i1, %l0, %l0
	ldx	    [%l0 + %i1], %l0
	jmp	    %l0			! switch on number of arguments
	nop				! (from 0 to 6)

push32:
	sub	    %i1, 8, %g1		! offset to get the rounding right
	andcc	    %g1, -STACK_ALIGN, %g1	! still need 16 byte alignment
	movneg	    %xcc, 0, %g1	! don't add to stack pointer...

	sub	    %sp, %g1, %sp	! allocate add'l stack space


	/*
	 * At this point, we've allocated needed stack space for
	 * args, and adjusted %i1 to be a count of the total bytes
	 * being passed on the stack.
	 *
	 * Note that %i1 >= 4, and (%i1 % 4) == 0
	 *
	 * In the loop below, we bias the pointers by -4 since
	 * %i1 == 4 in the last iteration.
	 */

havespace:
	add	    %i2, 5*8, %l0	! address of argv[5]
	add	    %sp, STACK_BIAS+WINDOWSIZE64+ARGPUSHSIZE32, %l1
					! address of arg space - 4

	add	    %i1, %l0, %l0	! ... %i1 was a word count
	ldx	    [%l0 + %i1], %l2	! last arg
1:
	stw	    %l2, [%l1 + %i1]
	sub	    %i1, 4, %i1
	sub	    %l0, 4, %l0
	brnz,a,pt   %i1, 1b		! stop when %i1 is 0
	ldx	    [%l0 + %i1], %l2	! next arg down

arg06:	ldx	    [%i2 + 5*8], %o5
arg05:	ldx	    [%i2 + 4*8], %o4
arg04:	ldx	    [%i2 + 3*8], %o3
arg03:	ldx	    [%i2 + 2*8], %o2
arg02:	ldx	    [%i2 + 1*8], %o1
arg01:	ldx	    [%i2 + 0*8], %o0
arg00:
	mov	    %g6, %l0
	mov	    %g7, %l1
	srlx	    %g6, 32, %l2
	srlx	    %g7, 32, %l3

	rdpr	    %pstate, %l5	! get current %pstate
	and	    %l4, PSTATE_AM, %l4	! get saved PSTATE.AM
	and	    %l4, %l5, %l4
	or	    %l4, %l5, %l4	! %l4 now has current %pstate,
					! with saved PSTATE.AM
	rdpr	    %pstate, %l5	! save current %pstate
	wrpr	    %l4, %pstate	! put in new PSTATE.AM

	mov	    %l6, %g6		! Restore THREAD_REG and PROC_REG
	mov	    %l7, %g7		! for kernel call
	jmpl	    %i0, %o7
	add	    %sp, STACK_BIAS+64, %sp
	sub	    %sp, STACK_BIAS+64, %sp
	wrpr	    %l5, %pstate	! restore saved %pstate

	srl	    %l0, 0, %l0		! clear the upper 32 bits of
	srl	    %l1, 0, %l1		! %l0 and %l1 just to be safe...
	sllx	    %l2, 32, %l2
	sllx	    %l3, 32, %l3
	or	    %l0, %l2, %g6
	or	    %l1, %l3, %g7

	ret
	restore	    %g0, %o0, %o0



	SET_SIZE(kernel_invoke)
#endif
