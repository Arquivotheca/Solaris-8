/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)disp.c	1.154	99/07/29 SMI"	/* from SVr4.0 1.30 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/sysinfo.h>
#include <sys/var.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/inline.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/bitmap.h>
#include <sys/kmem.h>
#include <sys/cpuvar.h>
#include <sys/vtrace.h>
#include <sys/tnf.h>
#include <sys/cpupart.h>
#include <sys/schedctl.h>
#include <sys/atomic.h>

#include <vm/as.h>

#define	BOUND_CPU	0x1
#define	BOUND_PARTITION	0x2
#define	BOUND_INTR	0x4

/* Dispatch queue allocation structure and functions */
struct disp_queue_info {
	disp_t	*dp;
	dispq_t *olddispq;
	dispq_t *newdispq;
	ulong_t	*olddqactmap;
	ulong_t	*newdqactmap;
	int	oldnglobpris;
};
static void	disp_dq_alloc(struct disp_queue_info *dptr, int numpris,
    disp_t *dp);
static void	disp_dq_assign(struct disp_queue_info *dptr, int numpris);
static void	disp_dq_free(struct disp_queue_info *dptr);

/* Function to run when UP machine is idle */
static void	generic_idle_cpu();
void		(*idle_cpu)() = generic_idle_cpu;

pri_t	kpreemptpri;	/* priority where kernel preemption applies */
pri_t	upreemptpri = 0; /* priority where normal preemption applies */
pri_t	intr_pri;	/* interrupt thread priority base level */

#define	KPQPRI	-1 /* priority where cpu affinity is dropped for kp queue */
pri_t	kpqpri = KPQPRI; /* can be set in /etc/system */

disp_lock_t	swapped_lock;	/* lock swapped threads and swap queue */
int	nswapped;	/* total number of swapped threads */
void	disp_swapped_enq(kthread_id_t tp);
static void	disp_swapped_setrun(kthread_id_t tp);
static void	cpu_resched(cpu_t *cp, pri_t tpri);
extern void	shuttle_resume_async(kthread_t *);

/*
 * If this is set, only interrupt threads will cause kernel preemptions.
 * This is done by changing the value of kpreemptpri.  kpreemptpri
 * will either be the max sysclass pri + 1 or the min interrupt pri.
 */
int	only_intr_kpreempt;

extern void set_idle_cpu(int cpun);
extern void unset_idle_cpu(int cpun);
static void setkpdq(kthread_id_t tp, int borf);
#define	SETKP_BACK	0
#define	SETKP_FRONT	1
/*
 * Parameter that determines how recently a thread must have run
 * on the CPU to be considered loosely-bound to that CPU to reduce
 * cold cache effects.  The interval is in hertz.
 */
#define	RECHOOSE_INTERVAL 3
int	rechoose_interval = RECHOOSE_INTERVAL;
static cpu_t	*cpu_choose(kthread_id_t, pri_t);

id_t	initcid;

disp_lock_t	transition_lock;	/* lock on transitioning threads */
disp_lock_t	stop_lock;		/* lock on stopped threads */
disp_lock_t	shuttle_lock;		/* lock on shuttle objects */

static void		cpu_dispqalloc(int numpris);

static kthread_id_t	disp_getwork(cpu_t *to);
static kthread_id_t	disp_getbest(disp_t *from);
static kthread_id_t	disp_ratify(kthread_id_t tp, disp_t *kpq);

void	swtch_to(kthread_id_t);

/*
 * Kernel preemption occurs if a higher-priority thread is runnable with
 * a priority at or above kpreemptpri.
 *
 * So that other processors can watch for such threads, a separate
 * dispatch queue with unbound work above kpreemptpri is maintained.
 * This is part of the CPU partition structure (cpupart_t).
 */
extern void	disp_kp_alloc(pri_t npri);	/* allocate kp queues */
extern void	disp_kp_free(disp_t *dq);	/* free kp queue */

/*
 * dispatcher and scheduler initialization
 */

/*
 * disp_setup - Common code to calculate and allocate dispatcher
 *		variables and structures based on the maximum priority.
 */
static void
disp_setup(pri_t maxglobpri, pri_t oldnglobpris)
{
	pri_t	newnglobpris;

	newnglobpris = maxglobpri + 1 + LOCK_LEVEL;

	if (newnglobpris > oldnglobpris) {
		/*
		 * Allocate new kp queues.
		 */
		disp_kp_alloc(newnglobpris);

		/*
		 * Allocate new dispatch queues for each CPU.
		 */
		cpu_dispqalloc(newnglobpris);

		/*
		 * compute new interrupt thread base priority
		 */
		intr_pri = maxglobpri;
		if (only_intr_kpreempt) {
			kpreemptpri = intr_pri + 1;
			if (kpqpri == KPQPRI)
				kpqpri = kpreemptpri;
		}
		v.v_nglobpris = newnglobpris;
	}
}

/*
 * disp_getsched - Called to load a scheduler and check whether the
 *		   load succeeded. If not, do the right thing.
 */
static int
disp_getsched(char *iclass, id_t *cid, int canfail)
{
	if (getcid(iclass, cid) != 0) {
		cmn_err(canfail ? CE_WARN : CE_PANIC,
			"Couldn't load scheduler class: %s", iclass);
		return (1);
	}
	return (0);
}

/*
 * dispinit - Called to initialize all loaded classes and the
 *	      dispatcher framework.
 */
void
dispinit(void)
{
	id_t	cid;
	pri_t	maxglobpri;
	pri_t	cl_maxglobpri;

	maxglobpri = -1;

	/*
	 * Initialize transition lock, which will always be set.
	 */
	DISP_LOCK_INIT(&transition_lock);
	disp_lock_enter_high(&transition_lock);
	DISP_LOCK_INIT(&stop_lock);
	DISP_LOCK_INIT(&shuttle_lock);

	mutex_enter(&cp_list_lock);
	mutex_enter(&cpu_lock);
	CPU->cpu_disp.disp_maxrunpri = -1;
	CPU->cpu_disp.disp_max_unbound_pri = -1;
	/*
	 * Initialize the default CPU partition.
	 */
	cpupart_initialize_default();
	/*
	 * Call the class specific initialization functions for
	 * all pre-installed schedulers.
	 *
	 * We pass the size of a class specific parameter
	 * buffer to each of the initialization functions
	 * to try to catch problems with backward compatibility
	 * of class modules.
	 *
	 * For example a new class module running on an old system
	 * which didn't provide sufficiently large parameter buffers
	 * would be bad news. Class initialization modules can check for
	 * this and take action if they detect a problem.
	 */

	for (cid = 0; cid < nclass; cid++) {
		sclass_t	*sc;

		sc = &sclass[cid];
		if (SCHED_INSTALLED(sc)) {
			cl_maxglobpri = sc->cl_init(cid, PC_CLPARMSZ,
			    &sc->cl_funcs);
			if (cl_maxglobpri > maxglobpri)
				maxglobpri = cl_maxglobpri;
		}
	}
	kpreemptpri = (pri_t)v.v_maxsyspri + 1;
	if (kpqpri == KPQPRI)
		kpqpri = kpreemptpri;

	ASSERT(maxglobpri >= 0);
	disp_setup(maxglobpri, 0);

	mutex_exit(&cpu_lock);
	mutex_exit(&cp_list_lock);
	/*
	 * Get the init-class CID.
	 * This will load the class (normally TS, unless overridden),
	 * and that will call disp_add(), which is why we had to drop
	 * cpu_lock and cp_list_lock first.
	 */
	if (disp_getsched(initclass, &initcid, initclass != defaultclass)) {
		initclass = defaultclass;
		cmn_err(CE_WARN, "Switching init to default class: %s",
		    initclass);
		(void) disp_getsched(initclass, &initcid, 0);
	}

	/*
	 * Check for any extra classes to preload.
	 */
	if (extraclass != NULL && strcmp(extraclass, initclass) != 0) {
		id_t	extracid;

		(void) disp_getsched(extraclass, &extracid, 1);
	}
}

