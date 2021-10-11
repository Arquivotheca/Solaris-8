/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)ntp_adjtime.s	1.2	96/12/02 SMI"

/* C library -- ntp_adjtime					*/
/* int ntp_adjtime (struct timex *);				*/

	.file	"ntp_adjtime.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(ntp_adjtime,function)

#include "SYS.h"

	SYSCALL(ntp_adjtime)
	RET

	SET_SIZE(ntp_adjtime)
