/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sfmmu_asm.s	1.123	99/06/30 SMI"

/*
 * SFMMU primitives.  These primitives should only be used by sfmmu
 * routines.
 */

#if defined(lint)
#include <sys/types.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/machtrap.h>
#include <sys/spitasi.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <vm/hat_sfmmu.h>
#include <sys/machparam.h>
#include <sys/privregs.h>
#include <sys/scb.h>
#include <sys/machthread.h>

#include <sys/spitregs.h>
#include <sys/clock.h>

#ifdef TRAPTRACE
#include <sys/traptrace.h>
#endif /* TRAPTRACE */

#ifndef	lint

/*
 * Assumes TSBE_TAG is 0
 * Assumes TSBE_INTHI is 0
 * Assumes TSBREG.split is 0	
 */

#if TSBE_TAG != 0
#error	- TSB_UPDATE and TSB_INVALIDATE assume TSBE_TAG = 0
#endif

#if TSBTAG_INTHI != 0
#error	- TSB_UPDATE and TSB_INVALIDATE assume TSBTAG_INTHI = 0
#endif

#if (1 << TSBINFO_SZSHIFT) != TSBINFO_SIZE
#error	- TSBBASESZ_SHIFT does not correspond to TSBINFO_SIZE
#endif

/*
 * The following code assumes the tsb is not split.
 */
#define	GET_TSB_POINTER8K(tsbreg, tagacc, tsbp, tmp1, tmp2, tmp3)	\
	/* get ctx */;							\
	sll	tagacc, TAGACC_CTX_SHIFT, tmp1;				\
	srln	tagacc, MMU_PAGESHIFT, tmp2;				\
	srl	tmp1, TAGACC_CTX_SHIFT, tmp1;				\
	sethi	%hi(TSB_MIN_BASE_MASK), tsbp;				\
	and	tsbreg, TSB_SOFTSZ_MASK, tmp3;	/* tsb size */		\
	sra	tsbp, 0, tsbp;			/* sign extend */	\
	sub	tmp3, TSB_MIN_SZCODE, tmp3;	/* adjust tsb size */	\
	sll	tsbp, tmp3, tmp3;		/* get mask */		\
	xor	tmp1, tmp2, tmp1;		/* hash ctx ^ vpg */	\
	andn	tmp1, tmp3, tmp1;		/* entry number */	\
	andn	tsbreg, TSB_SOFTSZ_MASK, tsbp;	/* get tsb base */	\
	sll	tmp1, TSB_ENTRY_SHIFT, tmp1;	/* get offset */	\
	or	tsbp, tmp1, tsbp

/*
 * This macro assumes the tsb is not split.
 *
 * We use bits [18:15] of the 8k tsb pointer in calculating
 * the 4M tsb pointer. This was stumbled across by mistake.
 * So far seems to produce the best results by spreading
 * the 4M entries in the TSB.
 */
#define	GET_TSB_POINTER4M(tsbreg, tagacc, tsbp, tmp1, tmp2, tmp3)	\
	GET_TSB_POINTER8K(tsbreg, tagacc, tsbp, tmp1, tmp2, tmp3)	;\
	srlx	tagacc, MMU_PAGESHIFT4M, tmp3	/* tmp3 = 4M vpn */	;\
	sethi	%hi(UTSB_MAX_BASE_MASK), tmp1				;\
	and	tsbp, tmp1, tmp2		/* tmp2 = 512kbbase */	;\
	andn	tmp3, tmp1, tmp3		/* mask off base */	;\
	srl	tmp3, 0, tmp3			/* tmp3 = vpn */	;\
	sll	tagacc, TAGACC_CTX_SHIFT, tmp1				;\
	srl	tmp1, TAGACC_CTX_SHIFT, tmp1	/* tmp1 = ctx */	;\
	xor	tmp1, tmp3, tmp1		/* tmp1 = ctx ^ vpg */	;\
	sll	tmp1, TSB_ENTRY_SHIFT, tmp1	/* tmp1 = offset */	;\
	or	tmp1, tmp2, tsbp

/* CSTYLED */
#define	BUILD_TSB_TAG(vaddr, ctx, tag, tmp1)				\
	sllx	ctx, TTARGET_CTX_SHIFT, tag;				\
	srln	vaddr, TTARGET_VA_SHIFT, tmp1;				\
	or	tmp1, tag, tag

	
#define	TSB_LOCK_ENTRY(tsb8k, tmp1, tmp2, label)			\
	ld	[tsb8k], tmp1						;\
label:									;\
	sethi	%hi(TSBTAG_LOCKED), tmp2				;\
	cmp	tmp1, tmp2 						;\
	be,a,pn	%icc, label/**/b	/* if locked spin */		;\
	  ld	[tsb8k], tmp1						;\
	casa	[tsb8k] ASI_N , tmp1, tmp2				;\
	cmp	tmp1, tmp2 						;\
	bne,a,pn %icc, label/**/b	/* didn't lock so try again */	;\
	  ld	[tsb8k], tmp1						;\
	/* tsbe lock acquired */					;\
	membar #StoreStore

#define	TSB_INSERT_UNLOCK_ENTRY(tsb8k, tte, tagtarget)			\
	stx	tte, [tsb8k + TSBE_TTE]		/* write tte data */	;\
	membar #StoreStore						;\
	stx	tagtarget, [tsb8k + TSBE_TAG]	/* write tte tag & unlock */

#define	TSB_UPDATE_TL(tsbp, tte, tagtarget, tmp1, tmp2, ttepa, label)	\
	TSB_LOCK_ENTRY(tsbp, tmp1, tmp2, label)				;\
	/*								;\
	 * I don't need to update the TSB then check for the valid tte.	;\
	 * TSB invalidate will spin till the entry is unlocked.	Note,	;\
	 * we always invalidate the hash table before we unload the TSB.;\
	 */								;\
	ldxa	[ttepa] ASI_MEM, tmp1					;\
	sethi	%hi(TSBTAG_INVALID), tmp2				;\
	brgez,a,pn tmp1, label/**/f					;\
	 st	tmp2, [tsbp + TSBE_TAG]	/* unlock */			;\
	TSB_INSERT_UNLOCK_ENTRY(tsbp, tte, tagtarget)			;\
label:

#define	TSB_UPDATE(tsbp, tteva, tagtarget, tmp1, tmp2, label)		\
	/* can't rd tteva after locking tsb because it can tlb miss */	;\
	ldx	[tteva] , tteva			/* load tte */		;\
	TSB_LOCK_ENTRY(tsbp, tmp1, tmp2, label)				;\
	sethi	%hi(TSBTAG_INVALID), tmp2				;\
	brgez,a,pn tteva, label/**/f					;\
	 st	tmp2, [tsbp + TSBE_TAG]	/* unlock */			;\
	TSB_INSERT_UNLOCK_ENTRY(tsbp, tteva, tagtarget)			;\
label:

#define	TSB_INVALIDATE8K(tsbp8k, tag, tmp1, tmp2, tmp3, label)		\
	ld	[tsbp8k], tmp1						;\
	sethi	%hi(TSBTAG_LOCKED), tmp2				;\
label/**/1:								;\
	cmp	tmp1, tmp2						;\
	be,a,pn	%icc, label/**/1					;\
	  ld	[tsbp8k], tmp1						;\
	ldx	[tsbp8k + TSBE_TAG], tmp3	/* load tag */		;\
	cmp	tag, tmp3						;\
	bne,pt	%xcc, label/**/2					;\
	sethi	%hi(TSBTAG_INVALID), tmp3				;\
	cas	[tsbp8k], tmp1, tmp3					;\
	cmp	tmp1, tmp3						;\
	bne,a,pn %icc, label/**/1					;\
	  ld	[tsbp8k], tmp1						;\
label/**/2:	

/*
 * This macro tries to avoid but does not guarentee invalidating
 * 8k entries since 8k and 4m entries can have the same tag.
 */
#define	TSB_INVALIDATE4M(tsbp4m, tag, tmp1, tmp2, tmp3, label)		\
	ldx	[tsbp4m + TSBE_TTE], tmp3	/* check for 4m */	;\
	srlx	tmp3, TTE_SZ_SHFT, tmp3					;\
	cmp	tmp3, (TTESZ_VALID | TTE_SZ_BITS)			;\
	bne,pt %icc, label/**/2						;\
	  ld	[tsbp4m], tmp1						;\
	sethi	%hi(TSBTAG_LOCKED), tmp2				;\
label/**/1:								;\
	cmp	tmp1, tmp2						;\
	be,a,pn	%icc, label/**/1					;\
	  ld	[tsbp4m], tmp1						;\
	ldx	[tsbp4m + TSBE_TAG], tmp3	/* load tag */		;\
	cmp	tag, tmp3						;\
	bne,pt	%xcc, label/**/2					;\
	sethi	%hi(TSBTAG_INVALID), tmp3				;\
	cas	[tsbp4m], tmp1, tmp3					;\
	cmp	tmp1, tmp3						;\
	bne,a,pn %icc, label/**/1					;\
	  ld	[tsbp4m], tmp1						;\
label/**/2:

/*
 * Macro to flush all tsb entries with ctx = ctxnum.
 * If this ctx contains large pages then we adjust base
 * for maximum tsb size supported for user.  This assumes
 * that all smaller tsb's are contained in the largest
 * supported tsb size. See also SFMMU_SELECT_TSBSIZE macro
 * and startup code. This macro uses virtual addresses to
 * access the tsb. It is never called for kernel context.
 * We assume that the TSBREG.split == 0. We also assume
 * that the tsb's live in the lower 32-bits of the kernel's
 * address space.
 *
 * ctxnum = 32 bit reg containing ctxnum to flush
 */
#define	FLUSH_TSBCTX(tsbreg, ctxnum, tmp1, tmp2, tmp3)			\
	sethi	%hi(utsb_4m_disable), tmp1	/* branch if disabled */;\
	ld	[tmp1 + %lo(utsb_4m_disable)], tmp1			;\
	brnz,pn	tmp1, 1f						;\
	  sethi	%hi(ctxs), tmp1						;\
	sll	ctxnum, CTX_SZ_SHIFT, tmp2				;\
	ldn	[tmp1 + %lo(ctxs)], tmp1				;\
	add	tmp1, tmp2, tmp1		/* tmp1 = ctxptr */	;\
	lduh	[tmp1 + C_FLAGS], tmp1		/* tmp1 = c_flags */	;\
	andcc	tmp1, LTTES_FLAG, %g0					;\
	bz,pt 	%icc, 1f			/* branch !lgpgs */	;\
	  sethi	%hi(UTSB_MAX_BASE_MASK), tmp3				;\
	/* Adjust to maximum supported tsb */				;\
	sll	tmp3, TSB_ENTRY_SHIFT, tmp3	/* tmp3 = base mask */	;\
	and	tsbreg, tmp3, tmp1		/* tmp1 = tsb base */	;\
	ba,pt	%xcc, 2f			/* tmp2 = entry # */	;\
	  sethi	%hi(TSB_ENTRIES(UTSB_MAX_SZCODE)), tmp2			;\
1:									;\
	/* use tsbreg for base and size */				;\
	andn	tsbreg, TSB_SZ_MASK, tmp1	/* tmp1 = tsbbase */	;\
	and	tsbreg, TSB_SZ_MASK, tmp2	/* get sz */		;\
	add	tmp2, TSB_START_SIZE, tmp2	/* adjust sz */		;\
	set	1, tmp3							;\
	sll	tmp3, tmp2, tmp2		/* get entry count */	;\
2:									;\
	sethi	%hi(TSBTAG_INVALID), tmp3	/* tmp3 = inv entry */	;\
3:									;\
	lduh	[tmp1], tsbreg			/* read tag */		;\
	dec	tmp2				/* dec. entry count */	;\
	cmp	tsbreg, ctxnum			/* compare the tags */	;\
	be,a,pt %icc, 4f						;\
	  st	tmp3, [tmp1]		/* invalidate the entry */	;\
4:									;\
	brnz,a,pt tmp2, 3b		/* if not end of TSB go back */ ;\
	add	tmp1, TSB_ENTRY_SIZE, tmp1	/* the next entry */	;\
	
	/* loads tsb hardware register */
#define	GET_UTSB_REGISTER(reg)						\
	set	MMU_TSB, reg;						\
	ldxa	[reg] ASI_DMMU, reg					

#define	GET_KTSB_REGISTER(reg)						\
	sethi	%hi(ktsb_reg), reg;					\
	ldx	[reg + %lo(ktsb_reg)], reg

/*
 * Check and load correct tsb info (tsb register and tte)
 */
#if TSB_SOFTSZ_MASK < TSB_SZ_MASK
#error	- TSB_SOFTSZ_MASK too small
#endif

#define	LOAD_TSBINFO(tsbinfo, tmp1, tmp2, tmp3)				\
	ldx	[tsbinfo + TSBINFO_REG], tmp1;	/* tsbreg */		\
	set	MMU_TSB, tmp3;						\
	ldx	[tsbinfo + TSBINFO_TTE], tmp2;	/* tsbtte */		\
	stxa	tmp1, [tmp3] ASI_IMMU;		/* itsb reg */		\
	stxa	tmp1, [tmp3] ASI_DMMU;		/* dtsb reg */		\
	sethi	%hi(FLUSH_ADDR), tmp3;					\
	brz,pn	tmp2, 1f;						\
	flush	tmp3;							\
	sethi	%hi(utsb_dtlb_ttenum), tsbinfo;				\
	ld	[tsbinfo + %lo(utsb_dtlb_ttenum)], tsbinfo;		\
	andn	tmp1, TSB_SZ_MASK, tmp1;	/* get tsb address */	\
	set	MMU_TAG_ACCESS, tmp3;					\
	sll	tsbinfo, 3, tsbinfo;		/* new entry index */	\
	stxa	tmp1, [tmp3] ASI_DMMU;		/* set tag acc */	\
	or	tmp1, DEMAP_NUCLEUS, tmp3;	/* new tsb */		\
	stxa	%g0, [tmp3] ASI_DTLB_DEMAP;	/* demap prev. */	\
	membar	#Sync;							\
	stxa	tmp2, [tsbinfo] ASI_DTLB_ACCESS;/* load locked tte */	\
	membar	#Sync;							\
1:
			
#define	CTXNUM_TO_TSBINFO(ctxnum, tsbinfo, tmp)				\
	sethi	%hi(ctxs), tmp;						\
	sll	ctxnum, CTX_SZ_SHIFT, tsbinfo;				\
	ldn	[tmp + %lo(ctxs)], tmp;					\
	add	tmp, tsbinfo, tmp;					\
	lduh	[tmp + C_FLAGS], tmp;					\
	sethi	%hi(tsb_bases), tsbinfo;				\
	andcc	tmp, LTSB_FLAG, %g0;					\
	srl	tmp, CTX_TSBINDEX_SHIFT, tmp;				\
	bz,a,pt %icc, 9f;						\
	  ldn	[tsbinfo + %lo(tsb_bases)], tsbinfo;			\
	sethi	%hi(tsb512k_bases), tsbinfo;				\
	ldn	[tsbinfo + %lo(tsb512k_bases)], tsbinfo;		\
9:	sll	tmp, TSBINFO_SZSHIFT, tmp;				\
	add	tsbinfo, tmp, tsbinfo

#define	CTXNUM_TO_TSBREG(ctxnum, tsbreg, tmp)				\
	CTXNUM_TO_TSBINFO(ctxnum, tsbreg, tmp);				\
	ldx	[tsbreg + TSBINFO_REG], tsbreg
	
#endif (lint)


#if defined (lint)

/*
 * sfmmu related subroutines
 */

/* ARGSUSED */
void
sfmmu_ctx_steal_tl1(uint64_t sctx, uint64_t rctx)
{}

/* ARGSUSED */
void
sfmmu_itlb_ld(caddr_t vaddr, int ctxnum, tte_t *tte)
{}

/* ARGSUSED */
void
sfmmu_dtlb_ld(caddr_t vaddr, int ctxnum, tte_t *tte)
{}

