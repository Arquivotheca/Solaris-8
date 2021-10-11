/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpu.c	1.94	99/10/09 SMI"

/*
 * Architecture-independent CPU control functions.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/var.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/kstat.h>
#include <sys/uadmin.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/procset.h>
#include <sys/processor.h>
#include <sys/debug.h>
#include <sys/cpupart.h>
#include <sys/pset.h>
#include <sys/kmem.h>
#include <sys/kmem_impl.h>	/* to set per-cpu kmem_cache offset */
#include <sys/atomic.h>
#include <sys/callb.h>
#include <sys/vtrace.h>
#include <sys/cyclic.h>

extern int	mp_cpu_start(cpu_t *);
extern int	mp_cpu_stop(cpu_t *);
extern int	mp_cpu_poweron(cpu_t *);
extern int	mp_cpu_poweroff(cpu_t *);
extern int	mp_cpu_configure(int);
extern int	mp_cpu_unconfigure(int);

static void cpu_add_active_internal(cpu_t *cp);
static void cpu_remove_active(cpu_t *cp);
static void cpu_stat_kstat_create(cpu_t *cp);
static void cpu_info_kstat_create(cpu_t *cp);
static void cpu_info_kstat_destroy(cpu_t *cp);

/*
 * cpu_lock protects ncpus, ncpus_online, cpu_flag, cpu_list, cpu_active,
 * and dispatch queue reallocations
 *
 * Warning:  Certain sections of code do not use the cpu_lock when
 * traversing the cpu_list (e.g. mutex_vector_enter(), clock()).  Since
 * all cpus are paused during modifications to this list, a solution
 * to protect the list is too either disable kernel preemption while
 * walking the list, *or* recheck the cpu_next pointer at each
 * iteration in the loop.  Note that in no cases can any cached
 * copies of the cpu pointers be kept as they may become invalid.
 */
kmutex_t	cpu_lock;
cpu_t		*cpu_list;		/* list of all CPUs */
cpu_t		*cpu_active;		/* list of active CPUs */
static cpuset_t	cpu_available;		/* set of available CPUs */
cpuset_t	cpu_seqid_inuse;	/* which cpu_seqids are in use */

/*
 * max_ncpus keeps the max cpus the system can have. Initially
 * it's NCPU, but since most archs scan the devtree for cpus
 * fairly early on during boot, the real max can be known before
 * ncpus is set (useful for early NCPU based allocations).
 */
int max_ncpus = NCPU;
/*
 * platforms that set max_ncpus to maxiumum number of cpus that can be
 * dynamically added will set boot_max_ncpus to the number of cpus found
 * at device tree scan time during boot.
 */
int boot_max_ncpus = -1;

int ncpus = 1;
int ncpus_online = 1;

/*
 * values for safe_list.  Pause state that CPUs are in.
 */
#define	PAUSE_IDLE	0		/* normal state */
#define	PAUSE_READY	1		/* paused thread ready to spl */
#define	PAUSE_WAIT	2		/* paused thread is spl-ed high */
#define	PAUSE_DIE	3		/* tell pause thread to leave */
#define	PAUSE_DEAD	4		/* pause thread has left */

/*
 * Variables used in pause_cpus().
 */
static volatile char safe_list[NCPU];

static struct _cpu_pause_info {
	int		cp_spl;		/* spl saved in pause_cpus() */
	volatile int	cp_go;		/* Go signal sent after all ready */
	int		cp_count;	/* # of CPUs to pause */
	ksema_t		cp_sem;		/* synch pause_cpus & cpu_pause */
#ifdef DEBUG
	kthread_id_t	cp_paused;
#endif /* DEBUG */
} cpu_pause_info;

static kmutex_t pause_free_mutex;
static kcondvar_t pause_free_cv;

/*
 * Force the specified thread to migrate to the appropriate processor.
 * Called with thread lock held, returns with it dropped.
 */
static void
force_thread_migrate(kthread_id_t tp)
{
	ASSERT(THREAD_LOCK_HELD(tp));
	if (tp == curthread) {
		THREAD_TRANSITION(tp);
		CL_SETRUN(tp);
		thread_unlock_nopreempt(tp);
		swtch();
	} else {
		if (tp->t_state == TS_ONPROC) {
			cpu_surrender(tp);
		} else if (tp->t_state == TS_RUN) {
			(void) dispdeq(tp);
			setbackdq(tp);
		}
		thread_unlock(tp);
	}
}

/*
 * Set affinity for a specified CPU.
 * A reference count is incremented and the affinity is held until the
 * reference count is decremented to zero by thread_affinity_clear().
 * This is so regions of code requiring affinity can be nested.
 * Caller needs to ensure that cpu_id remains valid, which can be
 * done by holding cpu_lock across this call, unless the caller
 * specifies CPU_CURRENT in which case the cpu_lock will be acquired
 * by thread_affinity_set and CPU->cpu_id will be the target CPU.
 */
void
thread_affinity_set(kthread_id_t t, int cpu_id)
{
	cpu_t		*cp;
	int		c;

	if ((c = cpu_id) == CPU_CURRENT) {
		mutex_enter(&cpu_lock);
		cpu_id = CPU->cpu_id;
	}
	/*
	 * We should be asserting that cpu_lock is held here, but
	 * the NCA code doesn't acquire it.  The following assert
	 * should be uncommented when the NCA code is fixed.
	 *
	 * ASSERT(MUTEX_HELD(&cpu_lock));
	 */
	ASSERT((cpu_id >= 0) && (cpu_id < NCPU));
	cp = cpu[cpu_id];
	ASSERT(cp != NULL);		/* user must provide a good cpu_id */
	/*
	 * If there is already a hard affinity requested, and this affinity
	 * conflicts with that, panic.
	 */
	thread_lock(t);
	if (t->t_affinitycnt > 0 && t->t_bound_cpu != cp) {
		panic("affinity_set: setting %p but already bound to %p",
		    (void *)cp, (void *)t->t_bound_cpu);
	}
	t->t_affinitycnt++;
	t->t_bound_cpu = cp;

	/*
	 * Make sure we're running on the right CPU.
	 */
	if (cp != t->t_cpu) {
		force_thread_migrate(t);	/* drops thread lock */
	} else {
		thread_unlock(t);
	}

	if (c == CPU_CURRENT)
		mutex_exit(&cpu_lock);
}

