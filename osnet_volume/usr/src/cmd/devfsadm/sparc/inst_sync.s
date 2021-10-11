/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.	
 */

#pragma	ident	"@(#)inst_sync.s 1.2     98/07/14 SMI"

/*
 * System call:
 *		int inst_sync(char *pathname, int flags);
 */

#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include <sys/syscall.h>

	.file	"inst_sync.s"

	.global	_cerror
#ifdef notdef
	ANSI_PRAGMA_WEAK(inst_sync,function)
#endif
	ENTRY(inst_sync)

	mov	SYS_inst_sync, %g1
	t	ST_SYSCALL
	bcs	_cerror
	nop
	retl
	nop

	SET_SIZE(inst_sync)
