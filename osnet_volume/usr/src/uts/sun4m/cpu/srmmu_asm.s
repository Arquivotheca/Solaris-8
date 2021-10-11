/*
 * Copyright (c) 1991, 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)srmmu_asm.s	1.23	97/05/24 SMI"

/*
 * Interface to flexible module routines
 */

#if defined(lint)
#include <sys/types.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/machparam.h>
#include <sys/machthread.h>
#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/psr.h>
#include <sys/trap.h>
#include <sys/devaddr.h>

#if defined(lint)

int
srmmu_mmu_getcr(void)
{ return (0); }

int
srmmu_mmu_getctp(void)
{ return (0); }

int
srmmu_mmu_getctx(void)
{ return (0); }

int
srmmu_mmu_probe(void)
{ return (0); }

/* ARGSUSED */
void
srmmu_mmu_setcr(int v)
{}

/* ARGSUSED */
void
srmmu_mmu_setctxreg(int v)
{}

void
srmmu_mmu_flushall(void)
{}

void
srmmu_mmu_handle_ebe(void)
{}

#else	/* lint */

	ENTRY(srmmu_mmu_getcr)
	set	RMMU_CTL_REG, %o1	! get srmmu control register
	retl
	lda	[%o1]ASI_MOD, %o0
        SET_SIZE(srmmu_mmu_getcr)

	ENTRY(srmmu_mmu_getctp)
	set	RMMU_CTP_REG, %o1	! get srmmu context table ptr
	retl
	lda	[%o1]ASI_MOD, %o0
        SET_SIZE(srmmu_mmu_getctp)

	ENTRY(srmmu_mmu_getctx)
	set	RMMU_CTX_REG, %o1	! get srmmu context number
	retl
	lda	[%o1]ASI_MOD, %o0
        SET_SIZE(srmmu_mmu_getctx)

	ENTRY(srmmu_mmu_probe)
	and	%o0, MMU_PAGEMASK, %o0	! virtual page number
	or	%o0, FT_ALL<<8, %o0	! match criteria
	lda	[%o0]ASI_FLPR, %o0	! do the probe
	set	RMMU_FSR_REG, %o1	! setup to clear fsr
	retl
	lda	[%o1]ASI_MOD, %o1	! clear fsr
        SET_SIZE(srmmu_mmu_probe)

	ENTRY(srmmu_mmu_setcr)
	set	RMMU_CTL_REG, %o1	! set srmmu control register
	retl
	sta	%o0, [%o1]ASI_MOD
        SET_SIZE(srmmu_mmu_setcr)

	ENTRY(srmmu_mmu_setctxreg)
	set	RMMU_CTX_REG, %o5	! set srmmu context number
	retl
	sta	%o0, [%o5]ASI_MOD
        SET_SIZE(srmmu_mmu_setctxreg)

	ENTRY(srmmu_mmu_flushall)
	or	%g0, FT_ALL<<8, %o0	! flush entire mmu
	retl
	sta	%g0, [%o0]ASI_FLPR	! do the flush
        SET_SIZE(srmmu_mmu_flushall)

	ENTRY(srmmu_mmu_handle_ebe)	! handle ebe error
	retl
	nop				! do nothing for now
        SET_SIZE(srmmu_mmu_handle_ebe)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
srmmu_mmu_flushctx(int ctx)
{}

#else	/* lint */

/*
 * BORROW_CONTEXT: temporarily set the context number
 * to that given in %o1.
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
 * The probe sequence (middle block) is documented more clearly in
 * the routine "mmu_flushctx".
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

	ENTRY(srmmu_mmu_flushctx)
	mov	%o0, %o2		! BORROW_CONTEXT expects context in %o2
	set	FT_CTX<<8, %o0
	BORROW_CONTEXT
	sta	%g0, [%o0]ASI_FLPR	! do the flush
	subcc	%o2, %o3, %g0
	bne	1f
	nop
	mov	KCONTEXT, %o3
1:
	RESTORE_CONTEXT
	retl
	nop				! psr delay
        SET_SIZE(srmmu_mmu_flushctx)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
srmmu_mmu_flushrgn(caddr_t addr)
{}

/* ARGSUSED */
void
srmmu_mmu_flushseg(caddr_t addr)
{}

/* ARGSUSED */
void
srmmu_mmu_flushpage(caddr_t addr)
{}

#else	/* lint */

	ENTRY(srmmu_mmu_flushrgn)
	mov	%o1, %o2		! BORROW_CONTEXT expects context in %o2
	b	.srmmu_flushcommon	! flush region in context from mmu
	or	%o0, FT_RGN<<8, %o0

	ENTRY(srmmu_mmu_flushseg)
	mov	%o1, %o2		! BORROW_CONTEXT expects context in %o2
	b	.srmmu_flushcommon	! flush segment in context from mmu
	or	%o0, FT_SEG<<8, %o0

	ENTRY(srmmu_mmu_flushpage)
	or	%o0, FT_PAGE<<8, %o0

.srmmu_flushcommon:
	BORROW_CONTEXT
	sta	%g0, [%o0]ASI_FLPR	! do the flush
	RESTORE_CONTEXT
	retl
	nop				! PSR or MMU delay.
        SET_SIZE(srmmu_mmu_flushpage)
        SET_SIZE(srmmu_mmu_flushseg)
        SET_SIZE(srmmu_mmu_flushrgn)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
