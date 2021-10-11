/*
 * Copyright (c) 1990-1991,1993,1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * assembly code support for spitfire modules
 */

#pragma ident	"@(#)spitfire_asm.s	1.51	99/11/10 SMI"

#if !defined(lint)
#include "assym.h"
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <vm/hat_sfmmu.h>
#include <sys/machparam.h>
#include <sys/machcpuvar.h>
#include <sys/privregs.h>
#include <sys/asm_linkage.h>
#include <sys/spitasi.h>
#include <sys/trap.h>
#include <sys/spitregs.h>
#include <sys/xc_impl.h>
#include <sys/intreg.h>
#include <sys/async.h>

#ifdef TRAPTRACE
#include <sys/traptrace.h>
#endif /* TRAPTRACE */

#ifndef	lint

/* BEGIN CSTYLED */
#define	DCACHE_FLUSHPAGE(arg1, arg2, tmp1, tmp2, tmp3)			\
	ldxa	[%g0]ASI_LSU, tmp1					;\
	btst	LSU_DC, tmp1		/* is dcache enabled? */	;\
	bz,pn	%icc, 1f						;\
	sethi	%hi(dcache_linesize), tmp1				;\
	ld	[tmp1 + %lo(dcache_linesize)], tmp1			;\
	sethi	%hi(dflush_type), tmp2					;\
	ld	[tmp2 + %lo(dflush_type)], tmp2				;\
	cmp	tmp2, FLUSHPAGE_TYPE					;\
	be,pt	%icc, 2f						;\
	sllx	arg1, DC_VBIT_SHIFT, arg1	/* tag to compare */	;\
	sethi	%hi(dcache_size), tmp3					;\
	ld	[tmp3 + %lo(dcache_size)], tmp3				;\
	cmp	tmp2, FLUSHMATCH_TYPE					;\
	be,pt	%icc, 3f						;\
	nop								;\
	/*								\
	 * flushtype = FLUSHALL_TYPE, flush the whole thing		\
	 * tmp3 = cache size						\
	 * tmp1 = cache line size					\
	 */								\
	sub	tmp3, tmp1, tmp2					;\
4:									\
	stxa	%g0, [tmp2]ASI_DC_TAG					;\
	membar	#Sync							;\
	cmp	%g0, tmp2						;\
	bne,pt	%icc, 4b						;\
	sub	tmp2, tmp1, tmp2					;\
	ba,pt	%icc, 1f						;\
	nop								;\
	/*								\
	 * flushtype = FLUSHPAGE_TYPE					\
	 * arg1 = tag to compare against				\
	 * arg2 = virtual color						\
	 * tmp1 = cache line size					\
	 * tmp2 = tag from cache					\
	 * tmp3 = counter						\
	 */								\
2:									\
	set	MMU_PAGESIZE, tmp3					;\
	sllx	arg2, MMU_PAGESHIFT, arg2  /* color to dcache page */	;\
	sub	tmp3, tmp1, tmp3					;\
4:									\
	ldxa	[arg2 + tmp3]ASI_DC_TAG, tmp2	/* read tag */		;\
	btst	DC_VBIT_MASK, tmp2					;\
	bz,pn	%icc, 5f	  /* branch if no valid sub-blocks */	;\
	andn	tmp2, DC_VBIT_MASK, tmp2	/* clear out v bits */	;\
	cmp	tmp2, arg1						;\
	bne,pn	%icc, 5f			/* br if tag miss */	;\
	nop								;\
	stxa	%g0, [arg2 + tmp3]ASI_DC_TAG				;\
	membar	#Sync							;\
5:									\
	cmp	%g0, tmp3						;\
	bnz,pt	%icc, 4b		/* branch if not done */	;\
	sub	tmp3, tmp1, tmp3					;\
	ba,pt	%icc, 1f						;\
	nop								;\
	/*								\
	 * flushtype = FLUSHMATCH_TYPE					\
	 * arg1 = tag to compare against				\
	 * tmp1 = cache line size					\
	 * tmp3 = cache size						\
	 * arg2 = counter						\
	 * tmp2 = cache tag						\
	 */								\
3:									\
	sub	tmp3, tmp1, arg2					;\
4:									\
	ldxa	[arg2]ASI_DC_TAG, tmp2		/* read tag */		;\
	btst	DC_VBIT_MASK, tmp2					;\
	bz,pn	%icc, 5f		/* br if no valid sub-blocks */	;\
	andn	tmp2, DC_VBIT_MASK, tmp2	/* clear out v bits */	;\
	cmp	tmp2, arg1						;\
	bne,pn	%icc, 5f		/* branch if tag miss */	;\
	nop								;\
	stxa	%g0, [arg2]ASI_DC_TAG					;\
	membar	#Sync							;\
5:									\
	cmp	%g0, arg2						;\
	bne,pt	%icc, 4b		/* branch if not done */	;\
	sub	arg2, tmp1, arg2					;\
1:

/*
 * macro that flushes the entire dcache color
 */
