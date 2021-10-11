/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fchown.s	1.6	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* C library -- fchown						*/
/* int fchown(int fildes, uid_t owner, gid_t group)		*/

	.file	"fchown.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fchown,function)

#include "SYS.h"

	SYSCALL(fchown)
	RETC

	SET_SIZE(fchown)
