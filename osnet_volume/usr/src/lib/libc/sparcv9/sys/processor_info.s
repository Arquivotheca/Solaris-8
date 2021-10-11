/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)processor_info.s	1.1	97/07/28 SMI"

/*
 * C library -- processor_info
 * processor_info(processorid_t processorid, processorid_t *obind)
 */
	.file	"processor_info.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL(processor_info)
	RET

	SET_SIZE(processor_info)