/*
 * Wrapper for backward compatibility.
 */
void
affinity_set(int cpu_id)
{
	thread_affinity_set(curthread, cpu_id);
}

/*
 * Decrement the affinity reservation count and if it becomes zero,
 * clear the CPU affinity for the current thread, or set it to the user's
 * software binding request.
 */
void
thread_affinity_clear(kthread_id_t t)
{
	register processorid_t binding;

	thread_lock(t);
	if (--t->t_affinitycnt == 0) {
		if ((binding = t->t_bind_cpu) == PBIND_NONE) {
			t->t_bound_cpu = NULL;
			if (t->t_cpu->cpu_part != t->t_cpupart) {
				force_thread_migrate(t);
				return;
			}
		} else {
			t->t_bound_cpu = cpu[binding];
			/*
			 * Make sure the thread is running on the bound CPU.
			 */
			if (t->t_cpu != t->t_bound_cpu) {
				force_thread_migrate(t);
				return;		/* already dropped lock */
			}
		}
	}
	thread_unlock(t);
}

/*
 * Wrapper for backward compatibility.
 */
void
affinity_clear(void)
{
	thread_affinity_clear(curthread);
}

/*
 * This routine is called to place the CPUs in a safe place so that
 * one of them can be taken off line or placed on line.  What we are
 * trying to do here is prevent a thread from traversing the list
 * of active CPUs while we are changing it or from getting placed on
 * the run queue of a CPU that has just gone off line.  We do this by
 * creating a thread with the highest possible prio for each CPU and
 * having it call this routine.  The advantage of this method is that
 * we can eliminate all checks for CPU_ACTIVE in the disp routines.
 * This makes disp faster at the expense of making p_online() slower
 * which is a good trade off.
 */
static void
cpu_pause(volatile char *safe)
{
	int s;
	struct _cpu_pause_info *cpi = &cpu_pause_info;

	ASSERT((curthread->t_bound_cpu != NULL) || (*safe == PAUSE_DIE));

	while (*safe != PAUSE_DIE) {
		*safe = PAUSE_READY;
		membar_enter();		/* make sure stores are flushed */
		sema_v(&cpi->cp_sem);	/* signal requesting thread */

		/*
		 * Wait here until all pause threads are running.  That
		 * indicates that it's safe to do the spl.  Until
		 * cpu_pause_info.cp_go is set, we don't want to spl
		 * because that might block clock interrupts needed
		 * to preempt threads on other CPUs.
		 */
		while (cpi->cp_go == 0)
			;
		/*
		 * Even though we are at the highest disp prio, we need
		 * to block out all interrupts below LOCK_LEVEL so that
		 * an intr doesn't come in, wake up a thread, and call
		 * setbackdq/setfrontdq.
		 */
		s = splhigh();
		/*
		 * This cpu is now safe.
		 */
		*safe = PAUSE_WAIT;
		membar_enter();		/* make sure stores are flushed */

		/*
		 * Now we wait.  When we are allowed to continue, safe will
		 * be set to PAUSE_IDLE.
		 */
		while (*safe != PAUSE_IDLE)
			;

		splx(s);
		/*
		 * Waiting is at an end. Switch out of cpu_pause
		 * loop and resume useful work.
		 */
		swtch();
	}

	mutex_enter(&pause_free_mutex);
	*safe = PAUSE_DEAD;
	cv_broadcast(&pause_free_cv);
	mutex_exit(&pause_free_mutex);
}

/*
 * Allow the cpus to start running again.
 */
void
start_cpus()
{
	int i;

	ASSERT(MUTEX_HELD(&cpu_lock));
#ifdef DEBUG
	ASSERT(cpu_pause_info.cp_paused);
	cpu_pause_info.cp_paused = NULL;
#endif /* DEBUG */
	for (i = 0; i < NCPU; i++)
		safe_list[i] = PAUSE_IDLE;
	membar_enter();			/* make sure stores are flushed */
	affinity_clear();
	splx(cpu_pause_info.cp_spl);
	kpreempt_enable();
}

/*
 * Allocate a pause thread for a CPU.
 */
static void
cpu_pause_alloc(cpu_t *cp)
{
	kthread_id_t	t;
	int		cpun = cp->cpu_id;

	/*
	 * Note, v.v_nglobpris will not change value as long as I hold
	 * cpu_lock.
	 */
	t = thread_create(NULL, PAGESIZE, cpu_pause, (caddr_t)&safe_list[cpun],
				0, &p0, TS_STOPPED, v.v_nglobpris - 1);
	if (t == NULL)
		cmn_err(CE_PANIC, "Cannot allocate CPU pause thread");
	thread_lock(t);
	t->t_bound_cpu = cp;
	t->t_disp_queue = &cp->cpu_disp;
	t->t_affinitycnt = 1;
	t->t_preempt = 1;
	thread_unlock(t);
	cp->cpu_pause_thread = t;
	/*
	 * Registering a thread in the callback table is usually done
	 * in the initialization code of the thread.  In this
	 * case, we do it right after thread creation because the
	 * thread itself may never run, and we need to register the
	 * fact that it is safe for cpr suspend.
	 */
	CALLB_CPR_INIT_SAFE(t, "cpu_pause");
}

/*
 * Free a pause thread for a CPU.
 */
