/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)stat.s	1.3	97/02/12 SMI"

/*
 * C library -- stat
 * int stat(const char *path, struct stat *buf);
 */

	.file	"stat.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(stat,function)
	
#include "SYS.h"

	SYSCALL(stat)
	RETC

	SET_SIZE(stat)
