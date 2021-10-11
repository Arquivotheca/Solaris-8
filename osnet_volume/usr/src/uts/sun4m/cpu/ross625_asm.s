/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ross625_asm.s	1.18	99/04/13 SMI"

/*
 * Assembly language support for processor modules based on the
 * Ross Technology RT625 cache controller and memory management
 * unit and the RT620 CPU.
 */

#ifdef	lint
#include <sys/types.h>
#else	/* def lint */
#include "assym.h"
#endif	/* def lint */

#include <sys/machparam.h>
#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/psr.h>
#include <sys/trap.h>
#include <sys/devaddr.h>
#include <sys/async.h>
#include <sys/module_ross625.h>
#include <sys/machtrap.h>
#include <sys/intreg.h>
#include <sys/x_call.h>

/*
 * Local #defines
 */
#define	GET(val, r) \
	sethi	%hi(val), r; \
	ld	[r+%lo(val)], r

/*
 * Stolen from module_srmmu_asm.s
 *
 * BORROW_CONTEXT: temporarily set the context number
 * to that given in %o2.
 * 
 * NOTE: traps are disabled while we are in a borrowed context. It is
 * not possible to prove that the only traps that can happen while the
 * context is borrowed are safe to activate while we are in a borrowed
 * context (this includes random level-15 interrupts!).
 * 
 * %o0	flush/probe address [don't touch!]
 * %o2	context number to borrow
 * %o3	saved context number
 * %o4	psr temp / RMMU_CTX_REG
 * %o5	saved psr
 *
 * Note: No need to flush icache since the kernel exists in all contexts.
 */

#define BORROW_CONTEXT			\
	mov	%psr, %o5;		\
	andn	%o5, PSR_ET, %o4;	\
	mov	%o4, %psr;		\
	nop;nop;nop;			\
	set	RMMU_CTX_REG, %o4;	\
	lda	[%o4]ASI_MOD, %o3;	\
	sta	%o2, [%o4]ASI_MOD;

/*
 * RESTORE_CONTEXT: back out from whatever BORROW_CONTEXT did.
 * Assumes that two cycles of PSR DELAY follow.
 */
#define RESTORE_CONTEXT			\
	sta	%o3, [%o4]ASI_MOD;	\
	mov	%o5, %psr;		\
	nop;nop;nop


/*
 * IFLUSH - this bit of code flushes the icache
 */
#if defined(lint)

/*ARGSUSED*/
void
ross620_ic_flush(void)
{}

#else	/* lint */

	ENTRY(ross620_ic_flush)
	retl
	sta	%g0,[%g0]RT620_ASI_IC
	SET_SIZE(ross620_ic_flush)
#endif	/* def lint */

#ifdef lint
/*ARGSUSED*/
void
ross625_mmu_setctxreg(int ctx)
{}

#else	/* def lint */
	ENTRY(ross625_mmu_setctxreg)

        set     RMMU_CTX_REG, %o5       ! get srmmu context number
	lda	[%o5]ASI_MOD, %o1

	cmp	%o1, %o0		! compare to desired context
	bz	1f
	
        sta     %o0, [%o5]ASI_MOD	! flush icache, we are changing
					! icache here

        sta     %g0, [%g0]ASI_IBUF_FLUSH

! Need to keep track of the ctx per cpu for IFLUSH broadcast context matching
	CPU_INDEX(%o5)			! cpu number
	sll	%o5,2,%o5		! as an int index
	set	rt625_cpu_ctx,%o1	! into this array
	st	%o0, [%o5 + %o1]	! save the new context
	
1:      retl
        nop
	SET_SIZE(ross625_mmu_setctxreg)  
#endif	/* def lint */
 

/*
 * ross625_cache_init_asm
 *
 * Initialize the cache and leave it off.
 *
 * We may be entered with the cache turned on and in copyback mode.
 * If so, flush it and turn it off.  Do the same for the ICACHE.
 */
#ifdef	lint
void
ross625_cache_init_asm(void)
{}

#else	/* def lint */

	ENTRY(ross625_cache_init_asm)
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1
	btst	RT625_CTL_CE, %o1
	bz	1f
	nop
	!
	! cache is on, so flush it
	!
	GET(vac_nlines, %o2)
	GET(vac_linesize, %o3)
	mov	0, %o4
2:	sta	%g0, [%o4]ASI_FCC
	subcc	%o2, 1, %o2
	bnz	2b
	add	%o4, %o3, %o4

	!
	! turn cache off
	!
	! (%o0 == RMMU_CTL_REG, %o1 == current val)
1:	andn	%o1, RT625_CTL_CE, %o1
	sta	%o1, [%o0]ASI_MOD

	!
	! Clear the cache tags 
	!
	GET(vac_nlines, %o2)
	GET(vac_linesize, %o3)
	mov 	0, %o4
2:	sta	%g0, [%o4]RT625_ASI_CTAG
	subcc	%o2, 1, %o2
	bnz	2b
	add	%o4, %o3, %o4

	!
	! Turn off and clear icache
	!
	rd	RT620_ICCR, %o0
	andn	%o0, RT620_ICCR_ICE, %o0
	wr	%o0, RT620_ICCR
	retl
	sta	%g0, [%g0]RT620_ASI_IC
	SET_SIZE(ross625_cache_init_asm)

#endif	/* def lint */

/*
 * ross620_iccr_offon
 *
 * Modify bits in RT620 Instruction Cache Control Register
 */
#ifdef	lint
/*ARGSUSED*/
u_int
ross620_iccr_offon(u_int clrbits, u_int setbits)
{ return (0); }