static void
cpu_pause_free(cpu_t *cp)
{
	kthread_id_t	t;
	int		cpun = cp->cpu_id;

	ASSERT(MUTEX_HELD(&cpu_lock));
	/*
	 * We have to get the thread and tell him to die.
	 */
	if ((t = cp->cpu_pause_thread) == NULL) {
		ASSERT(safe_list[cpun] == PAUSE_IDLE);
		return;
	}
	thread_lock(t);
	t->t_cpu = CPU;		/* disp gets upset if last cpu is quiesced. */
	t->t_bound_cpu = NULL;	/* Must un-bind; cpu may not be running. */
	t->t_pri = v.v_nglobpris - 1;
	ASSERT(safe_list[cpun] == PAUSE_IDLE);
	safe_list[cpun] = PAUSE_DIE;
	THREAD_TRANSITION(t);
	setbackdq(t);
	thread_unlock_nopreempt(t);

	/*
	 * If we don't wait for the thread to actually die, it may try to
	 * run on the wrong cpu as part of an actual call to pause_cpus().
	 */
	mutex_enter(&pause_free_mutex);
	while (safe_list[cpun] != PAUSE_DEAD) {
		cv_wait(&pause_free_cv, &pause_free_mutex);
	}
	mutex_exit(&pause_free_mutex);
	safe_list[cpun] = PAUSE_IDLE;

	cp->cpu_pause_thread = NULL;
}

/*
 * Initialize basic structures for pausing CPUs.
 */
void
cpu_pause_init()
{
	sema_init(&cpu_pause_info.cp_sem, 0, NULL, SEMA_DEFAULT, NULL);
	/*
	 * Create initial CPU pause thread.
	 */
	cpu_pause_alloc(CPU);
}

/*
 * Start the threads used to pause another CPU.
 */
static int
cpu_pause_start(processorid_t cpu_id)
{
	int	i;
	int	cpu_count = 0;

	for (i = 0; i < NCPU; i++) {
		cpu_t		*cp;
		kthread_id_t	t;

		cp = cpu[i];
		if (!CPU_IN_SET(cpu_available, i) || (i == cpu_id)) {
			safe_list[i] = PAUSE_WAIT;
			continue;
		}

		/*
		 * Skip CPU if it is quiesced or not yet started.
		 */
		if ((cp->cpu_flags & (CPU_QUIESCED | CPU_READY)) != CPU_READY) {
			safe_list[i] = PAUSE_WAIT;
			continue;
		}

		/*
		 * Start this CPU's pause thread.
		 */
		t = cp->cpu_pause_thread;
		thread_lock(t);
		/*
		 * Reset the priority, since nglobpris may have
		 * changed since the thread was created, if someone
		 * has loaded the RT (or some other) scheduling
		 * class.
		 */
		t->t_pri = v.v_nglobpris - 1;
		THREAD_TRANSITION(t);
		setbackdq(t);
		thread_unlock_nopreempt(t);
		++cpu_count;
	}
	return (cpu_count);
}


/*
 * Pause all of the CPUs except the one we are on by creating a high
 * priority thread bound to those CPUs.
 */
void
pause_cpus(cpu_t *off_cp)
{
	processorid_t	cpu_id;
	int		i;
	struct _cpu_pause_info	*cpi = &cpu_pause_info;

	ASSERT(MUTEX_HELD(&cpu_lock));
#ifdef DEBUG
	ASSERT(cpi->cp_paused == NULL);
	cpi->cp_paused = curthread;
#endif /* DEBUG */
	cpi->cp_count = 0;
	cpi->cp_go = 0;
	for (i = 0; i < NCPU; i++)
		safe_list[i] = PAUSE_IDLE;
	kpreempt_disable();

	/*
	 * If running on the cpu that is going offline, get off it.
	 * This is so that it won't be necessary to rechoose a CPU
	 * when done.
	 */
	if (CPU == off_cp)
		cpu_id = off_cp->cpu_next_part->cpu_id;
	else
		cpu_id = CPU->cpu_id;
	affinity_set(cpu_id);

	/*
	 * Start the pause threads and record how many were started
	 */
	cpi->cp_count = cpu_pause_start(cpu_id);

	/*
	 * Now wait for all CPUs to be running the pause thread.
	 */
	THREAD_KPRI_REQUEST();
	while (cpi->cp_count > 0) {
		sema_p(&cpi->cp_sem);
		--cpi->cp_count;
	}
	THREAD_KPRI_RELEASE();
	cpi->cp_go = 1;			/* all have reached cpu_pause */

	/*
	 * Now wait for all CPUs to spl. (Transition from PAUSE_READY
	 * to PAUSE_WAIT.)
	 */
	for (i = 0; i < NCPU; i++) {
		while (safe_list[i] != PAUSE_WAIT)
			;
	}
	cpi->cp_spl = splhigh();	/* block dispatcher on this CPU */
}


/*
 * Check whether cpun is a valid processor id. If it is,
 * return a pointer to the associated CPU structure.
 */
cpu_t *
cpu_get(processorid_t cpun)
{
	ASSERT(MUTEX_HELD(&cpu_lock));

	if (cpun >= NCPU || cpun < 0 || !CPU_IN_SET(cpu_available, cpun))
		return (NULL);
	return (cpu[cpun]);
}

/*
 * Check offline/online status for the indicated
 * CPU.
 */
int
cpu_status(cpu_t *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	if (cp->cpu_flags & CPU_POWEROFF) {
		return (P_POWEROFF);
	} else if ((cp->cpu_flags & (CPU_READY | CPU_OFFLINE)) != CPU_READY) {
		return (P_OFFLINE);
	} else if (cp->cpu_flags & CPU_ENABLE) {
		return (P_ONLINE);
	} else {
		return (P_NOINTR);
	}
}

/*
 * Bring the indicated CPU online.
 */
/*ARGSUSED*/
int
cpu_online(cpu_t *cp)
{
	int	error = 0;

	/*
	 * Handle on-line request.
	 *	This code must put the new CPU on the active list before
	 *	starting it because it will not be paused, and will start
	 * 	using the active list immediately.  The real start occurs
	 *	when the CPU_QUIESCED flag is turned off.
	 */

	ASSERT(MUTEX_HELD(&cpu_lock));

	/*
	 * Put all the cpus into a known safe place.
	 * No mutexes can be entered while CPUs are paused.
	 */
	pause_cpus(NULL);
	error = mp_cpu_start(cp);	/* arch-dep hook */
	if (error == 0) {
		cpu_add_active_internal(cp);
		cp->cpu_flags &= ~(CPU_QUIESCED | CPU_OFFLINE);
		start_cpus();
		cpu_intr_enable(cp);	/* arch-dep hook */
		cyclic_online(cp);
	} else {
		start_cpus();
	}
	cpu_stat_kstat_create(cp);
	return (error);
}

