/*
 *	Copyright (c) 1993 by Sun Microsystems, Inc.
 *
 * assembly code support for modules based on the
 * Sun/Fujitsu SWIFT chip set.
 */

#pragma ident   "@(#)swift_asm.s 1.17     97/05/24 SMI"

#if defined(lint)
#include <sys/types.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/machparam.h>
#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/psr.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/devaddr.h>
#include <sys/async.h>
#include <sys/machthread.h>

/* XXX - these belong in module_swift.h */

/*
 * SR71 CPU Configuration Register
 */
#define	RMMU_CCR_REG	0x600		/* cpu configuration register */
#define	SR71_CCR_SE	0x00000008	/* secondary cache enable */
#define	SR71_CCR_MS2	0x00000010	/* ms2 compatibility mode */
#define	SR71_CCR_WT	0x00000020	/* writethru enable */
#define	SR71_CCR_SNP	0x40000000	/* io snoop enable */

/*
 * SR71/MS2 Module Control Register
 */
#define	MS2_MCR_IE	0x00000200	/* instruction cache enable */
#define	MS2_MCR_DE	0x00000100	/* data cache enable */
#define	MS2_MCR_PE	0x00040000	/* parity enable */

#define	SWIFT_KDNX	/* workaround for 1156639 */

#if !defined(lint)

/*
 * Swift Virtual Address Cache routines.
 */

#define	Sctx	%g1
! preserve for RESTORE_CONTEXT
#define	Spsr	%g2
! preserve for RESTORE_CONTEXT
#define	Fctx	%o2
! flush context

#define	Faddr	%o0
! XXX - check register assignments ok
#define	Addr	%o1
#define	Tmp1	%o2
#define	Tmp2	%o3
#define	Tmp3 	%o4
#define	Tmp4 	%o5
#define	Tmp5 	%g3
#define	Tmp6	%g4
#define	Tmp7	%g5


/*
 * Local #defines
 */
#define	GET(val, r) \
	sethi	%hi(val), r; \
	ld	[r+%lo(val)], r

/*
 * FLUSH_CONTEXT: temporarily set the context number
 * to that given in Fctx. See above for register assignments.
 * Runs with traps disabled - XXX - interrupt latency issue?
 */

#define	FLUSH_CONTEXT			\
	mov	%psr, Spsr;		\
	andn	Spsr, PSR_ET, Tmp7;	\
	mov	Tmp7, %psr;		\
	nop;	nop;			\
\
	set	RMMU_CTP_REG, Tmp7;	\
	lda	[Tmp7]ASI_MOD, Tmp7;	\
	sll	Fctx, 2, Sctx;		\
	sll	Tmp7, 4, Tmp7;		\
	add	Tmp7, Sctx, Tmp7;	\
	lda	[Tmp7]ASI_MEM, Tmp7;	\
	and	Tmp7, 3, Tmp7;		\
	subcc	Tmp7, MMU_ET_PTP, %g0;	\
\
	set	RMMU_CTX_REG, Tmp7;	\
	bne	1f;			\
	lda	[Tmp7]ASI_MOD, Sctx;	\
	sta	Fctx, [Tmp7]ASI_MOD;	\
1:


/*
 * RESTORE_CONTEXT: back out from whatever FLUSH_CONTEXT did.
 * NOTE: asssumes two cycles of PSR DELAY follow the macro;
 * currently, all uses are followed by "retl ; nop".
 */
#define	RESTORE_CONTEXT			\
	set	RMMU_CTX_REG, Tmp7;	\
	sta	Sctx, [Tmp7]ASI_MOD;	\
	mov	Spsr, %psr;		\
	nop;				\
	nop

#endif /* !defined(lint) */

/*
 * swift_cache_init_asm
 *
 * Initialize the cache and leave it off.
 *
 * We may be entered with the cache on.
 * If so turn it off.
 */
#ifdef	lint

