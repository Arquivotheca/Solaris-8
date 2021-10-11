/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)creat.s	1.3	97/02/12 SMI"

/*
 * C library -- creat
 * int creat(char *path, mode_t mode)
 */

	.file	"creat.s"

#include "SYS.h"

	.weak	_libc_creat;
	.type	_libc_creat, #function
	_libc_creat = creat

	SYSCALL(creat)
	RET

	SET_SIZE(creat)
