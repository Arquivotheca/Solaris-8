/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mp_startup.c	1.62	99/11/20 SMI"

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/class.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/asm_linkage.h>
#include <sys/x_call.h>
#include <sys/systm.h>
#include <sys/var.h>
#include <sys/vtrace.h>
#include <vm/hat.h>
#include <sys/mmu.h>
#include <vm/as.h>
#include <vm/seg_kmem.h>
#include <sys/segment.h>
#include <sys/kmem.h>
#include <sys/stack.h>
#include <sys/smp_impldefs.h>
#include <sys/x86_archext.h>
#include <sys/cpupart.h>
#include <sys/pset.h>
#include <sys/machsystm.h>
#include <sys/traptrace.h>
#include <sys/clock.h>

struct cpu	cpus[1];			/* CPU data */
struct cpu	*cpu[NCPU] = {&cpus[0]};	/* pointers to all CPUs */
cpupartid_t 	cl_cpupartid[MAXNODES];
void    (*kadb_mpinit_ptr) (void) = 0;

/*
 * Useful for disabling MP bring-up for an MP capable kernel
 * (a kernel that was built with MP defined)
 */
int use_mp = 1;

int mp_cpus = 0x1;	/* to be set by platform specific module	*/

/*
 * This variable is used by the hat layer to decide whether or not
 * critical sections are needed to prevent race conditions.  For sun4m,
 * this variable is set once enough MP initialization has been done in
 * order to allow cross calls.
 */
int flushes_require_xcalls = 0;
u_long	cpu_ready_set = 1;

extern	void	real_mode_start(void);
extern	void	real_mode_end(void);
static 	void	mp_startup(void);
/*
 * functions to atomically manipulate i86mmu_cpusrunning mask
 * (defined in i86/vm/i86.il)
 */
extern void atomic_orl(unsigned long *addr, unsigned long val);
extern void atomic_andl(unsigned long *addr, unsigned long val);

extern int tsc_gethrtime_enable;

/*
 * Init CPU info - get CPU type info for processor_info system call.
 */
void
init_cpu_info(struct cpu *cp)
{
	register processor_info_t *pi = &cp->cpu_type_info;
	extern	int fpu_exists;
	extern	int cpu_freq;

	/* XXX Need review */
	/*
	 * Get clock-frequency property for the CPU.
	 */
	pi->pi_clock = cpu_freq;

	(void) strcpy(pi->pi_processor_type, "i386");
	if (fpu_exists)
		(void) strcpy(pi->pi_fputypes, "i387 compatible");
}


/*
 * Multiprocessor initialization.
 *
 * Allocate and initialize the cpu structure, TRAPTRACE buffer, and the
 * startup and idle threads for the specified CPU.
 */