/* ARGSUSED */
void
swift_cache_init_asm(void)
{}

#else	/* def lint */

/* I$ is 16KB with 32B linesize */
#define	ICACHE_LINES	512
#define	ICACHE_LINESHFT	5
#define	ICACHE_LINESZ	(1<<ICACHE_LINESHFT)
#define	ICACHE_LINEMASK	(ICACHE_LINESZ-1)
#define	ICACHE_BYTES	(ICACHE_LINES<<ICACHE_LINESHFT)

/* D$ is 8KB with 16B linesize */
#define	DCACHE_LINES	512
#define	DCACHE_LINESHFT	4
#define	DCACHE_LINESZ	(1<<DCACHE_LINESHFT)
#define	DCACHE_LINEMASK	(DCACHE_LINESZ-1)
#define	DCACHE_BYTES	(DCACHE_LINES<<DCACHE_LINESHFT)

/* Derived from D$/I$ values */
#define	CACHE_LINES	1024
#define	CACHE_LINESHIFT	4
#define	CACHE_LINESZ	(1<<CACHE_LINESHIFT)
#define	CACHE_LINEMASK	(CACHE_LINESZ-1)
#define	CACHE_BYTES	(CACHE_LINES<<CACHE_LINESHIFT)

	ENTRY(swift_cache_init_asm)
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1
	andcc	%o1, MS2_MCR_IE|MS2_MCR_DE, %g0
	bz	1f
	nop

	!
	! cache is on, so flush it
	!
	GET(vac_nlines, %o2)
	GET(vac_linesize, %o3)
	clr	%o4
2:	sta	%g0, [%o4]ASI_FCC
	subcc	%o2, 1, %o2
	bnz	2b
	add	%o4, %o3, %o4

	!
	! turn cache off
	!
	! (%o0 == RMMU_CTL_REG, %o1 == current val)
1:	andn	%o1, MS2_MCR_IE|MS2_MCR_DE, %o1
	sta	%o1, [%o0]ASI_MOD

	!
	! clear the cache tags
	!

	GET(vac_nlines, %o2)
	GET(vac_linesize, %o3)
	clr	%o4
2:	sta	%g0, [%o4]ASI_ICT
	sta	%g0, [%o4]ASI_DCT
	subcc	%o2, 1, %o2
	bnz	2b
	add	%o4, %o3, %o4

	retl
	nop
	.align  4
	SET_SIZE(swift_cache_init_asm)

#endif	/* lint */

/*
 * sr71_cache_init
 *
 * Initialize the cache and leave it off.
 *
 * We may be entered with the cache turned on and in writeback mode.
 * If so, flush it and turn it off. Do the same for the ICACHE.
 */

#if defined(lint)
void
sr71_cache_init_asm(void)
{}

#else	/* def lint */

	ENTRY(sr71_cache_init_asm)
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1
	andcc	%o1, MS2_MCR_IE|MS2_MCR_DE, %g0
	bz	1f
	nop

	!
	! cache is on, so flush it
	!
	GET(vac_nlines, %o2)
	GET(vac_linesize, %o3)
	clr	%o4
2:	sta	%g0, [%o4]ASI_FCC
	subcc	%o2, 1, %o2
	bnz	2b
	add	%o4, %o3, %o4

	!
	! turn cache off
	!
	! (%o0 == RMMU_CTL_REG, %o1 == current val)
1:	andn	%o1, MS2_MCR_IE|MS2_MCR_DE, %o1
	sta	%o1, [%o0]ASI_MOD

	!
	! clear the cache tags
	!

	GET(vac_nlines, %o2)
	GET(vac_linesize, %o3)
	clr	%o4
2:	sta	%g0, [%o4]ASI_ICT
	sta	%g0, [%o4]ASI_DCT
	subcc	%o2, 1, %o2
	bnz	2b
	add	%o4, %o3, %o4

	retl
	nop
	.align  4
	SET_SIZE(sr71_cache_init_asm)

