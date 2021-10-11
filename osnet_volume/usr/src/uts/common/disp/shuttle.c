/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)shuttle.c	1.19	99/07/29 SMI"

/*
 * Routines to support shuttle synchronization objects
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <sys/class.h>
#include <sys/debug.h>
#include <sys/sobject.h>
#include <sys/cpuvar.h>


/*
 * Place the thread in question on the run q.
 */
static void
shuttle_unsleep(kthread_t *t)
{
	ASSERT(THREAD_LOCK_HELD(t));

	/* Waiting on a shuttle */
	ASSERT(t->t_wchan0 == (caddr_t)1 && t->t_wchan == NULL);
	t->t_flag &= ~T_WAKEABLE;
	t->t_wchan0 = NULL;
	t->t_sobj_ops = NULL;
	THREAD_TRANSITION(t);
	CL_SETRUN(t);
}

static kthread_t *
shuttle_owner()
{
	return (NULL);
}

/*ARGSUSED*/
static void
shuttle_change_pri(kthread_t *t, pri_t p, pri_t *t_prip)
{
	ASSERT(THREAD_LOCK_HELD(t));
	*t_prip = p;
}

static sobj_ops_t shuttle_sobj_ops = {
	SOBJ_SHUTTLE, shuttle_owner, shuttle_unsleep, shuttle_change_pri
};

/*
 * Mark the current thread as sleeping on a shuttle object, and
 * resume the specified thread. The 't' thread must be marked as ONPROC.
 *
 * No locks other than 'l' should be held at this point.
 */
void
shuttle_resume(kthread_t *t, kmutex_t *l)
{
	klwp_t	*lwp = ttolwp(curthread);
	cpu_t	*cp;
	extern disp_lock_t	shuttle_lock;

	thread_lock(curthread);
	disp_lock_enter_high(&shuttle_lock);
	if (lwp != NULL) {
		lwp->lwp_asleep = 1;			/* /proc */
		lwp->lwp_sysabort = 0;			/* /proc */
		lwp->lwp_ru.nvcsw++;
	}
	curthread->t_flag |= T_WAKEABLE;
	curthread->t_sobj_ops = &shuttle_sobj_ops;
	/*
	 * setting cpu_dispthread before changing thread state
	 * so that kernel preemption will be deferred to after swtch_to()
	 */
	cp = CPU;
	cp->cpu_dispthread = t;
	cp->cpu_dispatch_pri = DISP_PRIO(t);
	/*
	 * Set the wchan0 field so that /proc won't just do a setrun
	 * on this thread when trying to stop a process. Instead,
	 * /proc will mark the thread as VSTOPPED similar to threads
	 * that are blocked on user level condition variables.
	 */
	curthread->t_wchan0 = (caddr_t)1;
	CL_INACTIVE(curthread);
	THREAD_SLEEP(curthread, &shuttle_lock);

	/* Update ustate records (there is no waitrq obviously) */
	if (curthread->t_proc_flag & TP_MSACCT)
		(void) new_mstate(curthread, LMS_SLEEP);
	if (t->t_proc_flag & TP_MSACCT)
		restore_mstate(t);
	t->t_flag &= ~T_WAKEABLE;
	t->t_wchan0 = NULL;
	t->t_sobj_ops = NULL;
	disp_lock_exit_high(&shuttle_lock);
	mutex_exit(l);
	/*
	 * Make sure we didn't receive any important events while
	 * we weren't looking
	 */
	if (ISANOMALOUS(curproc)) {
		if (lwp &&
		    (ISSIG(curthread, JUSTLOOKING) ||
		    MUSTRETURN(curproc, curthread)))
			setrun(curthread);
	}
	CL_ACTIVE(t);
	swtch_to(t);
	/*
	 * Caller must check for ISSIG/lwp_sysabort conditions
	 * and clear lwp->lwp_asleep/lwp->lwp_sysabort
	 */
}

/*
 * Resume the specified thread.  The current thread has already been
 * put in a state other than ONPROC.  The 't' thread must be marked
 * as ONPROC.  This is essentially the back end of a shuttle_resume(),
 * and is called from swtch() when the t_handoff pointer is set.
 */
void
shuttle_resume_async(kthread_t *t)
{
	cpu_t	*cp;

	(void) splhigh();
	/*
	 * setting cpu_dispthread before changing thread state
	 * so that kernel preemption will be deferred to after swtch_to()
	 */
	cp = CPU;
	cp->cpu_dispthread = t;
	cp->cpu_dispatch_pri = DISP_PRIO(t);

	if (t->t_proc_flag & TP_MSACCT)
		restore_mstate(t);
	t->t_flag &= ~T_WAKEABLE;
	t->t_wchan0 = NULL;
	t->t_sobj_ops = NULL;
	CL_ACTIVE(t);
	swtch_to(t);
}

/*
 * Mark the current thread as sleeping on a shuttle object, and
 * switch to a new thread.
 * No locks other than 'l' should be held at this point.
 */
void
shuttle_swtch(kmutex_t *l)
{
	klwp_t	*lwp = ttolwp(curthread);
	extern disp_lock_t	shuttle_lock;

	thread_lock(curthread);
	disp_lock_enter_high(&shuttle_lock);
	lwp->lwp_asleep = 1;			/* /proc */
	lwp->lwp_sysabort = 0;			/* /proc */
	lwp->lwp_ru.nvcsw++;
	curthread->t_flag |= T_WAKEABLE;
	curthread->t_sobj_ops = &shuttle_sobj_ops;
	curthread->t_wchan0 = (caddr_t)1;
	CL_INACTIVE(curthread);
	THREAD_SLEEP(curthread, &shuttle_lock);
	if (curthread->t_proc_flag & TP_MSACCT)
		(void) new_mstate(curthread, LMS_SLEEP);
	disp_lock_exit_high(&shuttle_lock);
	mutex_exit(l);
	if (ISANOMALOUS(curproc)) {
		if (ISSIG(curthread, JUSTLOOKING) ||
		    MUSTRETURN(curproc, curthread))
			setrun(curthread);
	}
	swtch();
	/*
	 * Caller must check for ISSIG/lwp_sysabort conditions
	 * and clear lwp->lwp_asleep/lwp->lwp_sysabort
	 */
}

/*
 * Mark the specified thread as sleeping on a shuttle object.
 */
void
shuttle_sleep(kthread_t *t)
{
	klwp_t	*lwp = ttolwp(t);
	proc_t	*p = ttoproc(t);

	extern disp_lock_t	shuttle_lock;

	thread_lock(t);
	disp_lock_enter_high(&shuttle_lock);
	if (lwp != NULL) {
		lwp->lwp_asleep = 1;			/* /proc */
		lwp->lwp_sysabort = 0;			/* /proc */
		lwp->lwp_ru.nvcsw++;
	}
	t->t_flag |= T_WAKEABLE;
	t->t_sobj_ops = &shuttle_sobj_ops;
	t->t_wchan0 = (caddr_t)1;
	CL_INACTIVE(t);
	THREAD_SLEEP(t, &shuttle_lock);
	if (t->t_proc_flag & TP_MSACCT)
		(void) new_mstate(t, LMS_SLEEP);
	disp_lock_exit_high(&shuttle_lock);
	if (ISANOMALOUS(p)) {
		if (lwp && (ISSIG(t, JUSTLOOKING) || MUSTRETURN(p, t)))
			setrun(t);
	}
}