/*
 * disp_add - Called with class pointer to initialize the dispatcher
 *	      for a newly loaded class.
 */
void
disp_add(sclass_t *clp)
{
	pri_t	maxglobpri;
	pri_t	cl_maxglobpri;

	mutex_enter(&cp_list_lock);	/* needed for disp_kp_alloc */
	mutex_enter(&cpu_lock);
	/*
	 * Initialize the scheduler class.
	 */
	maxglobpri = (pri_t)(v.v_nglobpris - LOCK_LEVEL - 1);
	cl_maxglobpri = clp->cl_init(clp - sclass, PC_CLPARMSZ, &clp->cl_funcs);
	if (cl_maxglobpri > maxglobpri)
		maxglobpri = cl_maxglobpri;

	/*
	 * Save old queue information.  Since we're initializing a
	 * new scheduling class which has just been loaded, then
	 * the size of the dispq may have changed.  We need to handle
	 * that here.
	 */
	disp_setup(maxglobpri, v.v_nglobpris);

	mutex_exit(&cpu_lock);
	mutex_exit(&cp_list_lock);
}


/*
 * For each CPU, allocate new dispatch queues
 * with the stated number of priorities.
 */
static void
cpu_dispqalloc(int numpris)
{
	cpu_t	*cpup;
	struct disp_queue_info	disp_mem[NCPU];
	int i, num;

	ASSERT(MUTEX_HELD(&cpu_lock));

	/*
	 * This routine must allocate all of the memory before stopping
	 * the cpus because it must not sleep in kmem_alloc while the
	 * CPUs are stopped.  Locks they hold will not be freed until they
	 * are restarted.
	 */
	i = 0;
	cpup = cpu_list;
	do {
		disp_dq_alloc(&disp_mem[i], numpris, &cpup->cpu_disp);
		i++;
		cpup = cpup->cpu_next;
	} while (cpup != cpu_list);
	num = i;

	pause_cpus(NULL);
	for (i = 0; i < num; i++)
		disp_dq_assign(&disp_mem[i], numpris);
	start_cpus();

	/*
	 * I must free all of the memory after starting the cpus because
	 * I can not risk sleeping in kmem_free while the cpus are stopped.
	 */
	for (i = 0; i < num; i++)
		disp_dq_free(&disp_mem[i]);
}

static void
disp_dq_alloc(struct disp_queue_info *dptr, int numpris, disp_t	*dp)
{
	dptr->newdispq = kmem_zalloc(numpris * sizeof (dispq_t), KM_SLEEP);
	dptr->newdqactmap = kmem_zalloc(((numpris / BT_NBIPUL) + 1) *
	    sizeof (long), KM_SLEEP);
	dptr->dp = dp;
}

static void
disp_dq_assign(struct disp_queue_info *dptr, int numpris)
{
	disp_t	*dp;

	dp = dptr->dp;
	dptr->olddispq = dp->disp_q;
	dptr->olddqactmap = dp->disp_qactmap;
	dptr->oldnglobpris = dp->disp_npri;

	ASSERT(dptr->oldnglobpris < numpris);

	if (dptr->olddispq != NULL) {
		bcopy(dptr->olddispq, dptr->newdispq,
		    dptr->oldnglobpris * sizeof (dispq_t));
		bcopy(dptr->olddqactmap, dptr->newdqactmap,
		    ((dptr->oldnglobpris / BT_NBIPUL) + 1) *
		    sizeof (long));
	}
	dp->disp_q = dptr->newdispq;
	dp->disp_qactmap = dptr->newdqactmap;
	dp->disp_q_limit = &dptr->newdispq[numpris];
	dp->disp_npri = numpris;
}

static void
disp_dq_free(struct disp_queue_info *dptr)
{
	if (dptr->olddispq != NULL)
		kmem_free(dptr->olddispq,
		    dptr->oldnglobpris * sizeof (dispq_t));
	if (dptr->olddqactmap != NULL)
		kmem_free(dptr->olddqactmap,
		    ((dptr->oldnglobpris / BT_NBIPUL) + 1) * sizeof (long));
}

/*
 * For a newly created CPU, initialize the dispatch queue.
 * This is called before the CPU is known through cpu[] or on any lists.
 */
void
disp_cpu_init(cpu_t *cp)
{
	disp_t	*dp;
	dispq_t	*newdispq;
	ulong_t	*newdqactmap;

	ASSERT(MUTEX_HELD(&cpu_lock));	/* protect dispatcher queue sizes */

	dp = &cp->cpu_disp;
	dp->disp_cpu = cp;
	dp->disp_maxrunpri = -1;
	dp->disp_max_unbound_pri = -1;
	DISP_LOCK_INIT(&cp->cpu_thread_lock);
	/*
	 * Allocate memory for the dispatcher queue headers
	 * and the active queue bitmap.
	 */
	newdispq = kmem_zalloc(v.v_nglobpris * sizeof (dispq_t), KM_SLEEP);
	newdqactmap = kmem_zalloc(((v.v_nglobpris / BT_NBIPUL) + 1) *
	    sizeof (long), KM_SLEEP);
	dp->disp_q = newdispq;
	dp->disp_qactmap = newdqactmap;
	dp->disp_q_limit = &newdispq[v.v_nglobpris];
	dp->disp_npri = v.v_nglobpris;
}

void
disp_cpu_fini(cpu_t *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));

	disp_kp_free(&cp->cpu_disp);
}

/*
 * Allocate new, larger kpreempt dispatch queues to replace the old ones.
 */
void
disp_kp_alloc(pri_t npri)
{
	cpupart_t *cpp;
	struct disp_queue_info	mem_info;

	ASSERT(MUTEX_HELD(&cp_list_lock));

	cpp = cp_list_head;
	do {
		if (npri > cpp->cp_kp_queue.disp_npri) {
			/*
			 * Allocate memory for the new array.
			 */
			disp_dq_alloc(&mem_info, npri, &cpp->cp_kp_queue);

			/*
			 * We need to copy the old structures to the new
			 * and free the old.
			 */
			disp_dq_assign(&mem_info, npri);
			disp_dq_free(&mem_info);
		}
		cpp = cpp->cp_next;
	} while (cpp != cp_list_head);
}

/*
 * Free dispatch queue.
 * Used for the kpreempt queues for a removed CPU partition and
 * for deleted CPUs.
 */
void
disp_kp_free(disp_t *dq)
{
	struct disp_queue_info	mem_info;

	mem_info.olddispq = dq->disp_q;
	mem_info.olddqactmap = dq->disp_qactmap;
	mem_info.oldnglobpris = dq->disp_npri;
	disp_dq_free(&mem_info);
}

/*
 * End dispatcher and scheduler initialization.
 */

/*
 * Idle loop.
 */
void
idle()
{
	struct cpu	*cp = CPU;		/* pointer to this CPU */
	processorid_t	cpun = cp->cpu_id;	/* this processor's ID */
	kthread_id_t	t;			/* taken thread */

	CPU_STAT_ADDQ(cp, cpu_sysinfo.idlethread, 1);
	/*
	 * Uniprocessor version of idle loop.
	 * Do this until notified that we're on an actual multiprocessor.
	 */
	while (ncpus == 1) {
		if (cp->cpu_disp.disp_nrunnable != 0)
			swtch();
		else
			(*idle_cpu)();
	}

	/*
	 * Multiprocessor idle loop.
	 */
	for (;;) {
		/*
		 * If CPU is completely quiesced by p_online(2), just wait
		 * here with minimal bus traffic until put online.
		 */
		while (cp->cpu_flags & CPU_QUIESCED)
			;


		if (cp->cpu_disp.disp_nrunnable != 0) {
			unset_idle_cpu(cpun);	/* arch-dependent hook */
			swtch();
		} else {
			if (cp->cpu_flags & CPU_OFFLINE)
				continue;
			if ((t = disp_getwork(cp)) == NULL)
				continue;
			unset_idle_cpu(cpun);	/* arch-dependent hook */
			if (t->t_proc_flag & TP_MSACCT)
				restore_mstate(t);
			swtch_to(t);
		}
		CPU_STAT_ADDQ(cp, cpu_sysinfo.idlethread, 1);
		set_idle_cpu(cpun);	/* arch-dependent hook */
	}
}


