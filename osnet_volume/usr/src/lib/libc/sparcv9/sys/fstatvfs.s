/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)fstatvfs.s	1.3	97/02/12 SMI"

/*
 * C library -- fstatvfs
 * int fstatvfs(int fildes, struct statvfs *buf)
 */

	.file	"fstatvfs.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fstatvfs,function)

#include "SYS.h"

	SYSCALL(fstatvfs)
	RETC

	SET_SIZE(fstatvfs)
