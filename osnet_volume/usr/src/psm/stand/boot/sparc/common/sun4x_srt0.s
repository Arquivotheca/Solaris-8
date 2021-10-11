/*
 * Copyright (c) 1986-1992, Sun Microsystems, Inc.
 */

#pragma ident	"@(#)sun4x_srt0.s	1.57	98/03/20 SMI"

/*
 * srt0.s - standalone startup code
 */

#include <sys/asm_linkage.h>
#include <sys/psr.h>
#include <sys/cpu.h>

#if defined(lint)

/*ARGSUSED*/
void
_start(void *romp, ...)
{}

#else
	.seg	".text"
	.align	8
	.global	end
	.global	edata
	.global	main

/*
 * Initial interrupt priority and how to get there.
 */
#define	PSR_PIL_SHIFT	8
#define	PSR_PIL_INIT	(13 << PSR_PIL_SHIFT)

/*
 * The following variables are machine-dependent and are set in fiximp.
 * Space is allocated there.
 */
	.seg	".bss"
	.align	8


#define	STACK_SIZE	0x14000
	.skip	STACK_SIZE
.ebootstack:			! end --top-- of boot stack

/*
 * The following variables are more or less machine-independent
 * (or are set outside of fiximp).
 */

	.seg	".text"
	.align	8
	.global	prom_exit_to_mon
	.type	prom_exit_to_mon, #function


! Each standalone program is responsible for its own stack. Our strategy
! is that each program which uses this runtime code creates a stack just
! below its relocation address. Previous windows may, and probably do,
! have frames allocated on the prior stack; leave them alone. Starting with
! this window, allocate our own stack frames for our windows. (Overflows
! or a window flush would then pass seamlessly from our stack to the old.)
! RESTRICTION: A program running at some relocation address must not exec
! another which will run at the very same address: the stacks would collide.
!
! Careful: don't touch %o0 until the save, since it holds the romp
! for Forth PROMs.
!
! We cannot write to any symbols until we are relocated.
! Note that with the advent of 5.x boot, we no longer have to
! relocate ourselves, but this code is kept around cuz we *know*
! someone would scream if we did the obvious.
!

!
! Enter here for all booters loaded by a bootblk program or OBP.
! Careful, do not lose value of romp pointer in %o0
!

	ENTRY(_start)
	set	_start, %o1
	save	%o1, -SA(MINFRAME), %sp		! romp in %i0
	!
	! zero our bss segment (including our own private stack)
	!
	sethi	%hi(edata), %o0			! Beginning of bss
	or	%o0, %lo(edata), %o0
	set	end, %i2
	call	bzero
	sub	%i2, %o0, %o1			! end - edata = size of bss
	!
	! Switch to our own (larger) stack
	!
	set	.ebootstack, %o0
	and	%o0, ~(STACK_ALIGN-1), %o0
	sub	%o0, SA(MINFRAME), %sp

	/*
	 * Set the psr into a known state:
	 * supervisor mode, interrupt level >= 13, traps enabled
	 */
	mov	%psr, %o0
	andn	%o0, PSR_PIL, %g1
	set	PSR_S|PSR_PIL_INIT|PSR_ET, %o1
	or	%g1, %o1, %g1		! new psr is ready to go
	mov	%g1, %psr
	nop; nop; nop

	call	main			! main(romp) or main(0) for sunmon
	mov	%i0, %o0		! 0 = sunmon, other = obp romp

	call	prom_exit_to_mon	! can't happen .. :-)
	nop
	SET_SIZE(_start)

#endif	/* lint */

/*
 *  exitto is called from main() and does 2 things:
 *	it checks for a vac; if it finds one, it turns it off
 *	in an arch-specific way.
 *	It then jumps directly to the just-loaded standalone.
 *	There is NO RETURN from exitto().
 */

#if defined(lint)

/* ARGSUSED */
void
exitto(int (*entrypoint)())
{}

#else	/* lint */

	ENTRY(exitto)
	save	%sp, -SA(MINFRAME), %sp

	set	romp, %o0		! pass the romp to the callee
	clr	%o1			! boot passes no dvec
	set	bootops, %o2		! pass bootops vector to callee
	sethi	%hi(elfbootvec), %o3	! pass elf bootstrap vector
	ld	[%o3 + %lo(elfbootvec)], %o3
	clr	%o4			! 1210381 - no 1275 cif
	jmpl	%i0, %o7		! call thru register to the standalone
	ld	[%o0], %o0
	/*  there is no return from here */
	SET_SIZE(exitto)

#endif	/* lint */
