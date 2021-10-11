/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)thread.c	1.116	99/11/20 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/pcb.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/sysinfo.h>
#include <sys/var.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/debug.h>
#include <sys/inline.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <vm/seg_kp.h>
#include <sys/machlock.h>
#include <sys/kmem.h>
#include <sys/varargs.h>
#include <sys/turnstile.h>
#include <sys/poll.h>
#include <sys/vtrace.h>
#include <sys/callb.h>
#include <c2/audit.h>
#include <sys/tnf.h>
#include <sys/sobject.h>
#include <sys/cpupart.h>
#include <sys/pset.h>
#include <sys/door.h>
#include <sys/spl.h>
#include <sys/copyops.h>

struct kmem_cache *thread_cache;	/* cache of free threads */
struct kmem_cache *lwp_cache;		/* cache of free lwps */
struct kmem_cache *turnstile_cache;	/* cache of free turnstiles */

/*
 * allthreads is only for use by kmem_readers.  All kernel loops can use
 * the current thread as a start/end point.
 */
static kthread_t *allthreads = &t0;	/* circular list of all threads */

static kcondvar_t reaper_cv;		/* synchronization var */
kthread_t	*thread_deathrow;	/* circular list of reapable threads */
kthread_t	*lwp_deathrow;		/* circular list of reapable threads */
kmutex_t	reaplock;		/* protects lwp and thread deathrows */
kmutex_t	thread_free_lock;	/* protects clock from reaper */
int	thread_reapcnt = 0;		/* number of threads on deathrow */
int	lwp_reapcnt = 0;		/* number of lwps on deathrow */
int	reaplimit = 16;			/* delay reaping until reaplimit */

extern int nthread;

id_t	syscid;				/* system scheduling class ID */
void	*segkp_thread;			/* cookie for segkp pool */

int lwp_cache_sz = 32;
int t_cache_sz = 8;
static uint_t next_t_id = 1;

/*
 * Min/Max stack sizes for lwp's
 */
#define	LWP_MAX_STKSIZE	(256 * 1024)
#define	LWP_MIN_STKSIZE	(8 * 1024)

int	lwp_default_stksize;

/*
 * forward declarations for internal thread specific data (tsd)
 */
static void *tsd_realloc(void *, size_t, size_t);

/*ARGSUSED*/
static int
turnstile_constructor(void *buf, void *cdrarg, int kmflags)
{
	bzero(buf, sizeof (turnstile_t));
	return (0);
}

/*ARGSUSED*/
static void
turnstile_destructor(void *buf, void *cdrarg)
{
	turnstile_t *ts = buf;

	ASSERT(ts->ts_free == NULL);
	ASSERT(ts->ts_waiters == 0);
	ASSERT(ts->ts_inheritor == NULL);
	ASSERT(ts->ts_sleepq[0].sq_first == NULL);
	ASSERT(ts->ts_sleepq[1].sq_first == NULL);
}

void
thread_init(void)
{
	kthread_t *tp;
	extern char sys_name[];
	extern void idle();
	struct cpu *cpu = CPU;

	mutex_init(&reaplock, NULL, MUTEX_SPIN, (void *)ipltospl(LOCK_LEVEL));

	thread_cache = kmem_cache_create("thread_cache", sizeof (kthread_t),
	    PTR24_ALIGN, NULL, NULL, NULL, NULL, NULL, 0);

#if defined(__ia64)
	/*
	 * "struct _klwp" includes a "struct pcb", which includes a
	 * "struct fpu", which needs to be 16-byte aligned on ia64.
	 */
	lwp_cache = kmem_cache_create("lwp_cache", sizeof (klwp_t),
	    16, NULL, NULL, NULL, NULL, NULL, 0);
#else
	lwp_cache = kmem_cache_create("lwp_cache", sizeof (klwp_t),
	    0, NULL, NULL, NULL, NULL, NULL, 0);
#endif

	turnstile_cache = kmem_cache_create("turnstile_cache",
	    sizeof (turnstile_t), 0,
	    turnstile_constructor, turnstile_destructor, NULL, NULL, NULL, 0);

	cred_init();

	curthread->t_ts = kmem_cache_alloc(turnstile_cache, KM_SLEEP);

	if (lwp_default_stksize != DEFAULTSTKSZ) {
		if (lwp_default_stksize % PAGESIZE != 0 ||
		    lwp_default_stksize > LWP_MAX_STKSIZE ||
		    lwp_default_stksize < LWP_MIN_STKSIZE) {
			if (lwp_default_stksize)
				cmn_err(CE_WARN,
				    "Illegal stack size. Using %ld",
				    DEFAULTSTKSZ);
			lwp_default_stksize = DEFAULTSTKSZ;
		}
	}
	segkp_lwp = segkp_cache_init(segkp, lwp_cache_sz,
	    lwp_default_stksize,
	    (KPD_NOWAIT | KPD_HASREDZONE | KPD_LOCKED));

	segkp_thread = segkp_cache_init(segkp, t_cache_sz,
	    DEFAULTSTKSZ,
	    (KPD_HASREDZONE | KPD_LOCKED | KPD_NO_ANON | KPD_NOWAIT));

	(void) getcid(sys_name, &syscid);
	curthread->t_cid = syscid;	/* current thread is t0 */

	/*
	 * Set up the first CPU's idle thread.
	 * It runs whenever the CPU has nothing worthwhile to do.
	 */
	tp = thread_create(NULL, PAGESIZE, idle, NULL, 0, &p0, TS_STOPPED, -1);
	cpu->cpu_idle_thread = tp;
	tp->t_preempt = 1;
	tp->t_disp_queue = &cpu->cpu_disp;
	tp->t_bound_cpu = cpu;
	tp->t_affinitycnt = 1;

	/*
	 * Registering a thread in the callback table is usually
	 * done in the initialization code of the thread. In this
	 * case, we do it right after thread creation to avoid
	 * blocking idle thread while registering itself. It also
	 * avoids the possibility of reregistration in case a CPU
	 * restarts its idle thread.
	 */
	CALLB_CPR_INIT_SAFE(tp, "idle");

	/*
	 * Finish initializing the kernel memory allocator now that
	 * thread_create() is available.
	 */
	kmem_thread_init();
}

