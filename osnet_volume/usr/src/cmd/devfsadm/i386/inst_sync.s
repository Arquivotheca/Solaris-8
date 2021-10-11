/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
	
#pragma ident	"@(#)inst_sync.s 1.2     98/07/14 SMI"

/
/ System call:
/		int inst_sync(char *pathname, int flags);
/

#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include <sys/syscall.h>

	.file	"inst_sync.s"

	ENTRY(inst_sync)

	movl	$SYS_inst_sync,%eax
	lcall	$0x7,$0
	jc	.error
	ret
.error:
	movl	%eax,errno
	movl	$-1,%eax
	ret

	SET_SIZE(inst_sync)