/*
 * Use cas, if tte has changed underneath us then reread and try again.
 * In the case of a retry, it will update sttep with the new original.
 */
/* ARGSUSED */
int
sfmmu_modifytte(tte_t *sttep, tte_t *stmodttep, tte_t *dttep)
{ return(0); }

/*
 * Use cas, if tte has changed underneath us then return 1, else return 0
 */
/* ARGSUSED */
int
sfmmu_modifytte_try(tte_t *sttep, tte_t *stmodttep, tte_t *dttep)
{ return(0); }

/* ARGSUSED */
void
sfmmu_copytte(tte_t *sttep, tte_t *dttep)
{}

int
sfmmu_getctx_pri()
{ return(0); }

int
sfmmu_getctx_sec()
{ return(0); }

/* ARGSUSED */
void
sfmmu_setctx_pri(int ctx)
{}

/* ARGSUSED */
void
sfmmu_setctx_sec(int ctx)
{}

uint
sfmmu_get_dsfar()
{ return(0); }

uint
sfmmu_get_isfsr()
{ return(0); }

uint
sfmmu_get_dsfsr()
{ return(0); }

uint
sfmmu_get_itsb()
{ return(0); }

uint
sfmmu_get_dtsb()
{ return(0); }

#else	/* lint */

	.seg	".data"
	.global	sfmmu_panic1
sfmmu_panic1:
	.ascii	"sfmmu_asm: interupts already disabled"
	.byte	0

#ifdef	__sparcv9
	.global	sfmmu_panic2
sfmmu_panic2:
	.asciz	"sfmmu_asm: PSTATE_AM bit should not be set"
	.byte	0
#endif	/* __sparcv9 */

	.global	sfmmu_panic3
sfmmu_panic3:
	.ascii	"sfmmu_asm: sfmmu_vatopfn called for user"
	.byte	0

	.global	sfmmu_panic4
sfmmu_panic4:
	.ascii	"sfmmu_asm: 4M tsb pointer mis-match"
	.byte	0

	.align	4
	.seg	".text"


/*
 * 1. Flush all TLB entries whose ctx is ctx-being-stolen.
 * 2. If processor is running in the ctx-being-stolen, set the
 *    context to the resv context. That is 
 *    If processor in User-mode - pri/sec-ctx both set to ctx-being-stolen,
 *		change both pri/sec-ctx registers to resv ctx.
 *    If processor in Kernel-mode - pri-ctx is 0, sec-ctx is ctx-being-stolen,
 *		just change sec-ctx register to resv ctx. When it returns to
 *		kenel-mode, user_rtt will change pri-ctx.
 * Note: TSB is flushed by the caller.
 */
	ENTRY(sfmmu_ctx_steal_tl1)
	/*
	 * %g1 = ctx being stolen (victim)
	 * %g2 = invalid ctx to replace victim with
	 */
	set	MMU_SCONTEXT, %g3
	set	DEMAP_CTX_TYPE | DEMAP_SECOND, %g4
	ldxa	[%g3]ASI_DMMU, %g5		/* get sec-ctx */
	cmp	%g5, %g1			/* is it the victim? */
	be,pn	%icc, 0f			/* yes, don't need to set it */
	sethi	%hi(FLUSH_ADDR), %g6
	stxa	%g1, [%g3]ASI_DMMU		/* temporarily set our */
0:						/*   sec-ctx to victim */
	stxa	%g0, [%g4]ASI_DTLB_DEMAP	/* flush DTLB */
	stxa	%g0, [%g4]ASI_ITLB_DEMAP	/* flush ITLB */
	flush	%g6				/* ensure stxa's committed */
	!
	! if (old sec-ctx != victim) {
	!	restore old sec-ctx
	! } else {
	!	if (pri-ctx == victim) {
	!		write resv ctx to sec-ctx
	!		write resv ctx to pri-ctx
	!	} else {
	!		write resv ctx to sec-ctx
	!	}
	! }
	!
	be,a,pn %icc, 1f			/* was our sec-ctx a victim? */
	mov	MMU_PCONTEXT, %g7		/* nope, restore the */
	stxa	%g5, [%g3]ASI_DMMU		/*   original sec-ctx */
	retry					/* and return */
1:	
	ldxa	[%g7]ASI_DMMU, %g4		/* get pri-ctx */
	cmp	%g1, %g4			/* is it the victim? */
	bne 	%icc, 2f			/* nope, no need to change it */
	nop
	stxa	%g2, [%g3]ASI_DMMU		/* set sec-ctx to invalid ctx */
	stxa	%g2, [%g7]ASI_DMMU		/* set pri-ctx to invalid ctx */
	retry					/* and return */
2:
	stxa	%g2, [%g3]ASI_DMMU		/* set sec-ctx to invalid ctx */
	retry					/* and return */
	SET_SIZE(sfmmu_ctx_steal_tl1)

	ENTRY_NP(sfmmu_itlb_ld)
	rdpr	%pstate, %o3
#ifdef DEBUG
	andcc	%o3, PSTATE_IE, %g0		/* if interrupts already */
	bnz,pt %icc, 1f				/* disabled, panic	 */
	  nop
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic1), %o0
1:
#ifdef	__sparcv9
	andcc	%o3, PSTATE_AM, %g0		/* if PSTATE_AM set panic */
	bz,pt	%icc, 2f
	  nop
	sethi	%hi(sfmmu_panic2), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic2), %o0
2:
#endif	/* __sparcv9 */
#endif /* DEBUG */
	wrpr	%o3, PSTATE_IE, %pstate		/* disable interrupts */
	srln	%o0, MMU_PAGESHIFT, %o0
	slln	%o0, MMU_PAGESHIFT, %o0		/* clear page offset */
	or	%o0, %o1, %o0
	ldx	[%o2], %g1
	set	MMU_TAG_ACCESS, %o5
	stxa	%o0,[%o5]ASI_IMMU
	stxa	%g1,[%g0]ASI_ITLB_IN
	sethi	%hi(FLUSH_ADDR), %o1		/* flush addr doesn't matter */
	flush	%o1				/* flush required for immu */
	retl
	  wrpr	%g0, %o3, %pstate		/* enable interrupts */
	SET_SIZE(sfmmu_itlb_ld)

	ENTRY_NP(sfmmu_dtlb_ld)
	rdpr	%pstate, %o3
#ifdef DEBUG
	andcc	%o3, PSTATE_IE, %g0		/* if interrupts already */
	bnz,pt	%icc, 1f			/* disabled, panic	 */
	  nop
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic1), %o0
1:
#ifdef	__sparcv9
	andcc	%o3, PSTATE_AM, %g0		/* if PSTATE_AM set panic */
	bz,pt	%icc, 2f
	  nop
	sethi	%hi(sfmmu_panic2), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic2), %o0
2:
#endif	/* __sparcv9 */
#endif /* DEBUG */
	wrpr	%o3, PSTATE_IE, %pstate		/* disable interrupts */
	srln	%o0, MMU_PAGESHIFT, %o0
	slln	%o0, MMU_PAGESHIFT, %o0		/* clear page offset */
	or	%o0, %o1, %o0
	ldx	[%o2], %g1
	set	MMU_TAG_ACCESS, %o5
	stxa	%o0,[%o5]ASI_DMMU
	stxa	%g1,[%g0]ASI_DTLB_IN
	membar	#Sync
	retl
	  wrpr	%g0, %o3, %pstate		/* enable interrupts */
	SET_SIZE(sfmmu_dtlb_ld)

	ENTRY_NP(sfmmu_modifytte)
	ldx	[%o2], %g3			/* current */
	ldx	[%o0], %g1			/* original */
2:
	ldx	[%o1], %g2			/* modified */
	cmp	%g2, %g3			/* is modified = current? */
	be,a,pt	%xcc,1f				/* yes, don't write */
	  stx	%g3, [%o0]			/* update new original */
	casx	[%o2], %g1, %g2
	cmp	%g1, %g2
	be,pt	%xcc, 1f			/* cas succeeded - return */
	  nop
	ldx	[%o2], %g3			/* new current */
	stx	%g3, [%o0]			/* save as new original */
	ba,pt	%xcc, 2b
	  mov	%g3, %g1
1:	retl
	membar	#StoreLoad
	SET_SIZE(sfmmu_modifytte)

	ENTRY_NP(sfmmu_modifytte_try)
	ldx	[%o1], %g2			/* modified */
	ldx	[%o2], %g3			/* current */
	ldx	[%o0], %g1			/* original */
	cmp	%g3, %g2			/* is modified = current? */
	be,a,pn %xcc,1f				/* yes, don't write */
	  mov	0, %o1				/* as if cas failed. */
		
	casx	[%o2], %g1, %g2
	membar	#StoreLoad
	cmp	%g1, %g2
	movne	%xcc, -1, %o1			/* cas failed. */
	move	%xcc, 1, %o1			/* cas succeeded. */
1:
	stx	%g2, [%o0]			/* report "current" value */
	retl
	mov	%o1, %o0
	SET_SIZE(sfmmu_modifytte_try)

	ENTRY_NP(sfmmu_copytte)
	ldx	[%o0], %g1
	retl
	stx	%g1, [%o1]
	SET_SIZE(sfmmu_copytte)

	ENTRY_NP(sfmmu_getctx_pri)
	set	MMU_PCONTEXT, %o0
	retl
	ldxa	[%o0]ASI_DMMU, %o0
	SET_SIZE(sfmmu_getctx_pri)

	ENTRY_NP(sfmmu_getctx_sec)
	set	MMU_SCONTEXT, %o0
	retl
	ldxa	[%o0]ASI_DMMU, %o0
	SET_SIZE(sfmmu_getctx_sec)

	ENTRY_NP(sfmmu_setctx_pri)
	set	MMU_PCONTEXT, %o1
	stxa	%o0, [%o1]ASI_DMMU
	sethi	%hi(FLUSH_ADDR), %o1		/* flush addr doesn't matter */
	retl
	  flush	%o1				/* flush required for immu */
	SET_SIZE(sfmmu_setctx_pri)

	ENTRY_NP(sfmmu_setctx_sec)
	/*
	 * From resume we call sfmmu_setctx_sec with interrupts disabled.
	 * But we can also get called from C with interrupts enabled. So,
	 * I need to check first. Also, resume saves state in %o5 and I
	 * can't use this register here.
	 */
	rdpr	%pstate, %g1
	/*
	 * If interrupts are not disabled, then disable it
	 */
	btst	PSTATE_IE, %g1
	bz,pt   %icc, 2f
	mov	MMU_SCONTEXT, %o1
	
	wrpr	%g1, PSTATE_IE, %pstate		/* disable interrupts */
2:	
	sethi	%hi(FLUSH_ADDR), %o4
	
	stxa	%o0, [%o1]ASI_DMMU		/* set 2nd context reg. */

	/* if kernel & invalid_context, skip */
	cmp	%o0, INVALID_CONTEXT
	ble,pn	%icc, 1f
	flush	%o4

	/* we need to set the tsb hardware register */
	CTXNUM_TO_TSBINFO(%o0, %o1, %o2)
	LOAD_TSBINFO(%o1, %o2, %o3, %o4)
1:
	btst	PSTATE_IE, %g1
	bz,a,pt %icc, 3f
	nop
	wrpr	%g0, %g1, %pstate		/* enable interrupts */
3:		
	retl
	nop
	SET_SIZE(sfmmu_setctx_sec)

	ENTRY_NP(sfmmu_get_dsfar)
	set	MMU_SFAR, %o0
	retl
	ldxa	[%o0]ASI_DMMU, %o0
	SET_SIZE(sfmmu_get_dsfar)

	ENTRY_NP(sfmmu_get_isfsr)
	set	MMU_SFSR, %o0
	retl
	ldxa	[%o0]ASI_IMMU, %o0
	SET_SIZE(sfmmu_get_isfsr)

	ENTRY_NP(sfmmu_get_dsfsr)
	set	MMU_SFSR, %o0
	retl
	ldxa	[%o0]ASI_DMMU, %o0
	SET_SIZE(sfmmu_get_dsfsr)

	ENTRY_NP(sfmmu_get_itsb)
	set	MMU_TSB, %o0
	retl
	ldxa	[%o0]ASI_IMMU, %o0
	SET_SIZE(sfmmu_get_itsb)

	ENTRY_NP(sfmmu_get_dtsb)
	set	MMU_TSB, %o0
	retl
	ldxa	[%o0]ASI_DMMU, %o0
	SET_SIZE(sfmmu_get_dtsb)

#endif /* lint */

/*
 * Other sfmmu primitives
 */


#if defined (lint)
/* ARGSUSED */
void
sfmmu_make_tsbreg(uint64_t *tsbreg, caddr_t vaddr, int split, int size)
{
}

void
sfmmu_patch_ktsb(void)
{
}

void
sfmmu_patch_utsb(void)
{
}
	
/* ARGSUSED */
void
sfmmu_update_hword(u_short *ptr, u_short new)
{
}
	
/* ARGSUSED */
void
sfmmu_load_tsbstate(int ctx)
{
}

/* ARGSUSED */
void
sfmmu_load_tsbstate_tl1(uint64_t ctx, uint64_t tsbinfop)
{
}

/* ARGSUSED */
void
sfmmu_set_itsb(pfn_t tsb_bspg, uint split, uint size)
{
}

/* ARGSUSED */
void
sfmmu_set_dtsb(pfn_t tsb_bspg, uint split, uint size)
{
}

/* ARGSUSED */
void
sfmmu_load_tsb(caddr_t addr, int ctxnum, tte_t *ttep)
{
}

/* ARGSUSED */
void
sfmmu_load_tsb4m(caddr_t addr, int ctxnum, tte_t *ttep)
{
}

/* ARGSUSED */
void
sfmmu_unload_tsb(caddr_t addr, int ctxnum)
{
}

/* ARGSUSED */
void
sfmmu_unload_tsb4m(caddr_t addr, int ctxnum)
{
}

/* ARGSUSED */
void
sfmmu_unload_tsbctx(uint ctx)
{
}

/* ARGSUSED */
void
sfmmu_migrate_tsbctx(uint ctx, uint64_t *src_tsb, uint64_t *dest_tsb)
{
}

#else /* lint */
#define	I_SIZE		4
		
	ENTRY_NP(sfmmu_fix_ktlb_traptable)
	/*
	 * %o0 = start of patch area
	 *
	 * NOTE: This code assumes that the tsb's will always live
	 * 	 in the bottom 32 bits of kernel's address space.
	 * 	 Even for LP64 kernel.
	 */
	/* fix sethi(ktsb_base) */
	sethi	%hi(ktsb_base), %o1
	ldn	[%o1 + %lo(ktsb_base)], %o1
	srl	%o1, 10, %o2			/* get upper 22 bits */
	ld	[%o0], %o3			/* get sethi(ktsb_base) */
	or	%o3, %o2, %o3			/* create new sethi insn */
	st	%o3, [%o0]			/* write sethi(ktsb_base) */
	flush	%o0
	/* fix sll */
	add	%o0, I_SIZE, %o0		/* goto next instr. */
	sethi	%hi(ktsb_szcode), %o1
	ld	[%o1 + %lo(ktsb_szcode)], %o1	/* get ktsb_sz */
	add	%o1, TSB_START_SIZE, %o1	/* get number of bits */
	ld	[%o0], %o3			/* get sll */
	sub	%o3, %o1, %o3
	st	%o3, [%o0]			/* write sll */
	flush	%o0
	/* fix srl */
	add	%o0, I_SIZE, %o0		/* goto next instr. */
	ld	[%o0], %o3			/* get srl */
	sub	%o3, %o1, %o3
	st	%o3, [%o0]			/* write sll */
	retl
	flush	%o0
	SET_SIZE(sfmmu_fix_ktlb_traptable)


	ENTRY_NP(sfmmu_patch_ktsb)
	/*
	 * We need to fix tt0_iktsb, tt0_dktsb, tt1_iktsb & tt1_dktsb.
	 * NOTE: Any changes here also requires changes to trap_table.s.
	 */
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(tt0_iktsb), %o0
	call	sfmmu_fix_ktlb_traptable
	or	%o0, %lo(tt0_iktsb), %o0
	sethi	%hi(tt0_dktsb), %o0
	call	sfmmu_fix_ktlb_traptable
	or	%o0, %lo(tt0_dktsb), %o0
	sethi	%hi(tt1_iktsb), %o0
	call	sfmmu_fix_ktlb_traptable
	or	%o0, %lo(tt1_iktsb), %o0
	sethi	%hi(tt1_dktsb), %o0
	call	sfmmu_fix_ktlb_traptable
	or	%o0, %lo(tt1_dktsb), %o0
	ret
	restore
	SET_SIZE(sfmmu_patch_ktsb)

	ENTRY_NP(sfmmu_patch_utsb)
	/*
	 * NOTE: This code assumes that the tsb's will always live
	 * 	 in the bottom 32 bits of kernel's address space.
	 * 	 Even for LP64 kernel.
	 *
	 * NOTE2: DEBUG and TRAPTRACE kernels automatically
	 *	check if we are supporting 4M tte's in
	 *	the user tsb. So this code only patchs
	 *	the trap handler for non-debug kernels.
	 */
