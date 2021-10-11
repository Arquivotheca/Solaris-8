/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)resolvepath.s	1.1	97/04/30 SMI"	/* SVr4.0 1.1	*/

/* C library -- resolvepath					*/
/* int resolvepath(const char *path, void *buf, int bufsiz)	*/

	.file	"resolvepath.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(resolvepath,function)

#include "SYS.h"

	SYSCALL(resolvepath)
	RET

	SET_SIZE(resolvepath)
