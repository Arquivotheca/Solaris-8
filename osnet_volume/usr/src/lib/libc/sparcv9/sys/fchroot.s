/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fchroot.s	1.1	96/12/04 SMI"	/* SVr4.0 1.6	*/

/* C library -- fchroot						*/
/* int fchroot(const int fd)					*/
 
	.file	"fchroot.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fchroot,function)

#include "SYS.h"

	SYSCALL(fchroot)
	RET

	SET_SIZE(fchroot)