#if defined(TRAPTRACE) || defined(DEBUG)
	retl
	  nop
#endif
	/*
	 * Fix branch target for 8k utsb miss handler.
	 * We need to change it from sfmmu_udtlb_miss to
	 * sfmmu_utsb_miss. 
	 * We need to fix tt0_dutsb and tt1_dutsb.
	 * Its a BPcc type instruction with sign_ext(disp19)
	 * encoded in the bottom 19 bits.
	 * NOTE: Any changes here also requires changes to trap_table.s.
	 */
	sub	%g0, 1, %o4			/* o4 = -1 */
	srl	%o4, (32 - 19), %o4		/* o4 = disp19 mask */

	sethi	%hi(sfmmu_utsb_miss), %o0
	or	%o0, %lo(sfmmu_utsb_miss), %o0

	sethi	%hi(tt0_dutsb), %o1
	or	%o1, %lo(tt0_dutsb), %o1
	sub	%o0, %o1, %o2			/* o2 = displacement */
	srl	%o2, 2, %o2			/* divide disp by 4 */
	and	%o2, %o4, %o2			/* o2 = disp19 for new label */
	ld	[%o1], %o3			/* fetch branch instruction */
	andn	%o3, %o4, %o3			/* mask off disp19 portion */
	or	%o3, %o2, %o3			/* create new branch insn */
	st	%o3, [%o1]			/* write new insn */
	flush	%o1

	sethi	%hi(tt1_dutsb), %o1
	or	%o1, %lo(tt1_dutsb), %o1
	sub	%o0, %o1, %o2			/* o0 = displacement */
	srl	%o2, 2, %o2			/* divide disp by 4 */
	and	%o2, %o4, %o2			/* o2 = disp19 for new label */
	ld	[%o1], %o3			/* fetch branch instruction */
	andn	%o3, %o4, %o3			/* mask off disp19 portion */
	or	%o3, %o2, %o3			/* create new branch insn */
	st	%o3, [%o1]			/* write new insn */
	retl
	  flush	%o1
	SET_SIZE(sfmmu_patch_utsb)
	
	/*
	 * %o0 - pointer to a uint64_t result
	 * %o1 - base page
	 * %o2 - split code
	 * %o3 - size code
	 */
	ENTRY_NP(sfmmu_make_tsbreg)
	sll	%o2, TSBSPLIT_SHIFT, %o5
	or	%o1, %o5, %o1
	or	%o1, %o3, %o2
	retl
	stx     %o2, [%o0]	
	SET_SIZE(sfmmu_make_tsbreg)

	ENTRY_NP(sfmmu_update_hword)
	/*
	 * %o0 = u_short *
	 * %o1 = new value
	 */
	xor	%o0, 2, %o4			/* find 2nd half */
	lduh	[%o0], %o2			/* load 1st half */
1:
	lduh	[%o4], %o3			/* load 2nd half */
	btst	2, %o0
	bnz,a,pn %icc, 2f
	  sll	%o3, 16, %o3			/* move 2nd half upper */
	
	/* higher halfword */
	sll	%o1, 16, %o5			/* move new value upper */
	sll	%o2, 16, %o2			/* move 1st half upper */
	or	%o5, %o3, %o5			/* make new word */
	or	%o2, %o3, %o2			/* make old word */
	cas	[%o0], %o2, %o5			/* change */
	cmp	%o2, %o5
	bne,a,pn %icc, 1b
	  lduh	[%o0], %o2			/* get 1st half again */
	retl
	  membar	#StoreLoad|#StoreStore
2:
	/* lower halfword */
	or	%o1, %o3, %o5			/* make new word */
	or	%o2, %o3, %o2			/* make old word */
	cas	[%o4], %o2, %o5			/* change */
	cmp	%o2, %o5
	bne,a,pn %icc, 1b
	  lduh	[%o0], %o2			/* get 1st half again */
	retl
	  membar	#StoreLoad|#StoreStore
	SET_SIZE(sfmmu_update_hword)
	
	ENTRY_NP(sfmmu_load_tsbstate)
	/*
	 * %o0 = ctx number
	 */
	rdpr	%pstate, %o5
#ifdef DEBUG
	andcc	%o5, PSTATE_IE, %g0		/* if interrupts already */
	bnz,a,pt %icc, 1f			/* disabled, panic	 */
	  nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	or	%o0, %lo(sfmmu_panic1), %o0
1:	
#ifdef	__sparcv9
	andcc	%o5, PSTATE_AM, %g0		/* if PSTATE_AM set panic */
	bz,pt	%icc, 2f
	  nop
	sethi	%hi(sfmmu_panic2), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic2), %o0
2:
#endif	/* __sparcv9 */
#endif	/* DEBUG */
	wrpr	%o5, PSTATE_IE, %pstate		/* disable interrupts */
	/* skip if kernel context */
	cmp	%o0, KCONTEXT
	be,pn	%xcc, 1f
	  nop

	/*
	 * We need to set the tsb hardware register
	 */
	CTXNUM_TO_TSBINFO(%o0, %o1, %o2)
	LOAD_TSBINFO(%o1, %o2, %o3, %o4)
1:
	retl
	wrpr	%g0, %o5, %pstate		/* enable interrupts */
	SET_SIZE(sfmmu_load_tsbstate)

	ENTRY_NP(sfmmu_load_tsbstate_tl1)
	/*
	 * %g1 = ctx number
	 * %g2 = tsbinfo *
	 */
	set	MMU_SCONTEXT, %g3
	ldxa	[%g3]ASI_DMMU, %g5		/* rd sec ctxnum */
	cmp	%g5, %g1
	bne,a,pn %icc, 0f
	  nop
	/* skip if kernel context */
	cmp	%g1, KCONTEXT
	be,pn	%xcc, 0f
	  nop

	LOAD_TSBINFO(%g2, %g3, %g4, %g5)
0:		
	retry
	nop
	SET_SIZE(sfmmu_load_tsbstate_tl1)
	

	ENTRY_NP(sfmmu_set_itsb)
	sllx	%o0, TSBBASE_SHIFT, %o0	
	sll	%o1, TSBSPLIT_SHIFT, %o1
	or	%o0, %o1, %g1
	or	%g1, %o2, %g1
	set	MMU_TSB, %o3
	stxa    %g1, [%o3]ASI_IMMU
	sethi	%hi(FLUSH_ADDR), %o1		/* flush addr doesn't matter */
	retl
	  flush	%o1				/* flush required by immu */
	SET_SIZE(sfmmu_set_itsb)

	ENTRY_NP(sfmmu_set_dtsb)
	sllx	%o0, TSBBASE_SHIFT, %o0	
	sll	%o1, TSBSPLIT_SHIFT, %o1
	or	%o0, %o1, %g1
	or	%g1, %o2, %g1
	set	MMU_TSB, %o3
	stxa    %g1, [%o3]ASI_DMMU
	retl
	membar	#Sync
	SET_SIZE(sfmmu_set_dtsb)

	.seg	".data"
load_tsb_panic2:
	.ascii	"sfmmu_load_tsb: ctxnum is INVALID_CONTEXT"
	.byte	0

	.align	4
	.seg	".text"

/*
 * Routine that loads an entry into a tsb using virtual addresses.
 * Locking is required since all cpus can use the same TSB.
 */
	ENTRY_NP(sfmmu_load_tsb)
	/*
	 * %o0 = addr
	 * %o1 = ctxnum
	 * %o2 = ttep
	 */
	rdpr	%pstate, %o5
#ifdef DEBUG
	andcc	%o5, PSTATE_IE, %g0		/* if interrupts already */
	bnz,pt 	%icc, 1f			/* disabled, panic	 */
	  nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic1), %o0
1:
#ifdef	__sparcv9
	andcc	%o5, PSTATE_AM, %g0		/* if PSTATE_AM set panic */
	bz,pt	%icc, 2f
	  nop
	sethi	%hi(sfmmu_panic2), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic2), %o0
2:
#endif	/* __sparcv9 */
	cmp	%o1, INVALID_CONTEXT
	bne,pt	%icc, 3f
	  nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(load_tsb_panic2), %o0
	call	panic
	 or	%o0, %lo(load_tsb_panic2), %o0
3:
#endif /* DEBUG */
	/*
	 * get tsbreg from context 
	 */
	wrpr	%o5, PSTATE_IE, %pstate		/* disable interrupts */
	
	CTXNUM_TO_TSBREG(%o1, %o3, %o4)
	or	%o0, %o1, %o4			/* build tagaccess reg */
	GET_TSB_POINTER8K(%o3, %o4, %g1, %g2, %g3, %g4) /* g1 = tsbp */
	BUILD_TSB_TAG(%o0, %o1, %g2, %g3)	/* g2 = tag target */

	TSB_UPDATE(%g1, %o2, %g2, %o0, %o1, 1)

	wrpr	%g0, %o5, %pstate		/* enable interrupts */
	
	retl
	membar	#StoreStore|#StoreLoad

	SET_SIZE(sfmmu_load_tsb)

/*
 * Routine that loads a 4M entry into a tsb using virtual addresses.
 * Locking is required since all cpus can use the same TSB.
 */
	ENTRY_NP(sfmmu_load_tsb4m)
	/*
	 * %o0 = addr
	 * %o1 = ctxnum
	 * %o2 = ttep
	 */
	rdpr	%pstate, %o5
#ifdef DEBUG
	andcc	%o5, PSTATE_IE, %g0		/* if interrupts already */
	bnz,pt 	%icc, 1f			/* disabled, panic	 */
	  nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic1), %o0
1:
#ifdef	__sparcv9
	andcc	%o5, PSTATE_AM, %g0		/* if PSTATE_AM set panic */
	bz,pt	%icc, 2f
	  nop
	sethi	%hi(sfmmu_panic2), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic2), %o0
2:
#endif	/* __sparcv9 */
	cmp	%o1, INVALID_CONTEXT
	bne,pt	%icc, 3f
	  nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(load_tsb_panic2), %o0
	call	panic
	 or	%o0, %lo(load_tsb_panic2), %o0
3:
#endif /* DEBUG */
	/*
	 * Check if 4M support has been disabled.
	 */
	set	utsb_4m_disable, %o4
	ld	[%o4], %o4
	brz,pt	%o4, 4f
	  nop
	retl
	  nop
4:
	/*
	 * get tsbreg from context 
	 */
	wrpr	%o5, PSTATE_IE, %pstate		/* disable interrupts */
	
	CTXNUM_TO_TSBREG(%o1, %o3, %o4)
	or	%o0, %o1, %o4			/* build tagaccess reg */
	GET_TSB_POINTER4M(%o3, %o4, %g1, %g2, %g3, %g4) /* g1 = tsbp */
	BUILD_TSB_TAG(%o0, %o1, %g2, %g3)	/* g2 = tag target */

	TSB_UPDATE(%g1, %o2, %g2, %o0, %o1, 1)

	wrpr	%g0, %o5, %pstate		/* enable interrupts */

	retl
	membar	#StoreStore|#StoreLoad

	SET_SIZE(sfmmu_load_tsb4m)

/*
 * Flush TSB for a given vaddr and ctx pair.
 */ 

	ENTRY(sfmmu_unload_tsb)
	/*
	 * %o0 = vaddr to be flushed
	 * %o1 = ctx to be flushed
	 */

	/*
	 * Check for 8K entry.
	 */
	CTXNUM_TO_TSBREG(%o1, %o2, %o3)		/* o2 = tsbreg */
	or	%o0, %o1, %g2			/* g2 = tagaccess */
	GET_TSB_POINTER8K(%o2, %g2, %g1, %o3, %o4, %o5)	/* g1 = tsbp */
	BUILD_TSB_TAG(%o0, %o1, %o3, %o4)	/* o3 = target tag */
	TSB_INVALIDATE8K(%g1, %o3, %o2, %o4, %o5, tsbinv_label1)
	retl
	membar	#StoreStore|#StoreLoad
	SET_SIZE(sfmmu_unload_tsb)

/*
 * Flush TSB for a given vaddr and ctx pair.
 */ 

	ENTRY(sfmmu_unload_tsb4m)
	/*
	 * %o0 = vaddr to be flushed
	 * %o1 = ctx to be flushed
	 */

	/*
	 * Check if 4M support has been disabled.
	 */
	set	utsb_4m_disable, %o2
	ld	[%o2], %o2
	brz,pt	%o2, 1f
	  nop
	retl
	  nop
1:

	/*
	 * Check for 8K entry.
	 */
	CTXNUM_TO_TSBREG(%o1, %o2, %o3)		/* o2 = tsbreg */
	or	%o0, %o1, %g2			/* g2 = tagaccess */
	GET_TSB_POINTER4M(%o2, %g2, %g1, %o3, %o4, %o5)	/* g1 = tsbp */
	BUILD_TSB_TAG(%o0, %o1, %o3, %o4)	/* o3 = target tag */
	TSB_INVALIDATE4M(%g1, %o3, %o2, %o4, %o5, tsbinv4m_label1)
	retl
	membar	#StoreStore|#StoreLoad
	SET_SIZE(sfmmu_unload_tsb4m)

/*
 * Flush this cpus's TSB so that all entries with ctx = ctx-passed are flushed.
 * Assumes it is called with preemption disabled
 */ 
	.seg	".data"
unload_tsbctx_panic:
	.ascii	"sfmmu_unload_tsbctx: preemption enabled"
	.byte	0

	.align	4
	.seg	".text"


	ENTRY(sfmmu_unload_tsbctx)
	/*
	 * %o0 = ctx to be flushed
	 *
	 */
#ifdef DEBUG
	ldsb	[THREAD_REG + T_PREEMPT], %o1
	brnz,pt	%o1, 1f
	 nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(unload_tsbctx_panic), %o0
	call	panic
	 or	%o0, %lo(unload_tsbctx_panic), %o0
	ret
	restore
1:
#endif /* DEBUG */
	/*
	 * Get tsbreg from context
	 */
	CTXNUM_TO_TSBREG(%o0, %o1, %o2)
	FLUSH_TSBCTX(%o1, %o0, %o2, %o3, %o4)
	retl
	membar	#StoreStore|#StoreLoad
	SET_SIZE(sfmmu_unload_tsbctx)

	ENTRY(sfmmu_migrate_tsbctx)
	/*
	 * %o0 = ctx to be migrated
	 * %o1 = ptr to tsbregister to copy from
	 * %o2 = ptr to tsbregister to copy to
	 */

	ldx	[%o1], %o1			/* load src tsbreg */
	ldx	[%o2], %o2			/* load dest tsbreg */

	/* 
	 * The idea of TSB migration was to preserve the existing ttes in
	 * the current src TSB so that a process doesn't have to pay a
	 * performance penalty when its TSB is resized. However, I don't 
	 * really know how to do that just yet in a fast and correct fashion.
	 * For now, invalidate the src TSB and don't copy.
	 */
	FLUSH_TSBCTX(%o1, %o0, %o2, %o3, %o4)
	retl
	membar	#StoreStore|#StoreLoad
	SET_SIZE(sfmmu_migrate_tsbctx)

