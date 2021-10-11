/*
 *	Copyright (c) 1994 by Sun Microsystems, Inc.
 *
 * Assembly code support for modules based on the Sun/Fujitsu
 * Swift CPU
 */

#pragma ident	"@(#)module_swift_asm.s 1.5	96/02/27 SMI"

#if defined(lint)
#include <sys/types.h>
#else	/* lint */
#include "assym.s"
#endif	/* lint */

#include <sys/param.h>
#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/trap.h>
#include <sys/devaddr.h>
#include <sys/physaddr.h>

#ifdef VAC

#if defined(lint)

void
swift_cache_init(void)
{}

#else	/* lint */

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

/* I$ is 16KB with 32 Byte linesize */
#define	ICACHE_LINES    512
#define	ICACHE_LINESHFT 5
#define	ICACHE_LINESZ   (1<<ICACHE_LINESHFT)
#define	ICACHE_LINEMASK (ICACHE_LINESZ-1)
#define	ICACHE_BYTES    (ICACHE_LINES<<ICACHE_LINESHFT)

/* D$ is 8KB with 16 Byte linesize */
#define	DCACHE_LINES    512
#define	DCACHE_LINESHFT 4
#define	DCACHE_LINESZ   (1<<DCACHE_LINESHFT)
#define	DCACHE_LINEMASK (DCACHE_LINESZ-1)
#define	DCACHE_BYTES    (DCACHE_LINES<<DCACHE_LINESHFT)

/* Derived from D$/I$ values */
#define	CACHE_LINES	1024
#define	CACHE_LINESHIFT	4
#define	CACHE_LINESZ	(1<<CACHE_LINESHIFT)
#define	CACHE_LINEMASK	(CACHE_LINESZ-1)
#define	CACHE_BYTES	(CACHE_LINES<<CACHE_LINESHIFT)


	!
	! void swift_cache_init
	!	initialize Swift's I and D $'s.

	ENTRY(swift_cache_init)

	! Initialize Instruction Cache.
	set	CACHE_BYTES, %o1	! Icache bytes to init
1:
	deccc	CACHE_LINESZ, %o1
	sta	%g0, [%o1]ASI_ICT	! clear I$ tag
	bne	1b
	sta	%g0, [%o1]ASI_DCT	! clear D$ tag

	retl
	nop


#endif	/* lint */

#if defined (lint)

/* ARGSUSED */
void
swift_vac_pageflush(caddr_t va)
{}

#else	/* lint */

	! void swift_vac_pageflush(caddr_t va)
	!	flush data in this page from the cache.
	!
	ENTRY(swift_vac_pageflush)

	set	CACHE_BYTES, %o1
1:
	deccc	CACHE_LINESZ, %o1
	sta	%g0, [%o0]ASI_FCP
	bne	1b
	inc	CACHE_LINESZ, %o0

	retl
	nop

#endif	/* lint */

#if defined (lint)

void
swift_turn_cache_on(void)
{}

#else	/* lint */

	!
	! void swift_turn_cache_on
	!
	ENTRY (swift_turn_cache_on)
	set	0x300, %o2			! cache bits
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1		! load control reg
	or	%o1, %o2, %o1			! or in the bits
	sta	%o1, [%o0]ASI_MOD		! cache enable
	retl
	nop

#endif	/* lint */

#if defined (lint)

/* ARGSUSED */
void
swift_vac_flush(caddr_t va, int sz)
{}

#else	/* lint */

	!
	! flush data in this rage from the cache.
	!

	ENTRY(swift_vac_flush)
	and	%o0, CACHE_LINEMASK, %g1	! figure align err on start
	sub	%o0, %g1, %o0			! push start back that much
	add	%o1, %g1, %o1			! add it to size

1:	deccc	CACHE_LINESZ, %o1
	sta	%g0, [%o0]ASI_FCP
	bcc	1b
	inc	CACHE_LINESZ, %o0

	retl
	nop

#endif	/* lint */

#endif	/* VAC */
