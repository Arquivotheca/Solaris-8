/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)vfork.s	1.6	92/07/14 SMI"	/* SVr4.0 1.2	*/

/* C library -- vfork						*/
/* pid_t = vfork(void);						*/

/*
 * r1 == 0 in parent process, r1 == 1 in child process.
 * r0 == pid of child in parent, r0 == pid of parent in child.
 *
 * Note that since the SPARC architecture maintains stack maintence
 * information (return pc, sp, fp) in the register windows, both parent
 * and child can execute in a common address space without conflict.
*/

	.file	"vfork.s"

#if	!defined(ABI) && !defined(DSHLIB)

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(vfork,function)

#endif	/* !defined(ABI) && !defined(DSHLIB) */

#include "SYS.h"

	SYSCALL(vfork)
	tst     %o1             ! test for child
	bnz,a   1f
	clr     %o0             ! child, return (0)
1:
	RET                     ! parent, return (%o0 = child pid)

	SET_SIZE(vfork)
