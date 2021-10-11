/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)creat.s	1.10	96/05/24 SMI"	/* SVr4.0 1.8	*/

	.file	"creat.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
/* C library -- creat						*/
/* int creat(char *path, mode_t mode)				*/


	.weak	_libc_creat;
	.type	_libc_creat, #function
	_libc_creat = creat

	SYSCALL(creat)
	RET

	SET_SIZE(creat)

#else
/* C library -- creat64						*/
/* int creat64(char *path, mode_t mode)				*/

	.weak	_libc_creat64;
	.type	_libc_creat64, #function
	_libc_creat64 = creat64

	SYSCALL(creat64)
	RET

	SET_SIZE(creat64)

#endif