#endif	/* def lint */

/*
 * sr71_getccr
 *
 * Get SR71 CPU Configuration Register
 */
#ifdef 	lint
u_int
sr71_getccr(void)
{ return (0); }

#else	/* def lint */
	ENTRY(sr71_getccr)
	set	RMMU_CCR_REG, %o0
	retl
	lda	[%o0]ASI_MOD, %o0
	SET_SIZE(sr71_getccr)
#endif 	/* def lint */

/*
 * sr71_setccr
 *
 * Set SR71 CPU Configuration Register
 */
#ifdef 	lint
/*ARGSUSED*/
void
sr71_setccr(u_int ccr)
{}

#else	/* def lint */
	ENTRY(sr71_setccr)
	set	RMMU_CCR_REG, %o1
	retl
	sta	%o0, [%o1]ASI_MOD
	SET_SIZE(sr71_setccr)
#endif 	/* def lint */

/*
 * swift_vac_usrflush: flush all user data from the cache
 */

#if defined(lint)

void
swift_vac_usrflush(void)
{}

#else	/* lint */

	ENTRY_NP(swift_vac_usrflush)
	set	CACHE_BYTES/8, Addr
	clr	Faddr
	add	Faddr, Addr, Tmp1
	add	Tmp1, Addr, Tmp2
	add	Tmp2, Addr, Tmp3

	add	Tmp3, Addr, Tmp4
	add	Tmp4, DCACHE_LINESZ, Tmp4	! switch to odd D$ lines
	add	Tmp4, Addr, Tmp5
	add	Tmp5, Addr, Tmp6
	add	Tmp6, Addr, Tmp7

1:	deccc   ICACHE_LINESZ, Addr
	sta	%g0, [Faddr]ASI_FCU
	sta	%g0, [Faddr + Tmp1]ASI_FCU
	sta	%g0, [Faddr + Tmp2]ASI_FCU
	sta	%g0, [Faddr + Tmp3]ASI_FCU
	sta	%g0, [Faddr + Tmp4]ASI_FCU
	sta	%g0, [Faddr + Tmp5]ASI_FCU
	sta	%g0, [Faddr + Tmp6]ASI_FCU
	sta	%g0, [Faddr + Tmp7]ASI_FCU
	bne	1b
	inc	ICACHE_LINESZ, Faddr

	retl
	nop

	SET_SIZE(swift_vac_usrflush)

#endif	/* lint */



/*
 * swift_vac_flush: flush data for range 'va' to 'va + sz - 1'
 */

#if defined(lint)

/* ARGSUSED */
void
swift_vac_flush(caddr_t va, int sz)
{}

#else	/* lint */

	ENTRY_NP(swift_vac_flush)

	! XXX - optimize this - ddi_dma_sync() uses this?

	and	Faddr, CACHE_LINEMASK, Tmp7	! figure align error on start
	sub	Faddr, Tmp7, Faddr		! push start back that much
	add	Addr, Tmp7, Addr		! add it to size

1:	deccc	CACHE_LINESZ, Addr
	sta	%g0, [Faddr]ASI_FCP
	bcc	1b
	inc	CACHE_LINESZ, Faddr

	retl
	nop

	SET_SIZE(swift_vac_flush)

#endif	/* lint */

#if defined(lint)

void
swift_turn_cache_on(void)
{}

#else	/* lint */

	.seg    ".data"
	.align  4

.global use_dc
.global use_ic

	.seg    ".text"
	.align  4

	ENTRY_NP(swift_turn_cache_on)

	sethi	%hi(use_ic), %o3
	ld 	[%o3 + %lo(use_ic)], %o3	! load use_ic
	tst	%o3
	be	1f
	clr	%o2

	or	%o2, 0x200, %o2			! enable I$

1:	sethi	%hi(use_dc), %o3
	ld 	[%o3 + %lo(use_dc)], %o3	! load use_dc
	tst	%o3
	be	2f
	nop

	or 	%o2, 0x100, %o2				! enable D$

