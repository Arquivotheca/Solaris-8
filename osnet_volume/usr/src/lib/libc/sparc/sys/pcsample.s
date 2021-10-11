/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)pcsample.s	1.1	98/01/29 SMI"	/* SVr4.0 1.7	*/

/* C library -- pcsample */
/* long pcsample(uintptr_t buf[], long nsamples); */

	.file	"pcsample.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(pcsample,function)

#include "SYS.h"

	SYSCALL(pcsample)
	RET

	SET_SIZE(pcsample)