#endif /* lint */

#if defined (lint)
/* ARGSUSED */
pfn_t sfmmu_ttetopfn(tte_t *tte, caddr_t vaddr)
{ return(0); }

#else /* lint */
#define	TTETOPFN(tte, vaddr, label)					\
	srlx	tte, TTE_SZ_SHFT, %g2					;\
	sllx	tte, TTE_PA_LSHIFT, tte					;\
	andcc	%g2, TTE_SZ_BITS, %g2		/* g2 = ttesz */	;\
	sllx	%g2, 1, %g3						;\
	add	%g3, %g2, %g3			/* mulx 3 */		;\
	add	%g3, MMU_PAGESHIFT + TTE_PA_LSHIFT, %g4			;\
	srlx	tte, %g4, tte						;\
	sllx	tte, %g3, tte						;\
	bz,pt	%xcc, label/**/1					;\
	  nop								;\
	set	1, %g2							;\
	add	%g3, MMU_PAGESHIFT, %g4					;\
	sllx	%g2, %g4, %g2						;\
	sub	%g2, 1, %g2		/* g2=TTE_PAGE_OFFSET(ttesz) */	;\
	and	vaddr, %g2, %g3						;\
	srln	%g3, MMU_PAGESHIFT, %g3					;\
	or	tte, %g3, tte						;\
label/**/1:


	ENTRY_NP(sfmmu_ttetopfn)
	ldx	[%o0], %g1			/* read tte */
	TTETOPFN(%g1, %o1, sfmmu_ttetopfn_l1)
	/*
	 * g1 = pfn
	 */
	retl
	mov	%g1, %o0
	SET_SIZE(sfmmu_ttetopfn)

#endif /* !lint */


#if defined (lint)
/*
 * The sfmmu_hblk_hash_add is the assembly primitive for adding hmeblks to the
 * the hash list.
 */
/* ARGSUSED */
void
sfmmu_hblk_hash_add(struct hmehash_bucket *hmebp, struct hme_blk *hmeblkp,
	uint64_t hblkpa)
{
}

/*
 * The sfmmu_hblk_hash_rm is the assembly primitive to remove hmeblks from the
 * hash list.
 */
/* ARGSUSED */
void
sfmmu_hblk_hash_rm(struct hmehash_bucket *hmebp, struct hme_blk *hmeblkp,
	uint64_t hblkpa, struct hme_blk *prev_hblkp)
{
}
#else /* lint */

/*
 * Functions to grab/release hme bucket list lock.  I only use a byte
 * instead of the whole int because eventually we might want to
 * put some counters on the other bytes (of course, these routines would
 * have to change).  The code that grab this lock should execute
 * with interrupts disabled and hold the lock for the least amount of time
 * possible.
 */
#define	HMELOCK_ENTER(hmebp, tmp1, tmp2, label1)		\
	mov	0xFF, tmp2					;\
	add	hmebp, HMEBUCK_LOCK, tmp1			;\
label1:								;\
	casa	[tmp1] ASI_N, %g0, tmp2				;\
	brnz,pn	tmp2, label1					;\
	 mov	0xFF, tmp2					;\
	membar	#StoreLoad|#StoreStore

#define	HMELOCK_EXIT(hmebp)					\
	membar	#StoreLoad|#StoreStore				;\
	st	%g0, [hmebp + HMEBUCK_LOCK]

	.seg	".data"
hblk_add_panic1:
	.ascii	"sfmmu_hblk_hash_add: interrupts disabled"
	.byte	0
hblk_add_panic2:
	.ascii	"sfmmu_hblk_hash_add: va hmeblkp is NULL but pa is not"
	.byte	0
	.align	4
	.seg	".text"

	ENTRY_NP(sfmmu_hblk_hash_add)
	/*
	 * %o0 = hmebp
	 * %o1 = hmeblkp
	 * %o2 = hblkpa	(LP64)
	 *    -OR-
	 * %o2 & %o3 = hblkpa (ILP32)
	 */
	rdpr	%pstate, %o5
#ifdef DEBUG
	andcc	%o5, PSTATE_IE, %g0		/* if interrupts already */
	bnz,pt %icc, 3f				/* disabled, panic	 */
	  nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(hblk_add_panic1), %o0
	call	panic
	 or	%o0, %lo(hblk_add_panic1), %o0
	ret
	restore

3:
#endif /* DEBUG */
	wrpr	%o5, PSTATE_IE, %pstate		/* disable interrupts */
#ifdef __sparcv9
	mov	%o2, %g1
#else
	sllx	%o2, 32, %g1
	or	%g1, %o3, %g1			/* g1 = hblkpa */
#endif

	/*
	 * g1 = hblkpa
	 */
	ldn	[%o0 + HMEBUCK_HBLK], %o4	/* next hmeblk */
	ldx	[%o0 + HMEBUCK_NEXTPA], %g2	/* g2 = next hblkpa */
#ifdef	DEBUG
	cmp	%o4, %g0
	bne,pt %xcc, 1f
	 nop
	brz,pt %g2, 1f
	 nop
	wrpr	%g0, %o5, %pstate		/* enable interrupts */
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(hblk_add_panic2), %o0
	call	panic
	  or	%o0, %lo(hblk_add_panic2), %o0
	ret
	restore
1:
#endif /* DEBUG */
	/*
	 * We update hmeblks entries before grabbing lock because the stores
	 * could take a tlb miss and require the hash lock.  The buckets
	 * are part of the nucleus so we are cool with those stores.
	 */
	stn	%o4, [%o1 + HMEBLK_NEXT]	/* update hmeblk's next */
	stx	%g2, [%o1 + HMEBLK_NEXTPA]	/* update hmeblk's next pa */
	HMELOCK_ENTER(%o0, %o2 ,%o3, hashadd1)
	stn	%o1, [%o0 + HMEBUCK_HBLK]	/* update bucket hblk next */
	stx	%g1, [%o0 + HMEBUCK_NEXTPA]	/* add hmeblk to list */
	HMELOCK_EXIT(%o0)
	retl
	  wrpr	%g0, %o5, %pstate		/* enable interrupts */
	SET_SIZE(sfmmu_hblk_hash_add)

	ENTRY_NP(sfmmu_hblk_hash_rm)
	/*
	 * This function removes an hmeblk from the hash chain. 
	 * It is written to guarantee we don't take a tlb miss
	 * by using physical addresses to update the list.
	 * 
	 * %o0 = hmebp
	 * %o1 = hmeblkp
	 * %o2 = hmeblkp previous pa	(LP64)
	 * %o3 = hmeblkp previous	(LP64)
	 * 	-OR-
	 * %o2 & %o3 = hmeblkp previous pa	(ILP32)
	 * %o4 = hmeblkp previous		(ILP32)
	 */

#ifdef	__sparcv9
	mov	%o3, %o4			/* o4 = hmeblkp previous */
#else	/* ! __sparcv9 */
	/*
	 * Clear upper 32 bits.
	 */
	srl	%o0, 0, %o0
	srl	%o1, 0, %o1
	srl	%o2, 0, %o2
	srl	%o3, 0, %o3
	srl	%o4, 0, %o4
#endif	/* __sparcv9 */

	rdpr	%pstate, %o5
#ifdef DEBUG
	andcc	%o5, PSTATE_IE, %g0		/* if interrupts already */
	bnz,pt 	%icc, 3f			/* disabled, panic	 */
	  nop
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic1), %o0
3:
#ifdef	__sparcv9
	andcc	%o5, PSTATE_AM, %g0		/* If PSTATE_AM set panic */
	bz,pt	%icc, 4f
	 nop
	sethi	%hi(sfmmu_panic2), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic2), %o0
4:
#endif	/* __sparcv9 */
#endif /* DEBUG */
	/*
	 * disable interrupts, clear Address Mask to access 64 bit physaddr
	 */
	andn    %o5, PSTATE_IE | PSTATE_AM, %g1
	wrpr    %g1, 0, %pstate

	HMELOCK_ENTER(%o0, %g1, %g3 , hashrm1)
	ldn	[%o0 + HMEBUCK_HBLK], %g2	/* first hmeblk in list */
	cmp	%g2, %o1
	bne,pt	%ncc,1f
	 mov	ASI_MEM, %asi
	/*
	 * hmeblk is first on list
	 */
	ldx	[%o0 + HMEBUCK_NEXTPA], %g2	/* g2 = hmeblk pa */
	ldna	[%g2 + HMEBLK_NEXT] %asi, %o3	/* read next hmeblk va */
	ldxa	[%g2 + HMEBLK_NEXTPA] %asi, %g1	/* read next hmeblk pa */
	stn	%o3, [%o0 + HMEBUCK_HBLK]	/* write va */
	ba,pt	%xcc, 2f
	  stx	%g1, [%o0 + HMEBUCK_NEXTPA]	/* write pa */
1:
	/* hmeblk is not first on list */
	sethi   %hi(dcache_line_mask), %g4
	ld	[%g4 + %lo(dcache_line_mask)], %g4

#ifdef	__sparcv9
	mov	%o2, %g3
#else
	sllx	%o2, 32, %g3
	or	%g3, %o3, %g3			/* g3 = prev hblk pa */
#endif
	GET_CPU_IMPL(%g2)
	cmp %g2, CHEETAH_IMPL
	bge %icc, hblk_hash_rm_1
	and	%o4, %g4, %g2
	stxa	%g0, [%g2] ASI_DC_TAG		/* flush prev pa from dcache */
	add	%o4, HMEBLK_NEXT, %o4
	and	%o4, %g4, %g2
	ba	hblk_hash_rm_2
	stxa	%g0, [%g2] ASI_DC_TAG		/* flush prev va from dcache */
hblk_hash_rm_1:

        stxa    %g0, [%g3] ASI_DC_INVAL         /* flush prev pa from dcache */
        add     %g3, HMEBLK_NEXT, %g2
        stxa    %g0, [%g2] ASI_DC_INVAL         /* flush prev va from dcache */
hblk_hash_rm_2:

	membar	#Sync
	ldxa	[%g3 + HMEBLK_NEXTPA] %asi, %g2	/* g2 = hmeblk pa */ 
	ldna	[%g2 + HMEBLK_NEXT] %asi, %o3	/* read next hmeblk va */
	ldxa	[%g2 + HMEBLK_NEXTPA] %asi, %g1	/* read next hmeblk pa */
	stna	%o3, [%g3 + HMEBLK_NEXT] %asi	/* write va */
	stxa	%g1, [%g3 + HMEBLK_NEXTPA] %asi	/* write pa */
2:
	HMELOCK_EXIT(%o0)
	retl
	  wrpr	%g0, %o5, %pstate		/* enable interrupts */
	SET_SIZE(sfmmu_hblk_hash_rm)

#endif /* lint */

/*
 * This macro is used to update any of the global sfmmu kstats
 * in perf critical paths.
 * It is only enabled in debug kernels or if SFMMU_STAT_GATHER is defined
 */
#if defined(DEBUG) || defined(SFMMU_STAT_GATHER)
#define	HAT_GLOBAL_DBSTAT(statname, tmp1, tmp2)				\
	sethi	%hi(sfmmu_global_stat), tmp1				;\
	add	tmp1, statname, tmp1					;\
	ld	[tmp1 + %lo(sfmmu_global_stat)], tmp2			;\
	inc	tmp2							;\
	st	tmp2, [tmp1 + %lo(sfmmu_global_stat)]

#else /* DEBUG || SFMMU_STAT_GATHER */

#define	HAT_GLOBAL_DBSTAT(statname, tmp1, tmp2)

#endif  /* DEBUG || SFMMU_STAT_GATHER */

/*
 * This macro is used to update global sfmmu kstas in non
 * perf critical areas so they are enabled all the time
 */
#define	HAT_GLOBAL_STAT(statname, tmp1, tmp2)				\
	sethi	%hi(sfmmu_global_stat), tmp1				;\
	add	tmp1, statname, tmp1					;\
	ld	[tmp1 + %lo(sfmmu_global_stat)], tmp2			;\
	inc	tmp2							;\
	st	tmp2, [tmp1 + %lo(sfmmu_global_stat)]

/*
 * These macros are used to update per cpu stats in non perf
 * critical areas so they are enabled all the time
 */
#define	HAT_PERCPU_STAT32(tsbarea, stat, tmp1)				\
	ld	[tsbarea + stat], tmp1					;\
	inc	tmp1							;\
	st	tmp1, [tsbarea + stat]

/*
 * These macros are used to update per cpu stats in non perf
 * critical areas so they are enabled all the time
 */
#define	HAT_PERCPU_STAT16(tsbarea, stat, tmp1)				\
	lduh	[tsbarea + stat], tmp1					;\
	inc	tmp1							;\
	stuh	tmp1, [tsbarea + stat]

#if defined (lint)
/*
 * The following routines are jumped to from the mmu trap handlers to do
 * the setting up to call systrap.  They are separate routines instead of 
 * being part of the handlers because the handlers would exceed 32
 * instructions and since this is part of the slow path the jump
 * cost is irrelevant.
 */
void
sfmmu_pagefault()
{
}

void
sfmmu_mmu_trap()
{
}

void
sfmmu_window_trap()
{
}

#else /* lint */

#ifdef	PTL1_PANIC_DEBUG
	.seg	".data"
	.global	test_ptl1_panic
test_ptl1_panic:
	.word	0
	.align	8

	.seg	".text"
	.align	4
#endif	/* PTL1_PANIC_DEBUG */

#define	USE_ALTERNATE_GLOBALS						\
	rdpr	%pstate, %g5						;\
	wrpr	%g5, PSTATE_MG | PSTATE_AG, %pstate

	ENTRY_NP(sfmmu_pagefault)
	USE_ALTERNATE_GLOBALS
	mov	MMU_TAG_ACCESS, %g4
	rdpr	%tt, %g6
	ldxa	[%g4] ASI_DMMU, %g5
	ldxa	[%g4] ASI_IMMU, %g2
	cmp	%g6, FAST_IMMU_MISS_TT
	be,a,pn	%icc, 1f
	  mov	T_INSTR_MMU_MISS, %g3
	mov	%g5, %g2
	cmp	%g6, FAST_DMMU_MISS_TT
	move	%icc, T_DATA_MMU_MISS, %g3	/* arg2 = traptype */
	movne	%icc, T_DATA_PROT, %g3		/* arg2 = traptype */

	/*
	 * If ctx indicates large ttes then let sfmmu_tsb_miss 
	 * perform hash search before faulting.
	 * g2 = tag access
	 * g3 = trap type
	 */
#ifdef  PTL1_PANIC_DEBUG
	/* check if we want to test the tl1 panic */
	sethi	%hi(test_ptl1_panic), %g4
	ld	[%g4 + %lo(test_ptl1_panic)], %g1
	st	%g0, [%g4 + %lo(test_ptl1_panic)]
	cmp	%g1, %g0
	bne,a,pn %icc, ptl1_panic
	  or	%g0, PTL1_BAD_WTRAP, %g1
#endif	/* PTL1_PANIC_DEBUG */
1:
	sll	%g2, TAGACC_CTX_SHIFT, %g4
	srl	%g4, TAGACC_CTX_SHIFT, %g4
	sll	%g4, CTX_SZ_SHIFT, %g4
	sethi	%hi(ctxs), %g6
	ldn	[%g6 + %lo(ctxs)], %g1
	add	%g4, %g1, %g1
	lduh	[%g1 + C_FLAGS], %g7
	and	%g7, LTTES_FLAG, %g7
	brz,pt	%g7, 1f
	nop
	sethi	%hi(sfmmu_tsb_miss), %g1
	or	%g1, %lo(sfmmu_tsb_miss), %g1
	ba,pt	%xcc, 2f
	  nop
1:
	HAT_GLOBAL_STAT(HATSTAT_PAGEFAULT, %g6, %g4)
	/*
	 * g2 = tag access reg
	 * g3.l = type
	 * g3.h = 0
	 */
	sethi	%hi(trap), %g1
	or	%g1, %lo(trap), %g1
