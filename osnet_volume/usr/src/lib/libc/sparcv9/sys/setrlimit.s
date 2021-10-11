/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)setrlimit.s	1.3	97/02/12 SMI"

/*
 * C library -- setrlimit
 * int setrlimit(int resource, const struct rlimit *rlp)
 */

	.file	"setrlimit.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(setrlimit,function)

#include "SYS.h"

	SYSCALL(setrlimit)
	RET

	SET_SIZE(setrlimit)
