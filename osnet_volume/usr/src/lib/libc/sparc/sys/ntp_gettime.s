/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)ntp_gettime.s	1.2	96/12/02 SMI"

/* C library -- ntp_gettime					*/
/* int ntp_gettime (struct ntptimeval *);			*/

	.file	"ntp_gettime.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(ntp_gettime,function)

#include "SYS.h"

	SYSCALL(ntp_gettime)
	RET

	SET_SIZE(ntp_gettime)
