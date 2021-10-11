/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)write.s	1.7	96/05/02 SMI"	/* SVr4.0 1.9	*/

/* C library -- write						*/
/* int write (int fildes, const void *buf, unsigned nbyte);	*/

	.file	"write.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.weak	_libc_write;
	.type	_libc_write, #function
	_libc_write = write

	SYSCALL_RESTART(write)
	RET

	SET_SIZE(write)