/*
 * Take the indicated CPU offline.
 */
int
cpu_offline(cpu_t *cp)
{
	cpupart_t *pp;
	int	error = 0;
	cpu_t	*ncp;
	int	intr_enable;
	int	cyclic_off = 0;
	int	loop_count;
	int	no_quiesce = 0;
	int	(*bound_func)(struct cpu *);
	kthread_t *t;

	/*
	 * Handle off-line request.
	 */

	ASSERT(MUTEX_HELD(&cpu_lock));

	pp = cp->cpu_part;
	/* don't turn off last online CPU in partition */
	if (ncpus_online <= 1 || curthread->t_bound_cpu == cp ||
	    pp->cp_ncpus <= 1 || cpu_intr_count(cp) < 2) {
		return (EBUSY);
	}

	/*
	 * Take the CPU out of interrupt participation so we won't find
	 * bound kernel threads.  If the architecture cannot completely
	 * shut off interrupts on the CPU, don't quiesce it, but don't
	 * run anything but interrupt thread .. this is indicated by
	 * the CPU_OFFLINE flag being on but the CPU_QUIESCE flag being
	 * off.
	 */
	intr_enable = cp->cpu_flags & CPU_ENABLE;
	if (intr_enable)
		no_quiesce = cpu_intr_disable(cp);

	/*
	 * Check for kernel threads bound to that CPU.
	 * Inactive interrupt threads are OK (they'll be in TS_FREE
	 * state).  If test finds some bound threads, wait a few ticks
	 * to give short-lived threads (such as interrupts) chance to
	 * complete.  Note that if no_quiesce is set, i.e. this cpu
	 * is required to service interrupts, then we take the route
	 * that permits interrupt threads to be active (or bypassed).
	 */
	bound_func = no_quiesce ? disp_bound_threads : disp_bound_anythreads;

	for (loop_count = 0; (*bound_func)(cp); loop_count++) {
		if (loop_count >= 5) {
			error = EBUSY;	/* some threads still bound */
			break;
		}

		/*
		 * If some threads were assigned, give them
		 * a chance to complete or move.
		 *
		 * This assumes that the clock_thread is not bound
		 * to any CPU, because the clock_thread is needed to
		 * do the delay(hz/100).
		 *
		 * Note: we still hold the cpu_lock while waiting for
		 * the next clock tick.  This is OK since it isn't
		 * needed for anything else except processor_bind(2),
		 * and system initialization.  If we drop the lock,
		 * we would risk another p_online disabling the last
		 * processor.
		 */
		delay(hz/100);
	}

	if (error == 0) {
		if (!cyclic_offline(cp)) {
			/*
			 * We must have bound cyclics...
			 */
			error = EBUSY;
			goto out;
		}
		cyclic_off = 1;
	}

	/*
	 * Call mp_cpu_stop() to perform any special operations
	 * needed for this machine architecture to offline a CPU.
	 */
	if (error == 0)
		error = mp_cpu_stop(cp);	/* arch-dep hook */

	/*
	 * If that all worked, take the CPU offline and decrement
	 * ncpus_online.
	 */
	if (error == 0) {
		/*
		 * Put all the cpus into a known safe place.
		 * No mutexes can be entered while CPUs are paused.
		 */
		pause_cpus(cp);
		ncp = cp->cpu_next_part;
		/*
		 * Remove the CPU from the list of active CPUs.
		 */
		cpu_remove_active(cp);
		/*
		 * Update any thread that has loose affinity
		 * for the cpu so that it won't end up being placed
		 * on the offline cpu's runqueue.
		 */
		for (t = curthread->t_next; t != curthread; t = t->t_next) {
			ASSERT(t != NULL);
			if (t->t_cpu == cp && t->t_bound_cpu != cp)
				t->t_cpu = disp_lowpri_cpu(ncp, 0);
			ASSERT(t->t_cpu != cp || t->t_bound_cpu == cp);
		}
		ASSERT(curthread->t_cpu != cp);
		cp->cpu_flags |= CPU_OFFLINE;
		disp_cpu_inactive(cp);
		if (!no_quiesce)
			cp->cpu_flags |= CPU_QUIESCED;
		ncpus_online--;
		cpu_setstate(cp, P_OFFLINE);
		start_cpus();
		/*
		 * Remove this CPU's kstat.
		 */
		if (cp->cpu_kstat != NULL) {
			kstat_delete(cp->cpu_kstat);
			cp->cpu_kstat = NULL;
		}
	}

out:
	/*
	 * If we failed, but managed to offline the cyclic subsystem on this
	 * CPU, bring it back online.
	 */
	if (error && cyclic_off)
		cyclic_online(cp);

	/*
	 * If we failed, re-enable interrupts.
	 * Do this even if cpu_intr_disable returned an error, because
	 * it may have partially disabled interrupts.
	 */
	if (error && intr_enable)
		cpu_intr_enable(cp);
	return (error);
}

/*
 * Take the indicated CPU from poweroff to offline.
 */
/* ARGSUSED */
int
cpu_poweron(cpu_t *cp)
{
	int	error = ENOTSUP;

	ASSERT(MUTEX_HELD(&cpu_lock));

	ASSERT(cpu_status(cp) == P_POWEROFF);

	error = mp_cpu_poweron(cp);	/* arch-dep hook */
	if (error == 0) {
		cpu_setstate(cp, P_OFFLINE);
	}

	return (error);
}

/*
 * Take the indicated CPU from offline to poweroff.
 */
/* ARGSUSED */
int
cpu_poweroff(cpu_t *cp)
{
	int	error = ENOTSUP;

	ASSERT(MUTEX_HELD(&cpu_lock));

	ASSERT(cpu_status(cp) == P_OFFLINE);

	if (!(cp->cpu_flags & CPU_QUIESCED))
		return (EBUSY);		/* not completely idle */

	error = mp_cpu_poweroff(cp);	/* arch-dep hook */
	if (error == 0) {
		cpu_setstate(cp, P_POWEROFF);
	}

	return (error);
}

