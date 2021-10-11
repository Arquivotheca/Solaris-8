/*
 * Copyright (c) 1993,1994,1995,1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)swift.c	1.40	99/07/08 SMI"

#include <sys/machparam.h>
#include <sys/module.h>
#include <sys/cpu.h>
#include <sys/pte.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <vm/hat_srmmu.h>
#include <sys/sunddi.h>
#include <sys/debug.h>
#include <sys/openprom.h>
#include <sys/ddi_impldefs.h>

/*
 * Support for modules based on the:
 * 	Sun/Fujitsu microSPARC2 (Swift) CPU
 * 	FMI SR71 CPU
 *
 * The following Swift module versions are supported by this
 * module driver:
 *
 * 	IMPL	VERS	MASK	NAME		COMMENT
 *	0	4	0.0	swift_pg1.0	obsolete
 *	0	4	1.1	swift_pg1.1	proto systems only
 *	0	4	2.0	swift_pg2.3	fcs systems
 *	0	4	2.5	swift_pg2.5	fcs+3 systems
 *	0	4	3.0	swift_pg3.0	proto systems only
 *
 * The following SR71 module versions are supported by this
 * module driver:
 *
 * 	IMPL	VERS	MASK	NAME		COMMENT
 *	0	4/5	9.1	sr71_pg1.1	proto systems only (i$ broken)
 *	0	4/5	A.1	sr71_pg2.0	alpha systems
 */

/* XXX - belongs in module_{swift,sr71}.h */

/*
 * SR71 CPU Configuration Register
 */
#define	SR71_CCR_SE	0x00000008	/* secondary cache enable */
#define	SR71_CCR_MS2	0x00000010	/* ms2 compatibility mode */
#define	SR71_CCR_WT	0x00000020	/* writethru enable */
#define	SR71_CCR_SNP	0x40000000	/* io snoop enable */

/*
 * SR71/MS2 Module Control Register
 */
#define	MS2_MCR_IE	0x00000200	/* instruction cache enable */
#define	MS2_MCR_DE	0x00000100	/* data cache enable */
#define	MS2_MCR_PE	0x00040000	/* parity enable */


extern void	swift_vac_allflush();
extern void	swift_vac_usrflush();
extern void	swift_vac_ctxflush();
extern void	swift_vac_rgnflush();
extern void	swift_vac_segflush();
extern void	swift_vac_pageflush();
extern void	swift_vac_flush();
extern void	swift_uncache_pt_page();
extern void	small_sun4m_mmu_getasyncflt();
extern int	small_sun4m_mmu_chk_wdreset();
extern int	small_sun4m_ebe_handler();
extern void	small_sun4m_sys_setfunc();

extern void	srmmu_mmu_flushall();
extern void	srmmu_vacflush();
extern void	srmmu_tlbflush();
extern int	swift_mmu_probe();
static void	swift_mmu_writeptp();
static int	swift_mmu_writepte();
static int	swift_mmu_ltic();
static void	init_swift_idle_cpu();
static void	swift_idle_cpu();

extern u_int	swift_getversion();

extern void	swift_cache_init_asm();
extern u_int	sr71_getccr(void);
extern void	sr71_setccr(u_int);
extern void	sr71_cache_init_asm();

int		swift_module_identify(u_int mcr);
void		swift_module_setup(u_int mcr);

static void	swift_turn_cache_on(int cpuid);
static void	swift_cache_init();

static void	sr71_turn_cache_on(int cpuid);
static void	sr71_cache_init();

static power_req_t	pwr_req;
extern void		(*idle_cpu)();		/* defined in disp.c */

extern int	mmu_l3only;	/* use only level3 pte - no large mapping */
extern int 	cache;		/* cache type */
extern int	vac_copyback;	/* copyback flag */
extern char	*cache_mode;	/* cache mode for pretty print */

static u_int	swift_version = 0; /* cpu maskid */
static u_int	sr71 = 0;	/* running on sr71 */

#define	FEATURE_DISABLE 	0 	/* kernel disable feature */
#define	FEATURE_ENABLE		1	/* kernel enable feature */
#define	FEATURE_OBP		2	/* kernel preserve obp setting */
#define	FEATURE_PROP		3	/* kernel from obp property */

