/*
 *	Copyright (c) 1994 by Sun Microsystems, Inc.
 *
 * assembly code support for modules based on the
 * Cypress CY7C604 or CY7C605 Cache Controller and
 * Memory Management Units.
 */

#pragma ident	"@(#)module_ross_asm.s	1.6	96/02/27 SMI" /* From SunOS 4.1.1 */

#include <sys/param.h>
#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/trap.h>
#include <sys/devaddr.h>
#include <sys/physaddr.h>
#include "assym.s"

#ifdef	VAC
#if !defined(lint)
	.seg	".data"
ross_setbits:
	.word	0
ross_clrbits:
	.word	0

/*
 * Virtual Address Cache routines.
 *
 *	Standard register allocation:
 *
 * %g1	scratchpad / jump vector / alt ctx reg ptr
 * %o0	virtual address			ctxflush puts context here
 * %o1	incoming size / loop counter
 * %o2	context number to borrow
 * %o3	saved context number
 * %o4	srmmu ctx reg ptr
 * %o5	saved psr
 * %o6	reserved (stack pointer)
 * %o7	reserved (return address)
 */

#define	CACHE_LINES	2048

#define	CACHE_LINESHFT	5
#define	CACHE_LINESZ	(1<<CACHE_LINESHFT)
#define	CACHE_LINEMASK	(CACHE_LINESZ-1)

#define	CACHE_BYTES	(CACHE_LINES<<CACHE_LINESHFT)

#define	MPTAG_OFFSET	0x40000

	ENTRY(ross_cache_init)
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1
	sethi	%hi(ross_setbits), %o2
	ld	[%o2+%lo(ross_setbits)], %o2
	or	%o1, %o2, %o1		! turn some on
	sethi	%hi(ross_clrbits), %o2
	ld	[%o2+%lo(ross_clrbits)], %o2
	andn	%o1, %o2, %o1		! turn some off
	sta	%o1, [%o0]ASI_MOD

	!
	! Yes, we clear MPTAGs on the 604, even though
	! they don't exist. It just means that it takes
	! twice as long to initialize the 604 cache than
	! it would have to.
	!

	set	CACHE_LINES*CACHE_LINESZ, %o1
	set	MPTAG_OFFSET, %o2
	add	%o2, %o1, %o2
	deccc	CACHE_LINESZ, %o1
1:	dec	CACHE_LINESZ, %o2
	sta	%g0, [%o1]0xE		! clear PVTAG
	sta	%g0, [%o2]0xE		! clear MPTAG
	bne	1b
	deccc	CACHE_LINESZ, %o1
	retl
	nop

	!
	! void ross_vac_pageflush(va)
	!	flush data in this page from the cache.
	!

	ENTRY(ross_vac_pageflush)
	set	PAGESIZE, %o1
1:	deccc	CACHE_LINESZ, %o1
	sta	%g0, [%o0]ASI_FCP
	bne	1b
	inc	CACHE_LINESZ, %o0
	retl
	nop

	!
	! void ross_vac_flush(va, sz)
	!	flush data in this range from the cache
	!

	ENTRY(ross_vac_flush)
	and	%o0, CACHE_LINEMASK, %g1	! figure align error on start
	sub	%o0, %g1, %o0			! push start back that much
	add	%o1, %g1, %o1			! add it to size
1:	deccc	CACHE_LINESZ, %o1
	sta	%g0, [%o0]ASI_FCP
	bcc	1b
	inc	CACHE_LINESZ, %o0
	retl
	nop

	ENTRY(ross_cache_on)
	set	0x100, %o2			! cache enable
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1
	or	%o1, %o2, %o1
	sta	%o1, [%o0]ASI_MOD
	retl
	nop

	ENTRY(check_cache)
	set	RMMU_CTL_REG, %o0
	retl
	lda	[%o0]ASI_MOD, %o0

#endif	/* !defined(lint) */
#endif	/* VAC */
