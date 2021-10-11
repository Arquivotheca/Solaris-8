/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)p_online.s	1.1	97/07/28 SMI"

/*
 * C library -- p_online
 * int p_online(processorid_t cpu, int flag);
 */
	.file	"p_online.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL(p_online)
	RET

	SET_SIZE(p_online)
