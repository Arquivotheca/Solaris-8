/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)geteuid.s	1.10	92/07/14 SMI"	/* SVr4.0 1.7	*/

/* C library -- geteuid						*/
/* uid_t geteuid (void);					*/

	.file	"geteuid.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(geteuid,function)

#include "SYS.h"

	ENTRY(geteuid)		/* shared syscall: %o0 = uid	*/
	SYSTRAP(getuid)		/*	           %o1 = euid	*/
	retl
	mov	%o1, %o0

	SET_SIZE(geteuid)
