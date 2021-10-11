/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)module_asm.s	1.37	99/04/13 SMI"

/*
 * Interfaces for dynamically selectable modules.
 */

#if defined(lint)
#include <sys/types.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <vm/seg.h>
#else	/* lint */
#include "assym.h"
#include <sys/mmu.h>
#endif	/* lint */

#include <sys/machparam.h>
#include <sys/asm_linkage.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/trap.h>
#include <sys/devaddr.h>
#include <sys/machthread.h>

#if defined(lint)

u_int
mmu_getcr(void)
{ return (0); }

u_int
mmu_getctp(void)
{ return (0); }

u_int
mmu_getctx(void)
{ return (0); }

/*ARGSUSED*/
u_int
mmu_probe(caddr_t probe_val, u_int *fsr)
{ return (0); }

/*ARGSUSED*/
void
mmu_setcr(u_int c)
{}

/*ARGSUSED*/
void
mmu_setctp(u_int c)
{}

/*ARGSUSED*/
void
mmu_setctxreg(u_int c_num)
{}

void
mmu_flushall(void)
{}

/*ARGSUSED*/
void
mmu_readpte(struct pte *opte, struct pte *npte)
{}

/*ARGSUSED*/
u_int
mod_writepte(struct pte *pte, u_int entry, caddr_t addr, int level,
    u_int cxn, int rmkeep)
{ return (0); }

/*ARGSUSED*/
void
mod_writeptp(struct ptp *ptp, u_int entry, caddr_t addr, int level,
    u_int cxn, int flag)
{}

/*ARGSUSED*/
void
vac_color_sync(u_int vaddr, u_int tag)
{}
#else	/* lint */

	.seg	".text"
	.align	4

#define	READCXN(r)				\
	set	RMMU_CTX_REG, r			; \
	lda	[r]ASI_MOD, r

#define	REVEC(name, r)				\
	sethi	%hi(v_/**/name), r		; \
	ld	[r+%lo(v_/**/name)], r		; \
	jmp	r				; \
	nop

	ENTRY(mmu_getcr)
	REVEC(mmu_getcr, %o5)
	SET_SIZE(mmu_getcr)

	ENTRY(mmu_getctp)
	REVEC(mmu_getctp, %o5)
	SET_SIZE(mmu_getctp)

	ENTRY(mmu_getctx)
	REVEC(mmu_getctx, %o5)
	SET_SIZE(mmu_getctx)

	ENTRY(mmu_probe)
	REVEC(mmu_probe, %o5)
	SET_SIZE(mmu_probe)

	ENTRY(mmu_setcr)
	REVEC(mmu_setcr, %o5)
	SET_SIZE(mmu_setcr)

	ENTRY(mmu_setctp)
	REVEC(mmu_setctp, %o5)
	SET_SIZE(mmu_setctp)

	ENTRY(mmu_setctxreg)
	REVEC(mmu_setctxreg, %o5)
	SET_SIZE(mmu_setctxreg)

	ENTRY(mmu_flushall)
	REVEC(mmu_flushall, %o5)
	SET_SIZE(mmu_flushall)

	/*
	 * NOTE WELL: the ld in the following routine has been
	 * strategically placed in its own instruction group to
	 * work around a Viking bug.  Do not disturb!  See the comments
	 * preceding vik_mmu_writepte in uts/sun4m/osmodule_vik.c for
	 * more details.
	 */
	ENTRY(mmu_readpte)
	ALTENTRY(mmu_readptp);
	ALTENTRY(iommu_readpte)
	ld	[%o0], %o0
	st	%o0, [%o1]
	retl
	nop
	SET_SIZE(mmu_readpte)
	SET_SIZE(mmu_readptp);
	SET_SIZE(iommu_readpte)

	ENTRY(mod_writepte)
	REVEC(mmu_writepte, %g1)
	SET_SIZE(mod_writepte)

	ENTRY(mod_writeptp)
	REVEC(mmu_writeptp, %g1)
	SET_SIZE(mod_writeptp)

	ENTRY(vac_color_sync)
	REVEC(vac_color_sync, %g1)
	SET_SIZE(vac_color_sync)
#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
mmu_flushctx(u_int ctx, u_int flags)
{}

#else	/* lint */

	ENTRY(mmu_flushctx)
	mov	%o1, %o3		! flags is passed in %o3
	mov	%o0, %o2		! dup ctx to expected location
	REVEC(mmu_flushctx, %o5)
	SET_SIZE(mmu_flushctx)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
