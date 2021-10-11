/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)condvar.c	1.75	99/08/31 SMI"

#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/sobject.h>
#include <sys/sleepq.h>
#include <sys/cpuvar.h>
#include <sys/condvar.h>
#include <sys/condvar_impl.h>
#include <sys/schedctl.h>
#include <sys/procfs.h>

/*
 * CV_MAX_WAITERS is the maximum number of waiters we track; once
 * the number becomes higher than that, we look at the sleepq to
 * see whether there are *really* any waiters.
 */
#define	CV_MAX_WAITERS		1024		/* must be power of 2 */
#define	CV_WAITERS_MASK		(CV_MAX_WAITERS - 1)

/*
 * Threads don't "own" condition variables.
 */
/* ARGSUSED */
static kthread_t *
cv_owner(void *cvp)
{
	return (NULL);
}

/*
 * Unsleep a thread that's blocked on a condition variable.
 */
static void
cv_unsleep(kthread_t *t)
{
	condvar_impl_t *cvp = (condvar_impl_t *)t->t_wchan;
	sleepq_head_t *sqh = SQHASH(cvp);

	ASSERT(THREAD_LOCK_HELD(t));

	if (cvp == NULL)
		panic("cv_unsleep: thread %p not on sleepq %p", t, sqh);
	sleepq_unsleep(t);
	if (cvp->cv_waiters != CV_MAX_WAITERS)
		cvp->cv_waiters--;
	disp_lock_exit_high(&sqh->sq_lock);
	CL_SETRUN(t);
}

/*
 * Change the priority of a thread that's blocked on a condition variable.
 */
static void
cv_change_pri(kthread_t *t, pri_t pri, pri_t *t_prip)
{
	condvar_impl_t *cvp = (condvar_impl_t *)t->t_wchan;
	sleepq_t *sqp = t->t_sleepq;

	ASSERT(THREAD_LOCK_HELD(t));
	ASSERT(&SQHASH(cvp)->sq_queue == sqp);

	if (cvp == NULL)
		panic("cv_change_pri: %p not on sleep queue", t);
	sleepq_dequeue(t);
	*t_prip = pri;
	sleepq_insert(sqp, t);
}

/*
 * The sobj_ops vector exports a set of functions needed when a thread
 * is asleep on a synchronization object of this type.
 */
static sobj_ops_t cv_sobj_ops = {
	SOBJ_CV, cv_owner, cv_unsleep, cv_change_pri
};

/* ARGSUSED */
void
cv_init(kcondvar_t *cvp, char *name, kcv_type_t type, void *arg)
{
	((condvar_impl_t *)cvp)->cv_waiters = 0;
}

/*
 * cv_destroy is not currently needed, but is part of the DDI.
 * This is in case cv_init ever needs to allocate something for a cv.
 */
/* ARGSUSED */
void
cv_destroy(kcondvar_t *cvp)
{
	ASSERT((((condvar_impl_t *)cvp)->cv_waiters & CV_WAITERS_MASK) == 0);
}

/*
 * The cv_block() function blocks a thread on a condition variable
 * by putting it in a hashed sleep queue associated with the
 * synchronization object.
 *
 * Threads are taken off the hashed sleep queues via calls to
 * cv_signal(), cv_broadcast(), or cv_unsleep().
 */
static void
cv_block(condvar_impl_t *cvp)
{
	sleepq_head_t	*sqh;
	klwp_t *lwp	= ttolwp(curthread);

	ASSERT(THREAD_LOCK_HELD(curthread));
	ASSERT(curthread != CPU->cpu_idle_thread);
	ASSERT(CPU->cpu_on_intr == 0);
	ASSERT(curthread->t_wchan0 == NULL && curthread->t_wchan == NULL);
	ASSERT(curthread->t_state == TS_ONPROC);

	CL_SLEEP(curthread, 0);			/* assign kernel priority */
	curthread->t_wchan = (caddr_t)cvp;
	curthread->t_sobj_ops = &cv_sobj_ops;
	if (lwp != NULL) {
		lwp->lwp_ru.nvcsw++;
		if (curthread->t_proc_flag & TP_MSACCT)
			(void) new_mstate(curthread, LMS_SLEEP);
	}

	sqh = SQHASH(cvp);
	disp_lock_enter_high(&sqh->sq_lock);
	if (cvp->cv_waiters < CV_MAX_WAITERS)
		cvp->cv_waiters++;
	ASSERT(cvp->cv_waiters <= CV_MAX_WAITERS);
	THREAD_SLEEP(curthread, &sqh->sq_lock);
	sleepq_insert(&sqh->sq_queue, curthread);
	/*
	 * THREAD_SLEEP() moves curthread->t_lockp to point to the
	 * lock sqh->sq_lock. This lock is later released by the caller
	 * when it calls thread_unlock() on curthread.
	 */
}

