/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)_fork1.s	1.2	94/11/02 SMI"	/* SVr4.0 1.7	*/

/* C library -- fork						*/
/* pid_t _fork1(void)						*/

/*
 * From the syscall:
 * %edx == 0 in parent process, %edx == 1 in child process.
 * %eax == pid of child in parent, %eax == pid of parent in child.
 */

	.file	"_fork1.s"

#include <sys/asm_linkage.h>
#include "i386/SYS.h"

	fwdef(_fork1)
	SYSTRAP(fork1)
	SYSCERROR(fork1)
	testl	%edx, %edx	/test for child
	jz	.parent		/if 0, then parent
	xorl	%eax, %eax	/child, return (0)
.parent:			/parent, return (%eax = child pid)
	ret