static void
mp_startup_init(register int cpun)
{
	struct cpu *cp;
	kthread_id_t tp;
	caddr_t	sp;
	int	size, node;
	proc_t *procp;
	extern void idle();
	extern	int	x86_feature;
	extern	int	cr4_value;
	extern struct gate_desc idt[];
#ifdef	BCOPY_BUF
	extern int bcopy_buf;
#endif
	extern void init_intr_threads(struct cpu *);

	struct dscr *dscrp;
	struct cpu_tables *tablesp;
	rm_platter_t *real_mode_platter = (rm_platter_t *)rm_platter_va;

#ifdef TRAPTRACE
	trap_trace_ctl_t *ttc = &trap_trace_ctl[cpun];
#endif

	ASSERT(cpun < NCPU && cpu[cpun] == NULL);

	node = cpuid2nodeid(cpun);
	if ((cp = kmem_node_alloc(sizeof (*cp), KM_NOSLEEP, node)) == NULL)
		cmn_err(CE_PANIC, "mp_startup_init: can not allocate "
			"memory for cpu structure");
	bzero((caddr_t)cp, sizeof (*cp));
	procp = curthread->t_procp;

	/*
	 * Allocate and initialize the startup thread for this CPU.
	 * Interrupt and process switch stacks get allocated later
	 * when the CPU starts running.
	 */
	tp = thread_create(NULL, NULL, NULL, NULL, 0, procp,
	    TS_STOPPED, maxclsyspri);
	if (tp == NULL) {
		cmn_err(CE_PANIC,
	"mp_startup_init: Can't create startup thread for cpu: %d", cpun);
		/*NOTREACHED*/
	}

	/*
	 * Set state to TS_ONPROC since this thread will start running
	 * as soon as the CPU comes online.
	 *
	 * All the other fields of the thread structure are setup by
	 * thread_create().
	 */
	THREAD_ONPROC(tp, cp);
	tp->t_preempt = 1;
	tp->t_bound_cpu = cp;
	tp->t_affinitycnt = 1;
	tp->t_cpu = cp;


	/*
	 * Setup thread to start in mp_startup.
	 */
	sp = tp->t_stk;
	tp->t_pc = (u_int)mp_startup;
	tp->t_sp = (u_int)(sp - MINFRAME);

	cp->cpu_id = cpun;
	cp->cpu_mask = 1 << cpun;
	cp->cpu_thread = tp;
	cp->cpu_lwp = NULL;
	cp->cpu_dispthread = tp;
	cp->cpu_dispatch_pri = DISP_PRIO(tp);
#ifdef TRACE
	cp->cpu_trace.event_map = null_event_map;
#endif /* TRACE */

	/*
	 * Now, initialize per-CPU idle thread for this CPU.
	 */
	tp = thread_create(NULL, PAGESIZE, idle, NULL, 0, procp, TS_ONPROC, -1);
	if (tp == NULL) {
		cmn_err(CE_PANIC,
		"mp_startup_init: Can't create idle thread for cpu: %d", cpun);
		/*NOTREACHED*/
	}
	cp->cpu_idle_thread = tp;

	tp->t_preempt = 1;
	tp->t_bound_cpu = cp;
	tp->t_affinitycnt = 1;
	tp->t_cpu = cp;

	init_cpu_info(cp);

#ifdef	BCOPY_BUF
	cp->cpu_m.bcopy_res = bcopy_buf ? 0 : -1;
#endif

	/*
	 * Allocate virtual addresses for cpu_caddr1 and cpu_caddr2
	 * for each CPU.
	 */

	setup_vaddr_for_ppcopy(cp);

	/*
	 * allocate space for page directory,stack,tss,gdt and idt
	 * This assumes that kmem_alloc will return memory which is aligned
	 * to the next higher power of 2 or a page(if size > MAXABIG)
	 * If this assumption goes wrong at any time due to change in
	 * kmem alloc, things may not wrk as the page directory has to be
	 * page aligned
	 */
	tablesp =
	    (struct cpu_tables *)kmem_node_alloc(sizeof (struct cpu_tables),
		KM_NOSLEEP, node);
	bzero((caddr_t)tablesp, sizeof (struct cpu_tables));

	if (tablesp == NULL) {
		cmn_err(CE_PANIC,
		"mp_startup_init: Can't allocate memory for cpu tables "
		"for cpu: %d", cpun);
		/*NOTREACHED*/
	}

	if ((uint)tablesp & ~MMU_STD_PAGEMASK) {
	    kmem_free(tablesp, sizeof (struct cpu_tables));
	    size = sizeof (struct cpu_tables) + MMU_STD_PAGESIZE;
	    tablesp = (struct cpu_tables *)kmem_zalloc(size, KM_NOSLEEP);
	    tablesp = (struct cpu_tables *)(((uint)tablesp + MMU_STD_PAGESIZE) &
		MMU_STD_PAGEMASK);
	}

	cp->cpu_tss = &tablesp->ct_tss;

	cp->cpu_tss->t_esp0 = (ulong)&tablesp->ct_stack[0xffc];
	cp->cpu_tss->t_esp1 = (ulong)&tablesp->ct_stack[0xffc];
	cp->cpu_tss->t_esp2 = (ulong)&tablesp->ct_stack[0xffc];
	cp->cpu_tss->t_esp = (ulong)&tablesp->ct_stack[0xffc];
	cp->cpu_tss->t_ss0 = cp->cpu_tss->t_ss1 = (unsigned long)KDSSEL;
	cp->cpu_tss->t_ss2 = cp->cpu_tss->t_ss = (unsigned long)KDSSEL;
	cp->cpu_tss->t_es = cp->cpu_tss->t_ds = (unsigned long)KDSSEL;
	cp->cpu_tss->t_eip = (unsigned long)mp_startup;
	cp->cpu_tss->t_cs = (unsigned long)KCSSEL;
	cp->cpu_tss->t_fs = (unsigned long)KFSSEL;
	cp->cpu_tss->t_gs = (unsigned long)KGSSEL;
	cp->cpu_tss->t_bitmapbase = (unsigned long)0xDFFF0000;

	cp->cpu_gdt = &tablesp->ct_gdt[0];
	bcopy((caddr_t)CPU->cpu_gdt, (caddr_t)cp->cpu_gdt,
		GDTSZ*(sizeof (struct seg_desc)));
	/*
	 * Do we really need the entries for gdt and idt
	 * If so we need to modify those also.
	 */
	/* get fs entry */
	dscrp = (struct dscr *)(&cp->cpu_gdt[KFSSEL/sizeof (struct seg_desc)]);
	dscrp->a_base0015 = (u_int)(&cpu[cpun]) & 0xffff;
	dscrp->a_base1623 = ((u_int)(&cpu[cpun])>>16) & 0xff;
	dscrp->a_base2431 = ((u_int)(&cpu[cpun])>>24) & 0xff;

	/* get gs entry */
	dscrp = (struct dscr *)(&cp->cpu_gdt[KGSSEL/sizeof (struct seg_desc)]);
	dscrp->a_base0015 = (u_int)cp & 0xffff;
	dscrp->a_base1623 = ((u_int)cp >>16) & 0xff;
	dscrp->a_base2431 = ((u_int)cp >>24) & 0xff;

	/* get tss entry */
	dscrp = (struct dscr *)(&cp->cpu_gdt[UTSSSEL/sizeof (struct seg_desc)]);
	dscrp->a_acc0007 = TSS3_KACC1;	/* remove busy bit */
	dscrp->a_base0015 = (u_int)(cp->cpu_tss) & 0xffff;
	dscrp->a_base1623 = ((u_int)(cp->cpu_tss)>>16) & 0xff;
	dscrp->a_base2431 = ((u_int)(cp->cpu_tss)>>24) & 0xff;

	/* get tss entry */
	dscrp = (struct dscr *)(&cp->cpu_gdt[KTSSSEL/sizeof (struct seg_desc)]);
	dscrp->a_acc0007 = TSS3_KACC1;	/* remove busy bit */
	dscrp->a_base0015 = (u_int)(cp->cpu_tss) & 0xffff;
	dscrp->a_base1623 = ((u_int)(cp->cpu_tss)>>16) & 0xff;
	dscrp->a_base2431 = ((u_int)(cp->cpu_tss)>>24) & 0xff;


	if ((system_hardware.hd_nodes) && !(x86_feature & X86_P5)) {
	/*
	 * If we have more than one node, each cpu gets a copy of IDT
	 * local to its node. If this is a Pentium box, we use cpu 0's
	 * IDT. cpu 0's IDT has been made read-only to workaround the
	 * cmpxchgl register bug
	 */
	    cp->cpu_idt = (struct gate_desc *)
		kmem_node_alloc(IDTSZ * sizeof (struct gate_desc),
		KM_NOSLEEP, node);
	    if (cp->cpu_idt == NULL)
		cp->cpu_idt = CPU->cpu_idt;
	    else
		bcopy((caddr_t)&idt[0], (caddr_t)cp->cpu_idt,
		IDTSZ*(sizeof (struct gate_desc)));
	} else
		cp->cpu_idt = CPU->cpu_idt;

	setup_kernel_page_directory(cp);

	/* Should remove all entries for the current process/thread here */

	/*
	 * Fill up the real mode platter to make it easy for real mode code to
	 * kick it off. This area should really be one passed by boot to kernel
	 * and guarnteed to be below 1MB and aligned to 16 bytes. Should also
	 * have identical physical and virtual address in paged mode.
	 */

	real_mode_platter->rm_idt_base = cp->cpu_idt;
	real_mode_platter->rm_idt_lim = (IDTSZ * (sizeof (struct gate_desc)))
	    - 1;
	real_mode_platter->rm_gdt_base = cp->cpu_gdt;
	real_mode_platter->rm_gdt_lim = (GDTSZ * (sizeof (struct seg_desc))) -1;
	real_mode_platter->rm_pdbr = cp->cpu_cr3;
	real_mode_platter->rm_cpu = cpun;
	real_mode_platter->rm_x86feature = x86_feature;
	real_mode_platter->rm_cr4 = cr4_value;

#ifdef TRAPTRACE
	/*
	 * If this is a TRAPTRACE kernel, allocate TRAPTRACE buffers for this
	 * CPU.
	 */
	ttc->ttc_first = (uintptr_t)kmem_zalloc(trap_trace_bufsize, KM_SLEEP);
	ttc->ttc_next = ttc->ttc_first;
	ttc->ttc_limit = ttc->ttc_first + trap_trace_bufsize;
#endif

	/*
	 * Initialize the interrupt threads for this CPU
	 *
	 * We used to do it in mp_startup() - but that's just wrong
	 * - we might sleep while allocating stuff from segkp - that
	 * would leave us in a yucky state where we'd be handling
	 * interrupts without an interrupt thread .. see 1120597.
	 */
	init_intr_threads(cp);

	/*
	 * Record that we have another CPU.
	 */
	mutex_enter(&cpu_lock);
	/*
	 * Add CPU to list of available CPUs.  It'll be on the active list
	 * after mp_startup().
	 */
	cpu_add_unit(cp);
	mutex_exit(&cpu_lock);
}