#define	DCACHE_FLUSHCOLOR(arg, tmp1, tmp2)				\
	ldxa	[%g0]ASI_LSU, tmp1;					\
	btst	LSU_DC, tmp1;		/* is dcache enabled? */	\
	bz,pn	%icc, 1f;						\
	sethi	%hi(dcache_linesize), tmp1;				\
	ld	[tmp1 + %lo(dcache_linesize)], tmp1;			\
	set	MMU_PAGESIZE, tmp2;					\
	/*								\
	 * arg = virtual color						\
	 * tmp2 = page size						\
	 * tmp1 = cache line size					\
	 */								\
	sllx	arg, MMU_PAGESHIFT, arg; /* color to dcache page */	\
	sub	tmp2, tmp1, tmp2;					\
2:									\
	stxa	%g0, [arg + tmp2]ASI_DC_TAG;				\
	membar	#Sync;							\
	cmp	%g0, tmp2;						\
	bne,pt	%icc, 2b;						\
	sub	tmp2, tmp1, tmp2;					\
1:

/*
 * macro that flushes the entire dcache
 */
#define	DCACHE_FLUSHALL(tmp1, tmp2, tmp3)				\
	ldxa	[%g0]ASI_LSU, tmp1;					\
	btst	LSU_DC, tmp1;		/* is dcache enabled? */	\
	bz,pn	%icc, 1f;						\
	sethi	%hi(dcache_linesize), tmp1;				\
	ld	[tmp1 + %lo(dcache_linesize)], tmp1;			\
	sethi	%hi(dcache_size), tmp3;					\
	ld	[tmp3 + %lo(dcache_size)], tmp3;			\
	/*								\
	 * tmp3 = cache size						\
	 * tmp1 = cache line size					\
	 */								\
	sub	tmp3, tmp1, tmp2;					\
2:									\
	stxa	%g0, [tmp2]ASI_DC_TAG;					\
	membar	#Sync;							\
	cmp	%g0, tmp2;						\
	bne,pt	%icc, 2b;						\
	sub	tmp2, tmp1, tmp2;					\
1:

/* END CSTYLED */

#endif	/* !lint */

/*
 * Spitfire MMU and Cache operations.
 */

#if defined(lint)

/* ARGSUSED */
void
vtag_flushpage(caddr_t vaddr, u_int ctxnum)
{}

/* ARGSUSED */
void
vtag_flushctx(u_int ctxnum)
{}

/* ARGSUSED */
void
vtag_flushpage_tl1(uint64_t vaddr, uint64_t ctxnum)
{}

/* ARGSUSED */
void
vtag_flushctx_tl1(uint64_t ctxnum, uint64_t dummy)
{}

/* ARGSUSED */
void
vac_flushpage(pfn_t pfnum, int vcolor)
{}

/* ARGSUSED */
void
vac_flushpage_tl1(uint64_t pfnum, uint64_t vcolor)
{}

/* ARGSUSED */
void
init_mondo(xcfunc_t *func, uint64_t arg1, uint64_t arg2)
{}

/* ARGSUSED */
void
send_mondo(int upaid)
{}

/* ARGSUSED */
void
flush_instr_mem(caddr_t vaddr, size_t len)
{}

/* ARGSUSED */
void
flush_ecache(uint64_t physaddr, size_t size)
{}

#else	/* lint */

	ENTRY_NP(vtag_flushpage)
	/*
	 * flush page from the tlb
	 *
	 * %o0 = vaddr
	 * %o1 = ctxnum
	 */
	rdpr	%pstate, %o5
#ifdef DEBUG
	andcc	%o5, PSTATE_IE, %g0		/* if interrupts already */
	bnz,a,pt %icc, 3f			/* disabled, panic */
	nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	  or	%o0, %lo(sfmmu_panic1), %o0
	ret
	restore
3:
#endif /* DEBUG */
	/*
	 * disable ints
	 */
	andn	%o5, PSTATE_IE, %o4
	wrpr	%o4, 0, %pstate

	/*
	 * Then, blow out the tlb
	 * Interrupts are disabled to prevent the secondary ctx register
	 * from changing underneath us.
	 */
	brnz,pt	%o1, 1f			/* KCONTEXT? */
	sethi	%hi(FLUSH_ADDR), %o3
	/*
	 * For KCONTEXT demaps use primary. type = page implicitly
	 */
	stxa	%g0, [%o0]ASI_DTLB_DEMAP	/* dmmu flush for KCONTEXT */
	stxa	%g0, [%o0]ASI_ITLB_DEMAP	/* immu flush for KCONTEXT */
	b	5f
	  flush	%o3
1:
	/*
	 * User demap.  We need to set the secondary context properly.
	 * %o0 = vaddr
	 * %o1 = ctxnum
	 * %o3 = FLUSH_ADDR
	 */
	set	MMU_SCONTEXT, %o4
	ldxa	[%o4]ASI_DMMU, %o2		/* rd old ctxnum */
	or	DEMAP_SECOND | DEMAP_PAGE_TYPE, %o0, %o0
	cmp	%o2, %o1
	be,a,pt	%icc, 4f
	  nop
	stxa	%o1, [%o4]ASI_DMMU		/* wr new ctxum */
4:
	stxa	%g0, [%o0]ASI_DTLB_DEMAP
	stxa	%g0, [%o0]ASI_ITLB_DEMAP
	flush	%o3
	be,a,pt	%icc, 5f
	  nop
	stxa	%o2, [%o4]ASI_DMMU		/* restore old ctxnum */
	flush	%o3
