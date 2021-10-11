/*
 * Copyright (c) 1990-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ross625.c	1.19	98/09/30 SMI"

/*
 * Support for modules based on the Ross Technology RT625 cache controller
 * and memory management unit and Ross Technology RT620 CPU.
 */

#include <sys/machparam.h>
#include <sys/module.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/pte.h>
#include <vm/hat_srmmu.h>
#include <vm/as.h>
#include <vm/page.h>
#include <sys/module_ross625.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/machcpuvar.h>

/*
 * External definitions
 */
extern int use_cache;				/* use cache at all */
extern int use_ic;				/* use instr cache at all */
extern int use_ec;				/* use instr cache at all */
extern int use_store_buffer;			/* enable write buffers */

extern int use_page_coloring;
extern int do_pg_coloring;



/*
 * Global definitions
 */
int	use_cache_wrap = 0;				/* enable cache wrap */
int	iflush_broadcast = 1;
int	rt620_iflush_broadcast_minimal = 1;
int	rt62x_rev_level = 0;
int	rt620_idc_present = 0;		/* Set if L1 dcache flush required */
int	rt625_cpu_ctx [ NCPU ] = {0};

/*
 * This is the alignment used to decide whether to use the alternate set
 * of vac routines. Set it to 0 to disable the check and feature
 */
int	rt625_alt_vac_set_align = MMU_STD_SEGMENTSIZE;

int ross625_module_identify(u_int mcr);
void ross625_module_setup(u_int mcr);
void ross625_turn_cache_on(int cpuid);
int ross625_mmu_writepte(struct pte *pte, u_int value, caddr_t addr,
    int level, int cxn, u_int flags);
int ross625_mp_mmu_writepte(struct pte *pte, u_int value, caddr_t addr,
    int level, int cxn, u_int flags);
void ross625_mmu_writeptp(struct ptp *ptp, u_int value, caddr_t addr, int level,
    int cxn, u_int flags);
int ross620_unimpflush(int my_cpu);	/* IFLUSH  - handle flush broadcasts */
static int get_rt62x_rev_level(void);
static void ross625_cache_init(void);

void ross625_vac_color_sync(u_int vaddr, u_int pfn);
void ross625_mp_vac_color_sync(u_int vaddr, u_int pfn);

/*
 * Externs
 */
extern int cache;			/* cache type */
extern u_int nctxs;			/* # of hardware contexts */
extern int vac_copyback;		/* copyback flag */
extern char *cache_mode;		/* cache mode */

extern int ross625;			/* Hypersparc present */
extern int virtual_bcopy;		/* use virtual bcopy */

/*
 * From ../ml/module_ross625_asm.s:
 */
extern void ross625_cache_init_asm(void);
extern u_int ross620_iccr_offon(u_int clrbits, u_int setbits);
extern void ross625_mmu_setctxreg(u_int c_num);
extern void ross625_mmu_getasyncflt(u_int *ptr);
extern int ross625_mmu_probe(caddr_t probe_val, u_int *fsr);

extern void ross625_vac_allflush(void);
extern void ross625_vac_ctxflush(int cxn, u_int flags);
extern void ross625_vac_rgnflush(caddr_t vaddr, u_int cxn, u_int flags);

/*
 * See SPECIAL NOTE in module_ros625_asm.s.
 * Two sets of these routines are needed. Here's the first set:
 */
extern void ross625_vac_set(void);
extern void ross625_vac_pageflush(caddr_t va, u_int cxn);
extern void ross625_vac_flush(caddr_t va, int len, u_int flags);
extern void ross625_vac_segflush(caddr_t vaddr, u_int cxn, u_int flags);
/*
 * and here's the second set
 */
extern void ross625_vac_set_alt(void);
extern void ross625_vac_pageflush_alt(caddr_t va, u_int cxn);
extern void ross625_vac_flush_alt(caddr_t va, int len);
extern void ross625_vac_segflush_alt(caddr_t vaddr, u_int cxn, int flags);

extern void ross620_ic_flush(void);  /* IFLUSH  - perform local icache flush */

extern void ross625_uncache_pt_page(caddr_t, u_int);

extern void ross625_window_overflow(void);
extern void ross625_window_underflow(void);

