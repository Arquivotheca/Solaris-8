/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)_fork.s	1.2	94/11/02 SMI"	/* SVr4.0 1.7	*/

/* C library -- fork						*/
/* pid_t fork(void)						*/

/*
 * From the syscall:
 * %edx == 0 in parent process, %edx == 1 in child process.
 * %eax == pid of child in parent, %eax == pid of parent in child.
 */

	.file	"_fork.s"

#include "i386/SYS.h"

	fwdef(_fork)
	SYSTRAP(fork)
	SYSCERROR(fork)
	testl	%edx, %edx	/test for child
	jz	.parent		/if 0, then parent
	xorl	%eax, %eax	/child, return (0)
.parent:			/parent, return (%eax = child pid)
	ret