static u_int	sr71_writeback = FEATURE_PROP; /* use writeback */

#define	MCR_AP	0x10000	/* AFX page mode control bit in PCR */

#define	SWIFT_KDNC	/* workaround for 1156639 */
#define	SWIFT_KDNX	/* workaround for 1156640 */

#ifdef SWIFT_KDNC
int		swift_kdnc = -1;	/* don't cache kernel data space */
static int	swift_kdnc_inited = 0;	/* startup initialization done flag */
#endif SWIFT_KDNC

#ifdef SWIFT_KDNX
extern void	mmu_flushall();
extern int	swift_mmu_probe_kdnx();
int		swift_kdnx = -1;	/* don't mark non-memory executable */
int		swift_kdnx_inited = 0;	/* startup initialization done flag */
#endif

extern int	swift_tlb_flush_bug;

/*
 * This file supports both Swift and SR71 (in Swift compatibility mode).
 * Swift has maskid (version field) between 0x00-0x7f.
 * SR71 has maskid (version field) between 0x80-0xff.
 */


/*
 * Identify module and get maskid, determine if Swift or SR71
 */

int
swift_module_identify(u_int mcr)
{
	if (((mcr >> 24) & 0xff) == 0x04) {
		/* Now get the version of Swift. */
		swift_version = (swift_getversion() >> 24);

		if (swift_version >= 0x80)
			sr71 = 1;

		return (1);
	}

	return (0);
}


/*
 * Setup (attach?) Swift/SR71 module
 */

/*ARGSUSED*/
void
swift_module_setup(u_int mcr)
{
	int kdnc = 0;
	int kdnx = 0;
	u_int pcr;
	extern int use_page_coloring;
	extern int do_pg_coloring;
	extern int small_4m;

	/*
	 * Swift systems are small4m machines
	 */
	small_4m = 1;
	small_sun4m_sys_setfunc();

	/*
	 * MCR, CTPR, CTXR
	 */

	/* workaround for bug 1166390 - enable AFX page mode  */
	pcr = mmu_getcr();
	pcr |= MCR_AP;
	mmu_setcr(pcr);

	/* Swift is standard SRMMU */

	/*
	 * TLB PROBE, FLUSH
	 */
	v_mmu_probe = swift_mmu_probe;

	/* Swift is standard SRMMU */

	/* PTE, PTP update */
	v_mmu_writepte = swift_mmu_writepte;
	v_mmu_writeptp = swift_mmu_writeptp;

	/* SFSR, SFAR, EBE, AFSR, AFAR, WDOG */
	v_mmu_handle_ebe = small_sun4m_ebe_handler;
	v_mmu_getasyncflt = small_sun4m_mmu_getasyncflt;
	v_mmu_chk_wdreset = small_sun4m_mmu_chk_wdreset;

	/* Swift is standard SRMMU */

	/* TRCR */
	v_mmu_ltic = swift_mmu_ltic;

	/* PAC INIT, FLUSHALL, PARITY? */

	/* Swift/SR71 are VAC module(s) */

	/* VAC INIT, FLUSH, CONTROL */
	cache |= (CACHE_VAC | CACHE_VTAG);

	/*
	 * VAC/TLB FLUSH. v_vac_XXX() always flushes the cache.
	 * If you need to flush the TLB, pass FLUSH_TLB as the flags.
	 */
	if (sr71) {
		v_cache_init = sr71_cache_init;
		v_turn_cache_on = sr71_turn_cache_on;
	} else {
		v_cache_init = swift_cache_init;
		v_turn_cache_on = swift_turn_cache_on;

		/* Swift needs to flush entire tlb for iommu */
		swift_tlb_flush_bug = 1;
	}


	v_vac_usrflush = swift_vac_usrflush;
	v_vac_ctxflush = swift_vac_ctxflush;
	v_vac_rgnflush = swift_vac_rgnflush;
	v_vac_segflush = swift_vac_segflush;
	v_vac_pageflush = swift_vac_pageflush;
	v_vac_flush = swift_vac_flush;
	v_vac_allflush = swift_vac_allflush;
	v_uncache_pt_page = swift_uncache_pt_page;

	switch (swift_version) {
	case 0x00:
		/* P1.0 Swifts. See bug id # 1139511 */
		v_mmu_flushseg = srmmu_mmu_flushall;
		v_mmu_flushrgn = srmmu_mmu_flushall;
		v_mmu_flushctx = srmmu_mmu_flushall;
		v_vac_segflush = swift_vac_allflush;
		v_vac_rgnflush = swift_vac_allflush;
		v_vac_ctxflush = swift_vac_allflush;
		mmu_l3only = 1;
		kdnc = 1;
		kdnx = 1;
		break;
	case 0x11:
		/* PG1.1 Swift. */
		mmu_l3only = 1;
		kdnc = 1;
		kdnx = 1;
		break;
	case 0x20:
		/* PG2.0 Swift. */
		mmu_l3only = 1;
		kdnc = 1;
		kdnx = 1;
		break;
	case 0x23:
		/* PG2.3 Swift. */
		mmu_l3only = 1;
		kdnc = 1;
		kdnx = 1;
		break;
	case 0x25:
		/* PG2.5 Swift. */
		kdnx = 1;
		break;
	case 0x30:
		/* PG3.0 Swift. */
		mmu_l3only = 1;
		kdnc = 1;
		kdnx = 1;
		break;
	case 0x31:
		/* PG3.1 Swift. */
		kdnx = 1;
		break;
	case 0x32:
		/* PG3.2 Swift. */
		break;
	default:
		/* Should not get here. */
		break;
	}

	nctxs = 256; /* XXX - why is this here? */

#ifdef SWIFT_KDNC
	if (swift_kdnc == -1)
		swift_kdnc = kdnc;
#endif SWIFT_KDNC

#ifdef SWIFT_KDNX
	if (swift_kdnx == -1) {
		swift_kdnx = kdnx;
		if (swift_kdnx == 1)
			v_mmu_probe = swift_mmu_probe_kdnx;
	}
#endif SWIFT_KDNX

	/*
	 * Indicate if we want page coloring.
	 */
	if (use_page_coloring)
		do_pg_coloring = 0;	/* disable until this works */

	/* Replace generic idle function (in disp.c) with our own */
	idle_cpu = init_swift_idle_cpu;

	if (sr71)
		isa_list = "sparcv8 sparcv8-fsmuld sparcv7 sparc";
	else
		isa_list = "sparcv8-fsmuld sparcv7 sparc";
}

