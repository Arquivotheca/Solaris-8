/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mp_startup.c	1.97	99/09/09 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/thread.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/class.h>
#include <sys/cmn_err.h>
#include <sys/intreg.h>
#include <sys/debug.h>
#include <sys/x_call.h>
#include <sys/vtrace.h>
#include <sys/var.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/prom_debug.h>
#include <vm/hat_sfmmu.h>
#include <vm/as.h>
#include <vm/seg_kp.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/kmem.h>
#include <sys/disp.h>
#include <sys/callb.h>
#include <sys/platform_module.h>
#include <sys/stack.h>
#include <sys/obpdefs.h>
#include <sys/bitmap.h>

#ifdef TRAPTRACE
#include <sys/traptrace.h>
#include <sys/bootconf.h>
#endif /* TRAPTRACE */

#include <sys/cpu_sgnblk_defs.h>

struct cpu	cpu0;	/* the first cpu data; statically allocate */
struct cpu	*cpus;	/* pointer to other cpus; dynamically allocate */
struct cpu	*cpu[NCPU];		/* pointers to all CPUs */

#ifdef TRAPTRACE
caddr_t	ttrace_buf;	/* bop alloced traptrace for all cpus except 0 */
#endif /* TRAPTRACE */

/* bit mask of cpus ready for x-calls, protected by cpu_lock */
cpuset_t cpu_ready_set;

/* bit mask used to communicate with cpus during bringup */
static cpuset_t proxy_ready_set;

/*
 * cpu_bringup_set is a tunable (via /etc/system, debugger, etc.) that
 * can be used during debugging to control which processors are brought
 * online at boot time.  The variable represents a bitmap of the id's
 * of the processors that will be brought online.  The initialization
 * of this variable depends on the type of cpuset_t, which varies
 * depending on the number of processors supported (see cpuvar.h).
 */
cpuset_t cpu_bringup_set;

/*
 * Useful for disabling MP bring-up for an MP capable kernel
 * (a kernel that was built with MP defined)
 */
int use_mp = 1;			/* set to come up mp */

static void	slave_startup(void);

/*
 * Amount of time (in milliseconds) we should wait before giving up on CPU
 * initialization and assuming that the CPU we're trying to wake up is dead
 * or out of control.
 */
#define	CPU_WAKEUP_GRACE_MSEC 1000

/*
 * MP configurations may reserve additional interrupt request entries.
 * intr_add_{div,max} can be modified to tune memory usage.
 */

uint_t	intr_add_div = 1;			/* 1=worst case memory usage */
size_t	intr_add_max = (NCPU * INTR_POOL_SIZE);	/* (32*512)=16k bytes max */

/* intr_add_{pools,head,tail} calculated based on intr_add_{div,max} */

size_t	intr_add_pools = 0;			/* additional pools per cpu */
struct intr_req	*intr_add_head = (struct intr_req *)NULL;
#ifdef	DEBUG
struct intr_req	*intr_add_tail = (struct intr_req *)NULL;
#endif	/* DEBUG */

/*
 * Init CPU info - get CPU type info for processor_info system call.
 */
