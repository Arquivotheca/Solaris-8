/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sfdr_asm.s	1.7	98/09/08 SMI"

/*
 * This file is through cpp before being used as
 * an inline.  It contains support routines used
 * only by DR for the copy-rename sequence.
 */

#if defined(lint)
#include <sys/types.h>
#else
#include "assym.h"
#endif /* lint */

#include <sys/asm_linkage.h>
#include <sys/param.h>
#include <sys/privregs.h>
#include <sys/spitasi.h>
#include <sys/spitregs.h>
#include <sys/mmu.h>
#include <sys/machthread.h>
#include <sys/pte.h>
#include <sys/stack.h>
#include <sys/vis.h>

#ifndef	lint

/*
 * arg1 = icache_size
 * arg2 = icache_linesize
 */
#define	ICACHE_FLUSHALL(lbl, arg1, arg2, tmp1)			\
	ldxa	[%g0]ASI_LSU, tmp1				;\
	btst	LSU_IC, tmp1					;\
	bz,pn	%icc, lbl/**/1					;\
	sub	arg1, arg2, tmp1				;\
lbl/**/0:							;\
	stxa	%g0, [tmp1]ASI_IC_TAG				;\
	membar	#Sync						;\
	cmp	%g0, tmp1					;\
	bne,pt	%icc, lbl/**/0					;\
	sub	tmp1, arg2, tmp1				;\
lbl/**/1:

/*
 * arg1 = dcache_size
 * arg2 = dcache_linesize
 */
#define	DCACHE_FLUSHALL(lbl, arg1, arg2, tmp1)			\
	ldxa	[%g0]ASI_LSU, tmp1				;\
	btst	LSU_DC, tmp1					;\
	bz,pn	%icc, lbl/**/1					;\
	sub	arg1, arg2, tmp1				;\
lbl/**/0:							;\
	stxa	%g0, [tmp1]ASI_DC_TAG				;\
	membar	#Sync						;\
	cmp	%g0, tmp1					;\
	bne,pt	%icc, lbl/**/0					;\
	sub	tmp1, arg2, tmp1				;\
lbl/**/1:

/*
 * arg1 = ecache flush physaddr
 * arg2 = size
 * arg3 = ecache_linesize
 */
#define	ECACHE_FLUSHALL(lbl, arg1, arg2, arg3, tmp1, tmp2)	\
	rdpr	%pstate, tmp1					;\
	andn	tmp1, PSTATE_IE | PSTATE_AM, tmp2		;\
	wrpr	%g0, tmp2, %pstate				;\
	b	lbl/**/1					;\
lbl/**/0:							;\
	sub	arg2, arg3, arg2				;\
lbl/**/1:							;\
	brgez,a	arg2, lbl/**/0					;\
	ldxa	[arg1 + arg2]ASI_MEM, %g0			;\
	wrpr	%g0, tmp1, %pstate

#ifdef SF_ERRATA_32
#define	SF_WORKAROUND(tmp1, tmp2)				\
	sethi	%hi(FLUSH_ADDR), tmp2				;\
	set	MMU_PCONTEXT, tmp1				;\
	stxa	%g0, [tmp1]ASI_DMMU				;\
	flush	tmp2						;
#else
#define	SF_WORKAROUND(tmp1, tmp2)
#endif /* SF_ERRATA_32 */

/*
 * arg1 = vaddr
 * arg2 = ctxnum
 *	- disable interrupts and clear address mask
 *	  to access 64 bit physaddr
 *	- Blow out the TLB.
 *	  . If it's kernel context, then use primary context.
 *	  . Otherwise, use secondary.
 */
#define VTAG_FLUSHPAGE(lbl, arg1, arg2, tmp1, tmp2, tmp3, tmp4)	\
	rdpr	%pstate, tmp1					;\
	andn	tmp1, PSTATE_IE | PSTATE_AM, tmp2		;\
	wrpr	tmp2, 0, %pstate				;\
	brnz,pt	arg2, lbl/**/1					;\
	sethi	%hi(FLUSH_ADDR), tmp2				;\
	stxa	%g0, [arg1]ASI_DTLB_DEMAP			;\
	stxa	%g0, [arg1]ASI_ITLB_DEMAP			;\
	b	lbl/**/5					;\
	  flush	tmp2						;\
lbl/**/1:							;\
	set	MMU_SCONTEXT, tmp3				;\
	ldxa	[tmp3]ASI_DMMU, tmp4				;\
	or	DEMAP_SECOND | DEMAP_PAGE_TYPE, arg1, arg1	;\
	cmp	tmp4, arg2					;\
	be,a,pt	%icc, lbl/**/4					;\
	  nop							;\
	stxa	arg2, [tmp3]ASI_DMMU				;\
