/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)statvfs.s	1.3	97/02/12 SMI"

/*
 * C library -- statvfs
 * int statvfs(const char *path, struct statvfs *statbuf)
 */

	.file	"statvfs.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(statvfs,function)

#include "SYS.h"

	SYSCALL(statvfs)
	RETC

	SET_SIZE(statvfs)
