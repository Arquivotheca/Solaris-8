/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_uthread.c	1.28	99/10/19 SMI"

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/conf.h>
#include <sys/cpuvar.h>
#include <sys/cpr.h>
#include <sys/user.h>
#include <sys/cmn_err.h>
#include <sys/callb.h>

extern void utstop_init(void);
extern void add_one_utstop(void);
extern void utstop_timedwait(long ticks);

static void cpr_stop_user(int);
static int cpr_check_user_threads(void);

/*
 * CPR user thread related support routines
 */
void
cpr_signal_user(int sig)
{
/*
 * The signal SIGTHAW and SIGFREEZE cannot be sent to every thread yet
 * since openwin is catching every signal and default action is to exit.
 * We also need to implement the true SIGFREEZE and SIGTHAW to stop threads.
 */
	struct proc *p;

	mutex_enter(&pidlock);

	for (p = practive; p; p = p->p_next) {
		/* only user threads */
		if (p->p_exec == NULL || p->p_stat == SZOMB ||
			p == proc_init || p == ttoproc(curthread))
			continue;

		mutex_enter(&p->p_lock);
		sigtoproc(p, NULL, sig);
		mutex_exit(&p->p_lock);
	}
	mutex_exit(&pidlock);

	DELAY(MICROSEC);
}

/* max wait time for user thread stop */
#define	CPR_UTSTOP_WAIT		hz
#define	CPR_UTSTOP_RETRY	4
static int count;

int
cpr_stop_user_threads()
{
	utstop_init();

	count = 0;
	do {
		if (++count > CPR_UTSTOP_RETRY)
			return (ESRCH);
		cpr_stop_user(count * count * CPR_UTSTOP_WAIT);
	} while (cpr_check_user_threads() &&
		(count < CPR_UTSTOP_RETRY || CPR->c_fcn != AD_CPR_FORCE));

	return (0);
}

/*
 * This routine tries to stop all user threads before we get rid of all
 * its pages.It goes through allthreads list and set the TP_CHKPT flag
 * for all user threads and make them runnable. If all of the threads
 * can be stopped within the max wait time, CPR will proceed. Otherwise
 * CPR is aborted after a few of similiar retries.
 */
static void
cpr_stop_user(int wait)
{
	kthread_id_t tp;
	proc_t *p;

	/* The whole loop below needs to be atomic */
	mutex_enter(&pidlock);

	/* faster this way */
	tp = curthread->t_next;
	do {
		/* kernel threads will be handled later */
		p = ttoproc(tp);
		if (p->p_as == &kas || p->p_stat == SZOMB)
			continue;

		/*
		 * If the thread is stopped (by CPR) already, do nothing;
		 * if running, mark TP_CHKPT;
		 * if sleeping normally, mark TP_CHKPT and setrun;
		 * if sleeping non-interruptable, mark TP_CHKPT only for now;
		 * if sleeping with t_wchan0 != 0 etc, virtually stopped,
		 * do nothing.
		 */

		/* p_lock is needed for modifying t_proc_flag */
		mutex_enter(&p->p_lock);
		thread_lock(tp); /* needed to check CPR_ISTOPPED */

		if (tp->t_state == TS_STOPPED) {
			/*
			 * if already stopped by other reasons, add this new
			 * reason to it.
			 */
			if (tp->t_schedflag & TS_RESUME)
				tp->t_schedflag &= ~TS_RESUME;
		} else {

			tp->t_proc_flag |= TP_CHKPT;

			thread_unlock(tp);
			mutex_exit(&p->p_lock);
			add_one_utstop();
			mutex_enter(&p->p_lock);
			thread_lock(tp);

			aston(tp);

			if (tp->t_state == TS_SLEEP &&
			    (tp->t_flag & T_WAKEABLE)) {
				setrun_locked(tp);
			}
		}
		/*
		 * force the thread into the kernel if it is not already there.
		 */
		if (tp->t_state == TS_ONPROC && tp->t_cpu != CPU)
			poke_cpu(tp->t_cpu->cpu_id);
		thread_unlock(tp);
		mutex_exit(&p->p_lock);

	} while ((tp = tp->t_next) != curthread);
	mutex_exit(&pidlock);

	utstop_timedwait(wait);
}