lbl/**/4:							;\
	stxa	%g0, [arg1]ASI_DTLB_DEMAP			;\
	stxa	%g0, [arg1]ASI_ITLB_DEMAP			;\
	flush	tmp2						;\
	be,a,pt	%icc, lbl/**/5					;\
	  nop							;\
	stxa	tmp4, [tmp3]ASI_DMMU				;\
	flush	tmp2						;\
lbl/**/5:							;\
	wrpr	%g0, tmp1, %pstate

/*
 * arg1 = dtlb entry
 *	- Before first compare:
 *		tmp4 = tte
 *		tmp5 = vaddr
 *		tmp6 = cntxnum
 */
#define	DTLB_FLUSH_UNLOCKED(lbl, arg1, tmp1, tmp2, tmp3, \
				tmp4, tmp5, tmp6) \
lbl/**/0:							;\
	sllx	arg1, 3, tmp3					;\
	SF_WORKAROUND(tmp1, tmp2)				;\
	ldxa	[tmp3]ASI_DTLB_ACCESS, tmp4			;\
	srlx	tmp4, 6, tmp4					;\
	andcc	tmp4, 1, %g0					;\
	bnz,pn	%xcc, lbl/**/1					;\
	srlx	tmp4, 57, tmp4					;\
	andcc	tmp4, 1, %g0					;\
	beq,pn	%xcc, lbl/**/1					;\
	  nop							;\
	set	TAGREAD_CTX_MASK, tmp1				;\
	ldxa	[tmp3]ASI_DTLB_TAGREAD, tmp2			;\
	and	tmp2, tmp1, tmp6				;\
	andn	tmp2, tmp1, tmp5				;\
	VTAG_FLUSHPAGE(VD, tmp5, tmp6, tmp1, tmp2, tmp3, tmp4)	;\
lbl/**/1:							;\
	brgz,pt	arg1, lbl/**/0					;\
	sub	arg1, 1, arg1

/*
 * arg1 = itlb entry
 *	- Before first compare:
 *		tmp4 = tte
 *		tmp5 = vaddr
 *		tmp6 = cntxnum
 */
#define	ITLB_FLUSH_UNLOCKED(lbl, arg1, tmp1, tmp2, tmp3, \
				tmp4, tmp5, tmp6) \
lbl/**/0:							;\
	sllx	arg1, 3, tmp3					;\
	SF_WORKAROUND(tmp1, tmp2)				;\
	ldxa	[tmp3]ASI_ITLB_ACCESS, tmp4			;\
	srlx	tmp4, 6, tmp4					;\
	andcc	tmp4, 1, %g0					;\
	bnz,pn	%xcc, lbl/**/1					;\
	srlx	tmp4, 57, tmp4					;\
	andcc	tmp4, 1, %g0					;\
	beq,pn	%xcc, lbl/**/1					;\
	  nop							;\
	set	TAGREAD_CTX_MASK, tmp1				;\
	ldxa	[tmp3]ASI_ITLB_TAGREAD, tmp2			;\
	and	tmp2, tmp1, tmp6				;\
	andn	tmp2, tmp1, tmp5				;\
	VTAG_FLUSHPAGE(VI, tmp5, tmp6, tmp1, tmp2, tmp3, tmp4)	;\
lbl/**/1:							;\
	brgz,pt	arg1, lbl/**/0					;\
	sub	arg1, 1, arg1

#define	CLEARTL(lvl)			\
	wrpr	%g0, lvl, %tl		;\
	wrpr	%g0, %g0, %tpc		;\
	wrpr	%g0, %g0, %tnpc		;\
	wrpr	%g0, %g0, %tt

#define	SWITCH_STACK(estk)					\
	flushw							;\
	sub	estk, SA(V9FPUSIZE+GSR_SIZE), estk		;\
	andn	estk, 0x3f, estk				;\
	sub	estk, SA(MINFRAME) + STACK_BIAS, %sp		;\
	mov	estk, %fp

#endif	/* !lint */

#if defined(lint)

/*ARGSUSED*/
void
sfdr_shutdown_asm(uint64_t estack, uint64_t flushaddr, int size)
{}

#else /* lint */

	ENTRY_NP(sfdr_shutdown_asm)

#if !defined(__sparcv9)
	sllx	%o0, 32, %o0
	srl	%o1, 0, %o1
	or	%o0, %o1, %o0		! estack

	sllx	%o2, 32, %o2
	srl	%o3, 0, %o3
	or	%o2, %o3, %o1		! flushaddr

	mov	%o4, %o2		! size