/*
 * Create a thread.
 * 	If stk is NULL, the thread is created at the base of the stack
 *	and cannot be swapped.
 */
kthread_t *
thread_create(
	caddr_t	stk,
	size_t	stksize,
	void	(*proc)(),
	caddr_t arg,
	size_t len,
	proc_t *pp,
	int state,
	int pri)
{
	kthread_t *t;
	extern struct classfuncs sys_classfuncs;
	turnstile_t *ts;
#if defined(__ia64)
	size_t regstksize;
#endif

	/*
	 * Every thread keeps a turnstile around in case it needs to block.
	 * The only reason the turnstile is not simply part of the thread
	 * structure is that we may have to break the association whenever
	 * more than one thread blocks on a given synchronization object.
	 * From a memory-management standpoint, turnstiles are like the
	 * "attached mblks" that hang off dblks in the streams allocator.
	 */
	ts = kmem_cache_alloc(turnstile_cache, (state == TS_ONPROC) ?
	    KM_NOSLEEP : KM_SLEEP);

	if (ts == NULL)
		return (NULL);

	if (stk == NULL) {
		/*
		 * alloc both thread and stack in segkp chunk
		 */
#if defined(__ia64)
		/* PAGESIZE and 2*PAGESIZE is not big enough on ia64 */
		if (stksize < DEFAULTSTKSZ)
			stksize = DEFAULTSTKSZ;
#endif
		if (stksize == 0) {
			stksize = DEFAULTSTKSZ;
			stk = (caddr_t)segkp_cache_get(segkp_thread);
		} else {
			stksize = roundup(stksize, PAGESIZE);
			stk = (caddr_t)segkp_get(segkp, stksize,
			    (KPD_HASREDZONE | KPD_NO_ANON | KPD_LOCKED));
		}

		if (stk == NULL) {
			kmem_cache_free(turnstile_cache, ts);
			return (NULL);
		}
		/*
		 * The machine-dependent mutex code may require that
		 * thread pointers (since they may be used for mutex owner
		 * fields) have certain alignment requirements.
		 * PTR24_ALIGN is the size of the alignment quanta.
		 * XXX - assumes stack grows toward low addresses.
		 */
		if (stksize <= sizeof (kthread_t) + PTR24_ALIGN)
			cmn_err(CE_PANIC, "thread_create: proposed stack size"
			    " too small to hold thread.");
#ifdef STACK_GROWTH_DOWN
#if defined(__ia64)
		/* "stksize / 2" may need to be adjusted */
		stksize = stksize / 2;	/* needs to match below */
		regstksize = stksize;
#endif
		stksize -= SA(sizeof (kthread_t) + PTR24_ALIGN - 1);
		stksize &= -PTR24_ALIGN;	/* make thread aligned */
		t = (kthread_t *)(stk + stksize);
		bzero(t, sizeof (kthread_t));
		t->t_stk = stk + stksize;
		t->t_stkbase = stk;
#if defined(__ia64)
		t->t_regstk = stk + regstksize;
		t->t_stksize = regstksize * 2;	/* needs to match above */
#endif
#else	/* stack grows to larger addresses */
		stksize -= SA(sizeof (kthread_t));
		t = (kthread_t *)(stk);
		bzero(t, sizeof (kthread_t));
		t->t_stk = stk + sizeof (kthread_t);
		t->t_stkbase = stk + stksize + sizeof (kthread_t);
#endif	/* STACK_GROWTH_DOWN */
		t->t_flag |= T_TALLOCSTK;
		t->t_swap = stk;
	} else {
		if ((t = kmem_cache_alloc(thread_cache, KM_NOSLEEP)) == NULL) {
			kmem_cache_free(turnstile_cache, ts);
			return (NULL);
		}
		bzero(t, sizeof (kthread_t));
		ASSERT(((uintptr_t)t & (PTR24_ALIGN - 1)) == 0);
		/*
		 * Initialize t_stk to the kernel stack pointer to use
		 * upon entry to the kernel
		 */
#ifdef STACK_GROWTH_DOWN
#if defined(__ia64)
		/* "stksize / 2" may need to be adjusted */
		t->t_stk = stk + (stksize / 2);	/* grows down */
		t->t_regstk = t->t_stk;		/* grows up from same place */
		t->t_stkbase = stk;
		t->t_stksize = stksize;
#else
		t->t_stk = stk + stksize;
		t->t_stkbase = stk;
#endif
#else
		t->t_stk = stk;			/* 3b2-like */
		t->t_stkbase = stk + stksize;
#endif /* STACK_GROWTH_DOWN */
	}

	t->t_ts = ts;

	/*
	 * p_cred could be NULL if it thread_create is called before cred_init
	 * is called in main.
	 */
	mutex_enter(&pp->p_crlock);
	if (pp->p_cred)
		crhold(t->t_cred = pp->p_cred);
	mutex_exit(&pp->p_crlock);
#ifdef	C2_AUDIT
	if (audit_active)
		audit_thread_create(t, state);
#endif
	t->t_start = hrestime.tv_sec;
	t->t_startpc = proc;
	t->t_procp = pp;
	t->t_clfuncs = &sys_classfuncs.thread;
	t->t_cid = syscid;
	t->t_pri = pri;
	t->t_stime = lbolt;
	t->t_schedflag = TS_LOAD | TS_DONT_SWAP;
	t->t_bind_cpu = PBIND_NONE;
	t->t_bind_pset = PS_NONE;
	t->t_plockp = &pp->p_lock;
	t->t_copyops = &default_copyops;
	CPU_STAT_ADDQ(CPU, cpu_sysinfo.nthreads, 1);
#ifdef TRACE
	trace_kthread_label(t, -1);
#endif	/* TRACE */
#ifndef NPROBE
	/* Kernel probe */
	tnf_thread_create(t);
#endif /* NPROBE */
	LOCK_INIT_CLEAR(&t->t_lock);

	/*
	 * Callers who give us a NULL proc must do their own
	 * stack initialization.  e.g. lwp_create()
	 */
	if (proc != NULL) {
		t->t_stk = thread_stk_init(t->t_stk);
		(void) thread_load(t, proc, arg, len);
	}

	mutex_enter(&pidlock);
	nthread++;
	t->t_did = next_t_id++;
	t->t_prev = curthread->t_prev;
	t->t_next = curthread;

	/*
	 * Add the thread to the list of all threads, and initialize
	 * its t_cpu pointer.  We need to block preemption since
	 * cpu_offline walks the thread list looking for threads
	 * with t_cpu pointing to the CPU being offlined.  We want
	 * to make sure that the list is consistent and that if t_cpu
	 * is set, the thread is on the list.
	 */
	kpreempt_disable();
	curthread->t_prev->t_next = t;
	curthread->t_prev = t;

	/*
	 * Threads should never have a NULL t_cpu pointer so assign it
	 * here.  If the thread is being created with state TS_RUN a
	 * better CPU may be chosen when it is placed on the run queue.
	 *
	 * We need to keep kernel preemption disabled when setting all
	 * three fields to keep them in sync.
	 */
	t->t_cpu = CPU;
	t->t_disp_queue = &t->t_cpu->cpu_disp;
	t->t_cpupart = t->t_cpu->cpu_part;
	kpreempt_enable();

	/*
	 * Initialize thread state and the dispatcher lock pointer.
	 * Need to hold onto pidlock to block allthreads walkers until
	 * the state is set.
	 */
	switch (state) {
	case TS_RUN:
		curthread->t_oldspl = splhigh();	/* get dispatcher spl */
		THREAD_SET_STATE(t, TS_STOPPED, &transition_lock);
		CL_SETRUN(t);
		thread_unlock(t);
		break;

	case TS_ONPROC:
		THREAD_ONPROC(t, t->t_cpu);
		break;

	case TS_FREE:
		/*
		 * Free state will be used for intr threads.
		 * The interrupt routine must set the thread dispatcher
		 * lock pointer (t_lockp) if starting on a CPU
		 * other than the current one.
		 */
		THREAD_FREEINTR(t, CPU);
		break;

	case TS_STOPPED:
		THREAD_SET_STATE(t, TS_STOPPED, &stop_lock);
		break;

	default:			/* TS_SLEEP, TS_ZOMB or TS_TRANS */
		cmn_err(CE_PANIC, "thread_create: invalid state %d", state);
	}
	mutex_exit(&pidlock);
	return (t);
}