2: 	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1
	andn	%o1, 0x300, %o1
	or	%o1, %o2, %o1
	retl
	sta	%o1, [%o0]ASI_MOD

	SET_SIZE(swift_turn_cache_on)

#endif	/* lint */

#if defined(lint)

int
swift_check_cache(void)
{ return (0); }

#else	/* lint */

	ENTRY(swift_check_cache)
	set	RMMU_CTL_REG, %o0
	retl
	lda	[%o0]ASI_MOD, %o0
	SET_SIZE(swift_check_cache)

#endif	/* lint */

#if defined(lint)

/*
 * swift_vac_allflush: flush entire cache. If FL_TLB is set in
 * flags, also flush the TLB.
 */

/*ARGSUSED*/
void
swift_vac_allflush(u_int flags)
{}

#else	/* lint */

	! XXX - only works for write-thru cache,
	!	for wb code see module_ross_asm.s
	ENTRY(swift_vac_allflush)

	! if FL_TLB bit is not set, skip TLB flush
	andcc	%o0, FL_TLB, %o0
	bz,a	1f
	set	CACHE_BYTES, Addr

	or	%g0, FT_ALL<<8, Faddr		! flush entire tlb
	sta	%g0, [Faddr]ASI_FLPR		! do the flush


	set	CACHE_BYTES, Addr		! cache bytes to invalidate
1:
	deccc	CACHE_LINESZ, Addr
	sta	%g0, [Addr]ASI_ICT		! clear I$ tag
	bne	1b
	sta	%g0, [Addr]ASI_DCT		! clear D$ tag

	retl
	nop

	SET_SIZE(swift_vac_allflush)

#endif	/* lint */

#if defined(lint)

/*
 * swift_vac_ctxflush: flush all data for ctx 'Fctx' from the cache. If
 *			flags has FL_TLB bit set, flush TLB too.
 */

/* ARGSUSED */
void
swift_vac_ctxflush(void * a, u_int flags)
{}

#else	/* lint */

	ENTRY_NP(swift_vac_ctxflush)

	mov	%o0, %o2
	FLUSH_CONTEXT

	! if FL_TLB bit is not set, skip TLB flush
	andcc	%o1, FL_TLB, %o1
	bz,a	0f
	set	CACHE_BYTES/8, Addr

	set	FT_CTX<<8, Faddr
	sta	%g0, [Faddr]ASI_FLPR		! context flush TLB

	set	CACHE_BYTES/8, Addr
0:	clr	Faddr
	add	Faddr, Addr, Tmp1
	add	Tmp1, Addr, Tmp2
	add	Tmp2, Addr, Tmp3

	add	Tmp3, Addr, Tmp4
	add	Tmp4, DCACHE_LINESZ, Tmp4 	! switch to odd D$ lines
	add	Tmp4, Addr, Tmp5
	add	Tmp5, Addr, Tmp6
	add	Tmp6, Addr, Tmp7

1:	deccc	ICACHE_LINESZ, Addr
	sta	%g0, [Faddr]ASI_FCC
	sta	%g0, [Faddr + Tmp1]ASI_FCC
	sta	%g0, [Faddr + Tmp2]ASI_FCC
	sta	%g0, [Faddr + Tmp3]ASI_FCC
	sta	%g0, [Faddr + Tmp4]ASI_FCC
	sta	%g0, [Faddr + Tmp5]ASI_FCC
	sta	%g0, [Faddr + Tmp6]ASI_FCC
	sta	%g0, [Faddr + Tmp7]ASI_FCC
	bne	1b
	inc	ICACHE_LINESZ, Faddr

	mov	KCONTEXT, Sctx			! restore to kernel context
	RESTORE_CONTEXT

	retl
	nop

	SET_SIZE(swift_vac_ctxflush)

#endif	/* lint */


#if defined(lint)