/*
 * Preempt the currently running thread in favor of the highest
 * priority thread.  The class of the current thread controls
 * where it goes on the dispatcher queues. If panicking, turn
 * preemption off.
 */
void
preempt()
{
	kthread_t 	*t = curthread;
	klwp_t 		*lwp = ttolwp(curthread);

	if (panicstr)
		return;

	TRACE_0(TR_FAC_DISP, TR_PREEMPT_START, "preempt_start");

	thread_lock(t);

	if (t->t_state != TS_ONPROC || t->t_disp_queue != &CPU->cpu_disp) {
		/*
		 * this thread has already been chosen to be run on
		 * another CPU. Clear kprunrun on this CPU since we're
		 * already headed for swtch().
		 */
		CPU->cpu_kprunrun = 0;
		thread_unlock_nopreempt(t);
		TRACE_0(TR_FAC_DISP, TR_PREEMPT_END, "preempt_end");
	} else {
		if (lwp != NULL)
			lwp->lwp_ru.nivcsw++;
		CPU_STAT_ADDQ(CPU, cpu_sysinfo.inv_swtch, 1);
		THREAD_TRANSITION(t);
		CL_PREEMPT(t);
		thread_unlock_nopreempt(t);

		TRACE_0(TR_FAC_DISP, TR_PREEMPT_END, "preempt_end");

		swtch();		/* clears CPU->cpu_runrun via disp() */
	}
}

extern kthread_id_t	thread_unpin();

/*
 * disp() - find the highest priority thread for this processor to run, and
 * set it in TS_ONPROC state so that resume() can be called to run it.
 */
static kthread_id_t
disp()
{
	cpu_t		*cpup;
	disp_t		*dp;
	kthread_id_t	tp;
	dispq_t		*dq;
	int		maxrunword;
	pri_t		pri;
	disp_t		*kpq;

	TRACE_0(TR_FAC_DISP, TR_DISP_START, "disp_start");

	cpup = CPU;
	/*
	 * Find the highest priority loaded, runnable thread.
	 */
	dp = &cpup->cpu_disp;

reschedule:
	/*
	 * If there is more important work on the global queue with a better
	 * priority than the maximum on this CPU, take it now.
	 */
	kpq = &cpup->cpu_part->cp_kp_queue;
	while ((pri = kpq->disp_maxrunpri) >= 0 &&
	    pri >= dp->disp_maxrunpri &&
	    (cpup->cpu_flags & CPU_OFFLINE) == 0 &&
	    (tp = disp_getbest(kpq)) != NULL) {
		if (disp_ratify(tp, kpq) != NULL) {
			TRACE_3(TR_FAC_DISP, TR_DISP_END,
			    "disp_end:pri %d tid %p (%T)",
			    DISP_PRIO(tp), tp, tp);
			if (tp->t_proc_flag & TP_MSACCT)
				restore_mstate(tp);
			return (tp);
		}
	}

	disp_lock_enter(&dp->disp_lock);
	pri = dp->disp_maxrunpri;

	/*
	 * If there is nothing to run, look at what's runnable on other queues.
	 * Choose the idle thread if the CPU is quiesced.
	 * Note that CPUs that have the CPU_OFFLINE flag set can still run
	 * interrupt threads, which will be the only threads on the CPU's own
	 * queue, but cannot run threads from other queues.
	 */
	if (pri == -1) {
		if (!(cpup->cpu_flags & CPU_OFFLINE)) {
			disp_lock_exit(&dp->disp_lock);
			if ((tp = disp_getwork(cpup)) == NULL) {
				tp = cpup->cpu_idle_thread;
				(void) splhigh();
				THREAD_ONPROC(tp, cpup);
				cpup->cpu_dispthread = tp;
				cpup->cpu_dispatch_pri = -1;
				cpup->cpu_runrun = cpup->cpu_kprunrun = 0;
				cpup->cpu_chosen_level = -1;
			}
		} else {
			disp_lock_exit_high(&dp->disp_lock);
			tp = cpup->cpu_idle_thread;
			THREAD_ONPROC(tp, cpup);
			cpup->cpu_dispthread = tp;
			cpup->cpu_dispatch_pri = -1;
			cpup->cpu_runrun = cpup->cpu_kprunrun = 0;
			cpup->cpu_chosen_level = -1;
		}
		TRACE_3(TR_FAC_DISP, TR_DISP_END,
			"disp_end:pri %d tid %p (%T)", DISP_PRIO(tp), tp, tp);
		if (tp->t_proc_flag & TP_MSACCT)
			restore_mstate(tp);
		return (tp);
	}

	dq = &dp->disp_q[pri];
	tp = dq->dq_first;

	ASSERT(tp != NULL);
	ASSERT(tp->t_schedflag & TS_LOAD);	/* thread must be swapped in */

	/*
	 * Found it so remove it from queue.
	 */
	dp->disp_nrunnable--;
	dq->dq_sruncnt--;
	if ((dq->dq_first = tp->t_link) == NULL) {
		ulong_t	*dqactmap = dp->disp_qactmap;

		ASSERT(dq->dq_sruncnt == 0);
		dq->dq_last = NULL;

		/*
		 * The queue is empty, so the corresponding bit needs to be
		 * turned off in dqactmap.   If nrunnable != 0 just took the
		 * last runnable thread off the
		 * highest queue, so recompute disp_maxrunpri.
		 */
		maxrunword = pri >> BT_ULSHIFT;
		dqactmap[maxrunword] &= ~BT_BIW(pri);

		if (dp->disp_nrunnable == 0) {
			dp->disp_max_unbound_pri = -1;
			dp->disp_maxrunpri = -1;
		} else {
			int ipri;

			ipri = bt_gethighbit(dqactmap, maxrunword);
			dp->disp_maxrunpri = ipri;
			if (ipri < dp->disp_max_unbound_pri)
				dp->disp_max_unbound_pri = ipri;
		}
	} else {
		tp->t_link = NULL;
	}

	/*
	 * Set TS_DONT_SWAP flag to prevent another processor from swapping
	 * out this thread before we have a chance to run it.
	 * While running, it is protected against swapping by t_lock.
	 */
	tp->t_schedflag |= TS_DONT_SWAP;
	cpup->cpu_dispthread = tp;		/* protected by spl only */
	cpup->cpu_dispatch_pri = pri;
	ASSERT(pri == DISP_PRIO(tp));
	thread_onproc(tp, cpup);  		/* set t_state to TS_ONPROC */
	disp_lock_exit_high(&dp->disp_lock);	/* drop run queue lock */

	ASSERT(tp != NULL);
	TRACE_3(TR_FAC_DISP, TR_DISP_END,
		"disp_end:pri %d tid %p (%T)", pri, tp, tp);

	if (disp_ratify(tp, kpq) == NULL)
		goto reschedule;
	if (tp->t_proc_flag & TP_MSACCT)
		restore_mstate(tp);
	return (tp);
}

/*
 * swtch()
 *	Find best runnable thread and run it.
 *	Called with the current thread already switched to a new state,
 *	on a sleep queue, run queue, stopped, and not zombied.
 *	May be called at any spl level less than or equal to LOCK_LEVEL.
 *	Always drops spl to the base level (spl0()).
 */