#define	cv_block_sig(cvp)	\
	{ curthread->t_flag |= T_WAKEABLE; cv_block(cvp); }

/*
 * Block on the indicated condition variable and release the
 * associated kmutex while blocked.
 */
void
cv_wait(kcondvar_t *cvp, kmutex_t *mp)
{
	if (panicstr)
		return;

	ASSERT(curthread->t_schedflag & TS_DONT_SWAP);
	thread_lock(curthread);			/* lock the thread */
	cv_block((condvar_impl_t *)cvp);
	thread_unlock_nopreempt(curthread);	/* unlock the waiters field */
	mutex_exit(mp);
	swtch();
	mutex_enter(mp);
}

/*
 * Same as cv_wait except the thread will unblock at 'tim'
 * (an absolute time) if it hasn't already unblocked.
 *
 * Returns the amount of time left from the original 'tim' value
 * when it was unblocked.
 */
clock_t
cv_timedwait(kcondvar_t *cvp, kmutex_t *mp, clock_t tim)
{
	timeout_id_t	id;
	clock_t		timeleft;

	if (panicstr)
		return (0);

	timeleft = tim - lbolt;
	if (timeleft <= 0)
		return (-1);
	id = realtime_timeout((void (*)(void *))setrun, curthread, timeleft);
	thread_lock(curthread);		/* lock the thread */
	cv_block((condvar_impl_t *)cvp);
	thread_unlock_nopreempt(curthread);
	mutex_exit(mp);
	if ((tim - lbolt) <= 0)		/* allow for wrap */
		setrun(curthread);
	swtch();
	/*
	 * Get the time left. untimeout() returns -1 if the timeout has
	 * occured or the time remaining.  If the time remaining is zero,
	 * the timeout has occured between when we were awoken and
	 * we called untimeout.  We will treat this as if the timeout
	 * has occured and set timeleft to -1.
	 */
	timeleft = untimeout(id);
	if (timeleft <= 0)
		timeleft = -1;
	mutex_enter(mp);
	return (timeleft);
}

int
cv_wait_sig(kcondvar_t *cvp, kmutex_t *mp)
{
	kthread_t *t = curthread;
	proc_t *p = ttoproc(t);
	klwp_t *lwp = ttolwp(t);
	int rval = 1;
	int scblock;

	if (panicstr)
		return (rval);

	if (lwp == NULL) {
		cv_wait(cvp, mp);
		return (rval);
	}

	ASSERT(curthread->t_schedflag & TS_DONT_SWAP);
	lwp->lwp_asleep = 1;
	lwp->lwp_sysabort = 0;
	scblock = schedctl_check(t, SC_BLOCK);
	if (scblock == 0 || schedctl_block(mp) == 0) {
		thread_lock(t);
		cv_block_sig((condvar_impl_t *)cvp);
		thread_unlock_nopreempt(t);
		mutex_exit(mp);
		if (ISSIG(t, JUSTLOOKING) || MUSTRETURN(p, t))
			setrun(t);
		/* ASSERT(no locks are held) */
		swtch();
		t->t_flag &= ~T_WAKEABLE;
		if (scblock)
			schedctl_unblock();
		mutex_enter(mp);
	}
	if (ISSIG_PENDING(t, lwp, p)) {
		mutex_exit(mp);
		if (issig(FORREAL))
			rval = 0;
		mutex_enter(mp);
	}
	if (lwp->lwp_sysabort || MUSTRETURN(p, t))
		rval = 0;
	lwp->lwp_asleep = 0;
	lwp->lwp_sysabort = 0;
	return (rval);
}

/*
 * Returns:
 * 	Function result in order of presidence:
 *			 0 if if a signal was recieved
 *			-1 if timeout occured
 *			 1 if awakened via cv_signal() or cv_broadcast().
 *
 * cv_timedwait_sig() is now part of the DDI.
 */