mmu_flushrgn(caddr_t addr, u_int cxn, u_int flags)
{}

#else	/* lint */

	ENTRY(mmu_flushrgn)
	srl	%o0, MMU_STD_RGNSHIFT, %o0 ! round address to
	sll	%o0, MMU_STD_RGNSHIFT, %o0 ! base of region.
	mov	%o2, %o3		! flags is passed in %o3
	mov	%o1, %o2		! duplicate cxn to expected location.
	REVEC(mmu_flushrgn, %o5)
	SET_SIZE(mmu_flushrgn)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
mmu_flushseg(caddr_t addr, u_int cxn, u_int flags)
{}

#else	/* lint */

	ENTRY(mmu_flushseg)
	srl	%o0, MMU_STD_SEGSHIFT, %o0 ! round address to
	sll	%o0, MMU_STD_SEGSHIFT, %o0 ! base of segment.
	mov	%o2, %o3		! flags is passed in %o3
	mov	%o1, %o2		! duplicate cxn to expected location.
	REVEC(mmu_flushseg, %o5)
	SET_SIZE(mmu_flushseg)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */ 
void
mmu_flushpage(caddr_t addr)
{}

#else	/* lint */

	ENTRY(mmu_flushpage)
	srl	%o0, MMU_STD_PAGESHIFT, %o0 ! round address to
	sll	%o0, MMU_STD_PAGESHIFT, %o0 ! base of page.
	READCXN(%o2)
	REVEC(mmu_flushpage, %o5)
	SET_SIZE(mmu_flushpage)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */ 
void
mmu_flushpagectx(caddr_t vaddr, u_int ctx, u_int flags)
{}

#else	/* lint */

	ENTRY(mmu_flushpagectx)
	srl	%o0, MMU_STD_PAGESHIFT, %o0 ! round address to
	sll	%o0, MMU_STD_PAGESHIFT, %o0 ! base of page.
	mov	%o2, %o3		! flags is passed in %o3
	mov	%o1, %o2		! duplicate cxn to expected location.
	REVEC(mmu_flushpagectx, %o5)
	SET_SIZE(mmu_flushpagectx)

#endif	/* lint */

#if defined(lint)

u_int
mmu_getsyncflt(void)
{ return (0); }

#else	/* lint */

	!
	! BE CAREFUL - register useage must correspond to code
	! in locore.s that calls this routine!
	!
	! AT THIS TIME, on entry, %o7 holds our return address;
	! this routine is assumed to use only %g1 and %g4.
	! Check the caller to verify these register assignments.
	! to verify these register assignments.
	!
	ENTRY_NP(mmu_getsyncflt)
	REVEC(mmu_getsyncflt, %g4)
	SET_SIZE(mmu_getsyncflt)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */ 
void
mmu_getasyncflt(u_int *ptr)
{}

int
mmu_chk_wdreset(void)
{ return (0); }

#else	/* lint */

	! Passed a ptr to where to store asynchronous fault reg contents
	! Stored in the following order:
	! AFSR (lowest MID master)
	! AFAR (lowest MID master)
	! AFSR (highest MID master) -- -1 if not defined
	! AFAR (highest MID master)
	!
	ENTRY(mmu_getasyncflt)
	REVEC(mmu_getasyncflt, %o5)
	SET_SIZE(mmu_getasyncflt)

	ENTRY(mmu_chk_wdreset)
	REVEC(mmu_chk_wdreset, %o5)
	SET_SIZE(mmu_chk_wdreset)

#endif	/* lint */

#if defined(lint)

void
mmu_ltic(void)
{}

/* IFLUSH - unimpflush - handles unimplemented flush trap */
int
unimpflush (void)
{return 0;}

/* IFLUSH - srmmu_mmu_unimpflush - default handler returns 0 - not handled */
int
srmmu_unimpflush (void)
{return 0;}

/* IFLUSH - ic_flush - perform an ic_flush operation		*/
void
ic_flush (void)
{}

void
cache_init(void)
{}

void
srmmu_noop(void)
{}

int
srmmu_inoop(void)
{ return (0); }

/*ARGSUSED*/
int
get_hwcap_flags(int inkernel)
{ return (0); }

#else	/* lint */

	ENTRY(mmu_ltic)
	REVEC(mmu_ltic, %o5)
	SET_SIZE(mmu_ltic)

/* IFLUSH - unimpflush - handles unimplemented flush trap */
	ENTRY(unimpflush)
	CPU_INDEX(%o0)			! pass in current cpu# as an arg
	REVEC(unimpflush, %g1)
	SET_SIZE(unimpflush)

