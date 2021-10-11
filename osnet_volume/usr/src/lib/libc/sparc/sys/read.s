/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)read.s	1.7	96/05/02 SMI"	/* SVr4.0 1.9	*/

/* C library -- read						*/
/* int read (int fildes, void *buf, unsigned nbyte);		*/

	.file	"read.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.weak	_libc_read;
	.type	_libc_read, #function
	_libc_read = read

	SYSCALL_RESTART(read)
	RET

	SET_SIZE(read)