/*
 * Checks and makes sure all user threads are stopped
 */
static int
cpr_check_user_threads()
{
	kthread_id_t tp;
	int rc = 0;

	mutex_enter(&pidlock);
	tp = curthread->t_next;
	do {
		if (ttoproc(tp)->p_as == &kas || ttoproc(tp)->p_stat == SZOMB)
			continue;

		thread_lock(tp);
		/*
		 * make sure that we are off all the queues and in a stopped
		 * state.
		 */
		if (!CPR_ISTOPPED(tp)) {
			thread_unlock(tp);
			mutex_exit(&pidlock);

			if (count == CPR_UTSTOP_RETRY) {
			DEBUG1(errp("Suspend failed: cannt stop "
				"uthread\n"));
			cmn_err(CE_WARN, "cpr: Suspend cannot stop "
				"process %s (%x:%x).",
				ttoproc(tp)->p_user.u_psargs, (int)tp,
				tp->t_state);
			cmn_err(CE_WARN, "cpr: Process may be waiting for"
				" network request, please try again.");
			}

			DEBUG2(errp("cant stop t=%x state=%x pfg=%x sched=%x\n",
			tp, tp->t_state, tp->t_proc_flag, tp->t_schedflag));
			DEBUG2(errp("proc %x state=%x pid=%d\n",
				ttoproc(tp), ttoproc(tp)->p_stat,
				ttoproc(tp)->p_pidp->pid_id));
			return (1);
		}
		thread_unlock(tp);

	} while ((tp = tp->t_next) != curthread && rc == 0);

	mutex_exit(&pidlock);
	return (0);
}


/*
 * start all threads that were stopped for checkpoint.
 */
void
cpr_start_user_threads()
{
	kthread_id_t tp;
	proc_t *p;

	mutex_enter(&pidlock);
	tp = curthread->t_next;
	do {
		p = ttoproc(tp);
		/*
		 * kernel threads are callback'ed rather than setrun.
		 */
		if (ttoproc(tp)->p_as == &kas) continue;
		/*
		 * t_proc_flag should have been cleared. Just to make sure here
		 */
		mutex_enter(&p->p_lock);
		tp->t_proc_flag &= ~TP_CHKPT;
		mutex_exit(&p->p_lock);

		thread_lock(tp);
		if (CPR_ISTOPPED(tp)) {

			/*
			 * put it back on the runq
			 */
			tp->t_schedflag |= TS_RESUME;
			setrun_locked(tp);
		}
		thread_unlock(tp);
		/*
		 * DEBUG - Keep track of current and next thread pointer.
		 */
	} while ((tp = tp->t_next) != curthread);

	mutex_exit(&pidlock);
}

/*
 * Stop kernel threads by using the callback mechanism.  If any thread
 * cannot be stopped, return failure.
 */
int
cpr_stop_kernel_threads(void)
{
	caddr_t	name;
	kthread_id_t tp;
	proc_t *p;

	callb_lock_table();	/* Note: we unlock the table in resume. */

	if ((name = callb_execute_class(CB_CL_CPR_DAEMON,
	    CB_CODE_CPR_CHKPT)) != (caddr_t)NULL) {
		cmn_err(CE_WARN,
		    "cpr: Could not stop \"%s\" kernel thread.  "
		    "Please try again later.", name);
		return (EBUSY);
	}

	/*
	 * We think we stopped all the kernel threads.  Just in case
	 * someone is not playing by the rules, take a spin through
	 * the threadlist and see if we can account for everybody.
	 */
	mutex_enter(&pidlock);
	tp = curthread->t_next;
	do {
		p = ttoproc(tp);
		if (p->p_as != &kas)
			continue;

		if (tp->t_flag & T_INTR_THREAD)
			continue;

		if (! callb_is_stopped(tp, &name)) {
			mutex_exit(&pidlock);
			cmn_err(CE_WARN,
			    "cpr: \"%s\" kernel thread not stopped.", name);
			return (EBUSY);
		}
	} while ((tp = tp->t_next) != curthread);

	mutex_exit(&pidlock);
	return (0);
}