/* IFLUSH - srmmu_unimpflush - default handler returns 0 - not handled */
	ENTRY(srmmu_unimpflush)
	retl
	mov %g0, %o0
	SET_SIZE(srmmu_unimpflush)

/* IFLUSH - ic_flush - revec for icache flush */
	ENTRY(ic_flush)
	REVEC(ic_flush, %g1)
	SET_SIZE(ic_flush)

	ENTRY(cache_init)
	REVEC(cache_init, %g1)
	SET_SIZE(cache_init)

	ENTRY(pac_pageflush)
	REVEC(pac_pageflush, %g1)
	SET_SIZE(pac_pageflush)

	ENTRY(srmmu_noop)
	retl
	nop
	SET_SIZE(srmmu_noop)

	ENTRY(srmmu_inoop)
	retl
	mov %g0, %o0
	SET_SIZE(srmmu_inoop)

	ENTRY(get_hwcap_flags)
	REVEC(get_hwcap_flags, %o5)
	SET_SIZE(get_hwcap_flags)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
int
mmu_handle_ebe(u_int mmu_fsr, caddr_t addr, u_int type, struct regs *rp,
    enum seg_rw rw)
{ return (0); }

#else   /* lint */

        ENTRY(mmu_handle_ebe)
        REVEC(mmu_handle_ebe, %g1)
        SET_SIZE(mmu_handle_ebe)

#endif  /* lint */

#if defined(lint)

/*ARGSUSED*/
void
mmu_log_module_err(u_int r1, u_int r2, u_int r3, u_int r4)
{}

#else   /* lint */

        ENTRY(mmu_log_module_err)
        REVEC(mmu_log_module_err, %g1)
        SET_SIZE(mmu_log_module_err)

#endif  /* lint */


#if defined(lint)

/*ARGSUSED*/
void
vac_usrflush(u_int flags)
{}

#else	/* lint */

	ENTRY(vac_usrflush)
	sethi	%hi(vac), %g1
	ld	[%g1 + %lo(vac)], %g1
	tst	%g1			! check if cache is turned on
	bz	srmmu_noop
	nop

	set	flush_cnt, %o5
	ld	[%o5 + FM_USR], %g1
	inc	%g1			! increment flush count
	st	%g1, [%o5 + FM_USR]
	REVEC(vac_usrflush, %g1)
	SET_SIZE(vac_usrflush)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */ 
void
vac_ctxflush(u_int ctx, u_int flags)
{}

#else	/* lint */

	!
	! flush user pages in context from cache
	!
	ENTRY(vac_ctxflush)

	! flush TLB if flags has FL_TLB bit set
	andcc	%o1, FL_TLB, %g1
	bnz,a	1f
	nop

	sethi	%hi(vac), %g1
	ld	[%g1 + %lo(vac)], %g1
	tst	%g1			! check if cache is turned on
	bz	srmmu_noop
	nop

1:	set	flush_cnt, %o5
	ld	[%o5 + FM_CTX], %g1
	inc	%g1			! increment flush count
	st	%g1, [%o5 + FM_CTX]
	REVEC(vac_ctxflush, %g1)
	SET_SIZE(vac_ctxflush)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */ 
void
vac_rgnflush(caddr_t va, u_int cxn, u_int flags)
{}

#else	/* lint */

	!
	! flush rgn [in current context or supv] from cache
	!
	ENTRY(vac_rgnflush)

	! flush TLB if flags has FL_TLB bit set
	andcc	%o2, FL_TLB, %g1
	bnz,a	1f
	nop

	sethi	%hi(vac), %g1
	ld	[%g1 + %lo(vac)], %g1
	tst	%g1			! check if cache is turned on
	bz	srmmu_noop
	nop

1:	set	flush_cnt, %o5
	ld	[%o5 + FM_REGION], %g1
	inc	%g1			! increment flush count
	st	%g1, [%o5 + FM_REGION]
	srl	%o0, MMU_STD_RGNSHIFT, %o0 ! round address to
	sll	%o0, MMU_STD_RGNSHIFT, %o0 ! base of region
	REVEC(vac_rgnflush, %g1)
	SET_SIZE(vac_rgnflush)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */ 
void
vac_segflush(caddr_t va, u_int cxn, u_int u_flags)
{}

#else	/* lint */

	!
	! flush seg [in current context or supv] from cache
	!
	ENTRY(vac_segflush)

	! flush TLB if flags has FL_TLB bit set
	andcc	%o2, FL_TLB, %g1
	bnz,a	1f
	nop

	sethi	%hi(vac), %g1
	ld	[%g1 + %lo(vac)], %g1
	tst	%g1			! check if cache is turned on
	bz	srmmu_noop
	nop