5:
	retl
	  wrpr	%g0, %o5, %pstate		/* enable interrupts */
	SET_SIZE(vtag_flushpage)

	ENTRY_NP(vtag_flushctx)
	/*
	 * flush context from the tlb
	 *
	 * %o0 = ctxnum
	 * We disable interrupts to prevent the secondary ctx register changing
	 * underneath us.
	 */
	sethi	%hi(FLUSH_ADDR), %o3
	set	DEMAP_CTX_TYPE | DEMAP_SECOND, %g1
	rdpr	%pstate, %o2

#ifdef DEBUG
	andcc	%o2, PSTATE_IE, %g0		/* if interrupts already */
	bnz,a,pt %icc, 1f			/* disabled, panic	 */
	  nop
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	  or	%o0, %lo(sfmmu_panic1), %o0
1:
#endif /* DEBUG */

	wrpr	%o2, PSTATE_IE, %pstate		/* disable interrupts */
	set	MMU_SCONTEXT, %o4
	ldxa	[%o4]ASI_DMMU, %o5		/* rd old ctxnum */
	cmp	%o5, %o0
	be,a,pt	%icc, 4f
	  nop
	stxa	%o0, [%o4]ASI_DMMU		/* wr new ctxum */
4:
	stxa	%g0, [%g1]ASI_DTLB_DEMAP
	stxa	%g0, [%g1]ASI_ITLB_DEMAP
	flush	%o3
	be,a,pt	%icc, 5f
	  nop
	stxa	%o5, [%o4]ASI_DMMU		/* restore old ctxnum */
	flush	%o3
5:
	retl
	  wrpr	%g0, %o2, %pstate		/* enable interrupts */
	SET_SIZE(vtag_flushctx)

	ENTRY_NP(vtag_flushpage_tl1)
	/*
	 * x-trap to flush page from tlb and tsb
	 *
	 * %g1 = vaddr, zero-extended on 32-bit kernel
	 * %g2 = ctxnum
	 *
	 * NOTE: any changes to this code probably need to be reflected in
	 * sf_ttecache_flushpage_tl1.
	 * assumes TSBE_TAG = 0
	 */
	srln	%g1, MMU_PAGESHIFT, %g1
	slln	%g1, MMU_PAGESHIFT, %g1			/* g1 = vaddr */
	/* We need to set the secondary context properly. */
	set	MMU_SCONTEXT, %g4
	ldxa	[%g4]ASI_DMMU, %g5		/* rd old ctxnum */
	or	DEMAP_SECOND | DEMAP_PAGE_TYPE, %g1, %g1
	stxa	%g2, [%g4]ASI_DMMU		/* wr new ctxum */
	stxa	%g0, [%g1]ASI_DTLB_DEMAP
	stxa	%g0, [%g1]ASI_ITLB_DEMAP
	stxa	%g5, [%g4]ASI_DMMU		/* restore old ctxnum */
	membar #Sync
	retry
	SET_SIZE(vtag_flushpage_tl1)

	ENTRY_NP(vtag_flushctx_tl1)
	/*
	 * x-trap to flush context from tlb
	 *
	 * %g1 = ctxnum
	 */
	set	DEMAP_CTX_TYPE | DEMAP_SECOND, %g4
	set	MMU_SCONTEXT, %g3
	ldxa	[%g3]ASI_DMMU, %g5		/* rd old ctxnum */
	stxa	%g1, [%g3]ASI_DMMU		/* wr new ctxum */
	stxa	%g0, [%g4]ASI_DTLB_DEMAP
	stxa	%g0, [%g4]ASI_ITLB_DEMAP
	stxa	%g5, [%g3]ASI_DMMU		/* restore old ctxnum */
	membar #Sync
	retry
	SET_SIZE(vtag_flushctx_tl1)

/*
 * vac_flushpage(pfnum, color)
 *	Flush 1 8k page of the D-$ with physical page = pfnum
 *	Algorithm:
 *		The spitfire dcache is a 16k direct mapped virtual indexed,
 *		physically tagged cache.  Given the pfnum we read all cache
 *		lines for the corresponding page in the cache (determined by
 *		the color).  Each cache line is compared with
 *		the tag created from the pfnum. If the tags match we flush
 *		the line.
 *	NOTE:	Any changes to this code probably need to be reflected in
 *		sfmmu_tlbcache_flushpage_tl1.
 */
	.seg	".data"
	.align	8
	.global	dflush_type
dflush_type:
	.word	FLUSHPAGE_TYPE
	.seg	".text"

	ENTRY(vac_flushpage)
	/*
	 * flush page from the d$
	 *
	 * %o0 = pfnum, %o1 = color
	 */
	DCACHE_FLUSHPAGE(%o0, %o1, %o2, %o3, %o4)
	retl
	nop
	SET_SIZE(vac_flushpage)

	ENTRY_NP(vac_flushpage_tl1)
	/*
	 * x-trap to flush page from the d$
	 *
	 * %g1 = pfnum, %g2 = color
	 */
	DCACHE_FLUSHPAGE(%g1, %g2, %g3, %g4, %g5)
	retry
	SET_SIZE(vac_flushpage_tl1)

	ENTRY(vac_flushcolor)
	/*
	 * %o0 = vcolor
	 */
	DCACHE_FLUSHCOLOR(%o0, %o1, %o2)
	retl
	  nop
	SET_SIZE(vac_flushcolor)

	ENTRY(vac_flushcolor_tl1)
	/*
	 * %g1 = vcolor
	 */
	DCACHE_FLUSHCOLOR(%g1, %g2, %g3)
	retry
	SET_SIZE(vac_flushcolor_tl1)