#else	/* def lint */
	ENTRY(ross620_iccr_offon)
	rd	RT620_ICCR, %o2
	andn	%o2, %o0, %o2
	or	%o2, %o1, %o2
	wr	%o2, RT620_ICCR
	retl
	mov	%o2, %o0
	SET_SIZE(ross620_iccr_offon)
#endif	/* def lint */

/*
 * ross625_mmu_getasyncflt
 *
 * Get the asynchronous fault status registers.  On the RT625, we must
 * read the AFSR first, and then the AFAR.  Reading the AFAR unlocks
 * the AFSR.
 */
#ifdef	lint
 /*ARGSUSED*/
void
ross625_mmu_getasyncflt(u_int *ptr)
{}

#else	/* def lint */
	ENTRY(ross625_mmu_getasyncflt)
	set	RMMU_AFS_REG, %o1
	lda	[%o1]ASI_MOD, %o1
	st	%o1, [%o0]
	btst	AFSREG_AFV, %o1
	bz	1f
	set	RMMU_AFA_REG, %o1
	lda	[%o1]ASI_MOD, %o1
	st	%o1, [%o0+4]
1:
	set	-1, %o1
	retl
	st	%o1, [%o0+8]
	SET_SIZE(ross625_mmu_getasyncflt)
#endif	/* def lint */

/*
 * VAC routines
 */

/*
 * ross625_vac_allflush() flushes the entire cache.
 *
 */
#ifdef 	lint
/*ARGSUSED*/
void
ross625_vac_allflush(u_int flags)
{}
/*ARGSUSED*/
void
ross625_vac_ctxflush(int ctx, u_int flags)
{}
/*ARGSUSED*/
void
ross625_vac_rgnflush(caddr_t va, int ctx, u_int flags)
{}

#else	/* def lint */

	ENTRY(ross625_vac_allflush)
	! flush TLB always, no need to pay attention to flags
	ba	.common_flush
	mov	FT_ALL<<8, %g1

	ENTRY(ross625_vac_ctxflush)
	! flush TLB if flags has FL_TLB bit set
	andcc	%o1, FL_TLB,	%o1
	bz,a	.common_flush
	mov	0, %g1
	ba	.common_flush
	mov	FT_ALL<<8, %g1

	ENTRY(ross625_vac_rgnflush)
	! flush TLB if flags has FL_TLB bit set
	andcc	%o2, FL_TLB,	%o2
	bz,a	.common_flush
	mov	0, %g1
	ba	.common_flush
	mov	FT_ALL<<8, %g1

.common_flush:
	GET(vac_nlines, %o0)
	GET(vac_linesize, %o1)
	mov	0, %o2
1:	sta	%g0, [%o2]ASI_FCC
	subcc	%o0, 1, %o0
	bnz	1b
	add	%o2, %o1, %o2

	tst	%g1
	bnz,a	1f
	sta	%g0, [%g1]ASI_FLPR
1:
	retl
	nop
	SET_SIZE(ross625_vac_allflush)

#endif	/* def lint */

#ifdef	lint
/*
 * ross625_mmu_probe - the new probe routine
 * int ross625_mmu_probe(caddr_t probe_val, u_int *fsr) 
 *
 * Return a L3 looking PTE for virtual address probe_val. In the
 * case of failure, return 0 and the fsr in *fsr (if non-null)
 *
 * Algorithm is:
 * 1) Perform a probe on the indicated value.
 * 2) If there's a table walk error, then save the sfsr if desired and
 *    return 0.
 * 3) Look at the PTE entry itself, if it has any bits on in the part
 *    of the address that is the offset in a L2 page table
 *    (0x000003f0 = MMU_L2_BITS>>4), then it can't be other than a L3
 *    PTE because the physical page for an L2 or L1 mapping must also
 *    have these bits 0.
 * 4) Now try a second probe of the probe_val address, but with the L2
 *    offset bits set to 0.
 * 5) If this produces a PTE that's different than the first probe, then
 *    the probe_val is a L3 mapping.
 * 6) Again look at the PTE entry itself, if it has any bits on in the part
 *    of the address that is the offset in a L1 page table
 *    (0x0000FFF0 = MMU_L1_BITS>>4), then it can't be other than a L2
 *    PTE because the physical page for an L1 mapping must also
 *    have these bits 0.
 * 7) Otherwise, we need one further probe with both the L1 and L2 bits
 *    off to see whether it's a L1 or L2 mapping.
 * 8) If this probe produced a value different than the first probe, then
 *    it's a L2 mapping, otherwise its a L1 mapping.
 * 9) Mask off the appropriate bits in the probe_val and add those pages
 *    into the pte (watch out for the 4 bit shift from the address to the
 *    ppn in the PTE.
 */
/*ARGSUSED*/
int
ross625_mmu_probe(caddr_t probe_val, u_int *fsr)
{return (0);}

