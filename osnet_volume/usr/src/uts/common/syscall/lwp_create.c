/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)lwp_create.c	1.22	99/10/01 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/syscall.h>
#include <sys/proc.h>
#include <sys/processor.h>
#include <sys/fault.h>
#include <sys/ucontext.h>
#include <sys/signal.h>
#include <sys/unistd.h>
#include <sys/procfs.h>
#include <sys/prsystm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/klwp.h>

/*
 * A process can create a special lwp, the "aslwp", to take signals sent
 * asynchronously to this process. The "aslwp" (short for Asynchronous Signals'
 * lwp) is like a daemon lwp within this process and it is the first recipient
 * of any signal sent asynchronously to the containing process. The aslwp is
 * created via a new, reserved flag (__LWP_ASLWP) to _lwp_create(2). Currently
 * only an MT process, i.e. a process linked with -lthread, creates such an lwp.
 * At user-level, "aslwp" is usually in a "sigwait()", waiting for all signals.
 * The aslwp is set up by calling setup_aslwp() from syslwp_create().
 */
static void
setup_aslwp(kthread_t *t)
{
	proc_t *p = ttoproc(t);

	ASSERT(MUTEX_HELD(&p->p_lock));
	ASSERT((p->p_flag & ASLWP) == 0 && p->p_aslwptp == NULL);
	p->p_flag |= ASLWP;
	p->p_aslwptp = t;
	/*
	 * Since "aslwp"'s thread pointer has just been advertised above, it
	 * is impossible for it to have received any signals directed via
	 * sigtoproc(). They are all in p_sig and the sigqueue is all in
	 * p_sigqueue.
	 */
	ASSERT(sigisempty(&t->t_sig));
	ASSERT(t->t_sigqueue == NULL);
	t->t_sig = p->p_sig;
	sigemptyset(&p->p_sig);
	t->t_sigqueue = p->p_sigqueue;
	p->p_sigqueue = NULL;
}

/*
 * Create a lwp.
 */
int
syslwp_create(ucontext_t *ucp, int flags, id_t *new_lwp)
{
	klwp_t *lwp;
	proc_t *p = ttoproc(curthread);
	kthread_t *t;
	ucontext_t uc;
#ifdef _SYSCALL32_IMPL
	ucontext32_t uc32;
#endif /* _SYSCALL32_IMPL */
	k_sigset_t sigmask;
	int	tid;
	model_t model = get_udatamodel();

	/*
	 * lwp_create() is not supported for the /proc agent lwp.
	 */
	if (curthread == p->p_agenttp)
		return (set_errno(ENOTSUP));

	if (model == DATAMODEL_NATIVE) {
		if (copyin(ucp, &uc, sizeof (ucontext_t)))
			return (set_errno(EFAULT));
		sigutok(&uc.uc_sigmask, &sigmask);
	}
#ifdef _SYSCALL32_IMPL
	else {
		if (copyin(ucp, &uc32, sizeof (ucontext32_t)))
			return (set_errno(EFAULT));
		sigutok(&uc32.uc_sigmask, &sigmask);
#if defined(__sparcv9)
		ucontext_32ton(&uc32, &uc, NULL, NULL);
#elif defined(__ia64)
		ucontext_32ton(&uc32, &uc);
#else
#error "neither __sparcv9 nor __ia64 is defined"
#endif
	}
#endif /* _SYSCALL32_IMPL */
	sigdiffset(&sigmask, &cantmask);

	(void) save_syscall_args();	/* save args for tracing first */
	lwp = lwp_create(lwp_rtt, NULL, NULL, curproc, TS_STOPPED,
		curthread->t_pri, sigmask, curthread->t_cid);
	if (lwp == NULL)
		return (set_errno(EAGAIN));

	lwp_load(lwp, uc.uc_mcontext.gregs);

	t = lwptot(lwp);
	/*
	 * copy new LWP's lwpid into the caller's specified buffer.
	 */
	if (new_lwp && copyout(&t->t_tid, new_lwp, sizeof (id_t))) {
		/*
		 * caller's buffer is not writable, return
		 * EFAULT, and terminate new LWP.
		 */
		mutex_enter(&p->p_lock);
		t->t_proc_flag |= TP_EXITLWP;
		t->t_sig_check = 1;
		t->t_sysnum = 0;
		t->t_proc_flag &= ~TP_HOLDLWP;
		lwp_create_done(t);
		mutex_exit(&p->p_lock);
		return (set_errno(EFAULT));
	}

	/*
	 * clone callers context, if any.  must be invoked
	 * while -not- holding p_lock.
	 */
	if (curthread->t_ctx)
		lwp_createctx(curthread, t);

	mutex_enter(&p->p_lock);
	/*
	 * Copy the syscall arguments to the new lwp's arg area
	 * for the benefit of debuggers.
	 */
	t->t_sysnum = SYS_lwp_create;
	lwp->lwp_ap = lwp->lwp_arg;
	lwp->lwp_arg[0] = (long)ucp;
	lwp->lwp_arg[1] = (long)flags;
	lwp->lwp_arg[2] = (long)new_lwp;
	lwp->lwp_argsaved = 1;

	/*
	 * If we are creating the aslwp, do some checks then set it up.
	 */
	if (flags & __LWP_ASLWP) {
		if (p->p_flag & ASLWP) {
			/*
			 * There is already an aslwp.
			 * Return EINVAL and terminate the new LWP.
			 */
			t->t_proc_flag |= TP_EXITLWP;
			t->t_sig_check = 1;
			t->t_sysnum = 0;
			t->t_proc_flag &= ~TP_HOLDLWP;
			lwp_create_done(t);
			mutex_exit(&p->p_lock);
			return (set_errno(EINVAL));
		}
		setup_aslwp(t);
	}

	if (!(flags & LWP_DETACHED))
		t->t_proc_flag |= TP_TWAIT;

	tid = (int)t->t_tid;	/* for /proc debuggers */

	/*
	 * We now set the newly-created lwp running.
	 * If it is being created as LWP_SUSPENDED, we leave its
	 * TP_HOLDLWP flag set so it will stop in system call exit.
	 */
	if (!(flags & LWP_SUSPENDED))
		t->t_proc_flag &= ~TP_HOLDLWP;
	lwp_create_done(t);
	mutex_exit(&p->p_lock);

	return (tid);
}

/*
 * Exit the calling lwp
 */
void
syslwp_exit()
{
	proc_t *p = ttoproc(curthread);

	mutex_enter(&p->p_lock);
	lwp_exit();
	/* NOTREACHED */
}