#ifdef	SWIFT_KDNX
/*
 * Remove execute permission from a PTE.
 */
void
swift_kdnx_fix_pte(struct pte *ptep)
{

	switch (ptep->AccessPermissions) {
		/*
		 * ACC 2,4,6 -> 0
		 */
		case MMU_STD_SRXURX:
		case MMU_STD_SXUX:
		case MMU_STD_SRX:
			ptep->AccessPermissions = MMU_STD_SRUR;
			break;
		/*
		 * ACC 3 -> 1
		 */
		case MMU_STD_SRWXURWX:
			ptep->AccessPermissions = MMU_STD_SRWURW;
			break;
		/*
		 * ACC 7 -> 5
		 */
		case MMU_STD_SRWX:
			ptep->AccessPermissions = MMU_STD_SRWUR;
			break;
		default:
			break;
	}
}

/*
 * Cycle thru the mappings within the monitor's virtual address range and
 * remove execute permissions on all mappings to non-main memory space.
 */
void
swift_kdnx_init()
{
	extern struct as kas;
	struct pte *ptep;
	caddr_t addr;
	int level;
	u_int value;

	for (addr = (caddr_t)SUNMON_START; addr < (caddr_t)SUNMON_END;
			addr += PAGESIZE) {
		ptep = srmmu_ptefind_nolock(&kas, addr, &level);
		if (!ptep)
			continue;
		value = *(u_int *)ptep;
		if ((PTE_ETYPE(value) == MMU_ET_PTE) && (value & 0x0f000000))
			swift_kdnx_fix_pte(ptep);
	}
	mmu_flushall();
	swift_kdnx_inited = 1;
}
#endif	SWIFT_KDNX

#ifdef	SWIFT_KDNC

