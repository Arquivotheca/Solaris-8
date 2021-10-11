/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setreid.s	1.1	96/12/04 SMI"

	.file	"setreid.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(setreuid,function)
	ANSI_PRAGMA_WEAK(setregid,function)

#include "SYS.h"

	SYSCALL(setreuid)
	RET
	SET_SIZE(setreuid)

	SYSCALL(setregid)
	RET
	SET_SIZE(setregid)