void
thread_exit()
{
	kthread_t *t = curthread;

	tsd_exit();		/* Clean up this thread's TSD */

	/*
	 * No kernel thread should have called poll() without arranging
	 * calling pollcleanup() here.
	 */
	ASSERT(t->t_pollstate == NULL);
	ASSERT(t->t_schedctl == NULL);
	if (t->t_door)
		door_slam();	/* in case thread did an upcall */

#ifndef NPROBE
	/* Kernel probe */
	if (t->t_tnf_tpdp)
		tnf_thread_exit();
#endif /* NPROBE */

	t->t_preempt++;
	/*
	 * remove thread from the all threads list so that
	 * death-row can use the same pointers.
	 */
	mutex_enter(&pidlock);
	t->t_next->t_prev = t->t_prev;
	t->t_prev->t_next = t->t_next;
	ASSERT(allthreads != t);	/* t0 never exits */
	mutex_exit(&pidlock);

	t->t_state = TS_ZOMB;	/* set zombie thread */
	swtch_from_zombie();	/* give up the CPU */
	/* NOTREACHED */
}

void
thread_free(kthread_t *t)
{
	ASSERT(t != &t0 && t->t_state == TS_FREE);
	ASSERT(t->t_door == NULL);
	ASSERT(t->t_schedctl == NULL);
	ASSERT(t->t_pollstate == NULL);

	t->t_pri = 0;
	t->t_pc = 0;
	t->t_sp = 0;
	t->t_wchan0 = NULL;
	t->t_wchan = NULL;
	if (t->t_cred != NULL) {
		crfree(t->t_cred);
		t->t_cred = 0;
	}
#ifdef	C2_AUDIT
	if (audit_active)
		audit_thread_free(t);
#endif
#ifndef NPROBE
	if (t->t_tnf_tpdp)
		tnf_thread_free(t);
#endif /* NPROBE */
	if (t->t_cldata) {
		CL_EXITCLASS(t->t_cid, (caddr_t *)t->t_cldata);
	}
	if (t->t_rprof != NULL) {
		kmem_free(t->t_rprof, sizeof (*t->t_rprof));
		t->t_rprof = NULL;
	}
	t->t_lockp = NULL;	/* nothing should try to lock this thread now */
	if (t->t_lwp)
		lwp_freeregs(t->t_lwp, 0);
	if (t->t_ctx)
		freectx(t, 0);
	t->t_stk = NULL;
	if (t->t_lwp)
		lwp_stk_fini(t->t_lwp);
	lock_clear(&t->t_lock);

	if (t->t_ts->ts_waiters > 0)
		panic("thread_free: turnstile still active");

	kmem_cache_free(turnstile_cache, t->t_ts);

	free_afd(&t->t_activefd);

	/*
	 * Barrier for clock thread.  The clock holds this lock to
	 * keep the thread from going away while it's looking at it.
	 */
	mutex_enter(&thread_free_lock);
	mutex_exit(&thread_free_lock);

	/*
	 * Free thread struct and its stack.
	 */
	if (t->t_flag & T_TALLOCSTK) {
		/* thread struct is embedded in stack */
		segkp_release(segkp, t->t_swap);
		mutex_enter(&pidlock);
		nthread--;
		mutex_exit(&pidlock);
	} else {
		if (t->t_swap) {
			segkp_release(segkp, t->t_swap);
			t->t_swap = NULL;
		}
		if (t->t_lwp) {
			kmem_cache_free(lwp_cache, t->t_lwp);
			t->t_lwp = NULL;
		}
		mutex_enter(&pidlock);
		nthread--;
		mutex_exit(&pidlock);
		kmem_cache_free(thread_cache, t);
	}
}

