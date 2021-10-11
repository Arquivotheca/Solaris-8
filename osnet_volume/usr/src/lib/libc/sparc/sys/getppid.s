/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)getppid.s	1.9	92/07/14 SMI"	/* SVr4.0 1.5	*/

/* C library -- getppid						*/
/* pid_t getppid (void);					*/

	.file	"getppid.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getppid,function)

#include "SYS.h"

	ENTRY(getppid)		/* shared syscall: %o0 = pid	*/
	SYSTRAP(getpid)		/*	           %o1 = ppid	*/
	retl
	mov	%o1, %o0

	SET_SIZE(getppid)
