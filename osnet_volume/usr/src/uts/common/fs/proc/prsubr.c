/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)prsubr.c	1.133	99/09/20 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/inline.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/sobject.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/session.h>
#include <sys/pcb.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/ts.h>
#include <sys/bitmap.h>
#include <sys/poll.h>
#include <sys/shm.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/processor.h>
#include <sys/cpuvar.h>
#include <sys/copyops.h>
#include <vm/as.h>
#include <vm/rm.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/seg_dev.h>
#include <vm/seg_spt.h>
#include <vm/page.h>
#include <sys/vmparam.h>
#include <sys/swap.h>
#include <fs/proc/prdata.h>

typedef struct prpagev {
	uint_t *pg_protv;	/* vector of page permissions */
	char *pg_incore;	/* vector of incore flags */
	size_t pg_npages;	/* number of pages in protv and incore */
	ulong_t pg_pnbase;	/* pn within segment of first protv element */
} prpagev_t;

size_t pagev_lim = 256 * 1024;	/* limit on number of pages in prpagev_t */

extern struct seg_ops segdev_ops;	/* needs a header file */
extern struct seg_ops segspt_shmops;	/* needs a header file */

static	int	set_watched_page(proc_t *, caddr_t, caddr_t, ulong_t,
			ulong_t, struct watched_page *);
static	void	clear_watched_page(proc_t *, caddr_t, caddr_t, ulong_t);

/*
 * Choose an lwp from the complete set of lwps for the process.
 * This is called for any operation applied to the process
 * file descriptor that requires an lwp to operate upon.
 *
 * Returns a pointer to the thread for the selected LWP,
 * and with the dispatcher lock held for the thread.
 *
 * The algorithm for choosing an lwp is critical for /proc semantics;
 * don't touch this code unless you know all of the implications.
 */
kthread_t *
prchoose(proc_t *p)
{
	kthread_t *t;
	kthread_t *t_onproc = NULL;	/* running on processor */
	kthread_t *t_run = NULL;	/* runnable, on disp queue */
	kthread_t *t_sleep = NULL;	/* sleeping */
	kthread_t *t_hold = NULL;	/* sleeping, performing hold */
	kthread_t *t_susp = NULL;	/* suspended stop */
	kthread_t *t_jstop = NULL;	/* jobcontrol stop, w/o directed stop */
	kthread_t *t_jdstop = NULL;	/* jobcontrol stop with directed stop */
	kthread_t *t_req = NULL;	/* requested stop */
	kthread_t *t_istop = NULL;	/* event-of-interest stop */

	ASSERT(MUTEX_HELD(&p->p_lock));

	/*
	 * If the agent lwp exists, it takes precedence over all others.
	 */
	if ((t = p->p_agenttp) != NULL) {
		thread_lock(t);
		return (t);
	}

	if ((t = p->p_tlist) == NULL)	/* start at the head of the list */
		return (t);
	do {		/* for eacn lwp in the process */
		if (VSTOPPED(t)) {	/* virtually stopped */
			if (t_req == NULL)
				t_req = t;
			continue;
		}

		thread_lock(t);		/* make sure thread is in good state */
		switch (t->t_state) {
		default:
			cmn_err(CE_PANIC,
			    "prchoose: bad thread state %d, thread 0x%p",
			    t->t_state, (void *)t);
			break;
		case TS_SLEEP:
			/* this is filthy */
			if (t->t_wchan == (caddr_t)&p->p_holdlwps &&
			    t->t_wchan0 == NULL) {
				if (t_hold == NULL)
					t_hold = t;
			} else {
				if (t_sleep == NULL)
					t_sleep = t;
			}
			break;
		case TS_RUN:
			if (t_run == NULL)
				t_run = t;
			break;
		case TS_ONPROC:
			if (t_onproc == NULL)
				t_onproc = t;
			break;
		case TS_ZOMB:		/* last possible choice */
			break;
		case TS_STOPPED:
			switch (t->t_whystop) {
			case PR_SUSPENDED:
				if (t_susp == NULL)
					t_susp = t;
				break;
			case PR_JOBCONTROL:
				if (t->t_proc_flag & TP_PRSTOP) {
					if (t_jdstop == NULL)
						t_jdstop = t;
				} else {
					if (t_jstop == NULL)
						t_jstop = t;
				}
				break;
			case PR_REQUESTED:
				if (t_req == NULL)
					t_req = t;
				break;
			case PR_SYSENTRY:
			case PR_SYSEXIT:
			case PR_SIGNALLED:
			case PR_FAULTED:
				/*
				 * Make an lwp calling exit() be the
				 * last lwp seen in the process.
				 */
				if (t_istop == NULL ||
				    (t_istop->t_whystop == PR_SYSENTRY &&
				    t_istop->t_whatstop == SYS_exit))
					t_istop = t;
				break;
			case PR_CHECKPOINT:	/* can't happen? */
				break;
			default:
				cmn_err(CE_PANIC,
				    "prchoose: bad t_whystop %d, thread 0x%p",
				    t->t_whystop, (void *)t);
				break;
			}
			break;
		}
		thread_unlock(t);
	} while ((t = t->t_forw) != p->p_tlist);

	if (t_onproc)
		t = t_onproc;
	else if (t_run)
		t = t_run;
	else if (t_sleep)
		t = t_sleep;
	else if (t_jstop)
		t = t_jstop;
	else if (t_jdstop)
		t = t_jdstop;
	else if (t_istop)
		t = t_istop;
	else if (t_req)
		t = t_req;
	else if (t_hold)
		t = t_hold;
	else if (t_susp)
		t = t_susp;
	else			/* TS_ZOMB */
		t = p->p_tlist;

	if (t != NULL)
		thread_lock(t);
	return (t);
}

/*
 * Wakeup anyone sleeping on the /proc vnode for the process/lwp to stop.
 * Also call pollwakeup() if any lwps are waiting in poll() for POLLPRI
 * on the /proc file descriptor.  Called from stop() when a traced
 * process stops on an event of interest.  Also called from exit()
 * and prinvalidate() to indicate POLLHUP and POLLERR respectively.
 */
void
prnotify(struct vnode *vp)
{
	prcommon_t *pcp = VTOP(vp)->pr_common;

	mutex_enter(&pcp->prc_mutex);
	cv_broadcast(&pcp->prc_wait);
	mutex_exit(&pcp->prc_mutex);
	if (pcp->prc_flags & PRC_POLL) {
		/*
		 * We call pollwakeup() with POLLHUP to ensure that
		 * the pollers are awakened even if they are polling
		 * for nothing (i.e., waiting for the process to exit).
		 * This enables the use of the PRC_POLL flag for optimization
		 * (we can turn off PRC_POLL only if we know no pollers remain).
		 */
		pcp->prc_flags &= ~PRC_POLL;
		pollwakeup(&pcp->prc_pollhead, POLLHUP);
	}
}

/* called immediately below, in prfree() */
static void
prfreenotify(vnode_t *vp)
{
	prnode_t *pnp;
	prcommon_t *pcp;

	while (vp != NULL) {
		pnp = VTOP(vp);
		pcp = pnp->pr_common;
		ASSERT(pcp->prc_thread == NULL);
		pcp->prc_proc = NULL;
		/*
		 * We can't call prnotify() here because we are holding
		 * pidlock.  We assert that there is no need to.
		 */
		mutex_enter(&pcp->prc_mutex);
		cv_broadcast(&pcp->prc_wait);
		mutex_exit(&pcp->prc_mutex);
		ASSERT(!(pcp->prc_flags & PRC_POLL));

		vp = pnp->pr_next;
		pnp->pr_next = NULL;
	}
}

/*
 * Called from a hook in freeproc() when a traced process is removed
 * from the process table.  The proc-table pointers of all associated
 * /proc vnodes are cleared to indicate that the process has gone away.
 */
void
prfree(proc_t *p)
{
	uint_t slot = p->p_slot;

	ASSERT(MUTEX_HELD(&pidlock));

	/*
	 * Block the process against /proc so it can be freed.
	 * It cannot be freed while locked by some controlling process.
	 * Lock ordering:
	 *	pidlock -> pr_pidlock -> p->p_lock -> pcp->prc_mutex
	 */
	mutex_enter(&pr_pidlock);	/* protects pcp->prc_proc */
	mutex_enter(&p->p_lock);
	while (p->p_flag & SPRLOCK) {
		mutex_exit(&pr_pidlock);
		cv_wait(&pr_pid_cv[slot], &p->p_lock);
		mutex_exit(&p->p_lock);
		mutex_enter(&pr_pidlock);
		mutex_enter(&p->p_lock);
	}

	ASSERT(p->p_tlist == NULL);

	prfreenotify(p->p_plist);
	p->p_plist = NULL;

	prfreenotify(p->p_trace);
	p->p_trace = NULL;

	/*
	 * We broadcast to wake up everyone waiting for this process.
	 * No one can reach this process from this point on.
	 */
	cv_broadcast(&pr_pid_cv[slot]);

	mutex_exit(&p->p_lock);
	mutex_exit(&pr_pidlock);
}

/*
 * Called from a hook in exit() when a traced process is becoming a zombie.
 */
void
prexit(proc_t *p)
{
	ASSERT(MUTEX_HELD(&p->p_lock));

	if (p->p_warea) {
		pr_free_watchlist(p->p_warea);
		p->p_warea = NULL;
		p->p_nwarea = 0;
		curthread->t_proc_flag &= ~TP_WATCHPT;
		curthread->t_copyops = &default_copyops;
	}
	/* pr_free_my_pagelist() is called in exit(), after dropping p_lock */
	if (p->p_trace) {
		VTOP(p->p_trace)->pr_common->prc_flags |= PRC_DESTROY;
		prnotify(p->p_trace);
	}
	cv_broadcast(&pr_pid_cv[p->p_slot]);	/* pauselwps() */
}

/*
 * Called when an lwp is destroyed.
 * lwps either destroy themselves or a sibling destroys them.
 * The thread pointer t is not necessarily the curthread.
 */
void
prlwpexit(kthread_t *t)
{
	vnode_t *vp;
	prnode_t *pnp;
	prcommon_t *pcp;
	proc_t *p = ttoproc(t);

	ASSERT(MUTEX_HELD(&p->p_lock));
	ASSERT(p == ttoproc(curthread));

	/*
	 * The process must be blocked against /proc to do this safely.
	 * The lwp must not disappear while the process is marked SPRLOCK.
	 * It is the caller's responsibility to have called prbarrier(p).
	 */
	ASSERT(!(p->p_flag & SPRLOCK));

	for (vp = p->p_plist; vp != NULL; vp = pnp->pr_next) {
		pnp = VTOP(vp);
		pcp = pnp->pr_common;
		if (pcp->prc_thread == t)
			pcp->prc_thread = NULL;
	}

	vp = t->t_trace;
	t->t_trace = NULL;
	while (vp) {
		pnp = VTOP(vp);
		pnp->pr_common->prc_thread = NULL;
		prnotify(vp);
		vp = pnp->pr_next;
		pnp->pr_next = NULL;
	}
	if (p->p_trace)
		prnotify(p->p_trace);
}

/*
 * Called from a hook in exec() when a process starts exec().
 */
void
prexecstart()
{
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);

	/*
	 * The SPREXEC flag blocks /proc operations for
	 * the duration of the exec().
	 * We can't start exec() while the process is
	 * locked by /proc, so we call prbarrier().
	 * lwp_nostop keeps the process from being stopped
	 * via job control for the duration of the exec().
	 */

	ASSERT(MUTEX_HELD(&p->p_lock));
	prbarrier(p);
	lwp->lwp_nostop++;
	p->p_flag |= SPREXEC;
}

/*
 * Called from a hook in exec() when a process finishes exec().
 */
void
prexecend()
{
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	prcommon_t *pcp;
	vnode_t *vp;

	/*
	 * Wake up anyone waiting in /proc for the process to complete exec().
	 */
	ASSERT(MUTEX_HELD(&p->p_lock));
	lwp->lwp_nostop--;
	p->p_flag &= ~SPREXEC;
	if ((vp = p->p_trace) != NULL) {
		pcp = VTOP(vp)->pr_common;
		pcp->prc_datamodel = p->p_model;
		mutex_enter(&pcp->prc_mutex);
		cv_broadcast(&pcp->prc_wait);
		mutex_exit(&pcp->prc_mutex);
	}
	if ((vp = curthread->t_trace) != NULL) {
		/*
		 * We dealt with the process common above.
		 */
		ASSERT(p->p_trace != NULL);
		pcp = VTOP(vp)->pr_common;
		pcp->prc_datamodel = p->p_model;
		mutex_enter(&pcp->prc_mutex);
		cv_broadcast(&pcp->prc_wait);
		mutex_exit(&pcp->prc_mutex);
	}
}

/*
 * Called from a hook in relvm() just before freeing the address space.
 * We free all the watched areas now.
 */
void
prrelvm()
{
	proc_t *p = ttoproc(curthread);

	mutex_enter(&p->p_lock);
	prbarrier(p);	/* block all other /proc operations */
	if (p->p_warea) {
		pr_free_watchlist(p->p_warea);
		p->p_warea = NULL;
		p->p_nwarea = 0;
		curthread->t_proc_flag &= ~TP_WATCHPT;
		curthread->t_copyops = &default_copyops;
	}
	mutex_exit(&p->p_lock);
	if (p->p_as->a_wpage)
		pr_free_my_pagelist();
}

/*
 * Called from hooks in exec-related code when a traced process
 * attempts to exec(2) a setuid/setgid program or an unreadable
 * file.  Rather than fail the exec we invalidate the associated
 * /proc vnodes so that subsequent attempts to use them will fail.
 *
 * All /proc vnodes, except directory vnodes, are retained on a linked
 * list (rooted at p_plist in the process structure) until last close.
 *
 * A controlling process must re-open the /proc files in order to
 * regain control.
 */
void
prinvalidate(struct user *up)
{
	kthread_t *t = curthread;
	proc_t *p = ttoproc(t);
	vnode_t *vp;
	prnode_t *pnp;
	int writers = 0;

	mutex_enter(&p->p_lock);
	prbarrier(p);	/* block all other /proc operations */

	/*
	 * At this moment, there can be only one lwp in the process.
	 */
	ASSERT(p->p_tlist == t && t->t_forw == t);

	/*
	 * Invalidate any currently active /proc vnodes.
	 */
	for (vp = p->p_plist; vp != NULL; vp = pnp->pr_next) {
		pnp = VTOP(vp);
		switch (pnp->pr_type) {
		case PR_PSINFO:		/* these files can read by anyone */
		case PR_LPSINFO:
		case PR_LWPSINFO:
		case PR_LWPDIR:
		case PR_LWPIDDIR:
		case PR_USAGE:
		case PR_LUSAGE:
		case PR_LWPUSAGE:
			break;
		default:
			pnp->pr_flags |= PR_INVAL;
			break;
		}
	}
	/*
	 * Wake up anyone waiting for the process or lwp.
	 * p->p_trace is guaranteed to be non-NULL if there
	 * are any open /proc files for this process.
	 */
	if ((vp = p->p_trace) != NULL) {
		prcommon_t *pcp = VTOP(vp)->pr_pcommon;

		prnotify(vp);
		/*
		 * Are there any writers?
		 */
		if ((writers = pcp->prc_writers) != 0) {
			/*
			 * Clear the exclusive open flag (old /proc interface).
			 * Set prc_selfopens equal to prc_writers so that
			 * the next O_EXCL|O_WRITE open will succeed
			 * even with existing (though invalid) writers.
			 * prclose() must decrement prc_selfopens when
			 * the invalid files are closed.
			 */
			pcp->prc_flags &= ~PRC_EXCL;
			ASSERT(pcp->prc_selfopens <= writers);
			pcp->prc_selfopens = writers;
		}
	}
	vp = t->t_trace;
	while (vp != NULL) {
		/*
		 * We should not invalidate the lwpiddir vnodes,
		 * but the necessities of maintaining the old
		 * ioctl()-based version of /proc require it.
		 */
		pnp = VTOP(vp);
		pnp->pr_flags |= PR_INVAL;
		prnotify(vp);
		vp = pnp->pr_next;
	}

	/*
	 * If any tracing flags are in effect and any vnodes are open for
	 * writing then set the requested-stop and run-on-last-close flags.
	 * Otherwise, clear all tracing flags.
	 */
	t->t_proc_flag &= ~TP_PAUSE;
	if ((p->p_flag & SPROCTR) && writers) {
		t->t_proc_flag |= TP_PRSTOP;
		aston(t);		/* so ISSIG will see the flag */
		p->p_flag |= SRUNLCL;
	} else {
		premptyset(&up->u_entrymask);		/* syscalls */
		premptyset(&up->u_exitmask);
		up->u_systrap = 0;
		premptyset(&p->p_sigmask);		/* signals */
		premptyset(&p->p_fltmask);		/* faults */
		t->t_proc_flag &= ~(TP_PRSTOP|TP_PRVSTOP|TP_STOPPING);
		p->p_flag &= ~(SRUNLCL|SKILLCL|SPROCTR);
		prnostep(ttolwp(t));
	}

	mutex_exit(&p->p_lock);
}

/*
 * Acquire the controlled process's p_lock and mark it SPRLOCK.
 * Return with pr_pidlock held in all cases.
 * Return with p_lock held if the the process still exists.
 * Return value is the process pointer if the process still exists, else NULL.
 * If we lock the process, give ourself kernel priority to avoid deadlocks;
 * this is undone in prunlock().
 */