extern void ross625_noxlate_pgflush(caddr_t va, u_int pfn, u_int cxn);

extern void vac_color_flush(caddr_t vaddr, u_int pfn, u_int cxn);

/*
 * ross625_module_identify
 *
 * Return 1 if _mcr_ argument indicates an RT625-based module, 0 otherwise.
 */
int
ross625_module_identify(u_int mcr)
{
	return ((mcr & RT625_CTL_IDMASK) == RT625_CTL_ID);
}

/*
 * ross625_module_setup
 * Set vectors for running on an RT625-based module.
 */

/*ARGSUSED*/
void
ross625_module_setup(u_int mcr)
{
	int mask;

	/*
	 * Indicate we can use MAX_CTXS contexts
	 */
	nctxs = RT625_MAX_CTXS;

	/*
	 * Set up cache variable
	 */
	cache |= (CACHE_VAC | CACHE_IOCOHERENT | CACHE_PTAG);

	/*
	 * Use virtual bcopy code
	 */
	virtual_bcopy = 1;

	if (use_page_coloring)
		do_pg_coloring = PG_COLORING_ON;

	/*
	 * Set vectors for various routines we implement.
	 */

	/*
	 * PTE and PTP manipulation
	 */
	v_mmu_writepte = ross625_mmu_writepte;
	v_mp_mmu_writepte = ross625_mp_mmu_writepte;
	v_mmu_writeptp = ross625_mmu_writeptp;

	/*
	 * Cache manipulation
	 */
	v_turn_cache_on = ross625_turn_cache_on;
	v_cache_init = ross625_cache_init;

	/*
	 * Window overflow/underflow
	 */
	v_window_overflow = ross625_window_overflow;
	v_window_underflow = ross625_window_underflow;
	v_mp_window_overflow = ross625_window_overflow;
	v_mp_window_underflow = ross625_window_underflow;

	/*
	 * See the SPECIAL NOTE in module_625_asm.s.
	 * We need to use an alternate set of routines if the first set
	 * would have crossed a Level 2 page boundary (256K)
	 */
	mask = ~(rt625_alt_vac_set_align - 1);
	if (((unsigned)&ross625_vac_set & mask) ==
	    ((unsigned)&ross625_vac_set_alt & mask)) {
		v_vac_pageflush = ross625_vac_pageflush;
		v_vac_flush = ross625_vac_flush;
		v_vac_segflush = ross625_vac_segflush;
	} else {
		v_vac_pageflush = ross625_vac_pageflush_alt;
		v_vac_flush = ross625_vac_flush_alt;
		v_vac_segflush = ross625_vac_segflush_alt;
	}

	/*
	 * Cache/TLB flushes
	 */
	v_vac_usrflush = ross625_vac_allflush;
	v_vac_ctxflush = ross625_vac_ctxflush;
	v_vac_rgnflush = ross625_vac_rgnflush;
	v_vac_allflush = ross625_vac_allflush;

	v_vac_color_sync = ross625_vac_color_sync;
	v_mp_vac_color_sync = ross625_mp_vac_color_sync;
	v_vac_color_flush = ross625_noxlate_pgflush;

	/*
	 * Miscellaneous
	 */
	v_mmu_getasyncflt = ross625_mmu_getasyncflt;
	v_mmu_probe = ross625_mmu_probe;
	v_mmu_setctxreg = ross625_mmu_setctxreg;

	/*
	 * IFLUSH - support of unimplemented flush needed
	 * to broadcast the IFLUSH to other processors
	 */
	v_unimpflush = ross620_unimpflush; /* trap here */
	v_ic_flush = ross620_ic_flush;	/* ic_flush routine */

	rt62x_rev_level = get_rt62x_rev_level();
	ross625 = 1;		/* Allow cpu specific calls */

	{
		extern void (*level13_fasttrap_handler)(void);
		extern void ross625_xcall_medpri(void);

		level13_fasttrap_handler = ross625_xcall_medpri;
	}

	{
		isa_list = "sparcv8 sparcv8-fsmuld sparcv7 sparc";
	}

	/*
	 * Vectors we leave alone (and use the generic code):
	 *
	 *	v_mmu_getcr			- generic behavior
	 *	v_mmu_getctp			- generic behavior
	 *	v_mmu_getctx			- generic behavior
	 *	v_mmu_setcr			- generic behavior
	 *	v_mmu_setctp			- generic behavior
	 *
	 *	v_mmu_flushall			- generic behavior
	 *	v_mmu_flushctx			- generic behavior
	 *	v_mmu_flushrgn			- generic behavior
	 *	v_mmu_flushseg			- generic behavior
	 *	v_mmu_flushpage			- generic behavior
	 *	v_mmu_flushpagectx		- generic behavior
	 *
	 *	v_mmu_handle_ebe		- generic behavior
	 *	v_mmu_log_module_err		- generic behavior
	 *	v_mmu_chk_wdreset		- generic behavior
	 *
	 *	v_mmu_ltic			- fails -- not implemented
	 *	v_pac_flushall			- not applicable -- noop
	 *	v_pac_parity_chk_dis		- not applicable -- noop
	 *	v_pac_pageflush			- not applicable -- noop
	 *
	 */
}