void
swtch()
{
	kthread_id_t	t = curthread;
	kthread_id_t	next;
	cpu_t		*cp;

	TRACE_0(TR_FAC_DISP, TR_SWTCH_START, "swtch_start");

	if (t->t_intr != NULL) {
		/*
		 * We are an interrupt thread.  Setup and return
		 * the interrupted thread to be resumed.
		 */
		(void) splhigh();	/* block other scheduler action */
		cp = CPU;		/* now protected against migration */
		ASSERT(cp->cpu_on_intr == 0);	/* not called with PIL > 10 */
		CPU_STAT_ADDQ(cp, cpu_sysinfo.pswitch, 1);
		CPU_STAT_ADDQ(cp, cpu_sysinfo.intrblk, 1);
		next = thread_unpin();
		TRACE_0(TR_FAC_DISP, TR_RESUME_START, "resume_start");
		resume_from_intr(next);
	} else if (t->t_handoff != NULL) {
		/*
		 * Deal with a "handoff".
		 */
		next = t->t_handoff;
		t->t_handoff = NULL;
		shuttle_resume_async(next);
	} else {
#ifdef	DEBUG
		if (t->t_state == TS_ONPROC &&
		    t->t_disp_queue->disp_cpu == CPU &&
		    t->t_preempt == 0) {
			thread_lock(t);
			ASSERT(t->t_state != TS_ONPROC ||
			    t->t_disp_queue->disp_cpu != CPU ||
			    t->t_preempt != 0);	/* cannot migrate */
			thread_unlock_nopreempt(t);
		}
#endif	/* DEBUG */
		cp = CPU;
		next = disp();		/* returns with spl high */
		CPU_STAT_ADDQ(cp, cpu_sysinfo.pswitch, 1);
		ASSERT(cp->cpu_on_intr == 0);	/* not called with PIL > 10 */
		if (next != t) {
			cp->cpu_last_swtch = t->t_disp_time = lbolt;
			TRACE_0(TR_FAC_DISP, TR_RESUME_START, "resume_start");
#ifndef NPROBE
			if (tnf_tracing_active)
				tnf_thread_switch(next);
#endif /* NPROBE */
			resume(next);
			/*
			 * The TR_RESUME_END and TR_SWTCH_END trace points
			 * appear at the end of resume(), because we may not
			 * return here
			 */
		} else {
			TRACE_0(TR_FAC_DISP, TR_SWTCH_END, "swtch_end");
			(void) spl0();
		}
	}
}

/*
 * swtch_from_zombie()
 *	Special case of swtch(), which allows checks for TS_ZOMB to be
 *	eliminated from normal resume.
 *	Find best runnable thread and run it.
 *	Called with the current thread zombied.
 *	Zombies cannot migrate, so CPU references are safe.
 */
void
swtch_from_zombie()
{
	kthread_id_t	next;

	TRACE_0(TR_FAC_DISP, TR_SWTCH_START, "swtch_start");

	ASSERT(curthread->t_state == TS_ZOMB);

	next = disp();			/* returns with spl high */
	ASSERT(CPU->cpu_on_intr == 0);	/* not called with PIL > 10 */
	CPU_STAT_ADDQ(CPU, cpu_sysinfo.pswitch, 1);
	ASSERT(next != curthread);
	TRACE_0(TR_FAC_DISP, TR_RESUME_START, "resume_start");
#ifndef NPROBE
	if (tnf_tracing_active)
		tnf_thread_switch(next);
#endif /* NPROBE */
	resume_from_zombie(next);
	/*
	 * The TR_RESUME_END and TR_SWTCH_END trace points
	 * appear at the end of resume(), because we certainly will not
	 * return here
	 */
}

#if defined(DEBUG) && (defined(DISP_DEBUG) || defined(lint))
static int
thread_on_queue(kthread_t *tp)
{
	cpu_t	*cp;
	cpu_t	*self;
	disp_t	*dp;

	self = CPU;
	cp = self->cpu_next_onln;
	dp = &cp->cpu_disp;
	for (;;) {
		dispq_t		*dq;
		dispq_t		*eq;

		disp_lock_enter_high(&dp->disp_lock);
		for (dq = dp->disp_q, eq = dp->disp_q_limit; dq < eq; ++dq) {
			kthread_t	*rp;

			ASSERT(dq->dq_last == NULL ||
				dq->dq_last->t_link == NULL);
			for (rp = dq->dq_first; rp; rp = rp->t_link)
				if (tp == rp) {
					disp_lock_exit_high(&dp->disp_lock);
					return (1);
				}
		}
		disp_lock_exit_high(&dp->disp_lock);
		if (cp == NULL)
			break;
		if (cp == self) {
			cp = NULL;
			dp = &cp->cpu_part->cp_kp_queue;
		} else {
			cp = cp->cpu_next_onln;
			dp = &cp->cpu_disp;
		}
	}
	return (0);
}	/* end of thread_on_queue */
#else

#define	thread_on_queue(tp)	0	/* ASSERT must be !thread_on_queue */

#endif  /* DEBUG */

/*
 * like swtch(), but switch to a specified thread taken from another CPU.
 *	called with spl high..
 */
void
swtch_to(kthread_id_t next)
{
	cpu_t			*cp = CPU;

	TRACE_0(TR_FAC_DISP, TR_SWTCH_START, "swtch_start");

	/*
	 * Update context switch statistics.
	 */
	CPU_STAT_ADDQ(cp, cpu_sysinfo.pswitch, 1);

	TRACE_0(TR_FAC_DISP, TR_RESUME_START, "resume_start");

	/* record last execution time */
	cp->cpu_last_swtch = curthread->t_disp_time = lbolt;
#ifndef NPROBE
	if (tnf_tracing_active)
		tnf_thread_switch(next);
#endif /* NPROBE */

	resume(next);
	/*
	 * The TR_RESUME_END and TR_SWTCH_END trace points
	 * appear at the end of resume(), because we may not
	 * return here
	 */
}



#define	CPU_IDLING(pri)	((pri) == -1)

static void
cpu_resched(cpu_t *cp, pri_t tpri)
{
	pri_t   cpupri = cp->cpu_dispatch_pri;
	if (!CPU_IDLING(cpupri) && (cpupri < tpri)) {
		TRACE_2(TR_FAC_DISP, TR_CPU_RESCHED,
		    "CPU_RESCHED:Tpri %d Cpupri %d", tpri, cpupri);
		if (tpri >= upreemptpri && cp->cpu_runrun == 0) {
			cp->cpu_runrun = 1;
			aston(cp->cpu_dispthread);
			if (tpri < kpreemptpri && cp != CPU)
				poke_cpu(cp->cpu_id);
		}
		if (tpri >= kpreemptpri && cp->cpu_kprunrun == 0) {
			cp->cpu_kprunrun = 1;
			if (cp != CPU)
				poke_cpu(cp->cpu_id);
		}
	}
}


/*
 * To help balance the run queues, setbackdq tries to maintain
 * difference in run queue length between the chosen CPU and the next one
 * of less than MAX_RUNQ_DIFF threads.  For the lowest priority levels, the
 * queue's lengths must match.
 */
#define	Q_MATCH_PRI	16	/* pri below which queue lengths must match */
#define	MAX_RUNQ_DIFF	2	/* maximum runq diff */

/*
 * Put the specified thread on the back of the dispatcher
 * queue corresponding to its current priority.
 *
 * Called with the thread in transition, onproc or stopped state
 * and locked (transition implies locked) and at high spl.
 * Returns with the thread in TS_RUN state and still locked.
 */
