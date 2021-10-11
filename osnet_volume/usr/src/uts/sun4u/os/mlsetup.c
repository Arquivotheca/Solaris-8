/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mlsetup.c	1.69	99/07/21 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/disp.h>
#include <sys/autoconf.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/clock.h>
#include <sys/pte.h>
#include <sys/scb.h>
#include <sys/cpu.h>
#include <sys/stack.h>
#include <sys/eeprom.h>
#include <sys/intreg.h>
#include <sys/ivintr.h>
#include <vm/as.h>
#include <vm/hat_sfmmu.h>
#include <sys/reboot.h>
#include <sys/sysmacros.h>
#include <sys/vtrace.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/debug/debug.h>
#include <sys/privregs.h>
#include <sys/machpcb.h>
#include <sys/proc.h>
#include <sys/cpupart.h>
#include <sys/pset.h>
#include <sys/cpu_module.h>
#include <sys/copyops.h>
#include <sys/panic.h>
#include <sys/bootconf.h>	/* for bootops */

#include <sys/prom_debug.h>
#include <sys/debug.h>

#include <sys/iommu.h>
#include <sys/sunddi.h>

#ifdef TRAPTRACE
#include <sys/traptrace.h>
#endif /* TRAPTRACE */

/*
 * External Routines:
 */
extern void map_wellknown_devices(void);

int	dcache_size;
int	dcache_linesize;
int	icache_size;
int	icache_linesize;
int	ecache_size;
int	ecache_linesize;
int	itlb_entries;
int	dtlb_entries;
int	dcache_associativity = 1;
int	ecache_associativity = 1;
int	icache_associativity = 1;
int	vac_size;			/* cache size in bytes */
uint_t	vac_mask;			/* VAC alignment consistency mask */
int	vac_shift;			/* log2(vac_size) for ppmapout() */
int	dcache_line_mask;
int	vac = 0;	/* virtual address cache type (none == 0) */

/*
 * Static Routines:
 */
static int getintprop(dnode_t node, char *name, int deflt);
static void kern_splr_preprom(void);
static void kern_splx_postprom(void);

/*
 * Setup routine called right before main(). Interposing this function
 * before main() allows us to call it in a machine-independent fashion.
 */