srmmu_mmu_flushpagectx(caddr_t vaddr, u_int ctx)
{}

#else	/* lint */

	ENTRY(srmmu_mmu_flushpagectx)
	mov	%o1, %o2		! BORROW_CONTEXT expects context in %o2
	or	%o0, FT_PAGE<<8, %o0
	BORROW_CONTEXT
	sta	%g0, [%o0]ASI_FLPR	! do the flush
	RESTORE_CONTEXT
	retl
	nop				! PSR or MMU delay.
        SET_SIZE(srmmu_mmu_flushpagectx)

#endif	/* lint */

#if defined(lint)

void
srmmu_mmu_getsyncflt(void)
{}

#else	/* lint */

	!
	! BE CAREFUL - register usage must correspond to code
	! in locore.s that calls this routine
	!
	! See comments for "mmu_getsyncflt" for allocation information.
	!
	! Note that this routine now stores the synchronous fault status
	! and address register contents in the per-CPU struct.  Previously,
	! the contents of the status and address registers were stored in
	! %g4 and %g6, respectively, but due to register conflicts
	! cause by the new FLAG_REG in locore.s, this can no longer be
	! done.
	!
	ENTRY_NP(srmmu_mmu_getsyncflt)
	! load CPU struct addr to %g1 using %g4.
	CPU_ADDR(%g1, %g4)
	set	RMMU_FAV_REG, %g4
	lda	[%g4]ASI_MOD, %g4
	st	%g4, [%g1 + CPU_SYNCFLT_ADDR]
	set	RMMU_FSR_REG, %g4
	lda	[%g4]ASI_MOD, %g4
	retl
	st	%g4, [%g1 + CPU_SYNCFLT_STATUS]
        SET_SIZE(srmmu_mmu_getsyncflt)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
srmmu_mmu_getasyncflt(caddr_t afsrbuf)
{}

#else	/* lint */

	ENTRY(srmmu_mmu_getasyncflt)
	set	RMMU_AFA_REG, %o1
	lda	[%o1]ASI_MOD, %o1
	st	%o1, [%o0+4]
	set	RMMU_AFS_REG, %o1
	lda	[%o1]ASI_MOD, %o1
	st	%o1, [%o0]
	set	-1, %o1
	retl
	st	%o1, [%o0+8]
        SET_SIZE(srmmu_mmu_getasyncflt)

#endif	/* lint */

#if defined(lint)

int
srmmu_mmu_chk_wdreset(void)
{ return (0); }

#else	/* lint */

	ENTRY(srmmu_mmu_chk_wdreset)
	set	RMMU_RST_REG, %o1	! D-side reset reg.
	lda	[%o1]ASI_MOD, %o0
	retl
	and	%o0, RSTREG_WD, %o0	! mask WD reset bit
        SET_SIZE(srmmu_mmu_chk_wdreset)

#endif	/* lint */

#if defined(lint)

int
srmmu_mmu_ltic(void)
{ return (0); }

#else	/* lint */

/*
 * lowest-common-denominator SRMMU has no way
 * of locking translations in the TLB, so just
 * return "failure".
 */
	ENTRY(srmmu_mmu_ltic)
	retl
	sub	%g0, 1, %o0
        SET_SIZE(srmmu_mmu_ltic)

#endif	/* lint */

#if defined(lint)

void
/*ARGSUSED*/
srmmu_mmu_setctp(u_int pa)
{}

#else	/* lint */

	/*
	 * Fix for bug 1170275: Booting over the net causes the machine to
	 * panic or watchdog sometimes
	 * Routine to switch to the kernel's context and page tables.We come
	 * here with the kernel running out of the Open Boot Prom's context
	 * and page tables. At this time all kernel text and data is set up
	 * with non-cacheable level 3 PTEs. We cut over to cacheable level 2
	 * PTEs.
	 * The code here should not cross page-boundaries which can cause a
	 * TLB miss and thus cause inconsistent TLB entries (some cached,
	 * some non-cached) causing watchdog resets. We disable traps, load
	 * the kernel's context table in the MMU's context table register,
	 * flush all TLB entries, re-enable traps and return. This routine is
	 * called from hat_kern_setup().
 	 */

	.align 128		! Must align on at least this boundary to
				! avoid instructions in this routine from
				! crossing page boundaries.
	ENTRY(srmmu_mmu_setctp)

	mov	%psr, %o5;
	andn	%o5, PSR_ET, %o4;
	mov	%o4, %psr;		! Disable traps
	nop;nop;nop;			! PSR delay
	!
	set	RMMU_CTP_REG, %o1	! set srmmu context table pointer
	sta	%o0, [%o1]ASI_MOD
	or	%g0, FT_ALL<<8, %o0	! flush entire mmu
	sta	%g0, [%o0]ASI_FLPR	! do the flush
	!
	mov	%o5, %psr;		! Enable traps
	nop;nop;nop;			! PSR delay
	retl
	nop

	SET_SIZE(srmmu_mmu_setctp)

#endif	/* lint */