void
setbackdq(kthread_id_t tp)
{
	dispq_t	*dq;
	disp_t		*dp;
	cpu_t		*cp;
	pri_t		tpri;

	ASSERT(THREAD_LOCK_HELD(tp));
	ASSERT((tp->t_schedflag & TS_ALLSTART) == 0);

	if ((tp->t_proc_flag & TP_MSACCT) && tp->t_waitrq == 0)
		tp->t_waitrq = gethrtime();

	ASSERT(!thread_on_queue(tp));	/* make sure tp isn't on a runq */

	/*
	 * If thread is "swapped" or on the swap queue don't
	 * queue it, but wake sched.
	 */
	if ((tp->t_schedflag & (TS_LOAD | TS_ON_SWAPQ)) != TS_LOAD) {
		disp_swapped_setrun(tp);
		return;
	}

	tpri = DISP_PRIO(tp);

	if (ncpus == 1)
		cp = tp->t_cpu;
	else if ((cp = tp->t_bound_cpu) == NULL) {
		int	qlen;

		if (tpri >= kpqpri) {
			setkpdq(tp, SETKP_BACK);
			return;
		}
		/*
		 * Let cpu_choose select the CPU.
		 */
		cp = cpu_choose(tp, tpri);

		if (tp->t_cpupart == cp->cpu_part) {
			qlen = cp->cpu_disp.disp_q[tpri].dq_sruncnt;
			if (tpri >= Q_MATCH_PRI)
				qlen -= MAX_RUNQ_DIFF;
			if (qlen > 0) {
				cpu_t	*np;

				np = cp->cpu_next_part;
				if (np->cpu_disp.disp_q[tpri].dq_sruncnt < qlen)
					cp = np;
			}
		} else {
			/*
			 * Migrate to a cpu in the new partition.
			 */
			cp = disp_lowpri_cpu(tp->t_cpupart->cp_cpulist, 0);
		}
		ASSERT((cp->cpu_flags & CPU_QUIESCED) == 0);
	}
	dp = &cp->cpu_disp;
	disp_lock_enter_high(&dp->disp_lock);

	TRACE_4(TR_FAC_DISP, TR_BACKQ, "setbackdq:pri %d cpu %d tid %p (%T)",
		tpri, cp->cpu_id, tp, tp);

#ifndef NPROBE
	/* Kernel probe */
	if (tnf_tracing_active)
		tnf_thread_queue(tp, cp, tpri);
#endif /* NPROBE */

	ASSERT(tpri >= 0 && tpri < dp->disp_npri);
	THREAD_RUN(tp, &dp->disp_lock);		/* set t_state to TS_RUN */
	tp->t_disp_queue = dp;
	tp->t_link = NULL;

	dq = &dp->disp_q[tpri];
	dp->disp_nrunnable++;
	if (dq->dq_sruncnt++ != 0) {
		ASSERT(dq->dq_first != NULL);
		dq->dq_last->t_link = tp;
		dq->dq_last = tp;
	} else {
		ASSERT(dq->dq_first == NULL);
		ASSERT(dq->dq_last == NULL);
		dq->dq_first = dq->dq_last = tp;
		BT_SET(dp->disp_qactmap, tpri);
		if (tpri > dp->disp_maxrunpri) {
			dp->disp_maxrunpri = tpri;
			membar_enter();
			cpu_resched(cp, tpri);
		}
	}

	if (tp->t_bound_cpu == NULL && tpri > dp->disp_max_unbound_pri)
		dp->disp_max_unbound_pri = tpri;
}

/*
 * Put the specified thread on the front of the dispatcher
 * queue corresponding to its current priority.
 *
 * Called with the thread in transition, onproc or stopped state
 * and locked (transition implies locked) and at high spl.
 * Returns with the thread in TS_RUN state and still locked.
 */
void
setfrontdq(kthread_id_t tp)
{
	disp_t		*dp;
	dispq_t		*dq;
	cpu_t		*cp;
	pri_t		tpri;

	ASSERT(THREAD_LOCK_HELD(tp));
	ASSERT((tp->t_schedflag & TS_ALLSTART) == 0);

	if ((tp->t_proc_flag & TP_MSACCT) && tp->t_waitrq == 0)
		tp->t_waitrq = gethrtime();

	ASSERT(!thread_on_queue(tp));	/* make sure tp isn't on a runq */

	/*
	 * If thread is "swapped" or on the swap queue don't
	 * queue it, but wake sched.
	 */
	if ((tp->t_schedflag & (TS_LOAD | TS_ON_SWAPQ)) != TS_LOAD) {
		disp_swapped_setrun(tp);
		return;
	}

	tpri = DISP_PRIO(tp);

	if (ncpus == 1)
		cp = tp->t_cpu;
	else if ((cp = tp->t_bound_cpu) == NULL) {
		if (tpri >= kpqpri) {
			setkpdq(tp, SETKP_FRONT);
			return;
		}
		cp = tp->t_cpu;
		if (tp->t_cpupart == cp->cpu_part) {
			/*
			 * If we are of higher or equal priority than
			 * the highest priority runnable thread of
			 * the current CPU, just pick this CPU.  Otherwise
			 * Let cpu_choose() select the CPU.
			 */
			if (tpri < cp->cpu_disp.disp_maxrunpri)
				cp = cpu_choose(tp, tpri);
		} else {
			/*
			 * Migrate to a cpu in the new partition.
			 */
			cp = disp_lowpri_cpu(tp->t_cpupart->cp_cpulist, 0);
		}
		ASSERT((cp->cpu_flags & CPU_QUIESCED) == 0);
	}
	dp = &cp->cpu_disp;
	disp_lock_enter_high(&dp->disp_lock);

	TRACE_4(TR_FAC_DISP, TR_FRONTQ, "setfrontdq:pri %d cpu %d tid %p (%T)",
		tpri, cp->cpu_id, tp, tp);

#ifndef NPROBE
	/* Kernel probe */
	if (tnf_tracing_active)
		tnf_thread_queue(tp, cp, tpri);
#endif /* NPROBE */

	ASSERT(tpri >= 0 && tpri < dp->disp_npri);
	THREAD_RUN(tp, &dp->disp_lock);		/* set TS_RUN state and lock */

	tp->t_disp_queue = dp;
	dq = &dp->disp_q[tpri];
	dp->disp_nrunnable++;
	if (dq->dq_sruncnt++ != 0) {
		ASSERT(dq->dq_last != NULL);
		tp->t_link = dq->dq_first;
		dq->dq_first = tp;
	} else {
		ASSERT(dq->dq_last == NULL);
		ASSERT(dq->dq_first == NULL);
		tp->t_link = NULL;
		dq->dq_first = dq->dq_last = tp;
		BT_SET(dp->disp_qactmap, tpri);
		if (tpri > dp->disp_maxrunpri) {
			dp->disp_maxrunpri = tpri;
			membar_enter();
			cpu_resched(cp, tpri);
		}
	}

	if (tp->t_bound_cpu == NULL && tpri > dp->disp_max_unbound_pri)
		dp->disp_max_unbound_pri = tpri;
}

/*
 * Put a high-priority unbound thread on the kp queue
 */
static void
setkpdq(kthread_id_t tp, int borf)
{
	dispq_t	*dq;
	disp_t	*dp;
	cpu_t	*cp;
	pri_t	tpri;

	tpri = DISP_PRIO(tp);

	dp = &tp->t_cpupart->cp_kp_queue;
	disp_lock_enter_high(&dp->disp_lock);

	TRACE_4(TR_FAC_DISP, TR_FRONTQ, "setkpdq:pri %d borf %d tid %p (%T)",
		tpri, borf, tp, tp);

	ASSERT(tpri >= 0 && tpri < dp->disp_npri);
	THREAD_RUN(tp, &dp->disp_lock);		/* set t_state to TS_RUN */
	tp->t_disp_queue = dp;
	dp->disp_nrunnable++;
	dq = &dp->disp_q[tpri];

	if (dq->dq_sruncnt++ != 0) {
		if (borf == SETKP_BACK) {
			ASSERT(dq->dq_first != NULL);
			tp->t_link = NULL;
			dq->dq_last->t_link = tp;
			dq->dq_last = tp;
		} else {
			ASSERT(dq->dq_last != NULL);
			tp->t_link = dq->dq_first;
			dq->dq_first = tp;
		}
	} else {
		if (borf == SETKP_BACK) {
			ASSERT(dq->dq_first == NULL);
			ASSERT(dq->dq_last == NULL);
			dq->dq_first = dq->dq_last = tp;
		} else {
			ASSERT(dq->dq_last == NULL);
			ASSERT(dq->dq_first == NULL);
			tp->t_link = NULL;
			dq->dq_first = dq->dq_last = tp;
		}
		BT_SET(dp->disp_qactmap, tpri);
		if (tpri > dp->disp_max_unbound_pri)
			dp->disp_max_unbound_pri = tpri;
		if (tpri > dp->disp_maxrunpri)
			dp->disp_maxrunpri = tpri;
	}

	cp = tp->t_cpu;
	if (tp->t_cpupart != cp->cpu_part) {
		/* migrate to a cpu in the new partition */
		cp = tp->t_cpupart->cp_cpulist;
	}
	cp = disp_lowpri_cpu(cp, 0);
	disp_lock_enter_high(&cp->cpu_disp.disp_lock);
	ASSERT((cp->cpu_flags & CPU_QUIESCED) == 0);

#ifndef NPROBE
	/* Kernel probe */
	if (tnf_tracing_active)
		tnf_thread_queue(tp, cp, tpri);
#endif /* NPROBE */

	if (cp->cpu_chosen_level < tpri)
		cp->cpu_chosen_level = tpri;
	cpu_resched(cp, tpri);
	disp_lock_exit_high(&cp->cpu_disp.disp_lock);
}

