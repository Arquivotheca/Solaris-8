/*
 * Copyright (c) 1993-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)gethrtime.s	1.2	97/03/20 SMI"

	.file	"gethrtime.s"

#include <sys/asm_linkage.h>

/*
 * hrtime_t gethrtime(void)
 *
 * Returns the current hi-res real time.
 *
 * (Note that the fast trap actually assumes V8 parameter passing
 * conventions, so we have to reassemble a 64-bit value here)
 *
 * XXX Perhaps we should add another trap type for V9?
 */

	ENTRY(gethrtime)
	ta	ST_GETHRTIME
	sllx	%o0, 32, %o0
	retl
	or	%o1, %o0, %o0
	SET_SIZE(gethrtime)

/*
 * hrtime_t gethrvtime(void)
 *
 * Returns the current hi-res LWP virtual time.
 *
 * (Note that the fast trap actually assumes V8 parameter passing
 * conventions, so we have to reassemble a 64-bit value here)
 *
 * XXX Perhaps we should add another trap type for V9?
 */

	ENTRY(gethrvtime)
	ta	ST_GETHRVTIME
	sllx	%o0, 32, %o0
	retl
	or	%o1, %o0, %o0
	SET_SIZE(gethrvtime)