#ifdef SEND_MONDO_STATS
	.global	x_early
	.seg 	"data"
	.align	4
x_early:
	.skip	NCPU * 64 * 4		/* 64 integer sized element/cpu */
	.seg	"text"
#endif

	.global _dispatch_status_busy
_dispatch_status_busy:
	.asciz	"ASI_INTR_DISPATCH_STATUS error: busy"
	.align	4

/*
 * Setup interrupt dispatch data registers
 * Entry:
 *	%o0 - function or inumber to call
 *	ILP32:	%o1/%o2, %o3/%o4 - arguments (2 uint64_t's)
 *	LP64:	%o1, %o2 - arguments (2 uint64_t's)
 */
	.seg "text"

	ENTRY(init_mondo)
#ifdef DEBUG
	!
	! IDSR should not be busy at the moment
	!
	ldxa	[%g0]ASI_INTR_DISPATCH_STATUS, %g1
	btst	IDSR_BUSY, %g1
	bz,pt	%xcc, 1f
	mov	IDDR_0, %o5
	sethi	%hi(_dispatch_status_busy), %o0
	call	panic
	or	%o0, %lo(_dispatch_status_busy), %o0
#endif /* DEBUG */
	!
	! interrupt vector dispach data reg 0
	!
	mov	IDDR_0, %o5
1:
	stxa	%o0, [%o5]ASI_INTR_DISPATCH

	!
	! interrupt vector dispach data reg 1
	!
#ifndef __sparcv9
	! arg1 passed in %o1/%o2
	sllx	%o1, 32, %o1
	srl	%o2, 0, %o2
	or	%o1, %o2, %o1
#endif
	mov	IDDR_1, %o5
	stxa	%o1, [%o5]ASI_INTR_DISPATCH

	!
	! interrupt vector dispach data reg 2
	!
#ifndef __sparcv9
	! arg2 passed in %o3/%o4
	sllx	%o3, 32, %o3
	srl	%o4, 0, %o4
	or	%o3, %o4, %o2
#endif
	mov	IDDR_2, %o5
	stxa	%o2, [%o5]ASI_INTR_DISPATCH

	retl
	membar	#Sync			! allowed to be in the delay slot
	SET_SIZE(init_mondo)

/*
 * Send a mondo interrupt to the processor. (TL==0)
 * 	Entry:
 *		%o0 - upa port id of the processor
 *
 * 	Register Usage:
 *		%o1 - dispatch status
 *		%o2 - count of NACKS
 *		%o3 - count of BUSYS within most recent NACK
 *		%o4 - baseline tick value
 *		%o5 - tick limit
 *		%g1 - DCR value
 *		%g2 - temporary
 *		%g3 - 0x20	 (SF_ERRATA_54 only)
 *		%g5 - value of tick for current iteration
 */
	ENTRY(send_mondo)
	!
	! construct the interrupt dispatch command register in %g1
	! also, get the dispatch out as SOON as possible
	! (initial analysis puts the minimum dispatch time at around
	!  30-60 cycles.  hence, we try to get the dispatch out quickly
	!  and then start the rapid check loop).
	!
	rd	%tick, %o4			! baseline tick
	sll	%o0, IDCR_PID_SHIFT, %g1	! IDCR<18:14> = upa port id
	or	%g1, IDCR_OFFSET, %g1		! IDCR<13:0> = 0x70
	stxa	%g0, [%g1]ASI_INTR_DISPATCH	! interrupt vector dispatch
#if defined(SF_ERRATA_54)
	membar	#Sync				! store must occur before load
	mov	0x20, %g3			! UDBH Control Register Read
	ldxa	[%g3]ASI_SDB_INTR_R, %g0
#endif
	membar	#Sync
	clr	%o2				! clear NACK counter
	clr	%o3				! clear BUSY counter

	!
	! how long, in ticks, are we willing to wait completely
	!
	sethi	%hi(xc_tick_limit), %g2
	ldx	[%g2 + %lo(xc_tick_limit)], %g2
	add	%g2, %o4, %o5			! compute the limit value

	!
	! check the dispatch status
	!
.check_dispatch:
	ldxa	[%g0]ASI_INTR_DISPATCH_STATUS, %o1
	brz,pn	%o1, .dispatch_complete
	  rd	%tick, %g5

	!
	! see if we've gone beyond the limit
	! (can tick ever overflow?)
	!
