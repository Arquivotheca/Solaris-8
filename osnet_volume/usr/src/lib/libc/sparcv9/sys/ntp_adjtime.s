/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)ntp_adjtime.s	1.2	97/02/02 SMI"

/*
 * C library -- ntp_adjtime
 * int ntp_adjtime(struct timex *);
 */

	.file	"ntp_adjtime.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(ntp_adjtime,function)
	
#include "SYS.h"

	SYSCALL_RESTART(ntp_adjtime)
	RET

	SET_SIZE(ntp_adjtime)
