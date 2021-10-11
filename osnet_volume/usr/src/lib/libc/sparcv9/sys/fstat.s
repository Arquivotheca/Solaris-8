/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)fstat.s	1.3	97/02/12 SMI"

/*
 * C library -- fstat
 * int fstat (int fildes, struct stat *buf)
 */

	.file	"fstat.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fstat,function)

#include "SYS.h"

	SYSCALL(fstat)
	RETC

	SET_SIZE(fstat)