.timeout_primed:
	sub	%o5, %g5, %g2			! limit - tick < 0 if timeout
	brgez,pt %g2, .check_busy
	  inc	%o3				! bump the BUSY counter

	!
	! Before we die, see if we are already panicking by checking
	! panic_quiesce (rather than panicstr); the panic code itself can call
	! here to stop the non-panic CPUs prior to setting panicstr.
	!
	mov	%o0, %o1			! save target
	sethi	%hi(_send_mondo_nack), %o0
	or	%o0, %lo(_send_mondo_nack), %o0
	sethi	%hi(panic_quiesce), %g2
	ld	[%g2 + %lo(panic_quiesce)], %g2
	brnz	%g2, .dispatch_complete		! skip if already in panic
	  nop
	call	panic
	  nop

.check_busy:
	btst	IDSR_BUSY, %o1			! was it BUSY?
	bnz,pt	%xcc, .check_dispatch
	  nop

	!
	! we weren't busy, we must have been NACK'd
	! wait a while and send again
	! (this might need jitter)
	!
	sethi	%hi(Cpudelay), %g2
	ld	[%g2 + %lo(Cpudelay)], %g2	! microsecond countdown counter
	orcc	%g2, 0, %g2			! set cc bits to nz
.delay:	bnz,pt	%xcc, .delay			! microsecond countdown loop
	  subcc	%g2, 1, %g2			! 2 instructions in loop

	stxa	%g0, [%g1]ASI_INTR_DISPATCH	! interrupt vector dispatch
#if defined(SF_ERRATA_54)
	membar	#Sync				! store must occur before load
	ldxa	[%g3]ASI_SDB_INTR_R, %g0
#endif
	membar	#Sync
	clr	%o3				! reset BUSY counter
	ba	.check_dispatch
	  inc	%o2				! bump the NACK counter

.dispatch_complete:
#ifdef SEND_MONDO_STATS
	!
	! Increment the appropriate entry in a send_mondo timeout array
	! x_entry[CPU][MSB]++;
	sub	%g5, %o4, %g5			! how long did we wait?
	clr	%o1				! o1 is now bit counter
1:	orcc	%g5, %g0, %g0			! any bits left?
	srlx	%g5, 1, %g5			! bits to the right
	bne,a,pt %xcc, 1b
	  add	%o1, 4, %o1			! pointer increment

	!
	! now compute the base of the x_early entry for our cpu
	!
	CPU_INDEX(%o0, %g5)
	sll	%o0, 8, %o0			! 64 * 4
	add	%o0, %o1, %o1			! %o0 = &[CPU][delay]

	!
	! and increment the appropriate value
	!
	sethi	%hi(x_early), %o0
	or	%o0, %lo(x_early), %o0
	ld	[%o0 + %o1], %g5
	inc	%g5
	st	%g5, [%o0 + %o1]
#endif	/* SEND_MONDO_STATS */
	retl
	  nop
	SET_SIZE(send_mondo)

_send_mondo_nack:
	.asciz	"send_mondo: timeout (target 0x%x) [%d NACK, %d BUSY]"
	.align	4


/*
 * flush_instr_mem:
 *	Flush a portion of the I-$ starting at vaddr
 * 	%o0 vaddr
 *	%o1 bytes to be flushed
 */

	ENTRY(flush_instr_mem)
	membar	#StoreStore				! Ensure the stores
							! are globally visible
1:
	flush	%o0
	subcc	%o1, ICACHE_FLUSHSZ, %o1		! bytes = bytes-0x20
	bgu,pt	%ncc, 1b
	add	%o0, ICACHE_FLUSHSZ, %o0		! vaddr = vaddr+0x20

	retl
	nop
	SET_SIZE(flush_instr_mem)

/*
 * flush_ecache:
 * Flush the entire e$ using displacement flush by reading through a
 * physically contiguous area. We use mmu bypass asi (ASI_MEM) while
 * reading this physical address range so that data doesn't go to d$.
 * incoming arguments:
 *   ILP32:
 *	%o0,%o1 - 64 bit physical address
 *	%o2 - size of address range to read
 *   LP64:
 *	%o0 - 64 bit physical address
 *	%o1 - size of address range to read
 *
 * For ILP32, we assume that PSTATE_AM will be correctly saved
 * and restored on interrupts.
 */
	ENTRY(flush_ecache)
	set	ecache_linesize, %g3
	ld	[%g3], %g3

#ifndef __sparcv9
	sllx	%o0, 32, %o0		! shift upper 32 bits
	srl	%o1, 0, %o1		! clear upper 32 bits
	or	%o0, %o1, %o0		! form 64 bit physaddr in %g2
					! using (%o0, %o1)
	mov	%o2, %o1

	rdpr	%pstate, %g4
	andn	%g4, PSTATE_AM, %g5
	wrpr	%g0, %g5, %pstate	! clear AM to access 64 bit physaddr
#endif

	b	2f
	  nop
1:
	ldxa	[%o0 + %o1]ASI_MEM, %g0	! start reading from physaddr + size
2:
	subcc	%o1, %g3, %o1
	bcc,a,pt %ncc, 1b
	  nop

#ifndef __sparcv9
	wrpr	%g0, %g4, %pstate	! restore earlier pstate
#endif

	retl
	nop
	SET_SIZE(flush_ecache)

#endif /* lint */