proc_t *
pr_p_lock(prnode_t *pnp)
{
	proc_t *p;
	prcommon_t *pcp;

	mutex_enter(&pr_pidlock);
	if ((pcp = pnp->pr_pcommon) == NULL || (p = pcp->prc_proc) == NULL)
		return (NULL);
	mutex_enter(&p->p_lock);
	while (p->p_flag & SPRLOCK) {
		/*
		 * This cv/mutex pair is persistent even if
		 * the process disappears while we sleep.
		 */
		kcondvar_t *cv = &pr_pid_cv[p->p_slot];
		kmutex_t *mp = &p->p_lock;

		mutex_exit(&pr_pidlock);
		cv_wait(cv, mp);
		mutex_exit(mp);
		mutex_enter(&pr_pidlock);
		if (pcp->prc_proc == NULL)
			return (NULL);
		ASSERT(p == pcp->prc_proc);
		mutex_enter(&p->p_lock);
	}
	p->p_flag |= SPRLOCK;
	THREAD_KPRI_REQUEST();
	return (p);
}

/*
 * Lock the target process by setting SPRLOCK and grabbing p->p_lock.
 * This prevents any lwp of the process from disappearing and
 * blocks most operations that a process can perform on itself.
 * Returns 0 on success, a non-zero error number on failure.
 *
 * 'zdisp' is ZYES or ZNO to indicate whether encountering a
 * zombie process is to be considered an error.
 *
 * error returns:
 *	ENOENT: process or lwp has disappeared
 *		(or has become a zombie and zdisp == ZNO).
 *	EAGAIN: procfs vnode has become invalid.
 *	EINTR:  signal arrived while waiting for exec to complete.
 */
int
prlock(prnode_t *pnp, int zdisp)
{
	prcommon_t *pcp;
	proc_t *p;
	kthread_t *t;

again:
	pcp = pnp->pr_common;
	p = pr_p_lock(pnp);
	mutex_exit(&pr_pidlock);

	/*
	 * Return ENOENT immediately if there is no process.
	 */
	if (p == NULL)
		return (ENOENT);

	ASSERT(p == pcp->prc_proc && p->p_stat != 0 && p->p_stat != SIDL);

	/*
	 * Return EAGAIN if we have encountered a security violation.
	 * (The process exec'd a set-id or unreadable executable file.)
	 */
	if (pnp->pr_flags & PR_INVAL) {
		prunlock(pnp);
		return (EAGAIN);
	}

	/*
	 * Return ENOENT if process entered zombie state
	 * and we are not interested in zombies.
	 */
	if (zdisp == ZNO &&
	    ((pcp->prc_flags & PRC_DESTROY) || p->p_tlist == NULL)) {
		prunlock(pnp);
		return (ENOENT);
	}

	/*
	 * If lwp-specific, check to see if lwp has disappeared.
	 */
	if (pcp->prc_flags & PRC_LWP) {
		if ((t = pcp->prc_thread) == NULL ||
		    (zdisp == ZNO && t->t_state == TS_ZOMB)) {
			prunlock(pnp);
			return (ENOENT);
		}
		ASSERT(t->t_state != TS_FREE);
		ASSERT(ttoproc(t) == p);
	}

	/*
	 * If process is undergoing an exec(), wait for
	 * completion and then start all over again.
	 */
	if (p->p_flag & SPREXEC) {
		mutex_enter(&pcp->prc_mutex);
		prunlock(pnp);
		if (!cv_wait_sig(&pcp->prc_wait, &pcp->prc_mutex)) {
			mutex_exit(&pcp->prc_mutex);
			return (EINTR);
		}
		mutex_exit(&pcp->prc_mutex);
		goto again;
	}

	/*
	 * We return holding p->p_lock.
	 */
	return (0);
}

/*
 * Undo prlock() and pr_p_lock().
 * p->p_lock is still held; pr_pidlock is no longer held.
 *
 * prunmark() drops the SPRLOCK flag and wakes up another thread,
 * if any, waiting for the flag to be dropped; it retains p->p_lock.
 *
 * prunlock() calls prunmark() and then drops p->p_lock.
 */
void
prunmark(proc_t *p)
{
	ASSERT(p->p_flag & SPRLOCK);
	ASSERT(MUTEX_HELD(&p->p_lock));

	cv_signal(&pr_pid_cv[p->p_slot]);
	p->p_flag &= ~SPRLOCK;
	THREAD_KPRI_RELEASE();
}

void
prunlock(prnode_t *pnp)
{
	proc_t *p = pnp->pr_pcommon->prc_proc;

	prunmark(p);
	mutex_exit(&p->p_lock);
}

/*
 * Called while holding p->p_lock to delay until the process is unlocked.
 * We enter holding p->p_lock; p->p_lock is dropped and reacquired.
 * The process cannot become locked again until p->p_lock is dropped.
 */
void
prbarrier(proc_t *p)
{
	ASSERT(MUTEX_HELD(&p->p_lock));

	if (p->p_flag & SPRLOCK) {
		/* The process is locked; delay until not locked */
		uint_t slot = p->p_slot;

		while (p->p_flag & SPRLOCK)
			cv_wait(&pr_pid_cv[slot], &p->p_lock);
		cv_signal(&pr_pid_cv[slot]);
	}
}

/*
 * Return process/lwp status.
 * The u-block is mapped in by this routine and unmapped at the end.
 */
void
prgetstatus(proc_t *p, pstatus_t *sp)
{
	kthread_t *t;
	kthread_t *aslwptp;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = prchoose(p);	/* returns locked thread */
	ASSERT(t != NULL);
	thread_unlock(t);

	/* just bzero the process part, prgetlwpstatus() does the rest */
	bzero(sp, sizeof (pstatus_t) - sizeof (lwpstatus_t));
	sp->pr_nlwp = p->p_lwpcnt;
	if ((aslwptp = p->p_aslwptp) != NULL) {
		k_sigset_t set;

		set = aslwptp->t_sig;
		sigorset(&set, &p->p_notifsigs);
		prassignset(&sp->pr_sigpend, &set);
	} else {
		prassignset(&sp->pr_sigpend, &p->p_sig);
	}
	sp->pr_brkbase = (uintptr_t)p->p_brkbase;
	sp->pr_brksize = p->p_brksize;
	sp->pr_stkbase = (uintptr_t)prgetstackbase(p);
	sp->pr_stksize = p->p_stksize;
	sp->pr_pid   = p->p_pid;
	sp->pr_ppid  = p->p_ppid;
	sp->pr_pgid  = p->p_pgrp;
	sp->pr_sid   = p->p_sessp->s_sid;
	sp->pr_reserved[0] = -1;
	sp->pr_reserved[1] = -1;
	TICK_TO_TIMESTRUC(p->p_utime, &sp->pr_utime);
	TICK_TO_TIMESTRUC(p->p_stime, &sp->pr_stime);
	TICK_TO_TIMESTRUC(p->p_cutime, &sp->pr_cutime);
	TICK_TO_TIMESTRUC(p->p_cstime, &sp->pr_cstime);
	prassignset(&sp->pr_sigtrace, &p->p_sigmask);
	prassignset(&sp->pr_flttrace, &p->p_fltmask);
	prassignset(&sp->pr_sysentry, &PTOU(p)->u_entrymask);
	prassignset(&sp->pr_sysexit, &PTOU(p)->u_exitmask);
	switch (p->p_model) {
	case DATAMODEL_ILP32:
		sp->pr_dmodel = PR_MODEL_ILP32;
		break;
	case DATAMODEL_LP64:
		sp->pr_dmodel = PR_MODEL_LP64;
		break;
	}
	if (p->p_aslwptp)
		sp->pr_aslwpid = p->p_aslwptp->t_tid;
	if (p->p_agenttp)
		sp->pr_agentid = p->p_agenttp->t_tid;

	/* get the chosen lwp's status */
	prgetlwpstatus(t, &sp->pr_lwp);

	/* replicate the flags */
	sp->pr_flags = sp->pr_lwp.pr_flags;
}

#ifdef _SYSCALL32_IMPL
void
prgetlwpstatus32(kthread_t *t, lwpstatus32_t *sp)
{
	proc_t *p = ttoproc(t);
	klwp_t *lwp = ttolwp(t);
	int flags;
	ulong_t instr;

	ASSERT(MUTEX_HELD(&p->p_lock));

	bzero(sp, sizeof (*sp));
	flags = 0L;
	if (t->t_state == TS_STOPPED) {
		flags |= PR_STOPPED;
		if ((t->t_schedflag & TS_PSTART) == 0)
			flags |= PR_ISTOP;
	} else if (VSTOPPED(t)) {
		flags |= PR_STOPPED|PR_ISTOP;
	}
	if (!(flags & PR_ISTOP) && (t->t_proc_flag & TP_PRSTOP))
		flags |= PR_DSTOP;
	if (lwp->lwp_asleep)
		flags |= PR_ASLEEP;
	if (t == p->p_aslwptp)
		flags |= PR_ASLWP;
	if (t == p->p_agenttp)
		flags |= PR_AGENT;
	if (p->p_flag & SPRFORK)
		flags |= PR_FORK;
	if (p->p_flag & SRUNLCL)
		flags |= PR_RLC;
	if (p->p_flag & SKILLCL)
		flags |= PR_KLC;
	if (p->p_flag & SPASYNC)
		flags |= PR_ASYNC;
	if (p->p_flag & SBPTADJ)
		flags |= PR_BPTADJ;
	if (p->p_flag & STRC)
		flags |= PR_PTRACE;
	if (p->p_flag & SMSACCT)
		flags |= PR_MSACCT;
	if (p->p_flag & SMSFORK)
		flags |= PR_MSFORK;
	if (p->p_flag & SVFWAIT)
		flags |= PR_VFORKP;
	sp->pr_flags = flags;
	if (VSTOPPED(t)) {
		sp->pr_why   = PR_REQUESTED;
		sp->pr_what  = 0;
	} else {
		sp->pr_why   = t->t_whystop;
		sp->pr_what  = t->t_whatstop;
	}
	sp->pr_lwpid = t->t_tid;
	sp->pr_cursig  = lwp->lwp_cursig;
	prassignset(&sp->pr_lwppend, &t->t_sig);
	prassignset(&sp->pr_lwphold, &t->t_hold);
	if (t->t_whystop == PR_FAULTED)
		siginfo_kto32(&lwp->lwp_siginfo, &sp->pr_info);
	else if (lwp->lwp_curinfo)
		siginfo_kto32(&lwp->lwp_curinfo->sq_info, &sp->pr_info);
	sp->pr_altstack.ss_sp = (caddr32_t)lwp->lwp_sigaltstack.ss_sp;
	sp->pr_altstack.ss_size = (size32_t)lwp->lwp_sigaltstack.ss_size;
	sp->pr_altstack.ss_flags = (int32_t)lwp->lwp_sigaltstack.ss_flags;
	prgetaction32(p, PTOU(p), lwp->lwp_cursig, &sp->pr_action);
	sp->pr_oldcontext = (caddr32_t)lwp->lwp_oldcontext;
	(void) strncpy(sp->pr_clname, sclass[t->t_cid].cl_name,
		sizeof (sp->pr_clname) - 1);
	if (flags & PR_STOPPED)
		hrt2ts32(t->t_stoptime, &sp->pr_tstamp);

	/*
	 * Fetch the current instruction, if not a system process.
	 * We don't attempt this unless the lwp is stopped.
	 */
	if ((p->p_flag & SSYS) || p->p_as == &kas)
		sp->pr_flags |= (PR_ISSYS|PR_PCINVAL);
	else if (!(flags & PR_STOPPED))
		sp->pr_flags |= PR_PCINVAL;
	else if (!prfetchinstr(lwp, &instr))
		sp->pr_flags |= PR_PCINVAL;
	else
		sp->pr_instr = (uint32_t)instr;

	/*
	 * Drop p_lock while touching the lwp's stack.
	 */
	mutex_exit(&p->p_lock);
	if (prisstep(lwp))
		sp->pr_flags |= PR_STEP;
	if ((flags & (PR_STOPPED|PR_ASLEEP)) && t->t_sysnum) {
		int i;

		sp->pr_syscall = get_syscall32_args(lwp,
			(int *)sp->pr_sysarg, &i);
		sp->pr_nsysarg = (ushort_t)i;
	}
	if (flags & PR_VFORKP) {
		sp->pr_syscall = SYS_vfork;
		sp->pr_nsysarg = 0;
	}
	prgetprregs32(lwp, sp->pr_reg);
	if ((t->t_state == TS_STOPPED && t->t_whystop == PR_SYSEXIT) ||
	    (flags & PR_VFORKP)) {
		long r1, r2;
		user_t *up;
		auxv_t *auxp;
		int i;

		sp->pr_errno = prgetrvals(lwp, &r1, &r2);
		if (sp->pr_errno == 0) {
			sp->pr_rval1 = (int32_t)r1;
			sp->pr_rval2 = (int32_t)r2;
		}
		if (t->t_sysnum == SYS_exec || t->t_sysnum == SYS_execve) {
			up = prumap(p);
			sp->pr_sysarg[0] = 0;
			sp->pr_sysarg[1] = (caddr32_t)up->u_argv;
			sp->pr_sysarg[2] = (caddr32_t)up->u_envp;
			for (i = 0, auxp = up->u_auxv;
			    i < sizeof (up->u_auxv) / sizeof (up->u_auxv[0]);
			    i++, auxp++) {
				if (auxp->a_type == AT_SUN_EXECNAME) {
					sp->pr_sysarg[0] =
						(caddr32_t)auxp->a_un.a_ptr;
					break;
				}
			}
			prunmap(p);
		}
	}
	if (prhasfp())
		prgetprfpregs32(lwp, &sp->pr_fpreg);
	mutex_enter(&p->p_lock);
}

void
prgetstatus32(proc_t *p, pstatus32_t *sp)
{
	kthread_t *t;
	kthread_t *aslwptp;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = prchoose(p);	/* returns locked thread */
	ASSERT(t != NULL);
	thread_unlock(t);

	/* just bzero the process part, prgetlwpstatus32() does the rest */
	bzero(sp, sizeof (pstatus32_t) - sizeof (lwpstatus32_t));
	sp->pr_nlwp = p->p_lwpcnt;
	if ((aslwptp = p->p_aslwptp) != NULL) {
		k_sigset_t set;

		set = aslwptp->t_sig;
		sigorset(&set, &p->p_notifsigs);
		prassignset(&sp->pr_sigpend, &set);
	} else {
		prassignset(&sp->pr_sigpend, &p->p_sig);
	}
	sp->pr_brkbase = (uint32_t)p->p_brkbase;
	sp->pr_brksize = (uint32_t)p->p_brksize;
	sp->pr_stkbase = (uint32_t)prgetstackbase(p);
	sp->pr_stksize = (uint32_t)p->p_stksize;
	sp->pr_pid   = p->p_pid;
	sp->pr_ppid  = p->p_ppid;
	sp->pr_pgid  = p->p_pgrp;
	sp->pr_sid   = p->p_sessp->s_sid;
	sp->pr_reserved[0] = -1;
	sp->pr_reserved[1] = -1;
	TICK_TO_TIMESTRUC32(p->p_utime, &sp->pr_utime);
	TICK_TO_TIMESTRUC32(p->p_stime, &sp->pr_stime);
	TICK_TO_TIMESTRUC32(p->p_cutime, &sp->pr_cutime);
	TICK_TO_TIMESTRUC32(p->p_cstime, &sp->pr_cstime);
	prassignset(&sp->pr_sigtrace, &p->p_sigmask);
	prassignset(&sp->pr_flttrace, &p->p_fltmask);
	prassignset(&sp->pr_sysentry, &PTOU(p)->u_entrymask);
	prassignset(&sp->pr_sysexit, &PTOU(p)->u_exitmask);
	switch (p->p_model) {
	case DATAMODEL_ILP32:
		sp->pr_dmodel = PR_MODEL_ILP32;
		break;
	case DATAMODEL_LP64:
		sp->pr_dmodel = PR_MODEL_LP64;
		break;
	}
	if (p->p_aslwptp)
		sp->pr_aslwpid = p->p_aslwptp->t_tid;
	if (p->p_agenttp)
		sp->pr_agentid = p->p_agenttp->t_tid;

	/* get the chosen lwp's status */
	prgetlwpstatus32(t, &sp->pr_lwp);

	/* replicate the flags */
	sp->pr_flags = sp->pr_lwp.pr_flags;
}
#endif	/* _SYSCALL32_IMPL */

/*
 * Return lwp status.
 */
