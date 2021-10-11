/*	Copyright (c) 1988 AT&T	*/
/*	  All rights reserved.	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1989-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_fork.s	1.3	97/11/06 SMI"

/* C library -- fork						*/
/* pid_t fork(void)						*/

/*
 * From the syscall:
 * %o1 == 0 in parent process, %o1 == 1 in child process.
 * %o0 == pid of child in parent, %o0 == pid of parent in child.
 */

	.file	"_fork.s"

#include "SYS.h"

	ENTRY(__fork);
	SYSTRAP(fork);
	SYSCERROR
	tst	%o1		! test for child
	bnz,a	1f		! if !0, child - jump
	clr	%o0		! child, return (0)
1:
	RET			! parent, return (%o0 = child pid)
	SET_SIZE(__fork)