/*
 * swift_vac_rgnflush: flush all data for rgn 'va' and context 'Fctx'
 * from the cache. If flags has FL_TLB bit set, flush TLB too.
 */

/*ARGSUSED*/
void
swift_vac_rgnflush(caddr_t va, void * b, int Fctx, u_int flags)
{}

#else	/* lint */

	ENTRY_NP(swift_vac_rgnflush)

	mov	%o2, %o4		! save flags for later use
	mov	%o1, %o2
	FLUSH_CONTEXT

	! if FL_TLB bit is not set, skip TLB flush
	andcc	%o4, FL_TLB, %o4
	bz,a	0f
	set	CACHE_BYTES/8, Addr

	or	Faddr, FT_RGN<<8, Tmp1
	sta	%g0, [Tmp1]ASI_FLPR		! region flush TLB

	set	CACHE_BYTES/8, Addr
0:	mov	Addr, Tmp1
	add	Tmp1, Addr, Tmp2
	add	Tmp2, Addr, Tmp3

	add	Tmp3, Addr, Tmp4
	add	Tmp4, DCACHE_LINESZ, Tmp4	! switch to odd D$ lines
	add	Tmp4, Addr, Tmp5
	add	Tmp5, Addr, Tmp6
	add	Tmp6, Addr, Tmp7

1:	deccc	ICACHE_LINESZ, Addr
	sta	%g0, [Faddr]ASI_FCR
	sta	%g0, [Faddr + Tmp1]ASI_FCR
	sta	%g0, [Faddr + Tmp2]ASI_FCR
	sta	%g0, [Faddr + Tmp3]ASI_FCR
	sta	%g0, [Faddr + Tmp4]ASI_FCR
	sta	%g0, [Faddr + Tmp5]ASI_FCR
	sta	%g0, [Faddr + Tmp6]ASI_FCR
	sta	%g0, [Faddr + Tmp7]ASI_FCR
	bne	1b
	inc	ICACHE_LINESZ, Faddr

	RESTORE_CONTEXT
	retl
	nop

	SET_SIZE(swift_vac_rgnflush)

#endif	/* lint */


#if defined(lint)

/*
 * swift_vac_segflush: flush all data for seg 'va' and context 'Fctx'
 * from the cache. If flags has FL_TLB bit set, flush TLB too.
 */

/*ARGSUSED*/
void
swift_vac_segflush(caddr_t va, void * b, int flags)
{}

#else	/* lint */

	ENTRY_NP(swift_vac_segflush)

	mov	%o2, %o4		! save flags for later use
	mov	%o1, %o2
	FLUSH_CONTEXT

	! if FL_TLB bit is not set, skip TLB flush
	andcc	%o4, FL_TLB, %o4
	bz,a	0f
	set	CACHE_BYTES/8, Addr

	or	Faddr, FT_SEG<<8, Tmp1
	sta	%g0, [Tmp1]ASI_FLPR		! segment flush TLB

	set	CACHE_BYTES/8, Addr
0:	mov	Addr, Tmp1
	add	Tmp1, Addr, Tmp2
	add	Tmp2, Addr, Tmp3

	add	Tmp3, Addr, Tmp4
	add	Tmp4, DCACHE_LINESZ, Tmp4	! switch to odd D$ lines
	add	Tmp4, Addr, Tmp5
	add	Tmp5, Addr, Tmp6
	add	Tmp6, Addr, Tmp7

1:	deccc	ICACHE_LINESZ, Addr
	sta	%g0, [Faddr]ASI_FCS
	sta	%g0, [Faddr + Tmp1]ASI_FCS
	sta	%g0, [Faddr + Tmp2]ASI_FCS
	sta	%g0, [Faddr + Tmp3]ASI_FCS
	sta	%g0, [Faddr + Tmp4]ASI_FCS
	sta	%g0, [Faddr + Tmp5]ASI_FCS
	sta	%g0, [Faddr + Tmp6]ASI_FCS
	sta	%g0, [Faddr + Tmp7]ASI_FCS
	bne	1b
	inc	ICACHE_LINESZ, Faddr

	RESTORE_CONTEXT
	retl
	nop

	SET_SIZE(swift_vac_segflush)

