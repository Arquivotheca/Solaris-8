/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)exit.c	1.107	99/09/23 SMI"	/* from SVr4.0 1.74 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/ucontext.h>
#include <sys/procfs.h>
#include <sys/vnode.h>
#include <sys/acct.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/wait.h>
#include <sys/siginfo.h>
#include <sys/procset.h>
#include <sys/class.h>
#include <sys/file.h>
#include <sys/session.h>
#include <sys/kmem.h>
#include <sys/vtrace.h>
#include <sys/prsystm.h>
#include <sys/acct.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <c2/audit.h>
#include <sys/aio_impl.h>
#include <vm/as.h>
#include <sys/poll.h>
#include <sys/door.h>
#include <sys/lwpchan_impl.h>
#include <sys/utrap.h>
#include <sys/rce.h>

#if defined(i386) || defined(__ia64)
extern void ldt_free(proc_t *pp);
#endif /* defined(i386) || defined(__ia64) */

/*
 * convert code/data pair into old style wait status
 */
int
wstat(int code, int data)
{
	int stat = (data & 0377);

	switch (code) {
	case CLD_EXITED:
		stat <<= 8;
		break;
	case CLD_DUMPED:
		stat |= WCOREFLG;
		break;
	case CLD_KILLED:
		break;
	case CLD_TRAPPED:
	case CLD_STOPPED:
		stat <<= 8;
		stat |= WSTOPFLG;
		break;
	case CLD_CONTINUED:
		stat = WCONTFLG;
		break;
	default:
		cmn_err(CE_PANIC, "wstat: bad code");
		/* NOTREACHED */
	}
	return (stat);
}

/*
 * exit system call: pass back caller's arg.
 */
void
rexit(int rval)
{
	exit(CLD_EXITED, rval);
}

/*
 * Release resources.
 * Enter zombie state.
 * Wake up parent and init processes,
 * and dispose of children.
 */
