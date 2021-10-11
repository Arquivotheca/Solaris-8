/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)thread.c	1.206	99/11/24 SMI"

#include <stdio.h>
#include <ucontext.h>
#include <sys/signal.h>
#include <errno.h>
#include "libthread.h"
#include "tdb_agent.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <thread.h>

#ifdef __STDC__
#pragma weak thr_create = _thr_create
#pragma weak thr_join = _thr_join
#pragma weak thr_setconcurrency = _thr_setconcurrency
#pragma weak thr_getconcurrency = _thr_getconcurrency
#pragma weak thr_exit = _thr_exit
#pragma weak thr_self = _thr_self
#pragma weak thr_sigsetmask = _thr_sigsetmask
#pragma weak thr_kill = _thr_kill
#pragma weak thr_suspend = _thr_suspend
#pragma weak thr_continue = _thr_continue
#pragma weak thr_yield = _thr_yield
#pragma weak thr_setprio = _thr_setprio
#pragma weak thr_getprio = _thr_getprio
#pragma weak thr_min_stack = _thr_min_stack
#pragma weak thr_main = _thr_main
#pragma weak thr_stksegment = _thr_stksegment

#pragma weak thr_setmutator = _thr_setmutator
#pragma weak thr_mutators_barrier = _thr_mutators_barrier
#pragma weak thr_suspend_allmutators = _thr_suspend_allmutators
#pragma weak thr_suspend_mutator = _thr_suspend_mutator
#pragma weak thr_continue_allmutators = _thr_continue_allmutators
#pragma weak thr_continue_mutator = _thr_continue_mutator
#pragma weak thr_wait_mutator = _thr_wait_mutator
#pragma weak thr_getstate = _thr_getstate
#pragma weak thr_setstate = _thr_setstate
#pragma	weak thr_sighndlrinfo = _thr_sighndlrinfo

#pragma weak pthread_exit = _pthread_exit
#pragma weak pthread_kill = _thr_kill
#pragma weak pthread_self = _thr_self
#pragma weak pthread_sigmask = _thr_sigsetmask
#pragma weak pthread_detach = _thr_detach
#pragma weak _pthread_kill = _thr_kill
#pragma weak _pthread_self = _thr_self
#pragma weak _pthread_sigmask = _thr_sigsetmask
#pragma weak _pthread_detach = _thr_detach
#pragma weak __pthread_min_stack = _thr_min_stack

#pragma	weak _ti_thr_self = _thr_self
#pragma	weak _ti_thr_continue = _thr_continue
#pragma	weak _ti_thr_create = _thr_create
#pragma	weak _ti_thr_errnop = _thr_errnop
#pragma	weak _ti_thr_exit = _thr_exit
#pragma	weak _ti_thr_getconcurrency = _thr_getconcurrency
#pragma	weak _ti_thr_getprio = _thr_getprio
#pragma	weak _ti_thr_join = _thr_join
#pragma	weak _ti_thr_kill = _thr_kill
#pragma	weak _ti_thr_main = _thr_main
#pragma weak _ti_thr_min_stack = _thr_min_stack
#pragma	weak _ti_thr_setconcurrency = _thr_setconcurrency
#pragma	weak _ti_thr_setprio = _thr_setprio
#pragma	weak _ti_thr_sigsetmask = _thr_sigsetmask
#pragma	weak _ti_thr_stksegment = _thr_stksegment
#pragma	weak _ti_thr_suspend = _thr_suspend
#pragma	weak _ti_thr_yield = _thr_yield

#pragma	weak _ti_pthread_self = _thr_self
#pragma	weak _ti_pthread_sigmask  = _thr_sigsetmask
#pragma weak _ti_pthread_exit = _pthread_exit
#pragma weak _ti_pthread_kill = _thr_kill
#pragma weak _ti_pthread_detach = _thr_detach

#endif /* __STDC__ */

uthread_t _thread;
#ifdef TLS
#pragma unshared(_thread);
#endif

extern int __syslwp_suspend(lwpid_t, int *);
/*
 * Static functions
 */
static	int _thrp_stksegment(uthread_t *t, stack_t *stk);
static	int _thrp_kill(uthread_t *t, int ix, int sig);
static	int _thrp_suspend(thread_t tid, int whystopped);
static	int _thrp_continue(thread_t tid, int whystopped);
static	void _thrp_mutators_lock();
static	void _thrp_mutators_unlock();

static	void _suspendself(uthread_t *);
/*
 * Global variables
 */
thrtab_t _allthreads [ALLTHR_TBLSIZ];	/* hash table of all threads */
mutex_t _tidlock = DEFAULTMUTEX;	/* protects access to _lasttid */
thread_t _lasttid = 0;			/* monotonically increasing tid */
sigset_t _pmask;			/* virtual process signal mask */
mutex_t _pmasklock = DEFAULTMUTEX;	/* to protect _pmask updates */
int _first_thr_create = 0;		/* flag to indicate first thr_create */
int _uconcurrency;
mutex_t _concurrencylock = DEFAULTMUTEX;

/*
 * A special cancelation cleanup hook for DCE.
 * _cleanuphndlr, when it is not NULL, will contain a callback
 * function to be called before a thread is terminated in _thr_exit()
 * as a result of being canceled.
 */
int _pthread_setcleanupinit(void (*func)(void));
static void (*_cleanuphndlr)(void) = NULL;

/*
 * _pthread_setcleanupinit: sets the cleanup hook.
 *
 * Returns:
 *	0 for success
 *	some errno on error
 */
int
_pthread_setcleanupinit(void (*func)(void))
{
	_cleanuphndlr = func;
	return (0);
}

/*
 * create a thread that begins executing func(arg). if stk is NULL
 * and stksize is zero, then allocate a default sized stack with a
 * redzone.
 */

int
_thr_create(void *stk, size_t stksize, void *(*func)(void *),
			void *arg, long flags, thread_t *new_thread)
{
	/* default priority which is same as parent thread */
	return (_thrp_create(stk, stksize, func, arg,
	    flags, new_thread, curthread->t_pri, SCHED_OTHER, _lpagesize));
}

int
_thrp_create(void *stk, size_t stksize, void *(*func)(), void *arg,
		long flags, thread_t *new_thread, int prio, int policy,
		size_t guardsize)
{
	int tid;
	thrtab_t *tabp;
	uthread_t *first;
	uthread_t *t;
	int ret;

	TRACE_0(UTR_FAC_THREAD, UTR_THR_CREATE_START, "_thr_create start");
	/*
	TRACE_4(UTR_FAC_THREAD, UTR_THR_CREATE_START,
	    "thread_create start:func %lx, flags %lx, stk %lx stksize %d",
	    (ulong_t)(func), flags, (ulong_t)stk, stksize);
	*/

	/*
	 * This flag will leave /dev/zero file opened while allocating
	 * the stack.
	 * This is a Q&D solution, ideally this problem should be solved
	 * as part of grand scheme where single threaded program linked
	 * with libthread should not have any overheads.
	 */
	_first_thr_create = 1;

	/* check for valid parameter combinations */
	if (stk && stksize < MINSTACK)
		return (EINVAL);
	if (stksize && stksize < MINSTACK)
		return (EINVAL);
	if (prio < 0)
		return (EINVAL);

	/* alloc thread local storage */
	if (!_alloc_thread(stk, stksize, &t, guardsize))
		return (ENOMEM);
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_THC_STK,
				"in _thr_create after stack");
	t->t_policy = policy;
	t->t_pri = prio;

	t->t_hold = curthread->t_hold;
	_fpinherit(t);

	ASSERT(!_sigismember(&t->t_hold, SIGLWP));
	ASSERT(!_sigismember(&t->t_hold, SIGCANCEL));

	/* XXX: if assertions are true, why do we need following 2 calls */

	_sigdelset(&t->t_hold, SIGLWP);
	_sigdelset(&t->t_hold, SIGCANCEL);

	t->t_usropts = flags;
	t->t_startpc = (void (*)())func;
	if ((flags & (THR_BOUND | THR_SUSPENDED)) == THR_BOUND) {
		t->t_state = TS_RUN;
		t->t_stop = 0;
	} else {
		t->t_state = TS_STOPPED;
		t->t_stop = TSTP_REGULAR;
	}
	t->t_nosig = 0;
	/*
	 * t_nosig is initialized to    0 for bound threads.
	 * 				1 for unbound threads (see below).
	 * New unbound threads, before they hit _thread_start, always
	 * execute _resume_ret() (see _threadjmp() and _threadjmpsig()).
	 * _resume_ret() is also executed by threads which have _swtch()ed
	 * earlier and are now re-scheduled to resume inside _swtch().
	 * For such threads, _resume_ret() needs to call _sigon(),
	 * which decrements t_nosig to turn signals back on for such threads.
	 * So if a new thread's t_nosig is initialized to 1, by the time it
	 * hits the start_pc, it will have a 0 value in t_nosig since it
	 * would have executed _resume_ret().
	 */

	_lwp_sema_init(&t->t_park, 0);
	_thread_call(t, (void (*)())func, arg);

	_sched_lock();
	_totalthreads++;
	if (!(flags & THR_DAEMON))
		_userthreads++;
	_sched_unlock();

	/*
	 * allocate tid and add thread to list of all threads.
	 */
	_lmutex_lock(&_tidlock);
	tid = t->t_tid = ++_lasttid;
	_lmutex_unlock(&_tidlock);

	tabp = &(_allthreads[HASH_TID(tid)]);
	_lmutex_lock(&(tabp->lock));
	if (tabp->first == NULL)
		tabp->first = t->t_next = t->t_prev = t;
	else {
		first = tabp->first;
		t->t_prev = first->t_prev;
		t->t_next = first;
		first->t_prev->t_next = t;
		first->t_prev = t;
	}
	_lmutex_unlock(&(tabp->lock));


	/*
	 * store thread id *before* resuming the thread, so that references to
	 * "new_thread" made by the newly created thread get the new id.
	*/

	if (new_thread)
		*new_thread = tid;

	if ((flags & THR_BOUND)) {
		if (__td_event_report(curthread, TD_CREATE)) {
			curthread->t_td_evbuf.eventnum = TD_CREATE;
			curthread->t_td_evbuf.eventdata = (void *)tid;
			tdb_event_create();
		}
		if (ret = _new_lwp(t, (void (*)())_thread_start, 0)) {
			if ((t->t_flag & T_ALLOCSTK))
				_thread_free(t);
			return (ret);
		}
		/*
		 * Initialize t_lock to be held, since this thread is bound
		 * to the underlying lwp, and so starts life as a thread
		 * not available to run on any other lwp but the one created
		 * above. This is to match the semantics of t_lock, as also
		 * to enable the simulation of a stall for PI locks (see
		 * mutex.c where a hang is simulated by attempting to acquire
		 * the thread's t_lock, assumed to never be in the free state
		 * for ONPROC threads).
		 */
		_lwp_mutex_lock(&t->t_lock);
	} else {
		t->t_flag |= T_OFFPROC;
		t->t_nosig = 1; /* See Huge Comment above */
		_sched_lock();
		_nthreads++;
		_sched_unlock();
		ITRACE_0(UTR_FAC_TLIB_MISC, UTR_THC_CONT1,
		    "in _thr_create before cont");
		if (__td_event_report(curthread, TD_CREATE)) {
			curthread->t_td_evbuf.eventnum = TD_CREATE;
			curthread->t_td_evbuf.eventdata = (void *)tid;
			tdb_event_create();
		}
		if ((flags & THR_SUSPENDED) == 0) {
			_thr_continue(tid);
		}
		ITRACE_0(UTR_FAC_TLIB_MISC, UTR_THC_CONT2,
		    "in _thr_create after cont");
	}
	if ((flags & THR_NEW_LWP))
		_new_lwp(NULL, _age, 0);
	/*
	TRACE_5(UTR_FAC_THREAD, UTR_THR_CREATE_END,
	    "thread_create end:func %lx, flags %lx, stk %lx stksize %d tid %x",
	    (ulong_t)(func), flags, (ulong_t)stk, stksize, tid);
	*/
	TRACE_1(UTR_FAC_THREAD, UTR_THR_CREATE_END,
	    "_thr_create end:id 0x%x", tid);

	return (0);
}