/*
 * Remove a thread from the dispatcher queue if it is on it.
 * It is not an error if it is not found but we return whether
 * or not it was found in case the caller wants to check.
 */
int
dispdeq(kthread_id_t tp)
{
	disp_t		*dp;
	dispq_t		*dq;
	kthread_id_t	rp;
	kthread_id_t	trp;
	kthread_id_t	*ptp;
	int		tpri;

	ASSERT(THREAD_LOCK_HELD(tp));

	if (tp->t_state != TS_RUN)
		return (0);

	/*
	 * The thread is "swapped" or is on the swap queue and
	 * hence no longer on the run queue, so return true.
	 */
	if ((tp->t_schedflag & (TS_LOAD | TS_ON_SWAPQ)) != TS_LOAD)
		return (1);

	tpri = DISP_PRIO(tp);
	dp = tp->t_disp_queue;
	ASSERT(tpri < dp->disp_npri);
	dq = &dp->disp_q[tpri];
	ptp = &dq->dq_first;
	rp = *ptp;
	trp = NULL;

	ASSERT(dq->dq_last == NULL || dq->dq_last->t_link == NULL);

	/*
	 * Search for thread in queue.
	 * Double links would simplify this at the expense of disp/setrun.
	 */
	while (rp != tp && rp != NULL) {
		trp = rp;
		ptp = &trp->t_link;
		rp = trp->t_link;
	}

	if (rp == NULL) {
		panic("dispdeq: thread not on queue");
	}

	/*
	 * Found it so remove it from queue.
	 */
	if ((*ptp = rp->t_link) == NULL)
		dq->dq_last = trp;

	dp->disp_nrunnable--;
	if (--dq->dq_sruncnt == 0) {
		dp->disp_qactmap[tpri >> BT_ULSHIFT] &= ~BT_BIW(tpri);
		if (dp->disp_nrunnable == 0) {
			dp->disp_max_unbound_pri = -1;
			dp->disp_maxrunpri = -1;
		} else if (tpri == dp->disp_maxrunpri) {
			int ipri;

			ipri = bt_gethighbit(dp->disp_qactmap,
			    dp->disp_maxrunpri >> BT_ULSHIFT);
			if (ipri < dp->disp_max_unbound_pri)
				dp->disp_max_unbound_pri = ipri;
			dp->disp_maxrunpri = ipri;
		}
	}
	tp->t_link = NULL;
	THREAD_TRANSITION(tp);		/* put in intermediate state */
	return (1);
}


/*
 * dq_sruninc and dq_srundec are public functions for
 * incrementing/decrementing the sruncnts when a thread on
 * a dispatcher queue is made schedulable/unschedulable by
 * resetting the TS_LOAD flag.
 *
 * The caller MUST have the thread lock and therefore the dispatcher
 * queue lock so that the operation which changes
 * the flag, the operation that checks the status of the thread to
 * determine if it's on a disp queue AND the call to this function
 * are one atomic operation with respect to interrupts.
 */

/*
 * Called by sched AFTER TS_LOAD flag is set on a swapped, runnable thread.
 */
void
dq_sruninc(kthread_id_t t)
{
	ASSERT(t->t_state == TS_RUN);
	ASSERT(t->t_schedflag & TS_LOAD);

	THREAD_TRANSITION(t);
	setfrontdq(t);
}

/*
 * See comment on calling conventions above.
 * Called by sched BEFORE TS_LOAD flag is cleared on a runnable thread.
 */
void
dq_srundec(kthread_id_t t)
{
	ASSERT(t->t_schedflag & TS_LOAD);

	(void) dispdeq(t);
	disp_swapped_enq(t);
}

/*
 * Change the dispatcher lock of thread to the "swapped_lock"
 * and return with thread lock still held.
 *
 * Called with thread_lock held, in transition state, and at high spl.
 */
void
disp_swapped_enq(kthread_id_t tp)
{
	ASSERT(THREAD_LOCK_HELD(tp));
	ASSERT(tp->t_schedflag & TS_LOAD);

	switch (tp->t_state) {
	case TS_RUN:
		disp_lock_enter_high(&swapped_lock);
		THREAD_SWAP(tp, &swapped_lock);	/* set TS_RUN state and lock */
		break;
	case TS_ONPROC:
		disp_lock_enter_high(&swapped_lock);
		THREAD_TRANSITION(tp);
		wake_sched_sec = 1;		/* tell clock to wake sched */
		THREAD_SWAP(tp, &swapped_lock);	/* set TS_RUN state and lock */
		break;
	default:
		panic("disp_swapped: tp: %p bad t_state", (void *)tp);
	}
}

/*
 * This routine is called by setbackdq/setfrontdq if the thread is
 * not loaded or loaded and on the swap queue.
 *
 * Thread state TS_SLEEP implies that a swapped thread
 * has been woken up and needs to be swapped in by the swapper.
 *
 * Thread state TS_RUN, it implies that the priority of a swapped
 * thread is being increased by scheduling class (e.g. ts_update).
 */
static void
disp_swapped_setrun(kthread_id_t tp)
{
	ASSERT(THREAD_LOCK_HELD(tp));
	ASSERT((tp->t_schedflag & (TS_LOAD | TS_ON_SWAPQ)) != TS_LOAD);

	switch (tp->t_state) {
	case TS_SLEEP:
		disp_lock_enter_high(&swapped_lock);
		/*
		 * Wakeup sched immediately (i.e., next tick) if the
		 * thread priority is above maxclsyspri.
		 */
		if (DISP_PRIO(tp) > maxclsyspri)
			wake_sched = 1;
		else
			wake_sched_sec = 1;
		THREAD_RUN(tp, &swapped_lock); /* set TS_RUN state and lock */
		break;
	case TS_RUN:				/* called from ts_update */
		break;
	default:
		panic("disp_swapped_setrun: tp: %p bad t_state", tp);
	}
}


/*
 *	Make a thread give up its processor.  Find the processor on
 *	which this thread is executing, and have that processor
 *	preempt.
 */