static void
init_cpu_info(struct cpu *cp)
{
	processor_info_t *pi = &cp->cpu_type_info;
	int cpuid = cp->cpu_id;
	struct cpu_node *cpunode = &cpunodes[cpuid];
	char buf[100];
	const char *cpu_info_fmt =
	    "?cpu%d: %s (upaid %d impl 0x%x ver 0x%x clock %d MHz)\n";

	cp->cpu_fpowner = NULL;		/* not used for V9 */

	/*
	 * Get clock-frequency property from cpunodes[] for the CPU.
	 */
	pi->pi_clock = (cpunode->clock_freq + 500000) / 1000000;

	(void) strcpy(pi->pi_processor_type, "sparcv9");
	(void) strcpy(pi->pi_fputypes, "sparcv9");

	(void) sprintf(buf, &cpu_info_fmt[1],
	    cpuid, cpunode->name, cpunode->upaid,
	    cpunode->implementation, cpunode->version, pi->pi_clock);

	cmn_err(CE_CONT, cpu_info_fmt,
	    cpuid, cpunode->name, cpunode->upaid,
	    cpunode->implementation, cpunode->version, pi->pi_clock);

	cp->cpu_m.cpu_info = kmem_alloc(strlen(buf) + 1, KM_SLEEP);
	(void) strcpy(cp->cpu_m.cpu_info, buf);

	/*
	 * StarFire requires the signature block stuff setup here
	 */
	CPU_SGN_MAPIN(cpuid);
	if (cpuid == cpu0.cpu_id) {
		/*
		 * cpu0 starts out running.  Other cpus are
		 * still in OBP land and we will leave them
		 * alone for now.
		 */
		SGN_UPDATE_CPU_OS_RUN_NULL(cpuid);
#ifdef	lint
		cpuid = cpuid;
#endif	/* lint */
	}
}


#ifdef	TRAPTRACE
/*
 * This function bop allocs traptrace buffers for all cpus
 * other than boot cpu.
 */
caddr_t
trap_trace_alloc(caddr_t base)
{
	caddr_t	vaddr;
	extern int max_ncpus;

	if (max_ncpus == 1) {
		return (base);
	}

	if ((vaddr = (caddr_t)BOP_ALLOC(bootops, base, (TRAP_TSIZE *
		(max_ncpus - 1)), TRAP_TSIZE)) == NULL) {
		cmn_err(CE_PANIC, "traptrace_alloc: can't bop alloc\n");
	}
	ttrace_buf = vaddr;
	PRM_DEBUG(ttrace_buf);
	return (vaddr + (TRAP_TSIZE * (max_ncpus - 1)));
}
#endif	/* TRAPTRACE */

/*
 * common slave cpu initialization code
 */
