/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)getdents.s	1.3	97/02/12 SMI"

/*
 * C library -- getdents
 * int getdents (int fildes, struct dirent *buf, size_t count)
 */

	.file	"getdents.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getdents,function)

#include "SYS.h"

	SYSCALL(getdents)
	RET

	SET_SIZE(getdents)