/*
 * Uncache mappings from _etext to _end.
 */
void
swift_kdnc_init()
{
	extern char _etext[], _end[];
	extern struct as kas;
	struct pte *ptep;
	caddr_t addr, end_addr;
	int level;

	addr = MMU_L3_BASE((caddr_t)&_etext);
	end_addr = MMU_L3_BASE((caddr_t)&_end);
	for (; addr <= end_addr; addr += PAGESIZE) {
		ptep = srmmu_ptefind_nolock(&kas, addr, &level);
		if (!ptep)
			continue;
		ptep->Cacheable = 0;
	}
	mmu_flushall();
	swift_kdnc_inited = 1;
}
#endif	SWIFT_KDNC

/*
 * The Swift writepte function
 */

#define	PFN_C_V_MASK 0xFFFFFF9F /* check pfn, cache bit, acc type, entry type */

int
swift_mmu_writepte(
	struct pte *pte,
	u_int	value,
	caddr_t	addr,
	int	level,
	int	cxn,
	u_int	rmkeep)
{
	u_int old, *ipte;
	int vcache;

#ifdef SWIFT_KDNC
	extern char _end[];

	if (swift_kdnc == 1) {
		if (swift_kdnc_inited == 0)
			swift_kdnc_init();

		/* If kernel mapping and writable then don't cache it. */
		if ((addr >= (caddr_t)&_end) &&
			(((value >> PTE_PERMSHIFT) & 1) == 1))
			value &= ~0x80;
	}
#endif SWIFT_KDNC

#ifdef SWIFT_KDNX
	if (swift_kdnx == 1) {
		if (swift_kdnx_inited == 0)
			swift_kdnx_init();

		/* If non-memory mapping then remove execute permission. */
		if ((PTE_ETYPE(value) == MMU_ET_PTE) && (value & 0x0f000000))
			swift_kdnx_fix_pte((struct pte *)&value);
	}
#endif	SWIFT_KDNX

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

		if (vcache)
			srmmu_vacflush(level, addr, cxn, FL_TLB_CACHE);
		else
			srmmu_tlbflush(level, addr, cxn, FL_ALLCPUS);
	}
	value |= old & rmkeep;
	(void) swapl(value, (int *)ipte);
	return (old & PTE_RM_MASK);
}

/*
 * The Swift writeptp function.
 */

void
swift_mmu_writeptp(
	struct ptp *ptp,
	u_int	value,
	caddr_t	addr,
	int	level,
	int	cxn)
{
	u_int *iptp, old;

	iptp = (u_int *)ptp;
	old = *iptp;

	/* Install new ptp before TLB flush. */
	(void) swapl(value, (int *)iptp);

	if (PTE_ETYPE(old) == MMU_ET_PTP && cxn != -1) {
		srmmu_vacflush(level, addr, cxn, FL_TLB_CACHE);
	}
}

/*
 * Uncache a page of memory and flush the TLB that maps this page.
 */
/*ARGSUSED*/
void
swift_uncache_pt_page(caddr_t va, u_int pfn)
{
	vac_pageflush(va, KCONTEXT, FL_TLB_CACHE);
}

/*
 * These functions are called when the idle loop can't find anything better
 * to run. It uses the platform specific power manager power routine to
 * power off the CPU. The first function saves the power function args
 * so then the optimized second function can be used instead.
 */
static void
init_swift_idle_cpu()
{
	extern int (*pm_platform_power)(power_req_t *);

	pwr_req.request_type = PMR_SET_POWER;
	pwr_req.req.set_power_req.who = ddi_root_node();
	pwr_req.req.set_power_req.cmpt = 1;
	pwr_req.req.set_power_req.level = 0;
	idle_cpu = swift_idle_cpu;
	(void) (*pm_platform_power)(&pwr_req);
}

static void
swift_idle_cpu()
{
	extern int (*pm_platform_power)(power_req_t *);

	(void) (*pm_platform_power)(&pwr_req);
}

/*
 * This is a do nothing routine.
 * The routine that used to be here did not work for Swift.
 */
int
swift_mmu_ltic()
{
	return (0);
}

/*
 * swift_cache_init
 *
 * Called to initialize internal cache.
 */
