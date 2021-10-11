/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)llseek.s	1.2	96/12/19 SMI"

/*
 * C library -- llseek
 * offset_t llseek(int fildes, offset_t offset, int whence);
 */

	.file	"llseek.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(llseek,function)

#include "SYS.h"

	SYSCALL2(llseek,lseek)
	RET

	SET_SIZE(llseek)
