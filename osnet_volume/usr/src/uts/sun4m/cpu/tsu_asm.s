/*
 *	Copyright (c) 1990 - 1993 by Sun Microsystems, Inc.
 *
 * assembly code support for modules based on the
 * TSUNAMI chip set.
 */

#ident	"@(#)tsu_asm.s	1.19	97/05/24 SMI"


#if defined(lint)
#include <sys/types.h>
#else
#include "assym.h"
#endif /* lint */

#include <sys/machparam.h>
#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/psr.h>
#include <sys/trap.h>
#include <sys/devaddr.h>

#define	TSUNAMI_PROBE_BUG
#define	TSUNAMI_TLB_FLUSH_BUG
#define TSUNAMI_CONTROL_READ_BUG
#define	USE_TSUNAMI_WINDOW_VECTORS

#if defined(lint)

void
tsu_cache_init(void)
{}

void
tsu_turn_cache_on(void)
{}

/*ARGSUSED*/
void
tsu_pac_flushall()
{}

u_int
tsu_mmu_probe(u_int a)
{
	return (a);
}

#if defined(TSUNAMI_TLB_FLUSH_BUG)
void
tsu_mmu_flushall(void)
{}
#endif

#if defined(TSUNAMI_CONTROL_READ_BUG)
void
tsu_cs_read_enter(void)
{}

void
tsu_cs_read_exit(void)
{}
#endif

#else	/* lint */

	.seg    ".text"
	.align  4

	ENTRY(tsu_cache_init)
	sta	%g0, [%g0]ASI_ICFCLR	! flash clear icache
	sta	%g0, [%g0]ASI_DCFCLR	! flash clear dcache
	retl
	nop
	SET_SIZE(tsu_cache_init)

	ENTRY(tsu_turn_cache_on)
	sta	%g0, [%g0]ASI_ICFCLR	! flash clear icache
	sta	%g0, [%g0]ASI_DCFCLR	! flash clear dcache
	set	0x300, %o2	! i & d cache enable
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1
	or	%o1, %o2, %o1
	sta	%o1, [%o0]ASI_MOD
	retl
	nop
	SET_SIZE(tsu_turn_cache_on)

	ENTRY(tsu_pac_flushall)
	sta     %g0, [%g0]ASI_ICFCLR    ! flash clear icache
	sta     %g0, [%g0]ASI_DCFCLR    ! flash clear dcache
	retl
	nop
	SET_SIZE(tsu_pac_flushall)

#if defined(TSUNAMI_PROBE_BUG) || defined(TSUNAMI_TLB_FLUSH_BUG) \
			       || defined(TSUNAMI_CONTROL_READ_BUG)

#define DISABLE_TRAPS			\
	mov	%psr, %o5;		\
	andn	%o5, PSR_ET, %o4;	\
	mov	%o4, %psr;		\
	nop ; nop ; nop				

#define ENABLE_TRAPS			\
	mov	%psr, %o5;		\
	or	%o5, PSR_ET, %o4;	\
	mov	%o4, %psr;		\
	nop ; nop ; nop				

#endif
	
	! Probe entire operations in tsunami 2.1 parts have a bug
	! in which a failed probe can leave a corrupt entry in the
	! tlb.  This corrupt entry is marked as valid and its pte
	! has a valid entry type.  This creates the possibility of
	! an accidental match of entry and a blown address translation.
	!
	! To work around this bug we use the following algorithm for
	! doing probe entires:
	!
	!	1. disable traps 
	!	2. lock the trcr to confine tlb corruption to one entry
	!	3. do the probe (now the locked entry may corrupt)
	!	4. do a level 0 probe (which will invalidate the locked entry)
	!	5. unlock the trcr
	!	6. enable traps
	!
	
        ENTRY(tsu_mmu_probe)
#if defined(TSUNAMI_PROBE_BUG)
	DISABLE_TRAPS
        set     0x20, %o2			! freeze the
        set	0x1000, %o3			!     trcr at 
        sta     %o2, [%o3]ASI_MOD		!         tlb entry zero
#endif
	and	%o0, MMU_PAGEMASK, %o2		! and off page offset bits
	or	%o2, FT_ALL << 8, %o3
	lda	[%o3]ASI_FLPR, %o0		! probe all
	cmp	%o0, 0				! if invalid, we're done
	be	probe_done
	or	%o2, FT_RGN << 8, %o3		! probe rgn
	sta	%g0, [%o3]ASI_FLPR		! must flush first
#if defined(TSUNAMI_TLB_FLUSH_BUG)
	nop; nop; nop; nop; nop;		! flush workaround
#endif
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
	sta	%g0, [%o3]ASI_FLPR		! must flush first
#if defined(TSUNAMI_TLB_FLUSH_BUG)
	nop; nop; nop; nop; nop;		! flush workaround
#endif
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
	or	%o2, FT_PAGE << 8, %o2		! probe page to invalidate
	lda	[%o2]ASI_FLPR, %g0		! trashed tlb entry
	set	RMMU_FSR_REG, %o2		! setup to clear fsr
	cmp	%o1, 0
	be	1f				! if fsr not wanted, don't st
	lda	[%o2]ASI_MOD, %o2		! clear fsr
	st	%o2, [%o1]			! return fsr
1:
#if defined(TSUNAMI_PROBE_BUG)
	set	0x1000, %o2
	sta	%g0, [%o2]ASI_MOD		! thaw trcr
	ENABLE_TRAPS
#endif
	retl
	nop
        SET_SIZE(tsu_mmu_probe)

#if defined(TSUNAMI_TLB_FLUSH_BUG)

	! We've seen problems with the 4th instruction following
	! the flush not being fetched, so we follow the flush with
	! a run of nop's.

	ENTRY(tsu_mmu_flushall)
	DISABLE_TRAPS
	or	%g0, FT_ALL<<8, %o0	! flush entire mmu
	sta	%g0, [%o0]ASI_FLPR	! do the flush
	nop
	nop
	nop
	nop				! pad with nop's 
	nop
	ENABLE_TRAPS
	retl
	nop
        SET_SIZE(tsu_mmu_flushall)

	ENTRY(tsu_mmu_flushpage)
	DISABLE_TRAPS
	or	%o0, FT_PAGE<<8, %o0
	sta	%g0, [%o0]ASI_FLPR	! do the flush
	nop
	nop
	nop
	nop
	nop
	ENABLE_TRAPS
	retl
	nop				! PSR or MMU delay.
        SET_SIZE(tsu_mmu_flushpage)
#endif

#if defined(TSUNAMI_CONTROL_READ_BUG)

#define V_MOD_ID	0x10002000
#define SBUS_ARB_ENABLE	0x001f0000

	ENTRY(tsu_cs_read_enter)
	DISABLE_TRAPS
	set	V_MOD_ID, %o0		
	set	0x70000000, %o1
	sta	%g0, [%o0]0x20		! disable DVMA arbitration
	lda	[%o1]0x20, %g0		! read from SBus to sync SBC
	retl
	nop
        SET_SIZE(tsu_cs_read_enter)

	ENTRY(tsu_cs_read_exit)
	set	V_MOD_ID, %o0		
	set	SBUS_ARB_ENABLE, %o1
	sta	%o1, [%o0]0x20		! enable DVMA arbitration
	ENABLE_TRAPS
	retl
	nop
        SET_SIZE(tsu_cs_read_exit)
#endif

#endif 	/* lint */