/*
 * Initialize the CPU lists for the first CPU.
 */
void
cpu_list_init(cpu_t *cp)
{
	cp->cpu_next = cp;
	cp->cpu_prev = cp;
	cpu_list = cp;

	cp->cpu_next_onln = cp;
	cp->cpu_prev_onln = cp;
	cpu_active = cp;

	cp->cpu_seqid = 0;
	CPUSET_ADD(cpu_seqid_inuse, 0);
	cp->cpu_cache_offset = KMEM_CACHE_SIZE(cp->cpu_seqid);
	cp_default.cp_cpulist = cp;
	cp_default.cp_ncpus = 1;
	cp->cpu_next_part = cp;
	cp->cpu_prev_part = cp;
	cp->cpu_part = &cp_default;

	CPUSET_ADD(cpu_available, cp->cpu_id);
}

/*
 * Insert a CPU into the list of available CPUs.
 */
void
cpu_add_unit(cpu_t *cp)
{
	int seqid;

	extern void disp_cpu_init(cpu_t *);	/* XXX  - disp.h */

	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(cpu_list != NULL);	/* list started in cpu_list_init */

	disp_cpu_init(cp);

	/*
	 * Note: most users of the cpu_list will grab the
	 * cpu_lock to insure that it isn't modified.  However,
	 * Certain users can't or won't do that.  To allow this
	 * we pause the other cpus.  Users who walk the list
	 * without cpu_lock, must disable kernel preemption
	 * to insure that the list isn't modified underneath
	 * them.  Also, any cached pointers to cpu structures
	 * must be revalidated by checking to see if the
	 * cpu_next pointer points to itself.  This check must
	 * be done with the cpu_lock held or kernel preemption
	 * disabled.  This check relies upon the fact that
	 * old cpu structures are not free'ed or cleared after
	 * then are removed from the cpu_list.
	 */
	(void) pause_cpus(NULL);
	cp->cpu_next = cpu_list;
	cp->cpu_prev = cpu_list->cpu_prev;
	cpu_list->cpu_prev->cpu_next = cp;
	cpu_list->cpu_prev = cp;
	start_cpus();

	for (seqid = 0; CPU_IN_SET(cpu_seqid_inuse, seqid); seqid++)
		continue;
	CPUSET_ADD(cpu_seqid_inuse, seqid);
	cp->cpu_seqid = seqid;
	ncpus++;
	cp->cpu_cache_offset = KMEM_CACHE_SIZE(cp->cpu_seqid);
	cpu[cp->cpu_id] = cp;
	CPUSET_ADD(cpu_available, cp->cpu_id);

	/*
	 * allocate a pause thread for this CPU.
	 */
	cpu_pause_alloc(cp);

	/*
	 * So that new CPUs won't have NULL prev_onln and next_onln pointers,
	 * link them into a list of just that CPU.
	 * This is so that disp_lowpri_cpu will work for thread_create in
	 * pause_cpus() when called from the startup thread in a new CPU.
	 */
	cp->cpu_next_onln = cp;
	cp->cpu_prev_onln = cp;
	cpu_info_kstat_create(cp);
	cp->cpu_next_part = cp;
	cp->cpu_prev_part = cp;
	cp->cpu_part = &cp_default;
}

/*
 * Do the opposite of cpu_add_unit().
 */
void
cpu_del_unit(int cpuid)
{
	struct cpu	*cp, *cpnext;

	ASSERT(MUTEX_HELD(&cpu_lock));
	cp = cpu[cpuid];
	ASSERT(cp != NULL);

	ASSERT(cp->cpu_next_onln == cp);
	ASSERT(cp->cpu_prev_onln == cp);
	ASSERT(cp->cpu_next_part == cp);
	ASSERT(cp->cpu_prev_part == cp);

	/*
	 * Destroy kstat stuff.
	 */
	cpu_info_kstat_destroy(cp);
	/*
	 * Free up pause thread.
	 */
	cpu_pause_free(cp);

	CPUSET_DEL(cpu_available, cp->cpu_id);
	cpu[cp->cpu_id] = NULL;
	/*
	 * The clock thread and mutex_vector_enter cannot hold the
	 * cpu_lock while traversing the cpu list, therefore we pause
	 * all other threads by pausing the other cpus. These, and any
	 * other routines holding cpu pointers while possibly sleeping
	 * must be sure to call kpreempt_disable before processing the
	 * list and be sure to check that the cpu has not been deleted
	 * after any sleeps (check cp->cpu_next != NULL). We guarantee
	 * to keep the deleted cpu structure around.
	 *
	 * Note that this MUST be done AFTER cpu_available
	 * has been updated so that we don't waste time
	 * trying to pause the cpu we're trying to delete.
	 */
	(void) pause_cpus(NULL);

	cpnext = cp->cpu_next;
	cp->cpu_prev->cpu_next = cp->cpu_next;
	cp->cpu_next->cpu_prev = cp->cpu_prev;
	if (cp == cpu_list)
	    cpu_list = cpnext;

	/*
	 * Signals that the cpu has been deleted (see above).
	 */
	cp->cpu_next = NULL;
	cp->cpu_prev = NULL;

	start_cpus();

	CPUSET_DEL(cpu_seqid_inuse, cp->cpu_seqid);
	ncpus--;
}

/*
 * Add a CPU to the list of active CPUs.
 *	This routine must not get any locks, because other CPUs are paused.
 */