void
prgetlwpstatus(kthread_t *t, lwpstatus_t *sp)
{
	proc_t *p = ttoproc(t);
	klwp_t *lwp = ttolwp(t);
	int flags;
	ulong_t instr;

	ASSERT(MUTEX_HELD(&p->p_lock));

	bzero(sp, sizeof (*sp));
	flags = 0L;
	if (t->t_state == TS_STOPPED) {
		flags |= PR_STOPPED;
		if ((t->t_schedflag & TS_PSTART) == 0)
			flags |= PR_ISTOP;
	} else if (VSTOPPED(t)) {
		flags |= PR_STOPPED|PR_ISTOP;
	}
	if (!(flags & PR_ISTOP) && (t->t_proc_flag & TP_PRSTOP))
		flags |= PR_DSTOP;
	if (lwp->lwp_asleep)
		flags |= PR_ASLEEP;
	if (t == p->p_aslwptp)
		flags |= PR_ASLWP;
	if (t == p->p_agenttp)
		flags |= PR_AGENT;
	if (p->p_flag & SPRFORK)
		flags |= PR_FORK;
	if (p->p_flag & SRUNLCL)
		flags |= PR_RLC;
	if (p->p_flag & SKILLCL)
		flags |= PR_KLC;
	if (p->p_flag & SPASYNC)
		flags |= PR_ASYNC;
	if (p->p_flag & SBPTADJ)
		flags |= PR_BPTADJ;
	if (p->p_flag & STRC)
		flags |= PR_PTRACE;
	if (p->p_flag & SMSACCT)
		flags |= PR_MSACCT;
	if (p->p_flag & SMSFORK)
		flags |= PR_MSFORK;
	if (p->p_flag & SVFWAIT)
		flags |= PR_VFORKP;
	if (p->p_pgidp->pid_pgorphaned)
		flags |= PR_ORPHAN;
	sp->pr_flags = flags;
	if (VSTOPPED(t)) {
		sp->pr_why   = PR_REQUESTED;
		sp->pr_what  = 0;
	} else {
		sp->pr_why   = t->t_whystop;
		sp->pr_what  = t->t_whatstop;
	}
	sp->pr_lwpid = t->t_tid;
	sp->pr_cursig  = lwp->lwp_cursig;
	prassignset(&sp->pr_lwppend, &t->t_sig);
	prassignset(&sp->pr_lwphold, &t->t_hold);
	if (t->t_whystop == PR_FAULTED)
		bcopy(&lwp->lwp_siginfo,
		    &sp->pr_info, sizeof (k_siginfo_t));
	else if (lwp->lwp_curinfo)
		bcopy(&lwp->lwp_curinfo->sq_info,
		    &sp->pr_info, sizeof (k_siginfo_t));
	sp->pr_altstack = lwp->lwp_sigaltstack;
	prgetaction(p, PTOU(p), lwp->lwp_cursig, &sp->pr_action);
	sp->pr_oldcontext = (uintptr_t)lwp->lwp_oldcontext;
	(void) strncpy(sp->pr_clname, sclass[t->t_cid].cl_name,
		sizeof (sp->pr_clname) - 1);
	if (flags & PR_STOPPED)
		hrt2ts(t->t_stoptime, &sp->pr_tstamp);

	/*
	 * Fetch the current instruction, if not a system process.
	 * We don't attempt this unless the lwp is stopped.
	 */
	if ((p->p_flag & SSYS) || p->p_as == &kas)
		sp->pr_flags |= (PR_ISSYS|PR_PCINVAL);
	else if (!(flags & PR_STOPPED))
		sp->pr_flags |= PR_PCINVAL;
	else if (!prfetchinstr(lwp, &instr))
		sp->pr_flags |= PR_PCINVAL;
	else
		sp->pr_instr = instr;

	/*
	 * Drop p_lock while touching the lwp's stack.
	 */
	mutex_exit(&p->p_lock);
	if (prisstep(lwp))
		sp->pr_flags |= PR_STEP;
	if ((flags & (PR_STOPPED|PR_ASLEEP)) && t->t_sysnum) {
		int i;

		sp->pr_syscall = get_syscall_args(lwp,
			(long *)sp->pr_sysarg, &i);
		sp->pr_nsysarg = (ushort_t)i;
	}
	if (flags & PR_VFORKP) {
		sp->pr_syscall = SYS_vfork;
		sp->pr_nsysarg = 0;
	}
	prgetprregs(lwp, sp->pr_reg);
	if ((t->t_state == TS_STOPPED && t->t_whystop == PR_SYSEXIT) ||
	    (flags & PR_VFORKP)) {
		user_t *up;
		auxv_t *auxp;
		int i;

		sp->pr_errno = prgetrvals(lwp, &sp->pr_rval1, &sp->pr_rval2);
		if (t->t_sysnum == SYS_exec || t->t_sysnum == SYS_execve) {
			up = prumap(p);
			sp->pr_sysarg[0] = 0;
			sp->pr_sysarg[1] = (uintptr_t)up->u_argv;
			sp->pr_sysarg[2] = (uintptr_t)up->u_envp;
			for (i = 0, auxp = up->u_auxv;
			    i < sizeof (up->u_auxv) / sizeof (up->u_auxv[0]);
			    i++, auxp++) {
				if (auxp->a_type == AT_SUN_EXECNAME) {
					sp->pr_sysarg[0] =
						(uintptr_t)auxp->a_un.a_ptr;
					break;
				}
			}
			prunmap(p);
		}
	}
	if (prhasfp())
		prgetprfpregs(lwp, &sp->pr_fpreg);
	mutex_enter(&p->p_lock);
}

/*
 * Get the sigaction structure for the specified signal.  The u-block
 * must already have been mapped in by the caller.
 */
void
prgetaction(proc_t *p, user_t *up, uint_t sig, struct sigaction *sp)
{
	bzero(sp, sizeof (*sp));

	if (sig != 0 && (unsigned)sig < NSIG) {
		sp->sa_handler = up->u_signal[sig-1];
		prassignset(&sp->sa_mask, &up->u_sigmask[sig-1]);
		if (sigismember(&up->u_sigonstack, sig))
			sp->sa_flags |= SA_ONSTACK;
		if (sigismember(&up->u_sigresethand, sig))
			sp->sa_flags |= SA_RESETHAND;
		if (sigismember(&up->u_sigrestart, sig))
			sp->sa_flags |= SA_RESTART;
		if (sigismember(&p->p_siginfo, sig))
			sp->sa_flags |= SA_SIGINFO;
		if (sigismember(&up->u_signodefer, sig))
			sp->sa_flags |= SA_NODEFER;
		switch (sig) {
		case SIGCLD:
			if (p->p_flag & SNOWAIT)
				sp->sa_flags |= SA_NOCLDWAIT;
			if ((p->p_flag & SJCTL) == 0)
				sp->sa_flags |= SA_NOCLDSTOP;
			break;
		case SIGWAITING:
			if (p->p_flag & SWAITSIG)
				sp->sa_flags |= SA_WAITSIG;
			break;
		}
	}
}

#ifdef _SYSCALL32_IMPL
void
prgetaction32(proc_t *p, user_t *up, uint_t sig, struct sigaction32 *sp)
{
	bzero(sp, sizeof (*sp));

	if (sig != 0 && (unsigned)sig < NSIG) {
		sp->sa_handler = (caddr32_t)up->u_signal[sig-1];
		prassignset(&sp->sa_mask, &up->u_sigmask[sig-1]);
		if (sigismember(&up->u_sigonstack, sig))
			sp->sa_flags |= SA_ONSTACK;
		if (sigismember(&up->u_sigresethand, sig))
			sp->sa_flags |= SA_RESETHAND;
		if (sigismember(&up->u_sigrestart, sig))
			sp->sa_flags |= SA_RESTART;
		if (sigismember(&p->p_siginfo, sig))
			sp->sa_flags |= SA_SIGINFO;
		if (sigismember(&up->u_signodefer, sig))
			sp->sa_flags |= SA_NODEFER;
		switch (sig) {
		case SIGCLD:
			if (p->p_flag & SNOWAIT)
				sp->sa_flags |= SA_NOCLDWAIT;
			if ((p->p_flag & SJCTL) == 0)
				sp->sa_flags |= SA_NOCLDSTOP;
			break;
		case SIGWAITING:
			if (p->p_flag & SWAITSIG)
				sp->sa_flags |= SA_WAITSIG;
			break;
		}
	}
}
#endif	/* _SYSCALL32_IMPL */

/*
 * Count the number of segments in this process's address space.
 */
int
prnsegs(struct as *as, int reserved)
{
	int n = 0;
	struct seg *seg;

	ASSERT(as != &kas && AS_WRITE_HELD(as, &as->a_lock));

	for (seg = AS_SEGP(as, as->a_segs); seg != NULL;
	    seg = AS_SEGP(as, seg->s_next)) {
		caddr_t eaddr = seg->s_base + pr_getsegsize(seg, reserved);
		caddr_t saddr, naddr;
		void *tmp = NULL;

		for (saddr = seg->s_base; saddr < eaddr; saddr = naddr) {
			(void) pr_getprot(seg, reserved, &tmp,
			    &saddr, &naddr, eaddr);
			if (saddr != naddr)
				n++;
		}

		ASSERT(tmp == NULL);
	}

	return (n);
}

/*
 * Convert uint32_t to decimal string w/o leading zeros.
 * Add trailing null characters if 'len' is greater than string length.
 * Return the string length.
 */
int
pr_u32tos(uint32_t n, char *s, int len)
{
	char cbuf[11];		/* 32-bit unsigned integer fits in 10 digits */
	char *cp = cbuf;
	char *end = s + len;

	do {
		*cp++ = (char)(n % 10 + '0');
		n /= 10;
	} while (n);

	len = (int)(cp - cbuf);

	do {
		*s++ = *--cp;
	} while (cp > cbuf);

	while (s < end)		/* optional pad */
		*s++ = '\0';

	return (len);
}

/*
 * Convert uint64_t to decimal string w/o leading zeros.
 * Return the string length.
 */
static int
pr_u64tos(uint64_t n, char *s)
{
	char cbuf[21];		/* 64-bit unsigned integer fits in 20 digits */
	char *cp = cbuf;
	int len;

	do {
		*cp++ = (char)(n % 10 + '0');
		n /= 10;
	} while (n);

	len = (int)(cp - cbuf);

	do {
		*s++ = *--cp;
	} while (cp > cbuf);

	return (len);
}

void
pr_object_name(char *name, vnode_t *vp, struct vattr *vattr)
{
	char *s = name;
	struct vfs *vfsp;
	struct vfssw *vfsswp;

	if ((vfsp = vp->v_vfsp) != NULL &&
	    ((vfsswp = vfssw + vfsp->vfs_fstype), vfsswp->vsw_name) &&
	    *vfsswp->vsw_name) {
		(void) strcpy(s, vfsswp->vsw_name);
		s += strlen(s);
		*s++ = '.';
	}
	s += pr_u32tos(getmajor(vattr->va_fsid), s, 0);
	*s++ = '.';
	s += pr_u32tos(getminor(vattr->va_fsid), s, 0);
	*s++ = '.';
	s += pr_u64tos(vattr->va_nodeid, s);
	*s++ = '\0';
}

struct seg *
break_seg(proc_t *p)
{
	caddr_t addr = p->p_brkbase;
	struct seg *seg;
	struct vnode *vp;

	if (p->p_brksize != 0)
		addr += p->p_brksize - 1;
	seg = as_segat(p->p_as, addr);
	if (seg != NULL && seg->s_ops == &segvn_ops &&
	    (SEGOP_GETVP(seg, seg->s_base, &vp) != 0 || vp == NULL))
		return (seg);
	return (NULL);
}

/*
 * Return an array of structures with memory map information.
 * We allocate here; the caller must deallocate.
 */