/*
 * define the level of concurrency for unbound threads. the thread library
 * will allocate up to "n" lwps for running unbound threads.
 */

int
_thr_setconcurrency(int n)
{
	uthread_t *t;
	int ret;

	TRACE_1(UTR_FAC_THREAD, UTR_THR_SETCONC_START,
	    "_thr_setconcurrency start:conc = %d", n);
	if (n < 0)
		return (EINVAL);
	if (n == 0)
		return (0);
	_lmutex_lock(&_concurrencylock);
	_uconcurrency = n;
	_lmutex_unlock(&_concurrencylock);
	if (n <= _nlwps) {
		TRACE_0(UTR_FAC_THREAD, UTR_THR_SETCONC_END,
		    "_thr_setconcurrency end");
		return (0);
	}
	n -= _nlwps;
	/* add more lwps to the pool */
	while ((n--) > 0) {
		if (ret = _new_lwp(NULL, _age, 0)) {
			TRACE_0(UTR_FAC_THREAD, UTR_THR_SETCONC_END,
			    "thr_setconcurrency end");
			return (ret);
		}
	}
	TRACE_0(UTR_FAC_THREAD, UTR_THR_SETCONC_END, "thr_setconcurrency end");

	return (0);
}

/*
 * _thr_getconcurrency() reports back the size of the LWP pool.
 */
int
_thr_getconcurrency(void)
{
	TRACE_1(UTR_FAC_THREAD, UTR_THR_GETCONC_START,
	    "_thr_getconcurrency start:conc = %d", _nlwps);
	return (_nlwps);
}

/*
 * wait for a thread to exit.
 * If tid == 0, then wait for any thread.
 * If the threads library ever needs to call this function, it should call
 * _thr_join(), which should be defined just as __thr_continue() is.
 *
 * Note:
 *	a thread must never call resume() with the reaplock mutex held.
 *	this will lead to a deadlock if the thread is then resumed due
 *	to another thread exiting. The zombie thread is placed onto
 *	deathrow by the new thread, which happens to hold the reaplock.
 *
 *
 */
int
_thr_join(thread_t tid, thread_t *departed, void **status)
{
	int ret;

	ret = _thrp_join(tid, departed, status);
	if (ret == EINVAL)
		/* Solaris expects ESRCH for detached threads also */
		return (ESRCH);
	else
		return (ret);
}

int
_thrp_join(thread_t tid, thread_t *departed, void **status)
{
	uthread_t *t;
	int v, ix, xtid;

	TRACE_1(UTR_FAC_THREAD, UTR_THR_JOIN_START,
			"_thr_join start:tid %x", (ulong_t)tid);
	if (tid == NULL) {
		_reap_lock();
		while (_userthreads > 1 || _u2bzombies || _zombiecnt ||
		    _d2bzombies) {
			while (!_zombies)
				_reap_wait_cancel(&_zombied);
			if (_zombiecnt == 1) {
				t = _zombies;
				_zombies = NULL;
			} else {
				t = _zombies->t_ibackw;
				t->t_ibackw->t_iforw = _zombies;
				_zombies->t_ibackw = t->t_ibackw;
			}

			/*
			 * set the state to TS_REAPED, so another thr_join()
			 * can notice it
			 */
			t->t_state = TS_REAPED;
			_zombiecnt--;
			if (t->t_flag & T_INTERNAL)
				continue;
			/*
			 * XXX: For now, leave the following out. It seems that
			 * DAEMON threads should be reclaimable. Of course,
			 * T_INTERNAL threads should not be, as the above
			 * ensures.
			 */
			/*
			if (t->t_usropts & THR_DAEMON)
				continue;
			*/
			xtid = t->t_tid;
			_reap_unlock();
			_lock_bucket((ix = HASH_TID(xtid)));
			if (t == THREAD(xtid))
				goto found;
			_unlock_bucket(ix);
			_reap_lock();
		}
		_reap_unlock();
		ASSERT(_userthreads == 1 && !_u2bzombies && !_zombiecnt &&
		    !_d2bzombies);
		return (EDEADLK);
	} else if (tid == curthread->t_tid) {
		return (EDEADLK);

	} else {
		_lock_bucket((ix = HASH_TID(tid)));
		if ((t = THREAD(tid)) == (uthread_t *)-1 ||
		    t->t_flag & T_INTERNAL) {
			_unlock_bucket(ix);
			return (ESRCH);
		}
		if (DETACHED(t)) {
			TRACE_1(UTR_FAC_THREAD, UTR_THR_JOIN_END,
			    "_thr_join end:tid %x", NULL);
			_unlock_bucket(ix);
			return (EINVAL);
		}
		_reap_lock();
		while (!(t->t_flag & T_ZOMBIE)) {
			_unlock_bucket(ix);
			_reap_wait_cancel(&_zombied);
			_reap_unlock();
			_lock_bucket(ix);
			if ((t = THREAD(tid)) == (uthread_t *)-1) {
				_unlock_bucket(ix);
				return (ESRCH);
			}
			if (DETACHED(t)) {
				_unlock_bucket(ix);
				return (EINVAL);
			}
			_reap_lock();
		}
		if (t->t_state == TS_REAPED) {
			/*
			 * reaped by a call to thr_join(0, ...)
			 * That is the only possible case. This is because of
			 * the order in which the thread's bucket lock is
			 * acquired with respect to the reap lock. Since the
			 * reap lock is the lowest level lock,
			 * thr_join(tid, ...) roughly does the following:
			 * 	lock_bucket_lock();
			 * 		lock_reap_lock();	---> C
			 * 		get thread...
			 * 		unlock_reap_lock();
			 * 	destroy thread...		-----> D
			 * 	unlock_bucket_lock();
			 * However, thr_join(0, ...) has to acquire the reap
			 * lock to inspect death row, When it finds a thread,
			 * it has to release reap lock and then get the
			 * thread's bucket lock (otherwise
			 * it would violate the lock hierarchy. So its
			 * actions look like:
			 *	lock_reap_lock();
			 * 	get thread...
			 * 	unlock_reap_lock();	---> A
			 * 	lock_bucket_lock();	---> B
			 *	destroy thread...	---> E
			 * 	unlock_bucket_lock();
			 *
			 * In the window between A and B above, if C above
			 * is allowed to make progress, the same thread will
			 * be attempted to be destroyed twice - once at D and
			 * then at E. That is why, one needs to mark the
			 * thread as reaped, before releasing the reap lock
			 * at A above. If the thread had been reaped by
			 * a previous call to thr_join(tid, ...), it would
			 * already have been destroyed before this thread could
			 * get the thread's bucket lock - so we cannot be here
			 * because the thread had been reaped by
			 * thr_join(tid, ...).
			 */
			_reap_unlock();
			_unlock_bucket(ix);
			return (ESRCH);
		} else {
			ASSERT(_zombiecnt >= 1);
			if (_zombiecnt == 1) {
				ASSERT(t == _zombies);
				_zombies = NULL;
			} else {
				t->t_ibackw->t_iforw = t->t_iforw;
				t->t_iforw->t_ibackw = t->t_ibackw;
				if (t == _zombies)
					_zombies = t->t_iforw;
			}
		    _zombiecnt--;
		    t->t_state = TS_REAPED;
		    _reap_unlock();
		}
	}
found:
	if (departed != NULL)
		*departed = t->t_tid;
	if (status != NULL)
		*status = t->t_exitstat;
	_thread_destroy(t, ix);
	_unlock_bucket(ix);
	TRACE_1(UTR_FAC_THREAD, UTR_THR_JOIN_END, "_thr_join end:tid %x",
	    (ulong_t)tid);
	return (0);
}

/*
 * POSIX function pthread_detach(). It is being called _thr_detach()
 * just to be compatible with rest of the functionality though there
 * is no thr_detach() API in Solaris.
 */