static ushort *mp_map_warm_reset_vector();
static void mp_unmap_warm_reset_vector(ushort *warm_reset_vector);

/*ARGSUSED*/
void
start_other_cpus(int cprboot)
{
	unsigned who;
	int cpuid;
	int delays = 0;
	int mp_setfunc_done = 0;
	ushort *warm_reset_vector = NULL;
	extern int procset;

	/*
	 * Initialize our own cpu_info.
	 */
	init_cpu_info(CPU);

	cmn_err(CE_CONT, "!cpu 0 initialization complete - online\n");
	if (!use_mp) {
		cmn_err(CE_CONT, "?***** Not in MP mode\n");
		return;
	}

	cpuid = getbootcpuid();
	/*
	 * if only boot cpu present, return since no other cpu's to start
	 */
	if (!(mp_cpus & ~(1<<cpuid)))
		return;

	/*
	 * perform such initialization as is needed
	 * to be able to take CPUs on- and off-line.
	 */
	cpu_pause_init();

	xc_init();		/* initialize processor crosscalls */

	kpreempt_disable();
	for (who = 0; who < NCPU; who++) {
		if (who != cpuid) {
			if ((mp_cpus & (1 << who)) == 0)
				continue;	/* skip this CPU */

			/*
			 * Indicate that for the x86 architecture
			 * that cache flushes require x-calls.
			 */
			if (!mp_setfunc_done) {
				warm_reset_vector = mp_map_warm_reset_vector();
				if (!warm_reset_vector)
					return;

				bcopy((caddr_t)real_mode_start,
				    (caddr_t)((rm_platter_t *)
					rm_platter_va)->rm_code,
				    (size_t)((u_int)real_mode_end -
					(u_int)real_mode_start));

				mp_setfunc_done = 1;
				flushes_require_xcalls = 1;
			}

			mp_startup_init(who);
			(*cpu_startf)(who, rm_platter_pa);

			while ((procset & (1 << who)) == 0) {
				delay(1);
				delays++;
				if (delays > (20 * hz)) {
				    cmn_err(CE_WARN,
				    "cpu %d failed to start\n", who);
				    break;
				}
			}
			if (tsc_gethrtime_enable)
				tsc_sync_master(who);
		}
	}
	kpreempt_enable();

	for (who = 0; who < NCPU; who++) {
		if (who == cpuid)
			continue;

		if (!(procset & (1 << who)))
			continue;

		while (!(cpu_ready_set & (1 << who)))
			delay(1);
	}

	/*
	 * create system partitions.
	 */
	if (system_hardware.hd_nodes > 1) {
		struct cpu *cp;
		int 	node;

		for (who = 0; who < MAXNODES; who++)
			cl_cpupartid[who] = CP_NONE;

		for (who = 0; who < NCPU; who++) {
			if ((procset & (1 << who)) == 0)
				continue;	/* skip this CPU */
			mutex_enter(&cpu_lock);
			/* Effectively cpu_get() - covered by cpu_lock. */
			cp = cpu[who];
			node = cp->cpu_nodeid;
			mutex_exit(&cpu_lock);
			if ((cl_cpupartid[node] == CP_NONE) &&
			    cpupart_create(&cl_cpupartid[node], CP_SYSTEM))
				cmn_err(CE_PANIC, "failed to create "
				    "cpu partition for node %d\n",
				    node);

			mutex_enter(&cp_list_lock);
			mutex_enter(&cpu_lock);
			/* Effectively cpu_get() - covered by cpu_lock. */
			cp = cpu[who];
			ASSERT(node == cp->cpu_nodeid);
			if (cpupart_attach_cpu(cl_cpupartid[node], cp) != 0)
				cmn_err(CE_PANIC, "cannot attach cpu "
				    "%d to partition %d", cp->cpu_id,
				    cl_cpupartid[node]);
			mutex_exit(&cpu_lock);
			mutex_exit(&cp_list_lock);
		}
	}

	if (warm_reset_vector)
		mp_unmap_warm_reset_vector(warm_reset_vector);

}

