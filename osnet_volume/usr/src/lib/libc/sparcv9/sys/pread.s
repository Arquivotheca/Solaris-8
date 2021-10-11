/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)pread.s	1.4	98/02/27 SMI"

/*
 * C library -- pread
 * ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset);
 */

	.file	"pread.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.weak   _libc_pread;
	.type   _libc_pread, #function
	_libc_pread = pread

	SYSCALL_RESTART(pread)
	RET

	SET_SIZE(pread)