2:
	ba,pt	%xcc, sys_trap
	  mov	-1, %g4	
	SET_SIZE(sfmmu_pagefault)

	ENTRY_NP(sfmmu_mmu_trap)
	USE_ALTERNATE_GLOBALS
	mov	MMU_TAG_ACCESS, %g4
	rdpr	%tt, %g6
	ldxa	[%g4] ASI_DMMU, %g5
	ldxa	[%g4] ASI_IMMU, %g2
	cmp	%g6, FAST_IMMU_MISS_TT
	be,a,pn	%icc, 1f
	  mov	T_INSTR_MMU_MISS, %g3
	mov	%g5, %g2
	cmp	%g6, FAST_DMMU_MISS_TT
	move	%icc, T_DATA_MMU_MISS, %g3	/* arg2 = traptype */
	movne	%icc, T_DATA_PROT, %g3		/* arg2 = traptype */
1:
	/*
	 * g2 = tag access reg
	 * g3 = type
	 */
	sethi	%hi(sfmmu_tsb_miss), %g1
	or	%g1, %lo(sfmmu_tsb_miss), %g1
	ba,pt	%xcc, sys_trap
	  mov	-1, %g4	
	SET_SIZE(sfmmu_mmu_trap)

	ENTRY_NP(sfmmu_window_trap)
	/* user miss at tl>1. better be the window handler */
	rdpr	%tl, %g5
	sub	%g5, 1, %g3
	wrpr	%g3, %tl
	rdpr	%tt, %g2
	wrpr	%g5, %tl
	and	%g2, WTRAP_TTMASK, %g4
	cmp	%g4, WTRAP_TYPE	
	bne,pn	%xcc, 1f
	 nop
	rdpr	%tpc, %g1
	/* tpc should be in the trap table */
	set	trap_table, %g4
	cmp	%g1, %g4
	blt,pn %xcc, 1f
	 .empty
	set	etrap_table, %g4
	cmp	%g1, %g4
	bge,pn %xcc, 1f
	 .empty
	andn	%g1, WTRAP_ALIGN, %g1	/* 128 byte aligned */
	add	%g1, WTRAP_FAULTOFF, %g1
	wrpr	%g0, %g1, %tnpc	
	/*
	 * some wbuf handlers will call systrap to resolve the fault
	 * we pass the trap type so they figure out the correct parameters.
	 * g5 = trap type, g6 = tag access reg
	 * only use g5, g6, g7 registers after we have switched to alternate
	 * globals.
	 */
	USE_ALTERNATE_GLOBALS
	mov	MMU_TAG_ACCESS, %g5
	ldxa	[%g5] ASI_DMMU, %g6
	rdpr	%tt, %g7
	cmp	%g7, FAST_IMMU_MISS_TT
	be,a,pn	%icc, ptl1_panic
	  mov	PTL1_BAD_WTRAP, %g1
	cmp	%g7, FAST_DMMU_MISS_TT
	move	%icc, T_DATA_MMU_MISS, %g5
	movne	%icc, T_DATA_PROT, %g5
	done
1:
	CPU_ADDR(%g1, %g4)
	ld	[%g1 + CPU_TL1_HDLR], %g4
	brnz,a,pt %g4, sfmmu_mmu_trap
	  st	%g0, [%g1 + CPU_TL1_HDLR]
	ba,pt	%icc, ptl1_panic
	  mov	PTL1_BAD_TRAP, %g1
	SET_SIZE(sfmmu_window_trap)
#endif /* lint */

#if defined (lint)
/*
 * sfmmu_tsb_miss handlers
 *
 * These routines are responsible for resolving tlb misses once they have also
 * missed in the TSB.  They traverse the hmeblk hash list.  In the
 * case of user address space, it will attempt to grab the hash mutex
 * and in the case it doesn't succeed it will go to tl = 0 to resolve the
 * miss.   In the case of a kernel tlb miss we grab no locks.  This
 * eliminates the problem of recursive deadlocks (taking a tlb miss while
 * holding a hash bucket lock and needing the same lock to resolve it - but
 * it forces us to use a capture cpus when deleting kernel hmeblks).
 * It order to eliminate the possibility of a tlb miss we will traverse
 * the list using physical addresses.  It executes at  TL > 0.
 * NOTE: the following routines currently do not support large page sizes.
 *
 * Parameters:
 *		%g2 = MMU_TARGET register
 *		%g3 = ctx number
 */
void
sfmmu_ktsb_miss()
{
}

void
sfmmu_utsb_miss()
{
}

void
sfmmu_kprot_trap()
{
}

void
sfmmu_uprot_trap()
{
}
#else /* lint */

#if (C_SIZE != (1 << CTX_SZ_SHIFT))
#error - size of context struct does not match with CTX_SZ_SHIFT
#endif

#if (IMAP_SEG != 0)
#error - ism_map->ism_seg offset is not zero
#endif

/*
 * Copies ism mapping for this ctx in param "ism" if this is a ISM 
 * dtlb miss and branches to label "ismhit". If this is not an ISM 
 * process or an ISM dtlb miss it falls thru.
 *
 * In the rare event this is a ISM process and a ISM dtlb miss has
 * not been detected in the first ism map block, it will branch
 * to "exitlabel".
 *
 * Also hat_unshare() will set a busy bit in position 0 in c_ismblkpa
 * when its about to manipulate this ctx's ism maps. In that case
 * we also branch to "exitlabel".
 *
 * NOTE: We will never have any holes in our ISM maps. sfmmu_share/unshare
 *       will make sure of that. This means we can terminate our search on
 *       the first zero mapping we find.
 *
 * Parameters:
 * ctxptr  = 64 bit reg that points to current context structure (CLOBBERED)
 * vaddr   = 32 bit reg containing virtual address of tlb miss
 * tsbmiss = 32 bit address of tsb miss area
 * ism     = 64 bit reg where address of ismmap->ism_sfmmu will be stored
 * maptr   = 64 bit reg where address of ismmap will be stored
 * tmp1    = 64 bit scratch reg
 * tmp2    = 32 bit scratch reg
 * label:    temporary labels
 * ismhit:   label where to jump to if an ism dtlb miss
 * exitlabel:label where to jump if end of list is reached and there
 *	      is a next ismblk or hat is busy due to hat_unshare.
 */
#define ISM_CHECK(ctxptr, vaddr, tsbmiss, ism, maptr, tmp1, tmp2	\
	label, ismhit, exitlabel)					\
	ldx	[ctxptr + C_ISMBLKPA], tmp1	/* tmp1= phys &ismblk*/	;\
	brlz,pt  tmp1, label/**/2		/* exit if -1 */	;\
	  and	tmp1, CTX_ISM_BUSY, tmp2 /* check for hat_unshare */	;\
	brnz,pn	tmp2, exitlabel			/* exit if true */	;\
	  add	tmp1, IBLK_MAPS, maptr	/* maptr = &ismblk.map[0]*/	;\
	stn	ctxptr, [tsbmiss + (TSBMISS_SCRATCH + TSBMISS_HMEBP)]	;\
	ldna	[maptr] ASI_MEM, ism	/* ismblk.map[0].ism_seg */	;\
	mov	tmp1, ctxptr			/* ctxptr = &ismblk */	;\
									;\
label/**/1:								;\
	brz,pt  ism, label/**/2			/* no mapping */	;\
	  srlx	ism, ISM_VBASE_SHIFT, tmp2	/* tmp2 = vbase */	;\
	srlx	vaddr, ISM_VBASE_SHIFT, tmp1 	/* tmp1 = 4MB va seg*/	;\
	sub	tmp1, tmp2, tmp2		/* tmp2 = va - vbase*/	;\
	set	ISM_SZ_MASK, tmp1					;\
	and	ism, tmp1, tmp1			/* tmp1 = size */	;\
	cmp	tmp2, tmp1		 	/* check va <= offset*/	;\
	blu,a,pt  %xcc, ismhit			/* ism hit */		;\
	  add	maptr, IMAP_ISMHAT, maptr 	/* maptr = &ism_sfmmu*/ ;\
									;\
	add	maptr, ISM_MAP_SZ, maptr /* maptr += sizeof(map) */ 	;\
	add	ctxptr, (IBLK_MAPS + ISM_MAP_SLOTS * ISM_MAP_SZ), tmp1	;\
	cmp	maptr, tmp1						;\
	bl,pt	%xcc, label/**/1		/* keep looking  */	;\
	  ldna	[maptr] ASI_MEM, ism		/* ism = map[maptr] */	;\
									;\
	add	ctxptr, IBLK_NEXT, tmp1					;\
	ldna	[tmp1] ASI_MEM, tmp2		/* check blk->next */	;\
	brnz,pt	tmp2, exitlabel			/* continue search */	;\
	  nop								;\
label/**/2:	

/*
 * returns the hme hash bucket (hmebp) given the vaddr, and the hatid
 * It also returns the virtual pg for vaddr (ie. vaddr << hmeshift)
 * Parameters:
 * vaddr = reg containing virtual address
 * hatid = reg containing sfmmu pointer
 * hashsz = global variable containing number of buckets in hash
 * hashstart = global variable containing start of hash
 * hmeshift = constant/register to shift vaddr to obtain vapg
 * hmebp = register where bucket pointer will be stored
 * vapg = register where virtual page will be stored
 * tmp1, tmp2 = tmp registers
 */
#define	HMEHASH_FUNC_ASM(vaddr, hatid, tsbarea, hashsz, hashstart,	\
	hmeshift, hmebp, vapg, tmp1, tmp2)				\
	lduh	[tsbarea + hashsz], hmebp				;\
	ldn	[tsbarea + hashstart], tmp1				;\
	srlx	vaddr, hmeshift, vapg					;\
	xor	vapg, hatid, tmp2	/* hatid ^ (vaddr >> shift) */	;\
	and	tmp2, hmebp, hmebp	/* index into hme_hash */	;\
	mulx	hmebp, HMEBUCK_SIZE, hmebp				;\
	add	hmebp, tmp1, hmebp


/*
 * For ILP32 hashtag includes bspage + hashno + hatid (64 bits).
 * For LP64  hashtag includes bspage + hashno (64 bits).
 */
#ifdef	__sparcv9

#define	MAKE_HASHTAG(vapg, hatid, hmeshift, hashno, hblktag)		\
	sllx	vapg, hmeshift, vapg					;\
	or	vapg, hashno, hblktag

#else	/* ! __sparcv9 */

#define	MAKE_HASHTAG(vapg, hatid, hmeshift, hashno, hblktag)		\
	sll	vapg, hmeshift, vapg					;\
	or	vapg, hashno, vapg					;\
	sllx	vapg, HTAG_SFMMUPSZ, hblktag				;\
	or	hatid, hblktag, hblktag

#endif	/* __sparcv9 */

/*
 * Function to traverse hmeblk hash link list and find corresponding match
 * The search is done using physical pointers. It returns the physical address
 * and virtual address pointers to the hmeblk that matches with the tag
 * provided.
 * Parameters:
 * hmebp = register that pointes to hme hash bucket, also used as tmp reg
 * hmeblktag = register with hmeblk tag match
 * hatid    = register with hatid (only used for LP64)
 * hmeblkpa = register where physical ptr will be stored
 * hmeblkva = register where virtual ptr will be stored
 * tmp1 = 32bit tmp reg
 * tmp2 = 64bit tmp reg
 * label: temporary label
 */
#ifdef	__sparcv9

#define	HMEHASH_SEARCH(hmebp, hmeblktag, hatid, hmeblkpa, hmeblkva,	\
	tmp1, tmp2, label, searchstat, linkstat)		 	\
	ldx	[hmebp + HMEBUCK_NEXTPA], hmeblkpa			;\
	ldn	[hmebp + HMEBUCK_HBLK], hmeblkva			;\
	HAT_GLOBAL_DBSTAT(searchstat, tmp2, tmp1)			;\
label/**/1:								;\
	brz,pn	hmeblkva, label/**/2					;\
	HAT_GLOBAL_DBSTAT(linkstat, tmp2, tmp1)				;\
	add	hmeblkpa, HMEBLK_TAG, tmp1				;\
	ldxa	[tmp1] ASI_MEM, tmp2	 /* read 1st part of tag */	;\
	add	tmp1, CLONGSIZE, tmp1					;\
	ldxa	[tmp1] ASI_MEM, tmp1 	/* read 2nd part of tag */	;\
	xor	tmp2, hmeblktag, tmp2					;\
	xor	tmp1, hatid, tmp1					;\
	or	tmp1, tmp2, tmp1					;\
	brz,pn	tmp1, label/**/2	/* branch on hit */		;\
	  add	hmeblkpa, HMEBLK_NEXT, tmp1				;\
	ldna	[tmp1] ASI_MEM, hmeblkva	/* hmeblk ptr va */	;\
	add	hmeblkpa, HMEBLK_NEXTPA, tmp1				;\
	ba,pt	%xcc, label/**/1					;\
	  ldxa	[tmp1] ASI_MEM, hmeblkpa	/* hmeblk ptr pa */	;\
label/**/2:

#else	/* ! __sparcv9 */

#define	HMEHASH_SEARCH(hmebp, hmeblktag, hatid, hmeblkpa, hmeblkva,	\
	tmp1, tmp2, label, searchstat, linkstat)		 	\
	ldx	[hmebp + HMEBUCK_NEXTPA], hmeblkpa			;\
	ld	[hmebp + HMEBUCK_HBLK], hmeblkva			;\
	HAT_GLOBAL_DBSTAT(searchstat, tmp2, tmp1)			;\
label/**/1:								;\
	brz,pn	hmeblkva, label/**/2					;\
	HAT_GLOBAL_DBSTAT(linkstat, tmp2, tmp1)				;\
	add	hmeblkpa, HMEBLK_TAG, tmp1				;\
	ldxa	[tmp1] ASI_MEM, tmp2		/* read hblk_tag */	;\
	cmp	hmeblktag, tmp2			/* compare tags */	;\
	be,pn	%xcc, label/**/2					;\
	  nop								;\
	add	hmeblkpa, HMEBLK_NEXT, tmp1				;\
	ldna	[tmp1] ASI_MEM, hmeblkva	/* hmeblk ptr va */	;\
	add	hmeblkpa, HMEBLK_NEXTPA, tmp1				;\
	ba,pt	%xcc, label/**/1					;\
	  ldxa	[tmp1] ASI_MEM, hmeblkpa	/* hmeblk ptr pa */	;\
label/**/2:	
#endif	/* __sparcv9 */


/*
 * HMEBLK_TO_HMENT is a macro that given an hmeblk and a vaddr returns
 * he offset for the corresponding hment.
 * Parameters:
 * vaddr = register with virtual address
 * hmeblkpa = physical pointer to hme_blk
 * hment = register where address of hment will be stored
 * hmentoff = register where hment offset will be stored
 * label1 = temporary label
 */
#define	HMEBLK_TO_HMENT(vaddr, hmeblkpa, hmentoff, tmp1, label1)	\
	add	hmeblkpa, HMEBLK_MISC, hmentoff				;\
	lda	[hmentoff] ASI_MEM, tmp1 				;\
	andcc	tmp1, HBLK_SZMASK, %g0	 /* tmp1 = get_hblk_sz(%g5) */	;\
	bnz,a,pn  %icc, label1		/* if sz != TTE8K branch */	;\
	  or	%g0, HMEBLK_HME1, hmentoff				;\
	srl	vaddr, MMU_PAGESHIFT, tmp1				;\
	and	tmp1, NHMENTS - 1, tmp1		/* tmp1 = index */	;\
	/* XXX use shift when SFHME_SIZE becomes power of 2 */		;\
	mulx	tmp1, SFHME_SIZE, tmp1 					;\
	add	tmp1, HMEBLK_HME1, hmentoff				;\
label1:									;\

