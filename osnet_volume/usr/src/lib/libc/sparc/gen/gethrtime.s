/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)gethrtime.s	1.1	93/01/25 SMI"

	.file	"gethrtime.s"

#include <sys/asm_linkage.h>

/*
 * hrtime_t gethrtime(void)
 *
 * Returns the current hi-res real time.
 */

	ENTRY(gethrtime)
	ta	ST_GETHRTIME
	retl
	nop
	SET_SIZE(gethrtime)

/*
 * hrtime_t gethrvtime(void)
 *
 * Returns the current hi-res LWP virtual time.
 */

	ENTRY(gethrvtime)
	ta	ST_GETHRVTIME
	retl
	nop
	SET_SIZE(gethrvtime)