/*
 * cleanup zombie threads that are on deathrow.
 */
void
thread_reaper()
{
	kthread_t *t, *l, *next;
	callb_cpr_t cprinfo;

	CALLB_CPR_INIT(&cprinfo, &reaplock, callb_generic_cpr, "t_reaper");
	for (;;) {
		mutex_enter(&reaplock);
		CALLB_CPR_SAFE_BEGIN(&cprinfo);
		while (thread_deathrow == NULL && lwp_deathrow == NULL)
			cv_wait(&reaper_cv, &reaplock);
		CALLB_CPR_SAFE_END(&cprinfo, &reaplock);
		t = thread_deathrow;
		l = lwp_deathrow;
		thread_deathrow = NULL;
		lwp_deathrow = NULL;
		thread_reapcnt = 0;
		lwp_reapcnt = 0;
		mutex_exit(&reaplock);

		/*
		 * Reap threads
		 */
		while (t != NULL) {
			next = t->t_forw;
			thread_free(t);
			t = next;
		}
		/*
		 * Reap lwps
		 */
		while (l != NULL) {
			next = l->t_forw;
			thread_free(l);
			l = next;
		}
	}
}

/*
 * This is called by resume() to put a zombie thread onto deathrow.
 * The thread's state is changed to TS_FREE to indicate that is reapable.
 * This is called from the idle thread so it must not block (just spin).
 */
void
reapq_add(kthread_t *t)
{
	mutex_enter(&reaplock);
	if (ttolwp(t)) {
		t->t_forw = lwp_deathrow;
		lwp_deathrow = t;
		lwp_reapcnt++;
	} else {
		t->t_forw = thread_deathrow;
		thread_deathrow = t;
		thread_reapcnt++;
	}
	if (lwp_reapcnt + thread_reapcnt > reaplimit)
		cv_signal(&reaper_cv);	/* wake the reaper */
	t->t_state = TS_FREE;
	lock_clear(&t->t_lock);
	mutex_exit(&reaplock);
}

/*
 * Install a device context for the current thread
 */
void
installctx(
	kthread_t *t,
	void	*arg,
	void	(*save)(void *),
	void	(*restore)(void *),
	void	(*fork)(void *, void *),
	void	(*lwp_create)(void *, void *),
	void	(*free)(void *, int))
{
	struct ctxop *ctx;

	ctx = kmem_alloc(sizeof (struct ctxop), KM_SLEEP);
	ctx->save_op = save;
	ctx->restore_op = restore;
	ctx->fork_op = fork;
	ctx->lwp_create_op = lwp_create;
	ctx->free_op = free;
	ctx->arg = arg;
	ctx->next = t->t_ctx;
	t->t_ctx = ctx;
}

/*
 * Remove a device context from the current thread
 * (Or allow the agent thread to remove device context from another
 * thread in the same, stopped, process)
 */
int
removectx(
	kthread_t *t,
	void	*arg,
	void	(*save)(void *),
	void	(*restore)(void *),
	void	(*fork)(void *, void *),
	void	(*lwp_create)(void *, void *),
	void	(*free)(void *, int))
{
	struct ctxop *ctx, *prev_ctx;

	ASSERT(t == curthread || ttoproc(t)->p_stat == SIDL ||
	    ttoproc(t)->p_agenttp == curthread);

	prev_ctx = NULL;
	for (ctx = t->t_ctx; ctx != NULL; ctx = ctx->next) {
		if (ctx->save_op == save && ctx->restore_op == restore &&
		    ctx->fork_op == fork && ctx->lwp_create_op == lwp_create &&
		    ctx->free_op == free && ctx->arg == arg) {
			if (prev_ctx)
				prev_ctx->next = ctx->next;
			else
				t->t_ctx = ctx->next;
			(ctx->free_op)(ctx->arg, 0);
			kmem_free(ctx, sizeof (struct ctxop));
			return (1);
		}
		prev_ctx = ctx;
	}
	return (0);
}

void
savectx(kthread_t *t)
{
	struct ctxop *ctx;

	ASSERT(t == curthread);
	for (ctx = t->t_ctx; ctx != 0; ctx = ctx->next)
		(ctx->save_op)(ctx->arg);
}

