/*
 * Copyright (c) 1990-1994, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)ross605.c	1.34	96/10/18 SMI"

#include <sys/machparam.h>
#include <sys/module.h>
#include <sys/cpu.h>
#include <sys/pte.h>
#include <sys/archsystm.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <vm/hat_srmmu.h>
#include <sys/debug.h>

/*
 * Support for modules based on the Cypress CY604/CY605
 * memory management unit.
 */
extern void	ross_mmu_getasyncflt();
extern int	ross_mmu_ltic();

static int	ross_get_hwcap_flags(int);

/*
 * Maximum number of contexts for Ross.
 */
#define	MAX_NCTXS	(1 << 12)
#define	VERSION_MASK	0x0F000000
#define	HYP_VERSION	0x07000000

int ross_hw_workaround = 0;
extern int ross_hw_workaround2;

extern int ross_mod_mcr;
extern void    (*v_mmu_setctxreg)();
extern void	ross_cache_init();
extern void	ross_vac_allflush();
extern void	ross_vac_usrflush();
extern void	ross_vac_ctxflush();
extern void	ross_vac_rgnflush();
extern void	ross_vac_segflush();
extern void	ross_vac_pageflush();
extern void	ross_vac_flush();
extern void	ross_turn_cache_on();
extern void	ross_uncache_pt_page();
extern int	ross_mmu_writepte();
extern int	ross_mmu_writepte();
extern int	ross_mp_mmu_writepte();
extern void	ross_mmu_writeptp();
extern int	ross_mmu_probe();

int
ross_module_identify(u_int mcr)
{
	if (((mcr >> 24) & 0xf0) == 0x10 &&
	    ((mcr & VERSION_MASK) != HYP_VERSION))
		return (1);

	return (0);
}


void
ross_module_setup(register int mcr)
{
	ross_mod_mcr = mcr;

	cache |= (CACHE_VAC | CACHE_VTAG | CACHE_IOCOHERENT);
	v_mmu_writepte = ross_mmu_writepte;
	v_mp_mmu_writepte = ross_mp_mmu_writepte;
	v_mmu_writeptp = ross_mmu_writeptp;
	v_mmu_getasyncflt = ross_mmu_getasyncflt;
	v_mmu_ltic = ross_mmu_ltic;
	v_mmu_probe = ross_mmu_probe;

	ross_hw_workaround = 1;
	ross_hw_workaround2 = 1;
	v_vac_usrflush = ross_vac_usrflush;
	v_vac_ctxflush = ross_vac_ctxflush;
	v_vac_rgnflush = ross_vac_rgnflush;
	v_vac_segflush = ross_vac_segflush;
	v_vac_pageflush = ross_vac_pageflush;
	v_vac_flush = ross_vac_flush;
	v_turn_cache_on = ross_turn_cache_on;
	v_cache_init = ross_cache_init;
	v_vac_allflush = ross_vac_allflush;

	v_uncache_pt_page = ross_uncache_pt_page;

	/*
	 * Use the maximum number of contexts available for Ross.
	 */
	nctxs = MAX_NCTXS;

	v_get_hwcap_flags = ross_get_hwcap_flags;

	isa_list = "sparcv7 sparc";
}

/*ARGSUSED*/
void
ross_uncache_pt_page(caddr_t va, u_int pfn)
{
	vac_pageflush(va, KCONTEXT, FL_TLB_CACHE);
}

/*
 * The UP ross writepte function, this is only used on startup before
 * the other cpus are started.
 */
ross_mmu_writepte(pte, value, addr, level, cxn, rmkeep)
	struct pte *pte;
	u_int value;
	caddr_t addr;
	int level;
	int cxn;
	u_int rmkeep;
{
	u_int old;

	ASSERT((rmkeep & ~PTE_RM_MASK) == 0);
	old = *(u_int *)pte;
	value |= old & rmkeep;
	(void) swapl(value, (int *)pte);
	if (PTE_ETYPE(old) == MMU_ET_PTE && cxn != -1)
		srmmu_vacflush(level, addr, cxn, FL_TLB_CACHE);
	return (old & PTE_RM_MASK);
}

/*
 * The MP ross writepte function.
 *
 * Other MP writepte functions avoid CAPTURE/RELEASE by invalidating
 * the pte before writing the new one.  This doesn't work on ross
 * because of a hw bug.
 */

#define	PFN_C_V_MASK	0xFFFFFF83 /* check pfn, cache bit, entry type */