void
exit(int why, int what)
{
	kthread_t *t = curthread;
	klwp_t *lwp = ttolwp(t);
	proc_t *p = ttoproc(t);
	timeout_id_t tmp_id;
	int rv;
	proc_t *q;
	sess_t *sp;
	vnode_t *exec_vp, *cdir, *rdir;
	sigqueue_t *sqp;

	/*
	 * Stop and discard the process's lwps except for the current one,
	 * unless someone beat us to it.  In that case, t_post_sys will have
	 * been set by exitlwps() so that we'll lwp_exit() in post_syscall().
	 */
	if (exitlwps(0) != 0)
		return;

	if (p->p_zomb_tid != NULL) {	/* free the zombie array */
		kmem_free(p->p_zomb_tid, p->p_zomb_max * sizeof (id_t));
		p->p_zomb_tid = NULL;
		p->p_zomb_max = 0;
		p->p_zombcnt = 0;
	}

	/*
	 * Allocate a sigqueue now, before we grab locks.
	 * It will be given to sigcld(), below.
	 */
	sqp = kmem_zalloc(sizeof (sigqueue_t), KM_SLEEP);

	/*
	 * revoke any doors created by the process.
	 */
	if (p->p_door_list)
		door_exit();

	/*
	 * release scheduler activations door (if any).
	 */
	if (p->p_sc_door) {
		VN_RELE(p->p_sc_door);
		p->p_sc_door = NULL;
	}

	/*
	 * make sure all pending kaio has completed.
	 */
	if (p->p_aio)
		aio_cleanup_exit();

	/*
	 * discard lwpchan cache.
	 */
	if (p->p_lcp)
		lwpchan_destroy_cache();

	/* untimeout the realtime timers */
	if (p->p_itimer != NULL)
		timer_exit();

	if ((tmp_id = p->p_alarmid) != 0) {
		p->p_alarmid = 0;
		(void) untimeout(tmp_id);
	}

	/*
	 * this must come before closeall(1).
	 */
	pollcleanup();

	mutex_enter(&p->p_lock);
	while ((tmp_id = p->p_rprof_timerid) != 0) {
		p->p_rprof_timerid = 0;
		mutex_exit(&p->p_lock);
		(void) untimeout(tmp_id);
		mutex_enter(&p->p_lock);
	}

	lwp_cleanup();

	/*
	 * Block the process against /proc now that we have really
	 * acquired p->p_lock (to manipulate p_tlist at least).
	 */
	prbarrier(p);

#ifdef	SUN_SRC_COMPAT
	if (code == CLD_KILLED)
		u.u_acflag |= AXSIG;
#endif
	sigfillset(&p->p_ignore);
	sigemptyset(&p->p_siginfo);
	sigemptyset(&p->p_sig);
	sigemptyset(&t->t_sig);
	sigemptyset(&p->p_sigmask);
	sigdelq(p, t, 0);
	lwp->lwp_cursig = 0;
	p->p_flag &= ~SKILLED;
	if (lwp->lwp_curinfo) {
		siginfofree(lwp->lwp_curinfo);
		lwp->lwp_curinfo = NULL;
	}

	t->t_proc_flag |= TP_LWPEXIT;
	ASSERT(p->p_lwpcnt == 1);
	p->p_lwpcnt = 0;
	p->p_tlist = NULL;
	sigqfree(p);
	term_mstate(t);
	p->p_mterm = gethrtime();
	prlwpexit(t);		/* notify /proc */
	prexit(p);
	if (p->p_exec) {
		exec_vp = p->p_exec;
		p->p_exec = NULLVP;
		mutex_exit(&p->p_lock);
		VN_RELE(exec_vp);
	} else {
		mutex_exit(&p->p_lock);
	}
	if (p->p_as->a_wpage)
		pr_free_my_pagelist();

	closeall(P_FINFO(p));

	mutex_enter(&pidlock);
	sp = p->p_sessp;
	if (sp->s_sidp == p->p_pidp && sp->s_vp != NULL) {
		mutex_exit(&pidlock);
		freectty(sp);
	} else
		mutex_exit(&pidlock);

	/*
	 * Insert calls to "exitfunc" functions.
	 * XXX - perhaps these should go in a configurable table,
	 * as is done with the init functions.
	 */
#if defined(__ia64)
	if (is_ia32_process(p)) {
		if (! (p->p_flag & SVFORK)) {
			unmap_ia32_ldt(p->p_as, (caddr_t)IA32_LDT_ADDR,
					(p->p_ldtlimit + 1)
					* sizeof (struct dscr));
		}
		/* If the process was using a private LDT then free it */
		if (p->p_ldt)
			ldt_free(p);
	}
#endif /* defined(__ia64) */
#if defined(i386)
	/* If the process was using a private LDT then free it */
	if (p->p_ldt)
		ldt_free(p);
#endif /* defined(i386) */
#ifdef __sparcv9cpu
	if (p->p_utraps != NULL)
		utrap_free(p);
#endif
	semexit();		/* IPC semaphore exit */
	rv = wstat(why, what);

#ifdef SYSACCT
	acct(rv & 0xff);
#endif

	/*
	 * Release any resources associated with C2 auditing
	 */
#ifdef C2_AUDIT
	if (audit_active) {
		/*
		 * audit exit system call
		 */
		audit_exit(why, what);
	}
#endif

	p->p_utime += p->p_cutime;
	p->p_stime += p->p_cstime;

	/*
	 * Free address space.
	 */
	relvm();

	/*
	 * SRM hook: for the last update of SRM per-process resource
	 * usage of the exiting process and for SRM accounting of "login"
	 * processes.
	 * This hook call is made here to occur as soon as the process
	 * has freed file/memory resources and is doomed; we don't want to
	 * delay logoff accounting until the process is reaped.
	 */
	SRM_EXIT(p);

	mutex_enter(&pidlock);

	/*
	 * Delete this process from the newstate list of its parent. We
	 * will put it in the right place in the sigcld in the end.
	 */
	delete_ns(p->p_parent, p);

	/*
	 * Reassign the orphans to the next of kin.
	 * Don't rearrange init's orphanage.
	 */
	if ((q = p->p_orphan) != NULL && p != proc_init) {

		proc_t *nokp = p->p_nextofkin;

		for (;;) {
			q->p_nextofkin = nokp;
			if (q->p_nextorph == NULL)
				break;
			q = q->p_nextorph;
		}
		q->p_nextorph = nokp->p_orphan;
		nokp->p_orphan = p->p_orphan;
		p->p_orphan = NULL;
	}

	/*
	 * Reassign the children to init.
	 * Don't try to assign init's children to init.
	 */
	if ((q = p->p_child) != NULL && p != proc_init) {
		struct proc	*np;
		struct proc	*initp = proc_init;

		pgdetach(p);

		do {
			np = q->p_sibling;
			/*
			 * Delete it from its current parent new state
			 * list and add it to init new state list
			 */
			delete_ns(q->p_parent, q);

			q->p_ppid = 1;
			q->p_parent = initp;

			/*
			 * Since q will be the first child,
			 * it will not have a previous sibling.
			 */
			q->p_psibling = NULL;
			if (initp->p_child) {
				initp->p_child->p_psibling = q;
			}
			q->p_sibling = initp->p_child;
			initp->p_child = q;
			if (q->p_flag & STRC) {
				mutex_enter(&q->p_lock);
				sigtoproc(q, NULL, SIGKILL);
				mutex_exit(&q->p_lock);
			}
			/*
			 * sigcld() will add the child to parents
			 * newstate list.
			 */
			if (q->p_stat == SZOMB)
				sigcld(q, NULL);
		} while ((q = np) != NULL);

		p->p_child = NULL;
		ASSERT(p->p_child_ns == NULL);
	}

	TRACE_1(TR_FAC_PROC, TR_PROC_EXIT, "proc_exit:pid %d", p->p_pid);

	CL_EXIT(curthread); /* tell the scheduler that curthread is exiting */

	mutex_enter(&p->p_lock);
	p->p_stat = SZOMB;
	p->p_flag &= ~STRC;
	p->p_wdata = what;
	p->p_wcode = (char)why;

	cdir = PTOU(p)->u_cdir;
	rdir = PTOU(p)->u_rdir;

	/*
	 * curthread's proc pointer is changed to point at p0 because
	 * curthread's original proc pointer can be freed as soon as
	 * the child sends a SIGCLD to its parent.
	 */
	t->t_procp = &p0;
	mutex_exit(&p->p_lock);
	sigcld(p, sqp);
	mutex_exit(&pidlock);

	/*
	 * We don't release u_cdir and u_rdir until SZOMB is set.
	 * This protects us against dofusers().
	 */
	VN_RELE(cdir);
	if (rdir)
		VN_RELE(rdir);

	thread_exit();
	/* NOTREACHED */
}