int
_thr_detach(thread_t tid)
{
	uthread_t *t;
	int ix;

	TRACE_1(UTR_FAC_THREAD, UTR_THR_DETACH_START,
	    "_thr_detach start:tid 0x%x", (ulong_t)tid);
	if (tid == (thread_t)0)
		return (ESRCH);

	_lock_bucket((ix = HASH_TID(tid)));
	if ((t = THREAD(tid)) == (uthread_t *)-1 ||
	    t->t_flag & T_INTERNAL) {
		_unlock_bucket(ix);
		return (ESRCH);
	}
	if (DETACHED(t)) {
		TRACE_1(UTR_FAC_THREAD, UTR_THR_DETACH_END,
		    "_thr_detach end:tid %x", NULL);
		_unlock_bucket(ix);
		return (EINVAL);
	}
	/*
	 * The target thread might have exited, but it could have
	 * made to the zombie list or not. If it does, then
	 * we will have to clean the thread as it is done in thr_join.
	 */
	_reap_lock();
	if (t->t_flag & T_ZOMBIE) {
		if (_zombiecnt == 1) {
			ASSERT(t == _zombies);
			_zombies = NULL;
		} else {
			t->t_ibackw->t_iforw = t->t_iforw;
			t->t_iforw->t_ibackw = t->t_ibackw;
			if (t == _zombies)
				_zombies = t->t_iforw;
		}
		_zombiecnt--;
		_reap_unlock();
		_thread_destroy(t, ix);
	} else {
		/*
		 * If it did not make it, then mark it THR_DETACHED so that
		 * when it dies _reapq_add will put it in deathrow queue to
		 * be reaped by reaper later.
		 */
		t->t_usropts |= THR_DETACHED;

		/*
		 * This thread might have just exited and is on the way
		 * to be added to deathrow (we have made it detached).
		 * Before we made it detached, thr_exit may have bumped up
		 * the d2/u2 counts since it was the non-detached thread.
		 * Undo it here, so that it looks as if the thread was
		 * "detached" when it was exited.
		 */
		if (t->t_flag & T_2BZOMBIE) {
			if (t->t_usropts & THR_DAEMON)
				_d2bzombies--;
			else
				_u2bzombies--;
		}

		/*
		 * Wake up threads trying to join target thread.
		 * This thread will not be available for joining.
		 * thr_join will check (after wake up) whether
		 * this thread is valid and non-detached.
		 */
		_cond_broadcast(&_zombied);
		_reap_unlock();
	}
	_unlock_bucket(ix);
	return (0);
}

thread_t
_thr_self(void)
{
	thread_t tid;

	TRACE_0(UTR_FAC_THREAD, UTR_THR_SELF_START, "_thr_self start");
	tid = curthread->t_tid;
	TRACE_0(UTR_FAC_THREAD, UTR_THR_SELF_END, "_thr_self end");
	return (tid);
}

/*
 * True if "new" unmasks any signal in "old" which is also blocked in "l"
 */
#define	unblocking(new, old, l) (\
	(~((new)->__sigbits[0]) & ((old)->__sigbits[0])\
	& ((l)->__sigbits[0])) || (~((new)->__sigbits[1])\
	& ((old)->__sigbits[1]) & ((l)->__sigbits[1])))

sigset_t __lwpdirsigs = {sigmask(SIGVTALRM)|sigmask(SIGPROF), 0, 0, 0};

int
_thr_sigsetmask(int how, const sigset_t *set, sigset_t *oset)
{
	sigset_t *t_set;
	sigset_t ot_hold;
	sigset_t pending;
	sigset_t ppending;
	sigset_t sigs_ub;
	sigset_t resend;
	uthread_t *t = curthread;
	int sig;

	TRACE_2(UTR_FAC_THREAD, UTR_THR_SIGSETMASK_START,
	    "_thr_sigsetmask start:how %d, set 0x%x", how,
	    set != NULL ? set->__sigbits[0] : 0);
	if (((how < SIG_BLOCK) || (how > SIG_SETMASK)) && (set != NULL))
		return (EINVAL);
	ot_hold = t->t_hold;
	t_set = &t->t_hold;
	if (set != NULL) {
		_sched_lock();
		ASSERT(t->t_flag & T_SIGWAIT || ISTEMPBOUND(t) ||
		    !sigand(&t->t_ssig, t_set));
		/*
		 * If the flag T_TEMPBOUND is set, it is possible to have the
		 * mask block signals in t_ssig. Hence the above ASSERT
		 * should include this case.
		 */
		/*
		 * If this thread has been sent signals which are currently
		 * unblocked but are about to be blocked, deliver them here
		 * before blocking them.
		 * This is necessary for 2 reasons :
		 * a) Correct reception of signals unblocked at the time of a
		 *    _thr_kill() by the application(see _thr_exit()
		 *	which calls  _thr_sigsetmask() to block and thus
		 *	flush all such signals)
		 *    Basically, if _thr_kill() returns success, the
		 *    signal must be delivered to the target thread.
		 * b) Guarantee correct reception of a bounced signal.
		 *    The scenario:
		 *    Assume two threads, t1 and t2 in this process. t1 blocks
		 *    SIGFOO but t2 does not. t1's lwp might receive SIGFOO sent
		 *    to the process. t1 bounces the signal to t2 via a
		 *    _thr_kill(). If t2 now blocks SIGFOO using
		 *    _thr_sigsetmask()
		 *    This mask could percolate down to its lwp, resulting in
		 *    SIGFOO pending on t2's lwp, if received after this event.
		 *    If t2 never unblocks SIGFOO, an asynchronously generated
		 *    signal sent to the process is  thus lost.
		 */
		if (how == SIG_BLOCK || how == SIG_SETMASK) {
			while (!sigisempty(&t->t_ssig) &&
			    _blocksent(&t->t_ssig, t_set, (sigset_t *)set)) {
				_sched_unlock();
				_deliversigs(set);
				_sched_lock();
			}
			while ((t->t_flag & T_BSSIG) &&
			    _blocking(&t->t_bsig, t_set, (sigset_t *)set,
				&resend)) {
				_sigemptyset(&t->t_bsig);
				t->t_bdirpend = 0;
				t->t_flag &= ~T_BSSIG;
				_sched_unlock();
				_bsigs(&resend);
				_sched_lock();
			}
		}
		switch (how) {
		case SIG_BLOCK:
			sigorset(t_set, set);
			sigdiffset(t_set, &_cantmask);
			_sched_unlock_nosig();
			break;
		case SIG_UNBLOCK:
			sigdiffset(t_set, set);
			break;
		case SIG_SETMASK:
			*t_set = *set;
			sigdiffset(t_set, &_cantmask);
			break;
		default:
			break;
		}
		if (how == SIG_UNBLOCK || how == SIG_SETMASK) {
			_sigmaskset(&t->t_psig, t_set, &pending);
			/*
			 * t->t_pending should only be set. it is
			 * possible for a signal to arrive after
			 * we've checked for pending signals. clearing
			 * t->t_pending should only be done when
			 * signals are really disabled like in
			 * _resetsig() or sigacthandler().
			 */
			if (!sigisempty(&pending))
				t->t_pending = 1;
			_sigemptyset(&ppending);
			if (!(t->t_usropts & THR_DAEMON))
				_sigmaskset(&_pmask, t_set, &ppending);
			_sched_unlock_nosig();
			if (!sigisempty(&ppending)) {
				_sigemptyset(&sigs_ub);
				_lwp_mutex_lock(&_pmasklock);
				_sigunblock(&_pmask, t_set, &sigs_ub);
				_lwp_mutex_unlock(&_pmasklock);
				if (!sigisempty(&sigs_ub)) {
					_sched_lock_nosig();
					sigorset(&t->t_bsig, &sigs_ub);
					t->t_bdirpend = 1;
					_sched_unlock_nosig();
				}
			}
		}
		/*
		 * Is this a  bound thread that uses calls resulting in LWP
		 * directed signals (indicated by the T_LWPDIRSIGS flag)? If so,
		 * is it changing such signals in its mask? If so, push down the
		 * mask, so these LWP directed signals are delivered in keeping
		 * with the state of the the thread's mask.
		 */
		if (((t->t_flag & T_LWPDIRSIGS) && ISBOUND(t) &&
		    (changesigs(&ot_hold, t_set, &__lwpdirsigs))))
			__sigprocmask(SIG_SETMASK, t_set, NULL);
		_sigon();
	}
	if (oset != NULL)
		*oset = ot_hold;
	ASSERT(!_sigismember(t_set, SIGLWP));
	ASSERT(!_sigismember(t_set, SIGCANCEL));
	TRACE_0(UTR_FAC_THREAD, UTR_THR_SIGSETMASK_END,
	    "_thr_sigsetmask end");
	return (0);
}

/*
 * The only difference between the above routine and the following one is
 * that the following routine does not delete the _cantmask set from the
 * signals masked on the thread as a result of the call. So this routine is
 * usable only internally by libthread.
 */