/*
 * Dummy functions - no i86pc platforms support dynamic cpu allocation.
 */
/*ARGSUSED*/
int
mp_cpu_configure(int cpuid)
{
	return (ENOTSUP);		/* not supported */
}

/*ARGSUSED*/
int
mp_cpu_unconfigure(int cpuid)
{
	return (ENOTSUP);		/* not supported */
}

/*
 * Startup function for 'other' CPUs (besides boot cpu).
 * Resumed from cpu_startup.
 */
void
mp_startup(void)
{
	struct cpu *cp = CPU;
	extern int procset;
	extern void setup_mca();
	extern void add_cpunode2promtree();

	/*
	 * We need to Sync MTRR with cpu0's MTRR. We have to do
	 * this with interrupts disabled.
	 */
	if (x86_feature & X86_MTRR)
		mtrr_sync();
	/*
	 * Enable  machine check architecture
	 */
	if (x86_feature & X86_MCA)
		setup_mca();

	(void) add_cpunode2promtree(cp->cpu_id);

	/*
	 * clear boot page directory entry. This entry was used
	 * for real mode startup.
	 */
	(void) clear_bootpde(cp);

	(void) spl0();				/* enable interrupts */

	mutex_enter(&cpu_lock);
	procset |= 1 << cp->cpu_id;
	mutex_exit(&cpu_lock);

	if (tsc_gethrtime_enable)
		tsc_sync_slave();

	mutex_enter(&cpu_lock);
	cp->cpu_flags |= CPU_RUNNING | CPU_READY | CPU_ENABLE | CPU_EXISTS;
	cpu_add_active(cp);
	mutex_exit(&cpu_lock);

	if (kadb_mpinit_ptr)
		(*kadb_mpinit_ptr) ();

	/*
	 * Setting the bit in cpu_ready_set must be the last operation in
	 * processor initialization; the boot CPU will continue to boot once
	 * it sees this bit set for all active CPUs.
	 */
	atomic_orl(&cpu_ready_set, cp->cpu_mask);

	/*
	 * Because mp_startup() gets fired off after init() starts, we
	 * can't use the '?' trick to do 'boot -v' printing - so we
	 * always direct the 'cpu .. online' messages to the log.
	 */
	cmn_err(CE_CONT, "!cpu %d initialization complete - online\n",
	    cp->cpu_id);

	/*
	 * Now we are done with the startup thread, so free it up.
	 */
	thread_exit();
	cmn_err(CE_PANIC, "mp_startup: cannot return");
	/*NOTREACHED*/
}


