/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)cladm.s	1.1	98/07/17 SMI"

	.file	"cladm.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_cladm,function)

#include "SYS.h"

/*
 * int
 * _cladm(int fac, int cmd, void *arg)
 *
 * Syscall entry point for cluster administration.
 */
	ENTRY(_cladm)
	SYSTRAP(cladm)
	SYSCERROR
	RET
	SET_SIZE(_cladm)
