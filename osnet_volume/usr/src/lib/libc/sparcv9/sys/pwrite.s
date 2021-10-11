/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)pwrite.s	1.5	98/02/27 SMI"

/*
 * C library -- pwrite
 * ssize_t pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);
 */

	.file	"pwrite.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.weak   _libc_pwrite;
	.type   _libc_pwrite, #function
	_libc_pwrite = pwrite

	SYSCALL_RESTART(pwrite)
	RET

	SET_SIZE(pwrite)
