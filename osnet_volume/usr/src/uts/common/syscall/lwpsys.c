/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)lwpsys.c	1.24	99/10/13 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/kmem.h>
#include <sys/unistd.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

kthread_t *
idtot(kthread_t *head, id_t lwpid)
{
	kthread_t *t;

	if (lwpid == 0)
		return (curthread);
	t = head;
	do {
		if (t->t_tid == lwpid)
			return (t);
	} while ((t = t->t_forw) != head);
	return (NULL);
}

/*
 * Stop an lwp of the current process
 */
longlong_t
syslwp_suspend(id_t lwpid)
{
	kthread_t *t;
	int error;
	int sysnum;
	rval_t r;
	proc_t *p = ttoproc(curthread);

	mutex_enter(&p->p_lock);
	if (lwpid == 0)
		t = curthread;
	else if ((t = idtot(p->p_tlist, lwpid)) == NULL) {
		mutex_exit(&p->p_lock);
		return ((longlong_t)set_errno(ESRCH));
	}
	error = lwp_suspend(t, &sysnum);
	mutex_exit(&p->p_lock);
	if (error)
		return ((longlong_t)set_errno(error));
	r.r_val1 = 0;
	r.r_val2 = sysnum;
	return (r.r_vals);
}

int
syslwp_continue(id_t lwpid)
{
	kthread_t *t;
	proc_t *p = ttoproc(curthread);

	mutex_enter(&p->p_lock);
	if (((t = idtot(p->p_tlist, lwpid)) == NULL)) {
		mutex_exit(&p->p_lock);
		return (set_errno(ESRCH));
	}
	lwp_continue(t);
	mutex_exit(&p->p_lock);
	return (0);
}

int
lwp_kill(id_t lwpid, int sig)
{
	sigqueue_t *sqp;
	kthread_t *t;
	proc_t *p = ttoproc(curthread);

	if (sig < 0 || sig >= NSIG)
		return (set_errno(EINVAL));
	if (sig != 0)
		sqp = kmem_zalloc(sizeof (sigqueue_t), KM_SLEEP);
	mutex_enter(&p->p_lock);
	if ((t = idtot(p->p_tlist, lwpid)) == NULL) {
		mutex_exit(&p->p_lock);
		if (sig != 0)
			kmem_free(sqp, sizeof (sigqueue_t));
		return (set_errno(ESRCH));
	}
	if (sig == 0) {
		mutex_exit(&p->p_lock);
		return (0);
	}
	sqp->sq_info.si_signo = sig;
	sqp->sq_info.si_code = SI_LWP;
	sqp->sq_info.si_pid = p->p_pid;
	sqp->sq_info.si_uid = CRED()->cr_ruid;
	sigaddqa(p, t, sqp);
	mutex_exit(&p->p_lock);
	return (0);
}

/*
 * Support fuction for lwp_wait().
 * Return the index in the zombie array for the specified lwpid.
 * If lwpid == 0, return the index of the first unreaped zombie.
 * Return the non-negative index of the found zombie, or -1 on failure.
 */
static int
zombie_index(proc_t *p, id_t lwpid)
{
	id_t *zid = p->p_zomb_tid;
	int zmax = p->p_zomb_max;
	id_t tid;
	int i;

	for (i = 0; i < zmax; i++) {
		if ((tid = zid[i]) != 0 && (tid == lwpid || lwpid == 0))
			return (i);
	}
	return (-1);
}

int
lwp_wait(id_t lwpid, id_t *departed)
{
	proc_t *p = curproc;
	int error = 0;
	kthread_t *t;
	int i;

	mutex_enter(&p->p_lock);
	curthread->t_waitfor = lwpid;
	p->p_lwpwait++;
	for (;;) {
		/* search the zombie list */
		if ((i = zombie_index(p, lwpid)) >= 0) {
			id_t ztid = p->p_zomb_tid[i];
			p->p_zomb_tid[i] = 0;
			ASSERT(p->p_zombcnt > 0);
			p->p_zombcnt--;
			p->p_lwpwait--;
			curthread->t_waitfor = -1;
			mutex_exit(&p->p_lock);
			if (departed != NULL &&
			    copyout(&ztid, departed, sizeof (id_t)))
				return (set_errno(EFAULT));
			return (0);
		}

		if (lwpid == 0) {
			/*
			 * We are waiting for anybody.
			 * If everybody is waiting, we have deadlock.
			 */
			if (p->p_lwpwait == p->p_lwpcnt)
				error = EDEADLK;
		} else if ((t = idtot(p->p_tlist, lwpid)) == NULL ||
		    !(t->t_proc_flag & TP_TWAIT)) {
			error = ESRCH;
		} else {
			/* fail if there is a deadlock loop */
			for (;;) {
				id_t tid;

				if (t == curthread) {
					error = EDEADLK;
					break;
				}
				if ((tid = t->t_waitfor) == -1)
					break;
				if (zombie_index(p, tid) >= 0)
					break;
				if (tid == 0) {
					if (p->p_lwpwait == p->p_lwpcnt)
						error = EDEADLK;
					break;
				}
				if ((t = idtot(p->p_tlist, tid)) == NULL)
					break;
				ASSERT(t->t_proc_flag & TP_TWAIT);
			}
		}

		if (error)
			break;

		/* wait for some lwp to die */
		if (!cv_wait_sig(&p->p_lwpexit, &p->p_lock)) {
			error = EINTR;
			break;
		}
	}
	p->p_lwpwait--;
	curthread->t_waitfor = -1;
	mutex_exit(&p->p_lock);
	return (set_errno(error));
}