/* ARGSUSED */
void
ross625_uncache_pt_page(caddr_t va, u_int pfn)
{
	vac_pageflush(va, KCONTEXT, FL_TLB_CACHE);
}

/*
 * ross625_cache_init
 *
 * Called by each cpu to initialize internal cache.  Update
 * processor revision global and call assembly routine to
 * do real work.
 */
void
ross625_cache_init()
{
	int	revision = get_rt62x_rev_level();

	/*
	 * Check revision level, and make rt62x_rev_level the oldest
	 * processor.
	 */
	if (revision < rt62x_rev_level)
		rt62x_rev_level = revision;

	ross625_cache_init_asm();
}

/*
 * ross625_turn_cache_on
 *
 * Should be called after cache has been initialized with
 * ross625_cache_init().
 */
/*ARGSUSED*/
void
ross625_turn_cache_on(int cpuid)
{
	static int calls = 0;
	static int setbits = 0;
	static int clrbits = 0;

	u_int cr;

	if (calls++ == 0) {
		if (use_cache && use_ec) {
			setbits |= (RT625_CTL_CE | RT625_CTL_SE);
			/*
			 * Set cache_mode appropriately for pretty printing of
			 * cache mode.
			 */
			if (vac_copyback)
				cache |= CACHE_WRITEBACK;
			cache_mode = vac_copyback ? "copyback":"write through";
		} else {
			clrbits |= (RT625_CTL_CE | RT625_CTL_SE);
			cache_mode = "disabled";
		}

		if (vac_copyback)
			setbits |= RT625_CTL_CM;
		else
			clrbits |= RT625_CTL_CM;

		if (use_store_buffer)
			setbits |= RT625_CTL_WBE;
		else
			clrbits |= RT625_CTL_WBE;

		if (use_cache_wrap)
			setbits |= RT625_CTL_CWE;
		else
			clrbits |= RT625_CTL_CWE;

		/*
		 * The cache variable was set in ross625_module_setup().
		 * Really should be done here, but that's too late.  :(
		 */
	}

	/*
	 * Set/clear bits and store the value
	 */
	cr = mmu_getcr();
	cr = (cr & ~clrbits) | setbits;
	mmu_setcr(cr);

	/*
	 * Flush icache, enable icache
	 *
	 */
	ross620_ic_flush();

	/*
	 * Decide on the use of ICE - Icache enable and
	 * Flush trap disable: philosophy is to leave the trap disabled
	 * unless we're using the ICACHE, and there is more than one
	 * cpu and iflush_broadcast is set
	 */
	if (use_cache && use_ic)
		cr = RT620_ICCR_ICE | RT620_ICCR_FTD;
	else
		cr = RT620_ICCR_FTD;

	/*
	 * turn off FTD(allow traps) if we could run MP and there
	 * other CPU's.
	 */
	if (use_mp && iflush_broadcast) {
		int i, count = 0;
		for (i = 0; i < NCPU; i++) {
			if (cpunodes[i].clock_freq != 0)
				count++;
		}
		if (count != 1)
			cr &= ~RT620_ICCR_FTD;
	}

	(void) ross620_iccr_offon(~0, cr);
}


/*
 * ross625_mp_mmu_writepte
 *
 * Write a PTE in an MP-safe way.
 */
