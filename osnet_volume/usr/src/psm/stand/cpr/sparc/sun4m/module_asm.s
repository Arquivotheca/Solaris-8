/*
 * Copyright (c) 1990 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Generic interfaces for dynamically selectable modules.
 */

#pragma ident	"@(#)module_asm.s	1.6	96/04/16 SMI"

#include <sys/asm_linkage.h>
#include <sys/machparam.h>
#include <sys/mmu.h>

#if defined(lint)

void
srmmu_noop(void)
{}

/* ARGSUSED */ 
void
turn_cache_on(int cpuid)
{}

 
#else	/* lint */

#define	REVEC(name, r)				\
	sethi	%hi(v_/**/name), r		; \
	ld	[r+%lo(v_/**/name)], r		; \
	jmp	r				; \
	nop

	ENTRY(srmmu_noop)
	retl
	nop
	SET_SIZE(srmmu_noop)

	ENTRY_NP(turn_cache_on)		! enable the cache
	REVEC(turn_cache_on, %g1)
	SET_SIZE(turn_cache_on)

#endif  /* lint */


/*
 * Get processor state register (XXX ported from sparc/ml/sparc_subr.s. Put
 * it here since we have no better place to keep it yet)
 */
 
#if defined(lint)
 
u_int
getpsr(void)
{ return (0); }
 
#else	/* lint */
 
	ENTRY(getpsr)
	retl
	mov	%psr, %o0
	SET_SIZE(getpsr)
 
#endif	/* lint */