int
__thr_sigsetmask(int how, const sigset_t *set, sigset_t *oset)
{
	sigset_t *t_set;
	sigset_t ot_hold;
	sigset_t pending;
	sigset_t ppending;
	sigset_t sigs_ub;
	sigset_t resend;
	uthread_t *t = curthread;
	int sig;

	TRACE_2(UTR_FAC_THREAD, UTR_THR_SIGSETMASK_START,
	    "__thr_sigsetmask start:how %d, set 0x%x", how,
	    set != NULL ? set->__sigbits[0] : 0);
	if (((how < SIG_BLOCK) || (how > SIG_SETMASK)) && (set != NULL))
		return (EINVAL);
	ot_hold = t->t_hold;
	t_set = &t->t_hold;
	if (set != NULL) {
		_sched_lock();
		ASSERT(t->t_flag & T_SIGWAIT || ISTEMPBOUND(t) ||
		    !sigand(&t->t_ssig, t_set));
		/*
		 * If the flag T_TEMPBOUND is set, it is possible to have the
		 * mask block signals in t_ssig. Hence the above ASSERT
		 * should include this case.
		 */
		/*
		 * If this thread has been sent signals which are currently
		 * unblocked but are about to be blocked, deliver them here
		 * before blocking them.
		 * This is necessary for 2 reasons :
		 * a) Correct reception of signals unblocked at the time of a
		 *    _thr_kill() by the application(see _thr_exit()
		 *	which calls  _thr_sigsetmask() to block and thus
		 *	flush all such signals)
		 *    Basically, if _thr_kill() returns success, the
		 *    signal must be delivered to the target thread.
		 * b) Guarantee correct reception of a bounced signal.
		 *    The scenario:
		 *    Assume two threads, t1 and t2 in this process. t1 blocks
		 *    SIGFOO but t2 does not. t1's lwp might receive SIGFOO sent
		 *    to the process. t1 bounces the signal to t2 via a
		 *    _thr_kill(). If t2 now blocks SIGFOO using
		 *    _thr_sigsetmask()
		 *    This mask could percolate down to its lwp, resulting in
		 *    SIGFOO pending on t2's lwp, if received after this event.
		 *    If t2 never unblocks SIGFOO, an asynchronously generated
		 *    signal sent to the process is  thus lost.
		 */
		if (how == SIG_BLOCK || how == SIG_SETMASK) {
			while (!sigisempty(&t->t_ssig) &&
			    _blocksent(&t->t_ssig, t_set, (sigset_t *)set)) {
				_sched_unlock();
				_deliversigs(set);
				_sched_lock();
			}
			while ((t->t_flag & T_BSSIG) &&
			    _blocking(&t->t_bsig, t_set, (sigset_t *)set,
				&resend)) {
				_sigemptyset(&t->t_bsig);
				t->t_bdirpend = 0;
				t->t_flag &= ~T_BSSIG;
				_sched_unlock();
				_bsigs(&resend);
				_sched_lock();
			}
		}
		switch (how) {
		case SIG_BLOCK:
			sigorset(t_set, set);
			_sched_unlock_nosig();
			break;
		case SIG_UNBLOCK:
			sigdiffset(t_set, set);
			break;
		case SIG_SETMASK:
			*t_set = *set;
			break;
		default:
			break;
		}
		if (how == SIG_UNBLOCK || how == SIG_SETMASK) {
			_sigmaskset(&t->t_psig, t_set, &pending);
			/*
			 * t->t_pending should only be set. it is
			 * possible for a signal to arrive after
			 * we've checked for pending signals. clearing
			 * t->t_pending should only be done when
			 * signals are really disabled like in
			 * _resetsig() or sigacthandler().
			 */
			if (!sigisempty(&pending))
				t->t_pending = 1;
			_sigemptyset(&ppending);
			if (!(t->t_usropts & THR_DAEMON))
				_sigmaskset(&_pmask, t_set, &ppending);
			_sched_unlock_nosig();
			if (!sigisempty(&ppending)) {
				_sigemptyset(&sigs_ub);
				_lwp_mutex_lock(&_pmasklock);
				_sigunblock(&_pmask, t_set, &sigs_ub);
				_lwp_mutex_unlock(&_pmasklock);
				if (!sigisempty(&sigs_ub)) {
					_sched_lock_nosig();
					sigorset(&t->t_bsig, &sigs_ub);
					t->t_bdirpend = 1;
					_sched_unlock_nosig();
				}
			}
		}
		/*
		 * Is this a  bound thread that uses calls resulting in LWP
		 * directed signals (indicated by the T_LWPDIRSIGS flag)? If so,
		 * is it changing such signals in its mask? If so, push down the
		 * mask, so these LWP directed signals are delivered in keeping
		 * with the state of the the thread's mask.
		 */
		if (((t->t_flag & T_LWPDIRSIGS) && ISBOUND(t) &&
		    (changesigs(&ot_hold, t_set, &__lwpdirsigs))))
			__sigprocmask(SIG_SETMASK, t_set, NULL);
		_sigon();
	}
	if (oset != NULL)
		*oset = ot_hold;
	TRACE_0(UTR_FAC_THREAD, UTR_THR_SIGSETMASK_END,
	    "__thr_sigsetmask end");
	return (0);
}

int
_thr_kill(thread_t tid, int sig)
{
	uthread_t *t;
	int ix;
	int rc = 0;

	TRACE_2(UTR_FAC_THREAD, UTR_THR_KILL_START,
	    "_thr_kill start:tid 0x%x, sig %d", (ulong_t)tid, sig);
	if (sig >= NSIG || sig < 0 || sig == SIGWAITING ||
	    sig == SIGCANCEL || sig == SIGLWP) {
		return (EINVAL);
	}
	if (tid == (thread_t)0)
		return (ESRCH);
	_lock_bucket((ix = HASH_TID(tid)));
	if ((t = THREAD(tid)) == (uthread_t *)-1 ||
	    t->t_flag & T_INTERNAL) {
		_unlock_bucket(ix);
		return (ESRCH);
	}
	rc = _thrp_kill(t, ix, sig);
	return (rc);
}

/*
 * XXX- Not used
 * The following functions should be used internally by the threads library,
 * i.e. instead of _thr_kill().
 */
int
__thr_kill(thread_t tid, int sig)
{
	uthread_t *t;
	int ix;

	TRACE_2(UTR_FAC_THREAD, UTR_THR_KILL_START,
	    "_thr_kill start:tid 0x%x, sig %d", (ulong_t)tid, sig);

	if (sig >= NSIG || sig < 0 || sig == SIGWAITING) {
		return (EINVAL);
	}
	if (tid == (thread_t)0)
		return (ESRCH);
	_lock_bucket((ix = HASH_TID(tid)));
	if ((t = THREAD(tid)) == (uthread_t *)-1) {
		_unlock_bucket(ix);
		return (ESRCH);
	}
	return (_thrp_kill(t, ix, sig));
}

static int
_thrp_kill(uthread_t *t, int ix, int sig)
{
	int rc = 0;
	lwpid_t lwpid;

	ASSERT(MUTEX_HELD(&(_allthreads[ix].lock)));

	if (sig == 0) {
		_sched_lock();
		if (t->t_state == TS_ZOMB)
			rc = ESRCH;
		_sched_unlock();
		_unlock_bucket(ix);
		return (rc);
	}
	_sched_lock();
	rc = _thrp_kill_unlocked(t, ix, sig, &lwpid);
	if (rc == 0 && lwpid != NULL) {
		/*
		 * XXX: If _lwp_kill() is called with _schedlock held, we *may*
		 * be able to do away with calling _thrp_kill_unlocked() with
		 * the lwpid pointer.
		 */
		rc = _lwp_kill(lwpid, sig);
		_sched_unlock();
		_unlock_bucket(ix);
	} else {
		_sched_unlock();
		_unlock_bucket(ix);
	}
	/*
	 * _thrp_kill_unlocked() called with _schedlock and
	 * _allthreads[ix].lock held.
	 * This is done because this function needs to be called from
	 * _sigredirect() with the 2 locks held.
	 */
	return (rc);
}

int
_thrp_kill_unlocked(uthread_t *t, int ix, int sig, lwpid_t *lwpidp)
{
	int rc;

	ASSERT(MUTEX_HELD(&(_allthreads[ix].lock)));
	ASSERT(MUTEX_HELD(&_schedlock));

	*lwpidp = 0;

	if (t->t_state == TS_ZOMB) {
		TRACE_0(UTR_FAC_THREAD, UTR_THR_KILL_END,
		    "_thr_kill end:zombie");
		return (ESRCH);
	} else {
		if (_sigismember(&t->t_psig, sig)) {
			TRACE_0(UTR_FAC_THREAD, UTR_THR_KILL_END,
			    "_thr_kill end:signal collapsed");
			return (0);
		}
		if (ISIGNORED(sig) && !_sigismember(&t->t_hold, sig)) {
			return (0);
		}
		_sigaddset(&t->t_psig, sig);
		t->t_pending = 1;
		if (_sigismember(&t->t_hold, sig)) {
			TRACE_0(UTR_FAC_THREAD, UTR_THR_KILL_END,
			    "_thr_kill end:signal masked");
			return (0);
		}
		if ((t != curthread) && (ISBOUND(t) ||
		    (ONPROCQ(t) && t->t_state == TS_ONPROC))) {
			if (ISBOUND(t) && t->t_state == TS_SLEEP) {
				t->t_flag |= T_INTR;
				_unsleep(t);
				_setrq(t);
				if (t->t_flag & T_SIGWAIT)
				/*
				 * At this point, the target thread is
				 * revealed to be a bound thread, in sigwait(),
				 * with the signal unblocked (the above check
				 * against t_hold failed - that is why we are
				 * here). Now, sending it a signal via
				 * _lwp_kill() is a bug. So, just return. The
				 * target thread will wake-up and do the right
				 * thing in sigwait().
				 */
					return (0);
			}
			/*
			 * The following is so the !sigand(t_ssig, t_hold)
			 * assertion in _thr_sigsetmask() stays true for
			 * threads in sigwait.
			 */
			if ((t->t_flag & T_SIGWAIT) == 0) {
				_sigaddset(&t->t_ssig, sig);
			}
			ASSERT(LWPID(t) != 0);
			*lwpidp = LWPID(t);
			return (0);
		} else if ((t->t_state == TS_SLEEP)) {
			t->t_flag |= T_INTR;
			_unsleep(t);
			_setrq(t);
		}
	}
	TRACE_0(UTR_FAC_THREAD, UTR_THR_KILL_END, "_thr_kill end");
	return (0);
}

/*
 * stop the specified thread.
 * Define a __thr_suspend() routine, similar to __thr_continue(),
 * if the threads library needs to call _thr_continue() internally.
 */

int
_thr_suspend(thread_t tid)
{

	TRACE_1(UTR_FAC_THREAD, UTR_THR_SUSPEND_START,
	    "_thr_suspend start:tid 0x%x", tid);

	if (tid == (thread_t)0)
		return (ESRCH);
	return (_thrp_suspend(tid, TSTP_REGULAR));
}

/*
 * External-stop the specified thread (e.g., stop it from a debugger).
 *
 * A thread's external-stop state is orthogonal to its regular state;
 * if it is stopped externally, it will not run, regardless of its
 * regular state.
 */

int
_thr_dbsuspend(thread_t tid)
{

	TRACE_1(UTR_FAC_THREAD, UTR_THR_SUSPEND_START,
	    "_thr_suspend start:tid 0x%x", tid);

	if (tid == (thread_t)0)
		return (ESRCH);
	return (_thrp_suspend(tid, TSTP_EXTERNAL));
}