int
ross625_mp_mmu_writepte(struct pte *pte, 	/* pointer to pte */
			u_int value, 		/* value to write */
			caddr_t addr,		/* virtual address mapped */
			int level,		/* level of mapping */
			int cxn,		/* context of mapping */
			u_int flags)		/* bits to keep */
{
	extern u_int srmmu_perms[];
	extern void ic_flush(void);

	u_int *ipte, old;
	int nvalid, local, operms, nperms;
	int vflushneeded;
	u_int no_pgflush;
	u_int rmkeep;

	no_pgflush = flags & SR_NOPGFLUSH;
	rmkeep = flags & ~SR_NOPGFLUSH;

	ASSERT((rmkeep & ~PTE_RM_MASK) == 0);
	ipte = (u_int *)pte;
	old = *ipte;

	/*
	 * If new value matches old pte entry, we may still
	 * need to flush local TLB, since a previous permission
	 * upgrade done by another cpu did not flush all TLBs.
	 */
	if (old == value || ((((old ^ value) & ~PTE_RM_MASK) == 0) &&
	    rmkeep == PTE_RM_MASK)) {
		if (PTE_ETYPE(old) == MMU_ET_PTE && cxn != -1)
			srmmu_tlbflush(level, addr, cxn, FL_LOCALCPU);
		return (old & PTE_RM_MASK);
	}

	/*
	 * If old pte was invalid, or we have no context,
	 * no flushing is necessary.  Just update memory pte.
	 *
	 * If old mapping was cacheable and SR_NOPGFLUSH flag
	 * was not on, set "vflushneeded" to signify we will
	 * need to do a vac cache flush.
	 */
	vflushneeded = (old & PTE_C_MASK) && !no_pgflush;

	if ((PTE_ETYPE(old) != MMU_ET_PTE) || (cxn == -1)) {
		value |= old & rmkeep;
		old |= swapl(value, (int *)ipte);

	} else if (vflushneeded) {
		/*
		 * Use CAPTURE/RELEASE protocol because srmmu_vacflush()
		 * routines require valid mappings.
		 */
		CAPTURE_CPUS;
		srmmu_vacflush(level, addr, cxn, FL_TLB_CACHE);
		value |= *ipte & rmkeep;
		old |= (u_int) swapl(value, (int *)ipte);
		RELEASE_CPUS;

	} else {
		/*
		 * Old pte mapping was valid.  We must always flush the
		 * local TLB.
		 *
		 * If the new pte value is just a permission upgrade
		 * from the previous value, we can skip flushing other
		 * processors.  We test this by comparing the RWX bits
		 * of the old & new permissions to make sure all the
		 * old bits remain set.  The new pte mapping must also
		 * be valid and for the same physical page.  This latter
		 * requirement is verified by the "rmkeep == PTE_RM_MASK"
		 * test.
		 *
		 * To avoid MP-race conditions, an invalid pte mapping
		 * must be temporarily set when doing cross-call flushes.
		 */
		nvalid = PTE_ETYPE(value) == MMU_ET_PTE;
		operms = srmmu_perms[(old & PTE_PERMMASK) >>
					PTE_PERMSHIFT];
		nperms = srmmu_perms[(value & PTE_PERMMASK) >>
					PTE_PERMSHIFT];
		local = nvalid && (operms & nperms) == operms &&
			rmkeep == PTE_RM_MASK;
		do {
			old |= swapl(MMU_STD_INVALIDPTE, (int *)ipte);
			if (local) {
				srmmu_tlbflush(level, addr, cxn,
					FL_LOCALCPU);
			} else {
				XCALL_PROLOG;
				srmmu_tlbflush(level, addr, cxn,
					FL_ALLCPUS);
				if (rt620_idc_present)
					ic_flush();
				XCALL_EPILOG;
			}
		} while (*ipte != MMU_STD_INVALIDPTE);

		value |= old & rmkeep;
		(void) swapl(value, (int *)ipte);
	}
	return (old & PTE_RM_MASK);
}


/*
 * ross625_mmu_writepte
 *
 * Uniprocessor function to write a PTE--only used on startup before
 * other CPUs are started and when running uniprocessor.
 */