void
swift_cache_init()
{
	swift_cache_init_asm();
}


/*
 * swift_turn_cache_on
 *
 * Should be called after cache has been initialized with
 * swift_cache_init().
 */
/*ARGSUSED*/
void
swift_turn_cache_on(int cpuid)
{
	static int calls = 0;
	int mcr_setbits = 0;
	int mcr_clrbits = 0;
	u_int cr;

	if (calls++ == 0) {
		if (use_cache)
			cache_mode = "write through";
		else
			cache_mode = "disabled";

		/*
		 * The cache variable was set in swift_module_setup().
		 * Really should be done here, but that's too late.  :(
		 */
	}

	if (use_cache && use_ic)
		mcr_setbits |= MS2_MCR_IE;
	else
		mcr_clrbits |= MS2_MCR_IE;

	if (use_cache && use_dc)
		mcr_setbits |= MS2_MCR_DE;
	else
		mcr_clrbits |= MS2_MCR_DE;

	/*
	 * Set/clear bits and store the value
	 */
	cr = mmu_getcr();
	cr = (cr & ~mcr_clrbits) | mcr_setbits;
	(void) mmu_setcr(cr);
}

/*
 * sr71_cache_init
 *
 * Called to initialize internal cache(s).
 * Determine cache and snoop mode based on flags and hardware settings.
 */
void
sr71_cache_init()
{
	sr71_cache_init_asm();
}



/*
 * sr71_turn_cache_on
 *
 * Should be called after cache has been initialized with
 * sr71_cache_init().
 */
/*ARGSUSED*/
void
sr71_turn_cache_on(int cpuid)
{
	static int calls = 0;
	int ccr_setbits = 0;
	int ccr_clrbits = 0;
	int mcr_setbits = 0;
	int mcr_clrbits = 0;
	int writeback;

	u_int cr;
	u_int ccr;

	if (calls++ == 0) {

		/*
		 * Determine current hw settings (set by OBP prior to boot)
		 */
		ccr = sr71_getccr();

		/*
		 * Determine property setting (for overrides)
		 */

		switch (sr71_writeback) {

		case FEATURE_ENABLE:
			writeback = 1;
			break;

		case FEATURE_DISABLE:
			writeback = 0;
			break;

		case FEATURE_OBP:
			writeback = !(ccr & SR71_CCR_WT);
			break;


		/*
		 * even though writeback dcache works the obp cannot
		 * enable as default so as to be compatible with
		 * existing SunOS/Solaris CD releases eg: 4.1.4/2.[345]
		 * You can't patch a CD..
		 */
		case FEATURE_PROP:
			if (ddi_prop_exists(DDI_DEV_T_ANY, ddi_root_node(),
			    DDI_PROP_DONTPASS, "writeback-bug?") == 1) {
				writeback = 0;
			} else
				writeback = 1;
			break;

		default:
			break;
		}
		if (writeback)
			ccr_clrbits |= SR71_CCR_WT;
		else
			ccr_setbits |= SR71_CCR_WT;

		if (use_cache) {
			/*
			 * Set cache_mode appropriately for pretty printing of
			 * cache mode.
			 */
			if (writeback)
				cache |= (CACHE_WRITEBACK|CACHE_IOCOHERENT);
			cache_mode = writeback ? "writeback":"write through";
		} else {
			cache_mode = "disabled";
		}

		/*
		 * The cache variable was set in swift_module_setup().
		 * Really should be done here, but that's too late.  :(
		 */
	}

	/*
	 * Set/clear bits and store the value
	 */
	ccr = sr71_getccr();
	ccr = (ccr & ~ccr_clrbits) | ccr_setbits;
	sr71_setccr(ccr);

	if (use_cache && use_ic)
		mcr_setbits |= MS2_MCR_IE;
	else
		mcr_clrbits |= MS2_MCR_IE;

	if (use_cache && use_dc)
		mcr_setbits |= MS2_MCR_DE;
	else
		mcr_clrbits |= MS2_MCR_DE;

	/*
	 * Set/clear bits and store the value
	 */
	cr = mmu_getcr();
	cr = (cr & ~mcr_clrbits) | mcr_setbits;
	(void) mmu_setcr(cr);
}