/*
 * This routine may be replaced by a modified sigaddqa() routine.
 * It is essentially the same, except that it does not call
 * sigtoproc() and does not treat job control specially.
 */
void
bsigaddqa(proc_t *p, kthread_t *t, sigqueue_t *sigqp)
{
	sigqueue_t **psqp;
	int sig = sigqp->sq_info.si_signo;

	ASSERT(sig >= 1 && sig < NSIG);
	psqp = &t->t_sigqueue;
	if (sigismember(&p->p_siginfo, sig) &&
	    SI_CANQUEUE(sigqp->sq_info.si_code)) {
		for (; *psqp != NULL; psqp = &(*psqp)->sq_next)
			;
	} else {
		for (; *psqp != NULL; psqp = &(*psqp)->sq_next)
			if ((*psqp)->sq_info.si_signo == sig) {
				siginfofree(sigqp);
				return;
			}
	}
	*psqp = sigqp;
	sigqp->sq_next = NULL;
}

int
lwp_sigredirect(id_t lwpid, int sig, int *queued)
{
	proc_t *p = curproc;
	kthread_t *target = NULL;
	kthread_t *ast = NULL;
	sigqueue_t *qp;
	int q = 0;

	if (sig <= 0 || sig >= NSIG)
		return (set_errno(EINVAL));
	mutex_enter(&p->p_lock);
	/*
	 * Only a process which has setup ASLWP can call lwp_sigredirect().
	 * Must be checked while holding p_lock
	 */
	if (!p->p_aslwptp) {
		mutex_exit(&p->p_lock);
		return (set_errno(EINVAL));
	}
	if (!sigismember(&p->p_notifsigs, sig)) {
		mutex_exit(&p->p_lock);
		return (set_errno(EINVAL));
	}
	if (lwpid != 0 && (target = idtot(p->p_tlist, lwpid)) == NULL) {
		mutex_exit(&p->p_lock);
		return (set_errno(ESRCH));
	}
	sigdelset(&p->p_notifsigs, sig);
	if (lwpid != 0)
		sigaddset(&target->t_sig, sig);
	ast = p->p_aslwptp;
	if (sigdeq(p, ast, sig, &qp) == 1) {
		/*
		 * If there is a signal queued up after this one, notification
		 * for this new signal needs to be sent up - wake up the aslwp.
		 */
		ASSERT(qp != NULL);
		if ((sig >= _SIGRTMIN) && (sig <= _SIGRTMAX) && queued) {

			q = 1;
			sigdelset(&ast->t_sig, sig);
			sigaddset(&p->p_notifsigs, sig);
		} else {
			cv_signal(&p->p_notifcv);
		}
	} else if (sigismember(&ast->t_sig, sig))
		/*
		 * If, after this signal was put into the process notification
		 * set, p_notifsigs, and deleted from the aslwp, another,
		 * non-queued signal was added to the aslwp, then wake-up
		 * the aslwp to do the needful.
		 */
		cv_signal(&p->p_notifcv);
	if (lwpid != 0) {
		if (qp != NULL)
			bsigaddqa(p, target, qp);
		thread_lock(target);
		(void) eat_signal(target, sig);
		thread_unlock(target);
	} else if (qp != NULL) {
		siginfofree(qp);
	}
	mutex_exit(&p->p_lock);
	if ((sig >= _SIGRTMIN) && (sig <= _SIGRTMAX) && queued) {
		(void) copyout(&q, queued, sizeof (int));
	}
	return (0);
}

/*
 * Wait until a signal within a specified set is posted or until the
 * time interval 'timeout' if specified.  The signal is caught but
 * not delivered. The value of the signal is returned to the caller.
 * Also return if another instance of the signal is queued or not.
 */
int
lwp_sigtimedwait(sigset_t *setp, siginfo_t *siginfop, timespec_t *timeoutp,
		int *queued)
{
	return (csigtimedwait(setp, siginfop, timeoutp, queued));
}
