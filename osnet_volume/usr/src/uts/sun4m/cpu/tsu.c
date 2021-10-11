/*
 * Copyright (c) 1990-1993 by Sun Microsystems, Inc.
 */

#ident	"@(#)tsu.c	1.33	96/10/07 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/machparam.h>
#include <sys/module.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/thread.h>
#include <sys/debug.h>

/*
 * Support for modules based on the Sun/TI MicroSPARC CPU
 *
 * The following MicroSPARC module versions are supported by this
 * module driver:
 *
 * 	IMPL    VERS    MASK    MODULE  	COMMENT
 *	4	1	2.1	tsunami_2.1
 *	4	1	2.2	tsunami_2.2
 *	4	1	3.0	tsunami_3.0	aka Tsupernami
 *
 * The following MicroSPARC module workarounds are supported by
 * this module driver:
 *
 *	MODULE		HWBUGID SWBUGID COMMENT
 *  	tsunami_2.1	1097676		TSUNAMI_PROBE_BUG
 *  	tsunami_2.1	1097677		TSUNAMI_CONTROL_STORE_BUG
 *	tsunami_2.1	1103841		TSUNAMI_TLB_FLUSH_BUG
 *	tsunami_2.1	1102768		TSUNAMI_CONTROL_READ_BUG
 *
 *	tsunami_2.2	1102768		TSUNAMI_CONTROL_READ_BUG
 *
 */

#define	TSUNAMI_PROBE_BUG
#define	TSUNAMI_CONTROL_STORE_BUG
#define	TSUNAMI_TLB_FLUSH_BUG
#define	TSUNAMI_CONTROL_READ_BUG

#ifdef	TSUNAMI_PROBE_BUG
int tsunami_probe_bug = 0;		/* tsunami P2.1 */
#endif

#ifdef	TSUNAMI_CONTROL_STORE_BUG
extern int tsunami_control_store_bug;	/* tsunami P2.1 */
#endif

#ifdef	TSUNAMI_TLB_FLUSH_BUG
int tsunami_tlb_flush_bug = 0;		/* tsunami P2.1 */
#endif

#ifdef  TSUNAMI_CONTROL_READ_BUG
extern int tsunami_control_read_bug;	/* tsunami P2.2 */
#endif


extern int tsunami;

extern int	tsu_mmu_probe();
extern void	tsu_cache_init();
extern void	tsu_turn_cache_on();
extern void	tsu_pac_flushall();
extern void	tsu_pac_flushall();
#ifdef	TSUNAMI_TLB_FLUSH_BUG
extern void	tsu_mmu_flushall();
extern void	tsu_mmu_flushpage();
#endif
extern void	srmmu_mmu_flushall();
extern void	small_sun4m_mmu_getasyncflt();
extern void	small_sun4m_sys_setfunc();
extern int	small_sun4m_mmu_chk_wdreset();
extern int	small_sun4m_ebe_handler();
extern int	tsu_mmu_probe();
extern int	swapl();

extern void	srmmu_tlbflush();
static int	tsu_mmu_writepte();
static void	tsu_mmu_writeptp();
static void	tsu_mmu_uncache_pt_page();

int
tsu_module_identify(u_int mcr)
{
	if (((mcr >> 24) & 0xff) == 0x41)
		return (1);

	return (0);
}