static int
_thrp_suspend(thread_t tid, int whystopped)
{
	uthread_t *t;
	int rc = 0;
	int ix;
	int sysnum;
	int stopped;

	_lock_bucket((ix = HASH_TID(tid)));
	if ((t = _idtot(tid)) == (uthread_t *)-1) {
		_unlock_bucket(ix);
		return (ESRCH);
	}
	if ((t->t_flag & T_INTERNAL) && whystopped != TSTP_INTERNAL) {
		_unlock_bucket(ix);
		return (ESRCH);
	}
	if (whystopped & TSTP_MUTATOR) {
		if (!t->t_mutator) {
			_unlock_bucket(ix);
			return (EINVAL);
		}
	}
	_sched_lock();
	if ((t->t_stop & whystopped) == whystopped) {
		_sched_unlock();
		_unlock_bucket(ix);
		TRACE_0(UTR_FAC_THREAD, UTR_THR_SUSPEND_END,
						"_thr_suspend end");
		return (0);
	}
	if (t->t_state == TS_ZOMB) {
		_sched_unlock();
		_unlock_bucket(ix);
		TRACE_0(UTR_FAC_THREAD, UTR_THR_SUSPEND_END,
						"_thr_suspend end");
		return (ESRCH);
	}
	stopped = t->t_stop;
	t->t_stop |= whystopped;
	if (t == curthread) {
		_suspendself(t);
		_sched_unlock_nosig();
		_unlock_bucket(ix);
		_swtch(0);
		_sigon();
		TRACE_0(UTR_FAC_THREAD, UTR_THR_SUSPEND_END,
		    "_thr_suspend end");
		return (0);
	} else {
		if (t->t_state == TS_ONPROC && !stopped) {
			while (rc = _lwp_suspend2(LWPID(t), &sysnum)) {
				if (rc == EINTR)
					continue;
				/* suspend failed, return error. */
				_sched_unlock();
				_unlock_bucket(ix);
				TRACE_0(UTR_FAC_THREAD, UTR_THR_SUSPEND_END,
					"_thr_suspend end");
				return (rc);
			}
			/*
			 * _lwp_suspend2() has suspended the target
			 * thread. if the thread was suspended during
			 * a system call, sysnum is set to the system
			 * call being suspended; otherwise, sysnum
			 * is clear, and the thread was suspended executing
			 * user code. If the suspended thread is masking
			 * thread suspension, the thread should always
			 * be resumed and the caller should wait for the
			 * target thread to suspend itself. The assumption
			 * being that the target thread is in a critical
			 * section, and will notice the pending suspension
			 * when it leaves this critical section. Only
			 * if the thread is executing user code, and not
			 * masking thread suspension, will the target
			 * thread be notified to suspend itself via a
			 * SIGLWP signal.
			 */
			if (sysnum && t->t_nosig == 0) {
				t->t_flag |= T_LWPSUSPENDED;
				if (!ISBOUND(t))
					_onproc_deq(t);
			} else if (whystopped != TSTP_EXTERNAL) {
				/*
				 * send SIGLWP to thread if thread
				 * suspension is not masked. signal is
				 * delivered immediately after thread
				 * is continued via _lwp_continue().
				 * caller should wait until thread
				 * suspends itself.
				 */
				if (t->t_nosig == 0)
					_lwp_kill(LWPID(t), SIGLWP);
				_lwp_continue(LWPID(t));
				for (;;) {
					if (t->t_stop &&
					    t->t_state == TS_ONPROC) {
						_lwp_cond_wait(&t->t_suspndcv,
						    &_schedlock);
						_sched_owner = curthread;
					} else
						break;
					/*
					 * check if target thread died
					 * while we waited.
					 */
					if (_idtot(tid) == (uthread_t *)-1 ||
					    t->t_state == TS_ZOMB) {
						rc = ESRCH;
						break;
					}
				}
			}
		}
	}
	_sched_unlock();
	_unlock_bucket(ix);
	TRACE_0(UTR_FAC_THREAD, UTR_THR_SUSPEND_END, "_thr_suspend end");
	return (rc);
}

/*
 * make the specified thread runnable
 */
int
_thr_continue(thread_t tid)
{
	uthread_t *t;
	int ix;

	TRACE_1(UTR_FAC_THREAD, UTR_THR_CONTINUE_START,
	    "_thr_continue start:tid 0x%x", tid);
	if (tid == (thread_t)0)
		return (ESRCH);
	return (_thrp_continue(tid, TSTP_REGULAR));
}

/*
 * Call the following function (instead of _thr_continue) inside
 * the threads library.
 */
int
__thr_continue(thread_t tid)
{
	uthread_t *t;
	int ix;

	TRACE_1(UTR_FAC_THREAD, UTR_THR_CONTINUE_START,
	    "_thr_continue start:tid 0x%x", tid);
	return (_thrp_continue(tid, TSTP_INTERNAL));
}

/*
 * Turn off the external-stop state of this thread (i.e., the debugger
 * has released it).
 */
int
_thr_dbcontinue(thread_t tid)
{
	uthread_t *t;
	int ix;

	TRACE_1(UTR_FAC_THREAD, UTR_THR_CONTINUE_START,
	    "_thr_dbcontinue start:tid 0x%x", tid);
	return (_thrp_continue(tid, TSTP_EXTERNAL));
}

static int
_thrp_continue(thread_t tid, int whystopped)
{
	uthread_t *t;
	int ix;
	int rc = 0;
	int error;
	char old_stop;

	ASSERT(whystopped != 0);
	_lock_bucket((ix = HASH_TID(tid)));
	if ((t = THREAD(tid)) == (uthread_t *)-1) {
		_unlock_bucket(ix);
		return (ESRCH);
	}
	if (whystopped != TSTP_INTERNAL) {
		if (whystopped == TSTP_MUTATOR) {
			if (!t->t_mutator) {
				_unlock_bucket(ix);
				return (EINVAL);
			}
		}
		if (t->t_flag & T_INTERNAL) {
			_unlock_bucket(ix);
			return (ESRCH);
		}
		_sched_lock();
		if (t->t_state == TS_ZOMB) {	/* Thread is dying */
			if (DETACHED(t)) {
				/*
				 * return ESRCH. thread terminated and its
				 * state will be reclaimed by the reaper.
				 */
				error = ESRCH;
			} else {
				/*
				 * return EINVAL. thread terminated but
				 * its state should be reclaimed by thr_join().
				 */
				error = EINVAL;
			}
			_sched_unlock();
			_unlock_bucket(ix);
			return (error);
		}
	} else
		_sched_lock();

	old_stop = t->t_stop;
	t->t_stop &= ~whystopped;
	if ((old_stop != 0) && ((old_stop & whystopped) != whystopped)) {
		/*
		 * t wasn't stopped for the reason consistent to whystopped,
		 * return immediately.
		 */
		_sched_unlock();
		_unlock_bucket(ix);
		TRACE_0(UTR_FAC_THREAD, UTR_THR_CONTINUE_END,
		    "_thr_continue end");
		return (0);
	}
	if (whystopped == TSTP_MUTATOR && _suspendedallmutators) {
		/*
		 * t->t_stop could be either zero or non-zero, when it got
		 * suspended by _thr_suspend_allmutators(). t comes here only
		 * when t was suspended by _thr_suspend_allmutators(), and
		 * _thr_continue_mutator() was called on t.
		 */
		ASSERT(t->t_mutator != 0);
		t->t_mutatormask = 1;
		if (t->t_state == TS_RUN) {
			ASSERT(!ISBOUND(t));
			/*
			 * remove mutator from suspended runq, and
			 * put it on the runq if it's not suspended.
			 */
			if (_srqdeq(t) == 0) {
				ASSERT(t->t_stop == 0);
				_setrq(t);
			}
		}
	}
	if (old_stop == 0) {
		/*
		 * t comes here only when _thrp_continue() got called
		 * falsely on t. return immediately.
		 */
		ASSERT(t->t_state != TS_STOPPED && t->t_state != TS_ONPROC);
		_sched_unlock();
		_unlock_bucket(ix);
		TRACE_0(UTR_FAC_THREAD, UTR_THR_CONTINUE_END,
		    "_thr_continue end");
		return (0);
	}
	/*
	 * t comes here only when _thrp_continue() got called with
	 * whystopped consistent to the reason which t suspends on,
	 * and it's not the _thr_continue_mutator(t) case.
	 */
	ASSERT(t->t_stop == 0);

	if (t->t_state == TS_STOPPED) {
		if (ISBOUND(t) && !ISPARKED(t)) {
			/*
			 * This condition can occur if the target thread is a
			 * bound thread which was created suspended such as the
			 * callout daemon thread. A subsequent thr_continue()
			 * would end up here.
			 *
			 * It may also occur if the thread is on its way to
			 * being parked from a call to sema_wait(), for example,
			 * when it got stopped in between. This is due to the
			 * window between going to sleep in _t_block() and
			 * actually parking inside _swtch() - the _schedlock is
			 * briefly released in this window. In the latter
			 * case, the call to _lwp_continue() is not necessary
			 * but is still done, since distinguishing between
			 * these two scenarios would require a new flag - this
			 * does not seem worth it.
			 */
			t->t_state = TS_ONPROC;
			rc = _lwp_continue(LWPID(t));
		} else {
			_setrq(t);
		}
	} else if (t->t_state == TS_ONPROC) {
		if (_suspendedallmutators) {
			rc = _lwp_continue(LWPID(t));
		} else if (t->t_flag & T_LWPSUSPENDED) {
			t->t_flag &= ~T_LWPSUSPENDED;
			rc = _lwp_continue(LWPID(t));
		} else {
			/*
			 * It's possible that the thread was continued
			 * before it suspended, and somebody could be
			 * waiting for this thread to be suspended. Since
			 * the suspension never happened, a broadcast
			 * should be sent to anybody waiting for this
			 * thread to be suspended. The threads trying
			 * to suspend this thread awaken as if the thread
			 * was suspended when in fact its not. This
			 * is a programming bug that is not detectible.
			 */
			_lwp_cond_broadcast(&t->t_suspndcv);
		}
		/*
		 * The target thread could be in the window where the
		 * thr_suspend() on the target thread has occurred, and
		 * it has been taken off the onproc queue, and its underlying
		 * LWP has been sent a SIGLWP, but the thread has not yet
		 * received the signal. In this window, the thread has yet
		 * to change its state to TS_STOPPED, since that happens
		 * in the signal handler. So, all that we need to do here
		 * is put it back on the onproc queue, and rely on the
		 * _siglwp() handler which will eventually be invoked on the
		 * target thread, to notice that the t_stop bit has been tuned
		 * off and not do the stopping in _dopreempt().
		 */
		if (!ISBOUND(t) && OFFPROCQ(t))
			_onproc_enq(t);
	}
	_sched_unlock();
	_unlock_bucket(ix);
	TRACE_0(UTR_FAC_THREAD, UTR_THR_CONTINUE_END, "_thr_continue end");
	return (rc);
}

/*
 * Define a __thr_setprio() function and call it internally (instead of
 * _thr_setprio()), similar to __thr_continue().
 */