#else	/* def lint */

	! Register usage
	!
	! o0 = probe_val, then & ~FFF and | FT_ALL << 8
	! o1 = *fsr, save fsr location - could be 0
	! o2 = scratch
	! o3 = PTE from third probe (L1 + L2 bits off)
	! o4 = PTE from second probe (L2 bits off)
	! o5 = PTE from first probe

	ENTRY(ross625_mmu_probe)

	! mask out page offset bits and or in values needed for probing
	! these won't get in our way with careful masking

	and	%o0, MMU_PAGEMASK, %o0
	or	%o0, FT_ALL<<8, %o0
	lda	[%o0]ASI_FLPR, %o5	! Probe all
	cmp	%o5, 0			! If invlid we are done
	bz,a	probe_done
	mov	%g0, %o0


	! check the PTE's bits for the L2 offset. If these are non-zero,
	! then the page can't be other than a L3 page

	set	MMU_L2_BITS, %o2
	srl	%o5, 8, %o4			! Get page frame number from PTE
	sll	%o4, MMU_STD_PAGESHIFT, %o4	! And the physical address
	andcc	%o2, %o4, %g0			! L2 PTE?
	bne,a	probe_done			! No, it is a L3 PTE
	mov	%o5, %o0

	! now construct an address with the Level 2 offset set to 0
	! and probe for a level2 PTE.

  	set	MMU_L2_MASK, %o4
	and	%o0, %o4, %o4
	or	%o4, FT_ALL<<8, %o4
	lda	[%o4]ASI_FLPR, %o4

	! Is this the same as the first probe. If so, then the probe_val
	! is for a page that is a L2 or a L1 mapping. Cannot compare PTE
	! to PTE because the reference bit gets set on the first probe

	srl	%o5, 8, %o2		! page frame num from PTE for 1st probe
	srl	%o4, 8, %o3		! page frame num from PTE for 2nd probe
	cmp	%o2, %o3		! Same page frame numbers?

	bne,a	probe_done		! No, must be a level 3 mapping
	mov	%o5, %o0

	! check the bits for the L1 offset. If these are non-zero,
	! then the page can't be other than a L2 page

1:	set	MMU_L1_BITS,  %o2
	srl	%o5, 8, %o4			! page frame number from PTE
	sll	%o4, MMU_STD_PAGESHIFT, %o4	! And the physical address
	andcc	%o2, %o4, %g0
	bnz	.is_l2				! Level2 PTE
	nop

	! now construct an address with the Level 1, 2 offset set to 0
	! and probe that

  	set	MMU_L1_MASK, %o3
	and	%o0, %o3, %o3
	or	%o3, FT_ALL<<8, %o3
	lda	[%o3]ASI_FLPR, %o3

	! Is the PTE from this probe the same as the first probe?

	srl	%o5, 8, %o2		! Get the page frame number from PTE
	srl	%o3, 8, %o4		! Get the page frame number from PTE
	cmp	%o4, %o2
	beq	.is_l1
	nop

	! No - then this must be a L2 mapping
	! set the mask to look at the L2 offset bits

.is_l2:
	set	MMU_L2_BITS, %o2
2:
	and	%o0, %o2, %o0

	srl	%o0, 4, %o0	! Correct for the PTE vs page offset 
	ba	probe_done
	add	%o5, %o0, %o0	! add offset to the first PTE's probe value

	! If the two PTE's are the same, then this must be a L1 mapping
	! set up to mask the L1 offset bits

.is_l1:	set	MMU_L1_BITS, %o2
	ba,a	2b

probe_done:
	set	RMMU_FSR_REG, %o2 	! and read the sfsr
	cmp	%o1, 0			! Look at *fsr, if non-null
	be	1f			! store fsr in the pointer passed to us
	lda	[%o2]ASI_MOD, %o2	! clear fsr
	st	%o2, [%o1]		! return fsr
	
1:	retl
	nop
	SET_SIZE(ross625_mmu_probe)

#endif	/* def lint */

#ifdef lint
/*ARGSUSED*/
void
ross625_noxlate_pgflush(caddr_t va, u_int pfn, int cxn)
{}
/*
 * ross625 implementation of vac_color_flush.
 * Use segment flush asi so that a tlb translation is not required.
 * Check tag for each line of page; flush on hit.
 *
 * If valid context is specified, flush TLB entry.
 */
#else	/* def lint */
        ENTRY(ross625_noxlate_pgflush)

	cmp	%o2, -1			! No context - skip TLB flush
	be	0f
	.empty

	BORROW_CONTEXT
	or	%o0, FT_PAGE<<8, %g1	! TLB flush
	sta	%g0, [%g1]ASI_FLPR
	RESTORE_CONTEXT

0:
	set     PAGESIZE, %o3
	set     vac_linesize, %o2
	ld      [%o2], %o2
1:
	lda	[%o0]ASI_DCT, %o4	! get line tag entry
	srl	%o4, 8, %o4		! tag only
	cmp	%o4, %o1		! flush on tag match
	beq,a	2f
	sta     %g0, [%o0]ASI_FCS
2:
	subcc   %o3, %o2, %o3
	bne     1b
	add     %o0, %o2, %o0

	retl
	nop
	SET_SIZE(ross625_noxlate_pgflush)
#endif /* def lint */

/*
 * ross625_xcall_medpri - HyperSPARC tlb & page flush cross-call
 *
 * Handles medium priority (level13) cross-calls in the trap window.
 */