void
restorectx(kthread_t *t)
{
	struct ctxop *ctx;

	ASSERT(t == curthread);
	for (ctx = t->t_ctx; ctx != 0; ctx = ctx->next)
		(ctx->restore_op)(ctx->arg);
}

void
forkctx(kthread_t *t, kthread_t *ct)
{
	struct ctxop *ctx;

	for (ctx = t->t_ctx; ctx != 0; ctx = ctx->next)
		(ctx->fork_op)(t, ct);
}

/*
 * Note that this operator is only invoked via the _lwp_create
 * system call.  The system may have other reasons to create lwps
 * e.g. the agent lwp or the doors unreferenced lwp.
 */
void
lwp_createctx(kthread_t *t, kthread_t *ct)
{
	struct ctxop *ctx;

	for (ctx = t->t_ctx; ctx != 0; ctx = ctx->next)
		(ctx->lwp_create_op)(t, ct);
}

/*
 * Freectx is called from thread_free and exec to get
 * rid of old device context.
 */
void
freectx(kthread_t *t, int isexec)
{
	struct ctxop *ctx;

	while ((ctx = t->t_ctx) != NULL) {
		t->t_ctx = ctx->next;
		(ctx->free_op)(ctx->arg, isexec);
		kmem_free(ctx, sizeof (struct ctxop));
	}
}

/*
 * Set the thread running; arrange for it to be swapped in if necessary.
 */
void
setrun_locked(kthread_t *t)
{
	ASSERT(THREAD_LOCK_HELD(t));
	if (t->t_state == TS_SLEEP) {
		/*
		 * Take off sleep queue.
		 */
		SOBJ_UNSLEEP(t->t_sobj_ops, t);
	} else if (t->t_state & (TS_RUN | TS_ONPROC)) {
		/*
		 * Already on dispatcher queue.
		 */
		return;
	} else if (t->t_state == TS_STOPPED) {
		/*
		 * All of the sending of SIGCONT (TC_XSTART) and /proc
		 * (TC_PSTART) and lwp_continue() (TC_CSTART) must have
		 * requested that the thread be run.
		 * Just calling setrun() is not sufficient to set a stopped
		 * thread running.  TP_TXSTART is always set if the thread
		 * is not stopped by a jobcontrol stop signal.
		 * TP_TPSTART is always set if /proc is not controlling it.
		 * TP_TCSTART is always set if lwp_stop() didn't stop it.
		 * The thread won't be stopped unless one of these
		 * three mechanisms did it.
		 *
		 * These flags must be set before calling setrun_locked(t).
		 * They can't be passed as arguments because the streams
		 * code calls setrun() indirectly and the mechanism for
		 * doing so admits only one argument.  Note that the
		 * thread must be locked in order to change t_schedflags.
		 */
		if ((t->t_schedflag & TS_ALLSTART) != TS_ALLSTART)
			return;
		/*
		 * Process is no longer stopped (a thread is running).
		 */
		t->t_whystop = 0;
		t->t_whatstop = 0;
		/*
		 * Strictly speaking, we do not have to clear these
		 * flags here; they are cleared on entry to stop().
		 * However, they are confusing when doing kernel
		 * debugging or when they are revealed by ps(1).
		 */
		t->t_schedflag &= ~TS_ALLSTART;
		THREAD_TRANSITION(t);	/* drop stopped-thread lock */
		ASSERT(t->t_lockp == &transition_lock);
		ASSERT(t->t_wchan0 == NULL && t->t_wchan == NULL);
		/*
		 * Let the class put the process on the dispatcher queue.
		 */
		CL_SETRUN(t);
	}


}

void
setrun(kthread_t *t)
{
	thread_lock(t);
	setrun_locked(t);
	thread_unlock(t);
}

/*
 * Unpin an interrupted thread.
 *	When an interrupt occurs, the interrupt is handled on the stack
 *	of an interrupt thread, taken from a pool linked to the CPU structure.
 *
 *	When swtch() is switching away from an interrupt thread because it
 *	blocked or was preempted, this routine is called to complete the
 *	saving of the interrupted thread state, and returns the interrupted
 *	thread pointer so it may be resumed.
 *
 *	Called by swtch() only at high spl.
 */
kthread_t *
thread_unpin()
{
	kthread_t	*t = curthread;	/* current thread */
	kthread_t	*itp;		/* interrupted thread */
	int		i;		/* interrupt level */
	extern int	intr_passivate();

	ASSERT(t->t_intr != NULL);

	itp = t->t_intr;		/* interrupted thread */
	t->t_intr = NULL;		/* clear interrupt ptr */

	/*
	 * Get state from interrupt thread for the one
	 * it interrupted.
	 */

	i = intr_passivate(t, itp);

	TRACE_5(TR_FAC_INTR, TR_INTR_PASSIVATE,
		"intr_passivate:level %d curthread %p (%T) ithread %p (%T)",
		i, t, t, itp, itp);

	/*
	 * Dissociate the current thread from the interrupted thread's LWP.
	 */
	t->t_lwp = NULL;

	/*
	 * Interrupt handlers above the level that spinlocks block must
	 * not block.
	 */
#if DEBUG
	if (i < 0 || i > LOCK_LEVEL)
		cmn_err(CE_PANIC, "thread_unpin: ipl out of range %x", i);
#endif

	/*
	 * Set flag to keep CPU's spl level high enough
	 * to block this interrupt, then recompute the CPU's
	 * base interrupt level from the active interrupts.
	 */
	CPU->cpu_intr_actv |= (1 << i);
	set_base_spl();

	return (itp);
}