void
cpu_surrender(kthread_id_t tp)
{
	cpu_t	*cpup;
	int	max_pri;
	int	max_run_pri;
	klwp_t	*lwp;

	ASSERT(THREAD_LOCK_HELD(tp));

	if (tp->t_state != TS_ONPROC)
		return;
	cpup = tp->t_disp_queue->disp_cpu;	/* CPU thread dispatched to */
	max_pri = cpup->cpu_disp.disp_maxrunpri; /* best priority of that CPU */
	max_run_pri = cpup->cpu_part->cp_kp_queue.disp_maxrunpri;
	if (max_pri < max_run_pri)
		max_pri = max_run_pri;

	cpup->cpu_runrun = 1;
	if (max_pri >= kpreemptpri && cpup->cpu_kprunrun == 0) {
		cpup->cpu_kprunrun = 1;
	}
	/*
	 * Make the target thread take an excursion through trap()
	 * to do preempt() (unless we're already in trap or post_syscall,
	 * calling cpu_surrender via CL_TRAPRET).
	 */
	if (tp != curthread || (lwp = tp->t_lwp) == NULL ||
	    lwp->lwp_state != LWP_USER) {
		aston(tp);
		if (cpup != CPU)
			poke_cpu(cpup->cpu_id);
	}
	TRACE_4(TR_FAC_DISP, TR_CPU_SURRENDER,
	    "cpu_surrender:tid %p (%T) pri %d surrenders cpu %d",
	    tp, tp, tp->t_pri, cpup->cpu_id);
}


/*
 * Commit to and ratify a scheduling decision
 */
/*ARGSUSED*/
static kthread_id_t
disp_ratify(kthread_id_t tp, disp_t *kpq)
{
	pri_t	tpri, maxpri;
	pri_t	maxkpri;
	cpu_t	*cpup;

	ASSERT(tp != NULL);
	/*
	 * Commit to, then ratify scheduling decision
	 */
	cpup = CPU;
	if (cpup->cpu_runrun != 0)
		cpup->cpu_runrun = 0;
	if (cpup->cpu_kprunrun != 0)
		cpup->cpu_kprunrun = 0;
	if (cpup->cpu_chosen_level != -1)
		cpup->cpu_chosen_level = -1;
	membar_enter();
	tpri = DISP_PRIO(tp);
	maxpri = cpup->cpu_disp.disp_maxrunpri;
	maxkpri = kpq->disp_maxrunpri;
	if (maxpri < maxkpri)
		maxpri = maxkpri;
	if (tpri < maxpri) {
		/*
		 * should have done better
		 * put this one back and indicate to try again
		 */
		cpup->cpu_dispthread = curthread;	/* fixup dispthread */
		cpup->cpu_dispatch_pri = DISP_PRIO(curthread);
		thread_lock_high(tp);
		THREAD_TRANSITION(tp);
		setfrontdq(tp);
		thread_unlock_nopreempt(tp);

		tp = NULL;
	}
	return (tp);
}

/*
 * See if there is any work on the dispatcher queue for other CPUs.
 * If there is, dequeue the best thread and return.
 */
static kthread_id_t
disp_getwork(cpu_t *cp)
{
	cpu_t		*ocp;		/* other CPU */
	cpu_t		*tcp;		/* target CPU */
	kthread_id_t	tp;
	pri_t		maxpri;
	int		s;
	disp_t		*kpq;		/* kp queue for this partition */

	maxpri = -1;
	tcp = NULL;

	kpq = &cp->cpu_part->cp_kp_queue;
	while (kpq->disp_maxrunpri >= 0) {
		/*
		 * Try to take a thread from the kp_queue.
		 */
		tp = (disp_getbest(kpq));
		if (tp)
			return (disp_ratify(tp, kpq));
	}

	s = splhigh();		/* protect the cpu_active list */
	/*
	 * Try to find something to do on another CPU's run queue.
	 * Loop through all other CPUs looking for the one with the highest
	 * priority unbound thread.
	 */
	for (ocp = cp->cpu_next_part; ocp != cp; ocp = ocp->cpu_next_part) {
		pri_t pri;

		ASSERT(CPU_ACTIVE(ocp));
		/*
		 * Don't take thread if it is the only runnable thread.
		 * If the CPU is just handling an interrupt or in the
		 * middle of a preemption, taking the thread might cause
		 * it to experience unnecessary cache misses.
		 */
		if (ocp->cpu_disp.disp_nrunnable <= 1)
			continue;

		pri = ocp->cpu_disp.disp_max_unbound_pri;
		if (pri > maxpri) {
			maxpri = pri;
			tcp = ocp;
		}
	}

	splx(s);

	/*
	 * If another queue looks good, try to transfer one or more threads
	 * from it to our queue.
	 */
	if (tcp) {
		tp = (disp_getbest(&tcp->cpu_disp));
		if (tp)
			return (disp_ratify(tp, kpq));
	}
	return (NULL);
}


/*
 * disp_fix_unbound_pri()
 *	Determines the maximum priority of unbound threads on the queue.
 *	The priority is kept for the queue, but is only increased, never
 *	reduced unless some CPU is looking for something on that queue.
 *
 *	The priority argument is the known upper limit.
 *
 *	Perhaps this should be kept accurately, but that probably means
 *	separate bitmaps for bound and unbound threads.  Since only idled
 *	CPUs will have to do this recalculation, it seems better this way.
 */
static void
disp_fix_unbound_pri(disp_t *dp, pri_t pri)
{
	kthread_id_t	tp;
	dispq_t		*dq;
	ulong_t		*dqactmap = dp->disp_qactmap;
	ulong_t		mapword;
	int		wx;

	ASSERT(DISP_LOCK_HELD(&dp->disp_lock));

	ASSERT(pri >= 0);			/* checked by caller */

	/*
	 * Start the search at the next lowest priority below the supplied
	 * priority.  This depends on the bitmap implementation.
	 */
	do {
		wx = pri >> BT_ULSHIFT;		/* index of word in map */

		/*
		 * Form mask for all lower priorities in the word.
		 */
		mapword = dqactmap[wx] & (BT_BIW(pri) - 1);

		/*
		 * Get next lower active priority.
		 */
		if (mapword != 0) {
			pri = (wx << BT_ULSHIFT) + highbit(mapword) - 1;
		} else if (wx > 0) {
			pri = bt_gethighbit(dqactmap, wx - 1); /* sign extend */
			if (pri < 0)
				break;
		} else {
			pri = -1;
			break;
		}

		/*
		 * Search the queue for unbound, runnable threads.
		 */
		dq = &dp->disp_q[pri];
		tp = dq->dq_first;

		while (tp != NULL && tp->t_bound_cpu != NULL) {
			tp = tp->t_link;
		}

		/*
		 * If a thread was found, set the priority and return.
		 */
	} while (tp == NULL);

	/*
	 * pri holds the maximum unbound thread priority or -1.
	 */
	if (dp->disp_max_unbound_pri != pri)
		dp->disp_max_unbound_pri = pri;
}

/*
 * disp_getbest() - de-queue the highest priority unbound runnable thread.
 *	returns with the thread unlocked and onproc
 *	but at splhigh (like disp()).
 *	returns NULL if nothing found.
 *
 *	Passed a pointer to a dispatch queue not associated with this CPU.
 */
static kthread_id_t
disp_getbest(disp_t *dp)
{
	kthread_id_t	tp;
	dispq_t		*dq;
	pri_t		pri;
	cpu_t		*cp;

	disp_lock_enter(&dp->disp_lock);

	/*
	 * If there is nothing to run, return NULL.
	 */
	pri = dp->disp_max_unbound_pri;
	if (pri == -1) {
		disp_lock_exit_nopreempt(&dp->disp_lock);
		return (NULL);
	}

	dq = &dp->disp_q[pri];
	tp = dq->dq_first;

	/*
	 * Skip over bound threads.
	 * Bound threads can be here even though disp_max_unbound_pri
	 * indicated this level.  Besides, it not always accurate because it
	 * isn't reduced until another CPU looks for work.
	 * Note that tp could be NULL right away due to this.
	 */
	while (tp != NULL && tp->t_bound_cpu != NULL) {
		tp = tp->t_link;
	}

	/*
	 * If there were no unbound threads on this queue, find the queue
	 * where they are and then return NULL so that other CPUs will be
	 * considered.
	 */
	if (tp == NULL) {
		disp_fix_unbound_pri(dp, pri);
		disp_lock_exit_nopreempt(&dp->disp_lock);
		return (NULL);
	}

	/*
	 * Found a runnable, unbound thread, so remove it from queue.
	 * dispdeq() requires that we have the thread locked, and we do,
	 * by virtue of holding the dispatch queue lock.  dispdeq() will
	 * put the thread in transition state, thereby dropping the dispq
	 * lock.
	 */
#ifdef DEBUG
	{
		int	thread_was_on_queue;

		thread_was_on_queue = dispdeq(tp);	/* drops disp_lock */
		ASSERT(thread_was_on_queue);
	}
#else /* DEBUG */
	(void) dispdeq(tp);			/* drops disp_lock */
#endif /* DEBUG */

	tp->t_schedflag |= TS_DONT_SWAP;

	/*
	 * Setup thread to run on the current CPU.
	 */
	cp = CPU;

	tp->t_disp_queue = &cp->cpu_disp;

	cp->cpu_dispthread = tp;		/* protected by spl only */
	cp->cpu_dispatch_pri = pri;
	ASSERT(pri == DISP_PRIO(tp));

	thread_onproc(tp, cp);			/* set t_state to TS_ONPROC */

	/*
	 * Return with spl high so that swtch() won't need to raise it.
	 * The disp_lock was dropped by dispdeq().
	 */

	return (tp);
}

