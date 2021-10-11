/*	Copyright (c) 1988 AT&T	*/
/*	  All rights reserved.	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1989-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_fork1.s	1.4	97/11/06 SMI"

/* C library -- fork1						*/
/* pid_t fork1(void)						*/

/*
 * From the syscall:
 * %o1 == 0 in parent process, %o1 == 1 in child process.
 * %o0 == pid of child in parent, %o0 == pid of parent in child.
 */

	.file	"_fork1.s"

#include "SYS.h"

	ENTRY(__fork1);
	SYSTRAP(fork1);
	SYSCERROR
	tst	%o1		!test for child
	bnz,a	1f		!if !0, then child - jump
	clr	%o0		!child, return (0)
1:				!parent, return (%o0 = child pid)
	RET
	SET_SIZE(__fork1)