clock_t
cv_timedwait_sig(kcondvar_t *cvp, kmutex_t *mp, clock_t tim)
{
	kthread_t *t = curthread;
	proc_t *p = ttoproc(t);
	klwp_t *lwp = ttolwp(t);
	timeout_id_t	id;
	clock_t		ret_value = 1;
	int		scblock;
	clock_t		timeleft;

	if (panicstr)
		return (ret_value);

	/*
	 * If there is no lwp, then we don't need to wait for a signal.
	 */
	if (lwp == NULL) {
		return (cv_timedwait(cvp, mp, tim));
	}

	/*
	 * If tim is less than or equal to lbolt, then the timeout
	 * has already occured.  So just check to see if there is a signal
	 * pending.  If so return 0 indicating that there is a signal pending.
	 * Else return -1 indicating that the timeout occured. No need to
	 * wait on anything.
	 */
	timeleft = tim - lbolt;
	if (timeleft <= 0) {
		lwp->lwp_asleep = 1;
		lwp->lwp_sysabort = 0;
		ret_value = -1;
		if (ISSIG_PENDING(t, lwp, p)) {
			mutex_exit(mp);
			if (issig(FORREAL))
				ret_value = 0;
			mutex_enter(mp);
		}
		if (lwp->lwp_sysabort || MUSTRETURN(p, t))
			ret_value = 0;
		lwp->lwp_asleep = 0;
		lwp->lwp_sysabort = 0;
		return (ret_value);
	}

	/*
	 * Set the timeout and wait.
	 */
	id = realtime_timeout((void (*)(void *))setrun, t, timeleft);
	lwp->lwp_asleep = 1;
	lwp->lwp_sysabort = 0;
	scblock = schedctl_check(t, SC_BLOCK);
	if (scblock == 0 || schedctl_block(mp) == 0) {
		thread_lock(t);
		cv_block_sig((condvar_impl_t *)cvp);
		thread_unlock_nopreempt(t);
		mutex_exit(mp);
		if (ISSIG(t, JUSTLOOKING) ||
		    MUSTRETURN(p, t) || (tim - lbolt <= 0))
			setrun(t);
		/* ASSERT(no locks are held) */
		swtch();
		t->t_flag &= ~T_WAKEABLE;
		if (scblock)
			schedctl_unblock();
		mutex_enter(mp);
	}

	/*
	 * Untimeout the thread.  untimeout() returns -1 if the timeout has
	 * occured or the time remaining.  If the time remaining is zero,
	 * the timeout has occured between when we were awoken and
	 * we called untimeout.  We will treat this as if the timeout
	 * has occured and set ret_value to -1.
	 */
	ret_value = untimeout(id);
	if (ret_value <= 0)
		ret_value = -1;

	/*
	 * Check to see if a signal is pending.  If so, regardless of whether
	 * or not we were awoken due to the signal, the signal is now pending
	 * and a return of 0 has the highest priority.
	 */
	if (ISSIG_PENDING(t, lwp, p)) {
		mutex_exit(mp);
		if (issig(FORREAL))
			ret_value = 0;
		mutex_enter(mp);
	}
	if (lwp->lwp_sysabort || MUSTRETURN(p, t))
		ret_value = 0;
	lwp->lwp_asleep = 0;
	lwp->lwp_sysabort = 0;
	return (ret_value);
}

/*
 * Same as cv_wait_sig but the thread can be swapped out while waiting.
 * This should only be used when we know we aren't holding any locks.
 */
int
cv_wait_sig_swap(kcondvar_t *cvp, kmutex_t *mp)
{
	kthread_t *t = curthread;
	proc_t *p = ttoproc(t);
	klwp_t *lwp = ttolwp(t);
	int rval = 1;
	int scblock;

	if (panicstr)
		return (rval);

	if (lwp == NULL) {
		cv_wait(cvp, mp);
		return (rval);
	}

	lwp->lwp_asleep = 1;
	lwp->lwp_sysabort = 0;
	scblock = schedctl_check(t, SC_BLOCK);
	if (scblock == 0 || schedctl_block(mp) == 0) {
		thread_lock(t);
		t->t_kpri_req = 0;	/* don't need kernel priority */
		cv_block_sig((condvar_impl_t *)cvp);
		/* I can be swapped now */
		curthread->t_schedflag &= ~TS_DONT_SWAP;
		thread_unlock_nopreempt(t);
		mutex_exit(mp);
		if (ISSIG(t, JUSTLOOKING) || MUSTRETURN(p, t))
			setrun(t);
		/* ASSERT(no locks are held) */
		swtch();
		t->t_flag &= ~T_WAKEABLE;
		/* TS_DONT_SWAP set by disp() */
		ASSERT(curthread->t_schedflag & TS_DONT_SWAP);
		if (scblock)
			schedctl_unblock();
		mutex_enter(mp);
	}
	if (ISSIG_PENDING(t, lwp, p)) {
		mutex_exit(mp);
		if (issig(FORREAL))
			rval = 0;
		mutex_enter(mp);
	}
	if (lwp->lwp_sysabort || MUSTRETURN(p, t))
		rval = 0;
	lwp->lwp_asleep = 0;
	lwp->lwp_sysabort = 0;
	return (rval);
}

