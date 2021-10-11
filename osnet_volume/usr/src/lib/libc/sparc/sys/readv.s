/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)readv.s	1.7	98/02/27 SMI"	/* SVr4.0 1.3	*/

/* C library -- readv						*/
/* nt readv(int fildes, void *iovp[], int iovcnt[])		*/

	.file	"readv.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.weak   _libc_readv;
	.type   _libc_readv, #function
	_libc_readv = readv

	SYSCALL_RESTART(readv)
	RET

	SET_SIZE(readv)