/*
 * GET_TTE is a macro that returns a TTE given a tag and hatid.
 *
 * Parameters:
 * tag       = 32 bit reg containing tag access eg (vaddr + ctx)
 * hatid     = 64 bit reg containing sfmmu pointer (CLOBBERED)
 * tte       = 64 bit reg where tte will be stored.
 * hmeblkpa  = 64 bit reg where physical pointer to hme_blk will be stored)
 * hmeblkva  = 32 bit reg where virtual pointer to hme_blk will be stored)
 * hmentoff  = 64 bit reg where hment offset will be stored)
 * hashsz    = global variable containing number of buckets in hash
 * hashstart = global variable containing start of hash
 * hmeshift  = constant/register to shift vaddr to obtain vapg
 * hashno    = constant/register hash number
 * label     = temporary label
 * exitlabel = label where to jump to when tte is not found. The hmebp lock
 *	 is still held at this time.
 * RFE: It might be worth making user programs update the cpuset field
 * in the hblk as well as the kernel.  This would allow us to go back to
 * one GET_TTE function for both kernel and user.
 */                                                             
#define GET_TTE(tag, hatid, tte, hmeblkpa, hmeblkva, tsbarea, hmentoff, \
		hashsz, hashstart, hmeshift, hashno, label, exitlabel,	\
		searchstat, linkstat)					\
									;\
	stn	tag, [tsbarea + (TSBMISS_SCRATCH + TSB_TAGACC)]		;\
	HMEHASH_FUNC_ASM(tag, hatid, tsbarea, hashsz, hashstart,	\
		hmeshift, tte, hmeblkpa, hmentoff, hmeblkva)		;\
									;\
	/*								;\
	 * tag   = vaddr						;\
	 * hatid = hatid						;\
	 * tsbarea = tsbarea						;\
	 * tte   = hmebp (hme bucket pointer)				;\
	 * hmeblkpa  = vapg  (virtual page)				;\
	 * hmentoff, hmeblkva = scratch					;\
	 */								;\
	MAKE_HASHTAG(hmeblkpa, hatid, hmeshift, hashno, hmentoff)	;\
									;\
	/*								;\
	 * tag   = vaddr						;\
	 * hatid = hatid						;\
	 * tte   = hmebp						;\
	 * hmeblkpa  = CLOBBERED					;\
	 * hmentoff  = hblktag (LP64 only htag_bspage & hashno)		;\
	 * hmeblkva  = scratch						;\
	 */								;\
	stn	tte, [tsbarea + (TSBMISS_SCRATCH + TSBMISS_HMEBP)]	;\
	HMELOCK_ENTER(tte, hmeblkpa, hmeblkva, label/**/3)		;\
	HMEHASH_SEARCH(tte, hmentoff, hatid, hmeblkpa, hmeblkva, 	\
		tte, tag, label/**/1, searchstat, linkstat)		;\
	/*								;\
	 * tag = CLOBBERED						;\
	 * tte = CLOBBERED						;\
	 * hmeblkpa = hmeblkpa						;\
	 * hmeblkva = hmeblkva						;\
	 */								;\
	brnz,pt	hmeblkva, label/**/4	/* branch if hmeblk found */	;\
	  ldn	[tsbarea + (TSBMISS_SCRATCH + TSB_TAGACC)], tag		;\
	ldn	[tsbarea + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], hatid	;\
	HMELOCK_EXIT(hatid)             /* drop lock */			;\
	ba,pt	%xcc, exitlabel		/* exit if hblk not found */	;\
	  nop								;\
label/**/4:								;\
	/*								;\
	 * We have found the hmeblk containing the hment.		;\
	 * Now we calculate the corresponding tte.			;\
	 *								;\
	 * tag   = vaddr						;\
	 * hatid = clobbered						;\
	 * tte   = hmebp						;\
	 * hmeblkpa  = hmeblkpa						;\
	 * hmentoff  = hblktag						;\
	 * hmeblkva  = hmeblkva 					;\
	 */								;\
	HMEBLK_TO_HMENT(tag, hmeblkpa, hmentoff, hatid, label/**/2)	;\
									;\
	add	hmentoff, SFHME_TTE, hmentoff				;\
	add     hmeblkpa, hmentoff, hmeblkpa				;\
	ldxa	[hmeblkpa] ASI_MEM, tte	/* MMU_READTTE through pa */	;\
	add     hmeblkva, hmentoff, hmeblkva				;\
	ldn	[tsbarea + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], hatid	;\
	HMELOCK_EXIT(hatid)		/* drop lock */			;\
	/*								;\
	 * tag   = vaddr						;\
	 * hatid = scratch						;\
	 * tte   = tte							;\
	 * hmeblkpa  = tte pa						;\
	 * hmentoff  = scratch						;\
	 * hmeblkva  = tte va						;\
	 */


/*
 * TTE_SET_REF_ML is a macro that updates the reference bit if it is
 * not already set.
 *
 * Parameters:
 * tte      = reg containing tte
 * ttepa    = physical pointer to tte
 * tteva    = virtual ptr to tte
 * tsbarea  = tsb miss area
 * tmp1     = tmp reg
 * label    = temporary label
 */

#define	TTE_SET_REF_ML(tte, ttepa, tteva, tsbarea, tmp1, label)		\
	/* check reference bit */					;\
	andcc	tte, TTE_REF_INT, %g0					;\
	bnz,pt	%xcc, label/**/4	/* if ref bit set-skip ahead */	;\
	  nop								;\
	GET_CPU_IMPL(tmp1)						;\
	cmp	tmp1, CHEETAH_IMPL					;\
	bl	%icc, label/**/1					;\
	/* update reference bit */					;\
	ld	[tsbarea + TSBMISS_DMASK], tmp1				;\
        stxa    %g0, [ttepa] ASI_DC_INVAL /* flush line from dcache */  ;\
	ba	label/**/2						;\
label/**/1:								;\
	and	tteva, tmp1, tmp1					;\
	stxa	%g0, [tmp1] ASI_DC_TAG /* flush line from dcache */	;\
label/**/2:								;\
	membar	#Sync							;\
label/**/3:								;\
	or	tte, TTE_REF_INT, tmp1					;\
	casxa	[ttepa] ASI_MEM, tte, tmp1 	/* update ref bit */	;\
	cmp	tte, tmp1						;\
	bne,a,pn %xcc, label/**/3					;\
	ldxa	[ttepa] ASI_MEM, tte	/* MMU_READTTE through pa */	;\
	or	tte, TTE_REF_INT, tte					;\
label/**/4:

#define	SAVE_TSBPTR8K(tsbptr, missarea)			\
	stn	tsbptr, [missarea + TSBMISS_TSBPTR]	/* save tsbptr */

#define RESTORE_TSBPTR8K(tsbptr, missarea)		\
	ldn	[missarea + TSBMISS_TSBPTR], tsbptr	/* restore tsbptr */

#define	SAVE_TSBPTR4M(tsbptr, missarea)			\
	stn	tsbptr, [missarea + TSBMISS_TSBPTR4M]	/* save tsbptr */

#define RESTORE_TSBPTR4M(tsbptr, missarea)		\
	ldn	[missarea + TSBMISS_TSBPTR4M], tsbptr	/* restore tsbptr */

/*
 * This function executes at tl>0.
 * It executes using the mmu alternate globals.
 */
	/*
	 * USER DATA 8K TSBMISS
	 *
	 * Checks tsb for 4M entry.
	 *
	 * Works under the assumption that 4M entries are only
	 * stored in 512K size tsbs. All small (128K) tsbs
	 * are contained inside 512K tsbs and the tsb must
	 * have the same alignment as its size. I only need
	 * to mask off the bottom 19 bits to get the 512K tsb
	 * base from the current 8k tsbp.
	 *
	 * g1 = tsb ptr
	 * g2 = TAG_TARGET
	 * g3 = ctx
	 * g4 = 512K tsb base mask bits[31:10] See GET_TSB_POINTER4M
	 */
	.align	64
	ENTRY_NP(sfmmu_udtlb_miss)
#ifdef	TRAPTRACE
	sethi %hi(UTSB_MAX_BASE_MASK), %g4 /* trap tracing uses g4 */
#endif
	and	%g1, %g4, %g6		/* g6 = 512k tsb base */
	andn	%g2, %g4, %g4		/* mask off base */
	srl	%g4, 0, %g4		/* g4 = vpg */
	xor	%g3, %g4, %g5		/* g5 = hash ctx ^ vpg */
	sll	%g5, TSB_ENTRY_SHIFT, %g4	/* g4 = offset */
	or	%g6, %g4, %g3		/* g3 = tsbp4m */
	ldda	[%g3] ASI_NQUAD_LD, %g4	/* g4 = tag, g5 = data */
	xor	%g4, %g2, %g6		/* g6 = tag ^ tag_target */
	srlx	%g5, TTE_SZ_SHFT, %g7	/* g7 = TTESZ_VALID | TTE_SZ_BITS */
	xor	%g7, (TTESZ_VALID | TTE4M), %g7
	or	%g6, %g7, %g6
	brnz,pn %g6, 1f
	  nop
	stxa    %g5, [%g0]ASI_DTLB_IN
	retry
	membar  #Sync
1:
	ALTENTRY(sfmmu_utsb_miss)
	/*
	 * USER TSB MISS
	 * g1 = tsb ptr
	 * g3 = 4M tsb ptr
	 */
	CPU_INDEX(%g7, %g6)
	sethi	%hi(tsbmiss_area), %g6
	sllx	%g7, TSBMISS_SHIFT, %g7
	or	%g6, %lo(tsbmiss_area), %g6
	add	%g6, %g7, %g6			/* g6 = tsbmiss area */

	SAVE_TSBPTR8K(%g1, %g6)
	SAVE_TSBPTR4M(%g3, %g6)

#ifdef	DEBUG
	/*
	 * Check tsb pointer calculations.
	 */
	rdpr	%tt, %g4
	cmp	%g4, FAST_IMMU_MISS_TT
	be,pn	%xcc, 2f			/* skip if instruction */
	  nop

	mov	MMU_TAG_ACCESS, %g3
	ldxa	[%g3] ASI_DMMU, %g2		/* g2 = tagacc */
	GET_UTSB_REGISTER(%g3)			/* g3 = utsb reg */

	set	utsb_4m_disable, %g4
	ld	[%g4], %g4
	brnz,pn	%g4, 1f				/* skip 4M check */
	  nop

	GET_TSB_POINTER4M(%g3, %g2, %g1, %g4, %g5, %g7) 
	RESTORE_TSBPTR4M(%g4, %g6)		/* g2 = saved 8k tsbp */
	cmp	%g1, %g4
	bne,pn	%xcc, ptl1_panic		/* panic on mis-match */
	  mov	PTL1_BAD_4MTSBP, %g1
1:
	GET_TSB_POINTER8K(%g3, %g2, %g1, %g4, %g5, %g7)
	RESTORE_TSBPTR8K(%g4, %g6)		/* g2 = saved 8k tsbp */
	cmp	%g1, %g4
	bne,pn	%xcc, ptl1_panic		/* panic on mis-match */
	  mov	PTL1_BAD_8KTSBP, %g1
2:
#endif	/* DEBUG */

	mov	MMU_TAG_ACCESS, %g4
	rdpr	%tt, %g5
	ldxa	[%g4] ASI_DMMU, %g3
	ldxa	[%g4] ASI_IMMU, %g2
	cmp	%g5, FAST_IMMU_MISS_TT
	movne	%xcc, %g3, %g2			/* g2 = vaddr + ctx */
	HAT_PERCPU_STAT32(%g6, TSBMISS_UTSBMISS, %g7)
	ldn	[%g6 + TSBMISS_CTXS], %g1	/* g1 = ctxs */
	sll	%g2, TAGACC_CTX_SHIFT, %g3
	srl	%g3, TAGACC_CTX_SHIFT, %g3	/* g3 = ctx */
	/* calculate hatid given ctxnum */
	sll	%g3, CTX_SZ_SHIFT, %g3
	add	%g3, %g1, %g1			/* g1 = ctx ptr */
        ldn	[%g1 + C_SFMMU], %g7            /* g7 = hatid */

	brz,pn	%g7, utsbmiss_tl0		/* if zero jmp ahead */
	  nop

	stn	%g7, [%g6 + (TSBMISS_SCRATCH + TSBMISS_HATID)]

	ISM_CHECK(%g1, %g2, %g6, %g3, %g4, %g5, %g7, utsb_l1,
		  utsb_ism, utsbmiss_tl0)

	ldn	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HATID)], %g7

1:
	GET_TTE(%g2, %g7, %g3, %g4, %g5, %g6, %g1,
		TSBMISS_UHASHSZ, TSBMISS_UHASHSTART,
		HBLK_RANGE_SHIFT, 1, utsb_l2, utsb_pagefault,
		HATSTAT_UHASH_SEARCH, HATSTAT_UHASH_LINKS)

	/*
	 * g1 = scratch
	 * g2 = tagacc and in TSB_TAGACC
	 * g3 = tte
	 * g4 = tte pa
	 * g5 = tte va
	 * g6 = tsbmiss area
	 * g7 = scratch
	 * hmebp in TSBMISS_HMEBP
	 */
	brgez,a,pn %g3, utsb_pagefault	/* if tte invalid branch */
	  nop

        /* 
	 * If itlb miss check exec bit.
	 * if not set treat as invalid.
	 */
        rdpr    %tt, %g7
        cmp     %g7, FAST_IMMU_MISS_TT
        bne,pt %icc, 3f
         andcc   %g3, TTE_EXECPRM_INT, %g0	/* check execute bit is set */
        bz,a,pn %icc, utsb_pagefault		/* it as invalid */
          nop
3:

	/*
	 * Set reference bit if not already set
	 */
	TTE_SET_REF_ML(%g3, %g4, %g5, %g6, %g7, utsb_l3) 

	/*
	 * Now, load into TSB/TLB
	 * g2 = tagacc
	 * g3 = tte
	 * g4 = patte
	 */
	rdpr	%tt, %g5
	cmp	%g5, FAST_IMMU_MISS_TT
	be,pn	%icc, 8f
  	  srlx	%g3, TTE_SZ_SHFT, %g5
	/* dmmu miss */
	cmp	%g5, TTESZ_VALID | TTE8K
	bne,pn	%icc, 4f
	  nop

	RESTORE_TSBPTR8K(%g1, %g6)

	ldxa	[%g0]ASI_DMMU, %g2		/* tag target */
	TSB_UPDATE_TL(%g1, %g3, %g2, %g5, %g6, %g4, 7)
	
4:
	stxa	%g3, [%g0] ASI_DTLB_IN
	retry
	membar	#Sync
8:
	/* immu miss */
	cmp	%g5, TTESZ_VALID | TTE8K
	bne,a,pn %icc, 4f
	  nop

	RESTORE_TSBPTR8K(%g1, %g6)

	ldxa	[%g0]ASI_IMMU, %g2		/* tag target */
	TSB_UPDATE_TL(%g1, %g3, %g2, %g5, %g6, %g4, 7)
4:
	stxa	%g3, [%g0] ASI_ITLB_IN
	retry
	membar	#Sync
utsb_pagefault:
	/*
	 * we get here if we couldn't find a valid tte in the hash.
	 * if we are at tl>0 we go to window handling code, otherwise
	 * we call pagefault.
	 */
	rdpr	%tl, %g5
	cmp	%g5, 1
	bg,pn	%icc, sfmmu_window_trap
	  nop
	ba,pt	%icc, sfmmu_pagefault
	  nop

