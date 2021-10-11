/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)processor_bind.s	1.1	97/07/28 SMI"

/*
 * C library -- processor_bind
 * processor_bind(idtype_t idtype, id_t id,
 *         processorid_t processorid, processorid_t *obind)
 */
	.file	"processor_bind.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL(processor_bind)
	RET

	SET_SIZE(processor_bind)