/*
 * Format siginfo structure for wait system calls.
 */
void
winfo(proc_t *pp, k_siginfo_t *ip, int waitflag)
{
	ASSERT(MUTEX_HELD(&pidlock));

	bzero(ip, sizeof (k_siginfo_t));
	ip->si_signo = SIGCLD;
	ip->si_code = pp->p_wcode;
	ip->si_pid = pp->p_pid;
	ip->si_status = pp->p_wdata;
	ip->si_stime = pp->p_stime;
	ip->si_utime = pp->p_utime;

	if (waitflag) {
		pp->p_wcode = 0;
		pp->p_wdata = 0;
		pp->p_pidflag &= ~CLDPEND;
	}
}

/*
 * Wait system call.
 * Search for a terminated (zombie) child,
 * finally lay it to rest, and collect its status.
 * Look also for stopped children,
 * and pass back status from them.
 */
int
waitid(idtype_t idtype, id_t id, k_siginfo_t *ip, int options)
{
	int found;
	proc_t *cp, *pp;
	proc_t **nsp;
	int proc_gone;
	int waitflag = !(options & WNOWAIT);
	int wnochld = (options & _WNOCHLD);	/* this should always be true */
						/* but !#?* Posix says no */

	if (options == 0 || (options & ~WOPTMASK))
		return (EINVAL);

	switch (idtype) {
	case P_PID:
	case P_PGID:
		if (id < 0 || id >= maxpid)
			return (EINVAL);
		/* FALLTHROUGH */
	case P_ALL:
		break;
	default:
		return (EINVAL);
	}

	pp = ttoproc(curthread);
	/*
	 * lock parent mutex so that sibling chain can be searched.
	 */
	mutex_enter(&pidlock);
	while ((cp = pp->p_child) != NULL) {

		proc_gone = 0;

		for (nsp = &pp->p_child_ns; *nsp; nsp = &(*nsp)->p_sibling_ns) {
			if (idtype == P_PID && id != (*nsp)->p_pid) {
				continue;
			}
			if (idtype == P_PGID && id != (*nsp)->p_pgrp) {
				continue;
			}

			switch ((*nsp)->p_wcode) {

			case CLD_TRAPPED:
			case CLD_STOPPED:
			case CLD_CONTINUED:
				cmn_err(CE_PANIC,
				    "waitid: wrong state %d on the p_newstate"
				    " list", (*nsp)->p_wcode);
				break;

			case CLD_EXITED:
			case CLD_DUMPED:
			case CLD_KILLED:
				if (!(options & WEXITED)) {
					/*
					 * Count how many are already gone
					 * for good.
					 */
					proc_gone++;
					break;
				}
				if (!waitflag) {
					winfo((*nsp), ip, 0);
				} else {
					proc_t *xp = *nsp;
					winfo(xp, ip, 1);
					freeproc(xp);
				}
				mutex_exit(&pidlock);
				if (waitflag && wnochld) {
					sigcld_delete(ip);
					sigcld_repost();
				}
				return (0);
			}

			if (idtype == P_PID)
				break;
		}

		/*
		 * Wow! None of the threads on the p_sibling_ns list were
		 * interesting threads. Check all the kids!
		 */
		found = 0;
		cp = pp->p_child;
		do {
			if (idtype == P_PID && id != cp->p_pid) {
				continue;
			}
			if (idtype == P_PGID && id != cp->p_pgrp) {
				continue;
			}

			found++;

			switch (cp->p_wcode) {
			case CLD_TRAPPED:
				if (!(options & WTRAPPED))
					break;
				winfo(cp, ip, waitflag);
				mutex_exit(&pidlock);
				if (waitflag && wnochld) {
					sigcld_delete(ip);
					sigcld_repost();
				}
				return (0);

			case CLD_STOPPED:
				if (!(options & WSTOPPED))
					break;
				/* Is it still stopped? */
				mutex_enter(&cp->p_lock);
				if (!jobstopped(cp)) {
					mutex_exit(&cp->p_lock);
					break;
				}
				mutex_exit(&cp->p_lock);
				winfo(cp, ip, waitflag);
				mutex_exit(&pidlock);
				if (waitflag && wnochld) {
					sigcld_delete(ip);
					sigcld_repost();
				}
				return (0);

			case CLD_CONTINUED:
				if (!(options & WCONTINUED))
					break;
				winfo(cp, ip, waitflag);
				mutex_exit(&pidlock);
				if (waitflag && wnochld) {
					sigcld_delete(ip);
					sigcld_repost();
				}
				return (0);

			case CLD_EXITED:
			case CLD_DUMPED:
			case CLD_KILLED:
				/*
				 * Don't complain if a process was found in
				 * the first loop but we broke out of the loop
				 * because of the arguments passed to us.
				 */
				if (proc_gone == 0) {
					cmn_err(CE_PANIC,
					    "waitid: wrong state on the"
					    " p_child list");
				} else {
					break;
				}
			}

			if (idtype == P_PID)
				break;
		} while ((cp = cp->p_sibling) != NULL);

		/*
		 * If we found no interesting processes at all,
		 * break out and return ECHILD.
		 */
		if (found + proc_gone == 0)
			break;

		if (options & WNOHANG) {
			bzero(ip, sizeof (k_siginfo_t));
			/*
			 * We should set ip->si_signo = SIGCLD,
			 * but there is an SVVS test that expects
			 * ip->si_signo to be zero in this case.
			 */
			mutex_exit(&pidlock);
			return (0);
		}

		/*
		 * If we found no processes of interest that could
		 * change state while we wait, we don't wait at all.
		 * Get out with ECHILD according to SVID.
		 */
		if (found == proc_gone)
			break;

		if (!cv_wait_sig_swap(&pp->p_cv, &pidlock)) {
			mutex_exit(&pidlock);
			return (EINTR);
		}
	}
	mutex_exit(&pidlock);
	return (ECHILD);
}