int
ross625_mmu_writepte(struct pte *pte, 		/* pointer to pte */
			u_int value,		/* value to write */
			caddr_t addr,		/* virtual address mapped */
			int level,		/* level of mapping */
			int cxn,		/* context of mapping */
			u_int flags)		/* bits to keep */
{
	u_int old;
	int vflushneeded = 0;
	u_int no_pgflush;
	u_int rmkeep;

	no_pgflush = flags & SR_NOPGFLUSH;
	rmkeep = flags & ~SR_NOPGFLUSH;

	/*
	 * Don't let rmkeep keep anything except R & M bts
	 */
	ASSERT((rmkeep & ~PTE_RM_MASK) == 0);

	/*
	 * Compute new value based on old one and bits to keep
	 */
	old = *(u_int *)pte;
	value |= old & rmkeep;

	/*
	 * If we're replacing a valid mapping and we have a context,
	 * flush cache and TLB.  Then install new PTE and return
	 * the R & M bits of the old one.
	 */
	if (PTE_ETYPE(old) == MMU_ET_PTE && cxn != -1) {

		/*
		 * We try to avoid the VAC flush using the same method
		 * as in ross625_mp_mmu_writepte().
		 */
		if ((old & PTE_C_MASK) != 0 && !no_pgflush) {
			vflushneeded = 1;
		}
		if (vflushneeded)
			srmmu_vacflush(level, addr, cxn, FL_TLB_CACHE);
		else {
			srmmu_tlbflush(level, addr, cxn, FL_ALLCPUS);
			if (rt620_idc_present)
				ross620_ic_flush();
		}
	}

	(void) swapl(value, (int *)pte);

	return (old & PTE_RM_MASK);
}


/*
 * ross625_mmu_writeptp
 *
 * Write _value_ to _ptp_ for address _addr_, level _level_, in
 * context _cxn_.
 */
void
ross625_mmu_writeptp(struct ptp *ptp, u_int value, caddr_t addr, int level,
			int cxn, u_int flags)
{
	int old;

	old = *(int *)ptp;
	if (cxn != -1 && PTE_ETYPE(old) == MMU_ET_PTP) {
		if (flags & SR_NOPGFLUSH) {
			/*
			 * No VAC flush is required, update the ptp
			 * value and flush TLB.
			 */
			(void) swapl(value, (int *)ptp);
			XCALL_PROLOG;
			srmmu_tlbflush(level, addr, cxn, FL_ALLCPUS);
			XCALL_EPILOG;
		} else {
			/*
			 * VAC flush required.
			 *
			 * Level 0 & level 1 flushes do not need valid
			 * mappings, so the prolog/epilog sequence can
			 * be used.
			 *
			 * Level 2 flush routines check current mapping to
			 * see if a page flush is required.  This requires
			 * CAPTURE/RELEASE protocol.
			 */
			if (level <= 1) {
				(void) swapl(value, (int *)ptp);
				XCALL_PROLOG;
				srmmu_vacflush(level, addr, cxn, FL_TLB_CACHE);
				XCALL_EPILOG;
			} else {
				CAPTURE_CPUS;
				srmmu_vacflush(level, addr, cxn, FL_TLB_CACHE);
				(void) swapl(value, (int *)ptp);
				RELEASE_CPUS;
			}
		}
	} else
		(void) swapl(value, (int *)ptp);
}


/*
 * IFLUSH
 *
 * This routine receives the unimplemented flush trap for the Ross 625.
 * This is necessary in order to xcall to other processors so that
 * the ICACHE flush takes place in all processors. The first time that
 * we come here, if we discover that we're running UP, we'll disable
 * the trap so that it never happens again.
 *
 */
