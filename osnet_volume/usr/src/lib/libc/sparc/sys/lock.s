/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident " @(#)lock.s 1.7 97/12/22"

#include <sys/asm_linkage.h>

/*
 * lock_try(lp)
 *	- returns non-zero on success.
 */
	ENTRY(_lock_try)
	ldstub	[%o0], %o1		! try to set lock, get value in %o1
	retl
	xor	%o1, 0xff, %o0		! delay - return non-zero if success
	SET_SIZE(_lock_try)

/*
 * lock_clear(lp)
 *	- clear lock and force it to appear unlocked in memory by
 *	  doing an ldstub which causes this CPUs store buffer to be
 *	  flushed.
 */
	ENTRY(_lock_clear)
	sub	%sp, 4, %o1		! dummy location on stack
	clrb	[%o0]
	retl
	ldstub	[%o1], %g0
	SET_SIZE(_lock_clear)