#define	MAPSIZE	8192
int
prgetmap(proc_t *p, int reserved, prmap_t **prmapp, size_t *sizep)
{
	struct as *as = p->p_as;
	int nmaps = 0;
	prmap_t *mp;
	size_t size;
	struct seg *seg;
	struct seg *brkseg, *stkseg;
	struct vnode *vp;
	struct vattr vattr;
	uint_t prot;

	ASSERT(as != &kas && AS_WRITE_HELD(as, &as->a_lock));

	/* initial allocation */
	*sizep = size = MAPSIZE;
	*prmapp = mp = kmem_alloc(MAPSIZE, KM_SLEEP);

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL)
		return (0);

	brkseg = break_seg(p);
	stkseg = as_segat(as, prgetstackbase(p));

	do {
		caddr_t eaddr = seg->s_base + pr_getsegsize(seg, reserved);
		caddr_t saddr, naddr;
		void *tmp = NULL;

		for (saddr = seg->s_base; saddr < eaddr; saddr = naddr) {
			prot = pr_getprot(seg, reserved, &tmp,
			    &saddr, &naddr, eaddr);
			if (saddr == naddr)
				continue;
			/* reallocate if necessary */
			if ((nmaps + 1) * sizeof (prmap_t) > size) {
				size_t newsize = size + MAPSIZE;
				prmap_t *newmp = kmem_alloc(newsize, KM_SLEEP);

				bcopy(*prmapp, newmp, nmaps * sizeof (prmap_t));
				kmem_free(*prmapp, size);
				*sizep = size = newsize;
				*prmapp = newmp;
				mp = newmp + nmaps;
			}
			bzero(mp, sizeof (*mp));
			mp->pr_vaddr = (uintptr_t)saddr;
			mp->pr_size = naddr - saddr;
			mp->pr_offset = SEGOP_GETOFFSET(seg, saddr);
			mp->pr_mflags = 0;
			if (prot & PROT_READ)
				mp->pr_mflags |= MA_READ;
			if (prot & PROT_WRITE)
				mp->pr_mflags |= MA_WRITE;
			if (prot & PROT_EXEC)
				mp->pr_mflags |= MA_EXEC;
			if (SEGOP_GETTYPE(seg, saddr) == MAP_SHARED)
				mp->pr_mflags |= MA_SHARED;
			if (seg->s_ops == &segspt_shmops ||
			    (seg->s_ops == &segvn_ops &&
			    (SEGOP_GETVP(seg, saddr, &vp) != 0 || vp == NULL)))
				mp->pr_mflags |= MA_ANON;
			if (seg == brkseg)
				mp->pr_mflags |= MA_BREAK;
			else if (seg == stkseg) {
				mp->pr_mflags |= MA_STACK;
				if (reserved) {
					size_t maxstack =
					    ((size_t)P_CURLIMIT(p,
					    RLIMIT_STACK) + PAGEOFFSET) &
					    PAGEMASK;
					prunmap(p);
					mp->pr_vaddr =
					    (uintptr_t)prgetstackbase(p) +
					    p->p_stksize - maxstack;
					mp->pr_size = (uintptr_t)naddr -
					    mp->pr_vaddr;
				}
			}
			if (seg->s_ops == &segspt_shmops)
				mp->pr_mflags |= MA_ISM;
			mp->pr_pagesize = PAGESIZE;

			/*
			 * Manufacture a filename for the "object" directory.
			 */
			vattr.va_mask = AT_FSID|AT_NODEID;
			if (seg->s_ops == &segvn_ops &&
			    SEGOP_GETVP(seg, saddr, &vp) == 0 &&
			    vp != NULL && vp->v_type == VREG &&
			    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {
				if (vp == p->p_exec)
					(void) strcpy(mp->pr_mapname, "a.out");
				else
					pr_object_name(mp->pr_mapname,
						vp, &vattr);
			}

			/*
			 * Get the SysV shared memory id, if any.
			 */
			if ((mp->pr_mflags & MA_SHARED) && p->p_segacct)
				mp->pr_shmid = shmgetid(p, saddr);
			else
				mp->pr_shmid = -1;

			mp++;
			nmaps++;
		}
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	return (nmaps);
}

#ifdef _SYSCALL32_IMPL
int
prgetmap32(proc_t *p, int reserved, prmap32_t **prmapp, size_t *sizep)
{
	struct as *as = p->p_as;
	int nmaps = 0;
	prmap32_t *mp;
	size_t size;
	struct seg *seg;
	struct seg *brkseg, *stkseg;
	struct vnode *vp;
	struct vattr vattr;
	uint_t prot;

	ASSERT(as != &kas && AS_WRITE_HELD(as, &as->a_lock));

	/* initial allocation */
	*sizep = size = MAPSIZE;
	*prmapp = mp = kmem_alloc(MAPSIZE, KM_SLEEP);

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL)
		return (0);

	brkseg = break_seg(p);
	stkseg = as_segat(as, prgetstackbase(p));

	do {
		caddr_t eaddr = seg->s_base + pr_getsegsize(seg, reserved);
		caddr_t saddr, naddr;
		void *tmp = NULL;

		for (saddr = seg->s_base; saddr < eaddr; saddr = naddr) {
			prot = pr_getprot(seg, reserved, &tmp,
			    &saddr, &naddr, eaddr);
			if (saddr == naddr)
				continue;
			/* reallocate if necessary */
			if ((nmaps + 1) * sizeof (prmap32_t) > size) {
				size_t newsize = size + MAPSIZE;
				prmap32_t *newmp =
					kmem_alloc(newsize, KM_SLEEP);

				bcopy(*prmapp, newmp,
					nmaps * sizeof (prmap32_t));
				kmem_free(*prmapp, size);
				*sizep = size = newsize;
				*prmapp = newmp;
				mp = newmp + nmaps;
			}
			bzero(mp, sizeof (*mp));
			mp->pr_vaddr = (caddr32_t)saddr;
			mp->pr_size = (size32_t)(naddr - saddr);
			mp->pr_offset = SEGOP_GETOFFSET(seg, saddr);
			mp->pr_mflags = 0;
			if (prot & PROT_READ)
				mp->pr_mflags |= MA_READ;
			if (prot & PROT_WRITE)
				mp->pr_mflags |= MA_WRITE;
			if (prot & PROT_EXEC)
				mp->pr_mflags |= MA_EXEC;
			if (SEGOP_GETTYPE(seg, saddr) == MAP_SHARED)
				mp->pr_mflags |= MA_SHARED;
			if (seg->s_ops == &segspt_shmops ||
			    (seg->s_ops == &segvn_ops &&
			    (SEGOP_GETVP(seg, saddr, &vp) != 0 || vp == NULL)))
				mp->pr_mflags |= MA_ANON;
			if (seg == brkseg)
				mp->pr_mflags |= MA_BREAK;
			else if (seg == stkseg) {
				mp->pr_mflags |= MA_STACK;
				if (reserved) {
					size_t maxstack =
					    ((size_t)P_CURLIMIT32(p,
					    RLIMIT_STACK) + PAGEOFFSET) &
					    PAGEMASK;
					uintptr_t vaddr =
					    (uintptr_t)prgetstackbase(p) +
					    p->p_stksize - maxstack;
					prunmap(p);
					mp->pr_vaddr = (caddr32_t)vaddr;
					mp->pr_size = (size32_t)
					    ((uintptr_t)naddr - vaddr);
				}
			}
			if (seg->s_ops == &segspt_shmops)
				mp->pr_mflags |= MA_ISM;
			mp->pr_pagesize = PAGESIZE;

			/*
			 * Manufacture a filename for the "object" directory.
			 */
			vattr.va_mask = AT_FSID|AT_NODEID;
			if (seg->s_ops == &segvn_ops &&
			    SEGOP_GETVP(seg, saddr, &vp) == 0 &&
			    vp != NULL && vp->v_type == VREG &&
			    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {
				if (vp == p->p_exec)
					(void) strcpy(mp->pr_mapname, "a.out");
				else
					pr_object_name(mp->pr_mapname,
						vp, &vattr);
			}

			/*
			 * Get the SysV shared memory id, if any.
			 */
			if ((mp->pr_mflags & MA_SHARED) && p->p_segacct)
				mp->pr_shmid = shmgetid(p, saddr);
			else
				mp->pr_shmid = -1;

			mp++;
			nmaps++;
		}
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	return (nmaps);
}
#endif	/* _SYSCALL32_IMPL */

/*
 * Return the size of the /proc page data file.
 */
size_t
prpdsize(struct as *as)
{
	struct seg *seg;
	size_t size;

	ASSERT(as != &kas && AS_WRITE_HELD(as, &as->a_lock));

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL)
		return (0);

	size = sizeof (prpageheader_t);
	do {
		caddr_t eaddr = seg->s_base + pr_getsegsize(seg, 0);
		caddr_t saddr, naddr;
		void *tmp = NULL;
		size_t npage;

		for (saddr = seg->s_base; saddr < eaddr; saddr = naddr) {
			(void) pr_getprot(seg, 0, &tmp, &saddr, &naddr, eaddr);
			if ((npage = (naddr - saddr) / PAGESIZE) != 0)
				size += sizeof (prasmap_t) + round8(npage);
		}
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	return (size);
}

#ifdef _SYSCALL32_IMPL
size_t
prpdsize32(struct as *as)
{
	struct seg *seg;
	size_t size;

	ASSERT(as != &kas && AS_WRITE_HELD(as, &as->a_lock));

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL)
		return (0);

	size = sizeof (prpageheader32_t);
	do {
		caddr_t eaddr = seg->s_base + pr_getsegsize(seg, 0);
		caddr_t saddr, naddr;
		void *tmp = NULL;
		size_t npage;

		for (saddr = seg->s_base; saddr < eaddr; saddr = naddr) {
			(void) pr_getprot(seg, 0, &tmp, &saddr, &naddr, eaddr);
			if ((npage = (naddr - saddr) / PAGESIZE) != 0)
				size += sizeof (prasmap32_t) + round8(npage);
		}
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	return (size);
}
#endif	/* _SYSCALL32_IMPL */

/*
 * Read page data information.
 * The address space is locked and will not change.
 */
int
prpdread(proc_t *p, uint_t hatid, struct uio *uiop)
{
	struct as *as = p->p_as;
	caddr_t buf;
	size_t size;
	prpageheader_t *php;
	prasmap_t *pmp;
	struct seg *seg;
	int error;

	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL) {
		AS_LOCK_EXIT(as, &as->a_lock);
		return (0);
	}
	size = prpdsize(as);
	if (uiop->uio_resid < size) {
		AS_LOCK_EXIT(as, &as->a_lock);
		return (E2BIG);
	}

	buf = kmem_zalloc(size, KM_SLEEP);
	php = (prpageheader_t *)buf;
	pmp = (prasmap_t *)(buf + sizeof (prpageheader_t));

	hrt2ts(gethrtime(), &php->pr_tstamp);
	php->pr_nmap = 0;
	php->pr_npage = 0;
	do {
		caddr_t eaddr = seg->s_base + pr_getsegsize(seg, 0);
		caddr_t saddr, naddr;
		void *tmp = NULL;

		for (saddr = seg->s_base; saddr < eaddr; saddr = naddr) {
			struct vnode *vp;
			struct vattr vattr;
			size_t len;
			size_t npage;
			uint_t prot;

			prot = pr_getprot(seg, 0, &tmp, &saddr, &naddr, eaddr);
			if ((len = (size_t)(naddr - saddr)) == 0)
				continue;
			npage = len / PAGESIZE;
			ASSERT(npage != 0);
			php->pr_nmap++;
			php->pr_npage += npage;
			bzero(pmp, sizeof (*pmp));
			pmp->pr_vaddr = (uintptr_t)saddr;
			pmp->pr_npage = npage;
			pmp->pr_offset = SEGOP_GETOFFSET(seg, saddr);
			pmp->pr_mflags = 0;
			if (prot & PROT_READ)
				pmp->pr_mflags |= MA_READ;
			if (prot & PROT_WRITE)
				pmp->pr_mflags |= MA_WRITE;
			if (prot & PROT_EXEC)
				pmp->pr_mflags |= MA_EXEC;
			if (SEGOP_GETTYPE(seg, saddr) == MAP_SHARED)
				pmp->pr_mflags |= MA_SHARED;
			if (seg->s_ops == &segspt_shmops ||
			    (seg->s_ops == &segvn_ops &&
			    (SEGOP_GETVP(seg, saddr, &vp) != 0 || vp == NULL)))
				pmp->pr_mflags |= MA_ANON;
			if (seg->s_ops == &segspt_shmops)
				pmp->pr_mflags |= MA_ISM;
			pmp->pr_pagesize = PAGESIZE;
			/*
			 * Manufacture a filename for the "object" directory.
			 */
			vattr.va_mask = AT_FSID|AT_NODEID;
			if (seg->s_ops == &segvn_ops &&
			    SEGOP_GETVP(seg, saddr, &vp) == 0 &&
			    vp != NULL && vp->v_type == VREG &&
			    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {
				if (vp == p->p_exec)
					(void) strcpy(pmp->pr_mapname, "a.out");
				else
					pr_object_name(pmp->pr_mapname,
						vp, &vattr);
			}

			/*
			 * Get the SysV shared memory id, if any.
			 */
			if ((pmp->pr_mflags & MA_SHARED) && p->p_segacct)
				pmp->pr_shmid = shmgetid(p, saddr);
			else
				pmp->pr_shmid = -1;

			hat_getstat(as, saddr, len, hatid,
			    (char *)(pmp+1), HAT_SYNC_ZERORM);
			pmp = (prasmap_t *)((caddr_t)(pmp+1) + round8(npage));
		}
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	AS_LOCK_EXIT(as, &as->a_lock);

	ASSERT((caddr_t)pmp == buf+size);
	error = uiomove(buf, size, UIO_READ, uiop);
	kmem_free(buf, size);

	return (error);
}

#ifdef _SYSCALL32_IMPL
int
prpdread32(proc_t *p, uint_t hatid, struct uio *uiop)
{
	struct as *as = p->p_as;
	caddr_t buf;
	size_t size;
	prpageheader32_t *php;
	prasmap32_t *pmp;
	struct seg *seg;
	int error;

	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL) {
		AS_LOCK_EXIT(as, &as->a_lock);
		return (0);
	}
	size = prpdsize32(as);
	if (uiop->uio_resid < size) {
		AS_LOCK_EXIT(as, &as->a_lock);
		return (E2BIG);
	}

	buf = kmem_zalloc(size, KM_SLEEP);
	php = (prpageheader32_t *)buf;
	pmp = (prasmap32_t *)(buf + sizeof (prpageheader32_t));

	hrt2ts32(gethrtime(), &php->pr_tstamp);
	php->pr_nmap = 0;
	php->pr_npage = 0;
	do {
		caddr_t eaddr = seg->s_base + pr_getsegsize(seg, 0);
		caddr_t saddr, naddr;
		void *tmp = NULL;

		for (saddr = seg->s_base; saddr < eaddr; saddr = naddr) {
			struct vnode *vp;
			struct vattr vattr;
			size_t len;
			size_t npage;
			uint_t prot;

			prot = pr_getprot(seg, 0, &tmp, &saddr, &naddr, eaddr);
			if ((len = (size_t)(naddr - saddr)) == 0)
				continue;
			npage = len / PAGESIZE;
			ASSERT(npage != 0);
			php->pr_nmap++;
			php->pr_npage += npage;
			bzero(pmp, sizeof (*pmp));
			pmp->pr_vaddr = (caddr32_t)saddr;
			pmp->pr_npage = (size32_t)npage;
			pmp->pr_offset = SEGOP_GETOFFSET(seg, saddr);
			pmp->pr_mflags = 0;
			if (prot & PROT_READ)
				pmp->pr_mflags |= MA_READ;
			if (prot & PROT_WRITE)
				pmp->pr_mflags |= MA_WRITE;
			if (prot & PROT_EXEC)
				pmp->pr_mflags |= MA_EXEC;
			if (SEGOP_GETTYPE(seg, saddr) == MAP_SHARED)
				pmp->pr_mflags |= MA_SHARED;
			if (seg->s_ops == &segspt_shmops ||
			    (seg->s_ops == &segvn_ops &&
			    (SEGOP_GETVP(seg, saddr, &vp) != 0 || vp == NULL)))
				pmp->pr_mflags |= MA_ANON;
			if (seg->s_ops == &segspt_shmops)
				pmp->pr_mflags |= MA_ISM;
			pmp->pr_pagesize = PAGESIZE;
			/*
			 * Manufacture a filename for the "object" directory.
			 */
			vattr.va_mask = AT_FSID|AT_NODEID;
			if (seg->s_ops == &segvn_ops &&
			    SEGOP_GETVP(seg, saddr, &vp) == 0 &&
			    vp != NULL && vp->v_type == VREG &&
			    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {
				if (vp == p->p_exec)
					(void) strcpy(pmp->pr_mapname, "a.out");
				else
					pr_object_name(pmp->pr_mapname,
						vp, &vattr);
			}

			/*
			 * Get the SysV shared memory id, if any.
			 */
			if ((pmp->pr_mflags & MA_SHARED) && p->p_segacct)
				pmp->pr_shmid = shmgetid(p, saddr);
			else
				pmp->pr_shmid = -1;

			hat_getstat(as, saddr, len, hatid,
			    (char *)(pmp+1), HAT_SYNC_ZERORM);
			pmp = (prasmap32_t *)((caddr_t)(pmp+1) + round8(npage));
		}
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	AS_LOCK_EXIT(as, &as->a_lock);

	ASSERT((caddr_t)pmp == buf+size);
	error = uiomove(buf, size, UIO_READ, uiop);
	kmem_free(buf, size);

	return (error);
}
#endif	/* _SYSCALL32_IMPL */

/*
 * Return information used by ps(1).
 */
void
prgetpsinfo(proc_t *p, psinfo_t *psp)
{
	kthread_t *t;
	struct cred *cred;

	ASSERT(MUTEX_HELD(&p->p_lock));

	if ((t = prchoose(p)) == NULL)	/* returns locked thread */
		bzero(psp, sizeof (*psp));
	else {
		thread_unlock(t);
		bzero(psp, sizeof (*psp) - sizeof (psp->pr_lwp));
	}

	psp->pr_flag = p->p_flag;
	psp->pr_nlwp = p->p_lwpcnt;
	mutex_enter(&p->p_crlock);
	cred = p->p_cred;
	psp->pr_uid = cred->cr_ruid;
	psp->pr_euid = cred->cr_uid;
	psp->pr_gid = cred->cr_rgid;
	psp->pr_egid = cred->cr_gid;
	mutex_exit(&p->p_crlock);
	psp->pr_pid = p->p_pid;
	psp->pr_ppid = p->p_ppid;
	psp->pr_pgid = p->p_pgrp;
	psp->pr_sid = p->p_sessp->s_sid;
	psp->pr_reserved[0] = -1;
	psp->pr_reserved[1] = -1;
	psp->pr_addr = (uintptr_t)prgetpsaddr(p);
	switch (p->p_model) {
	case DATAMODEL_ILP32:
		psp->pr_dmodel = PR_MODEL_ILP32;
		break;
	case DATAMODEL_LP64:
		psp->pr_dmodel = PR_MODEL_LP64;
		break;
	}
	TICK_TO_TIMESTRUC(p->p_utime + p->p_stime, &psp->pr_time);
	TICK_TO_TIMESTRUC(p->p_cutime + p->p_cstime, &psp->pr_ctime);

	if (t == NULL) {
		int wcode = p->p_wcode;		/* must be atomic read */

		if (wcode)
			psp->pr_wstat = wstat(wcode, p->p_wdata);
		psp->pr_ttydev = PRNODEV;
		psp->pr_lwp.pr_state = SZOMB;
		psp->pr_lwp.pr_sname = 'Z';
	} else {
		user_t *up = PTOU(p);
		struct as *as;
		dev_t d;
		extern dev_t rwsconsdev, rconsdev, uconsdev;

		d = cttydev(p);
		/*
		 * If the controlling terminal is the real
		 * or workstation console device, map to what the
		 * user thinks is the console device.
		 */
		if (d == rwsconsdev || d == rconsdev)
			d = uconsdev;
		psp->pr_ttydev = (d == NODEV) ? PRNODEV : d;
		psp->pr_start.tv_sec = up->u_start;
		psp->pr_start.tv_nsec = 0L;
		bcopy(up->u_comm, psp->pr_fname,
		    MIN(sizeof (up->u_comm), sizeof (psp->pr_fname)-1));
		bcopy(up->u_psargs, psp->pr_psargs,
		    MIN(PRARGSZ-1, PSARGSZ));
		psp->pr_argc = up->u_argc;
		psp->pr_argv = up->u_argv;
		psp->pr_envp = up->u_envp;

		/* get the chosen lwp's lwpsinfo */
		prgetlwpsinfo(t, &psp->pr_lwp);

		/* compute %cpu for the process */
		if (p->p_lwpcnt == 1)
			psp->pr_pctcpu = psp->pr_lwp.pr_pctcpu;
		else {
			uint64_t pct = 0;
			clock_t ticks;

			t = p->p_tlist;
			do {
				ticks = lbolt - t->t_lbolt - 1;
				pct += (ticks == 0 || ticks == -1)?
					t->t_pctcpu :
					cpu_decay(t->t_pctcpu, ticks);
			} while ((t = t->t_forw) != p->p_tlist);

			/* prorate over online cpus so we don't exceed 100% */
			if (ncpus > 1)
				pct /= ncpus;
			pct >>= 16;	/* convert to 16-bit scaled integer */
			if (pct > 0x8000) /* might happen, due to rounding */
				pct = 0x8000;
			psp->pr_pctcpu = (ushort_t)pct;
		}
		if ((p->p_flag & SSYS) || (as = p->p_as) == &kas) {
			psp->pr_size = 0;
			psp->pr_rssize = 0;
		} else {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
			psp->pr_size = btopr(rm_assize(as)) * (PAGESIZE / 1024);
			psp->pr_rssize = rm_asrss(as) * (PAGESIZE / 1024);
			psp->pr_pctmem = rm_pctmemory(as);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
	}
}

#ifdef _SYSCALL32_IMPL
void
prgetpsinfo32(proc_t *p, psinfo32_t *psp)
{
	kthread_t *t;
	struct cred *cred;

	ASSERT(MUTEX_HELD(&p->p_lock));

	if ((t = prchoose(p)) == NULL)	/* returns locked thread */
		bzero(psp, sizeof (*psp));
	else {
		thread_unlock(t);
		bzero(psp, sizeof (*psp) - sizeof (psp->pr_lwp));
	}

	psp->pr_flag = p->p_flag;
	psp->pr_nlwp = p->p_lwpcnt;
	mutex_enter(&p->p_crlock);
	cred = p->p_cred;
	psp->pr_uid = cred->cr_ruid;
	psp->pr_euid = cred->cr_uid;
	psp->pr_gid = cred->cr_rgid;
	psp->pr_egid = cred->cr_gid;
	mutex_exit(&p->p_crlock);
	psp->pr_pid = p->p_pid;
	psp->pr_ppid = p->p_ppid;
	psp->pr_pgid = p->p_pgrp;
	psp->pr_sid = p->p_sessp->s_sid;
	psp->pr_reserved[0] = -1;
	psp->pr_reserved[1] = -1;
	psp->pr_addr = 0;	/* cannot represent 64-bit addr in 32 bits */
	switch (p->p_model) {
	case DATAMODEL_ILP32:
		psp->pr_dmodel = PR_MODEL_ILP32;
		break;
	case DATAMODEL_LP64:
		psp->pr_dmodel = PR_MODEL_LP64;
		break;
	}
	TICK_TO_TIMESTRUC32(p->p_utime + p->p_stime, &psp->pr_time);
	TICK_TO_TIMESTRUC32(p->p_cutime + p->p_cstime, &psp->pr_ctime);

	if (t == NULL) {
		extern int wstat(int, int);	/* needs a header file */
		int wcode = p->p_wcode;		/* must be atomic read */

		if (wcode)
			psp->pr_wstat = wstat(wcode, p->p_wdata);
		psp->pr_ttydev = PRNODEV32;
		psp->pr_lwp.pr_state = SZOMB;
		psp->pr_lwp.pr_sname = 'Z';
	} else {
		user_t *up = PTOU(p);
		struct as *as;
		dev_t d;
		extern dev_t rwsconsdev, rconsdev, uconsdev;

		d = cttydev(p);
		/*
		 * If the controlling terminal is the real
		 * or workstation console device, map to what the
		 * user thinks is the console device.
		 */
		if (d == rwsconsdev || d == rconsdev)
			d = uconsdev;
		(void) cmpldev(&psp->pr_ttydev, d);
		psp->pr_start.tv_sec = (time32_t)up->u_start;
		psp->pr_start.tv_nsec = 0L;
		bcopy(up->u_comm, psp->pr_fname,
		    MIN(sizeof (up->u_comm), sizeof (psp->pr_fname)-1));
		bcopy(up->u_psargs, psp->pr_psargs,
		    MIN(PRARGSZ-1, PSARGSZ));
		psp->pr_argc = up->u_argc;
		psp->pr_argv = (caddr32_t)up->u_argv;
		psp->pr_envp = (caddr32_t)up->u_envp;

		/* get the chosen lwp's lwpsinfo */
		prgetlwpsinfo32(t, &psp->pr_lwp);

		/* compute %cpu for the process */
		if (p->p_lwpcnt == 1)
			psp->pr_pctcpu = psp->pr_lwp.pr_pctcpu;
		else {
			uint64_t pct = 0;
			clock_t ticks;

			t = p->p_tlist;
			do {
				ticks = lbolt - t->t_lbolt - 1;
				pct += (ticks == 0 || ticks == -1)?
					t->t_pctcpu :
					cpu_decay(t->t_pctcpu, ticks);
			} while ((t = t->t_forw) != p->p_tlist);

			/* prorate over online cpus so we don't exceed 100% */
			if (ncpus > 1)
				pct /= ncpus;
			pct >>= 16;	/* convert to 16-bit scaled integer */
			if (pct > 0x8000) /* might happen, due to rounding */
				pct = 0x8000;
			psp->pr_pctcpu = (ushort_t)pct;
		}
		if ((p->p_flag & SSYS) || (as = p->p_as) == &kas) {
			psp->pr_size = 0;
			psp->pr_rssize = 0;
		} else {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
			psp->pr_size = (size32_t)
				(btopr(rm_assize(as)) * (PAGESIZE / 1024));
			psp->pr_rssize = (size32_t)
				(rm_asrss(as) * (PAGESIZE / 1024));
			psp->pr_pctmem = rm_pctmemory(as);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
	}

	/*
	 * If we are looking at an LP64 process, zero out
	 * the fields that cannot be represented in ILP32.
	 */
	if (p->p_model != DATAMODEL_ILP32) {
		psp->pr_size = 0;
		psp->pr_rssize = 0;
		psp->pr_argv = 0;
		psp->pr_envp = 0;
	}
}
#endif	/* _SYSCALL32_IMPL */

void
prgetlwpsinfo(kthread_t *t, lwpsinfo_t *psp)
{
	klwp_t *lwp = ttolwp(t);
	sobj_ops_t *sobj;
	char c, state;
	ulong_t pct;
	int retval, niceval;
	clock_t ticks;

	ASSERT(MUTEX_HELD(&ttoproc(t)->p_lock));

	bzero(psp, sizeof (*psp));

	psp->pr_flag = t->t_flag;
	psp->pr_lwpid = t->t_tid;
	psp->pr_addr = (uintptr_t)t;
	psp->pr_wchan = (uintptr_t)t->t_wchan;

	/* map the thread state enum into a process state enum */
	state = VSTOPPED(t) ? TS_STOPPED : t->t_state;
	switch (state) {
	case TS_SLEEP:		state = SSLEEP;		c = 'S';	break;
	case TS_RUN:		state = SRUN;		c = 'R';	break;
	case TS_ONPROC:		state = SONPROC;	c = 'O';	break;
	case TS_ZOMB:		state = SZOMB;		c = 'Z';	break;
	case TS_STOPPED:	state = SSTOP;		c = 'T';	break;
	default:		state = 0;		c = '?';	break;
	}
	psp->pr_state = state;
	psp->pr_sname = c;
	if ((sobj = t->t_sobj_ops) != NULL)
		psp->pr_stype = SOBJ_TYPE(sobj);
	retval = CL_DONICE(t, NULL, 0, &niceval);
	if (retval == 0) {
		psp->pr_oldpri = v.v_maxsyspri - t->t_pri;
		psp->pr_nice = niceval + NZERO;
	}
	psp->pr_syscall = t->t_sysnum;
	psp->pr_pri = t->t_pri;
	psp->pr_start.tv_sec = t->t_start;
	psp->pr_start.tv_nsec = 0L;
	TICK_TO_TIMESTRUC(lwp->lwp_utime + lwp->lwp_stime, &psp->pr_time);
	/* compute %cpu for the lwp */
	ticks = lbolt - t->t_lbolt - 1;
	pct = (ticks == 0 || ticks == -1)? t->t_pctcpu :
		cpu_decay(t->t_pctcpu, ticks);
	/* prorate over the online cpus so we don't exceed 100% */
	if (ncpus > 1)
		pct /= ncpus;
	pct >>= 16;	/* convert to 16-bit scaled integer */
	if (pct > 0x8000)	/* might happen, due to rounding */
		pct = 0x8000;
	psp->pr_pctcpu = (ushort_t)pct;
	psp->pr_cpu = (psp->pr_pctcpu*100 + 0x6000) >> 15;	/* [0..99] */
	if (psp->pr_cpu > 99)
		psp->pr_cpu = 99;

	(void) strncpy(psp->pr_clname, sclass[t->t_cid].cl_name,
		sizeof (psp->pr_clname) - 1);
	bzero(psp->pr_name, sizeof (psp->pr_name));	/* XXX ??? */
	psp->pr_onpro = t->t_cpu->cpu_id;
	psp->pr_bindpro = t->t_bind_cpu;
	psp->pr_bindpset = t->t_bind_pset;
}

#ifdef _SYSCALL32_IMPL
void
prgetlwpsinfo32(kthread_t *t, lwpsinfo32_t *psp)
{
	proc_t *p = ttoproc(t);
	klwp_t *lwp = ttolwp(t);
	sobj_ops_t *sobj;
	char c, state;
	ulong_t pct;
	int retval, niceval;
	clock_t ticks;

	ASSERT(MUTEX_HELD(&p->p_lock));

	bzero(psp, sizeof (*psp));

	psp->pr_flag = t->t_flag;
	psp->pr_lwpid = t->t_tid;
	psp->pr_addr = 0;	/* cannot represent 64-bit addr in 32 bits */
	psp->pr_wchan = 0;	/* cannot represent 64-bit addr in 32 bits */

	/* map the thread state enum into a process state enum */
	state = VSTOPPED(t) ? TS_STOPPED : t->t_state;
	switch (state) {
	case TS_SLEEP:		state = SSLEEP;		c = 'S';	break;
	case TS_RUN:		state = SRUN;		c = 'R';	break;
	case TS_ONPROC:		state = SONPROC;	c = 'O';	break;
	case TS_ZOMB:		state = SZOMB;		c = 'Z';	break;
	case TS_STOPPED:	state = SSTOP;		c = 'T';	break;
	default:		state = 0;		c = '?';	break;
	}
	psp->pr_state = state;
	psp->pr_sname = c;
	if ((sobj = t->t_sobj_ops) != NULL)
		psp->pr_stype = SOBJ_TYPE(sobj);
	retval = CL_DONICE(t, NULL, 0, &niceval);
	if (retval == 0) {
		psp->pr_oldpri = v.v_maxsyspri - t->t_pri;
		psp->pr_nice = niceval + NZERO;
	} else {
		psp->pr_oldpri = 0;
		psp->pr_nice = 0;
	}
	psp->pr_syscall = t->t_sysnum;
	psp->pr_pri = t->t_pri;
	psp->pr_start.tv_sec = (time32_t)t->t_start;
	psp->pr_start.tv_nsec = 0L;
	TICK_TO_TIMESTRUC32(lwp->lwp_utime + lwp->lwp_stime, &psp->pr_time);
	/* compute %cpu for the lwp */
	ticks = lbolt - t->t_lbolt - 1;
	pct = (ticks == 0 || ticks == -1)? t->t_pctcpu :
		cpu_decay(t->t_pctcpu, ticks);
	/* prorate over the online cpus so we don't exceed 100% */
	if (ncpus > 1)
		pct /= ncpus;
	pct >>= 16;	/* convert to 16-bit scaled integer */
	if (pct > 0x8000)	/* might happen, due to rounding */
		pct = 0x8000;
	psp->pr_pctcpu = (ushort_t)pct;
	psp->pr_cpu = (psp->pr_pctcpu*100 + 0x6000) >> 15;	/* [0..99] */
	if (psp->pr_cpu > 99)
		psp->pr_cpu = 99;

	(void) strncpy(psp->pr_clname, sclass[t->t_cid].cl_name,
		sizeof (psp->pr_clname) - 1);
	bzero(psp->pr_name, sizeof (psp->pr_name));	/* XXX ??? */
	psp->pr_onpro = t->t_cpu->cpu_id;
	psp->pr_bindpro = t->t_bind_cpu;
	psp->pr_bindpset = t->t_bind_pset;
}
#endif	/* _SYSCALL32_IMPL */

/*
 * Called when microstate accounting information is requested for a thread
 * where microstate accounting (TP_MSACCT) isn't on.  Turn it on for this and
 * all other LWPs in the process and get an estimate of usage so far.
 */
void
estimate_msacct(kthread_t *t, hrtime_t curtime)
{
	proc_t *p;
	klwp_t *lwp;
	struct mstate *ms;

	if (t == NULL)
		return;

	p = ttoproc(t);
	ASSERT(MUTEX_HELD(&p->p_lock));

	/*
	 * A system process (p0) could be referenced if the thread is
	 * in the process of exiting.  Don't turn on microstate accounting
	 * in that case.
	 */
	if (p->p_flag & SSYS)
		return;

	/*
	 * Loop through all the LWPs (kernel threads) in the process.
	 */
	t = p->p_tlist;
	do {
		int ms_prev;
		int lwp_state;
		hrtime_t total, old_sleep, new_sleep;
		int i;

		ASSERT((t->t_proc_flag & TP_MSACCT) == 0);

		lwp = ttolwp(t);
		ms = &lwp->lwp_mstate;

		old_sleep = ms->ms_acct[LMS_SLEEP];
		bzero(&ms->ms_acct[0], sizeof (ms->ms_acct));

		/*
		 * Convert tick-based user and system time to microstate times.
		 */
		ms->ms_acct[LMS_USER] = TICK_TO_NSEC((hrtime_t)lwp->lwp_utime);
		ms->ms_acct[LMS_SYSTEM] =
			TICK_TO_NSEC((hrtime_t)lwp->lwp_stime);
		/*
		 * Add all unaccounted-for time to the LMS_SLEEP time.
		 */
		for (total = 0, i = 0; i < NMSTATES; i++)
			total += ms->ms_acct[i];

		/*
		 * Convert total time to whole number of ticks to match units
		 * for lwp_utime and lwp_stime.  Necessary because these are
		 * incremented in clock_tick(), and so can count a tick when
		 * lwp has not yet been running for a whole tick.  This would
		 * result in total > curtime - ms->ms_start.
		 */
		new_sleep = TICK_TO_NSEC(NSEC_TO_TICK_ROUNDUP(curtime -
		    ms->ms_start)) - total;

		/*
		 * Make sure that sleep estimate is continuously increasing
		 */
		ms->ms_acct[LMS_SLEEP] = MAX(old_sleep, new_sleep);

		t->t_waitrq = 0;

		/*
		 * Determine the current microstate and set the start time.
		 * Be careful not to touch the lwp while holding thread_lock().
		 */
		ms->ms_state_start = curtime;
		lwp_state = lwp->lwp_state;
		thread_lock(t);
		switch (t->t_state) {
		case TS_SLEEP:
			t->t_mstate = LMS_SLEEP;
			ms_prev = LMS_SYSTEM;
			break;
		case TS_RUN:
			t->t_waitrq = curtime;
			t->t_mstate = LMS_SLEEP;
			ms_prev = LMS_SYSTEM;
			break;
		case TS_ONPROC:
			/*
			 * The user/system state cannot be determined accurately
			 * on MP without stopping the thread.
			 * This might miss a system/user state transition.
			 */
			if (lwp_state == LWP_USER) {
				t->t_mstate = ms_prev = LMS_USER;
			} else {
				t->t_mstate = ms_prev = LMS_SYSTEM;
			}
			break;
		case TS_ZOMB:
		case TS_FREE:			/* shouldn't happen */
		case TS_STOPPED:
			t->t_mstate = LMS_STOPPED;
			ms_prev = LMS_SYSTEM;
			break;
		}
		thread_unlock(t);
		ms->ms_prev = ms_prev;	/* guess previous running state */
		t->t_proc_flag |= TP_MSACCT;
	} while ((t = t->t_forw) != p->p_tlist);

	p->p_flag |= SMSACCT;			/* set process-wide MSACCT */
	/*
	 * Set system call pre- and post-processing flags for the process.
	 * This must be done AFTER the TP_MSACCT flag is set.
	 * Do this outside of the loop to avoid re-ordering.
	 */
	set_proc_sys(p);
}

/*
 * Turn off microstate accounting for all LWPs in the process.
 */
void
disable_msacct(proc_t *p)
{
	kthread_t *t;

	ASSERT(MUTEX_HELD(&p->p_lock));

	p->p_flag &= ~SMSACCT;		/* clear process-wide MSACCT */
	/*
	 * Loop through all the LWPs (kernel threads) in the process.
	 */
	if ((t = p->p_tlist) != NULL) {
		do {
			/* clear per-thread flag */
			t->t_proc_flag &= ~TP_MSACCT;
		} while ((t = t->t_forw) != p->p_tlist);
	}
}

/*
 * Return resource usage information.
 */
void
prgetusage(kthread_t *t, prhusage_t *pup)
{
	klwp_t *lwp = ttolwp(t);
	hrtime_t *mstimep;
	struct mstate *ms = &lwp->lwp_mstate;
	int state;
	hrtime_t curtime;
	hrtime_t waitrq;

	curtime = pup->pr_tstamp;	/* passed by caller */

	/*
	 * If microstate accounting (TP_MSACCT) isn't on, turn it on and
	 * get an estimate of usage so far.
	 */
	if ((t->t_proc_flag & TP_MSACCT) == 0)
		estimate_msacct(t, curtime);

	pup->pr_lwpid	= t->t_tid;
	pup->pr_count	= 1;
	pup->pr_create	= ms->ms_start;
	pup->pr_term	= ms->ms_term;
	if (ms->ms_term == 0)
		pup->pr_rtime = curtime - ms->ms_start;
	else
		pup->pr_rtime = ms->ms_term - ms->ms_start;

	pup->pr_utime    = ms->ms_acct[LMS_USER];
	pup->pr_stime    = ms->ms_acct[LMS_SYSTEM];
	pup->pr_ttime    = ms->ms_acct[LMS_TRAP];
	pup->pr_tftime   = ms->ms_acct[LMS_TFAULT];
	pup->pr_dftime   = ms->ms_acct[LMS_DFAULT];
	pup->pr_kftime   = ms->ms_acct[LMS_KFAULT];
	pup->pr_ltime    = ms->ms_acct[LMS_USER_LOCK];
	pup->pr_slptime  = ms->ms_acct[LMS_SLEEP];
	pup->pr_wtime    = ms->ms_acct[LMS_WAIT_CPU];
	pup->pr_stoptime = ms->ms_acct[LMS_STOPPED];

	/*
	 * Adjust for time waiting in the dispatcher queue.
	 */
	waitrq = t->t_waitrq;	/* hopefully atomic */
	if (waitrq != 0) {
		pup->pr_wtime += curtime - waitrq;
		curtime = waitrq;
	}

	/*
	 * Adjust for time spent in current microstate.
	 */
	switch (state = t->t_mstate) {
	case LMS_SLEEP:
		/*
		 * Update the timer for the current sleep state.
		 */
		switch (state = ms->ms_prev) {
		case LMS_TFAULT:
		case LMS_DFAULT:
		case LMS_KFAULT:
		case LMS_USER_LOCK:
			break;
		default:
			state = LMS_SLEEP;
			break;
		}
		break;
	case LMS_TFAULT:
	case LMS_DFAULT:
	case LMS_KFAULT:
	case LMS_USER_LOCK:
		state = LMS_SYSTEM;
		break;
	}
	switch (state) {
	case LMS_USER:		mstimep = &pup->pr_utime;	break;
	case LMS_SYSTEM:	mstimep = &pup->pr_stime;	break;
	case LMS_TRAP:		mstimep = &pup->pr_ttime;	break;
	case LMS_TFAULT:	mstimep = &pup->pr_tftime;	break;
	case LMS_DFAULT:	mstimep = &pup->pr_dftime;	break;
	case LMS_KFAULT:	mstimep = &pup->pr_kftime;	break;
	case LMS_USER_LOCK:	mstimep = &pup->pr_ltime;	break;
	case LMS_SLEEP:		mstimep = &pup->pr_slptime;	break;
	case LMS_WAIT_CPU:	mstimep = &pup->pr_wtime;	break;
	case LMS_STOPPED:	mstimep = &pup->pr_stoptime;	break;
	default:		panic("prgetusage: unknown microstate");
	}
	*mstimep += curtime - ms->ms_state_start;

	/*
	 * Resource usage counters.
	 */
	pup->pr_minf  = lwp->lwp_ru.minflt;
	pup->pr_majf  = lwp->lwp_ru.majflt;
	pup->pr_nswap = lwp->lwp_ru.nswap;
	pup->pr_inblk = lwp->lwp_ru.inblock;
	pup->pr_oublk = lwp->lwp_ru.oublock;
	pup->pr_msnd  = lwp->lwp_ru.msgsnd;
	pup->pr_mrcv  = lwp->lwp_ru.msgrcv;
	pup->pr_sigs  = lwp->lwp_ru.nsignals;
	pup->pr_vctx  = lwp->lwp_ru.nvcsw;
	pup->pr_ictx  = lwp->lwp_ru.nivcsw;
	pup->pr_sysc  = lwp->lwp_ru.sysc;
	pup->pr_ioch  = lwp->lwp_ru.ioch;
}

/*
 * Sum resource usage information.
 */
void
praddusage(kthread_t *t, prhusage_t *pup)
{
	klwp_t *lwp = ttolwp(t);
	hrtime_t *mstimep;
	struct mstate *ms = &lwp->lwp_mstate;
	int state;
	hrtime_t curtime;
	hrtime_t waitrq;

	curtime = pup->pr_tstamp;	/* passed by caller */

	/*
	 * If microstate accounting (TP_MSACCT) isn't on, turn it on and
	 * get an estimate of usage so far.
	 */
	if ((t->t_proc_flag & TP_MSACCT) == 0)
		estimate_msacct(t, curtime);

	if (ms->ms_term == 0)
		pup->pr_rtime += curtime - ms->ms_start;
	else
		pup->pr_rtime += ms->ms_term - ms->ms_start;
	pup->pr_utime	+= ms->ms_acct[LMS_USER];
	pup->pr_stime	+= ms->ms_acct[LMS_SYSTEM];
	pup->pr_ttime	+= ms->ms_acct[LMS_TRAP];
	pup->pr_tftime	+= ms->ms_acct[LMS_TFAULT];
	pup->pr_dftime	+= ms->ms_acct[LMS_DFAULT];
	pup->pr_kftime	+= ms->ms_acct[LMS_KFAULT];
	pup->pr_ltime	+= ms->ms_acct[LMS_USER_LOCK];
	pup->pr_slptime	+= ms->ms_acct[LMS_SLEEP];
	pup->pr_wtime	+= ms->ms_acct[LMS_WAIT_CPU];
	pup->pr_stoptime += ms->ms_acct[LMS_STOPPED];

	/*
	 * Adjust for time waiting in the dispatcher queue.
	 */
	waitrq = t->t_waitrq;	/* hopefully atomic */
	if (waitrq != 0) {
		pup->pr_wtime += curtime - waitrq;
		curtime = waitrq;
	}

	/*
	 * Adjust for time spent in current microstate.
	 */
	switch (state = t->t_mstate) {
	case LMS_SLEEP:
		/*
		 * Update the timer for the current sleep state.
		 */
		switch (state = ms->ms_prev) {
		case LMS_TFAULT:
		case LMS_DFAULT:
		case LMS_KFAULT:
		case LMS_USER_LOCK:
			break;
		default:
			state = LMS_SLEEP;
			break;
		}
		break;
	case LMS_TFAULT:
	case LMS_DFAULT:
	case LMS_KFAULT:
	case LMS_USER_LOCK:
		state = LMS_SYSTEM;
		break;
	}
	switch (state) {
	case LMS_USER:		mstimep = &pup->pr_utime;	break;
	case LMS_SYSTEM:	mstimep = &pup->pr_stime;	break;
	case LMS_TRAP:		mstimep = &pup->pr_ttime;	break;
	case LMS_TFAULT:	mstimep = &pup->pr_tftime;	break;
	case LMS_DFAULT:	mstimep = &pup->pr_dftime;	break;
	case LMS_KFAULT:	mstimep = &pup->pr_kftime;	break;
	case LMS_USER_LOCK:	mstimep = &pup->pr_ltime;	break;
	case LMS_SLEEP:		mstimep = &pup->pr_slptime;	break;
	case LMS_WAIT_CPU:	mstimep = &pup->pr_wtime;	break;
	case LMS_STOPPED:	mstimep = &pup->pr_stoptime;	break;
	default:		panic("praddusage: unknown microstate");
	}
	*mstimep += curtime - ms->ms_state_start;

	/*
	 * Resource usage counters.
	 */
	pup->pr_minf  += lwp->lwp_ru.minflt;
	pup->pr_majf  += lwp->lwp_ru.majflt;
	pup->pr_nswap += lwp->lwp_ru.nswap;
	pup->pr_inblk += lwp->lwp_ru.inblock;
	pup->pr_oublk += lwp->lwp_ru.oublock;
	pup->pr_msnd  += lwp->lwp_ru.msgsnd;
	pup->pr_mrcv  += lwp->lwp_ru.msgrcv;
	pup->pr_sigs  += lwp->lwp_ru.nsignals;
	pup->pr_vctx  += lwp->lwp_ru.nvcsw;
	pup->pr_ictx  += lwp->lwp_ru.nivcsw;
	pup->pr_sysc  += lwp->lwp_ru.sysc;
	pup->pr_ioch  += lwp->lwp_ru.ioch;
}

/*
 * Convert a prhusage_t to a prusage_t.
 * This means convert each hrtime_t to a timestruc_t
 * and copy the count fields uint64_t => ulong_t.
 */
void
prcvtusage(prhusage_t *pup, prusage_t *upup)
{
	uint64_t *ullp;
	ulong_t *ulp;
	int i;

	upup->pr_lwpid = pup->pr_lwpid;
	upup->pr_count = pup->pr_count;

	hrt2ts(pup->pr_tstamp,	&upup->pr_tstamp);
	hrt2ts(pup->pr_create,	&upup->pr_create);
	hrt2ts(pup->pr_term,	&upup->pr_term);
	hrt2ts(pup->pr_rtime,	&upup->pr_rtime);
	hrt2ts(pup->pr_utime,	&upup->pr_utime);
	hrt2ts(pup->pr_stime,	&upup->pr_stime);
	hrt2ts(pup->pr_ttime,	&upup->pr_ttime);
	hrt2ts(pup->pr_tftime,	&upup->pr_tftime);
	hrt2ts(pup->pr_dftime,	&upup->pr_dftime);
	hrt2ts(pup->pr_kftime,	&upup->pr_kftime);
	hrt2ts(pup->pr_ltime,	&upup->pr_ltime);
	hrt2ts(pup->pr_slptime,	&upup->pr_slptime);
	hrt2ts(pup->pr_wtime,	&upup->pr_wtime);
	hrt2ts(pup->pr_stoptime, &upup->pr_stoptime);
	bzero(upup->filltime, sizeof (upup->filltime));

	ullp = &pup->pr_minf;
	ulp = &upup->pr_minf;
	for (i = 0; i < 22; i++)
		*ulp++ = (ulong_t)*ullp++;
}

#ifdef _SYSCALL32_IMPL
void
prcvtusage32(prhusage_t *pup, prusage32_t *upup)
{
	uint64_t *ullp;
	uint32_t *ulp;
	int i;

	upup->pr_lwpid = pup->pr_lwpid;
	upup->pr_count = pup->pr_count;

	hrt2ts32(pup->pr_tstamp,	&upup->pr_tstamp);
	hrt2ts32(pup->pr_create,	&upup->pr_create);
	hrt2ts32(pup->pr_term,		&upup->pr_term);
	hrt2ts32(pup->pr_rtime,		&upup->pr_rtime);
	hrt2ts32(pup->pr_utime,		&upup->pr_utime);
	hrt2ts32(pup->pr_stime,		&upup->pr_stime);
	hrt2ts32(pup->pr_ttime,		&upup->pr_ttime);
	hrt2ts32(pup->pr_tftime,	&upup->pr_tftime);
	hrt2ts32(pup->pr_dftime,	&upup->pr_dftime);
	hrt2ts32(pup->pr_kftime,	&upup->pr_kftime);
	hrt2ts32(pup->pr_ltime,		&upup->pr_ltime);
	hrt2ts32(pup->pr_slptime,	&upup->pr_slptime);
	hrt2ts32(pup->pr_wtime,		&upup->pr_wtime);
	hrt2ts32(pup->pr_stoptime,	&upup->pr_stoptime);
	bzero(upup->filltime, sizeof (upup->filltime));

	ullp = &pup->pr_minf;
	ulp = &upup->pr_minf;
	for (i = 0; i < 22; i++)
		*ulp++ = (uint32_t)*ullp++;
}
#endif	/* _SYSCALL32_IMPL */

/*
 * Determine whether a set is empty.
 */
int
setisempty(uint32_t *sp, uint_t n)
{
	while (n--)
		if (*sp++)
			return (0);
	return (1);
}

/*
 * Utility routine for establishing a watched area in the process.
 * Keep the list of watched areas sorted by virtual address.
 */
int
set_watched_area(proc_t *p,
	struct watched_area *pwa,
	struct watched_page *pwplist)
{
	caddr_t vaddr = pwa->wa_vaddr;
	caddr_t eaddr = pwa->wa_eaddr;
	ulong_t flags = pwa->wa_flags;
	struct watched_area *successor;
	int error = 0;

	/* we must not be holding p->p_lock, but the process must be locked */
	ASSERT(MUTEX_NOT_HELD(&p->p_lock));
	ASSERT(p->p_flag & SPRLOCK);

	if ((successor = p->p_warea) == NULL) {
		kthread_t *t;

		ASSERT(p->p_nwarea == 0);
		p->p_nwarea = 1;
		p->p_warea = pwa->wa_forw = pwa->wa_back = pwa;
		mutex_enter(&p->p_lock);
		if ((t = p->p_tlist) != NULL) {
			do {
				t->t_proc_flag |= TP_WATCHPT;
				ASSERT(t->t_copyops == &default_copyops);
				t->t_copyops = &watch_copyops;
			} while ((t = t->t_forw) != p->p_tlist);
		}
		mutex_exit(&p->p_lock);
	} else {
		ASSERT(p->p_nwarea > 0);
		do {
			if (successor->wa_eaddr <= vaddr)
				continue;
			if (successor->wa_vaddr >= eaddr)
				break;
			/*
			 * We discovered an existing, overlapping watched area.
			 * Allow it only if it is an exact match.
			 */
			if (successor->wa_vaddr != vaddr ||
			    successor->wa_eaddr != eaddr)
				error = EINVAL;
			else if (successor->wa_flags != flags) {
				error = set_watched_page(p, vaddr, eaddr,
				    flags, successor->wa_flags, pwplist);
				successor->wa_flags = flags;
			}
			kmem_free(pwa, sizeof (struct watched_area));
			return (error);
		} while ((successor = successor->wa_forw) != p->p_warea);

		if (p->p_nwarea >= prnwatch) {
			kmem_free(pwa, sizeof (struct watched_area));
			return (E2BIG);
		}
		p->p_nwarea++;
		insque(pwa, successor->wa_back);
		if (p->p_warea->wa_vaddr > vaddr)
			p->p_warea = pwa;
	}
	return (set_watched_page(p, vaddr, eaddr, flags, 0, pwplist));
}

/*
 * Utility routine for clearing a watched area in the process.
 * Must be an exact match of the virtual address.
 * size and flags don't matter.
 */
int
clear_watched_area(proc_t *p, struct watched_area *pwa)
{
	caddr_t vaddr = pwa->wa_vaddr;

	/* we must not be holding p->p_lock, but the process must be locked */
	ASSERT(MUTEX_NOT_HELD(&p->p_lock));
	ASSERT(p->p_flag & SPRLOCK);

	kmem_free(pwa, sizeof (struct watched_area));

	if ((pwa = p->p_warea) == NULL)
		return (0);

	/*
	 * Look for a matching address in the watched areas.
	 * If a match is found, clear the old watched area
	 * and adjust the watched page(s).
	 * It is not an error if there is no match.
	 */
	do {
		if (pwa->wa_vaddr == vaddr) {
			p->p_nwarea--;
			if (p->p_warea == pwa)
				p->p_warea = pwa->wa_forw;
			if (p->p_warea == pwa) {
				p->p_warea = NULL;
				ASSERT(p->p_nwarea == 0);
			} else {
				remque(pwa);
				ASSERT(p->p_nwarea > 0);
			}
			clear_watched_page(p, pwa->wa_vaddr,
			    pwa->wa_eaddr, pwa->wa_flags);
			kmem_free(pwa, sizeof (struct watched_area));
			break;
		}
	} while ((pwa = pwa->wa_forw) != p->p_warea);

	if (p->p_warea == NULL) {
		kthread_t *t;

		mutex_enter(&p->p_lock);
		if ((t = p->p_tlist) != NULL) {
			do {
				t->t_proc_flag &= ~TP_WATCHPT;
				t->t_copyops = &default_copyops;
			} while ((t = t->t_forw) != p->p_tlist);
		}
		mutex_exit(&p->p_lock);
	}

	return (0);
}

/*
 * Utility routine for deallocating a linked list of watched_area structs.
 */
void
pr_free_watchlist(struct watched_area *pwa)
{
	struct watched_area *delp;

	while (pwa != NULL) {
		delp = pwa;
		if ((pwa = pwa->wa_back) == delp)
			pwa = NULL;
		else
			remque(delp);
		kmem_free(delp, sizeof (struct watched_area));
	}
}

/*
 * Utility routines for deallocating a linked list of watched_page structs.
 * This one just deallocates the structures.
 */
void
pr_free_pagelist(struct watched_page *pwp)
{
	struct watched_page *delp;

	while (pwp != NULL) {
		delp = pwp;
		if ((pwp = pwp->wp_back) == delp)
			pwp = NULL;
		else
			remque(delp);
		kmem_free(delp, sizeof (struct watched_page));
	}
}

/*
 * This one is called by the traced process to unwatch all the
 * pages while deallocating the list of watched_page structs.
 */
void
pr_free_my_pagelist()
{
	struct as *as = curproc->p_as;
	struct watched_page *pwp;
	struct watched_page *delp;
	uint_t prot;

	ASSERT(MUTEX_NOT_HELD(&curproc->p_lock));
	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);

	pwp = as->a_wpage;
	as->a_wpage = NULL;
	as->a_nwpage = 0;

	while (pwp != NULL) {
		delp = pwp;
		if ((pwp = pwp->wp_back) == delp)
			pwp = NULL;
		else
			remque(delp);
		if ((prot = delp->wp_oprot) != 0) {
			caddr_t addr = delp->wp_vaddr;
			struct seg *seg;

			if ((delp->wp_prot != prot ||
			    (delp->wp_flags & WP_NOWATCH)) &&
			    (seg = as_segat(as, addr)) != NULL)
				(void) SEGOP_SETPROT(seg, addr, PAGESIZE, prot);
		}
		kmem_free(delp, sizeof (struct watched_page));
	}

	AS_LOCK_EXIT(as, &as->a_lock);
}

/*
 * Insert a watched area into the list of watched pages.
 * If oflags is zero then we are adding a new watched area.
 * Otherwise we are changing the flags of an existing watched area.
 */
static int
set_watched_page(proc_t *p, caddr_t vaddr, caddr_t eaddr,
	ulong_t flags, ulong_t oflags, struct watched_page *pwplist)
{
	struct as *as = p->p_as;
	struct watched_page **pwp_root;
	int *nwpagep;
	struct watched_page *pwp;
	struct watched_page *pnew;
	int npage;
	struct seg *seg;
	uint_t prot;
	int error;

	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);

	/*
	 * Search for an existing watched page to contain the watched area.
	 * If none is found, grab a new one from the available list
	 * and insert it in the active list, keeping the list sorted
	 * by user-level virtual address.
	 */
	if (p->p_flag & SVFWAIT) {
		pwp_root = &p->p_wpage;
		nwpagep = &p->p_nwpage;
	} else {
		pwp_root = &as->a_wpage;
		nwpagep = &as->a_nwpage;
	}

again:
	if ((pwp = *pwp_root) == NULL) {
		ASSERT(*nwpagep == 0);
		pwp = pwplist->wp_forw;
		remque(pwp);
		*pwp_root = pwp->wp_forw = pwp->wp_back = pwp;
		*nwpagep = 1;
		pwp->wp_vaddr = (caddr_t)((ulong_t)vaddr & PAGEMASK);
	} else {
		ASSERT(*nwpagep != 0);
		for (npage = *nwpagep; npage != 0;
		    npage--, pwp = pwp->wp_forw) {
			if (pwp->wp_vaddr > vaddr) {
				npage = 0;
				break;
			}
			if (pwp->wp_vaddr + PAGESIZE > vaddr)
				break;
		}
		/*
		 * If we exhaust the loop then pwp has reached the root
		 * and we will be adding a new element at the end.
		 * If we get here from the 'break' with npage = 0
		 * then we will be inserting a new element before
		 * the page whose uaddr exceeds vaddr.  Both of these
		 * conditions keep the list sorted by user virtual address.
		 */
		if (npage == 0) {
			pnew = pwplist->wp_forw;
			remque(pnew);
			insque(pnew, pwp->wp_back);
			pwp = pnew;
			(*nwpagep)++;
			pwp->wp_vaddr = (caddr_t)((ulong_t)vaddr & PAGEMASK);
			/*
			 * If we inserted a new page at the head of
			 * the list then reset the list head pointer.
			 */
			if ((*pwp_root)->wp_vaddr > pwp->wp_vaddr)
				*pwp_root = pwp;
		}
	}

	if (oflags & WA_READ)
		pwp->wp_read--;
	if (oflags & WA_WRITE)
		pwp->wp_write--;
	if (oflags & WA_EXEC)
		pwp->wp_exec--;

	ASSERT(pwp->wp_read >= 0);
	ASSERT(pwp->wp_write >= 0);
	ASSERT(pwp->wp_exec >= 0);

	if (flags & WA_READ)
		pwp->wp_read++;
	if (flags & WA_WRITE)
		pwp->wp_write++;
	if (flags & WA_EXEC)
		pwp->wp_exec++;

	if (!(p->p_flag & SVFWAIT)) {
		vaddr = pwp->wp_vaddr;
		if (pwp->wp_oprot == 0 &&
		    (seg = as_segat(as, vaddr)) != NULL) {
			SEGOP_GETPROT(seg, vaddr, 0, &prot);
			pwp->wp_oprot = (uchar_t)prot;
			pwp->wp_prot = (uchar_t)prot;
		}
		if (pwp->wp_oprot != 0) {
			prot = pwp->wp_oprot;
			if (pwp->wp_read)
				prot &= ~(PROT_READ|PROT_WRITE|PROT_EXEC);
			if (pwp->wp_write)
				prot &= ~PROT_WRITE;
			if (pwp->wp_exec)
				prot &= ~(PROT_READ|PROT_WRITE|PROT_EXEC);
			if (!(pwp->wp_flags & WP_NOWATCH) &&
			    pwp->wp_prot != prot)
				pwp->wp_flags |= WP_SETPROT;
			pwp->wp_prot = (uchar_t)prot;
		}
	}

	/*
	 * If the watched area extends into the next page then do
	 * it over again with the virtual address of the next page.
	 */
	if ((vaddr = pwp->wp_vaddr + PAGESIZE) < eaddr)
		goto again;

	if (*nwpagep > prnwatch)
		error = E2BIG;
	else
		error = 0;
	AS_LOCK_EXIT(as, &as->a_lock);
	return (error);
}

/*
 * Remove a watched area from the list of watched pages.
 * A watched area may extend over more than one page.
 */
static void
clear_watched_page(proc_t *p, caddr_t vaddr, caddr_t eaddr, ulong_t flags)
{
	struct as *as = p->p_as;
	struct watched_page *pwp;
	int npage;

	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);

	if (p->p_flag & SVFWAIT) {
		pwp = p->p_wpage;
		npage = p->p_nwpage;
	} else {
		pwp = as->a_wpage;
		npage = as->a_nwpage;
	}

	ASSERT(npage != 0 && pwp != NULL);

	while (npage-- != 0) {
		if (pwp->wp_vaddr >= eaddr)
			break;
		if (pwp->wp_vaddr + PAGESIZE <= vaddr) {
			pwp = pwp->wp_forw;
			continue;
		}

		if (flags & WA_READ)
			pwp->wp_read--;
		if (flags & WA_WRITE)
			pwp->wp_write--;
		if (flags & WA_EXEC)
			pwp->wp_exec--;

		if (pwp->wp_read + pwp->wp_write + pwp->wp_exec != 0) {
			/*
			 * Reset the hat layer's protections on this page.
			 */
			if (pwp->wp_oprot != 0) {
				uint_t prot = pwp->wp_oprot;

				if (pwp->wp_read)
					prot &=
					    ~(PROT_READ|PROT_WRITE|PROT_EXEC);
				if (pwp->wp_write)
					prot &= ~PROT_WRITE;
				if (pwp->wp_exec)
					prot &=
					    ~(PROT_READ|PROT_WRITE|PROT_EXEC);
				if (!(pwp->wp_flags & WP_NOWATCH) &&
				    pwp->wp_prot != prot)
					pwp->wp_flags |= WP_SETPROT;
				pwp->wp_prot = (uchar_t)prot;
			}
			pwp = pwp->wp_forw;
		} else {
			/*
			 * No watched areas remain in this page.
			 * Reset everything to normal.
			 */
			if (pwp->wp_oprot != 0) {
				pwp->wp_flags |= WP_SETPROT;
				pwp->wp_prot = pwp->wp_oprot;
			}
			pwp = pwp->wp_forw;
		}
	}

	AS_LOCK_EXIT(as, &as->a_lock);
}

/*
 * Return the original protections for the specified page.
 */
static void
getwatchprot(struct as *as, caddr_t addr, uint_t *prot)
{
	struct watched_page *pwp;

	if ((pwp = as->a_wpage) == NULL)
		return;

	ASSERT(AS_LOCK_HELD(as, &as->a_lock));

	do {
		if (addr < pwp->wp_vaddr)
			break;
		if (addr == pwp->wp_vaddr) {
			if (pwp->wp_oprot != 0)
				*prot = pwp->wp_oprot;
			break;
		}
	} while ((pwp = pwp->wp_forw) != as->a_wpage);
}

static prpagev_t *
pr_pagev_create(struct seg *seg, int check_noreserve)
{
	prpagev_t *pagev = kmem_alloc(sizeof (prpagev_t), KM_SLEEP);
	size_t total_pages = seg_pages(seg);

	/*
	 * Limit the size of our vectors to pagev_lim pages at a time.  We need
	 * 4 or 5 bytes of storage per page, so this means we limit ourself
	 * to about a megabyte of kernel heap by default.
	 */
	pagev->pg_npages = MIN(total_pages, pagev_lim);
	pagev->pg_pnbase = 0;

	pagev->pg_protv =
	    kmem_alloc(pagev->pg_npages * sizeof (uint_t), KM_SLEEP);

	if (check_noreserve)
		pagev->pg_incore =
		    kmem_alloc(pagev->pg_npages * sizeof (char), KM_SLEEP);
	else
		pagev->pg_incore = NULL;

	return (pagev);
}

static void
pr_pagev_destroy(prpagev_t *pagev)
{
	if (pagev->pg_incore != NULL)
		kmem_free(pagev->pg_incore, pagev->pg_npages * sizeof (char));

	kmem_free(pagev->pg_protv, pagev->pg_npages * sizeof (uint_t));
	kmem_free(pagev, sizeof (prpagev_t));
}

static caddr_t
pr_pagev_fill(prpagev_t *pagev, struct seg *seg, caddr_t addr, caddr_t eaddr)
{
	ulong_t lastpg = seg_page(seg, eaddr - 1);
	ulong_t pn, pnlim;
	caddr_t saddr;
	size_t len;

	ASSERT(addr >= seg->s_base && addr <= eaddr);

	if (addr == eaddr)
		return (eaddr);

refill:
	ASSERT(addr < eaddr);
	pagev->pg_pnbase = seg_page(seg, addr);
	pnlim = pagev->pg_pnbase + pagev->pg_npages;
	saddr = addr;

	if (lastpg < pnlim)
		len = (size_t)(eaddr - addr);
	else
		len = pagev->pg_npages * PAGESIZE;

	if (pagev->pg_incore != NULL) {
		/*
		 * INCORE cleverly has different semantics than GETPROT:
		 * it returns info on pages up to but NOT including addr + len.
		 */
		SEGOP_INCORE(seg, addr, len, pagev->pg_incore);
		pn = pagev->pg_pnbase;

		do {
			/*
			 * Guilty knowledge here:  We know that segvn_incore
			 * returns more than just the low-order bit that
			 * indicates the page is actually in memory.  If any
			 * bits are set, then the page has backing store.
			 */
			if (pagev->pg_incore[pn++ - pagev->pg_pnbase])
				goto out;

		} while ((addr += PAGESIZE) < eaddr && pn < pnlim);

		/*
		 * If we examined all the pages in the vector but we're not
		 * at the end of the segment, take another lap.
		 */
		if (addr < eaddr)
			goto refill;
	}

	/*
	 * Need to take len - 1 because addr + len is the address of the
	 * first byte of the page just past the end of what we want.
	 */
out:
	SEGOP_GETPROT(seg, saddr, len - 1, pagev->pg_protv);
	return (addr);
}

static caddr_t
pr_pagev_nextprot(prpagev_t *pagev, struct seg *seg,
    caddr_t *saddrp, caddr_t eaddr, uint_t *protp)
{
	/*
	 * Our starting address is either the specified address, or the base
	 * address from the start of the pagev.  If the latter is greater,
	 * this means a previous call to pr_pagev_fill has already scanned
	 * further than the end of the previous mapping.
	 */
	caddr_t base = seg->s_base + pagev->pg_pnbase * PAGESIZE;
	caddr_t addr = MAX(*saddrp, base);
	ulong_t pn = seg_page(seg, addr);
	uint_t prot, nprot;

	/*
	 * If we're dealing with noreserve pages, then advance addr to
	 * the address of the next page which has backing store.
	 */
	if (pagev->pg_incore != NULL) {
		while (pagev->pg_incore[pn - pagev->pg_pnbase] == 0) {
			addr += PAGESIZE;
			if (++pn == pagev->pg_pnbase + pagev->pg_npages) {
				addr = pr_pagev_fill(pagev, seg, addr, eaddr);
				if (addr == eaddr) {
					*saddrp = addr;
					prot = 0;
					goto out;
				}
				pn = seg_page(seg, addr);
			}
		}
	}

	/*
	 * Get the protections on the page corresponding to addr.
	 */
	pn = seg_page(seg, addr);
	ASSERT(pn >= pagev->pg_pnbase);
	ASSERT(pn < (pagev->pg_pnbase + pagev->pg_npages));

	prot = pagev->pg_protv[pn - pagev->pg_pnbase];
	getwatchprot(seg->s_as, addr, &prot);
	*saddrp = addr;

	/*
	 * Now loop until we find a backed page with different protections
	 * or we reach the end of this segment.
	 */
	while ((addr += PAGESIZE) < eaddr) {
		/*
		 * If pn has advanced to the page number following what we
		 * have information on, refill the page vector and reset
		 * addr and pn.  If pr_pagev_fill does not return the
		 * address of the next page, we have a discontiguity and
		 * thus have reached the end of the current mapping.
		 */
		if (++pn == pagev->pg_pnbase + pagev->pg_npages) {
			caddr_t naddr = pr_pagev_fill(pagev, seg, addr, eaddr);
			if (naddr != addr)
				goto out;
			pn = seg_page(seg, addr);
		}

		/*
		 * The previous page's protections are in prot, and it has
		 * backing.  If this page is MAP_NORESERVE and has no backing,
		 * then end this mapping and return the previous protections.
		 */
		if (pagev->pg_incore != NULL &&
		    pagev->pg_incore[pn - pagev->pg_pnbase] == 0)
			break;

		/*
		 * Otherwise end the mapping if this page's protections (nprot)
		 * are different than those in the previous page (prot).
		 */
		nprot = pagev->pg_protv[pn - pagev->pg_pnbase];
		getwatchprot(seg->s_as, addr, &nprot);

		if (nprot != prot)
			break;
	}

out:
	*protp = prot;
	return (addr);
}

size_t
pr_getsegsize(struct seg *seg, int reserved)
{
	size_t size = seg->s_size;

	/*
	 * If we're interested in the reserved space, return the size of the
	 * segment itself.  Everything else in this function is a special case
	 * to determine the actual underlying size of various segment types.
	 */
	if (reserved)
		return (size);

	/*
	 * If this is a segvn mapping of a regular file, return the smaller
	 * of the segment size and the remaining size of the file beyond
	 * the file offset corresponding to seg->s_base.
	 */
	if (seg->s_ops == &segvn_ops) {
		vattr_t vattr;
		vnode_t *vp;

		vattr.va_mask = AT_SIZE;

		if (SEGOP_GETVP(seg, seg->s_base, &vp) == 0 &&
		    vp != NULL && vp->v_type == VREG &&
		    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {

			u_offset_t fsize = vattr.va_size;
			u_offset_t offset = SEGOP_GETOFFSET(seg, seg->s_base);

			if (fsize < offset)
				fsize = 0;
			else
				fsize -= offset;

			fsize = roundup(fsize, (u_offset_t)PAGESIZE);

			if (fsize < (u_offset_t)size)
				size = (size_t)fsize;
		}

		return (size);
	}

	/*
	 * If this is an ISM shared segment, don't include pages that are
	 * beyond the real size of the spt segment that backs it.
	 */
	if (seg->s_ops == &segspt_shmops)
		return (MIN(spt_realsize(seg), size));

	/*
	 * If this is segment is a mapping from /dev/null, then this is a
	 * reservation of virtual address space and has no actual size.
	 * Such segments are backed by segdev and have type set to neither
	 * MAP_SHARED nor MAP_PRIVATE.
	 */
	if (seg->s_ops == &segdev_ops && SEGOP_GETTYPE(seg, seg->s_base) == 0)
		return (0);

	/*
	 * If this segment doesn't match one of the special types we handle,
	 * just return the size of the segment itself.
	 */
	return (size);
}

uint_t
pr_getprot(struct seg *seg, int reserved, void **tmp,
	caddr_t *saddrp, caddr_t *naddrp, caddr_t eaddr)
{
	struct as *as = seg->s_as;

	caddr_t saddr = *saddrp;
	caddr_t naddr;

	int check_noreserve;
	uint_t prot;

	union {
		struct segvn_data *svd;
		struct segdev_data *sdp;
		void *data;
	} s;

	s.data = seg->s_data;

	/*
	 * Even though we are performing a read-only operation, we
	 * must have acquired the address space writer lock because
	 * the as_setprot() function only acquires the readers lock.
	 * The reason as_setprot() does this is lost in mystification,
	 * but a lot of people think changing it to acquire the writer
	 * lock would severely impact some application's performance.
	 */
	ASSERT(AS_WRITE_HELD(as, &as->a_lock));
	ASSERT(saddr >= seg->s_base && saddr < eaddr);
	ASSERT(eaddr <= seg->s_base + seg->s_size);

	/*
	 * Don't include MAP_NORESERVE pages in the address range
	 * unless their mappings have actually materialized.
	 * We cheat by knowing that segvn is the only segment
	 * driver that supports MAP_NORESERVE.
	 */
	check_noreserve =
	    (!reserved && seg->s_ops == &segvn_ops && s.svd != NULL &&
	    (s.svd->vp == NULL || s.svd->vp->v_type != VREG) &&
	    (s.svd->flags & MAP_NORESERVE));

	/*
	 * Examine every page only as a last resort.  We use guilty knowledge
	 * of segvn and segdev to avoid this: if there are no per-page
	 * protections present in the segment and we don't care about
	 * MAP_NORESERVE, then s_data->prot is the prot for the whole segment.
	 */
	if (!check_noreserve && saddr == seg->s_base &&
	    seg->s_ops == &segvn_ops && s.svd != NULL && s.svd->pageprot == 0) {
		prot = s.svd->prot;
		getwatchprot(as, saddr, &prot);
		naddr = eaddr;

	} else if (!check_noreserve && saddr == seg->s_base && seg->s_ops ==
	    &segdev_ops && s.sdp != NULL && s.sdp->pageprot == 0) {
		prot = s.sdp->prot;
		getwatchprot(as, saddr, &prot);
		naddr = eaddr;

	} else {
		prpagev_t *pagev;

		/*
		 * If addr is sitting at the start of the segment, then
		 * create a page vector to store protection and incore
		 * information for pages in the segment, and fill it.
		 * Otherwise, we expect *tmp to address the prpagev_t
		 * allocated by a previous call to this function.
		 */
		if (saddr == seg->s_base) {
			pagev = pr_pagev_create(seg, check_noreserve);
			saddr = pr_pagev_fill(pagev, seg, saddr, eaddr);

			ASSERT(*tmp == NULL);
			*tmp = pagev;

			ASSERT(saddr <= eaddr);
			*saddrp = saddr;

			if (saddr == eaddr) {
				naddr = saddr;
				prot = 0;
				goto out;
			}

		} else {
			ASSERT(*tmp != NULL);
			pagev = (prpagev_t *)*tmp;
		}

		naddr = pr_pagev_nextprot(pagev, seg, saddrp, eaddr, &prot);
		ASSERT(naddr <= eaddr);
	}

out:
	if (naddr == eaddr && *tmp != NULL) {
		pr_pagev_destroy((prpagev_t *)*tmp);
		*tmp = NULL;
	}
	*naddrp = naddr;
	return (prot);
}

/*
 * Return true iff the vnode is a /proc file from the object directory.
 */
int
pr_isobject(vnode_t *vp)
{
	return (vp->v_op == &prvnodeops &&
	    VTOP(vp)->pr_type == PR_OBJECT);
}

/*
 * Return true iff the vnode is a /proc file opened by the process itself.
 */
int
pr_isself(vnode_t *vp)
{
	/*
	 * XXX: To retain binary compatibility with the old
	 * ioctl()-based version of /proc, we exempt self-opens
	 * of /proc/<pid> from being marked close-on-exec.
	 */
	return (vp->v_op == &prvnodeops &&
	    (VTOP(vp)->pr_flags & PR_SELF) &&
	    VTOP(vp)->pr_type != PR_PIDDIR);
}

/*
 * To calculate the amount of resident memory a process has, we need to sum
 * the amount of anonymous memory that is resident, and the number of vnode
 * pages mapped into the segment that are not shared with another process.
 * Copy on write makes this more difficult, because if a page is touched
 * in the data segment then a private copy is made in anon memory.
 * But there may be both the vnode page for that offset and/or the cow page.
 * The anonvnode measure is a count of the amount of anon pages (probably
 * from cow) that do have a vnode page as well as the anon cow page in memory.
 *
 * Therefore, real memory
 * = anon_in_core + vnode incore - vnode shared - anonvnode - anonvnodeshared
 * = pr_anon + pr_vnode - pr_vnodeshared - pr_anonshared
 *
 */
static void
pr_getanon(struct seg *seg, caddr_t saddr, caddr_t eaddr, prxmap_t *prxmap)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct anon_map *amp;
	struct anon *ap;
	ulong_t pn;
	ulong_t epn;
	u_offset_t voffbase;
	u_offset_t voffset;
	u_offset_t aoffset;
	vnode_t *vvnode;
	vnode_t *avnode;
	struct page *vpp;
	struct page *app;
	ulong_t anonvnodeshared = 0;
	ulong_t anonvnode = 0;
	int is_anon;

	ASSERT(seg->s_ops == &segvn_ops);

	SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);

	if (svd->amp == NULL && svd->vp == NULL) {
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		return;	/* no anonymous pages created yet or not segvn */
	}

	vvnode = svd->vp;
	amp = svd->amp;
	pn = seg_page(seg, saddr);
	epn = seg_page(seg, eaddr);
	voffbase = svd->offset + (saddr - seg->s_base);

	for (; pn < epn; pn++) {
		/*
		 * Find the anonymous map for this mapping, and count the pages
		 */
		avnode = NULL;

		if (amp != NULL) {
			mutex_enter(&amp->lock);
			ap = anon_get_ptr(amp->ahp, svd->anon_index + pn);
			if (ap != NULL)
				swap_xlate(ap, &avnode, &aoffset);
			mutex_exit(&amp->lock);
		}

		voffset = voffbase + ((u_offset_t)pn << PAGESHIFT);
		is_anon = 0;

		/*
		 * At this point we have avnode and aoffset set for the
		 * anon page (if exists) and vvnode and voffset set for
		 * the vnode page.
		 */
		if (avnode != NULL) {
			app = page_lookup_nowait(avnode, aoffset, SE_SHARED);
			if (app != NULL) {
				if (!page_isfree(app)) {
					is_anon = 1;
					prxmap->pr_anon++;
					if (page_isshared(app))
						prxmap->pr_ashared++;
					if (page_ismod(app))
						prxmap->pr_amod++;
					if (page_isref(app))
						prxmap->pr_aref++;
				}
				page_unlock(app);
			} else {
				if (page_exists(avnode, aoffset)) {
					prxmap->pr_anon++;
					is_anon = 1;
				}
			}
		}

		/*
		 * Now do the same check for vnode pages at this address.
		 * We bump anonvnode and anonvnodeshared by is_anon (0 or 1)
		 * so that the anon page counts can be deducted from the
		 * the final pr_vnode and pr_vshared counts below.
		 */
		if (vvnode != NULL) {
			vpp = page_lookup_nowait(vvnode, voffset, SE_SHARED);
			if (vpp != NULL) {
				if (!page_isfree(vpp)) {
					prxmap->pr_vnode++;
					anonvnode += is_anon;
					if (page_isshared(vpp)) {
						prxmap->pr_vshared++;
						anonvnodeshared += is_anon;
					}
					if (page_ismod(vpp))
						prxmap->pr_vmod++;
					if (page_isref(vpp))
						prxmap->pr_vref++;
				}
				page_unlock(vpp);

			} else if (page_exists(vvnode, voffset)) {
				prxmap->pr_vnode++;
				anonvnode += is_anon;
			}
		}

	}

	prxmap->pr_vnode -= anonvnode;
	prxmap->pr_vshared -= anonvnodeshared;

	ASSERT(prxmap->pr_vnode >= prxmap->pr_vshared);
	ASSERT(prxmap->pr_anon >= prxmap->pr_ashared);

	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
}

static void
pr_getsptanon(struct seg *seg, caddr_t saddr, caddr_t eaddr, prxmap_t *prxmap)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct anon_map *amp;
	struct anon *ap;
	ulong_t pn;
	ulong_t epn;
	u_offset_t aoffset;
	vnode_t *avnode;
	struct page *app;

	ASSERT(seg->s_ops == &segspt_shmops);

	if ((amp = ssd->amp) == NULL)
		return;

	/*
	 * If this is an ISM shared segment, don't include pages that
	 * are beyond the real size of the spt segment that backs it.
	 * We just have to adjust eaddr to match the real size.
	 */
	if (eaddr > seg->s_base + spt_realsize(seg))
		eaddr = seg->s_base + spt_realsize(seg);

	pn = seg_page(seg, saddr);
	epn = seg_page(seg, eaddr);

	for (; pn < epn; pn++) {
		/*
		 * Find the anonymous map for this mapping, and count the pages
		 */
		avnode = NULL;

		mutex_enter(&amp->lock);
		ap = anon_get_ptr(amp->ahp, pn);
		if (ap != NULL)
			swap_xlate(ap, &avnode, &aoffset);
		mutex_exit(&amp->lock);

		/*
		 * At this point we have avnode and aoffset set for the
		 * anon page (if exists)
		 */
		if (avnode != NULL) {
			app = page_lookup_nowait(avnode, aoffset, SE_SHARED);
			if (app != NULL) {
				if (!page_isfree(app)) {
					prxmap->pr_anon++;
					if (page_isshared(app))
						prxmap->pr_ashared++;
					if (page_ismod(app))
						prxmap->pr_amod++;
					if (page_isref(app))
						prxmap->pr_aref++;
				}
				page_unlock(app);
			} else {
				if (page_exists(avnode, aoffset))
					prxmap->pr_anon++;
			}
		}
	}
}

/*
 * Return an array of structures with extended memory map information.
 * We allocate here; the caller must deallocate.
 */
int
prgetxmap(proc_t *p, prxmap_t **prxmapp, size_t *sizep)
{
	struct as *as = p->p_as;
	int nmaps = 0;
	prxmap_t *mp;
	size_t size;
	struct seg *seg;
	struct seg *brkseg, *stkseg;
	struct vnode *vp;
	struct vattr vattr;
	uint_t prot;

	ASSERT(as != &kas && AS_WRITE_HELD(as, &as->a_lock));

	/* initial allocation */
	*sizep = size = MAPSIZE;
	*prxmapp = mp = kmem_alloc(MAPSIZE, KM_SLEEP);

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL)
		return (0);

	brkseg = break_seg(p);
	stkseg = as_segat(as, prgetstackbase(p));

	do {
		caddr_t eaddr = seg->s_base + pr_getsegsize(seg, 0);
		caddr_t saddr, naddr;
		void *tmp = NULL;

		for (saddr = seg->s_base; saddr < eaddr; saddr = naddr) {
			prot = pr_getprot(seg, 0, &tmp, &saddr, &naddr, eaddr);
			if (saddr == naddr)
				continue;
			/* reallocate if necessary */
			if ((nmaps + 1) * sizeof (prxmap_t) > size) {
				size_t newsize = size + MAPSIZE;
				prxmap_t *newmp = kmem_alloc(newsize, KM_SLEEP);

				bcopy(*prxmapp, newmp, nmaps*sizeof (prxmap_t));
				kmem_free(*prxmapp, size);
				*sizep = size = newsize;
				*prxmapp = newmp;
				mp = newmp + nmaps;
			}
			bzero(mp, sizeof (*mp));
			mp->pr_vaddr = (uintptr_t)saddr;
			mp->pr_size = naddr - saddr;
			mp->pr_offset = SEGOP_GETOFFSET(seg, saddr);
			mp->pr_mflags = 0;
			if (prot & PROT_READ)
				mp->pr_mflags |= MA_READ;
			if (prot & PROT_WRITE)
				mp->pr_mflags |= MA_WRITE;
			if (prot & PROT_EXEC)
				mp->pr_mflags |= MA_EXEC;
			if (SEGOP_GETTYPE(seg, saddr) == MAP_SHARED)
				mp->pr_mflags |= MA_SHARED;
			if (seg->s_ops == &segspt_shmops ||
			    (seg->s_ops == &segvn_ops &&
			    (SEGOP_GETVP(seg, saddr, &vp) != 0 || vp == NULL)))
				mp->pr_mflags |= MA_ANON;
			if (seg == brkseg)
				mp->pr_mflags |= MA_BREAK;
			else if (seg == stkseg)
				mp->pr_mflags |= MA_STACK;
			if (seg->s_ops == &segspt_shmops)
				mp->pr_mflags |= MA_ISM;
			mp->pr_pagesize = PAGESIZE;

			/*
			 * Manufacture a filename for the "object" directory.
			 */
			mp->pr_dev = PRNODEV;
			vattr.va_mask = AT_FSID|AT_NODEID;
			if (seg->s_ops == &segvn_ops &&
			    SEGOP_GETVP(seg, saddr, &vp) == 0 &&
			    vp != NULL && vp->v_type == VREG &&
			    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {
				mp->pr_dev = vattr.va_fsid;
				mp->pr_ino = vattr.va_nodeid;
				if (vp == p->p_exec)
					(void) strcpy(mp->pr_mapname, "a.out");
				else
					pr_object_name(mp->pr_mapname,
					    vp, &vattr);
			}

			/*
			 * Get the SysV shared memory id, if any.
			 */
			if ((mp->pr_mflags & MA_SHARED) && p->p_segacct)
				mp->pr_shmid = shmgetid(p, saddr);
			else
				mp->pr_shmid = -1;

			if (seg->s_ops == &segvn_ops)
				pr_getanon(seg, saddr, naddr, mp);
			else if (seg->s_ops == &segspt_shmops)
				pr_getsptanon(seg, saddr, naddr, mp);

			mp++;
			nmaps++;
		}
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	return (nmaps);
}

/*
 * Return the process's credentials.  We don't need a 32-bit equivalent of
 * this function because prcred_t and prcred32_t are actually the same.
 */
void
prgetcred(proc_t *p, prcred_t *pcrp)
{
	cred_t *cp;
	int i;

	mutex_enter(&p->p_crlock);
	cp = p->p_cred;

	pcrp->pr_euid = cp->cr_uid;
	pcrp->pr_ruid = cp->cr_ruid;
	pcrp->pr_suid = cp->cr_suid;
	pcrp->pr_egid = cp->cr_gid;
	pcrp->pr_rgid = cp->cr_rgid;
	pcrp->pr_sgid = cp->cr_sgid;
	pcrp->pr_ngroups = MIN(cp->cr_ngroups, (uint_t)ngroups_max);
	pcrp->pr_groups[0] = 0;	/* in case ngroups == 0 */

	for (i = 0; i < pcrp->pr_ngroups; i++)
		pcrp->pr_groups[i] = cp->cr_groups[i];

	mutex_exit(&p->p_crlock);
}

#ifdef _SYSCALL32_IMPL
static void
pr_getanon32(struct seg *seg, caddr_t saddr, caddr_t eaddr, prxmap32_t *prxmap)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct anon_map *amp;
	struct anon *ap;
	ulong_t pn;
	ulong_t epn;
	u_offset_t voffbase;
	u_offset_t voffset;
	u_offset_t aoffset;
	vnode_t *vvnode;
	vnode_t *avnode;
	struct page *vpp;
	struct page *app;
	ulong_t anonvnodeshared = 0;
	ulong_t anonvnode = 0;
	int is_anon;

	ASSERT(seg->s_ops == &segvn_ops);

	SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);

	if (svd->amp == NULL && svd->vp == NULL) {
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		return;	/* no anonymous pages created yet or not segvn */
	}

	vvnode = svd->vp;
	amp = svd->amp;
	pn = seg_page(seg, saddr);
	epn = seg_page(seg, eaddr);
	voffbase = svd->offset + (saddr - seg->s_base);

	for (; pn < epn; pn++) {
		/*
		 * Find the anonymous map for this mapping, and count the pages
		 */
		avnode = NULL;

		if (amp != NULL) {
			mutex_enter(&amp->lock);
			ap = anon_get_ptr(amp->ahp, svd->anon_index + pn);
			if (ap != NULL)
				swap_xlate(ap, &avnode, &aoffset);
			mutex_exit(&amp->lock);
		}

		voffset = voffbase + ((u_offset_t)pn << PAGESHIFT);
		is_anon = 0;

		/*
		 * At this point we have avnode and aoffset set for the
		 * anon page (if exists) and vvnode and voffset set for
		 * the vnode page.
		 */
		if (avnode != NULL) {
			app = page_lookup_nowait(avnode, aoffset, SE_SHARED);
			if (app != NULL) {
				if (!page_isfree(app)) {
					is_anon = 1;
					prxmap->pr_anon++;
					if (page_isshared(app))
						prxmap->pr_ashared++;
					if (page_ismod(app))
						prxmap->pr_amod++;
					if (page_isref(app))
						prxmap->pr_aref++;
				}
				page_unlock(app);
			} else {
				if (page_exists(avnode, aoffset)) {
					prxmap->pr_anon++;
					is_anon = 1;
				}
			}
		}

		/*
		 * Now do the same check for vnode pages at this address.
		 * We bump anonvnode and anonvnodeshared by is_anon (0 or 1)
		 * so that the anon page counts can be deducted from the
		 * the final pr_vnode and pr_vshared counts below.
		 */
		if (vvnode != NULL) {
			vpp = page_lookup_nowait(vvnode, voffset, SE_SHARED);
			if (vpp != NULL) {
				if (!page_isfree(vpp)) {
					prxmap->pr_vnode++;
					anonvnode += is_anon;
					if (page_isshared(vpp)) {
						prxmap->pr_vshared++;
						anonvnodeshared += is_anon;
					}
					if (page_ismod(vpp))
						prxmap->pr_vmod++;
					if (page_isref(vpp))
						prxmap->pr_vref++;
				}
				page_unlock(vpp);

			} else if (page_exists(vvnode, voffset)) {
				prxmap->pr_vnode++;
				anonvnode += is_anon;
			}
		}
	}

	prxmap->pr_vnode -= anonvnode;
	prxmap->pr_vshared -= anonvnodeshared;

	ASSERT(prxmap->pr_vnode >= prxmap->pr_vshared);
	ASSERT(prxmap->pr_anon >= prxmap->pr_ashared);

	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
}

