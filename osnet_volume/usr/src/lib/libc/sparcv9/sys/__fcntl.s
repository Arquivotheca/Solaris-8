/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)__fcntl.s	1.1	97/02/02 SMI"

/*
 * C library -- fcntl
 * int fcntl(int fildes, int cmd [, arg])
 */

	.file	"__fcntl.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL2_RESTART(__fcntl,fcntl)
	RET
	SET_SIZE(__fcntl)