#if defined(lint)
void
ross625_xcall_medpri(void)
{ }
#else /* lint */
	ENTRY_NP(ross625_xcall_medpri)

	/*
	 * NOTE: traps disabled for the entire function
	 *
	 * top of function register usage
	 * %l7 = cpu's interrupt reg pointer
	 * %l6 = scratch
	 * %l5 = cpu index (word offset) then xc_mboxes
	 * %l4 = scratch
	 * %l3 = scratch then cpu pointer
	 * %l2 = saved npc
	 * %l1 = saved pc
	 * %l0 = saved psr
	 */

	CPU_INDEX(%l5)			! get CPU number in l5
	sll	%l5, 2, %l5		! make word offset
	set	v_interrupt_addr, %l7	! base of #cpu ptrs to interrupt regs
	add	%l7, %l5, %l7		! ptr to our ptr
	ld	[%l7], %l7		! %l4 = &cpu's intr pending reg
	ld	[%l7], %l6		! %l4 = interrupts pending
	set	IR_SOFT_INT(13), %l4	! Soft interrupts are in bits <17-31>
	andcc	%l4, %l6, %g0		! Is soft level 13 interrupt pending?
	bz,a	interrupt_prologue	! No, let interrupt_prologue service it
	mov	13, %l4			! pass the intr # in %l4

	!! Check for x-call
	set	cpu, %l4
	ld	[%l4 + %l5], %l3	! read cpu[cpuindex]

        ! if (cpup->cpu_m.xc_pend[X_CALL_MEDPRI] == 0)
	!    goto interrupt_prologue
	ld	[%l3 + XC_PEND_MEDPRI], %l4
	tst	%l4
	bz,a	interrupt_prologue
	mov	13, %l4			! pass the intr # in %l4

	!! Can not use fast-path during CAPTURE/RELEASE protocol
	sethi	%hi(doing_capture_release), %l4
	ld	[%l4 + %lo(doing_capture_release)], %l4
	tst	%l4
	bnz	interrupt_prologue	! Yes, go to interrupt_prologue
	mov	13, %l4			! pass the intr # in %l4

        ! if (cpup->cpu_m.xc_state[X_CALL_MEDPRI] == XC_DONE)
	!    goto interrupt_prologue
	ld	[%l3 + XC_STATE_MEDPRI], %l4
	cmp	%l4, XC_DONE
	beq,a	interrupt_prologue
	mov	13, %l4			! pass the intr # in %l4

	! if (xc_mboxes[X_CALL_MEDPRI].func == tlbflush), handle here
	set	xc_mboxes, %l5
	ld	[%l5 + XC_MBOX_FUNC_MEDPRI], %l4
	set	srmmu_mmu_flushpagectx, %l6
	cmp	%l4, %l6
	bne	2f
	nop

	/*
	 * srmmu_mmu_flushpagectx(caddr_t vaddr, u_int ctx)
	 */

	!! Clear softint condition, we're handling this one
	set	IR_SOFT_INT(13), %l4	! Soft interrupts are in bits <17-31>
	st	%l4, [%l7 + 4]		! Clear soft level13
	ld	[%l7], %g0		! Wait for change to propate through ?

	/*
	 * Updated register usage until after RESTORE CONTEXT is finished
	 * %l7 = ctx
	 * %l6 = vaddr
	 * %l5 = xc_mboxes/RMMU_CTX_REG
	 * %l4 = saved ctx
	 * %l3 = cpu pointer
	 */

	ld	[%l5 + XC_MBOX_ARG1_MEDPRI], %l6	/* vaddr */
	ba	.ross625_flush_common
	or      %l6, FT_PAGE<<8, %l6


	/*
	 * srmmu_mmu_flushrgn(caddr_t addr, u_int cxn)
	 */
2:
	set	srmmu_mmu_flushrgn, %l6
	cmp	%l4, %l6
	bne	3f
	nop

.ross625_flushrgn:
	!! Clear softint condition, we're handling this one
	set	IR_SOFT_INT(13), %l4	! Soft interrupts are in bits <17-31>
	st	%l4, [%l7 + 4]		! Clear soft level13
	ld	[%l7], %g0		! Wait for change to propate through ?

	ld	[%l5 + XC_MBOX_ARG1_MEDPRI], %l6	/* vaddr */
	ba	.ross625_flush_common
	or      %l6, FT_RGN<<8, %l6


	/*
	 * srmmu_mmu_flushseg(caddr_t addr, u_int cxn)
	 */
3:
	set	srmmu_mmu_flushseg, %l6
	cmp	%l4, %l6
	bne	4f
	nop

	!! Clear softint condition, we're handling this one
	set	IR_SOFT_INT(13), %l4	! Soft interrupts are in bits <17-31>
	st	%l4, [%l7 + 4]		! Clear soft level13
	ld	[%l7], %g0		! Wait for change to propate through ?

	ld	[%l5 + XC_MBOX_ARG1_MEDPRI], %l6	/* vaddr */
	or      %l6, FT_SEG<<8, %l6

.ross625_flush_common:
	ld	[%l5 + XC_MBOX_ARG2_MEDPRI], %l7	/* ctx */

	!! BORROW_CONTEXT
	set     RMMU_CTX_REG, %l5
	lda     [%l5]ASI_MOD, %l4	! Stash away current context
	sta     %l7, [%l5]ASI_MOD	! Switch to new context

	!! FLUSH
	sta     %g0, [%l6]ASI_FLPR

	!! RESTORE_CONTEXT
	ba	.ross625_xcall_finish
	sta     %l4, [%l5]ASI_MOD	! restore previous context

	!! FINISHED, goto .ross625_xcall_finish


	/*
	 * srmmu_mmu_flushctx(int cxn)
	 */
4:
	set	srmmu_mmu_flushctx, %l6
	cmp	%l4, %l6
	bne	5f
	nop

.ross625_flushctx:
	!! Clear softint condition, we're handling this one
	set	IR_SOFT_INT(13), %l4	! Soft interrupts are in bits <17-31>
	st	%l4, [%l7 + 4]		! Clear soft level13
	ld	[%l7], %g0		! Wait for change to propate through ?

	set	FT_CTX<<8, %l6
	ld	[%l5 + XC_MBOX_ARG1_MEDPRI], %l7	/* ctx */

	!! BORROW_CONTEXT
	set     RMMU_CTX_REG, %l5
	lda     [%l5]ASI_MOD, %l4	! Stash away current context
	sta     %l7, [%l5]ASI_MOD	! Switch to new context

	!! FLUSH
	sta     %g0, [%l6]ASI_FLPR

	cmp	%l4, %l7		! if we're in the context we're flushing
	beq,a	4f			! then change to KCONTEXT
	mov	KCONTEXT, %l4
