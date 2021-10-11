/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)ntp_gettime.s	1.2	97/02/02 SMI"

/*
 * C library -- ntp_gettime
 * int ntp_gettime(struct ntptimeval *);
 */

	.file	"ntp_gettime.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(ntp_gettime,function)

#include "SYS.h"

	SYSCALL_RESTART(ntp_gettime)
	RET

	SET_SIZE(ntp_gettime)
