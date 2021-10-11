/*
 *	Copyright (c) 1994 by Sun Microsystems, Inc.
 *
 * assembly code support for dynamically selectable modules
 */

#pragma ident	"@(#)module_asm.s	1.8	96/03/17 SMI" /* From SunOS 4.1.1 */

#include <sys/param.h>
#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/trap.h>
#include <sys/devaddr.h>
#include <sys/physaddr.h>
#include "assym.s"

#if !defined(lint)

	.seg	".text"
	.align	4

#define	REVEC(name, r)				\
	sethi	%hi(v_/**/name), r		; \
	ld	[r+%lo(v_/**/name)], r		; \
	jmp	r				; \
	nop

	ENTRY(mmu_getctp)
	REVEC(mmu_getctp, %o5)

	ENTRY(mmu_flushall)
	REVEC(mmu_flushall, %o5)

	ENTRY(mmu_flushpage)
	srl	%o0, MMU_STD_PAGESHIFT, %o0 ! round address to
	sll	%o0, MMU_STD_PAGESHIFT, %o0 ! base of page.
	REVEC(mmu_flushpage, %o5)

	ENTRY(getpsr)
	retl
	mov	%psr, %o0

	ENTRY(getmcr)
	retl
	lda	[%g0]ASI_MOD, %o0	! module control/status register
	SET_SIZE(getmcr)

#ifdef	VAC

	ENTRY(vac_noop)
	retl
	nop

	ENTRY(cache_init)
	REVEC(cache_init, %g1)


	!
	! void vac_pageflush(va)
	!	flush page [in current context or supv] from cache
	!

	ENTRY(vac_pageflush)
	sethi	%hi(vac), %g1
	ld	[%g1 + %lo(vac)], %g1
	tst	%g1			! check if cache is turned on
	bz	vac_noop
	nop

	srl	%o0, MMU_STD_PAGESHIFT, %o0 ! round address to
	sll	%o0, MMU_STD_PAGESHIFT, %o0 ! base of page

	REVEC(vac_pageflush, %g1)


	!
	! void vac_flush(va, sz)
	!	flush range [in current context or supv] from cache
	!

	ENTRY(vac_flush)
	sethi	%hi(vac), %g1
	ld	[%g1 + %lo(vac)], %g1
	tst	%g1			! check if cache is turned on
	bz	vac_noop
	nop

	REVEC(vac_flush, %g1)

	!
	! void cache_on()
	!	enable the cache
	!

	ENTRY(cache_on)
	REVEC(cache_on, %g1)

#endif	/* VAC */
#endif	/* !defined(lint) */