int
_thr_setprio(thread_t tid, int newpri)
{
	uthread_t *t;
	int ix;

	TRACE_2(UTR_FAC_THREAD, UTR_THR_SETPRIO_START,
	    "_thr_setprio start:tid 0x%x, newpri %d", tid, newpri);
	if (tid == (thread_t)0)
		return (ESRCH);
	_lock_bucket((ix = HASH_TID(tid)));
	if ((t = THREAD(tid)) == (uthread_t *)-1 ||
	    t->t_flag & T_INTERNAL) {
		_unlock_bucket(ix);
		return (ESRCH);
	}
	return (_thrp_setprio(t, newpri, SCHED_OTHER, 0, ix));
}

/*
 * Set the thread's effective or assigned priority, depending on the
 * inherit flag. If set, only the effective priority is changed...otherwise
 * the assigned priority is changed. Used mainly for the implementation of
 * ceiling locks, via the call from _thread_setschedparam_main().
 */
int
_thrp_setprio(uthread_t *t, int newpri, int policy, int inherit, int ix)
{
	int oldpri, qx;
	int *prip;

	if (newpri < THREAD_MIN_PRIORITY || newpri > THREAD_MAX_PRIORITY) {
		_unlock_bucket(ix);
		return (EINVAL);
	}
	t->t_policy = policy;
	if (inherit)
		prip = &t->t_epri;
	else
		prip = &t->t_pri;
	if (newpri == *prip) {
		_unlock_bucket(ix);
		return (0);
	}
	if (ISBOUND(t)) {
		*prip = newpri;
		_unlock_bucket(ix);
		TRACE_0(UTR_FAC_THREAD, UTR_THR_SETPRIO_END,
						"_thr_setprio end");
		return (0);
	}
	_sched_lock();
	if (t->t_state == TS_ONPROC || t->t_state == TS_DISP) {
		oldpri = *prip;
		*prip = newpri;
		if (newpri < oldpri) {	/* lowering thread's priority */
			if (_maxpriq > newpri)
				_preempt(t, *prip);
		}
	} else {
		if (t->t_state == TS_RUN) {
			if (_suspendedallmutators && t->t_mutator)
				_srqdeq(t);
			else
				_rqdeq(t);
			*prip = newpri;
			_setrq(t); /* will do preemption, if required */
		} else if (t->t_state == TS_SLEEP) {
			/*
			 * sleep queues should also be ordered by
			 * priority. changing the priority of a sleeping
			 * thread should also cause the thread to move
			 * on the sleepq like what's done for the runq.
			 * XXX
			 * The same code here for both bound/unbound threads
			 */
			*prip = newpri;
		} else if (t->t_state == TS_STOPPED) {
			*prip = newpri;
		}
	}
	_sched_unlock();
	_unlock_bucket(ix);
	TRACE_0(UTR_FAC_THREAD, UTR_THR_SETPRIO_END, "_thr_setprio end");
	return (0);
}

/*
 * If the threads library ever needs this function, define a
 * __thr_getprio(), just like __thr_continue().
 */
int
_thr_getprio(thread_t tid, int *pri)
{
	uthread_t *t;
	int ix;

	TRACE_0(UTR_FAC_THREAD, UTR_THR_GETPRIO_START,
					"_thr_getprio start");
	if (tid == (thread_t)0)
		return (ESRCH);
	_lock_bucket((ix = HASH_TID(tid)));
	if ((t = THREAD(tid)) == (uthread_t *)-1 ||
	    t->t_flag & T_INTERNAL) {
		_unlock_bucket(ix);
		return (ESRCH);
	}
	*pri = t->t_pri;
	_unlock_bucket(ix);
	TRACE_0(UTR_FAC_THREAD, UTR_THR_GETPRIO_END, "_thr_getprio end");
	return (0);
}

/*
 * _thr_yield() causes the calling thread to be preempted.
 */
void
_thr_yield(void)
{
	uthread_t *t;

	TRACE_0(UTR_FAC_THREAD, UTR_THR_YIELD_START, "_thr_yield start");
	if (ISBOUND(curthread)) {
		_yield();
		TRACE_0(UTR_FAC_THREAD, UTR_THR_YIELD_END,
						"_thr_yield end");
	} else {
		/*
		 * If there are no threads on run-queue, yield
		 * processor.
		 */
		if (_nrunnable == 0) {
			_yield();
		} else {
			t = curthread;
			_sched_lock();
			if (ONPROCQ(t))
				_onproc_deq(t);
			_setrq(t);
			_sched_unlock_nosig();
			_swtch(0);
			_sigon();
			TRACE_0(UTR_FAC_THREAD, UTR_THR_YIELD_END,
			"_thr_yield end");
		}
	}
}

/*
 * thr_exit: thread is exiting without calling C++ destructors
 */
void
_thr_exit(void *status)
{
	_thr_exit_common(status, 0);
}

/*
 * pthread_exit: normal thr_exit + C++ desturctors need to be called
 */
void
_pthread_exit(void *status)
{
	_thr_exit_common(status, 1);
}

void
_thr_exit_common(void *status, int ex)
{
	sigset_t set;
	uthread_t *t;
	int cancelpending;

	t = curthread;
	TRACE_4(UTR_FAC_THREAD, UTR_THR_EXIT_START,
	    "_thr_exit start:usropts 0x%x, pc 0x%lx, stk 0x%p, lwpid %d",
	    t->t_usropts, t->t_pc, t->t_stk, LWPID(t));

	cancelpending = t->t_can_pending;
	/*
	 * cancellation is disabled
	 */
	*((int *)&(t->t_can_pending)) = 0;
	t->t_can_state = TC_DISABLE;

	/*
	 * special DCE cancellation cleanup hook.
	 */
	if (_cleanuphndlr != NULL &&
	    cancelpending == TC_PENDING &&
	    status == PTHREAD_CANCELED) {
		(*_cleanuphndlr)();
	}

	_lmutex_lock(&_calloutlock);
	if (t->t_itimer_callo.running)
		while (t->t_itimer_callo.running)
			_cond_wait(&t->t_itimer_callo.waiting, &_calloutlock);
	if (t->t_cv_callo.running)
		while (t->t_cv_callo.running)
			_cond_wait(&t->t_cv_callo.waiting,
			    &_calloutlock);
	_lmutex_unlock(&_calloutlock);

	/* remove callout entry */
	_rmcallout(&curthread->t_cv_callo);
	_rmcallout(&curthread->t_itimer_callo);

	maskallsigs(&set);
	/*
	 * Flush all signals sent to this thread, including bounced signals.
	 * After this point, this thread will never be a sig bounce target
	 * even though its state may still be TS_ONPROC.
	 */
	_thr_sigsetmask(SIG_SETMASK, &set, NULL);

	/*
	 * If thr_exit() is called from a signal handler, indicated by the
	 * T_TEMPBOUND flag, clean out any residual state on the
	 * underlying LWP signal mask, so it is clean when it resumes
	 * another thread.
	 */
	if (ISTEMPBOUND(t)) {
		/*
		 * XXX: Cannot just unblock signals here on the LWP. Any
		 * pending signals on the LWP will then come up violating
		 * the invariant of not receiving signals which are blocked
		 * on the thread on the LWP that receives the signal. So
		 * the pending signal on this LWP will have to be handled
		 * differently: figure out how to do this. Some alternatives:
		 * 	- unblock all signals on thread and then block them
		 *		problem: this violates the calling thread's
		 *			 request to block these signals
		 *	- call sigpending() and then call __sigwait() on them
		 *		problem with this is that sigpending() returns
		 *		signals pending to the whole process, not just
		 *		those on the calling LWP.
		 */
		__sigprocmask(SIG_SETMASK, &_null_sigset, NULL);
	}

	t->t_exitstat = status;

	if (t->t_flag & T_INTERNAL)
		_thrp_exit();
		/*
		 * Does not return. Just call the real thr_exit() if this is
		 * an internal thread, such as the aging thread. Otherwise,
		 * call _tcancel_all() to unwind the stack, popping C++
		 * destructors and cancellation handlers.
		 */
	else {
		/*
		 * If thr_exit is being called from the places where
		 * C++ destructors are to be called such as cancellation
		 * points, then set this flag. It is checked in _t_cancel()
		 * to decide whether _ex_unwind() is to be called or not!
		 */
		if (ex)
			t->t_flag |= T_EXUNWIND;
		_tcancel_all(0);
	}
}

/*
 * An exiting thread becomes a zombie thread. If the thread was created
 * with the THREAD_WAIT flag then it is placed on deathrow waiting to be
 * reaped by a caller() of thread_wait(). otherwise the thread may be
 * freed more quickly.
 */