/*ARGSUSED*/
int
ross620_unimpflush(int my_cpu)	/* receive cpu number as arg */
{

	extern void ic_flush(void);

	if (!rt620_iflush_broadcast_minimal) {
		/*
		 * Here we punt by cross calling and flushing all cpus
		 */
	    XCALL_PROLOG;		/* get set for cross call	*/
	    ic_flush();			/* call ross620_ic_flush */
					/* via xc_sync_cache		*/
	    XCALL_EPILOG;		/* done with cross call	*/
	}
	/*
	 * Otherwise we build up a set of "other" cpus that are running in
	 * our context and just send cross calls to them. We can do this
	 * because the Version 8 spec says that the "target" of the IFLUSH
	 * must be visible on other CPUs, and since a "target" is a virtual
	 * address, this can only mean other processes running in our
	 * context.
	 *
	 * There might appear to be an apparent race here with other cpus
	 * going into and out of contexts. The secret here is that every
	 * change in context involves a full icache flush anyway, so
	 * either transition solves the problem as well.
	 */
	else {
	    /* We received our cpu pointer as an argument, get out ctx */
	    int my_ctx = rt625_cpu_ctx[my_cpu];
	    int c;
	    cpuset_t set = 0;		/* set of cpus to xcall */

	    for (c = 0; c < NCPU; c++) { /* for all cpus */
		/* check other cpus for same context */
		if (my_cpu != c && cpu[c] && rt625_cpu_ctx[c] == my_ctx)
		    /* add it to a set of CPUs in the same context */
		    CPUSET_ADD(set, c);
	    }
	    /* No matter what, we need to do this locally */
	    ross620_ic_flush();
	    if (set) {			/* anyone else? */
		xc_prolog(set);		/* set up for cross calls to others */
		ic_flush();		/* perform this elsewhere */
		xc_epilog();		/* cleanup from cross call */
	    }
	}
	return (IFLUSHDONEIMP);	/* 2 = we handled this		*/
				/* don't execute the instr */
}

/*
 * get rt62x revision level
 *
 * implements the following table:
 *
 *	RT620 Rev	PSR version	MCR version	VER
 *	-----------	-----------	-----------	----
 *
 *	C1-A0		0x1F		0x17		  0
 *	C1-A1		0x1F		0x17		  0
 *	C2-A2		0x1E		0x17		  1
 *	C2-A3		0x1E		0x17		  1
 *	C3-B0		0x1E		0x17		  3
 *	C3-C0		0x1E		0x17		  7
 *	C4-A0		0x1E		0x17	       4000
 */

#define	RT620_MODULE_ID	0xF000		/* Mask for C4 and later */

static int
get_rt62x_rev_level()
{
	extern int ross625_diag();
	int psr_version, version;
	extern void ross625_fiximp(void);

	psr_version = (getpsr() >> 24) & 0xf;

	/*
	 * A0 and A1 parts have same version code
	 *
	 * For A2 and later processors, check bits in
	 * %asr30 diag register for module type.  Colorado-4
	 * and later processors have id code in bits 12-15.
	 */
	if (psr_version == 0xf)
		version = 0;
	else
		version = ross625_diag() & 0xFFFF;

	if (version & RT620_MODULE_ID) {
		/*
		 * Module with internal data cache
		 *
		 * Set flag to force flush whenever TLB entry
		 * is invalidated.
		 */
		rt620_idc_present = 1;
		ross625_fiximp();
	} else
		version |= 1;		/* Must be at least A2/A3 */

	return (version);
}

/*
 * Hypersparc modules with an internal L1 data cache must be
 * flushed using a 32-byte stride regardless of external linesize
 * reported by OBP.  Also, vac alignment rules must be enforced
 * on a 512Kbyte boundary, even if the external cache is smaller.
 */

#define	RT620_L1_ALIGNSIZE	(512*1024)

void
ross625_fiximp(void)
{
	extern int vac_linesize, vac_nlines, vac_pglines;
	extern int vac_size;
	extern u_int vac_mask;
	extern int rt620_idc_present;

	if (rt620_idc_present) {
		if (vac_linesize == 64) {
			vac_linesize = 32;
			vac_nlines *= 2;
			vac_pglines = PAGESIZE / vac_linesize;
		}
		if (vac_size < RT620_L1_ALIGNSIZE)
			vac_mask = MMU_PAGEMASK & (RT620_L1_ALIGNSIZE - 1);
	}
}

#define	NOCTX	-1		/* No context -- do not flush TLB */

void
ross625_vac_color_sync(u_int vaddr, u_int pfn)
{
	vac_color_flush((caddr_t)vaddr, pfn, NOCTX);
}

void
ross625_mp_vac_color_sync(u_int vaddr, u_int pfn)
{
	XCALL_PROLOG;
	vac_color_flush((caddr_t)vaddr, pfn, NOCTX);
	XCALL_EPILOG;
}
