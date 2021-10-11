/*
 *	Copyright (c) 1994 by Sun Microsystems, Inc.
 *
 * assembly code support for modules based on the
 * Cypress CY7C604 or CY7C605 Cache Controller and
 * Memory Management Units.
 */

#pragma ident "@(#)module_ross625_asm.s	1.3	96/02/27 SMI" /* From SunOS 4.1.1 */

#include <sys/param.h>
#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/trap.h>
#include <sys/devaddr.h>
#include <sys/physaddr.h>
#include "assym.s"

#define	CACHE_LINES	4096
#define SMALL_LINESIZE	32
#define BIG_LINESIZE	64

#define RT625_CTL_CE	0x00000100	/* cache enable */
#define RT625_CTL_CS	0x00001000	/* cache size	*/

#define ASI_FCC		0x13
#define RT625_ASI_CTAGS	0x0e
#define RT620_ASI_IC	0x31

#define GET(val, r) \
	sethi	%hi(val), r; \
	ld	[r+%lo(val)], r

#define PUT(val, r, t) \
	sethi	%hi(val), t; \
	st	r, [t+%lo(val)]

#ifdef	VAC

#if !defined(lint)
	.seg	".data"
ross625_setbits:
	.word	0
ross625_clrbits:
	.word	0
ross625_line_size:
	.word	64

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

	ENTRY(ross625_cache_init)
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1	! get the MMU ctl reg

	set	RT625_CTL_CS, %o2	
	andcc	%o2, %o1, %o2	
	bz	1f
	nop
	mov	BIG_LINESIZE, %o3	! CS on = 64 byte line size
	b	2f
	nop

1:	mov	SMALL_LINESIZE, %o3	! CS off = 32 byte line size
2:	PUT(ross625_line_size, %o3, %o2)

	sethi	%hi(ross625_setbits), %o2
	ld	[%o2+%lo(ross625_setbits)], %o2
	or	%o1, %o2, %o1		! turn some on
	sethi	%hi(ross625_clrbits), %o2
	ld	[%o2+%lo(ross625_clrbits)], %o2
	andn	%o1, %o2, %o1		! turn some off
	sta	%o1, [%o0]ASI_MOD

	set	CACHE_LINES, %o2 	! assume cache is on,
	GET(ross625_line_size,%o3)
	mov	0, %o4
1:	sta	%g0, [%o4]ASI_FCC
	subcc	%o2, 1, %o2
	bnz	1b
	add	%o4, %o3, %o4

	set	RMMU_CTL_REG, %o0		! turn cache off
	lda	[%o0]ASI_MOD, %o1
	andn	%o1, RT625_CTL_CE, %o1
	sta	%o1, [%o0]ASI_MOD
	
	set	CACHE_LINES, %o2 	! assume cache is on,
	GET(ross625_line_size,%o3)
	mov	0, %o4
1:	sta	%g0, [%o4]RT625_ASI_CTAGS
	subcc	%o2, 1, %o2
	bnz	1b
	add	%o4, %o3, %o4

	retl
	sta	%g0, [%g0]RT620_ASI_IC		! clear the icache

	!
	! void ross625_vac_pageflush(va)
	!	flush data in this page from the cache.
	!

	ENTRY(ross625_vac_pageflush)
	set	PAGESIZE, %o1			! the page size
	GET(ross625_line_size,%o2)		! the line size
1:	subcc	%o1, %o2, %o1			! all the lines in a page
	sta	%g0, [%o0]ASI_FCP		! flush cache page
	bne	1b
	add	%o0, %o2, %o0			! on to the next line
	retl
	sta	%g0, [%g0]RT620_ASI_IC		! clear the icache

	!
	! void ross625_vac_flush(va, sz)
	!	flush data in this range from the cache
	!

	ENTRY(ross625_vac_flush)
	GET(ross625_line_size,%o2)		! get line size
	sub	%o2, 1, %g1			! mask
	and	%o0, %g1, %g1			! figure align error on start
	sub	%o0, %g1, %o0			! push start back that much
	add	%o1, %g1, %o1			! add it to size
1:	subcc	%o1, %o2, %o1			! for each line
	sta	%g0, [%o0]ASI_FCP		! flush cache page
	bcc	1b
	add	%o0, %o2, %o0			! on to next line
	retl
	sta	%g0, [%g0]RT620_ASI_IC		! clear the icache

	ENTRY(ross625_cache_on)
	set	RT625_CTL_CE, %o2 		! cache enable
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1
	or	%o1, %o2, %o1
	sta	%o1, [%o0]ASI_MOD
	retl
	nop

#endif	/* !defined(lint) */
#endif	/* VAC */