static void
cpu_add_active_internal(cpu_t *cp)
{
	cpupart_t	*pp = cp->cpu_part;

	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(cpu_list != NULL);	/* list started in cpu_list_init */

	ncpus_online++;
	cpu_setstate(cp, P_ONLINE);
	cp->cpu_next_onln = cpu_active;
	cp->cpu_prev_onln = cpu_active->cpu_prev_onln;
	cpu_active->cpu_prev_onln->cpu_next_onln = cp;
	cpu_active->cpu_prev_onln = cp;

	if (pp->cp_cpulist) {
		cp->cpu_next_part = pp->cp_cpulist;
		cp->cpu_prev_part = pp->cp_cpulist->cpu_prev_part;
		pp->cp_cpulist->cpu_prev_part->cpu_next_part = cp;
		pp->cp_cpulist->cpu_prev_part = cp;
	} else {
		ASSERT(pp->cp_ncpus == 0);
		pp->cp_cpulist = cp->cpu_next_part = cp->cpu_prev_part = cp;
	}
	pp->cp_ncpus++;
}

/*
 * Add a CPU to the list of active CPUs.
 *	This is called from machine-dependent layers when a new CPU is started.
 */
void
cpu_add_active(cpu_t *cp)
{
	pause_cpus(NULL);
	cpu_add_active_internal(cp);
	start_cpus();
	cpu_stat_kstat_create(cp);
}


/*
 * Remove a CPU from the list of active CPUs.
 *	This routine must not get any locks, because other CPUs are paused.
 */
/* ARGSUSED */
static void
cpu_remove_active(cpu_t *cp)
{
	cpupart_t	*pp = cp->cpu_part;

	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(cp->cpu_next_onln != cp);	/* not the last one */
	ASSERT(cp->cpu_prev_onln != cp);	/* not the last one */

	cp->cpu_prev_onln->cpu_next_onln = cp->cpu_next_onln;
	cp->cpu_next_onln->cpu_prev_onln = cp->cpu_prev_onln;
	if (cpu_active == cp) {
		cpu_active = cp->cpu_next_onln;
	}
	cp->cpu_next_onln = cp;
	cp->cpu_prev_onln = cp;

	cp->cpu_prev_part->cpu_next_part = cp->cpu_next_part;
	cp->cpu_next_part->cpu_prev_part = cp->cpu_prev_part;
	if (pp->cp_cpulist == cp) {
		pp->cp_cpulist = cp->cpu_next_part;
		ASSERT(pp->cp_cpulist != cp);
	}
	cp->cpu_next_part = cp;
	cp->cpu_prev_part = cp;
	pp->cp_ncpus--;
}

/*
 * Ideally, these would be dynamically allocated and put into a linked
 * list; however that is not feasible because the registration routine
 * has to be available before the kmem allocator is working (in fact,
 * it is called by the kmem allocator init code).  In any case, there
 * are quite a few extra entries for future users.
 */
#define	NCPU_SETUPS	10

struct cpu_setup {
	cpu_setup_func_t *func;
	void *arg;
} cpu_setups[NCPU_SETUPS];

/*
 * Routine used to setup a newly inserted CPU in preparation for starting
 * it running code.
 */
int
cpu_configure(int cpuid)
{
	int i, retval = 0;
	int mp_dr_strinit(void);

	ASSERT(MUTEX_HELD(&cpu_lock));

#ifdef TRACE
	if (tracing_state != VTR_STATE_NULL) {
		cmn_err(CE_WARN, "cpu_configure:  operation not allowed "
		    "while tracing is enabled");
		return (EBUSY);
	}
#endif

	/*
	 * Some structures are statically allocated based upon
	 * the maximum number of cpus the system supports.  Do not
	 * try to add anything beyond this limit.
	 */
	if (cpuid < 0 || cpuid >= NCPU) {
		return (EINVAL);
	}

	if ((cpu[cpuid] != NULL) && (cpu[cpuid]->cpu_flags != 0)) {
		return (EALREADY);
	}

	if ((retval = mp_dr_strinit()) != 0) {
		return (retval);
	}

	if ((retval = mp_cpu_configure(cpuid)) != 0) {
		return (retval);
	}

	cpu[cpuid]->cpu_flags = CPU_QUIESCED | CPU_OFFLINE | CPU_POWEROFF;
	cpu[cpuid]->cpu_type_info.pi_state = P_POWEROFF;
	cpu[cpuid]->cpu_state_begin = hrestime.tv_sec;

	for (i = 0; i < NCPU_SETUPS; i++) {
		if (cpu_setups[i].func != NULL) {
			retval = cpu_setups[i].func(CPU_CONFIG, cpuid,
			    cpu_setups[i].arg);
			if (retval) {
				for (i--; i >= 0; i--) {
					if (cpu_setups[i].func != NULL)
						cpu_setups[i].func(CPU_UNCONFIG,
						    cpuid, cpu_setups[i].arg);
				}
				(void) mp_cpu_unconfigure(cpuid);
				break;
			}
		}
	}

	return (retval);
}

/*
 * Routine used to cleanup a CPU that has been powered off.  This will
 * destroy all per-cpu information related to this cpu.
 */
int
cpu_unconfigure(int cpuid)
{
	int i, retval = 0, tmp;

	ASSERT(MUTEX_HELD(&cpu_lock));

#ifdef TRACE
	if (tracing_state != VTR_STATE_NULL) {
		cmn_err(CE_WARN, "cpu_unconfigure:  operation not allowed "
		    "while tracing is enabled");
		return (EBUSY);
	}
#endif

	if (cpu[cpuid] == NULL) {
		return (ENODEV);
	}

	if (cpu[cpuid]->cpu_flags == 0) {
		return (EALREADY);
	}

	if ((cpu[cpuid]->cpu_flags & CPU_POWEROFF) == 0) {
		return (EBUSY);
	}

	for (i = 0; i < NCPU_SETUPS; i++) {
		if (cpu_setups[i].func != NULL) {
			tmp = cpu_setups[i].func(CPU_UNCONFIG, cpuid,
			    cpu_setups[i].arg);
			if (tmp && (retval == 0))
				retval = tmp;
		}
	}

	tmp = mp_cpu_unconfigure(cpuid);

	if (tmp && (retval == 0))
		retval = tmp;

	return (retval);
}

/*
 * Routines for registering and de-registering cpu_setup callback functions.
 */

