/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)__getloadavg.s	1.1	97/12/22 SMI"

/*
 * C library implementation -- __getloadavg()
 *
 * int __getloadavg(int *buf, int nelem);
 */

	.file	"__getloadavg.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(__getloadavg)
	SYSTRAP(getloadavg)
	SYSCERROR
	RET
	SET_SIZE(__getloadavg)