void
cv_signal(kcondvar_t *cvp)
{
	condvar_impl_t *cp = (condvar_impl_t *)cvp;

	if (cp->cv_waiters > 0) {
		sleepq_head_t *sqh = SQHASH(cp);
		disp_lock_enter(&sqh->sq_lock);
		ASSERT(CPU->cpu_on_intr == 0);
		if (cp->cv_waiters & CV_WAITERS_MASK) {
			cp->cv_waiters--;
			(void) sleepq_wakeone_chan(&sqh->sq_queue, cp);
		} else if (sleepq_wakeone_chan(&sqh->sq_queue, cp) == NULL) {
			cp->cv_waiters = 0;
		}
		disp_lock_exit(&sqh->sq_lock);
	}
}

void
cv_broadcast(kcondvar_t *cvp)
{
	condvar_impl_t *cp = (condvar_impl_t *)cvp;

	if (cp->cv_waiters > 0) {
		sleepq_head_t *sqh = SQHASH(cp);
		disp_lock_enter(&sqh->sq_lock);
		ASSERT(CPU->cpu_on_intr == 0);
		sleepq_wakeall_chan(&sqh->sq_queue, cp);
		cp->cv_waiters = 0;
		disp_lock_exit(&sqh->sq_lock);
	}
}

/*
 * Same as cv_wait(), but wakes up to check for requests to stop,
 * like cv_wait_sig() but without dealing with signals.
 * This is a horrible kludge.  It is evil.  It is vile.  It is swill.
 * If your code has to call this function then your code is the same.
 */
void
cv_wait_stop(kcondvar_t *cvp, kmutex_t *mp)
{
	kthread_t *t = curthread;
	klwp_t *lwp = ttolwp(t);
	proc_t *p = ttoproc(t);
	timeout_id_t id;
	clock_t tim;

	if (panicstr)
		return;

	/*
	 * Wakeup in 1/4 second, i.e., human time.
	 */
	tim = lbolt + hz / 4;
	id = realtime_timeout((void (*)(void *))setrun, t, tim - lbolt);
	thread_lock(t);			/* lock the thread */
	cv_block((condvar_impl_t *)cvp);
	thread_unlock_nopreempt(t);
	mutex_exit(mp);
	/* ASSERT(no locks are held); */
	if ((tim - lbolt) <= 0)		/* allow for wrap */
		setrun(t);
	swtch();
	(void) untimeout(id);

	/*
	 * Check for reasons to stop if lwp_nostop is not true.
	 * See issig_forreal() for explanations of the various stops.
	 */
	mutex_enter(&p->p_lock);
	while (lwp->lwp_nostop == 0 && !(p->p_flag & EXITLWPS)) {
		/*
		 * Hold the lwp here for watchpoint manipulation.
		 */
		if (t->t_proc_flag & TP_PAUSE) {
			stop(PR_SUSPENDED, SUSPEND_PAUSE);
			continue;
		}
		/*
		 * System checkpoint.
		 */
		if (t->t_proc_flag & TP_CHKPT) {
			stop(PR_CHECKPOINT, 0);
			continue;
		}
		/*
		 * Honor fork1(), watchpoint activity (remapping a page),
		 * and lwp_suspend() requests.
		 */
		if ((p->p_flag & (HOLDFORK1|HOLDWATCH)) ||
		    (t->t_proc_flag & TP_HOLDLWP)) {
			stop(PR_SUSPENDED, SUSPEND_NORMAL);
			continue;
		}
		/*
		 * Honor /proc requested stop.
		 */
		if (t->t_proc_flag & TP_PRSTOP) {
			stop(PR_REQUESTED, 0);
		}
		/*
		 * If some lwp in the process has already stopped
		 * showing PR_JOBCONTROL, stop in sympathy with it.
		 */
		if (p->p_stopsig && t != p->p_agenttp) {
			stop(PR_JOBCONTROL, p->p_stopsig);
			continue;
		}
		break;
	}
	mutex_exit(&p->p_lock);
	mutex_enter(mp);
}
