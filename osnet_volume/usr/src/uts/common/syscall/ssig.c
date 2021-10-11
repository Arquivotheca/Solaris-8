/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ssig.c	1.137	97/11/22 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/fault.h>
#include <sys/procset.h>
#include <sys/signal.h>
#include <sys/debug.h>


#ifdef i386
int is_coff_proc(proc_t *p);
#endif

/*
 * ssig() is the common entry for signal, sigset, sighold, sigrelse,
 * sigignore and sigpause.
 *
 * for implementations that don't require binary compatibility,
 * signal, sigset, sighold, sigrelse, sigignore and sigpause may
 * be made into library routines that call sigaction, sigsuspend and sigprocmask
 */

int
ssig(int signo, void (*func)())
{
	int sig;
	struct proc *p;
	int flags;
	int retval = 0;
	int sigcld_look = 0;

	sig = signo & SIGNO_MASK;

	if (sig <= 0 || sig >= NSIG || sigismember(&cantmask, sig))
		return (set_errno(EINVAL));

	p = ttoproc(curthread);
	mutex_enter(&p->p_lock);
	switch (signo & ~SIGNO_MASK) {

	case SIGHOLD:	/* sighold */
		sigaddset(&curthread->t_hold, sig);
		mutex_exit(&p->p_lock);
		return (0);

	case SIGRELSE:	/* sigrelse */
		sigdelset(&curthread->t_hold, sig);
		aston(curthread);		/* so ISSIG will see release */
		mutex_exit(&p->p_lock);
		return (0);

	case SIGPAUSE:	/* sigpause */
		sigdelset(&curthread->t_hold, sig);
		aston(curthread);		/* so ISSIG will see release */
		/* pause() */
		while (cv_wait_sig_swap(&u.u_cv, &p->p_lock))
			;
		mutex_exit(&p->p_lock);
		return (set_errno(EINTR));

	case SIGIGNORE:	/* signore */
		sigdelset(&curthread->t_hold, sig);
		aston(curthread);		/* so ISSIG will see release */
		func = SIG_IGN;
		flags = 0;
		break;

	case SIGDEFER:		/* sigset */
#ifdef i386
		/*
		 * If the system call is from a COFF program then register
		 * u_sigreturn (passed in %edx) for signal handling
		 * cleanup. (SVR3.2 binary compatibility)
		 */
		if (is_coff_proc(p)) {
			klwp_t *lwp = proctolwp(p);

			u.u_sigreturn =  (void (*)()) lwptoregs(lwp)->r_edx;
		}
#endif /* i386 */
		if (sigismember(&curthread->t_hold, sig))
			retval = (int)SIG_HOLD;
		else
			retval = (int)u.u_signal[sig-1];
		if (func == SIG_HOLD) {
			sigaddset(&curthread->t_hold, sig);
			mutex_exit(&p->p_lock);
			return (retval);
		}

#if defined(sparc)
		/*
		 * Check alignment of handler
		 */
		if (func != SIG_IGN && func != SIG_DFL &&
		    ((int)func & 0x3) != 0) {
			mutex_exit(&p->p_lock);
			return (set_errno(EINVAL));
		}
#endif	/* sparc */

		sigdelset(&curthread->t_hold, sig);
		aston(curthread); 	/* so post_syscall sees new t_hold */
		flags = 0;
		break;

	case 0:	/* signal */
#ifdef i386
		/*
		 * If the system call is from a COFF program then register
		 * u_sigreturn (passed in %edx) for signal handling
		 * cleanup. (SVR3.2 binary compatibility)
		 */
		if (is_coff_proc(p)) {
			klwp_t *lwp = proctolwp(p);

			u.u_sigreturn =  (void (*)()) lwptoregs(lwp)->r_edx;
		}
#endif /* i386 */

#if defined(sparc)
		/*
		 * Check alignment of handler
		 */
		if (func != SIG_IGN && func != SIG_DFL &&
		    ((int)func & 0x3) != 0) {
			mutex_exit(&p->p_lock);
			return (set_errno(EINVAL));
		}
#endif	/* sparc */

		retval = (int)u.u_signal[sig-1];
		flags = SA_RESETHAND|SA_NODEFER;
		break;

	default:		/* error */
		mutex_exit(&p->p_lock);
		return (set_errno(EINVAL));
	}

	if (sigismember(&stopdefault, sig))
		flags |= SA_RESTART;
	else if (sig == SIGCLD) {
		flags |= SA_NOCLDSTOP;
		if (func == SIG_IGN)
			flags |= SA_NOCLDWAIT;
		else if (func != SIG_DFL)
			sigcld_look = 1;
	}

	setsigact(sig, func, nullsmask, flags);
	mutex_exit(&p->p_lock);

	if (sigcld_look)
		sigcld_repost();

	return (retval);
}
