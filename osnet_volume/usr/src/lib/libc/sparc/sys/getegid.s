/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)getegid.s	1.10	92/07/14 SMI"	/* SVr4.0 1.7	*/

/* C library -- getegid						*/
/* gid_t getegid (void);					*/

	.file	"getegid.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getegid,function)

#include "SYS.h"

	ENTRY(getegid)		/* shared syscall: %o0 = gid	*/
	SYSTRAP(getgid)		/*	           %o1 = egid	*/
	retl
	mov	%o1, %o0

	SET_SIZE(getegid)