4:
	!! RESTORE_CONTEXT
	ba	.ross625_xcall_finish
	sta     %l4, [%l5]ASI_MOD	! restore previous context
	!! FINISHED, goto .ross625_xcall_finish

	/*
	 * ross625_noxlate_pgflush(caddtr_t va, u_int pfn, int cxn)
	 */

5:
	! if (xc_mboxes[X_CALL_MEDPRI].func != pgtlbflush)
	!    goto interrupt_prologue
	set	ross625_noxlate_pgflush, %l6
	cmp	%l4, %l6
	bne,a	interrupt_prologue
	mov	13, %l4			! pass the intr # in %l4

	!! Clear softint condition, we're handling this one
	set	IR_SOFT_INT(13), %l4	! Soft interrupts are in bits <17-31>
	st	%l4, [%l7 + 4]		! Clear soft level13
	ld	[%l7], %g0		! Wait for change to propate through ?

	! Use cpu_m.xc_pend[X_CALL_MEDPRI] field as storage for %l0 (%psr)
	st	%l0, [%l3 + XC_PEND_MEDPRI]

	/*
	 * Updated register usage until x_call wrapup
	 * %l7 = ctx/pfn
	 * %l6 = vaddr
	 * %l5 = xc_mboxes/scratch
	 * %l4 = saved ctx/linesize
	 * %l3 = cpu pointer
	 * %l0 = RMMU_CTX_REG/scratch
	 */

	!! Skip TLB flush if no ctx
	ld	[%l5 + XC_MBOX_ARG3_MEDPRI], %l7	/* ctx */
	cmp	%l7, -1			! NO_CTX
	be	0f
	ld	[%l5 + XC_MBOX_ARG1_MEDPRI], %l6	/* vaddr */

	!! BORROW_CONTEXT
	set     RMMU_CTX_REG, %l0
	lda     [%l0]ASI_MOD, %l4	! Stash away current context
	sta     %l7, [%l0]ASI_MOD	! Switch to new context

	!! FLUSH
	or      %l6, FT_PAGE<<8, %l7
	sta     %g0, [%l7]ASI_FLPR

	!! RESTORE_CONTEXT
	sta     %l4, [%l0]ASI_MOD	! restore previous context

0:
	ld	[%l5 + XC_MBOX_ARG2_MEDPRI], %l7	/* pfn */
	set	PAGESIZE, %l5		! MMU pagesize
	set	vac_linesize, %l4
	ld	[%l4], %l4		! size of processor cache line
1:
	lda	[%l6]ASI_DCT, %l0	! get line tag entry
	srl	%l0, 8, %l0		! tag only
	cmp	%l0, %l7		! flush on pfn tag match
	beq,a	0f
	sta	%g0, [%l6]ASI_FCS
0:
	subcc	%l5, %l4, %l5
	bne	1b
	add	%l6, %l4, %l6

	! Restore %l0 (%psr) value
	ld	[%l3 + XC_PEND_MEDPRI], %l0

	!! FINISHED, goto .ross625_xcall_finish

.ross625_xcall_finish:
	/*
	 * Finish the x-call protocol
	 */
	
	! cpup->cpu_m.xc_pend[X_CALL_MEDPRI] = 0
	st	%g0, [%l3 + XC_PEND_MEDPRI]

	! cpup->cpu_m.xc_retval[X_CALL_MEDPRI] = 0
	st	%g0, [%l3 + XC_RETVAL_MEDPRI]

	! cpup->cpu_m.xc_ack[X_CALL_MEDPRI] = 1
	mov	1, %l4
	st	%l4, [%l3 + XC_ACK_MEDPRI]

	!! finished, go home
	mov	%l0, %psr
	nop; nop; nop
	jmp	%l1
	rett	%l2

	SET_SIZE(ross625_xcall_medpri)
#endif /* !defined(lint) */

/*
 * ross625_diag()
 *
 * Determine processor implementation.  Silicon rev levels
 * before B.0 have value 0.  B.0 & B.1 silicon (at present)
 * have value 1.
 */
#if defined(lint)
int
ross625_diag(void)

{ return (0); }

#else /* lint */

	ENTRY_NP(ross625_diag)
	retl
	rd	%asr30, %o0
	SET_SIZE(ross625_diag)
#endif /* def lint */

/* 
 * SPECIAL NOTE:
 *
 * In the following routines:
 *
 *	ross625_vac_pageflush
 *	ross625_vac_flush
 *	ross625_vac_segflush
 *
 * It is necessary that a TLB entry exist for the page to be flushed. A
 * probe is performed in order to ensure that this TLB entry will exist.
 * This will create a new TLB entry if the page is not currently in the
 * TLB; this is not what we're worried about however. It's the following
 * case that we're worried about:
 *
 *	1) The page to be flushed occupies the next TLB to be replaced.
 *	   (The Ross 625 uses a circular pointer for TLB replacement and
 *	   this makes it random as far as the software can tell).
 *	2) The probe is done successfully, using the already existing TLB
 *	   entry.
 *	3) While executing the code, we cross a page boundary (256k in this
 *	   case) and cause a new TLB to be allocated. This will be the
 *	   TLB for the page to be flushed.
 *	4) When we try the page flush, we'll try to take a trap, but traps
 *	   are off and we'll watchdog.
 *
 * To fix this, we need to make sure that 3) above does not happen. That is,
 * that we don't cross this page boundary while executing one of these
 * subroutines. To do that, we'll have TWO copies of each of these
 * routines, with an additional _alt appended to the end. The module
 * initialize code will use the alternate set if it finds that the first
 * and second set of routines cross a level 2 page boundary (256k).
 * This, of course, assumes that the kernel is mapped with L2 PTEs.
 *
 * The first and second set of routines are identified by the labels:
 * 	ross625_vac_sec and ross625_vac_set_alt
 */