void
mlsetup(struct regs *rp, void *cif, struct v9_fpu *fp)
{
	struct machpcb *mpcb;

	extern struct classfuncs sys_classfuncs;
	unsigned long long pa;

#ifdef TRAPTRACE
	TRAP_TRACE_CTL *ctlp;
#endif /* TRAPTRACE */

	/*
	 * initialize t0
	 */
	t0.t_stk = (caddr_t)rp - REGOFF;
	t0.t_pri = maxclsyspri - 3;
	t0.t_schedflag = TS_LOAD | TS_DONT_SWAP;
	t0.t_procp = &p0;
	t0.t_plockp = &p0lock.pl_lock;
	t0.t_lwp = &lwp0;
	t0.t_forw = &t0;
	t0.t_back = &t0;
	t0.t_next = &t0;
	t0.t_prev = &t0;
	t0.t_cpu = &cpu0;			/* loaded by _start */
	t0.t_disp_queue = &cpu0.cpu_disp;
	t0.t_bind_cpu = PBIND_NONE;
	t0.t_bind_pset = PS_NONE;
	t0.t_cpupart = &cp_default;
	t0.t_clfuncs = &sys_classfuncs.thread;
	t0.t_copyops = &default_copyops;
	THREAD_ONPROC(&t0, CPU);

	lwp0.lwp_thread = &t0;
	lwp0.lwp_procp = &p0;
	lwp0.lwp_regs = (void *)rp;
	t0.t_tid = p0.p_lwpcnt = p0.p_lwprcnt = p0.p_lwptotal = 1;

	mpcb = lwptompcb(&lwp0);
	mpcb->mpcb_fpu = fp;
	mpcb->mpcb_fpu->fpu_q = mpcb->mpcb_fpu_q;
	mpcb->mpcb_thread = &t0;
	lwp0.lwp_fpu = (void *)mpcb->mpcb_fpu;

	p0.p_exec = NULL;
	p0.p_stat = SRUN;
	p0.p_flag = SSYS;
	p0.p_tlist = &t0;
	p0.p_stksize = 2*PAGESIZE;
	p0.p_as = &kas;
	p0.p_lockp = &p0lock;
	p0.p_utraps = NULL;
	sigorset(&p0.p_ignore, &ignoredefault);

	CPU->cpu_thread = &t0;
	CPU->cpu_dispthread = &t0;
	CPU->cpu_idle_thread = &t0;
	CPU->cpu_disp.disp_cpu = CPU;
	CPU->cpu_flags = CPU_RUNNING;
	CPU->cpu_id = getprocessorid();
	CPU->cpu_dispatch_pri = t0.t_pri;

	/*
	 * Initialize lists of available and active CPUs.
	 */
	cpu_list_init(CPU);

#ifdef  TRACE
	CPU->cpu_trace.event_map = null_event_map;
#endif  /* TRACE */

	prom_init("kernel", cif);
	(void) prom_set_preprom(kern_splr_preprom);
	(void) prom_set_postprom(kern_splx_postprom);

	PRM_INFO("mlsetup: now ok to call prom_printf");

	/*
	 * Claim the physical and virtual resources used by panicbuf,
	 * then map panicbuf.  This operation removes the phys and
	 * virtual addresses from the free lists.
	 */
	if (prom_claim_virt(PANICBUFSIZE, panicbuf) != panicbuf)
		prom_panic("Can't claim panicbuf virtual address");

	if (prom_retain("panicbuf", PANICBUFSIZE, MMU_PAGESIZE, &pa) != 0)
		prom_panic("Can't allocate retained panicbuf physical address");

	if (prom_map_phys(-1, PANICBUFSIZE, panicbuf, pa) != 0)
		prom_panic("Can't map panicbuf");

	PRM_DEBUG(panicbuf);
	PRM_DEBUG(pa);

#ifdef TRAPTRACE
	/*
	 * initialize the trap trace buffer for the boot cpu
	 * XXX todo, dynamically allocate this buffer too
	 */
	ctlp = &trap_trace_ctl[CPU->cpu_id];
	ctlp->d.vaddr_base = trap_tr0;
	ctlp->d.offset = ctlp->d.last_offset = 0;
	ctlp->d.limit = TRAP_TSIZE;		/* XXX dynamic someday */
	ctlp->d.paddr_base = va_to_pa(trap_tr0);
#endif /* TRAPTRACE */

	cpu_setup();
	bootflags();		/* XXX - need RB_DEBUG */
	setcputype();
	map_wellknown_devices();
	setcpudelay();

#if !defined(MPSAS)
	/*
	 * If the boot flags say that kadb is there, test and see
	 * if it really is by peeking at dvec.
	 * [The peeking at dvec was removed as part of bug 4014598]
	 */
	if ((boothowto & RB_DEBUG) != 0) {
		if (dvec == NULL)
			/*
			 * Apparently the debugger isn't really there;
			 * just turn off the flag and drive on.
			 */
			boothowto &= ~RB_DEBUG;
		else if (dvec->dv_version != DEBUGVEC_VERSION_2) {
			/*
			 * dvec is not compatible with the kernel;
			 * print a message and panic.
			 */
			boothowto &= ~RB_DEBUG;
			cmn_err(CE_NOTE, "kadb/kernel version mismatch. "
			"kadb will not be operational.\n"
			"    debugvec version: kadb %d, kernel %d.",
				dvec->dv_version, DEBUGVEC_VERSION_1);
			/*
			 * The use of dvec is inconsistent.  RB_DEBUG
			 * is checked in some places and 'dvec != NULL'
			 * in others.  Set dvec to NULL here also.
			 */
			dvec = NULL;
			/*
			 * The kadb breakpoint trap table entry was copied
			 * to kadb_bpt by the locore.s start-up code.
			 * This is difficult to un-copy as the original
			 * contents of kadb_bpt would have to be saved to
			 * preserve the relative branch offsets.
			 * To save complication introduced by this version
			 * test it will be assumed that the kadb user will
			 * get the correct version of kadb installed.
			 * Testing has shown this to be a reasonable
			 * approach.
			 */
			panic("kadb/kernel version mismatch");
			/*NOTREACHED*/
		} else {
			/*
			 * We already patched the trap table in locore.s, so
			 * all the following does is allow "boot kadb -d"
			 * to work correctly.
			 */
			(*dvec->dv_scbsync)();
		}
	} else {
		/*
		 * This case should never happen.  kadb always adds
		 * '-d' to the boot flags.
		 */
		ASSERT(dvec == NULL);
	}
#endif
}