static void
pr_getsptanon32(struct seg *seg,
    caddr_t saddr, caddr_t eaddr, prxmap32_t *prxmap)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct anon_map *amp;
	struct anon *ap;
	ulong_t pn;
	ulong_t epn;
	u_offset_t aoffset;
	vnode_t *avnode;
	struct page *app;

	ASSERT(seg->s_ops == &segspt_shmops);

	if ((amp = ssd->amp) == NULL)
		return;

	/*
	 * If this is an ISM shared segment, don't include pages that
	 * are beyond the real size of the spt segment that backs it.
	 * We just have to adjust eaddr to match the real size.
	 */
	if (eaddr > seg->s_base + spt_realsize(seg))
		eaddr = seg->s_base + spt_realsize(seg);

	pn = seg_page(seg, saddr);
	epn = seg_page(seg, eaddr);

	for (; pn < epn; pn++) {
		/*
		 * Find the anonymous map for this mapping, and count the pages
		 */
		avnode = NULL;

		mutex_enter(&amp->lock);
		ap = anon_get_ptr(amp->ahp, pn);
		if (ap != NULL)
			swap_xlate(ap, &avnode, &aoffset);
		mutex_exit(&amp->lock);

		/*
		 * At this point we have avnode and aoffset set for the
		 * anon page (if exists)
		 */
		if (avnode != NULL) {
			app = page_lookup_nowait(avnode, aoffset, SE_SHARED);
			if (app != NULL) {
				if (!page_isfree(app)) {
					prxmap->pr_anon++;
					if (page_isshared(app))
						prxmap->pr_ashared++;
					if (page_ismod(app))
						prxmap->pr_amod++;
					if (page_isref(app))
						prxmap->pr_aref++;
				}
				page_unlock(app);
			} else {
				if (page_exists(avnode, aoffset))
					prxmap->pr_anon++;
			}
		}
	}
}

