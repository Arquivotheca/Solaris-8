/*	Copyright (c) 1989 by Sun Microsystems, Inc.	*/

.ident	"@(#)pwrite.s	1.5	98/02/27 SMI"	/* SVr4.0 1.9	*/

/* C library -- pwrite						*/
/* int pwrite (int fildes, const void *buf, unsigned nbyte, off_t offset);	*/

	.file	"pwrite.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)

	.weak   _libc_pwrite;
	.type   _libc_pwrite, #function
	_libc_pwrite = pwrite

	SYSCALL_RESTART(pwrite)
	RET

	SET_SIZE(pwrite)

#else
/* C library -- lseek64 transitional large file API		*/
/* ssize_t pwrite(int, void *, size_t, off64_t);	*/

	.weak   _libc_pwrite64;
	.type   _libc_pwrite64, #function
	_libc_pwrite64 = pwrite64

	SYSCALL_RESTART(pwrite64)
	RET

	SET_SIZE(pwrite64)

#endif