#ifdef	lint
void
ross625_vac_set(void)
{}

#else	/* def lint */
	ENTRY(ross625_vac_set)
	mov	0, %g1

	mov	%o1, %o2	! Copy of the context num for BORROW CONTEXT
        BORROW_CONTEXT

	!
        ! The RT625 needs a correct mapping for page-based flushes
 	!
        and     %o0, MMU_PAGEMASK, %o2                  ! virtual page number
        or      %o2, FT_ALL<<8, %o2                     ! match criteria
        lda     [%o2]ASI_FLPR, %o2
        set     RMMU_FSR_REG, %o1			! clear sfsr else
        lda     [%o1]ASI_MOD, %g0			! could cause problems
        and     %o2, PTE_ETYPEMASK, %o2
        cmp     %o2, MMU_ET_PTE
        bne     .rt625_vac_set_exit
        nop

        set     PAGESIZE, %o1

	set	vac_linesize, %o2
	ld	[%o2], %o2
1:      subcc   %o1, %o2, %o1
        sta     %g0, [%o0]ASI_FCP
        bne     1b
        add     %o0, %o2, %o0

.rt625_vac_set_exit:
	tst	%g1
	bnz,a	1f
	sta	%g0, [%g1]ASI_FLPR
1:
        RESTORE_CONTEXT
        retl
        nop
	SET_SIZE(ross625_vac_set)
#endif	/* def lint */


/*
 * ross625_vac_pageflush() flushes the cache for page based at _va_
 * in context _cxn_. If flags has FL_TLB bit set, flush TLB as well.
 *
 */
#ifdef	lint
/*ARGSUSED*/
void
ross625_vac_pageflush(caddr_t va, u_int cxn, u_int flags)
{}

#else	/* def lint */
	ENTRY(ross625_vac_pageflush)

	! flush TLB if flags has FL_TLB bit set
	andcc	%o2, FL_TLB,	%o2
	bz,a	0f
	mov	0, %g1
	or	%o0, FT_PAGE<<8, %g1
0:
	mov	%o1, %o2	! Copy of the context num for BORROW CONTEXT
        BORROW_CONTEXT

	!
        ! The RT625 needs a correct mapping for page-based flushes
 	!
        and     %o0, MMU_PAGEMASK, %o2                  ! virtual page number
        or      %o2, FT_ALL<<8, %o2                     ! match criteria
        lda     [%o2]ASI_FLPR, %o2
        set     RMMU_FSR_REG, %o1			! clear sfsr else
        lda     [%o1]ASI_MOD, %g0			! could cause problems
        and     %o2, PTE_ETYPEMASK, %o2
        cmp     %o2, MMU_ET_PTE
        bne     .rt625_pageflush_exit
        nop

        set     PAGESIZE, %o1
	GET(vac_linesize, %o2)
 
1:      subcc   %o1, %o2, %o1
        sta     %g0, [%o0]ASI_FCP
        bne     1b
        add     %o0, %o2, %o0
 
.rt625_pageflush_exit:
	tst	%g1
	bnz,a	1f
	sta	%g0, [%g1]ASI_FLPR
1:
        RESTORE_CONTEXT
 
        retl
        nop
	SET_SIZE(ross625_vac_pageflush)
#endif	/* def lint */

/*
 * ross625_vac_flush
 *
 * Flush cache for virtual address _va_ and length _len_.
 *
 * XXX Currently not very efficient.  Is doing a probe on each line
 *     flushed.  Should be optimized if profiling reveals it's worth it.
 *
 * Note: The lint definition below is lying somewhat...We also get
 *       the current context number in %o2.
 */
#ifdef	lint
/*ARGSUSED*/
void
ross625_vac_flush(caddr_t va, int len)
{}

#else	/* def lint */
	ENTRY(ross625_vac_flush)

	set	RMMU_CTX_REG, %o4		! Load the context number
	lda	[%o4]ASI_MOD, %o2		! for borrow context
        BORROW_CONTEXT
	GET(vac_linesize, %g1)
	mov	%g1, %o2
	sub	%g1, 1, %g1
        and     %o0, %g1, %g1			! figure align error on start
        sub     %o0, %g1, %o0                   ! push start back that much
        add     %o1, %g1, %o1                   ! add it to size

1:
	mov	%o0, %g1
	and	%g1, MMU_PAGEMASK, %g1
	or	%g1, FT_ALL<<8, %g1
	lda	[%g1]ASI_FLPR, %g1
	and	%g1, PTE_ETYPEMASK, %g1
	cmp	%g1, MMU_ET_PTE
        set     RMMU_FSR_REG, %g1                       ! clear sfsr else
	bne	2f
        lda     [%g1]ASI_MOD, %g0                       ! could cause problems

        sta     %g0, [%o0]ASI_FCP

2:
     	subcc   %o1, %o2, %o1
        bcc     1b
        add     %o0, %o2, %o0


        RESTORE_CONTEXT
        retl
        nop
	SET_SIZE(ross625_vac_flush)

#endif	/* def lint */