#endif /* !__sparcv9 */

	! %o0 = base (va mapping this code in bbsram)
	! %o1 = flushaddr for ecache
	! %o2 = size to use for ecache flush
	!
	membar	#LoadStore

	!
	! Switch stack pointer to bbsram
	!
	SWITCH_STACK(%o0)

	!
	! Get some globals
	!
	sethi	%hi(ecache_linesize), %g1
	ld	[%g1 + %lo(ecache_linesize)], %g1

	sethi	%hi(dcache_linesize), %g2
	ld	[%g2 + %lo(dcache_linesize)], %g2

	sethi	%hi(dcache_size), %g3
	ld	[%g3 + %lo(dcache_size)], %g3

	sethi	%hi(icache_linesize), %g4
	ld	[%g4 + %lo(icache_linesize)], %g4

	sethi	%hi(icache_size), %g5
	ld	[%g5 + %lo(icache_size)], %g5

	sethi	%hi(dtlb_entries), %o5
	ld	[%o5 + %lo(dtlb_entries)], %o5
	sllx	%o5, 32, %o5
	srlx	%o5, 32, %o5

	sethi	%hi(itlb_entries), %o3
	ld	[%o3 + %lo(itlb_entries)], %o3
	!
	! cram Xtlb_entries into a single register (%o5)
	! %o5 upper 32 = itlb_entries
	!     lower 32 = dtlb_entries
	!
	sllx	%o3, 32, %o3
	or	%o5, %o3, %o5

	!
	! Flush E$
	!
	ECACHE_FLUSHALL(EC, %o1, %o2, %g1, %o3, %o4)
	!
	! %o1 & %o2 now available
	!

	membar	#Sync

	!
	! Flush D$
	!
	DCACHE_FLUSHALL(DC, %g3, %g2, %o3)

	!
	! Flush I$
	!
	ICACHE_FLUSHALL(IC, %g5, %g4, %o3)

	membar	#Sync

	!
	! Flush dtlb's
	!
	srlx	%o5, 32, %g5		! %g5 = itlb_entries
	sllx	%o5, 32, %o5
	srlx	%o5, 32, %g1
	sub	%g1, 1, %g1		! %g1 = dtlb_entries - 1

	DTLB_FLUSH_UNLOCKED(D, %g1, %g3, %g4, %o2, %o3, %o4, %o5)

	!
	! Flush itlb's
	!
	sub	%g5, 1, %g1		! %g1 = itlb_entries - 1

	ITLB_FLUSH_UNLOCKED(I, %g1, %g3, %g4, %o2, %o3, %o4, %o5)

	membar	#Sync

5:
	ba	5b
	nop
	SET_SIZE(sfdr_shutdown_asm)

	.global	sfdr_shutdown_asm_end

	.skip	2048

sfdr_shutdown_asm_end:

#endif /* lint */

#if 0	/* grab cpus support */
#define	TT_HSM	0x99

#if defined(lint)
void
sfdr_freeze(void)
{}
#else /* lint */
/*
 * This routine quiets a cpu and has it spin on a barrier.
 * It is used during memory sparing so that no memory operation
 * occurs during the memory copy.
 *
 *	Entry:
 *		%g1    - gate array base address
 *		%g2    - barrier base address
 *		%g3    - arg2
 *		%g4    - arg3
 *
 * 	Register Usage:
 *		%g3    - saved pstate
 *		%g4    - temporary
 *		%g5    - check for panicstr
 */
	ENTRY_NP(sfdr_freeze)
	CPU_INDEX(%g4, %g5)
	sll	%g4, 2, %g4
	add	%g4, %g1, %g4			! compute address of gate id

	st	%g4, [%g4]			! indicate we are ready
	membar	#Sync
1:
	sethi	%hi(panicstr), %g5
	ld	[%g5 + %lo(panicstr)], %g5
	brnz	%g5, 2f				! exit if in panic
	 nop
	ld	[%g2], %g4
	brz,pt	%g4, 1b				! spin until barrier true
	 nop

2:
#ifdef  TRAPTRACE
	TRACE_PTR(%g3, %g4)
	rdpr	%tick, %g4
	stxa	%g4, [%g3 + TRAP_ENT_TICK]%asi
	stha	%g0, [%g3 + TRAP_ENT_TL]%asi
	set	TT_HSM, %g2
	or	%g2, 0xee, %g4
	stha	%g4, [%g3 + TRAP_ENT_TT]%asi
	sta	%o7, [%g3 + TRAP_ENT_TPC]%asi
	sta	%g0, [%g3 + TRAP_ENT_SP]%asi
	sta	%g0, [%g3 + TRAP_ENT_TR]%asi
	sta	%g0, [%g3 + TRAP_ENT_F1]%asi
	sta	%g0, [%g3 + TRAP_ENT_F2]%asi
	sta	%g0, [%g3 + TRAP_ENT_F3]%asi
	sta	%g0, [%g3 + TRAP_ENT_F4]%asi
	stxa	%g0, [%g3 + TRAP_ENT_TSTATE]%asi	/* tstate = pstate */
	TRACE_NEXT(%g2, %g3, %g4)
#endif	TRAPTRACE

	retry
	membar	#Sync
	SET_SIZE(sfdr_freeze)

#endif	/* lint */
#endif /* 0 */
