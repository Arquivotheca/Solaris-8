/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)pread.s	1.5	98/02/27 SMI"	/* SVr4.0 1.9	*/

/* C library -- pread						*/
/* int pread (int fildes, void *buf, unsigned nbyte, off_t offset);	*/

	.file	"pread.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)

	.weak   _libc_pread;
	.type   _libc_pread, #function
	_libc_pread = pread

	SYSCALL_RESTART(pread)
	RET

	SET_SIZE(pread)

#else
/* C library -- pread64 transitional large file API	*/
/* ssize_t pread(int, void *, size_t, off64_t);		*/

	.weak   _libc_pread64;
	.type   _libc_pread64, #function
	_libc_pread64 = pread64

	SYSCALL_RESTART(pread64)
	RET

	SET_SIZE(pread64)

#endif