/*
 * Release an interrupt
 *
 * When an interrupt occurs, the stack of a new "interrupt thread"
 * is used.  The interrupt priority of the processor is held at a
 * level high enough to block the interrupt until the interrupt thread
 * exits or calls release_interrupt().
 *
 * Since lowering the interrupt level requires us to be able to create
 * a new thread, a thread must be allocated.  If there aren't enough
 * resources to allocate a new thread, the interrupt is not released.
 */
void
release_interrupt()
{
	kthread_t	*t = curthread;	/* current thread */
	int		s;
	struct cpu	*cp;		/* current CPU */
	extern int	intr_level();

	if ((t->t_flag & T_INTR_THREAD) == 0)
		return;
	s = spl7();			/* protect CPU thread lists */
	cp = CPU;
	if (thread_create_intr(cp)) {
		/*
		 * Failed to allocate.  Cannot release interrupt.
		 */
		splx(s);
		return;
	}

	/*
	 * Release the mask for this interrupt level, and recompute the
	 * CPU's base priority level from the new mask.
	 */
	cp->cpu_intr_actv &= ~(1 << intr_level(t));
	set_base_spl();
	t->t_flag &= ~T_INTR_THREAD;	/* clear interrupt thread flag */
	splx(s);
	/*
	 * Give interrupted threads a chance to run.
	 */
	setrun(t);
	swtch();
}

/*
 * Create and initialize an interrupt thread.
 *	Returns non-zero on error.
 *	Called at spl7() or better.
 */
int
thread_create_intr(struct cpu *cp)
{
	kthread_t *tp;

	tp = thread_create(NULL, DEFAULTSTKSZ,
		(void (*)())thread_create_intr, NULL, 0, &p0, TS_ONPROC, 0);
	if (tp == NULL)
		return (-1);

	/*
	 * Set the thread in the TS_FREE state.  The state will change
	 * to TS_ONPROC only while the interrupt is active.  Think of these
	 * as being on a private free list for the CPU.
	 * Being TS_FREE keeps inactive interrupt threads out of the kernel
	 * debugger (kadb)'s threadlist.
	 * We cannot call thread_create with TS_FREE because of the current
	 * checks there for ONPROC.  Fix this when thread_create takes flags.
	 */
	THREAD_FREEINTR(tp, cp);

	/*
	 * Nobody should ever reference the credentials of an interrupt
	 * thread so make it NULL to catch any such references.
	 */
	tp->t_cred = NULL;
	tp->t_flag |= T_INTR_THREAD;
	tp->t_cpu = cp;
	tp->t_bound_cpu = cp;
	tp->t_disp_queue = &cp->cpu_disp;
	tp->t_affinitycnt = 1;
	tp->t_preempt = 1;

	/*
	 * Don't make a user-requested binding on this thread so that
	 * the processor can be offlined.
	 */
	tp->t_bind_cpu = PBIND_NONE;	/* no USER-requested binding */
	tp->t_bind_pset = PS_NONE;

	/*
	 * Link onto CPU's interrupt pool.
	 */
	tp->t_link = cp->cpu_intr_thread;
	cp->cpu_intr_thread = tp;
	return (0);
}

/*
 * TSD -- THREAD SPECIFIC DATA
 */
static kmutex_t		tsd_mutex;	 /* linked list spin lock */
static uint_t		tsd_nkeys;	 /* size of destructor array */
/* per-key destructor funcs */
static void 		(**tsd_destructor)(void *);
/* list of tsd_thread's */
static struct tsd_thread	*tsd_list;

/*
 * Default destructor
 *	Needed because NULL destructor means that the key is unused
 */
/* ARGSUSED */
void
tsd_defaultdestructor(void *value)
{}

/*
 * Create a key (index into per thread array)
 *	Locks out tsd_create, tsd_destroy, and tsd_exit
 *	May allocate memory with lock held
 */
void
tsd_create(uint_t *keyp, void (*destructor)(void *))
{
	int	i;
	uint_t	nkeys;

	/*
	 * if key is allocated, do nothing
	 */
	mutex_enter(&tsd_mutex);
	if (*keyp) {
		mutex_exit(&tsd_mutex);
		return;
	}
	/*
	 * find an unused key
	 */
	if (destructor == NULL)
		destructor = tsd_defaultdestructor;

	for (i = 0; i < tsd_nkeys; ++i)
		if (tsd_destructor[i] == NULL)
			break;

	/*
	 * if no unused keys, increase the size of the destructor array
	 */
	if (i == tsd_nkeys) {
		if ((nkeys = (tsd_nkeys << 1)) == 0)
			nkeys = 1;
		tsd_destructor = (void (**)(void *))tsd_realloc(
		    (void *)tsd_destructor,
		    (size_t)(tsd_nkeys * sizeof (void (*)(void *))),
		    (size_t)(nkeys * sizeof (void (*)(void *))));
		tsd_nkeys = nkeys;
	}

	/*
	 * allocate the next available unused key
	 */
	tsd_destructor[i] = destructor;
	*keyp = i + 1;
	mutex_exit(&tsd_mutex);
}

/*
 * Destroy a key -- this is for unloadable modules
 *
 * Assumes that the caller is preventing tsd_set and tsd_get
 * Locks out tsd_create, tsd_destroy, and tsd_exit
 * May free memory with lock held
 */