#ifdef lint
/*
 * ross625_vac_segflush(caddr_t vaddr, u_int cxn, int flags)
 * 
 * ross625_vac_segflush() flushes data for segment based at _vaddr_ in
 * context _cxn_. If flags has FL_TLB bit set, flush TLB as well.
 *
 * The RT625 can't do a segment-based, region-based or context-based
 * flush.  For the region and context flushes, we punt and do a
 * ross625_vac_allflush().  For the segment flush, do through all
 * the pages in a segment, flushing all the lines for valid pages.
 *
 */
void
/*ARGSUSED*/
ross625_vac_segflush(caddr_t vaddr, u_int cxn, int flags)
{}

#else /* def lint */

! Register usage
! o0 = vaddr to flush, moves up as we flush/probe
! o1 = temp: PAGESIZE -> 0 during page flush
! o2 = probe results
! o3 = context to restore, don't touch
! o4 = addr of context register, don't touch
! o5 = psr to restore, don't touch
!
! g1 = 0/1 to do TLB flush or not
! g2 = vac_linesize
! g3 = PAGESIZE
! g4 = number of pages left in segment
!
	ENTRY(ross625_vac_segflush)

	! flush TLB if flags has FL_TLB bit set
	andcc	%o2, FL_TLB,	%o2
	bz,a	0f
	mov	0, %g1
	mov	1,%g1			! setup %g1 for later use
0:

	mov	%o1, %o2	! Copy of the context num for BORROW CONTEXT
	BORROW_CONTEXT

	set	1<<(MMU_STD_SEGSHIFT-MMU_STD_PAGESHIFT),%g4 ! # pages/seg
  	GET(vac_linesize,%g2)		! size of each vac line
	set	PAGESIZE, %g3		! PAGESIZE in g3

1:	or	%o0, FT_ALL<<8, %o2	! probe the va
	lda	[%o2]ASI_FLPR, %o2

	set	RMMU_FSR_REG,%o1	! get fsr to avoid problems
	lda	[%o1]ASI_MOD, %g0

	and	%o2, PTE_ETYPEMASK, %o2	! valid probe?
	cmp	%o2, MMU_ET_PTE
	be,a	2f			! yes - flush lines for page
	mov	%g3,%o1			! delay slot - size of area to flush

	subcc	%g4, 1, %g4		! more pages in segment?
	bnz	1b			! yes
	add	%o0, %g3, %o0		! on to next page

	RESTORE_CONTEXT			! otherwise all done
	retl
	nop

2:	subcc	%o1, %g2, %o1		! for all lines in page
	sta	%g0, [%o0]ASI_FCP	! flush the line
	bne	2b
	add	%o0, %g2, %o0		! next line

	tst	%g1			! want a TLB flush?
	bz	3f			! no - skip the TLB flush
	nop
	sub	%o0, %g3, %o0		! we're one page past good
	sta	%g0,[%o0]ASI_FLPR	! page flush for current page
	add	%o0, %g3, %o0

3:	subcc	%g4, 1, %g4		! more pages in seg?
	bnz	1b			! yes
	nop

	RESTORE_CONTEXT
	retl
	nop
#endif


/*
 *	START OF SECOND SET OF VAC PAGE FLUSHING CODE
 *
 *	WARNING - DON'T MOVE THIS!
 */

/*
 *
 */
#ifdef	lint
/*ARGSUSED*/
void
ross625_vac_set_alt(caddr_t va, u_int cxn)
{}

#else	/* def lint */
	ENTRY(ross625_vac_set_alt)

	mov	0, %g1	
	mov	%o1, %o2	! Copy of the context num for BORROW CONTEXT
        BORROW_CONTEXT

	!
        ! The RT625 needs a correct mapping for page-based flushes
 	!
        and     %o0, MMU_PAGEMASK, %o2                  ! virtual page number
        or      %o2, FT_ALL<<8, %o2                     ! match criteria
        lda     [%o2]ASI_FLPR, %o2
        set     RMMU_FSR_REG, %o1			! clear sfsr else
        lda     [%o1]ASI_MOD, %g0			! could cause problems
        and     %o2, PTE_ETYPEMASK, %o2
        cmp     %o2, MMU_ET_PTE
        bne     .rt625_vac_set_exit_alt
        nop

        set     PAGESIZE, %o1

	set	vac_linesize, %o2
	ld	[%o2], %o2
1:      subcc   %o1, %o2, %o1
        sta     %g0, [%o0]ASI_FCP
        bne     1b
        add     %o0, %o2, %o0

.rt625_vac_set_exit_alt:
	tst	%g1
	bnz,a	1f
	sta	%g0, [%g1]ASI_FLPR
1:
        RESTORE_CONTEXT
        retl
        nop
	SET_SIZE(ross625_vac_set_alt)
#endif	/* def lint */


/*
 * ross625_vac_pageflush_alt() flushes the cache for page based at _va_
 * in context _cxn_.
 */
#ifdef	lint
/*ARGSUSED*/
void
ross625_vac_pageflush_alt(caddr_t va, u_int cxn, u_int flags)
{}

#else	/* def lint */
	ENTRY(ross625_vac_pageflush_alt)

	! flush TLB if flags has FL_TLB bit set
	andcc	%o2, FL_TLB,	%o2
	bz,a	0f
	mov	0, %g1
	or	%o0, FT_PAGE<<8, %g1
0:
	mov	%o1, %o2	! Copy of the context num for BORROW CONTEXT
        BORROW_CONTEXT

	!
        ! The RT625 needs a correct mapping for page-based flushes
 	!
        and     %o0, MMU_PAGEMASK, %o2                  ! virtual page number
        or      %o2, FT_ALL<<8, %o2                     ! match criteria
        lda     [%o2]ASI_FLPR, %o2
        set     RMMU_FSR_REG, %o1			! clear sfsr else
        lda     [%o1]ASI_MOD, %g0			! could cause problems
        and     %o2, PTE_ETYPEMASK, %o2
        cmp     %o2, MMU_ET_PTE
        bne     .rt625_pageflush_exit_alt
        nop

        set     PAGESIZE, %o1
	GET(vac_linesize, %o2)
 
