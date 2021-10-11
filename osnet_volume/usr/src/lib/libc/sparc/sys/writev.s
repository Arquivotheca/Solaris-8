/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)writev.s	1.7	98/02/27 SMI"	/* SVr4.0 1.3	*/

/* C library -- writev 						*/
/* int writev(fd, iovp, iovcnt)					*/

	.file	"writev.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.weak   _libc_writev;
	.type   _libc_writev, #function
	_libc_writev = writev

	SYSCALL_RESTART(writev)
	RET

	SET_SIZE(writev)