void
tsd_destroy(uint_t *keyp)
{
	uint_t key;
	struct tsd_thread *tsd;

	/*
	 * protect the key namespace and our destructor lists
	 */
	mutex_enter(&tsd_mutex);
	key = *keyp;
	*keyp = 0;

	ASSERT(key <= tsd_nkeys);

	/*
	 * if the key is valid
	 */
	if (key != 0) {
		uint_t k = key - 1;
		/*
		 * for every thread with TSD, call key's destructor
		 */
		for (tsd = tsd_list; tsd; tsd = tsd->ts_next) {
			/*
			 * no TSD for key in this thread
			 */
			if (key > tsd->ts_nkeys)
				continue;
			/*
			 * call destructor for key
			 */
			if (tsd->ts_value[k] && tsd_destructor[k])
				(*tsd_destructor[k])(tsd->ts_value[k]);
			/*
			 * reset value for key
			 */
			tsd->ts_value[k] = NULL;
		}
		/*
		 * actually free the key (NULL destructor == unused)
		 */
		tsd_destructor[k] = NULL;
	}

	mutex_exit(&tsd_mutex);
}

/*
 * Quickly return the per thread value that was stored with the specified key
 * Assumes the caller is protecting key from tsd_create and tsd_destroy
 */
void *
tsd_get(uint_t key)
{
	return (tsd_agent_get(curthread, key));
}

/*
 * Set a per thread value indexed with the specified key
 */
int
tsd_set(uint_t key, void *value)
{
	return (tsd_agent_set(curthread, key, value));
}

/*
 * Like tsd_get(), except that the agent lwp can get the tsd of
 * another thread in the same process (the agent thread only runs when the
 * process is completely stopped by /proc), or syslwp is creating a new lwp.
 */
void *
tsd_agent_get(kthread_t *t, uint_t key)
{
	struct tsd_thread *tsd = t->t_tsd;

	ASSERT(t == curthread ||
	    ttoproc(t)->p_agenttp == curthread || t->t_state == TS_STOPPED);

	if (key && tsd != NULL && key <= tsd->ts_nkeys)
		return (tsd->ts_value[key - 1]);
	return (NULL);
}

/*
 * Like tsd_set(), except that the agent lwp can set the tsd of
 * another thread in the same process, or syslwp can set the tsd
 * of a thread it's in the middle of creating.
 *
 * Assumes the caller is protecting key from tsd_create and tsd_destroy
 * May lock out tsd_destroy (and tsd_create), may allocate memory with
 * lock held
 */
int
tsd_agent_set(kthread_t *t, uint_t key, void *value)
{
	struct tsd_thread *tsd = t->t_tsd;

	ASSERT(t == curthread ||
	    ttoproc(t)->p_agenttp == curthread || t->t_state == TS_STOPPED);

	if (key == 0)
		return (EINVAL);
	if (tsd == NULL)
		tsd = t->t_tsd = kmem_zalloc(sizeof (*tsd), KM_SLEEP);
	if (key <= tsd->ts_nkeys) {
		tsd->ts_value[key - 1] = value;
		return (0);
	}

	ASSERT(key <= tsd_nkeys);

	/*
	 * lock out tsd_destroy()
	 */
	mutex_enter(&tsd_mutex);
	if (tsd->ts_nkeys == 0) {
		/*
		 * Link onto list of threads with TSD
		 */
		if ((tsd->ts_next = tsd_list) != NULL)
			tsd_list->ts_prev = tsd;
		tsd_list = tsd;
	}

	/*
	 * Allocate thread local storage and set the value for key
	 */
	tsd->ts_value = tsd_realloc(tsd->ts_value,
	    tsd->ts_nkeys * sizeof (void *),
	    key * sizeof (void *));
	tsd->ts_nkeys = key;
	tsd->ts_value[key - 1] = value;
	mutex_exit(&tsd_mutex);

	return (0);
}


/*
 * Return the per thread value that was stored with the specified key
 *	If necessary, create the key and the value
 *	Assumes the caller is protecting *keyp from tsd_destroy
 */
void *
tsd_getcreate(uint_t *keyp, void (*destroy)(void *), void *(*allocate)(void))
{
	void *value;
	uint_t key = *keyp;
	struct tsd_thread *tsd = curthread->t_tsd;

	if (tsd == NULL)
		tsd = curthread->t_tsd = kmem_zalloc(sizeof (*tsd), KM_SLEEP);
	if (key && key <= tsd->ts_nkeys && (value = tsd->ts_value[key - 1]))
		return (value);
	if (key == 0)
		tsd_create(keyp, destroy);
	(void) tsd_set(*keyp, value = (*allocate)());

	return (value);
}

/*
 * Called from thread_exit() to run the destructor function for each tsd
 *	Locks out tsd_create and tsd_destroy
 *	Assumes that the destructor *DOES NOT* use tsd
 */
void
tsd_exit(void)
{
	int i;
	struct tsd_thread *tsd = curthread->t_tsd;

	if (tsd == NULL)
		return;

	if (tsd->ts_nkeys == 0) {
		kmem_free(tsd, sizeof (*tsd));
		curthread->t_tsd = NULL;
		return;
	}

	/*
	 * lock out tsd_create and tsd_destroy, call
	 * the destructor, and mark the value as destroyed.
	 */
	mutex_enter(&tsd_mutex);

	for (i = 0; i < tsd->ts_nkeys; i++) {
		if (tsd->ts_value[i] && tsd_destructor[i])
			(*tsd_destructor[i])(tsd->ts_value[i]);
		tsd->ts_value[i] = NULL;
	}

	/*
	 * remove from linked list of threads with TSD
	 */
	if (tsd->ts_next)
		tsd->ts_next->ts_prev = tsd->ts_prev;
	if (tsd->ts_prev)
		tsd->ts_prev->ts_next = tsd->ts_next;
	if (tsd_list == tsd)
		tsd_list = tsd->ts_next;

	mutex_exit(&tsd_mutex);

	/*
	 * free up the TSD
	 */
	kmem_free(tsd->ts_value, tsd->ts_nkeys * sizeof (void *));
	kmem_free(tsd, sizeof (struct tsd_thread));
	curthread->t_tsd = NULL;
}