/*
 * For implementations that don't require binary compatibility,
 * the wait system call may be made into a library call to the
 * waitid system call.
 */
int64_t
wait(void)
{
	int error;
	k_siginfo_t info;
	rval_t	r;

	if (error =  waitid(P_ALL, (id_t)0, &info, WEXITED|WTRAPPED))
		return (set_errno(error));
	r.r_val1 = info.si_pid;
	r.r_val2 = wstat(info.si_code, info.si_status);
	return (r.r_vals);
}

int
waitsys(idtype_t idtype, id_t id, siginfo_t *infop, int options)
{
	int error;
	k_siginfo_t info;

	if (error = waitid(idtype, id, &info, options))
		return (set_errno(error));
	if (copyout(&info, infop, sizeof (k_siginfo_t)))
		return (set_errno(EFAULT));
	return (0);
}

#ifdef _SYSCALL32_IMPL

int
waitsys32(idtype_t idtype, id_t id, siginfo_t *infop, int options)
{
	int error;
	k_siginfo_t info;
	siginfo32_t info32;

	if (error = waitid(idtype, id, &info, options))
		return (set_errno(error));
	siginfo_kto32(&info, &info32);
	if (copyout(&info32, infop, sizeof (info32)))
		return (set_errno(EFAULT));
	return (0);
}