#endif	/* lint */

/*
 * swift_vac_pageflush: flush data for page 'va' and ctx 'Fctx'
 * from the cache & TLB
 */

#if defined(lint)

/* ARGSUSED */
void
swift_vac_pageflush(caddr_t va, void * b, u_int flags)
{}

#else	/* lint */

	ENTRY(swift_vac_pageflush)

	mov	%o2, %o4		! save flags for later use
	mov	%o1, %o2
	FLUSH_CONTEXT

	! if FL_TLB bit is not set, skip TLB flush
	andcc	%o4, FL_TLB, %o4
	bz,a	0f
	set	PAGESIZE/8, Addr

	or	Faddr, FT_PAGE<<8, Tmp1
	sta	%g0, [Tmp1]ASI_FLPR		! page flush TLB

	set	PAGESIZE/8, Addr
0:	mov	Addr, Tmp1
	add	Tmp1, Addr, Tmp2
	add	Tmp2, Addr, Tmp3
	add	Tmp3, Addr, Tmp4
	add	Tmp4, Addr, Tmp5
	add	Tmp5, Addr, Tmp6
	add	Tmp6, Addr, Tmp7

1:	deccc	CACHE_LINESZ, Addr
	sta	%g0, [Faddr]ASI_FCP
	sta	%g0, [Faddr + Tmp1]ASI_FCP
	sta	%g0, [Faddr + Tmp2]ASI_FCP
	sta	%g0, [Faddr + Tmp3]ASI_FCP
	sta	%g0, [Faddr + Tmp4]ASI_FCP
	sta	%g0, [Faddr + Tmp5]ASI_FCP
	sta	%g0, [Faddr + Tmp6]ASI_FCP
	sta	%g0, [Faddr + Tmp7]ASI_FCP
	bne	1b
	inc	CACHE_LINESZ, Faddr

	RESTORE_CONTEXT

	retl
	nop

	SET_SIZE(swift_vac_pageflush)

#endif 	/* lint */

#if defined(lint)

/* ARGSUSED */
int
swift_mmu_probe(void)
{ return (0); }

#else	/* lint */

/*
 * Probe routine that takes advantage of multilevel probes.
 * It always returns what the level 3 pte looks like, even if
 * the address is mapped by a level 2 or 1 pte.
 */
	ENTRY(swift_mmu_probe)
	and	%o0, MMU_PAGEMASK, %o2		! and off page offset bits
	or	%o2, FT_ALL << 8, %o3
	lda	[%o3]ASI_FLPR, %o0		! probe all
	cmp	%o0, 0				! if invalid, we're done
	be	probe_done
	or	%o2, FT_RGN << 8, %o3		! probe rgn
	lda	[%o3]ASI_FLPR, %o0
	and	%o0, 3, %o3
	cmp	%o3, MMU_ET_PTE
	bne	1f				! branch if not a pte
	srl	%o2, MMU_STD_PAGESHIFT, %o3	! figure offset into rgn
	and	%o3, MMU_STD_FIRSTMASK, %o3
	sll	%o3, 8, %o3			! shift into pfn position
	b	probe_done
	add	%o0, %o3, %o0			! add it to pfn
1:
	or	%o2, FT_SEG << 8, %o3
	lda	[%o3]ASI_FLPR, %o0		! probe seg
	and	%o0, 3, %o3
	cmp	%o3, MMU_ET_PTE
	bne	1f
	srl	%o2, MMU_STD_PAGESHIFT, %o3	! figure offset into seg
	and	%o3, MMU_STD_SECONDMASK, %o3
	sll	%o3, 8, %o3			! shift into pfn position
	b	probe_done
	add	%o0, %o3, %o0			! add it to pfn
