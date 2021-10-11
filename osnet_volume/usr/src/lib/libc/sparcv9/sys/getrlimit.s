/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)getrlimit.s	1.3	97/02/12 SMI"

/*
 * C library -- getrlimit
 * int getrlimit(int resources, struct rlimit *rlp)
 */

	.file	"getrlimit.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getrlimit,function)

#include "SYS.h"

	SYSCALL(getrlimit)
	RET

	SET_SIZE(getrlimit)