void
_thrp_exit(void)
{
	uthread_t *t;
	int uexiting = 0;

	t = curthread;

	/*
	 * Destroy TSD after all signals are blocked. This is to prevent
	 * tsd references in signal handlers after the tsd has been
	 * destroyed.
	 */
	_destroy_tsd();

	/*
	 * If any slot has been allocated for the reserved TLS access,
	 * call the destructor to free up the thread-local storage if
	 * necessary.
	 * We do not want to penalize threads that do not use this
	 * TLS mechanism, so we call _destroy_resv_tls() conditionally.
	 * This is done by testing if any TLS slots have been allocated.
	 * If not, then there is no need to call _destroy_resv_tls().
	 * Note that we do not need to grab any lock to test the nslots
	 * variable below as the current thread is exiting and hence
	 * if there was indeed some slot used by this thread and the
	 * nslots variable is zero, then by the interface definition,
	 * it should have been deallocated by now and the TLS explicitly
	 * cleaned up by the application itself.
	 * Also since the thread is exiting, any changes in the value of
	 * nslots (while we are reading it below) due to newer slot
	 * allocations/deallocations should not impact the current thread.
	 * This makes it safe not to grab a lock to check the value of nslots.
	 * It is to be noted that this check here for a nonzero "nslots"
	 * value is basically aimed at checking whether we could bypass a call
	 * to _destroy_resv_tls() at all or not and in so doing avoid
	 * penalizing threads that do not use the reserved TLS mechanism.
	 * Within the function _destroy_resv_tls() we need to check each
	 * and every slot and not just "nslots" slots to see if it has
	 * a non NULL value and also if the slot is associated with a non
	 * NULL destructor.
	 */

	if (_resv_tls_common.nslots != 0)
		_destroy_resv_tls();

	/*
	 * Increment count of the non-detached threads which are
	 * available for joining. Mark the condition "on the way"
	 * to zombie queue. It is used by _thr_detach().
	 */
	_reap_lock();
	if (!(t->t_usropts & THR_DAEMON)) {
		if (!(t->t_usropts & THR_DETACHED)) {
			++_u2bzombies;
			t->t_flag |= T_2BZOMBIE;
		}
		uexiting = 1;
	} else if (!(t->t_usropts & THR_DETACHED)) {
		++_d2bzombies;
		t->t_flag |= T_2BZOMBIE;
	}
	_reap_unlock();

	if (__td_event_report(t, TD_DEATH)) {
		t->t_td_evbuf.eventnum = TD_DEATH;
		tdb_event_death();
	}
	_sched_lock();
	_totalthreads--;
	if (uexiting)
		--_userthreads;
	/*
	 * XXX:
	 * It is possible for a calling LWP to be not on the _sc_list, if
	 * this is the child of a fork(). Eventually, when _sc_cleanup()
	 * is fixed to leave LWPs on the _sc_list, for a forkall() call,
	 * _sc_exit() may be called unconditionally below. For now,
	 * _sc_cleanup() is not being fixed due to a risk/benefit tradeoff.
	 * And so, _sc_exit() is called conditionally, if the LWP *is* on
	 * the _sc_list.
	 */
	if (t->t_scforw != NULL) {
		ASSERT(t->t_scback != NULL);
		_sc_exit();
	}
	/*
	 * If thread is being suspended, notify everyone waiting
	 * that the suspension of this thread has happened.
	 */
	if (t->t_stop) {
		if (t->t_stop & TSTP_ALLMUTATORS) {
			t->t_stop &= ~TSTP_ALLMUTATORS;
			_samcnt--;
			if (!_samcnt)
				_lwp_cond_signal(&_samcv);
		}
		_lwp_cond_broadcast(&t->t_suspndcv);
	}
	if (_userthreads == 0) {
		/* last user thread to exit, exit process. */
		_sched_unlock();
		TRACE_5(UTR_FAC_THREAD, UTR_THR_EXIT_END,
		    "_thr_exit(last thread)end:usropts 0x%x, pc 0x%lx, \
		    stk 0x%p, lwpid %d, last? %d",
		    t->t_usropts, t->t_pc, t->t_stk, LWPID(t),
		    1);
		exit(0);
	}

	if (ISBOUND(t)) {
		t->t_state = TS_ZOMB;
		_sched_unlock();
		/* block out signals while thread is exiting */
		__sigprocmask(SIG_SETMASK, &_totalmasked, NULL);
		TRACE_5(UTR_FAC_THREAD, UTR_THR_EXIT_END,
		    "_thr_exit end:usropts 0x%x, pc 0x%lx, stk 0x%p, \
		    tid 0x%x last? %d",
		    t->t_usropts, t->t_pc, t->t_stk, LWPID(t),
		    0);
		_lwp_terminate(curthread);
	} else {
		if (ONPROCQ(t)) {
			_onproc_deq(t);
		}
		t->t_state = TS_ZOMB;
		_nthreads--;
		TRACE_5(UTR_FAC_THREAD, UTR_THR_EXIT_END,
		    "_thr_exit end:usropts 0x%x, pc 0x%lx, stk 0x%p, \
		    lwpid %d, last? %d",
		    t->t_usropts, t->t_pc, t->t_stk, LWPID(t),
		    0);
		_qswtch();
	}
	_panic("_thr_exit");
}

size_t
_thr_min_stack(void)
{
	return (MINSTACK);
}

/*
 * _thr_main() returns 1 if the calling thread is the initial thread,
 * 0 otherwise.
 */
int
_thr_main(void)
{

	if (_t0 == NULL)
		return (-1);
	else
		return (curthread == _t0);
}

/*
 * _thr_errnop() returns the address of the thread specific errno to implement
 * libc's ___errno() function.
 */

int *
_thr_errnop(void)
{
	return (&curthread->t_errno);
}


#undef lwp_self
lwpid_t
lwp_self(void)
{
	return (LWPID(curthread));
}

uthread_t *
_idtot_nolock(thread_t tid)
{
	uthread_t *next, *first;
	int ix = HASH_TID(tid);

	if ((first = _allthreads[ix].first) != NULL) {
		if (first->t_tid == tid) {
			return (first);
		} else {
			next = first->t_next;
			while (next != first) {
				if (next->t_tid == tid) {
					return (next);
				} else
					next = next->t_next;
			}
		}
	}
	return ((uthread_t *)-1);
}

uthread_t *
_idtot(thread_t tid)
{
#ifdef DEBUG
	int ix = HASH_TID(tid);

	ASSERT(MUTEX_HELD(&(_allthreads[ix].lock)));
#endif /* DEBUG */
	return (_idtot_nolock(tid));
}

#if defined(UTRACE) || defined(ITRACE)

thread_t
trace_thr_self(void)
{
	return (curthread->t_tid);
}
#endif

/*
 * Temporary routines for PCR usage.
 */
caddr_t
_tidtotls(thread_t tid)
{
	caddr_t tls;
	int ix;

	_lock_bucket((ix = HASH_TID(tid)));
	tls = (caddr_t)(THREAD(tid))->t_tls;
	_unlock_bucket(ix);
	return (tls);
}

thread_t
_tlstotid(caddr_t tls)
{
	int ix, i;

	for (i = 1; i <= _lasttid; i++) {
		_lock_bucket((ix = HASH_TID(i)));
		if (tls == (caddr_t)(THREAD(i))->t_tls) {
			_unlock_bucket(ix);
			return (i);
		}
		_unlock_bucket(ix);
	}
}

/*
 * Routines for MT Run Time Checking
 * Return 0 if values in stk are valid.  If not valid, return
 * non-zero errno value.
 */

int
_thr_stksegment(stack_t *stk)
{
	extern uthread_t *_t0;
	ucontext_t uc;
	struct rlimit rl;

	if (_t0 == NULL)
		return (EAGAIN);
	else {
		if (curthread->t_flag & T_INTERNAL)
			return (EAGAIN);
		return (_thrp_stksegment(curthread, stk));
	}
}

static int
_thrp_stksegment(uthread_t *t, stack_t *stk)
{
	ucontext_t uc;
	struct rlimit rl;

	/*
	 * If this is the main thread, always get the
	 * the current stack bottom and stack size.
	 * These values can change over the life of the
	 * process.
	 */
	if (t == _t0) {
		/*
		 * the main thread's stack base, and stack
		 * size can change over the life of the process.
		 */
		if (_getcontext(&uc) == 0) {
			t->t_stk = (char *)uc.uc_stack.ss_sp +
				uc.uc_stack.ss_size;
			_getrlimit(RLIMIT_STACK, &rl);
			t->t_stksize = rl.rlim_cur;
		} else
			return (errno);
	}
	stk->ss_sp = t->t_stk;
	stk->ss_size = t->t_stksize;
	stk->ss_flags = 0;
	return (0);
}

int
__thr_sigredirect(uthread_t *t, int ix, int sig, lwpid_t *lwpidp)
{
	int rc;

	ASSERT(MUTEX_HELD(&(_allthreads[ix].lock)));
	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT((t->t_flag & T_SIGWAIT) || !_sigismember(&t->t_hold, sig));
	ASSERT(t != curthread);

	*lwpidp = 0;

	if (t->t_state == TS_ZOMB) {
		TRACE_0(UTR_FAC_THREAD, UTR_THR_SIGBOUNCE_END,
		    "thr_sigredirect end:zombie");
		return (ESRCH);
	} else if (t->t_state == TS_SLEEP) {
		t->t_flag |= T_INTR;
		_unsleep(t);
		_setrq(t);
	}
	_sigaddset(&t->t_bsig, sig);
	t->t_bdirpend = 1;
	/*
	 * Return an lwpid, if
	 * 	this is a bound thread
	 * OR   this thread is ONPROCQ and its state is TS_ONPROC - note that
	 *	the thread could be ONPROCQ but its state may be TS_DISP - this
	 *	is when _disp() picks up this thread, turns off the T_OFFPROC
	 *	flag, puts the thread ONPROCQ, but leaves its state to be
	 *	TS_DISP. The state changes in _resume_ret(), after the lwp that
	 *	picked it up has successfully switched to it. The signal will
	 *	then be received via the call to _sigon() from _resume_ret().
	 * AND this thread is not in sigwait(). If it is in sigwait(), it will
	 *	be picked up by that routine. No need to actually send the
	 *	signal via _lwp_sigredirect().
	 */

	if ((ISBOUND(t) || (ONPROCQ(t) && t->t_state == TS_ONPROC)) &&
	    ((t->t_flag & T_SIGWAIT) == 0)) {
		t->t_flag |= T_BSSIG;
		ASSERT(LWPID(t) != 0);
		*lwpidp = LWPID(t);
		return (0);
	}
	TRACE_0(UTR_FAC_THREAD, UTR_THR_KILL_END, "thr_kill end");
	return (0);
}

/*
 * Private interface between JVM and libthread. JVM needs an offproc thread's
 * stack pointer value. It obtains this by calling the following routine.
 * The assumption is that the tid is always valid, and is an application
 * thread's tid. This saves overhead of validating the tid, and depends on
 * how the JVM works - since this is a private interface, it should be OK
 * to have such a dependency. The interface below may change in the future
 * for future implementations of a JVM.
 */

ulong_t
__gettsp(thread_t tid)
{
	return ((ulong_t)((THREAD(tid))->t_sp));
}

int _suspendingallmutators;	/* when non-zero, suspending all mutators. */
int _suspendedallmutators;	/* when non-zero, all mutators suspended. */
int _mutatorsbarrier;		/* when non-zero, mutators barrier imposed. */
mutex_t _mutatorslock;		/* used to enforce mutators barrier. */
cond_t _mutatorscv;		/* where non-mutators sleep. */
int _samcnt;			/* mutators signalled to suspend themselves */
lwp_cond_t _samcv;		/* wait for all mutators to be suspended */

/*
 * mark a thread a mutator or reset a mutator to being a default,
 * non-mutator thread.
 */