void		/*ARGSUSED*/
tsu_module_setup(u_int mcr)
{
	extern int use_mp;
	extern int small_4m;

	small_4m = 1;
	small_sun4m_sys_setfunc();

	tsunami = 1;
	cache |= (CACHE_PAC | CACHE_PTAG);
	use_mp = 0;

#ifdef	TSUNAMI_PROBE_BUG
	tsunami_probe_bug = 1;
#endif
#ifdef	TSUNAMI_CONTROL_STORE_BUG
	tsunami_control_store_bug = 1;
#endif
#ifdef	TSUNAMI_TLB_FLUSH_BUG
	tsunami_tlb_flush_bug = 1;
#endif
#ifdef  TSUNAMI_CONTROL_READ_BUG
	tsunami_control_read_bug = 1;
#endif

	v_mmu_probe = tsu_mmu_probe;
	v_mmu_writepte = tsu_mmu_writepte;
	v_mmu_writeptp = tsu_mmu_writeptp;
	v_mmu_chk_wdreset = small_sun4m_mmu_chk_wdreset;
	v_mmu_getasyncflt = small_sun4m_mmu_getasyncflt;
	v_mmu_handle_ebe = small_sun4m_ebe_handler;
	v_cache_init = tsu_cache_init;
	v_turn_cache_on = tsu_turn_cache_on;
	v_pac_flushall = tsu_pac_flushall;

	/*
	 * Tsunami only has entire and page flush operations.
	 * Since the srmmu_mmu_flush{ctx,rgn,seg,pagectx}
	 * routines have extra code to borrow contexts, we
	 * can save time by mapping these to flushall
	 */
#ifdef TSUNAMI_TLB_FLUSH_BUG
	if (tsunami_tlb_flush_bug) {
		v_mmu_flushpage = tsu_mmu_flushpage;
		v_mmu_flushall = tsu_mmu_flushall;
		v_mmu_flushctx = tsu_mmu_flushall;
		v_mmu_flushrgn = tsu_mmu_flushall;
		v_mmu_flushseg = tsu_mmu_flushall;
		v_mmu_flushpagectx = tsu_mmu_flushall;
		v_vac_ctxflush = tsu_mmu_flushall;
		v_vac_rgnflush = tsu_mmu_flushall;
		v_vac_segflush = tsu_mmu_flushall;
		v_vac_pageflush = tsu_mmu_flushall;
	} else {
		v_mmu_flushctx = srmmu_mmu_flushall;
		v_mmu_flushrgn = srmmu_mmu_flushall;
		v_mmu_flushseg = srmmu_mmu_flushall;
		v_mmu_flushpagectx = srmmu_mmu_flushall;
		v_vac_ctxflush = srmmu_mmu_flushall;
		v_vac_rgnflush = srmmu_mmu_flushall;
		v_vac_segflush = srmmu_mmu_flushall;
		v_vac_pageflush = srmmu_mmu_flushall;
	}
#else
	v_mmu_flushctx = srmmu_mmu_flushall;
	v_mmu_flushrgn = srmmu_mmu_flushall;
	v_mmu_flushseg = srmmu_mmu_flushall;
	v_mmu_flushpagectx = srmmu_mmu_flushall;
#endif

	v_uncache_pt_page = tsu_mmu_uncache_pt_page;

#ifndef	TSUNAMI_TLB_FLUSH_BUG
	v_vac_ctxflush = srmmu_mmu_flushall;
	v_vac_rgnflush = srmmu_mmu_flushall;
	v_vac_segflush = srmmu_mmu_flushall;
	v_vac_pageflush = srmmu_mmu_flushall;
#endif

	nctxs = 64;
	isa_list = "sparcv8-fsmuld sparcv7 sparc";
}


void		/*ARGSUSED*/
tsu_mmu_uncache_pt_page(caddr_t addr, u_int pfn)
{
	tsu_pac_flushall();
}



/*
 * Tsunami's writepte function is simple since it doesn't support MP.
 * We just load the old pte, or in any saved bits, store it, and flush
 * the tlb if the old pte was valid.
 */
tsu_mmu_writepte(pte, value, addr, level, cxn, rmkeep)
	struct pte *pte;
	u_int value;
	caddr_t addr;
	int level;
	int cxn;
	u_int rmkeep;
{
	u_int old, *ipte;

	ASSERT((rmkeep & ~PTE_RM_MASK) == 0);
	ipte = (u_int *)pte;
	old = *ipte;
	value |= old & rmkeep;
	*ipte = value;
	if (PTE_ETYPE(old) == MMU_ET_PTE && cxn != -1)
		srmmu_tlbflush(level, addr, cxn, FL_ALLCPUS);
	return (old & PTE_RM_MASK);
}

/*
 * Tsunami's writeptp function.
 */
void
tsu_mmu_writeptp(ptp, value, addr, level, cxn)
	struct ptp *ptp;
	u_int value;
	caddr_t addr;
	int level;
	int cxn;
{
	u_int old;

	old = swapl(value, (u_int *)ptp);
	if (PTE_ETYPE(old) == MMU_ET_PTP && cxn != -1)
		srmmu_tlbflush(level, addr, cxn, FL_ALLCPUS);
}
