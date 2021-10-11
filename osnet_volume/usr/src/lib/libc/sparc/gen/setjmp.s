/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1987,1989 by Sun Microsystems, Inc.	*/

.ident	"@(#)setjmp.s	1.18	96/05/06 SMI"	/* SVr4.0 1.4	*/

	.file	"setjmp.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(setjmp,function)
	ANSI_PRAGMA_WEAK(longjmp,function)

#include "synonyms.h"

#include <sys/trap.h>

JB_FLAGS	= (0*4)	! offsets in jmpbuf (see sigsetjmp.c)
JB_SP		= (1*4)	! words 5 through 11 are unused!
JB_PC		= (2*4)
JB_FP		= (3*4)
JB_I7		= (4*4)

/*
 * setjmp(buf_ptr)
 * buf_ptr points to a twelve word array (jmp_buf)
 */
	ENTRY(setjmp)
	clr	[%o0 + JB_FLAGS]	! clear flags (used by sigsetjmp)
	st	%sp, [%o0 + JB_SP]	! save caller's sp
	add	%o7, 8, %o1		! comupte return pc
	st	%o1, [%o0 + JB_PC]	! save pc
	st	%fp, [%o0 + JB_FP]	! save fp
	st	%i7, [%o0 + JB_I7]	! save %i7
	retl
	clr	%o0			! return (0)

	SET_SIZE(setjmp)

/*
 * longjmp(buf_ptr, val)
 * buf_ptr points to a jmpbuf which has been initialized by setjmp.
 * val is the value we wish to return to setjmp's caller
 *
 * We flush the register file to the stack by doing a kernel call.
 * This is necessary to ensure that the registers we want to
 * pick up are stored on the stack, and that subsequent restores
 * will function correctly.
 *
 * sp, fp, and %i7, the caller's return address, are all restored
 * to the values they had at the time of the call to setjmp().  All
 * other locals, ins and outs are set to potentially random values
 * (as per the man page).  This is sufficient to permit the correct
 * operation of normal code.
 *
 * Actually, the above description is not quite correct.  If the routine
 * that called setjmp() has not altered the sp value of their frame we
 * will restore the remaining locals and ins to the values these
 * registers had in the this frame at the time of the call to longjmp()
 * (not setjmp()!).  This is intended to help compilers, typically not
 * C compilers, that have some registers assigned to fixed purposes,
 * and that only alter the values of these registers on function entry
 * and exit.
 *
 * Since a C routine could call setjmp() followed by alloca() and thus
 * alter the sp this feature will typically not be helpful for a C
 * compiler.
 *
 * Note also that because the caller of a routine compiled "flat" (without
 * register windows) assumes that their ins and locals are preserved,
 * routines that call setjmp() must not be flat.
 */
	ENTRY(longjmp)
	ta	ST_FLUSH_WINDOWS	! flush all reg windows to the stack.
	ld	[%o0 + JB_SP], %o2	! sp in %o2 until safe to puke there
	ldd	[%o2 + (0*8)], %l0	! restore locals and ins if we can
	ldd	[%o2 + (1*8)], %l2
	ldd	[%o2 + (2*8)], %l4
	ldd	[%o2 + (3*8)], %l6
	ldd	[%o2 + (4*8)], %i0
	ldd	[%o2 + (5*8)], %i2
	ldd	[%o2 + (6*8)], %i4
	ld	[%o0 + JB_FP], %fp	! restore fp
	mov	%o2, %sp		! restore sp
	ld	[%o0 + JB_I7], %i7	! restore %i7
	ld	[%o0 + JB_PC], %o3	! get new return pc
	tst	%o1			! is return value 0?
	bnz	1f			! no - leave it alone
	sub	%o3, 8, %o7		! normalize return (for adb) (dly slot)
	mov	1, %o1			! yes - set it to one
1:
	retl
	mov	%o1, %o0		! return (val)

	SET_SIZE(longjmp)
