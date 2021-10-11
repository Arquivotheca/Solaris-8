/* 
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)lstat.s	1.3	97/02/12 SMI"

/*
 * C library -- lstat
 * int lstat(const char *path, struct lstat *buf);
 */

	.file	"lstat.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lstat,function)

#include "SYS.h"

	SYSCALL(lstat)
	RETC

	SET_SIZE(lstat)