#if defined(DEBUG_FIXIMP) || defined(lint) || defined(__lint)
static int debug_fiximp = 0;
#define	PROM_PRINTF	if (debug_fiximp) prom_printf
#else
#define	PROM_PRINTF
#endif	/* DEBUG_FIXIMP */

static int
getintprop(dnode_t node, char *name, int deflt)
{
	int	value;

	switch (prom_getproplen(node, name)) {
	case 0:
		value = 1;	/* boolean properties */
		break;

	case sizeof (int):
		(void) prom_getprop(node, name, (caddr_t)&value);
		break;

	default:
		value = deflt;
		break;
	}

	return (value);
}

/*
 * Set the magic constants of the implementation
 */
void
fiximp_obp(dnode_t cpunode)
{
#ifdef FIXME
/*
 * There has to be a better way of dealing with the caches.  Come back to
 * this sometime.
 * XXX specially try to remove the need for so many vac related globals.
 * XXX do we need both vac_size and shm_alignment?
 * XXX do we need both vac and cache?
 */
#endif /* FIXME */
	int i, a;
	static struct {
		char	*name;
		int	*var;
	} prop[] = {
		"dcache-size",		&dcache_size,
		"dcache-line-size",	&dcache_linesize,
		"icache-size",		&icache_size,
		"icache-line-size",	&icache_linesize,
		"ecache-size",		&ecache_size,
		"ecache-line-size",	&ecache_linesize,
		"#itlb-entries",	&itlb_entries,
		"#dtlb-entries",	&dtlb_entries,
		"dcache-associativity", &dcache_associativity,
		"ecache-associativity", &ecache_associativity,
		"icache-associativity", &icache_associativity,

		};

	for (i = 0; i < sizeof (prop) / sizeof (prop[0]); i++) {
		PROM_PRINTF("property '%s' default 0x%x",
			prop[i].name, *prop[i].var);
		if ((a = getintprop(cpunode, prop[i].name, -1)) != -1) {
			*prop[i].var = a;
			PROM_PRINTF(" now set to 0x%x", a);
		}
		PROM_PRINTF("\n");
	}

	if (cache & CACHE_VAC) {
		int ivac_size = dcache_size/dcache_associativity;
		int dvac_size = icache_size/icache_associativity;

		vac_size = MAX(ivac_size, dvac_size);
		vac_mask = MMU_PAGEMASK & (vac_size - 1);
		i = 0; a = vac_size;
		while (a >>= 1)
			++i;
		vac_shift = i;
		dcache_line_mask = (dcache_size - 1) & ~(dcache_linesize - 1);
		PRM_DEBUG(vac_size);
		PRM_DEBUG(vac_mask);
		PRM_DEBUG(vac_shift);
		PRM_DEBUG(dcache_line_mask);
		shm_alignment = vac_size;
		vac = 1;
	} else {
		shm_alignment = PAGESIZE;
		vac = 0;
	}
}

/*
 * These routines are called immediately before and
 * immediately after calling into the firmware.  The
 * firmware is significantly confused by preemption -
 * particularly on MP machines - but also on UP's too.
 */

static int saved_spl;

static void
kern_splr_preprom(void)
{
	saved_spl = spl7();
}

static void
kern_splx_postprom(void)
{
	splx(saved_spl);
}
