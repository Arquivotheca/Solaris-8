/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)execve.s	1.6	92/07/14 SMI"	/* SVr4.0 1.7	*/

/* C library -- execve						*/
/* int execve(const char *path, const char *argv[], const char *envp[])
 *
 * where argv is a vector argv[0] ... argv[x], 0
 * last vector element must be 0
 */

	.file	"execve.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(execve,function)

#include "SYS.h"

	ENTRY(execve)
	SYSTRAP(execve)
	SYSCERROR

	SET_SIZE(execve)