#endif	/* _SYSCALL32_IMPL */

/*
 * Remove zombie children from the process table.
 */
void
freeproc(proc_t *p)
{
	proc_t *q;

	ASSERT(p->p_stat == SZOMB);
	ASSERT(p->p_tlist == NULL);
	ASSERT(MUTEX_HELD(&pidlock));

	sigdelq(p, NULL, 0);

	prfree(p);	/* inform /proc */

	/*
	 * Don't free the init processes.
	 * Other dying processes will access it.
	 */
	if (p == proc_init)
		return;

	/*
	 * We wait until now to free the cred structure because a
	 * zombie process's credentials may be examined by /proc.
	 * No cred locking needed because there are no threads at this point.
	 */
	upcount_dec(p->p_cred->cr_ruid);
	crfree(p->p_cred);
	refstr_rele(p->p_corefile);
	p->p_corefile = NULL;

	if (p->p_nextofkin) {
		p->p_nextofkin->p_cutime += p->p_utime;
		p->p_nextofkin->p_cstime += p->p_stime;
	}

	q = p->p_nextofkin;
	if (q && q->p_orphan == p)
		q->p_orphan = p->p_nextorph;
	else if (q) {
		for (q = q->p_orphan; q; q = q->p_nextorph)
			if (q->p_nextorph == p)
				break;
		ASSERT(q && q->p_nextorph == p);
		q->p_nextorph = p->p_nextorph;
	}

	q = p->p_parent;
	ASSERT(q != NULL);

	/*
	 * Take it off the newstate list of its parent
	 */
	delete_ns(q, p);

	if (q->p_child == p) {
		q->p_child = p->p_sibling;
		/*
		 * If the parent has no children, it better not
		 * have any with new states either!
		 */
		ASSERT(q->p_child ? 1 : q->p_child_ns == NULL);
	}

	if (p->p_sibling) {
		p->p_sibling->p_psibling = p->p_psibling;
	}

	if (p->p_psibling) {
		p->p_psibling->p_sibling = p->p_sibling;
	}

	pid_exit(p);	/* frees pid and proc structure */
}

/*
 * Delete process "child" from the newstate list of process "parent"
 */
void
delete_ns(proc_t *parent, proc_t *child)
{
	proc_t **ns;

	ASSERT(MUTEX_HELD(&pidlock));
	ASSERT(child->p_parent == parent);
	for (ns = &parent->p_child_ns; *ns != NULL; ns = &(*ns)->p_sibling_ns) {
		if (*ns == child) {

			ASSERT((*ns)->p_parent == parent);

			*ns = child->p_sibling_ns;
			child->p_sibling_ns = NULL;
			return;
		}
	}
}

/*
 * Add process "child" to the new state list of process "parent"
 */
void
add_ns(proc_t *parent, proc_t *child)
{
	ASSERT(child->p_sibling_ns == NULL);
	child->p_sibling_ns = parent->p_child_ns;
	parent->p_child_ns = child;
}