1:	set	flush_cnt, %o5
	ld	[%o5 + FM_SEGMENT], %g1
	inc	%g1			! increment flush count
	st	%g1, [%o5 + FM_SEGMENT]
	srl	%o0, MMU_STD_SEGSHIFT, %o0 ! round address to
	sll	%o0, MMU_STD_SEGSHIFT, %o0 ! base of region
	REVEC(vac_segflush, %g1)
	SET_SIZE(vac_segflush)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */ 
void
vac_pageflush(caddr_t va, u_int ctx, u_int flags)
{}

#else	/* lint */

	!
	! flush page in ctx [or supv] from cache
	!
	ENTRY(vac_pageflush)

	! flush TLB if flags has FL_TLB bit set
	andcc	%o2, FL_TLB, %g1
	bnz,a	1f
	nop

	sethi	%hi(vac), %g1
	ld	[%g1 + %lo(vac)], %g1
	tst	%g1			! check if cache is turned on
	bz	srmmu_noop
	nop

1:	set	flush_cnt, %g1
	ld	[%g1 + FM_PAGE], %o5
	inc	%o5			! increment flush count
	st	%o5, [%g1 + FM_PAGE]
	srl	%o0, MMU_STD_PAGESHIFT, %o0 ! round address to
	sll	%o0, MMU_STD_PAGESHIFT, %o0 ! base of page.
	REVEC(vac_pageflush, %g1)
	SET_SIZE(vac_pageflush)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */ 
void
vac_flush(caddr_t va, u_int sz)
{}

#else	/* lint */

	!
	! flush range [in current context or supv] from cache
	!

	ENTRY(vac_flush)

	sethi	%hi(vac), %g1
	ld	[%g1 + %lo(vac)], %g1
	tst	%g1			! check if cache is turned on
	bz	srmmu_noop
	nop

	set	flush_cnt, %g1
	ld	[%g1 + FM_PARTIAL], %o5
	inc	%o5			! increment flush count
	st	%o5, [%g1 + FM_PARTIAL]
	READCXN(%o2)
	REVEC(vac_flush, %g1)
	SET_SIZE(vac_flush)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */ 
void
turn_cache_on(int cpuid)
{}

#else	/* lint */

	!
	! enable the cache
	!
	ENTRY_NP(turn_cache_on)
	REVEC(turn_cache_on, %g1)
	SET_SIZE(turn_cache_on)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
int
pac_parity_chk_dis(u_int r1, u_int r2)
{ return (0); }

#else	/* lint */

	ENTRY(pac_parity_chk_dis)
	REVEC(pac_parity_chk_dis, %o5)
	SET_SIZE(pac_parity_chk_dis)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
pac_flushall()
{}

#else   /* lint */

        !
        ! blow away ptags
        !
        ENTRY(pac_flushall)
        REVEC(pac_flushall, %g1)
        SET_SIZE(pac_flushall)

#endif  /* lint */

#if defined(lint)

/*ARGSUSED*/
void
vac_allflush(u_int flags)
{}

#else	/* lint */

!
! Routines for flushing TLB and VAC (if it exists) for the particular 
! module.
!
	ENTRY(vac_allflush)
	REVEC(vac_allflush, %o5)
	SET_SIZE(vac_allflush)

#endif	/* lint */

#if defined(lint)
/* ARGSUSED */
void
vac_color_flush(caddr_t vaddr, u_int pfn, int cxn)
{}
#else   /* lint */

	ENTRY(vac_color_flush)
	set     flush_cnt, %g1
	ld      [%g1 + FM_PAGE], %o5
	inc     %o5                     ! increment flush count
	st      %o5, [%g1 + FM_PAGE]
	srl     %o0, MMU_STD_PAGESHIFT, %o0 ! round address to
	sll     %o0, MMU_STD_PAGESHIFT, %o0 ! base of page.
	REVEC(vac_color_flush, %o5)
	SET_SIZE(vac_color_flush)

#endif  /* lint */

#if defined(lint)

void
uncache_pt_page(void)
{}

#else	/* lint */

!
! Routines for flushing TLB and Caches for a memory page that contains
! ptes for the particular module.
!
	ENTRY(uncache_pt_page)
	REVEC(uncache_pt_page, %o5)
	SET_SIZE(uncache_pt_page)

#endif	/* lint */
