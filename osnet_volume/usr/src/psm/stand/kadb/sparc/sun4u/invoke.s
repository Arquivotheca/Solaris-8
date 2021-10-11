/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)invoke.s	1.4	97/11/25 SMI"

/*
 * Kernel function call invocation for 32-bit sun4u systems
 */


#include <sys/asm_linkage.h>

#ifndef lint
	.section	RODATA
	.align		4

loadargs:
	.word	arg00
	.word	arg01
	.word	arg02
	.word	arg03
	.word	arg04
	.word	arg05
	.word	arg06
#endif


#ifdef lint
/*ARGSUSED*/
unsigned long
kernel_invoke(unsigned long (*func)(unsigned long, ...),
	      unsigned long argc,
	      unsigned long argv[]) { return (*func)(argv[0]); }
#else
	ENTRY(kernel_invoke)

	save	    %sp, -104, %sp	! standard frame plus space for
					! saved %g6 and %g7

	call	    readreg		! read g6 for later restoration
	mov	    6, %o0		! really need the symbolic Reg_G6...
	mov	    %o0, %l6

	call	    readreg		! read g7 for later restoration
	mov	    7, %o0		! really need the symbolic Reg_G7...
	mov	    %o0, %l7

	subcc	    %i1, 6, %i1		! first 6 args are in registers
	bgu,pn	    %icc, pushargs	! check for stack args
	sll	    %i1, 2, %i1		! convert words to bytes

	set	    loadargs+24, %l0
	lduw	    [%l0 + %i1], %l0
	jmp	    %l0
	nop

pushargs:
	and	    %i1, -8, %g1	! round to 8-byte boundary
	restore	    %g1, 104, %g1	! bias for extra space required
	sub	    %g0, %g1, %g1	! must subtract from %sp
	save	    %sp, %g1, %sp	! allocate stack space


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

	add	    %i2, 20, %l0	! address of argv[5]
	add	    %sp, 88, %l1	! address of arg space - 4
	lduw	    [%l0 + %i1], %l2	! last arg
1:
	stw	    %l2, [%l1 + %i1]
	subcc	    %i1, 4, %i1
	bnz,a,pt    %icc, 1b		! stop when %i1 is 0
	lduw	    [%l0 + %i1], %l2	! next arg down

arg06:	lduw	    [%i2 + 5*4], %o5
arg05:	lduw	    [%i2 + 4*4], %o4
arg04:	lduw	    [%i2 + 3*4], %o3
arg03:	lduw	    [%i2 + 2*4], %o2
arg02:	lduw	    [%i2 + 1*4], %o1
arg01:	lduw	    [%i2 + 0*4], %o0
arg00:
	stw	    %g6, [%fp - 8]
	stw	    %g7, [%fp - 4]

	mov	    %l6, %g6		! Restore THREAD_REG and PROC_REG
	jmpl	    %i0, %o7		! for kernel call
	mov	    %l7, %g7

	lduw	    [%fp - 8], %g6
	lduw	    [%fp - 4], %g7

	ret
	restore	    %g0, %o0, %o0

	SET_SIZE(kernel_invoke)
#endif