#if defined(lint)
/*
 * The ce_err function handles trap type 0x63 (corrected_ECC_error) at tl=0.
 * Steps: 1. GET AFSR  2. Get AFAR <40:4> 3. Get datapath error status
 *	  4. Clear datapath error bit(s) 5. Clear AFSR error bit
 *	  6. package data in %g2 and %g3 7. call cpu_ce_error vis sys_trap
 * %g2: [ 52:43 UDB lower | 42:33 UDB upper | 32:0 afsr ] - arg #3/arg #1
 * %g3: [ 40:4 afar ] - sys_trap->have_win: arg #4/arg #2
 */
void
ce_err(void)
{}

void
ce_err_tl1(void)
{}


/*
 * The async_err function handles trap types 0xA (instruction_access_error)
 * and 0x32 (data_access_error) at tl=0.
 * Steps: 1. Get AFSR 2. Get AFAR <40:4> 3. If not UE error skip UDP registers.
 *	  4. Else get and clear datapath error bit(s) 4. Clear AFSR error bits
 *	  6. package data in %g2 and %g3 7. disable all cpu errors, because
 *	  trap is likely to be fatal 8. call cpu_async_error vis sys_trap
 * %g3: [ 52:43 UDB lower | 42:33 UDB upper |32:0 afsr ] - arg #3/arg #1
 * %g2: [ 40:4 afar ] - sys_trap->have_win: arg #4/arg #2
 */
void
async_err(void)
{}

/*
 * The dis_err_panic1 function handles async errors at tl>=1 and also all
 * correctable ecc errors in DEBUG kernels at tl>=1.
 * Steps: 1. disable all errors 2. read existing error 3. panic
 */
void
dis_err_panic1(void)
{}

/*
 * The clr_datapath function clears any error bits set in the UDB regs.
 */
void
clr_datapath(void)
{}

/*
 * The get_udb_errors() function gets the current value of the
 * Datapath Error Registers.
 */
/* ARGSUSED */
void
get_udb_errors(uint64_t *udbh, uint64_t *udbl)
{
	*udbh = 0;
	*udbl = 0;
}

#else 	/* lint */

	ENTRY_NP(ce_err)
	ldxa	[%g0]ASI_AFSR, %g3	! save afsr in g3

	!
	! Check for a UE... From Kevin.Normoyle:
	! We try to switch to the trap for the UE, but since that's
	! a hardware pipeline, we might get to the CE trap before we
	! can switch. The UDB and AFSR registers will have both the
	! UE and CE bits set but the UDB syndrome and the AFAR will be
	! for the UE.
	!
	or	%g0, 1, %g1		! put 1 in g1
	sllx	%g1, 21, %g1		! shift left to <21> afsr UE
	andcc	%g1, %g3, %g0		! check for UE in afsr
	bnz	async_err		! handle the UE, not the CE
	  nop

	! handle the CE
	ldxa	[%g0]ASI_AFAR, %g2	! save afar in g2

	set	P_DER_H, %g4		! put P_DER_H in g4
	ldxa	[%g4]ASI_SDB_INTR_R, %g5 ! read sdb upper half into g5
	or	%g0, 1, %g6		! put 1 in g6
	sllx	%g6, 8, %g6		! shift g6 to <8> sdb CE
	andcc	%g5, %g6, %g1		! check for CE in upper half
	sllx	%g5, 33, %g5		! shift upper bits to <42:33>
	or	%g3, %g5, %g3		! or with afsr bits
	bz,a	1f			! no error, goto 1f
	  nop
	stxa	%g1, [%g4]ASI_SDB_INTR_W ! clear sdb reg error bit
	membar	#Sync			! membar sync required
1:
	set	P_DER_L, %g4		! put P_DER_L in g4
	ldxa	[%g4]ASI_SDB_INTR_R, %g5 ! read sdb lower half into g6
	andcc	%g5, %g6, %g1		! check for CE in lower half
	sllx	%g5, 43, %g5		! shift upper bits to <52:43>
	or	%g3, %g5, %g3		! or with afsr bits
	bz,a	2f			! no error, goto 2f
	  nop
	stxa	%g1, [%g4]ASI_SDB_INTR_W ! clear sdb reg error bit
	membar	#Sync			! membar sync required
2:
	or	%g0, 1, %g4		! put 1 in g4
	sllx	%g4, 20, %g4		! shift left to <20> afsr CE
	stxa	%g4, [%g0]ASI_AFSR	! use g4 to clear afsr CE error
	membar	#Sync			! membar sync required

	set	cpu_ce_error, %g1	! put *cpu_ce_error() in g1
	rdpr	%pil, %g6		! read pil into %g6
	subcc	%g6, PIL_15, %g0
	  movneg	%icc, PIL_14, %g4 ! run at pil 14 unless already at 15
	sethi	%hi(sys_trap), %g5
	jmp	%g5 + %lo(sys_trap)	! goto sys_trap
	  movge	%icc, PIL_15, %g4	! already at pil 15
	SET_SIZE(ce_err)

	ENTRY_NP(ce_err_tl1)
#ifndef	TRAPTRACE
	ldxa	[%g0]ASI_AFSR, %g7
	stxa	%g7, [%g0]ASI_AFSR
	membar	#Sync
	retry
#else
	set	ce_trap_tl1, %g1
	sethi	%hi(dis_err_panic1), %g4
	jmp	%g4 + %lo(dis_err_panic1)
	nop
#endif
	SET_SIZE(ce_err_tl1)

