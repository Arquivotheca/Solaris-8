/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)lseek.s	1.3	97/02/12 SMI"

/*
 * C library -- lseek
 * off_t lseek(int fildes, off_t offset, int whence);
 */

	.file	"lseek.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lseek,function)

#include "SYS.h"

	SYSCALL(lseek)
	RET

	SET_SIZE(lseek)