/*
 * Start CPU on user request.
 */
/* ARGSUSED */
int
mp_cpu_start(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	if (cp->cpu_id == getbootcpuid())
		return (EBUSY); 	/* Cannot start boot CPU */
	return (0);
}

/*
 * Stop CPU on user request.
 */
/* ARGSUSED */
int
mp_cpu_stop(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	if (cp->cpu_id == getbootcpuid())
		return (EBUSY); 	/* Cannot stop boot CPU */

	return (0);
}

/*
 * Power on CPU.
 */
/* ARGSUSED */
int
mp_cpu_poweron(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (ENOTSUP);		/* not supported */
}

/*
 * Power off CPU.
 */
/* ARGSUSED */
int
mp_cpu_poweroff(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (ENOTSUP);		/* not supported */
}


/*
 * Take the specified CPU out of participation in interrupts.
 */
int
cpu_disable_intr(struct cpu *cp)
{

	/*
	 * cannot disable interrupt on boot cpu
	 */
	if (cp == cpu[getbootcpuid()])
		return (EBUSY);

	if (psm_disable_intr(cp->cpu_id) != DDI_SUCCESS)
		return (EBUSY);

	cp->cpu_flags &= ~CPU_ENABLE;
	return (0);
}

/*
 * Allow the specified CPU to participate in interrupts.
 */
void
cpu_enable_intr(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	if (cp == cpu[getbootcpuid()])
		return;

	cp->cpu_flags |= CPU_ENABLE;
	psm_enable_intr(cp->cpu_id);
}


/*	return the cpu id of the initial startup cpu			*/
processorid_t
getbootcpuid(void)
{
	return (0);
}

static ushort*
mp_map_warm_reset_vector()
{
	register ushort *warm_reset_vector;

	if (!(warm_reset_vector = (ushort *)psm_map_phys(WARM_RESET_VECTOR,
				sizeof (ushort *), PROT_READ|PROT_WRITE))) {
		return (NULL);
	}

/*	setup secondary cpu bios boot up vector				*/
	*warm_reset_vector = (ushort)((caddr_t)
		((struct rm_platter *)rm_platter_va)->rm_code - rm_platter_va
		+ ((ulong)rm_platter_va & 0xf));
	warm_reset_vector++;
	*warm_reset_vector = (ushort)(rm_platter_pa >> 4);

	--warm_reset_vector;
	return (warm_reset_vector);
}

static void
mp_unmap_warm_reset_vector(register ushort *warm_reset_vector)
{
	psm_unmap_phys((caddr_t)warm_reset_vector, sizeof (ushort *));
}