int
_thr_setmutator(thread_t tid, int enabled)
{
	uthread_t *t;

	if (tid == 0)
		t = curthread;
	else {
		t = _idtot_nolock(tid);
		if (t == (uthread_t *)-1)
			return (ESRCH);
		if (t != curthread) {
			/*
			 * the target thread should be the caller itself
			 * or a suspended thread. this prevents the
			 * target thread from also changing its
			 * t_mutator field.
			 */
			if (enabled && !t->t_stop)
				return (EINVAL);
		}
	}
	if (enabled) {
		if (!t->t_mutator) {
			_thrp_mutators_lock();
			t->t_mutator = 1;
			_thrp_mutators_unlock();
		}
	} else {
		if (t->t_mutator) {
			_thrp_mutators_lock();
			t->t_mutator = 0;
			_thrp_mutators_unlock();
		}
	}
	return (0);
}

/*
 * establish a barrier against new mutators. any non-mutator
 * trying to become a mutator is suspended until the barrier
 * is removed.
 */
void
_thr_mutators_barrier(int enabled)
{
	int oldvalue;

	_lmutex_lock(&_mutatorslock);
	oldvalue = _mutatorsbarrier;
	_mutatorsbarrier = enabled;
	/*
	 * wakeup any blocked non-mutators when barrier
	 * is removed.
	 */
	if (oldvalue && !enabled)
		_cond_broadcast(&_mutatorscv);
	_lmutex_unlock(&_mutatorslock);
}

static void
_thrp_mutators_lock()
{
	_lmutex_lock(&_mutatorslock);
	while (_mutatorsbarrier)
		_cond_wait(&_mutatorscv, &_mutatorslock);
}

static void
_thrp_mutators_unlock()
{
	_lmutex_unlock(&_mutatorslock);
}

/*
 * suspend the set of all mutators except for the caller. the list
 * of actively running threads is searched, and only the mutators
 * in this list are suspended. actively running non-mutators remain
 * running. any other thread is suspended. this makes the cost of
 * suspending all mutators proportional to the number of active
 * threads. since only the active threads are separated.
 */
int
_thr_suspend_allmutators(void)
{
	uthread_t *t;
	uthread_t *caller = curthread;
	thread_t tid;
	int sysnum, rc;
	int stopped;

	_sched_lock();
	if (_suspendingallmutators || _suspendedallmutators) {
		_sched_unlock();
		return (EINVAL);
	}
	ASSERT(_samcnt == 0);
	_suspendingallmutators = 1;
	_suspend_rq();
	t = _sc_list;
	do {
		if ((caller == t) || (!t->t_mutator) ||
		    (t->t_stop & TSTP_MUTATOR) || (t->t_state == TS_ZOMB) ||
		    IDLETHREAD(t)) {
			continue;
		}
		ASSERT(!(t->t_stop & TSTP_MUTATOR));
		stopped = t->t_stop;
		t->t_stop |= TSTP_MUTATOR;
		if (t->t_state == TS_ONPROC) {
			if (stopped)
				continue;
			while (rc = _lwp_suspend2(LWPID(t), &sysnum)) {
				if (rc != EINTR)
					_panic("suspend all mutators");
			}
			if (t->t_nosig == 0) {
				if (!ISBOUND(t))
					_onproc_deq(t);
			} else {
				t->t_stop |= TSTP_ALLMUTATORS;
				_samcnt++;
				_lwp_continue(LWPID(t));
			}
		} else {
			/*
			 * wait for threads that are not in TS_ONPROC,
			 * but still running, to context switch, and
			 * manifest their register state.
			 */
			if (!ISPARKED(t)) {
				t->t_stop |= TSTP_ALLMUTATORS;
				_samcnt++;
			}
		}
	} while ((t = t->t_scforw) != _sc_list);

	while (_samcnt > 0)
		_lwp_cond_wait(&_samcv, &_schedlock);
	_sched_owner = caller;
	_suspendedallmutators = 1;
	_suspendingallmutators = 0;
	ASSERT(_sched_owner == caller);
	_sched_unlock();
	return (0);
}

/*
 * suspend the target mutator. the caller is permitted to suspend
 * itself. if a mutator barrier is enabled, the caller will suspend
 * itself as though it is suspended by thr_suspend_allmutators().
 * when the barrier is removed, this thread will be resumed. any
 * suspended mutator, whether suspeded by thr_suspend_mutator(), or
 * by thr_suspend_allmutators(), can be resumed by
 * thr_continue_mutator().
 */
int
_thr_suspend_mutator(thread_t tid)
{
	uthread_t *t;
	int ix, error;

	if (tid == (thread_t)0) {
		t = curthread;
		if (!t->t_mutator)
			return (EINVAL);
		_sched_lock();
		t->t_stop |= TSTP_MUTATOR;
		_suspendself(t);
		_sched_unlock_nosig();
		_swtch(0);
		_sigon();
		return (0);
	}
	return (_thrp_suspend(tid, TSTP_MUTATOR));
}

/*
 * resume the set of all suspended mutators.
 */
int
_thr_continue_allmutators(void)
{
	uthread_t *t;
	uthread_t *caller = curthread;
	extern uthread_t *_sc_list;

	_sched_lock();
	if (!_suspendedallmutators) {
		_sched_unlock();
		return (EINVAL);
	}
	_suspendedallmutators = 0;
	_resume_rq();
	t = _sc_list;
	do {
#ifdef DEBUG
		if (!t->t_stop) {
			ASSERT((t == caller) || !t->t_mutator ||
			    IDLETHREAD(t) || (t->t_state == TS_RUN) ||
			    (t->t_state == TS_ZOMB));
		}
#endif /* DEBUG */
		if (t->t_stop & TSTP_MUTATOR) {
			t->t_stop &= ~TSTP_MUTATOR;
			switch (t->t_state) {
				case TS_ONPROC:
					_lwp_continue(t->t_lwpid);
					if (!ISBOUND(t))
						_onproc_enq(t);
					continue;
				case TS_STOPPED:
					if (!t->t_stop)
						_setrq(t);
					continue;
				default:
					continue;
			}
		}
	} while ((t = t->t_scforw) != _sc_list);
	_sched_unlock();
	return (0);
}

/*
 * resume a suspended mutator.
 */
int
_thr_continue_mutator(thread_t tid)
{
	uthread_t *t;
	int ix;
	int rc;

	if (tid == (thread_t)0)
		return (ESRCH);
	return (_thrp_continue(tid, TSTP_MUTATOR));
}

/*
 * wait for a mutator continued by the caller to suspend
 * itself.
 */
int
_thr_wait_mutator(thread_t tid, int dontwait)
{
	uthread_t *t;
	int error = 0;

	_sched_lock();
	t = _idtot_nolock(tid);
	if (t == (uthread_t *)-1) {
		_sched_unlock();
		return (ESRCH);
	} else if (!t->t_mutator) {
		_sched_unlock();
		return (EINVAL);
	}
	while (t->t_state != TS_STOPPED && t->t_ssflg != 1) {
		if (dontwait) {
			error = EWOULDBLOCK;
			break;
		}
		_lwp_cond_wait(&t->t_suspndcv, &_schedlock);
		_sched_owner = curthread;
		if (_idtot_nolock(tid) == (uthread_t *)-1 ||
		    t->t_state == TS_ZOMB) {
			error = ESRCH;
			break;
		}
	}
	_sched_unlock();
	return (error);
}

/*
 * get the available register state for the target thread.
 */
int
_thr_getstate(thread_t tid, int *flag, lwpid_t *lwp, stack_t *ss, gregset_t rs)
{
	uthread_t *t;
	greg_t *savedgreg;
	int i;
	int trsflag = TRS_INVALID;

	/* When tid is 0, caller is assumed to be the target thread */
	if (tid == 0)
		t = curthread;
	else if ((t = _idtot_nolock(tid)) == (uthread_t *)-1)
		return (ESRCH);
	if (t->t_state == TS_ZOMB || (t->t_flag & (T_2BZOMBIE | T_ZOMBIE))) {
		if (flag != NULL)
			*flag = trsflag;
		return (0);
	}
	if (!t->t_stop && !_suspendedallmutators)
		return (EINVAL);
	if (ss != NULL)
		_thrp_stksegment(t, ss);
	if (rs != NULL) {
		if (t->t_state == TS_ONPROC) {
			if (lwp != NULL)
				*lwp = t->t_lwpid;
			trsflag = TRS_LWPID;
		} else if (t->t_savedstate) {
			memcpy((char *)rs,
			    (char *)(t->t_savedstate->uc_mcontext.gregs),
			    sizeof (gregset_t));
			trsflag = TRS_VALID;
		} else {
			_get_resumestate(t, rs);
			trsflag = TRS_NONVOLATILE;
		}
		if (flag != NULL)
			*flag = trsflag;
	}
	return (0);
}

/*
 * set the appropiate register state for the target thread.
 */
int
_thr_setstate(thread_t tid, int flag, gregset_t rs)
{
	uthread_t *t;
	greg_t *savedgreg;
	int i;
	int error = 0;

	if ((t = _idtot_nolock(tid)) == (uthread_t *)-1)
		return (ESRCH);
	if (t->t_state == TS_ZOMB || (t->t_flag & (T_2BZOMBIE | T_ZOMBIE)))
		return (ESRCH);
	if (!t->t_stop && !_suspendedallmutators)
		return (EINVAL);
	if (rs != NULL) {
		if (flag == TRS_VALID) {
			if (t->t_savedstate) {
				savedgreg = t->t_savedstate->uc_mcontext.gregs;
				for (i = 0; i < _NGREG; i++)
					savedgreg[i] = rs[i];
			} else
				error = EINVAL;
		} else if (flag == TRS_NONVOLATILE) {
			if (!ISBOUND(t) && !t->t_savedstate &&
			    t->t_state != TS_ONPROC)
				_set_resumestate(t, rs);
			else
				error = EINVAL;
		} else
			error = EINVAL;
	}
	return (error);
}

void
_thr_sighndlrinfo(void (**func)(), int *funcsize)
{
	*func = &__sighndlr;
	*funcsize = (char *)&__sighndlrend - (char *)&__sighndlr;
}

static void
_suspendself(uthread_t *t)
{
	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(t->t_state == TS_ONPROC);
	ASSERT(t->t_stop);
	if (t->t_stop & TSTP_MUTATOR) {
		ASSERT(t->t_mutator);
		t->t_mutatormask = 0;
	}
	if (!ISBOUND(t) && ONPROCQ(t))
		_onproc_deq(t);
}