/*
 * disp_bound_common() - common routine for higher level functions
 *	that check for bound threads under certain conditions.
 */
static int
disp_bound_common(cpu_t *cp, int flag)
{
	int		found = 0;
	kthread_id_t	tp;

	ASSERT(flag);

	mutex_enter(&pidlock);
	tp = curthread;		/* faster than allthreads */
	do {
		if (tp->t_state != TS_FREE) {
			/*
			 * If an interrupt thread is busy, but the
			 * caller doesn't care (i.e. BOUND_INTR is off),
			 * then just ignore it and continue through.
			 */
			if ((tp->t_flag & T_INTR_THREAD) &&
			    !(flag & BOUND_INTR))
				continue;

			/*
			 * Skip the idle thread for the CPU
			 * we're about to set offline.
			 */
			if (tp == cp->cpu_idle_thread)
				continue;

			/*
			 * Skip the pause thread for the CPU
			 * we're about to set offline.
			 */
			if (tp == cp->cpu_pause_thread)
				continue;

			if ((flag & BOUND_CPU) &&
					(tp->t_bound_cpu == cp)) {
				found = 1;
				break;
			}

			if ((flag & BOUND_PARTITION) &&
					(tp->t_cpupart == cp->cpu_part)) {
				found = 1;
				break;
			}
		}
	} while ((tp = tp->t_next) != curthread && found == 0);
	mutex_exit(&pidlock);
	return (found);
}

/*
 * disp_bound_threads - return nonzero if threads are bound to the processor.
 *	Called infrequently.  Keep this simple.
 *	Includes threads that are asleep or stopped but not onproc.
 */
int
disp_bound_threads(cpu_t *cp)
{
	return (disp_bound_common(cp, BOUND_CPU));
}

/*
 * disp_bound_anythreads - return nonzero if _any_ threads are bound
 * to the given processor, including interrupt threads.
 */
int
disp_bound_anythreads(cpu_t *cp)
{
	return (disp_bound_common(cp, BOUND_CPU | BOUND_INTR));
}

/*
 * disp_bound_partition - return nonzero if threads are bound to the same
 * partition as the processor.
 *	Called infrequently.  Keep this simple.
 *	Includes threads that are asleep or stopped but not onproc.
 */
int
disp_bound_partition(cpu_t *cp)
{
	return (disp_bound_common(cp, BOUND_PARTITION));
}

/*
 * disp_cpu_inactive - make a CPU inactive by moving all of its unbound
 * threads to other CPUs.
 */
void
disp_cpu_inactive(cpu_t *cp)
{
	kthread_id_t	tp;
	disp_t		*dp = &cp->cpu_disp;
	dispq_t		*dq;
	pri_t		pri;
	int		wasonq;

	disp_lock_enter(&dp->disp_lock);
	while ((pri = dp->disp_max_unbound_pri) != -1) {
		dq = &dp->disp_q[pri];
		tp = dq->dq_first;

		/*
		 * Skip over bound threads.
		 */
		while (tp != NULL && tp->t_bound_cpu != NULL) {
			tp = tp->t_link;
		}

		if (tp == NULL) {
			/* disp_max_unbound_pri must be inaccurate, so fix it */
			disp_fix_unbound_pri(dp, pri);
			continue;
		}

		wasonq = dispdeq(tp);		/* drops disp_lock */
		ASSERT(wasonq);

		setbackdq(tp);
		/*
		 * cp has already been removed from the list of active cpus
		 * and tp->t_cpu has been changed so there is no risk of
		 * tp ending up back on cp.
		 */
		ASSERT(tp->t_cpu != cp);
		thread_unlock(tp);

		disp_lock_enter(&dp->disp_lock);
	}
	disp_lock_exit(&dp->disp_lock);
}

/*
 * disp_lowpri_cpu - find CPU running the lowest priority thread.
 *	The hint passed in is used as a starting point so we don't favor
 *	CPU 0 or any other CPU.  The caller should pass in the most recently
 *	used CPU for the thread.
 *
 *	This function must be called at either high SPL, or with preemption
 *	disabled, so that the "hint" CPU cannot be removed from the online
 *	CPU list while we are traversing it.
 */
cpu_t *
disp_lowpri_cpu(cpu_t *hint, int global)
{
	cpu_t   *bestcpu;
	cpu_t   *cp;
	pri_t   bestpri;
	pri_t   cpupri;

	/*
	 * Scan for a CPU currently running the lowest priority thread.
	 * Cannot get cpu_lock here because it is adaptive.
	 * We do not require lock on CPU list.
	 */
	ASSERT(hint != NULL);
	bestpri = SHRT_MAX;		/* maximum possible priority */
	cp = bestcpu = hint;
	if (global == 0 || (cp->cpu_part->cp_level == CP_PRIVATE)) {
		do {
			cpupri = cp->cpu_dispatch_pri;
			if (cp->cpu_disp.disp_maxrunpri > cpupri)
				cpupri = cp->cpu_disp.disp_maxrunpri;
			if (cp->cpu_chosen_level > cpupri)
				cpupri = cp->cpu_chosen_level;
			if (cpupri < bestpri) {
				bestcpu = cp;
				bestpri = cpupri;
			}
		} while ((cp = cp->cpu_next_part) != hint);
	} else {
		/* allow migration between (public) partitions */
		do {
			if (cp->cpu_part->cp_level == CP_PRIVATE)
				continue;
			cpupri = cp->cpu_dispatch_pri;
			if (cp->cpu_disp.disp_maxrunpri > cpupri)
				cpupri = cp->cpu_disp.disp_maxrunpri;
			if (cp->cpu_chosen_level > cpupri)
				cpupri = cp->cpu_chosen_level;
			if (cpupri < bestpri) {
				bestcpu = cp;
				bestpri = cpupri;
			}
		} while ((cp = cp->cpu_next_onln) != hint);
	}

	/*
	 * The chosen CPU could be offline, but not quiesced, in the
	 * case of cpu_choose() on the clock interrupt thread.
	 */
	ASSERT((bestcpu->cpu_flags & CPU_QUIESCED) == 0);
	return (bestcpu);
}

/*
 * This routine provides the generic idle cpu function for all processors.
 * If a processor has some specific code to execute when idle (say, to stop
 * the pipeline and save power) then that routine should be defined in the
 * processors specific code (module_xx.c) and the global variable idle_cpu
 * set to that function.
 */
static void
generic_idle_cpu()
{
}


/*
 * Select a CPU for this thread to run on.
 */
static cpu_t *
cpu_choose(kthread_id_t t, pri_t tpri)
{
	if (tpri >= kpqpri ||
	    ((lbolt - t->t_disp_time) > rechoose_interval && t != curthread))
		return (disp_lowpri_cpu(t->t_cpu, 0));
	return (t->t_cpu);
}