/*
 * Return an array of structures with extended memory map information.
 * We allocate here; the caller must deallocate.
 */
int
prgetxmap32(proc_t *p, prxmap32_t **prxmapp, size_t *sizep)
{
	struct as *as = p->p_as;
	int nmaps = 0;
	prxmap32_t *mp;
	size_t size;
	struct seg *seg;
	struct seg *brkseg, *stkseg;
	struct vnode *vp;
	struct vattr vattr;
	uint_t prot;

	ASSERT(as != &kas && AS_WRITE_HELD(as, &as->a_lock));

	/* initial allocation */
	*sizep = size = MAPSIZE;
	*prxmapp = mp = kmem_alloc(MAPSIZE, KM_SLEEP);

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL)
		return (0);

	brkseg = break_seg(p);
	stkseg = as_segat(as, prgetstackbase(p));

	do {
		caddr_t eaddr = seg->s_base + pr_getsegsize(seg, 0);
		caddr_t saddr, naddr;
		void *tmp = NULL;

		for (saddr = seg->s_base; saddr < eaddr; saddr = naddr) {
			prot = pr_getprot(seg, 0, &tmp, &saddr, &naddr, eaddr);
			if (saddr == naddr)
				continue;
			/* reallocate if necessary */
			if ((nmaps + 1) * sizeof (prxmap32_t) > size) {
				size_t newsize = size + MAPSIZE;
				prxmap32_t *newmp =
				    kmem_alloc(newsize, KM_SLEEP);

				bcopy(*prxmapp, newmp,
				    nmaps * sizeof (prxmap32_t));
				kmem_free(*prxmapp, size);
				*sizep = size = newsize;
				*prxmapp = newmp;
				mp = newmp + nmaps;
			}
			bzero(mp, sizeof (*mp));
			mp->pr_vaddr = (caddr32_t)saddr;
			mp->pr_size = (size32_t)(naddr - saddr);
			mp->pr_offset = SEGOP_GETOFFSET(seg, saddr);
			mp->pr_mflags = 0;
			if (prot & PROT_READ)
				mp->pr_mflags |= MA_READ;
			if (prot & PROT_WRITE)
				mp->pr_mflags |= MA_WRITE;
			if (prot & PROT_EXEC)
				mp->pr_mflags |= MA_EXEC;
			if (SEGOP_GETTYPE(seg, saddr) == MAP_SHARED)
				mp->pr_mflags |= MA_SHARED;
			if (seg->s_ops == &segspt_shmops ||
			    (seg->s_ops == &segvn_ops &&
			    (SEGOP_GETVP(seg, saddr, &vp) != 0 || vp == NULL)))
				mp->pr_mflags |= MA_ANON;
			if (seg == brkseg)
				mp->pr_mflags |= MA_BREAK;
			else if (seg == stkseg)
				mp->pr_mflags |= MA_STACK;
			if (seg->s_ops == &segspt_shmops)
				mp->pr_mflags |= MA_ISM;
			mp->pr_pagesize = PAGESIZE;

			/*
			 * Manufacture a filename for the "object" directory.
			 */
			mp->pr_dev = PRNODEV32;
			vattr.va_mask = AT_FSID|AT_NODEID;
			if (seg->s_ops == &segvn_ops &&
			    SEGOP_GETVP(seg, saddr, &vp) == 0 &&
			    vp != NULL && vp->v_type == VREG &&
			    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {
				(void) cmpldev(&mp->pr_dev, vattr.va_fsid);
				mp->pr_ino = vattr.va_nodeid;
				if (vp == p->p_exec)
					(void) strcpy(mp->pr_mapname, "a.out");
				else
					pr_object_name(mp->pr_mapname,
					    vp, &vattr);
			}

			/*
			 * Get the SysV shared memory id, if any.
			 */
			if ((mp->pr_mflags & MA_SHARED) && p->p_segacct)
				mp->pr_shmid = shmgetid(p, saddr);
			else
				mp->pr_shmid = -1;

			if (seg->s_ops == &segvn_ops)
				pr_getanon32(seg, saddr, naddr, mp);
			else if (seg->s_ops == &segspt_shmops)
				pr_getsptanon32(seg, saddr, naddr, mp);

			mp++;
			nmaps++;
		}
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	return (nmaps);
}
#endif	/* _SYSCALL32_IMPL */