1:
	or	%o2, FT_PAGE << 8, %o3
	lda	[%o3]ASI_FLPR, %o0		! probe page
probe_done:
	set	RMMU_FSR_REG, %o2		! setup to clear fsr
	cmp	%o1, 0
	be	1f				! if fsr not wanted, don't st
	lda	[%o2]ASI_MOD, %o2		! clear fsr
	st	%o2, [%o1]			! return fsr
1:
	retl
	nop
	SET_SIZE(swift_mmu_probe)

#endif 	/* lint */

#ifdef	SWIFT_KDNX
#if defined(lint)

/* ARGSUSED */
int
swift_mmu_probe_kdnx(void)
{ return (0); }

#else	/* lint */

/*
 *  This is a special version of the probe routine above.
 *  It and is used instead of the one above when the Swift
 *  prefetch workaround (swift_kdnx = 1) is enabled.
 *  This probe routine checks for mappings to non-memory and
 *  flushes them after the probe is completed.
 */
	ENTRY(swift_mmu_probe_kdnx)
	and	%o0, MMU_PAGEMASK, %o2		! and off page offset bits
	or	%o2, FT_ALL << 8, %o3
	lda	[%o3]ASI_FLPR, %o0		! probe all
	cmp	%o0, 0				! if invalid, we're done
	be	probe_done_kdnx
	or	%o2, FT_RGN << 8, %o3		! probe rgn
	lda	[%o3]ASI_FLPR, %o0
	and	%o0, 3, %o3
	cmp	%o3, MMU_ET_PTE
	bne	1f				! branch if not a pte
	srl	%o2, MMU_STD_PAGESHIFT, %o3	! figure offset into rgn
	and	%o3, MMU_STD_FIRSTMASK, %o3
	sll	%o3, 8, %o3			! shift into pfn position
	b	probe_done_kdnx
	add	%o0, %o3, %o0			! add it to pfn
1:
	or	%o2, FT_SEG << 8, %o3
	lda	[%o3]ASI_FLPR, %o0		! probe seg
	and	%o0, 3, %o3
	cmp	%o3, MMU_ET_PTE
	bne	1f
	srl	%o2, MMU_STD_PAGESHIFT, %o3	! figure offset into seg
	and	%o3, MMU_STD_SECONDMASK, %o3
	sll	%o3, 8, %o3			! shift into pfn position
	b	probe_done_kdnx
	add	%o0, %o3, %o0			! add it to pfn
1:
	or	%o2, FT_PAGE << 8, %o3
	lda	[%o3]ASI_FLPR, %o0		! probe page
probe_done_kdnx:
	srl	%o0, 24, %o3			! get rid of all but bits
	andcc	%o3, 7, %o3			! 30, 29, 28 of physical addr
	be	1f				! if zero, its a memory mapping
	nop					! else its an IO mapping
	sta	%g0, [%o2]ASI_FLPR		! and we must flush the TLB

1:
	set	RMMU_FSR_REG, %o2		! setup to clear fsr
	cmp	%o1, 0
	be	1f				! if fsr not wanted, don't st
	lda	[%o2]ASI_MOD, %o2		! clear fsr
	st	%o2, [%o1]			! return fsr
1:
	retl
	nop
	SET_SIZE(swift_mmu_probe_kdnx)

#endif 	/* lint */
#endif	SWIFT_KDNX

#if defined(lint)

/* ARGSUSED */
int
swift_getversion(void)
{ return (0); }

#else	/* lint */

/*
 * swift_getversion will get the version of the Swift chip.  The
 * version is located in the VA mask register (PA = 0x1000.3018).
 */
	ENTRY(swift_getversion)
	set	0x10003018, %o2		! Address of version.
	retl
	lda	[%o2]0x20, %o0		! Get version.
	SET_SIZE(swift_getversion)

#endif	/* lint */