utsb_ism:
	/*
	 * This is an ISM dtlb miss. 
	 *
	 * g2 = vaddr + ctx
	 * g3 = ismmap->ism_seg
	 * g4 = &ismmap->ism_sfmmu
	 * g6 = tsbmiss area
	 */
	ldna	[%g4] ASI_MEM, %g1		/* g1 = ism hatid */
	brz,a,pn %g1, ptl1_panic		/* if zero jmp ahead */
	mov	PTL1_BAD_ISM, %g1

	srlx	%g3, ISM_VBASE_SHIFT, %g3	/* clr size field */
	sllx	%g3, ISM_VBASE_SHIFT, %g3	/* g4 = ism vbase */
	set	TAGACC_CTX_MASK, %g7		/* mask off ctx number */
	andn	%g2, %g7, %g5			/* g6 = tlb miss vaddr */
	sub	%g5, %g3, %g4			/* g4 = offset in ISM seg */	

	/*
	 * ISM pages are always locked down.
	 * If we can't find the tte then pagefault
	 * and let the spt segment driver resovle it
	 *
	 * We first check if this ctx has large pages.
	 * If so hash for 4mb first. If that fails,
	 * hopefully rare, then rehash for 8k. We
	 * don't support 64k and 512k pages for ISM
	 * so no need to hash for them.
	 *
	 * g1 = ISM hatid
	 * g2 = orig tag (vaddr + ctx)
	 * g4 = ISM vaddr (offset in ISM seg + ctx)
	 * g6 = tsb miss area
	 */

	ldn	[%g6 + TSBMISS_SCRATCH + TSBMISS_HMEBP], %g7 /* ctx ptr */
	lduh	[%g7 + C_FLAGS], %g7		/* g7 = ctx->c_flags	*/
	and	%g7, LTTES_FLAG, %g7
	brz,pn	%g7, 1f				/* branch if not lpages */
	  nop

	/* save %g1 */
	stn	%g1, [%g6 + (TSBMISS_SCRATCH + TSBMISS_HATID)]
	
	/*
	 * 4M hash.
	 */
	GET_TTE(%g4, %g1, %g3, %g5, %g7, %g6, %g2,
		TSBMISS_UHASHSZ, TSBMISS_UHASHSTART,
		MMU_PAGESHIFT4M, 3, utsb_l4, utsb_ism_small,
		HATSTAT_UHASH_SEARCH, HATSTAT_UHASH_LINKS)

	/*
	 * If tte is valid then skip ahead.
	 *
	 * g3 = tte
	 */
	brlz,pt %g3, 2f		/* if valid tte branch */
	  nop

utsb_ism_small:
	/* restore hatid */
	ldn	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HATID)], %g1
1:
	/*
	 * 8k hash.
	 */
	GET_TTE(%g4, %g1, %g3, %g5, %g7, %g6, %g2,
		TSBMISS_UHASHSZ, TSBMISS_UHASHSTART,
		HBLK_RANGE_SHIFT, 1, utsb_l5, utsb_pagefault,
		HATSTAT_UHASH_SEARCH, HATSTAT_UHASH_LINKS)

	/*
	 * If tte is invalid then pagefault and let the 
	 * spt segment driver resolve it.
	 *
	 * g3 = tte
	 * g5 = tte pa
	 * g7 = tte va
	 * g6 = tsbmiss area
	 * g2 = clobbered
	 * g4 = clobbered
	 */
	brgez,pn %g3, utsb_pagefault	/* if tte invalid branch */
	  nop
2:
	/*
	 * If itlb miss check execute bit.
	 * If not set treat as invalid
	 */
	rdpr	%tt, %g2
	cmp	%g2, FAST_IMMU_MISS_TT
	bne,pt %icc, 3f
	 andcc	%g3, TTE_EXECPRM_INT, %g0
	bz,a,pn	%icc, utsb_pagefault
	 nop

3:
	/*
	 * Set reference bit if not already set
	 */
	TTE_SET_REF_ML(%g3, %g5, %g7, %g6, %g4, utsb_l6)

	/*
	 * load into the TSB
	 * g2 = trap type
	 * g3 = tte
	 * g5 = tte pa
	 */
	cmp	%g2, FAST_IMMU_MISS_TT
	be,pn	%icc, 8f
	 srlx	%g3, TTE_SZ_SHFT, %g2

	/*
	 * dmmu miss
	 *
	 * Currently we only support 8K and 4M pages
	 * in the tsb.
	 *
	 * g2 = tte sz
	 * g3 = tte
	 * g5 = tte pa
	 */
	sethi	%hi(utsb_4m_disable), %g4
	ld	[%g4 + %lo(utsb_4m_disable)], %g4
	brz,pn	%g4, 1f				/*  if 4m support enabled */
	  nop
	cmp	%g2, TTESZ_VALID | TTE4M
	be,pn	%icc, 5f
	  nop
1:	
	cmp	%g2, TTESZ_VALID | TTE64K
	be,pn	%icc, 5f	
	  nop
	cmp	%g2, TTESZ_VALID | TTE512K
	be,pn	%icc, 5f	
	  mov	TSBMISS_TSBPTR, %g7
	cmp	%g2, TTESZ_VALID | TTE8K
	movne	%icc, TSBMISS_TSBPTR4M, %g7
	ldn	[%g6 + %g7], %g1		/* restore tsbp */
	
	ldxa	[%g0]ASI_DMMU, %g4		/* tag target */
	TSB_UPDATE_TL(%g1, %g3, %g4, %g2, %g6, %g5, 7)
5:

	stxa	%g3, [%g0] ASI_DTLB_IN
	retry
	membar	#Sync

8:
	/*
	 * immu miss 
	 *
	 * Currently only optimized for 8k entries.
	 */
	cmp	%g2, TTESZ_VALID | TTE8K
	bne,pn %icc, 5f	
	  nop

	ldn     [%g6 + TSBMISS_TSBPTR], %g1	/* g1 = 8ktsbptr */
	ldxa	[%g0]ASI_IMMU, %g4		/* tag target */
	TSB_UPDATE_TL(%g1, %g3, %g4, %g2, %g6, %g5, 7)
5:
	stxa	%g3, [%g0] ASI_ITLB_IN
	retry
	membar	#Sync

utsbmiss_tl0:
	/*
	 * We get here when we need to service this tsb miss at tl=0.
	 * Causes: ctx was stolen, more than ISM_MAP_SLOTS ism segments 
	 * or ism maps are being modified by C code.
	 *
	 * g2 = tag access
	 */
	rdpr	%tl, %g5
	cmp	%g5, 1
	ble,pt	%icc, sfmmu_mmu_trap
	  nop
	ba,pt	%icc, sfmmu_window_trap
	  nop
	SET_SIZE(sfmmu_utsb_miss)

#if (1<< TSBMISS_SHIFT) != TSBMISS_SIZE
#error - TSBMISS_SHIFT does not correspond to size of tsbmiss struct
#endif

/*
 * This routine can execute for both tl=0 and tl>0 traps.
 * When running for tl=0 traps it runs on the alternate globals,
 * otherwise it runs on the mmu globals.
 */

	ENTRY_NP(sfmmu_ktsb_miss)
	/*
	 * KERNEL TSB MISS
	 * %g1 = tsb ptr
	 */
	CPU_INDEX(%g7, %g6)
	sethi	%hi(tsbmiss_area), %g6
	sllx	%g7, TSBMISS_SHIFT, %g7
	or	%g6, %lo(tsbmiss_area), %g6
	add	%g6, %g7, %g6			/* g6 = tsbmiss area */

	SAVE_TSBPTR8K(%g1, %g6)
	
	mov	MMU_TAG_ACCESS, %g4
	rdpr	%tt, %g5
	ldxa	[%g4] ASI_DMMU, %g3
	ldxa	[%g4] ASI_IMMU, %g2
	cmp	%g5, FAST_IMMU_MISS_TT
	movne	%xcc, %g3, %g2			/* g2 = vaddr + ctx */
	HAT_PERCPU_STAT32(%g6, TSBMISS_KTSBMISS, %g7)
	ldn	[%g6 + TSBMISS_KHATID], %g1	/* g1 = ksfmmup */
	stn	%g2, [%g6 + (TSBMISS_SCRATCH + TSB_TAGACC)]
	/*
	 * I can't use the GET_TTE macro because I want to re-search on 512K.
	 * g1 = hatid
	 * g2 = tagaccess
	 */
	HMEHASH_FUNC_ASM(%g2, %g1, %g6, TSBMISS_KHASHSZ, TSBMISS_KHASHSTART,
		HBLK_RANGE_SHIFT, %g7, %g4, %g5, %g3)
	/*
	 * g7 = hme bucket
	 * g4 = virtual page of addr
	 */
	stn	%g7, [%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)]
	MAKE_HASHTAG(%g4, %g1, HBLK_RANGE_SHIFT, 1, %g5)
	/*
	 * g5 = hblktag
	 */
	HMELOCK_ENTER(%g7, %g1, %g3, ktsb_l1)
	ldn	[%g6 + TSBMISS_KHATID], %g1	/* g1 = ksfmmup */
	HMEHASH_SEARCH(%g7, %g5, %g1, %g4, %g2, %g7, %g3, ktsb_l2,
		HATSTAT_KHASH_SEARCH, HATSTAT_KHASH_LINKS)
	/*
	 * g4 = hmeblkpa
	 * g2 = hmeblkva
	 */
	brnz,a,pt %g2, khash_hit
	nop
	/*
 	 * We need to search the hash table using 512K rehash value.
	 */
	ldn	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g7
	HMELOCK_EXIT(%g7)	/* drop hashlock */
	ldn	[%g6 + (TSBMISS_SCRATCH + TSB_TAGACC)], %g2
	ldn	[%g6 + TSBMISS_KHATID], %g1
	HMEHASH_FUNC_ASM(%g2, %g1, %g6, TSBMISS_KHASHSZ, TSBMISS_KHASHSTART,
		MMU_PAGESHIFT512K, %g7, %g4, %g5, %g3)
	/*
	 * g7 = hme bucket
	 * g4 = virtual page of addr
	 */
	stn	%g7, [%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)]
	MAKE_HASHTAG(%g4, %g1, MMU_PAGESHIFT512K, TTE512K, %g5)
	/*
	 * g5 = hblktag
	 */
	HMELOCK_ENTER(%g7, %g1, %g3, ktsb_l512_1)
	ldn	[%g6 + TSBMISS_KHATID], %g1	/* g1 = ksfmmup */
	HMEHASH_SEARCH(%g7, %g5, %g1, %g4, %g2, %g7, %g3, ktsb_l512_2,
		HATSTAT_KHASH_SEARCH, HATSTAT_KHASH_LINKS)
	brz,pn %g2, ktsb_done
	  ldn	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g7

khash_hit:	
		
	ldn	[%g6 + (TSBMISS_SCRATCH + TSB_TAGACC)], %g7
	/*
	 * g7 = tag access
	 * g4 = hmeblkpa
	 * g2 = hmeblkva
	 */
	HMEBLK_TO_HMENT(%g7, %g4, %g5, %g3, ktsb_l3)

	/* g5 = hmentoff */
	add	%g5, SFHME_TTE, %g5
	add	%g4, %g5, %g4
	add	%g2, %g5, %g5
	ldxa	[%g4] ASI_MEM, %g3
	/*
	 * g7 = vaddr + ctx
	 * g3 = tte
	 * g4 = tte pa
	 * g5 = tte va
	 * g6 = tsbmiss area
	 * g1 = clobbered
	 * g2 = clobbered
	 */
	brgez,a,pn %g3, ktsb_done	/* if tte invalid branch */
	  ldn	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g7

#ifdef	DEBUG
	sllx	%g3, TTE_PA_LSHIFT, %g1
	srlx	%g1, 30 + TTE_PA_LSHIFT, %g1	/* if not memory continue */
	brnz,pn	%g1, 2f
	  nop
	andcc	%g3, TTE_CP_INT, %g0		/* if memory - check it is */
	bz,a,pn	%icc, ptl1_panic		/* ecache cacheable */
	  mov	PTL1_BAD_TTE_PA, %g1
2:
#endif	/* DEBUG */

	/* 
	 * If an itlb miss check nfo bit.
	 * If set, pagefault. XXX
	 */ 
	rdpr    %tt, %g1
	cmp     %g1, FAST_IMMU_MISS_TT
	bne,pt %icc, 4f
	  andcc    %g3, TTE_EXECPRM_INT, %g0	/* check execute bit is set */
	bz,a,pn %icc, ktsb_done			/* it is invalid */
	  ldn	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g7
4:
	ldn	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g1
	HMELOCK_EXIT(%g1)
	/*
	 * Set reference bit if not already set
	 */
	TTE_SET_REF_ML(%g3, %g4, %g5, %g6, %g1, ktsb_l4)

#ifdef	DEBUG
	ldxa	[%g4] ASI_MEM, %g5		/* MMU_READTTE through pa */
	sllx	%g5, TTE_PA_LSHIFT, %g1
	srlx	%g1, 30 + TTE_PA_LSHIFT, %g1	/* if not memory continue */
	brnz,pn	%g1, 6f
	  nop
	andcc	%g5, TTE_CP_INT, %g0		/* if memory - check it is */
	bz,a,pn	%icc, ptl1_panic		/* ecache cacheable */
	mov	PTL1_BAD_TTE_PA, %g1
6:
#endif	/* DEBUG */
	/*
	 * Now, load into TSB/TLB
	 * g7 = clobbered
	 * g6 = tsbmiss area
	 * g3 = tte
	 * g4 = ttepa
	 */
	rdpr	%tt, %g5
	cmp	%g5, FAST_IMMU_MISS_TT
	be,pn	%icc, 8f
	  srlx	%g3, TTE_SZ_SHFT, %g5
	/* dmmu miss */
	cmp	%g5, TTESZ_VALID | TTE8K
	bne,a,pn %icc, 5f
	  nop

	RESTORE_TSBPTR8K(%g1, %g6)

	ldxa	[%g0]ASI_DMMU, %g2		/* tag target */
	TSB_UPDATE_TL(%g1, %g3, %g2, %g5, %g6, %g4, 7)
5:
	stxa	%g3, [%g0] ASI_DTLB_IN
	retry
	membar	#Sync
8:
	/* immu miss */
	cmp	%g5, TTESZ_VALID | TTE8K
	bne,a,pn %icc, 4f
	  nop

	RESTORE_TSBPTR8K(%g1, %g6)

	ldxa	[%g0]ASI_IMMU, %g2		/* tag target */
	TSB_UPDATE_TL(%g1, %g3, %g2, %g5, %g6, %g4, 7)
4:
	stxa	%g3, [%g0] ASI_ITLB_IN
	retry
	membar	#Sync

