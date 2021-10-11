/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)syscall.s	1.6	97/10/10 SMI"

/*
 * C library -- int syscall(int sysnum, ...);
 *
 * Interpret a given system call
 *
 * This version handles up to 8 'long' arguments to a system call.
 *
 * There is no indirect system call support in the 64-bit kernel,
 * so the real trap for the desired system call is issued right here.
 *
 * Only a forced 'int' return is supported, even though many system
 * calls in the 64-bit world really return 'long' quantities.
 * Perhaps we should invent a "lsyscall()" for that purpose.
 */

	.file	"syscall.s"

#if	!defined(ABI) && !defined(DSHLIB)

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(syscall,function)

#endif	/* !defined(ABI) && !defined(DSHLIB) */

#include "SYS.h"

	ENTRY(syscall)
	save	%sp, -SA(MINFRAME + 2*CLONGSIZE), %sp
	ldx	[%fp + STACK_BIAS + MINFRAME], %o5	! arg 5
	mov	%i3, %o2				! arg 2
	ldx	[%fp + STACK_BIAS + MINFRAME + CLONGSIZE], %g5
	mov	%i4, %o3				! arg 3
	stx	%g5, [%sp + STACK_BIAS + MINFRAME]	! arg 6
	mov	%i5, %o4				! arg 4
	ldx	[%fp + STACK_BIAS + MINFRAME + 2*CLONGSIZE], %g5
	mov	%i0, %g1				! sysnum
	stx	%g5, [%sp + STACK_BIAS + MINFRAME + CLONGSIZE]	! arg 7
	mov	%i1, %o0				! arg 0
	mov	%i2, %o1				! arg 1
	ta	SYSCALL_TRAPNUM
	bcc,a,pt	%icc, 1f
	  sra	%o0, 0, %i0				! (int) cast
	restore	%o0, 0, %o0
	mov	%o7, %g1
	call	_cerror
	mov	%g1, %o7
1:
	ret
	  restore
	SET_SIZE(syscall)