void
register_cpu_setup_func(cpu_setup_func_t *func, void *arg)
{
	int i;

	ASSERT(MUTEX_HELD(&cpu_lock));

	for (i = 0; i < NCPU_SETUPS; i++)
		if (cpu_setups[i].func == NULL)
		    break;
	if (i >= NCPU_SETUPS)
		cmn_err(CE_PANIC, "Ran out of cpu_setup callback entries");

	cpu_setups[i].func = func;
	cpu_setups[i].arg = arg;
}

void
unregister_cpu_setup_func(cpu_setup_func_t *func, void *arg)
{
	int i;

	ASSERT(MUTEX_HELD(&cpu_lock));

	for (i = 0; i < NCPU_SETUPS; i++)
		if ((cpu_setups[i].func == func) &&
		    (cpu_setups[i].arg == arg))
		    break;
	if (i >= NCPU_SETUPS)
		cmn_err(CE_PANIC, "Could not find cpu_setup callback to "
		    "deregister");

	cpu_setups[i].func = NULL;
	cpu_setups[i].arg = 0;
}

/*
 * Export this CPU's statistics via the kstat mechanism.
 * This is done when a CPU is initialized or placed online via p_online(2).
 */
static void
cpu_stat_kstat_create(cpu_t *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	cp->cpu_kstat = kstat_create("cpu_stat", cp->cpu_id, NULL,
	    "misc", KSTAT_TYPE_RAW, sizeof (cpu_stat_t), KSTAT_FLAG_VIRTUAL);
	if (cp->cpu_kstat != NULL) {
		cp->cpu_kstat->ks_data = (void *)&cp->cpu_stat;
		kstat_install(cp->cpu_kstat);
	}
}

/*
 * Export information about this CPU via the kstat mechanism.
 */
static struct {
	kstat_named_t ci_state;
	kstat_named_t ci_state_begin;
	kstat_named_t ci_cpu_type;
	kstat_named_t ci_fpu_type;
	kstat_named_t ci_clock_MHz;
} cpu_info_template = {
	{ "state",		KSTAT_DATA_CHAR },
	{ "state_begin",	KSTAT_DATA_LONG },
	{ "cpu_type",		KSTAT_DATA_CHAR },
	{ "fpu_type",		KSTAT_DATA_CHAR },
	{ "clock_MHz",		KSTAT_DATA_LONG },
};

static int
cpu_info_kstat_update(kstat_t *ksp, int rw)
{
	cpu_t	*cp = ksp->ks_private;
	char	*pi_state;

	if (rw == KSTAT_WRITE)
		return (EACCES);
	switch (cp->cpu_type_info.pi_state) {
	case P_ONLINE:
		pi_state = "on-line";
		break;
	case P_POWEROFF:
		pi_state = "powered-off";
		break;
	case P_NOINTR:
		pi_state = "no-intr";
		break;
	case P_OFFLINE:
	default:
		pi_state = "off-line";
		break;
	}
	(void) strcpy(cpu_info_template.ci_state.value.c, pi_state);
	cpu_info_template.ci_state_begin.value.l = cp->cpu_state_begin;
	(void) strncpy(cpu_info_template.ci_cpu_type.value.c,
	    cp->cpu_type_info.pi_processor_type, 15);
	(void) strncpy(cpu_info_template.ci_fpu_type.value.c,
	    cp->cpu_type_info.pi_fputypes, 15);
	cpu_info_template.ci_clock_MHz.value.l = cp->cpu_type_info.pi_clock;
	return (0);
}

static void
cpu_info_kstat_create(cpu_t *cp)
{
	kstat_t *ksp;

	ASSERT(MUTEX_HELD(&cpu_lock));
	ksp = kstat_create("cpu_info", cp->cpu_id, NULL,
		"misc", KSTAT_TYPE_NAMED,
		sizeof (cpu_info_template) / sizeof (kstat_named_t),
		KSTAT_FLAG_VIRTUAL);
	if (ksp != NULL) {
		ksp->ks_data = &cpu_info_template;
		ksp->ks_private = cp;
		ksp->ks_update = cpu_info_kstat_update;
		kstat_install(ksp);
	}
}

static void
cpu_info_kstat_destroy(cpu_t *cp)
{
	kstat_t *ksp;
	char buf[KSTAT_STRLEN+16];

	ASSERT(MUTEX_HELD(&cpu_lock));

	(void) sprintf(buf, "%s%d", "cpu_info", cp->cpu_id);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("cpu_info", cp->cpu_id, buf);
	mutex_exit(&kstat_chain_lock);
	if (ksp) {
		kstat_delete(ksp);
	} else {
#ifdef DEBUG
		printf("cpu_info_kstat_destroy(cpuid = %d): No kstat info "
		    "found\n", cp->cpu_id);
#endif
	}
}

void
cpu_kstat_init(cpu_t *cp)
{
	mutex_enter(&cpu_lock);
	cpu_stat_kstat_create(cp);
	cpu_info_kstat_create(cp);
	cpu_setstate(cp, P_ONLINE);
	mutex_exit(&cpu_lock);
}

/*
 * Bind a thread to a CPU as requested.
 */
