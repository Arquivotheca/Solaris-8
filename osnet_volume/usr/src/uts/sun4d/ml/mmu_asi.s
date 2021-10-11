/*
 * Copyright (c) 1991, 1993 Sun Microsystems,  Inc.
 */

#pragma ident	"@(#)mmu_asi.s	1.24	97/05/24 SMI"

#if defined(lint)
#include <sys/types.h>
#else
#include "assym.h"
#endif

#include <sys/asm_linkage.h>
#include <sys/machparam.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/machthread.h>

/*
 * simple access macros
 */
#define	SRMMU_GET_CTL(reg)		\
	set	RMMU_CTL_REG, reg;	\
	lda	[reg]ASI_MOD, reg;

#define	SRMMU_GET_FAV(reg)		\
	set	RMMU_FAV_REG, reg;	\
	lda	[reg]ASI_MOD, reg;

#define	SRMMU_GET_FSR(reg)		\
	set	RMMU_FSR_REG, reg;	\
	lda	[reg]ASI_MOD, reg;

/*
 * simple optimization(s) - old state w/NF in (reg)
 */
#define	SRMMU_SET_NF(reg, scr)		\
	set	RMMU_CTL_REG, reg;	\
	lda	[reg]ASI_MOD, reg;	\
	or	reg, MMCREG_NF, reg;	\
	set	RMMU_CTL_REG, scr;	\
	sta	reg, [scr]ASI_MOD;

#define	SRMMU_CLR_NF(reg, scr)		\
	andn	reg, MMCREG_NF, reg;	\
	set	RMMU_CTL_REG, scr;	\
	sta	reg, [scr]ASI_MOD;


#ifdef	NOTDEF
#if defined(lint)
phys_ptp_rd_va(u_int ta)
{ return (0); }

#else	/* lint */

	/*
	 * This routine returns the virtual address of the page
	 * where 'ta' is pointing to. Ie. the virtual address of
	 * the page that contains the sw/hw ptbls pointed by 'ta'.
	 *
	 * 'Ta' is PA<6:35> or ptp.PageTablePointer part of ptp.
	 */
	ENTRY_NP(phys_ptp_rd_va)
	set	0x3C000000, %o1		! Get PA <32:36> 
	andcc	%o0, %o1, %o1		! top four bits are for the ASI
	sll	%o0, 6, %o0		! PA31 is now at bit 31
	or	%o0, APG_VA_OFF, %o0
	bne,a	1f
	lda	[%o0]0x21, %o0
	!
	retl
	lda	[%o0]0x20, %o0
1:
	retl
	nop
	SET_SIZE(phys_ptp_rd_va)

#endif	/*lint */

#endif	/* NOTDEF */
/*
 * u_int
 * mmu_getsyncflt()
 *
 *	Get the fault status and fault address info and stuff
 *	it into the per-CPU struct.  N.B.: register usage must
 *	match locore's expectations.
 */

#if defined(lint)

u_int
mmu_getsyncflt(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(mmu_getsyncflt)
	! load CPU struct addr to %g1 using %g4.
	CPU_ADDR(%g1, %g4)
	set	RMMU_FAV_REG, %g4
	lda	[%g4]ASI_MOD, %g4
	st	%g4, [%g1 + CPU_SYNCFLT_ADDR]
	set	RMMU_FSR_REG, %g4
	lda	[%g4]ASI_MOD, %g4
	retl
	st	%g4, [%g1 + CPU_SYNCFLT_STATUS]
	SET_SIZE(mmu_getsyncflt)
	! sun4d/ml/locore.s - .entry, sys_trap

#endif	/* lint */

#define	PAGE_MASK		0xfff
#define	FLUSH_CODE(type)	(type << 8)

#if defined(lint)

/* ARGSUSED */
u_int
mmu_probe(caddr_t probe_val, u_int *fsr)
{ return (0); }

#else	/* lint */

/*
 * Probe routine that takes advantage of multilevel probes.
 * It always returns what the level 3 pte looks like, even if
 * the address is mapped by a level 2 or 1 pte.
 */
	ENTRY_NP(mmu_probe)
	and	%o0, MMU_PAGEMASK, %o2		! and off page offset bits
	or	%o2, FT_ALL << 8, %o3
	lda	[%o3]ASI_FLPR, %o0		! probe all
	cmp	%o0, 0				! if invalid, we're done
	be	probe_done
	or	%o2, FT_PAGE << 8, %o3		! probe page
	lda	[%o3]ASI_FLPR, %o0
	and	%o0, 3, %o3
	cmp	%o3, MMU_ET_PTE
	be	probe_done			! branch if it's a pte
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
	or	%o2, FT_RGN << 8, %o3
	lda	[%o3]ASI_FLPR, %o0		! probe region
	and	%o0, 3, %o3
	cmp	%o3, MMU_ET_PTE
	bne	probe_done
	srl	%o2, MMU_STD_PAGESHIFT, %o3	! figure offset into rgn
	and	%o3, MMU_STD_FIRSTMASK, %o3
	sll	%o3, 8, %o3			! shift into pfn position
	add	%o0, %o3, %o0			! add it to pfn

probe_done:
	set	RMMU_FSR_REG, %o2		! setup to clear fsr
	cmp	%o1, 0
	be	1f				! if fsr not wanted, don't st
	lda	[%o2]ASI_MOD, %o2		! clear fsr
	st	%o2, [%o1]			! return fsr
1:
	retl
	nop
	SET_SIZE(mmu_probe)

#endif	/* lint */