#ifdef	TRAPTRACE
.celevel1msg:
	.asciz	"Softerror with trap tracing at tl1: AFAR 0x%08x.%08x AFSR 0x%08x.%08x";

	ENTRY_NP(ce_trap_tl1)
	! upper 32 bits of AFSR already in o3
	mov	%o4, %o0		! save AFAR upper 32 bits
	mov	%o2, %o4		! lower 32 bits of AFSR
	mov	%o1, %o2		! lower 32 bits of AFAR
	mov	%o0, %o1		! upper 32 bits of AFAR
	set	.celevel1msg, %o0
	call	panic
	nop
	SET_SIZE(ce_trap_tl1)
#endif

	ENTRY_NP(async_err)
	stxa	%g0, [%g0]ASI_ESTATE_ERR ! disable ecc and other cpu errors
	membar	#Sync			! membar sync required

	ldxa	[%g0]ASI_AFSR, %g3	! save afsr in g3
	ldxa	[%g0]ASI_AFAR, %g2	! save afar in g2

	or	%g0, 1, %g1		! put 1 in g1
	sllx	%g1, 21, %g1		! shift left to <21> afsr UE
	andcc	%g1, %g3, %g0		! check for UE in afsr
	bz,a,pn %icc, 2f		! if !UE skip sdb read/clear
	  nop

	set	P_DER_H, %g4		! put P_DER_H in g4
	ldxa	[%g4]ASI_SDB_INTR_R, %g5 ! read sdb upper half into 56
	or	%g0, 1, %g6		! put 1 in g6
	sllx	%g6, 9, %g6		! shift g6 to <9> sdb UE
	andcc	%g5, %g6, %g1		! check for UE in upper half
	sllx	%g5, 33, %g5		! shift upper bits to <42:33>
	or	%g3, %g5, %g3		! or with afsr bits
	bz,a	1f			! no error, goto 1f
	  nop
	stxa	%g1, [%g4]ASI_SDB_INTR_W ! clear sdb reg UE error bit
	membar	#Sync			! membar sync required
1:
	set	P_DER_L, %g4		! put P_DER_L in g4
	ldxa	[%g4]ASI_SDB_INTR_R, %g5 ! read sdb lower half into g5
	andcc	%g5, %g6, %g1		! check for UE in lower half
	sllx	%g5, 43, %g5		! shift upper bits to <52:43>
	or	%g3, %g5, %g3		! or with afsr bits
	bz,a	2f			! no error, goto 2f
	  nop
	stxa	%g1, [%g4]ASI_SDB_INTR_W ! clear sdb reg UE error bit
	membar	#Sync			! membar sync required
2:
	or	%g0, 0x1F, %g5		! upper 5 sticky bits of afsr
	sllx	%g5, 7, %g5		! shift left 7 positions
	or	%g5, 0x77, %g5		! lower 7 sticky bits of afsr (!CP)
	sllx	%g5, 21, %g5		! shift sticky bits to <32:21>
	and	%g3, %g5, %g4 		! mask saved afsr into g4
	stxa	%g4, [%g0]ASI_AFSR	! clear masked bits in afsr
	membar	#Sync			! membar sync required

	set	cpu_async_error, %g1	! put cpu_async_error in g1
	sethi	%hi(sys_trap), %g5
	jmp	%g5 + %lo(sys_trap)	! goto sys_trap
	  or	%g0, PIL_15, %g4	! run at pil 15
	SET_SIZE(async_err)

	ENTRY_NP(dis_err_panic1)
	stxa	%g0, [%g0]ASI_ESTATE_ERR ! disable all error traps
	membar	#Sync
	! save destination routine is in g1
	ldxa	[%g0]ASI_AFAR, %g2	! read afar
	ldxa	[%g0]ASI_AFSR, %g3	! read afsr
	set	P_DER_H, %g4		! put P_DER_H in g4
	ldxa	[%g4]ASI_SDB_INTR_R, %g5 ! read sdb upper half into g5
	sllx	%g5, 33, %g5		! shift upper bits to <42:33>
	or	%g3, %g5, %g3		! or with afsr bits
	set	P_DER_L, %g4		! put P_DER_L in g4
	ldxa	[%g4]ASI_SDB_INTR_R, %g5 ! read sdb lower half into g5
	sllx	%g5, 43, %g5		! shift upper bits to <52:43>
	or	%g3, %g5, %g3		! or with afsr bits
	sethi	%hi(sys_trap), %g5
	jmp	%g5 + %lo(sys_trap)	! goto sys_trap
	  sub	%g0, 1, %g4
	SET_SIZE(dis_err_panic1)

	ENTRY(clr_datapath)
	set	P_DER_H, %o4			! put P_DER_H in o4
	ldxa	[%o4]ASI_SDB_INTR_R, %o5	! read sdb upper half into o3
	or	%g0, 0x3, %o2			! put 0x3 in o2
	sllx	%o2, 8, %o2			! shift o2 to <9:8> sdb
	andcc	%o5, %o2, %o1			! check for UE,CE in upper half
	bz,a	1f				! no error, goto 1f
	  nop
	stxa	%o1, [%o4]ASI_SDB_INTR_W	! clear sdb reg UE,CE error bits
	membar	#Sync				! membar sync required