static int
common_startup_init(cpu_t *cp, int cpuid, boolean_t at_boot)
{
	proc_t *procp;
	kthread_id_t tp;
	sfmmu_t *sfmmup;
	caddr_t	sp;

	/*
	 * Allocate and initialize the startup thread for this CPU.
	 */
	procp = &p0;
	tp = thread_create(NULL, NULL, slave_startup, NULL, 0, procp,
	    TS_STOPPED, maxclsyspri);
	if (tp == NULL) {
		if (at_boot) {
			cmn_err(CE_PANIC, "common_startup_init: "
			    "Can't create startup thread for cpu: %d", cpuid);
			/*NOTREACHED*/
		} else {
			return (ENOMEM);
		}
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

	sfmmup = astosfmmu(&kas);
	CPUSET_ADD(sfmmup->sfmmu_cpusran, cpuid);

	/*
	 * Setup thread to start in slave_startup.
	 */
	sp = tp->t_stk;
	tp->t_pc = (uintptr_t)slave_startup - 8;
	tp->t_sp = (uintptr_t)((struct rwindow *)sp - 1) - STACK_BIAS;

	cp->cpu_id = cpuid;
	cp->cpu_thread = tp;
	cp->cpu_lwp = NULL;
	cp->cpu_dispthread = tp;
	cp->cpu_dispatch_pri = DISP_PRIO(tp);
#ifdef TRACE
	cp->cpu_trace.event_map = null_event_map;
#endif /* TRACE */

	return (0);
}

/*
 * parametric flag setting functions.  these routines set the cpu
 * state just prior to releasing the slave cpu.
 */
static void
cold_flag_set(int cpuid)
{
	cpu_t *cp;

	ASSERT(MUTEX_HELD(&cpu_lock));

	cp = cpu[cpuid];
	cp->cpu_flags |= CPU_RUNNING | CPU_ENABLE | CPU_EXISTS;
	cpu_add_active(cp);
	/*
	 * Add CPU_READY after the cpu_add_active() call
	 * to avoid pausing cp.
	 */
	cp->cpu_flags |= CPU_READY;		/* ready */
}

static void
warm_flag_set(int cpuid)
{
	cpu_t *cp;

	ASSERT(MUTEX_HELD(&cpu_lock));

	/*
	 * warm start activates cpus into the OFFLINE state
	 */
	cp = cpu[cpuid];
	cp->cpu_flags |= CPU_RUNNING | CPU_READY | CPU_EXISTS
		| CPU_OFFLINE | CPU_QUIESCED;
}

/*
 * Internal cpu startup sequencer
 * The sequence is as follows:
 *
 * MASTER	SLAVE
 * -------	----------
 * assume the kernel data is initialized
 * clear the proxy bit
 * start the slave cpu
 * wait for the slave cpu to set the proxy
 *
 *		the slave runs slave_startup and then sets the proxy
 *		the slave waits for the master to add slave to the ready set
 *
 * the master finishes the initialization and
 * adds the slave to the ready set
 *
 *		the slave exits the startup thread and is running
 */
static void
start_cpu(int cpuid, void(*flag_func)(int))
{
	extern caddr_t cpu_startup;
	int timout;
	dnode_t nodeid;

	ASSERT(MUTEX_HELD(&cpu_lock));

	/* get the prom handle for this cpu */
	nodeid = cpunodes[cpuid].nodeid;

	ASSERT(nodeid != (dnode_t)0);

	/* start the slave cpu */
	CPUSET_DEL(proxy_ready_set, cpuid);
	(void) prom_startcpu(nodeid, (caddr_t)&cpu_startup, cpuid);

	/* wait for the slave cpu to check in.  */
	for (timout = CPU_WAKEUP_GRACE_MSEC; timout; timout--) {
		if (CPU_IN_SET(proxy_ready_set, cpuid))
			break;
		DELAY(1000);
	}
	if (timout == 0) {
		cmn_err(CE_PANIC,
			"cpu %d node %x failed to start (2)\n",
			cpuid, nodeid);
	}

	/*
	 * deal with the cpu flags in a phase-specific manner
	 * for various reasons, this needs to run after the slave
	 * is checked in but before the slave is released.
	 */
	(*flag_func)(cpuid);

	/* release the slave */
	CPUSET_ADD(cpu_ready_set, cpuid);
}

#ifdef TRAPTRACE
int trap_tr0_inuse = 1;	/* it is always used on the boot cpu */
int trap_trace_inuse[NCPU];
#endif /* TRAPTRACE */

#define	cpu_next_free	cpu_prev

/*
 * Routine to set up a CPU to prepare for starting it up.
 */
int
setup_cpu_common(int cpuid)
{
	struct cpu *cp = NULL;
	kthread_id_t tp;
#ifdef TRAPTRACE
	int tt_index;
	TRAP_TRACE_CTL	*ctlp;
	caddr_t	newbuf;
#endif /* TRAPTRACE */

	extern void idle();
	extern void init_intr_threads(struct cpu *);

	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(cpu[cpuid] == NULL);

#ifdef TRAPTRACE
	/*
	 * allocate a traptrace buffer for this CPU.
	 */
	ctlp = &trap_trace_ctl[cpuid];
	if (!trap_tr0_inuse) {
		trap_tr0_inuse = 1;
		newbuf = trap_tr0;
		tt_index = -1;
	} else {
		for (tt_index = 0; tt_index < (max_ncpus-1); tt_index++)
			if (!trap_trace_inuse[tt_index])
			    break;
		if (tt_index >= (max_ncpus-1))
		    return (ENOMEM);	/* could only happen if more cpus */
					/* are added than max_ncpus */
		trap_trace_inuse[tt_index] = 1;
		newbuf = (caddr_t)(ttrace_buf + (tt_index * TRAP_TSIZE));
	}
	ctlp->d.vaddr_base = newbuf;
	ctlp->d.offset = ctlp->d.last_offset = 0;
	ctlp->d.limit = trap_trace_bufsize;
	ctlp->d.paddr_base = va_to_pa(newbuf);
	ASSERT(ctlp->d.paddr_base != (uint64_t)-1);
#endif /* TRAPTRACE */

	/*
	 * Obtain pointer to the appropriate cpu structure.
	 */
	if (cpu0.cpu_flags == 0) {
		cp = &cpu0;
	} else {
		/*
		 *  When dynamically allocating cpu structs,
		 *  cpus is used as a pointer to a list of freed
		 *  cpu structs.
		 */
		if (cpus) {
			/* grab the first cpu struct on the free list */
			cp = cpus;
			if (cp->cpu_next_free)
				cpus = cp->cpu_next_free;
			else
				cpus = NULL;
		}
	}

	if (cp == NULL) {
		cp = (struct cpu *)kmem_alloc(sizeof (struct cpu), KM_NOSLEEP);
		if (cp == NULL) {
#ifdef TRAPTRACE
			if (tt_index == -1)
				trap_tr0_inuse = 0;
			else
				trap_trace_inuse[tt_index] = 0;
			bzero(ctlp, sizeof (*ctlp));
#endif /* TRAPTRACE */
			return (ENOMEM);
		}
	}

	bzero(cp, sizeof (*cp));

	cp->cpu_id = cpuid;

	/*
	 * Now, initialize per-CPU idle thread for this CPU.
	 */
	tp = thread_create(NULL, PAGESIZE, idle, NULL, 0,
	    &p0, TS_ONPROC, -1);
	if (tp == NULL) {
		cp->cpu_id = 0;
#ifdef TRAPTRACE
		if (tt_index == -1)
			trap_tr0_inuse = 0;
		else
			trap_trace_inuse[tt_index] = 0;
		bzero(ctlp, sizeof (*ctlp));
#endif /* TRAPTRACE */
		return (ENOMEM);
	}
	cp->cpu_idle_thread = tp;

	tp->t_preempt = 1;
	tp->t_bound_cpu = cp;
	tp->t_affinitycnt = 1;
	tp->t_cpu = cp;

	/*
	 * Registering a thread in the callback table is usually
	 * done in the initialization code of the thread. In this
	 * case, we do it right after thread creation to avoid
	 * blocking idle thread while registering itself. It also
	 * avoids the possibility of reregistration in case a CPU
	 * restarts its idle thread.
	 */
	CALLB_CPR_INIT_SAFE(tp, "idle");

	init_cpu_info(cp);

	/*
	 * Initialize the interrupt threads for this CPU
	 *
	 * We used to do it in slave_startup() - but that's just wrong
	 * - we might sleep while allocating stuff from segkp - that
	 * would leave us in a yucky state where we'd be handling
	 * interrupts without an interrupt thread .. see 1120597.
	 */
	init_intr_pool(cp);
	init_intr_threads(cp);

	/*
	 * Add CPU to list of available CPUs.  It'll be on the
	 * active list after it is started.
	 */
	cpu_add_unit(cp);

	return (0);
}

/*
 * Routine to clean up a CPU after shutting it down.
 */
int
cleanup_cpu_common(int cpuid)
{
	struct cpu *cp;
#ifdef TRAPTRACE
	int i;
	TRAP_TRACE_CTL	*ctlp;
	caddr_t	newbuf;
#endif /* TRAPTRACE */
	extern void disp_cpu_fini(cpu_t *);	/* XXX  - disp.h */

	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(cpu[cpuid] != NULL);

	cp = cpu[cpuid];

	/*
	 * Remove CPU from list of available CPUs.
	 */
	cpu_del_unit(cpuid);

	/*
	 * Clean up the interrupt pool.
	 */
	cleanup_intr_pool(cp);

	/*
	 * At this point, the only threads bound to this CPU should be
	 * special per-cpu threads: it's idle thread, it's pause thread,
	 * and it's interrupt threads.  Clean these up.
	 */
	cpu_destroy_bound_threads(cp);

	/*
	 * Free the interrupt stack.
	 */
	segkp_release(segkp, cp->cpu_intr_stack);

#ifdef TRAPTRACE
	/*
	 * Free the traptrace buffer for this CPU.
	 */
	ctlp = &trap_trace_ctl[cpuid];
	newbuf = ctlp->d.vaddr_base;
	i = (newbuf - ttrace_buf) / TRAP_TSIZE;
	if (((newbuf - ttrace_buf) % TRAP_TSIZE == 0) &&
	    ((i >= 0) && (i < (max_ncpus-1)))) {
		/*
		 * This CPU got it's trap trace buffer from the
		 * boot-alloc'd bunch of them.
		 */
		trap_trace_inuse[i] = 0;
		bzero(newbuf, TRAP_TSIZE);
	} else if (newbuf == trap_tr0) {
		trap_tr0_inuse = 0;
		bzero(trap_tr0, TRAP_TSIZE);
	} else {
		cmn_err(CE_WARN, "Failed to free trap trace buffer from cpu%d",
		    cpuid);
	}
	bzero(ctlp, sizeof (*ctlp));
#endif /* TRAPTRACE */

	/*
	 * There is a race condition with mutex_vector_enter() which
	 * caches a cpu pointer. The race is detected by checking cpu_next.
	 */
	disp_cpu_fini(cp);
	ASSERT(cp->cpu_m.cpu_info != NULL);
	kmem_free(cp->cpu_m.cpu_info, strlen(cp->cpu_m.cpu_info) + 1);
	bzero(cp, sizeof (*cp));

	/*
	 * Place the freed cpu structure on the list of freed cpus.
	 */
	if (cp != &cpu0) {
		if (cpus) {
			cp->cpu_next_free = cpus;
			cpus = cp;
		}
		else
			cpus = cp;
	}

	return (0);
}

/*
 * Generic start-all cpus entry.  Typically used during cold initialization.
 * Note that cold start cpus are initialized into the online state.
 */
/*ARGSUSED*/
void
start_other_cpus(int flag)
{
	int i;
	extern int use_prom_stop;
	extern void idlestop_init(void);

	/*
	 * Check if cpu_bringup_set has been explicitly set before
	 * initializing it.
	 */
	if (CPUSET_ISNULL(cpu_bringup_set)) {
#ifdef MPSAS
		/* just CPU 0 */
		CPUSET_ADD(cpu_bringup_set, 0);
#else
		CPUSET_ALL(cpu_bringup_set);
#endif
	}

	mutex_enter(&cpu_lock);

	/*
	 * Initialize our own cpu_info.
	 */
	init_cpu_info(CPU);

#ifdef DEBUG
	if ((ncpunode > 1) && !use_prom_stop)
		cmn_err(CE_NOTE, "Not using PROM interface for cpu stop");
#endif /* DEBUG */

	/*
	 * perform such initialization as is needed
	 * to be able to take CPUs on- and off-line.
	 */
	cpu_pause_init();
	xc_init();		/* initialize processor crosscalls */
	idlestop_init();

	if (!use_mp) {
		mutex_exit(&cpu_lock);
		cmn_err(CE_CONT, "?***** Not in MP mode\n");
		return;
	}

	/*
	 * launch all the slave cpus now
	 */
	for (i = 0; i < NCPU; i++) {
		dnode_t nodeid = cpunodes[i].nodeid;
		int cpuid, mycpuid;

		if (nodeid == (dnode_t)0)
			continue;

		cpuid = UPAID_TO_CPUID(cpunodes[i].upaid);

		/*
		 * should we be initializing this cpu?
		 */
		mycpuid = getprocessorid();

		if (cpuid == mycpuid) {
			if (!CPU_IN_SET(cpu_bringup_set, cpuid)) {
				cmn_err(CE_WARN, "boot cpu not a member "
				    "of cpu_bringup_set, adding it");
				CPUSET_ADD(cpu_bringup_set, cpuid);
			}
			continue;
		}
		if (!CPU_IN_SET(cpu_bringup_set, cpuid))
			continue;

		ASSERT(cpu[cpuid] == NULL);

		if (setup_cpu_common(cpuid) != 0) {
			cmn_err(CE_PANIC, "Failed to setup internal "
			    "structures for cpu%d", cpuid);
		}

		(void) common_startup_init(cpu[cpuid], cpuid, B_TRUE);

		start_cpu(cpuid, cold_flag_set);
		/*
		 * Because slave_startup() gets fired off after init()
		 * starts, we can't use the '?' trick to do 'boot -v'
		 * printing - so we always direct the 'cpu .. online'
		 * messages to the log.
		 */
		cmn_err(CE_CONT, "!cpu %d initialization complete - online\n",
		    cpuid);
	}

	/*
	 * since all the cpus are online now, redistribute interrupts to them.
	 */
	intr_redist_all_cpus(intr_policy);

	mutex_exit(&cpu_lock);
}

#define	MAX_PROP_LEN	33	/* must be > strlen("cpu") */

/*
 * Routine used to setup a newly inserted CPU in preparation for starting
 * it running code.
 */
int
mp_cpu_configure(int cpuid)
{
	void fill_cpu(dnode_t);
	dnode_t nodeid;
	int upaid;
	char type[MAX_PROP_LEN];

	ASSERT(MUTEX_HELD(&cpu_lock));

	nodeid = prom_childnode(prom_rootnode());
	while (nodeid != OBP_NONODE) {
		if (prom_getproplen(nodeid, "device_type") < MAX_PROP_LEN)
			(void) prom_getprop(nodeid, "device_type",
				(caddr_t)type);
		else
			type[0] = '\0';
		(void) prom_getprop(nodeid, "upa-portid", (caddr_t)&upaid);
		if ((strcmp(type, "cpu") == 0) && (upaid == cpuid)) {
			goto found;
		}
		nodeid = prom_nextnode(nodeid);
	}
	return (ENODEV);

found:
	/*
	 * Note:  uses cpu_lock to protect cpunodes and ncpunodes
	 * which will be modified inside of fill_cpu.
	 */
	fill_cpu(nodeid);

	return (setup_cpu_common(cpuid));
}

/*
 * Routine used to cleanup a CPU that has been powered off.  This will
 * destroy all per-cpu information related to this cpu.
 */
int
mp_cpu_unconfigure(int cpuid)
{
	int retval;
	void empty_cpu(int);

	ASSERT(MUTEX_HELD(&cpu_lock));

	retval = cleanup_cpu_common(cpuid);

	empty_cpu(cpuid);

	return (retval);
}

/*
 * This routine is used to start a previously powered off processor.
 * Note that restarted cpus are initialized into the offline state.
 */
int
restart_other_cpu(int cpuid, boolean_t at_boot)
{
	struct cpu *cp;
	kthread_id_t tp;
	caddr_t	sp;
	extern void idle();
	int error;

	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(cpuid < NCPU && cpu[cpuid] != NULL);

	/*
	 * Obtain pointer to the appropriate cpu structure.
	 */
	cp = cpu[cpuid];

	if ((error = common_startup_init(cp, cpuid, at_boot)) != 0)
		return (error);

	/*
	 * idle thread t_lock is held when the idle thread is suspended.
	 * Manually unlock the t_lock of idle loop so that we can resume
	 * the suspended idle thread.
	 * Also adjust the PC of idle thread for re-retry.
	 */
	cp->cpu_on_intr = 0;	/* clear the value from previous life */
	cp->cpu_m.mutex_ready = 0; /* we are not ready yet */
	lock_clear(&cp->cpu_idle_thread->t_lock);
	tp = cp->cpu_idle_thread;

	sp = tp->t_stk;
	tp->t_sp = (uintptr_t)((struct rwindow *)sp - 1) - STACK_BIAS;
	tp->t_pc = (uintptr_t)idle - 8;

	/*
	 * restart the cpu now
	 */
	promsafe_pause_cpus();
	start_cpu(cpuid, warm_flag_set);
	start_cpus();

	/* call cmn_err outside pause_cpus/start_cpus to avoid deadlock */
	cmn_err(CE_CONT, "!cpu %d initialization complete - restarted\n",
	    cpuid);

	return (0);
}

/*
 * Startup function executed on 'other' CPUs.  This is the first
 * C function after cpu_start sets up the cpu registers.
 */
static void
slave_startup(void)
{
	struct cpu *cp = CPU;

	cp->cpu_m.mutex_ready = 1;
	cp->cpu_m.poke_cpu_outstanding = B_FALSE;
	(void) spl0();				/* enable interrupts */

	/* acknowledge that we are done with initialization */
	CPUSET_ADD(proxy_ready_set, cp->cpu_id);

	/*
	 * the slave will wait here forever -- assuming that the master
	 * will get back to us.  if it doesn't we've got bigger problems
	 * than a master not replying to this slave.
	 */
	while (!CPU_IN_SET(cpu_ready_set, cp->cpu_id))
		DELAY(100);
	/*
	 * StarFire requires the signature block update to indicate
	 * that this CPU is in OS now.
	 */
	SGN_UPDATE_CPU_OS_RUN_NULL(cp->cpu_id);

	/*
	 * Now we are done with the startup thread, so free it up.
	 */
	thread_exit();
	cmn_err(CE_PANIC, "slave_startup: cannot return");
	/*NOTREACHED*/
}

/*
 * 4163850 changes the allocation method for cpu structs. cpu structs
 * are dynamically allocated. This routine now determines if additional
 * per-cpu intr_req entries need to be allocated.
 */
caddr_t
ndata_alloc_cpus(caddr_t cpus_alloc_base)
{
	size_t real_sz;
	extern int max_ncpus;
	extern int niobus;
	extern int ecache_linesize;

	if (niobus > 1) {

		/*
		 * Allocate additional intr_req entries if we have more than
		 * one io bus.  The memory to allocate is calculated from four
		 * variables: niobus, max_ncpus, intr_add_div, and intr_add_max.
		 * Allocate multiple of INTR_POOL_SIZE bytes (512).  Each cpu
		 * already reserves 512 bytes in its machcpu structure, so the
		 * worst case is (512 * (niobus - 1) * max_ncpus) add'l bytes.
		 *
		 * While niobus and max_ncpus reflect the h/w, the following
		 * may be tuned (before boot):
		 *
		 *	intr_add_div -	divisor for scaling the number of
		 *			additional intr_req entries. use '1'
		 *			for worst case memory, '2' for half,
		 *			etc.
		 *
		 *   intr_add_max - upper limit on bytes of memory to reserve
		 */

		cpus_alloc_base = (caddr_t)roundup((uintptr_t)cpus_alloc_base,
		    ecache_linesize);

		real_sz = INTR_POOL_SIZE * (niobus - 1) * max_ncpus;

		/* tune memory usage by applying divisor and maximum */

		real_sz = MIN(intr_add_max, real_sz / MAX(intr_add_div, 1));

		/* round down to multiple of (max_ncpus * INTR_POOL_SIZE) */

		intr_add_pools = real_sz / (max_ncpus * INTR_POOL_SIZE);
		real_sz = intr_add_pools * (max_ncpus * INTR_POOL_SIZE);

		/* actually reserve the space */

		intr_add_head = (struct intr_req *)cpus_alloc_base;
		cpus_alloc_base += real_sz;
		PRM_DEBUG(intr_add_head);
#ifdef	DEBUG
		intr_add_tail = (struct intr_req *)cpus_alloc_base;
#endif	/* DEBUG */
	}

	return (cpus_alloc_base);
}
