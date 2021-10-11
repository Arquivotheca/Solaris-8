/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)module.s	1.3	96/05/17 SMI"

#include <sys/asm_linkage.h>
#if !defined(lint)
#include <sys/mmu.h>	/* Guarded to avoid a sunm mmu_getctx conflict */
#endif

#if defined(lint)

u_int
getmcr(void)
{ return (0); }

#else

	ENTRY(getmcr)
	retl
	lda	[%g0]ASI_MOD, %o0	! module control/status register
	SET_SIZE(getmcr)
#endif	/* lint */