ktsb_done:
	HMELOCK_EXIT(%g7)	/* drop hashlock */
	/*
	 * we get here if we couldn't find valid hment in hash
	 * first check if we are tl > 1, in which case we panic.
	 * otherwise call pagefault
	 */
	rdpr	%tl, %g5
	cmp	%g5, 1
	ble,pt	%xcc, sfmmu_mmu_trap
	  nop
	ba,pt	%xcc, ptl1_panic
	mov	PTL1_BAD_KMISS, %g1
	SET_SIZE(sfmmu_ktsb_miss)


	ENTRY_NP(sfmmu_kprot_trap)
	/*
	 * KERNEL Write Protect Traps
	 *
	 * %g2 = MMU_TAG_ACCESS
	 */
	CPU_INDEX(%g7, %g6)
	sethi	%hi(tsbmiss_area), %g6
	sllx	%g7, TSBMISS_SHIFT, %g7
	or	%g6, %lo(tsbmiss_area), %g6
	add	%g6, %g7, %g6			/* g6 = tsbmiss area */
	HAT_PERCPU_STAT16(%g6, TSBMISS_KPROTS, %g7)
	ldn	[%g6 + TSBMISS_KHATID], %g1	/* g1 = ksfmmup */
	stn	%g2, [%g6 + (TSBMISS_SCRATCH + TSB_TAGACC)] /* tagaccess */
	/*
	 * I can't use the GET_TTE macro because I want to update the
	 * kcpuset field.
	 * g1 = hatid
	 * g2 = tagaccess
	 * g6 = tsbmiss area
	 */
	HMEHASH_FUNC_ASM(%g2, %g1, %g6, TSBMISS_KHASHSZ, TSBMISS_KHASHSTART,
		HBLK_RANGE_SHIFT, %g7, %g4, %g5, %g3)
	/*
	 * g4 = virtual page of addr
	 * g7 = hme bucket
	 */
	stn	%g7, [%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)]
	MAKE_HASHTAG(%g4, %g1, HBLK_RANGE_SHIFT, 1, %g5)
	/*
	 * g5 = hblktag	(LP64 only htag_bspage & hashno)
	 */
	HMELOCK_ENTER(%g7, %g1, %g3, kprot_l1)
	ldn	[%g6 + TSBMISS_KHATID], %g1	/* g1 = ksfmmup */

	HMEHASH_SEARCH(%g7, %g5, %g1, %g4, %g2, %g7, %g3, kprot_l2,
		HATSTAT_KHASH_SEARCH, HATSTAT_KHASH_LINKS)
	/*
	 * g4 = hmeblkpa
	 * g2 = hmeblkva
	 */
	brz,a,pn %g2, kprot_tl0
	 ldn	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g7
	
	ldn	[%g6 + (TSBMISS_SCRATCH + TSB_TAGACC)], %g7 /* tag access */
	/*
	 * g7 = tag access
	 * g4 = hmeblkpa
	 * g2 = hmeblkva
	 */
	HMEBLK_TO_HMENT(%g7, %g4, %g5, %g3, kprot_l3)
	/* g5 = hmentoff */
	add	%g5, SFHME_TTE, %g5
	add	%g4, %g5, %g4
	ldxa	[%g4] ASI_MEM, %g3		/* read tte */
	add	%g2, %g5, %g5
	/*
	 * g7 = vaddr + ctx
	 * g3 = tte
	 * g4 = tte pa
	 * g5 = tte va
	 * g6 = tsbmiss area
	 * g1 = clobbered
	 * g2 = clobbered
	 */
	ldn	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g1
	HMELOCK_EXIT(%g1)
	brgez,pn %g3, kprot_inval	/* if tte invalid branch */
	 andcc	%g3, TTE_WRPRM_INT, %g0		/* check write permissions */
	bz,pn	%xcc, kprot_protfault		/* no, jump ahead */
	 andcc	%g3, TTE_HWWR_INT, %g0		/* check if modbit is set */
	bnz,a,pn	%xcc, 5f		/* yes, go load tte into tsb */
	 nop
	/* update mod bit  */
	GET_CPU_IMPL(%g1)
	cmp	%g1, CHEETAH_IMPL
	bl	%icc, kprot_trap_1
	ld	[%g6 + TSBMISS_DMASK], %g7
        stxa    %g0, [%g4]ASI_DC_INVAL          /* flush line from dcache */
	ba	kprot_trap_2
kprot_trap_1:
	and	%g7, %g5, %g5
	stxa	%g0, [%g5]ASI_DC_TAG		/* flush line from dcache */
kprot_trap_2:
	membar	#Sync
4:
	or	%g3, TTE_HWWR_INT | TTE_REF_INT, %g1
	casxa	[%g4] ASI_MEM, %g3, %g1		/* update ref/mod bit */
	cmp	%g3, %g1
	bne,a,pn %xcc, 4b
	  ldxa	[%g4] ASI_MEM, %g3		/* MMU_READTTE through pa */
	or	%g3, TTE_HWWR_INT | TTE_REF_INT, %g3
5:
	/*
	 * load into the TSB
	 * g2 = vaddr
	 * g3 = tte
	 * g4 = tte pa
	 */
	srlx	%g3, TTE_SZ_SHFT, %g5
	cmp	%g5, TTESZ_VALID | TTE8K
	bne,pn	%icc, 6f
	  nop
	ldn	[%g6 + (TSBMISS_SCRATCH + TSB_TAGACC)], %g2 /* tag access */
	GET_KTSB_REGISTER(%g6)
	GET_TSB_POINTER8K(%g6, %g2, %g1, %g5, %g2, %g7)
	ldxa	[%g0]ASI_DMMU, %g2
	TSB_UPDATE_TL(%g1, %g3, %g2, %g5, %g6, %g4, 7)
	/*
	 * Now, load into TLB
	 */
6:
	stxa	%g3, [%g0] ASI_DTLB_IN
	retry
	membar	#Sync

kprot_tl0:
	/*
	 * we get here if we couldn't find hmeblk in hash
	 * since we were able to find the tte in the tlb, the trap
	 * most likely ocurred on large page tte.
	 * first check if we are tl > 1, in which case we panic.
	 * otherwise call sfmmu_tsb_miss
	 * g7 = hme bucket
	 */
	HMELOCK_EXIT(%g7)
	rdpr	%tl, %g5
	cmp	%g5, 1
	ble,pt	%icc, sfmmu_mmu_trap
	  nop
	ba,pt	%icc, ptl1_panic
	mov	PTL1_BAD_KPROT_TL0, %g1

kprot_inval:
	/*
	 * We get here if tte was invalid. XXX  We fake a data mmu miss.
	 */
	rdpr	%tl, %g5
	cmp	%g5, 1
	ble,pt	%icc, sfmmu_pagefault
	  nop
	ba,pt	%icc, ptl1_panic
	mov	PTL1_BAD_KPROT_INVAL, %g1

kprot_protfault:
	/*
	 * We get here if we didn't have write permission on the tte.
	 * first check if we are tl > 1, in which case we panic.
	 * otherwise call pagefault
	 */
	rdpr	%tl, %g5
	cmp	%g5, 1
	ble,pt	%icc, sfmmu_pagefault
	  nop
	ba,pt %xcc, ptl1_panic
	mov	PTL1_BAD_KPROT_FAULT, %g1
	SET_SIZE(sfmmu_kprot_trap)


	ENTRY_NP(sfmmu_uprot_trap)
	/*
	 * USER Write Protect Trap
	 */
	mov	MMU_TAG_ACCESS, %g1
	ldxa	[%g1] ASI_DMMU, %g2		/* g2 = vaddr + ctx */
	sll	%g2, TAGACC_CTX_SHIFT, %g3
	srl	%g3, TAGACC_CTX_SHIFT, %g3	/* g3 = ctx */
	CPU_INDEX(%g7, %g6)
	sethi	%hi(tsbmiss_area), %g6
	sllx	%g7, TSBMISS_SHIFT, %g7
	or      %g6, %lo(tsbmiss_area), %g6
	add     %g6, %g7, %g6			/* g6 = tsbmiss area */
	HAT_PERCPU_STAT16(%g6, TSBMISS_UPROTS, %g7)
	/* calculate hatid given ctxnum */
	ldn	[%g6 + TSBMISS_CTXS], %g1	/* g1 = ctxs */
	sll	%g3, CTX_SZ_SHIFT, %g3
	add	%g3, %g1, %g1				/* g1 = ctx ptr */
	ldn	[%g1 + C_SFMMU], %g7                   /* g7 = hatid */
	brz,pn	%g7, uprot_tl0			/* if zero jmp ahead */
	  nop

	/*
	 * If ism goto sfmmu_tsb_miss
	 */
	stn	%g7, [%g6 + (TSBMISS_SCRATCH + TSBMISS_HATID)]
	ISM_CHECK(%g1, %g2, %g6, %g3, %g4, %g5, %g7, uprot_l1, 
		  uprot_tl0, uprot_tl0)
	ldn	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HATID)], %g7

	GET_TTE(%g2, %g7, %g3, %g4, %g5, %g6, %g1,
		TSBMISS_UHASHSZ, TSBMISS_UHASHSTART,
		HBLK_RANGE_SHIFT, 1, uprot_l2, uprot_fault,
		HATSTAT_UHASH_SEARCH, HATSTAT_UHASH_LINKS)
	/*
	 * g3 = tte
	 * g4 = tte pa
	 * g5 = tte va
	 * g6 = tsb miss area
	 * hmebp in TSBMISS_HMEBP
	 */
	brgez,a,pn %g3, uprot_fault		/* if tte invalid goto tl0 */
	  nop

	andcc	%g3, TTE_WRPRM_INT, %g0		/* check write permissions */
	bz,a,pn	%xcc, uprot_wrfault		/* no, jump ahead */
	  nop
	andcc	%g3, TTE_HWWR_INT, %g0		/* check if modbit is set */
	bnz,pn	%xcc, 6f			/* yes, go load tte into tsb */
	  nop
	/* update mod bit  */
	GET_CPU_IMPL(%g1)
	cmp	%g1, CHEETAH_IMPL
	bge	%icc, uprot_trap_1
	ld	[%g6 + TSBMISS_DMASK], %g7
	and	%g7, %g5, %g5
	ba	uprot_trap_2
	stxa	%g0, [%g5]ASI_DC_TAG		/* flush line from dcache */
uprot_trap_1:
        stxa    %g0, [%g4]ASI_DC_INVAL          /* flush line from dcache */
uprot_trap_2:
	membar	#Sync
9:
	or	%g3, TTE_HWWR_INT | TTE_REF_INT, %g1
	casxa	[%g4] ASI_MEM, %g3, %g1		/* update ref/mod bit */
	cmp	%g3, %g1
	bne,a,pn %xcc, 9b
	  ldxa	[%g4] ASI_MEM, %g3		/* MMU_READTTE through pa */
	or	%g3, TTE_HWWR_INT | TTE_REF_INT, %g3
6:

	/*
	 * load into the TSB
	 * g2 = vaddr
	 * g3 = tte
	 * g4 = tte pa
	 */
	srlx	%g3, TTE_SZ_SHFT, %g5
	cmp	%g5, TTESZ_VALID | TTE8K
	bne,pn	%icc, 4f
	  nop
	GET_UTSB_REGISTER(%g6)
	GET_TSB_POINTER8K(%g6, %g2, %g1, %g5, %g2, %g7)
	ldxa	[%g0]ASI_DMMU, %g2		/* tag target */
	TSB_UPDATE_TL(%g1, %g3, %g2, %g5, %g6, %g4, 7)
4:
	/*
	 * Now, load into TLB
	 */
	stxa	%g3, [%g0] ASI_DTLB_IN
	retry
	membar	#Sync

uprot_fault:
	/*
	 * we get here if we couldn't find valid hment in hash
	 * first check if we are tl > 1, in which case we call window_trap
	 * otherwise call sfmmu_tsb_miss to change trap type to MMU_MISS
	 * if no valid large page translations are found.
	 * g2 = tag access reg
         */

	rdpr	%tl, %g4
	cmp	%g4, 1
	bg,pn	%xcc, sfmmu_window_trap
	  nop
	ba,pt	%xcc, sfmmu_mmu_trap
	  nop

uprot_wrfault:
	/*
	 * we get here if we didn't have write permissions
	 */
	rdpr	%tl, %g4
	cmp	%g4, 1
	bg,pn	%xcc, sfmmu_window_trap
	  nop
	ba,pt	%xcc, sfmmu_pagefault
	  nop


uprot_tl0:
	/*
	 * We get here in the case we need to service this protection
	 * in c code.  Causes:
	 * ctx was stolen
	 * write fault on ism segment.
	 *
	 * first check if we are tl > 1, in which case we panic.
	 * otherwise call sfmmu_tsb_miss
	 * g2 = tag access reg
	 */
	rdpr	%tl, %g4
	cmp	%g4, 1
	ble,pt	%icc, sfmmu_mmu_trap
	  nop
	ba,pt	%icc, sfmmu_window_trap
	  nop
	SET_SIZE(sfmmu_uprot_trap)

#endif /* lint */

#if defined (lint)
/*
 * This routine will look for a user or kernel vaddr in the hash
 * structure.  It returns a valid pfn or PFN_INVALID.  It doesn't
 * grab any locks.  It should only be used by other sfmmu routines.
 */
/* ARGSUSED */
pfn_t
sfmmu_vatopfn(caddr_t vaddr, sfmmu_t *sfmmup)
{
	return(0);
}

#else /* lint */

	ENTRY_NP(sfmmu_vatopfn)
	save	%sp, -SA(MINFRAME), %sp
	/*
	 * disable interrupts
	 */
	rdpr	%pstate, %i4
#ifdef DEBUG
	andcc	%i4, PSTATE_IE, %g0		/* if interrupts already */
	bnz,pt	%icc, 1f			/* disabled, panic	 */
	  nop
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic1), %o0
1:
#ifdef	__sparcv9
	andcc	%i4, PSTATE_AM, %g0		/* if PSTATE_AM set panic */
	bz,pt	%icc, 2f
	  nop
	sethi	%hi(sfmmu_panic2), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic2), %o0
2:
#endif	/* __sparcv9 */
#endif /* DEBUG */
	/*
	 * disable interrupts, clear Address Mask to access 64 bit physaddr
	 */
	andn    %i4, PSTATE_IE | PSTATE_AM, %i5
	wrpr    %i5, 0, %pstate

	/*
	 * i0 = vaddr
	 * i1 = sfmmup
	 */
#ifndef	__sparcv9
	srl	%i0, 0, %i0			/* clear upper 32 bits */
	srl	%i1, 0, %i1			/* clear upper 32 bits */
#endif
	CPU_INDEX(%o1, %o2)
	sethi	%hi(tsbmiss_area), %o2
	sllx	%o1, TSBMISS_SHIFT, %o1
	or	%o2, %lo(tsbmiss_area), %o2
	add	%o2, %o1, %o2			/* o2 = tsbmiss area */
	ldn	[%o2 + TSBMISS_KHATID], %l1
	cmp	%l1, %i1
	be,pt	%ncc, vatopfn_kernel
	  mov	%i1, %o4			/* o4 = hatid */

	/*
	 * This routine does NOT support user addresses
	 * There is a routine in C that supports this.
	 * The only reason why we don't have the C routine
	 * support kernel addresses as well is because
	 * we do va_to_pa while holding the hashlock.
	 */
	wrpr	%g0, %i4, %pstate		/* enable interrupts */
	sethi	%hi(sfmmu_panic3), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic3), %o0
	ret
	restore

vatopfn_kernel:
	/*
	 * i0 = vaddr
	 * i1 & o4 = hatid
	 * o2 = tsbmiss area
	 */
	mov	1, %l5				/* l5 = rehash # */
	mov	HBLK_RANGE_SHIFT, %l6
1:

	/*
	 * i0 = vaddr
	 * i1 & o4 = hatid
	 * l5 = rehash #
	 * l6 = hmeshift
	 */
	GET_TTE(%i0, %o4, %g1, %g2, %g3, %o2, %g4,
		TSBMISS_KHASHSZ, TSBMISS_KHASHSTART, %l6, %l5,
		vatopfn_l1, kvtop_nohblk,
		HATSTAT_KHASH_SEARCH, HATSTAT_KHASH_LINKS)

	/*
	 * i0 = vaddr
	 * g1 = tte
	 * g2 = tte pa
	 * g3 = tte va
	 * o2 = tsbmiss area
	 * i1 = hat id
	 */
	brgez,a,pn %g1, 6f			/* if tte invalid goto tl0 */
	  sub	%g0, 1, %i0			/* output = -1 (PFN_INVALID) */
	TTETOPFN(%g1, %i0, vatopfn_l2)		/* uses g1, g2, g3, g4 */
	/*
	 * i0 = vaddr
	 * g1 = pfn
	 */
	ba,pt	%icc, 6f
	  mov	%g1, %i0

kvtop_nohblk:
	/*
	 * we get here if we couldn't find valid hblk in hash.  We rehash
	 * if neccesary.
	 */
	ldn	[%o2 + (TSBMISS_SCRATCH + TSB_TAGACC)], %i0
	cmp	%l5, MAX_HASHCNT
	be,a,pn	%icc, 6f
	 sub	%g0, 1, %i0			/* output = -1 (PFN_INVALID) */
	mov	%i1, %o4			/* restore hatid */
	add	%l5, 1, %l5
	cmp	%l5, 2
	move	%icc, MMU_PAGESHIFT512K, %l6
	ba,pt	%icc, 1b
	  movne	%icc, MMU_PAGESHIFT4M, %l6
6:
	wrpr	%g0, %i4, %pstate		/* enable interrupts */
	ret
	restore
	SET_SIZE(sfmmu_vatopfn)
#endif /* lint */

#ifndef lint

/*
 * per cpu tsbmiss area to avoid cache misses in tsb miss handler.
 */
	.seg	".data"
	.align	64
	.global tsbmiss_area
tsbmiss_area:
	.skip	(TSBMISS_SIZE * NCPU)

#endif	/* lint */