1:
	set	P_DER_L, %o4			! put P_DER_L in o4
	ldxa	[%o4]ASI_SDB_INTR_R, %o5	! read sdb lower half into o5
	andcc	%o5, %o2, %o1			! check for UE,CE in lower half
	bz,a	2f				! no error, goto 2f
	  nop
	stxa	%o1, [%o4]ASI_SDB_INTR_W	! clear sdb reg UE,CE error bits
	membar	#Sync
2:
	retl
	  nop
	SET_SIZE(clr_datapath)

	ENTRY(get_udb_errors)
	set	P_DER_H, %o3
	ldxa	[%o3]ASI_SDB_INTR_R, %o2
	stx	%o2, [%o0]
	set	P_DER_L, %o3
	ldxa	[%o3]ASI_SDB_INTR_R, %o2
	retl
	  stx	%o2, [%o1]
	SET_SIZE(get_udb_errors)

#endif /* lint */

#if defined(lint)
/*
 * The itlb_rd_entry and dtlb_rd_entry functions return the tag portion of the
 * tte, the virtual address, and the ctxnum of the specified tlb entry.  They
 * should only be used in places where you have no choice but to look at the
 * tlb itself.
 *
 * Note: These two routines are required by the Estar "cpr" loadable module.
 */
/* ARGSUSED */
void
itlb_rd_entry(u_int entry, tte_t *tte, caddr_t *addr, int *ctxnum)
{}

/* ARGSUSED */
void
dtlb_rd_entry(u_int entry, tte_t *tte, caddr_t *addr, int *ctxnum)
{}
#else 	/* lint */
/*
 * NB - In Spitfire cpus, when reading a tte from the hardware, we
 * need to clear [42-41] because the general definitions in pte.h
 * define the PA to be [42-13] whereas Spitfire really uses [40-13].
 * When cloning these routines for other cpus the "andn" below is not
 * necessary.
 */
	ENTRY_NP(itlb_rd_entry)
	sllx	%o0, 3, %o0
#if defined(SF_ERRATA_32)
	sethi	%hi(FLUSH_ADDR), %g2
	set	MMU_PCONTEXT, %g1
	stxa	%g0, [%g1]ASI_DMMU			! KCONTEXT
	flush	%g2
#endif
	ldxa	[%o0]ASI_ITLB_ACCESS, %g1
	set	TTE_SPITFIRE_PFNHI_CLEAR, %g2		! spitfire only
	sllx	%g2, TTE_SPITFIRE_PFNHI_SHIFT, %g2	! see comment above
	andn	%g1, %g2, %g1				! for details
	stx	%g1, [%o1]
	ldxa	[%o0]ASI_ITLB_TAGREAD, %g2
	set	TAGREAD_CTX_MASK, %o4
	and	%g2, %o4, %o5
	st	%o5, [%o3]
	andn	%g2, %o4, %o5
	retl
	  stn	%o5, [%o2]
	SET_SIZE(itlb_rd_entry)

	ENTRY_NP(dtlb_rd_entry)
	sllx	%o0, 3, %o0
#if defined(SF_ERRATA_32)
	sethi	%hi(FLUSH_ADDR), %g2
	set	MMU_PCONTEXT, %g1
	stxa	%g0, [%g1]ASI_DMMU			! KCONTEXT
	flush	%g2
#endif
	ldxa	[%o0]ASI_DTLB_ACCESS, %g1
	set	TTE_SPITFIRE_PFNHI_CLEAR, %g2		! spitfire only
	sllx	%g2, TTE_SPITFIRE_PFNHI_SHIFT, %g2	! see comment above
	andn	%g1, %g2, %g1				! itlb_rd_entry
	stx	%g1, [%o1]
	ldxa	[%o0]ASI_DTLB_TAGREAD, %g2
	set	TAGREAD_CTX_MASK, %o4
	and	%g2, %o4, %o5
	st	%o5, [%o3]
	andn	%g2, %o4, %o5
	retl
	  stn	%o5, [%o2]
	SET_SIZE(dtlb_rd_entry)
#endif /* lint */

#if defined(lint)

/*
 * routines to get and set the LSU register
 */
uint64_t
get_lsu()
{
	return ((uint64_t)0);
}

/* ARGSUSED */
void
set_lsu(uint64_t lsu)
{}

#else /* lint */

	ENTRY(set_lsu)
#ifndef __sparcv9
	sllx	%o0, 32, %o0			! shift upper 32 bits
	srl	%o1, 0, %o1			! clear upper 32 bits
	or	%o0, %o1, %o0			! form 64 bit physaddr in
						! %o0 using (%o0,%o1)
#endif
	stxa	%o0, [%g0]ASI_LSU		! store to LSU
	retl
	  membar	#Sync
	SET_SIZE(set_lsu)

	ENTRY(get_lsu)
	ldxa	[%g0]ASI_LSU, %o0		! load LSU
#ifndef __sparcv9
	srl	%o0, 0, %o1			! put lower 32 bits in o1
						! clear upper 32 bits
	retl
	srlx	%o0, 32, %o0			! put the high 32 bits in
						! low part of o0
#endif
	retl
	  nop
	SET_SIZE(get_lsu)

#endif /* lint */