ross_mp_mmu_writepte(pte, value, addr, level, cxn, rmkeep)
	struct pte *pte;
	u_int value;
	caddr_t addr;
	int level;
	int cxn;
	u_int rmkeep;
{
	u_int old, *ipte;
	int vcache;

	ASSERT((rmkeep & ~PTE_RM_MASK) == 0);
	ipte = (u_int *)pte;
	old = *ipte;
	if (PTE_ETYPE(old) == MMU_ET_PTE && cxn != -1) {
		/*
		 * 'vcache' controls whether we have to flush the vtags.
		 * It's set when we're replacing a valid pte with an
		 * invalid one.
		 */
		vcache = (vac && pte->Cacheable &&
			((old & PFN_C_V_MASK) != (value & PFN_C_V_MASK)));

		/*
		 * Software workaround for Ross Hardware bug.
		 * If the PTE is valid, clean and writable, then
		 * we don't want to invalidate it since the MMU
		 * will get confused. So, the CPUS are
		 * first captured to prevent any type of table
		 * walk or changing of permission bits underneath
		 * and then issue the necessary flushes and
		 * subsequently update the PTE
		 */
		CAPTURE_CPUS;
		if (vcache)
			srmmu_vacflush(level, addr, cxn, FL_TLB_CACHE);
		else
			srmmu_tlbflush(level, addr, cxn, FL_ALLCPUS);
		value |= *ipte & rmkeep;
		old |= (u_int) swapl(value, (int *)ipte);
		RELEASE_CPUS;
	} else {
		value |= old & rmkeep;
		(void) swapl(value, (int *)ipte);
	}
	return (old & PTE_RM_MASK);
}

/*
 * The ross writeptp function.
 *
 * It uses CAPTURE/RELEASE for the same reasons writepte does.
 */
void
ross_mmu_writeptp(ptp, value, addr, level, cxn)
	struct ptp *ptp;
	u_int value;
	caddr_t addr;
	int level;
	int cxn;
{
	u_int *iptp, old;

	iptp = (u_int *)ptp;
	old = *iptp;
	if (PTE_ETYPE(old) == MMU_ET_PTP && cxn != -1) {
		CAPTURE_CPUS;
		srmmu_vacflush(level, addr, cxn, FL_TLB_CACHE);
		(void) swapl(value, (int *)iptp);
		RELEASE_CPUS;
	} else
		(void) swapl(value, (int *)iptp);
}

extern int	ross_mmu_ltic_ramcam();


/*
 * Lock Translation in Cache
 *
 * Retrieve the translation(s) for the range of memory specified, and
 * lock them in the TLB; return number of translations locked down.
 *
 * FIXME -- this routine currently assumes that we are not using short
 * translations; if we want to lock down segment or region
 * translations, we need to replace the call to mmu_probe with
 * something that tells us the short translation bits, pass those bits
 * on to the tlb, and step our virtual address to the end of the
 * locked area.
 *
 * Too bad the PTE and RAM bits don't quite line up.
 */
int
ross_mmu_ltic(svaddr, evaddr)
	register char  *svaddr;
	register char  *evaddr;
{
	unsigned	pte;
	unsigned	ram;
	unsigned	cam;
	int		rv = 0;

	svaddr = (char *)((unsigned)svaddr &~ 0xFFF);

	while (svaddr < evaddr) {
		pte = mmu_probe(svaddr, NULL);
/*
 * Only lock down valid supervisor mappings.
 */
		if (((pte&0x1B) == 0x1A)) {
/*
 * cam is just virtual tag and context number
 * we know svaddr has zeros in the proper low bits,
 * so no math is needed here, but keep CAM around
 * in case we want to fold in context number or
 * clear out some bits for short translation.
 */
			cam = (unsigned)svaddr;
/*
 * ram has physical page and cacheable bit that line
 * up with the pte, access modes are offset by one
 * bit, and we force the valid bit on. Also, if
 * the translation is writable, set the modified bit
 * to prevent any attempt at tlb writeback. Later
 * we may want to worry about short-translation bits,
 * but not quite yet.
 */
			ram = ((pte & 0xFFFFFF80) |
			    ((pte & 0x1C) << 1) |
			    0x01);
			if ((pte & 0x1C) == 0x1C)
				ram |= 0x40;
/*
 * Blast into the support routine.
 * If the lockdown failed, stop where
 * we are and return the count; else,
 * increment the count and continue.
 */
			if (ross_mmu_ltic_ramcam(ram, cam))
				break;
			rv ++;
		}
		svaddr += PAGESIZE;
	}
	return (rv);
}

/*
 * Describe SPARC capabilities (performance hints)
 */
/*ARGSUSED*/
static int
ross_get_hwcap_flags(int inkernel)
{
	return (0);
}