1:      subcc   %o1, %o2, %o1
        sta     %g0, [%o0]ASI_FCP
        bne     1b
        add     %o0, %o2, %o0
 
.rt625_pageflush_exit_alt:
	tst	%g1
	bnz,a	1f
	sta	%g0, [%g1]ASI_FLPR
1:
        RESTORE_CONTEXT
 
        retl
        nop
	SET_SIZE(ross625_vac_pageflush_alt)
#endif	/* def lint */

/*
 * ross625_vac_flush_alt
 *
 * Flush cache for virtual address _va_ and length _len_.
 *
 * XXX Currently not very efficient.  Is doing a probe on each line
 *     flushed.  Should be optimized if profiling reveals it's worth it.
 */
#ifdef	lint
/*ARGSUSED*/
void
ross625_vac_flush_alt(caddr_t va, int len)
{}

#else	/* def lint */
	ENTRY(ross625_vac_flush_alt)

        BORROW_CONTEXT
	GET(vac_linesize, %g1)
	mov	%g1, %o2
	sub	%g1, 1, %g1
        and     %o0, %g1, %g1			! figure align error on start
        sub     %o0, %g1, %o0                   ! push start back that much
        add     %o1, %g1, %o1                   ! add it to size

1:
	mov	%o0, %g1
	and	%g1, MMU_PAGEMASK, %g1
	or	%g1, FT_ALL<<8, %g1
	lda	[%g1]ASI_FLPR, %g1
	and	%g1, PTE_ETYPEMASK, %g1
	cmp	%g1, MMU_ET_PTE
        set     RMMU_FSR_REG, %g1                       ! clear sfsr else
	bne	2f
        lda     [%g1]ASI_MOD, %g0                       ! could cause problems

        sta     %g0, [%o0]ASI_FCP

2:
     	subcc   %o1, %o2, %o1
        bcc     1b
        add     %o0, %o2, %o0

        RESTORE_CONTEXT
        retl
        nop
	SET_SIZE(ross625_vac_flush_alt)

#endif	/* def lint */

#ifdef lint
/*
 * ross625_vac_segflush_alt(caddr_t vaddr, u_int cxn, u_int flags)
 * 
 * ross625_vac_segflush_alt() flushes data for segment based at _vaddr_ in
 * context _cxn_.
 *
 * The RT625 can't do a segment-based, region-based or context-based
 * flush.  For the region and context flushes, we punt and do a
 * ross625_vac_allflush().  For the segment flush, do through all
 * the pages in a segment, flushing all the lines for valid pages.
 *
 */
void
/*ARGSUSED*/
ross625_vac_segflush_alt(caddr_t vaddr, u_int cxn, u_int flags);

#else /* def lint */

! Register usage
! o0 = vaddr to flush, moves up as we flush/probe
! o1 = temp: PAGESIZE -> 0 during page flush
! o2 = probe results
! o3 = context to restore, don't touch
! o4 = addr of context register, don't touch
! o5 = psr to restore, don't touch
!
! g1 = 0/1 to do TLB flush or not
! g2 = vac_linesize
! g3 = PAGESIZE
! g4 = number of pages left in segment
!
	ENTRY(ross625_vac_segflush_alt)

	! flush TLB if flags has FL_TLB bit set
	andcc	%o2, FL_TLB,	%o2
	bz,a	0f
	mov	0, %g1
	mov	1,%g1			! setup %g1 for later use
0:
	mov	%o1, %o2	! Copy of the context num for BORROW CONTEXT
	BORROW_CONTEXT

	set	1<<(MMU_STD_SEGSHIFT-MMU_STD_PAGESHIFT),%g4 ! # pages/seg
  	GET(vac_linesize,%g2)		! size of each vac line
	set	PAGESIZE, %g3		! PAGESIZE in g3

1:	or	%o0, FT_ALL<<8, %o2	! probe the va
	lda	[%o2]ASI_FLPR, %o2

	set	RMMU_FSR_REG,%o1	! get fsr to avoid problems
	lda	[%o1]ASI_MOD, %g0

	and	%o2, PTE_ETYPEMASK, %o2	! valid probe?
	cmp	%o2, MMU_ET_PTE
	be,a	2f			! yes - flush lines for page
	mov	%g3,%o1			! delay slot - size of area to flush

	subcc	%g4, 1, %g4		! more pages in segment?
	bnz	1b			! yes
	add	%o0, %g3, %o0		! on to next page

	RESTORE_CONTEXT			! otherwise all done
	retl
	nop

2:	subcc	%o1, %g2, %o1		! for all lines in page
	sta	%g0, [%o0]ASI_FCP	! flush the line
	bne	2b
	add	%o0, %g2, %o0		! next line

	tst	%g1			! want a TLB flush?
	bz	3f			! no - skip the TLB flush
	nop
	sub	%o0, %g3, %o0		! we're one page past good
	sta	%g0,[%o0]ASI_FLPR	! page flush for current page
	add	%o0, %g3, %o0

3:	subcc	%g4, 1, %g4		! more pages in seg?
	bnz	1b			! yes
	nop

	RESTORE_CONTEXT
	retl
	nop
#endif
/*
 *	END OF SECOND SET OF VAC PAGE FLUSHING CODE
 */