int
cpu_bind_thread(kthread_id_t tp, struct bind_arg *arg)
{
	processorid_t	binding;
	cpu_t		*cp;

	ASSERT(MUTEX_HELD(&ttoproc(tp)->p_lock));

	thread_lock(tp);

	/*
	 * Record old binding, but change the arg, which was initialized
	 * to PBIND_NONE, only if this thread has a binding.  This avoids
	 * reporting PBIND_NONE for a process when some LWPs are bound.
	 */
	binding = tp->t_bind_cpu;
	if (binding != PBIND_NONE)
		arg->obind = binding;	/* record old binding */

	if (arg->bind != PBIND_QUERY) {
		/*
		 * If this thread/LWP cannot be bound because of permission
		 * problems, just note that and return success so that the
		 * other threads/LWPs will be bound.  This is the way
		 * processor_bind() is defined to work.
		 *
		 * Binding will get EPERM if the thread is of system class
		 * or hasprocperm() fails.
		 */
		if (tp->t_cid == 0 || !hasprocperm(tp->t_cred, CRED())) {
			arg->err = EPERM;
			thread_unlock(tp);
			return (0);
		}
		binding = arg->bind;
		if (binding != PBIND_NONE) {
			cp = cpu[binding];
			/*
			 * Make sure binding is in right partition.  If it's
			 * in the wrong system partition, and hasn't been
			 * explicitly bound to the partition, move it to the
			 * new system partition.  Otherwise, return an error.
			 */
			if (tp->t_cpupart != cp->cpu_part) {
				if (CP_PUBLIC(tp->t_cpupart) &&
				    CP_PUBLIC(cp->cpu_part) &&
				    tp->t_bind_pset == PS_NONE) {
					tp->t_cpupart = cp->cpu_part;
				} else {
					arg->err = EINVAL;
					thread_unlock(tp);
					return (0);
				}
			}
		}
		tp->t_bind_cpu = binding;	/* set new binding */

		/*
		 * If there is no system-set reason for affinity, set
		 * the t_bound_cpu field to reflect the binding.
		 */
		if (tp->t_affinitycnt == 0) {
			if (binding == PBIND_NONE) {
				/* set new binding */
				tp->t_bound_cpu = NULL;

				if (tp->t_state == TS_ONPROC &&
				    tp->t_cpu->cpu_part != tp->t_cpupart) {
					cpu_surrender(tp);
				} else if (tp->t_state == TS_RUN) {
					/*
					 * need to requeue even if we're
					 * not changing CPU partitions
					 * since we may need to change
					 * disp_max_unbound_pri.
					 */
					(void) dispdeq(tp);
					setbackdq(tp);
				}
			} else {
				tp->t_bound_cpu = cp;

				/*
				 * Make the thread switch to the bound CPU.
				 */
				if (cp != tp->t_cpu) {
					if (tp->t_state == TS_ONPROC) {
						cpu_surrender(tp);
					} else if (tp->t_state == TS_RUN) {
						(void) dispdeq(tp);
						setbackdq(tp);
					}
				}
			}
		}
	}

	/*
	 * Our binding has changed; set TP_CHANGEBIND.
	 */
	tp->t_proc_flag |= TP_CHANGEBIND;
	aston(tp);

	thread_unlock(tp);

	return (0);
}

/*
 * Bind all the threads of a process to a CPU.
 */
int
cpu_bind_process(proc_t *pp, struct bind_arg *arg)
{
	kthread_t	*tp;
	kthread_t	*fp;
	int		err = 0;
	int		i;

	ASSERT(MUTEX_HELD(&pidlock));
	mutex_enter(&pp->p_lock);

	tp = pp->p_tlist;
	if (tp != NULL) {
		fp = tp;
		do {
			i = cpu_bind_thread(tp, arg);
			if (err == 0)
				err = i;
		} while ((tp = tp->t_forw) != fp);
	}

	mutex_exit(&pp->p_lock);
	return (err);
}

#if CPUSET_WORDS > 1

/*
 * Functions for implementing cpuset operations when a cpuset is more
 * than one word.  On platforms where a cpuset is a single word these
 * are implemented as macros in cpuvar.h.
 */

void
cpuset_all(cpuset_t *s)
{
	int i;

	for (i = 0; i < CPUSET_WORDS; i++)
		s->cpub[i] = ~0UL;
}

void
cpuset_all_but(cpuset_t *s, uint_t cpu)
{
	cpuset_all(s);
	CPUSET_DEL(*s, cpu);
}

void
cpuset_only(cpuset_t *s, uint_t cpu)
{
	CPUSET_ZERO(*s);
	CPUSET_ADD(*s, cpu);
}

int
cpuset_isnull(cpuset_t *s)
{
	int i;

	for (i = 0; i < CPUSET_WORDS; i++)
		if (s->cpub[i] != 0)
			return (0);
	return (1);
}

int
cpuset_cmp(cpuset_t *s1, cpuset_t *s2)
{
	int i;

	for (i = 0; i < CPUSET_WORDS; i++)
		if (s1->cpub[i] != s2->cpub[i])
			return (0);
	return (1);
}

#endif	/* CPUSET_WORDS */

/*
 * Destroy all remaining bound threads on a cpu.
 */
void
cpu_destroy_bound_threads(cpu_t *cp)
{
	extern id_t syscid;
	register kthread_id_t	t, tlist, tnext;

	/*
	 * Destroy all remaining bound threads on the cpu.  This
	 * should include both the interrupt threads and the idle thread.
	 * This requires some care, since we need to traverse the
	 * thread list with the pidlock mutex locked, but thread_free
	 * also locks the pidlock mutex.  So, we collect the threads
	 * we're going to reap in a list headed by "tlist", then we
	 * unlock the pidlock mutex and traverse the tlist list,
	 * doing thread_free's on the thread's.	 Simple, n'est pas?
	 * Also, this depends on thread_free not mucking with the
	 * t_next and t_prev links of the thread.
	 */

	if ((t = curthread) != NULL) {

		tlist = NULL;
		mutex_enter(&pidlock);
		do {
			tnext = t->t_next;
			if (t->t_bound_cpu == cp) {

				/*
				 * We've found a bound thread, carefully unlink
				 * it out of the thread list, and add it to
				 * our "tlist".	 We "know" we don't have to
				 * worry about unlinking curthread (the thread
				 * that is executing this code).
				 */
				t->t_next->t_prev = t->t_prev;
				t->t_prev->t_next = t->t_next;
				t->t_next = tlist;
				tlist = t;
				ASSERT(t->t_cid == syscid);
				/*
				 * t_lwp set by interrupt threads and not
				 * cleared.
				 */
				t->t_lwp = NULL;
				/*
				 * Pause and idle threads always have
				 * t_state set to TS_ONPROC.
				 */
				t->t_state = TS_FREE;
				t->t_prev = NULL;	/* Just in case */
			}

		} while ((t = tnext) != curthread);

		mutex_exit(&pidlock);


		for (t = tlist; t != NULL; t = tnext) {
			tnext = t->t_next;
			thread_free(t);
		}
	}
}
