/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)syscall.s	1.11	96/12/11 SMI"	/* SVr4.0 1.9	*/

/* C library -- syscall						*/
/* Interpret a given system call				*/

	.file	"syscall.s"

#if	!defined(ABI) && !defined(DSHLIB)

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(syscall,function)

#endif	/* !defined(ABI) && !defined(DSHLIB) */

#include "SYS.h"

	SYSCALL(syscall)
	RET

	SET_SIZE(syscall)