/*
 * realloc
 */
static void *
tsd_realloc(void *old, size_t osize, size_t nsize)
{
	void *new;

	new = kmem_zalloc(nsize, KM_SLEEP);
	if (old) {
		bcopy(old, new, osize);
		kmem_free(old, osize);
	}
	return (new);
}

/*
 * Check to see if an interrupt thread might be active at a given ipl.
 * If so return true.
 * We must be conservative--it is ok to give a false yes, but a false no
 * will cause disaster.  (But if the situation changes after we check it is
 * ok--the caller is trying to ensure that an interrupt routine has been
 * exited).
 * This is used when trying to remove an interrupt handler from an autovector
 * list in avintr.c.
 */
int
intr_active(struct cpu *cp, int level)
{
	if (level < LOCK_LEVEL)
		return (cp->cpu_thread != cp->cpu_dispthread);
	else
		return (cp->cpu_on_intr);
}

/*
 * Return non-zero if an interrupt is being serviced.
 */
int
servicing_interrupt()
{
	/*
	 * Note: single-OR used on purpose to return non-zero if T_INTR_THREAD
	 * flag set or CPU->cpu_on_intr is non-zero (indicating high-level
	 * interrupt).
	 */
	return ((curthread->t_flag & T_INTR_THREAD) | CPU->cpu_on_intr);
}


/*
 * Change the dispatch priority of a thread in the system.
 * Used when raising or lowering a thread's priority.
 * (E.g., priority inheritance)
 *
 * Since threads are queued according to their priority, we
 * we must check the thread's state to determine whether it
 * is on a queue somewhere. If it is, we've got to:
 *
 *	o Dequeue the thread.
 *	o Change its effective priority.
 *	o Enqueue the thread.
 *
 * Assumptions: The thread whose priority we wish to change
 * must be locked before we call thread_change_(e)pri().
 * The thread_change(e)pri() function doesn't drop the thread
 * lock--that must be done by its caller.
 */
void
thread_change_epri(kthread_t *t, pri_t disp_pri)
{
	uint_t	state;

	ASSERT(THREAD_LOCK_HELD(t));

	/*
	 * If the inherited priority hasn't actually changed,
	 * just return.
	 */
	if (t->t_epri == disp_pri)
		return;

	state = t->t_state;

	/*
	 * If it's not on a queue, change the priority with
	 * impunity.
	 */
	if ((state & (TS_SLEEP | TS_RUN)) == 0) {
		t->t_epri = disp_pri;

		if (state == TS_ONPROC) {
			cpu_t *cp = t->t_disp_queue->disp_cpu;

			if (t == cp->cpu_dispthread)
				cp->cpu_dispatch_pri = DISP_PRIO(t);
		}
		return;
	}

	/*
	 * It's either on a sleep queue or a run queue.
	 */
	if (state == TS_SLEEP) {

		/*
		 * Take the thread out of its sleep queue.
		 * Change the inherited priority.
		 * Re-enqueue the thread.
		 * Each synchronization object exports a function
		 * to do this in an appropriate manner.
		 */
		SOBJ_CHANGE_EPRI(t->t_sobj_ops, t, disp_pri);
	} else {
		/*
		 * The thread is on a run queue.
		 * Note: setbackdq() may not put the thread
		 * back on the same run queue where it originally
		 * resided.
		 */
		(void) dispdeq(t);
		t->t_epri = disp_pri;
		setbackdq(t);
	}
}	/* end of thread_change_epri */

/*
 * Function: Change the t_pri field of a thread.
 * Side Effects: Adjust the thread ordering on a run queue
 *		 or sleep queue, if necessary.
 * Returns: 1 if the thread was on a run queue, else 0.
 */
int
thread_change_pri(kthread_t *t, pri_t disp_pri, int front)
{
	uint_t	state;
	int	on_rq = 0;

	ASSERT(THREAD_LOCK_HELD(t));

	state = t->t_state;

	/*
	 * If it's not on a queue, change the priority with
	 * impunity.
	 */
	if ((state & (TS_SLEEP | TS_RUN)) == 0) {
		t->t_pri = disp_pri;

		if (state == TS_ONPROC) {
			cpu_t *cp = t->t_disp_queue->disp_cpu;

			if (t == cp->cpu_dispthread)
				cp->cpu_dispatch_pri = DISP_PRIO(t);
		}
		return (0);
	}

	/*
	 * It's either on a sleep queue or a run queue.
	 */
	if (state == TS_SLEEP) {
		/*
		 * If the priority has changed, take the thread out of
		 * its sleep queue and change the priority.
		 * Re-enqueue the thread.
		 * Each synchronization object exports a function
		 * to do this in an appropriate manner.
		 */
		if (disp_pri != t->t_pri)
			SOBJ_CHANGE_PRI(t->t_sobj_ops, t, disp_pri);
	} else {
		/*
		 * The thread is on a run queue.
		 * Note: setbackdq() may not put the thread
		 * back on the same run queue where it originally
		 * resided.
		 *
		 * We still requeue the thread even if the priority
		 * is unchanged to preserve round-robin (and other)
		 * effects between threads of the same priority.
		 */
		on_rq = dispdeq(t);
		ASSERT(on_rq);
		t->t_pri = disp_pri;
		if (front) {
			setfrontdq(t);
		} else {
			setbackdq(t);
		}
	}
	return (on_rq);
}
