/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fpathconf.s	1.6	92/07/14 SMI"	/* SVr4.0 1.3	*/

/* C library -- fpathconf					*/
/* int fpathconf(int fd, int name)				*/

	.file	"fpathconf.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpathconf,function)

#include "SYS.h"

	SYSCALL(fpathconf)
	RET

	SET_SIZE(fpathconf)
