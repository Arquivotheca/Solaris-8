/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sigprocmask.c	1.3	94/10/13 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/fault.h>
#include <sys/signal.h>
#include <sys/debug.h>

int
sigprocmask(int how, sigset_t *setp, sigset_t *osetp)
{
	k_sigset_t kset;
	proc_t *p;

	/*
	 * User's oset and set might be the same address, so copyin first and
	 * save before copying out.
	 */
	if (setp) {
		sigset_t set;
		if (copyin((caddr_t)setp, (caddr_t)&set, sizeof (sigset_t)))
			return (set_errno(EFAULT));
		sigutok(&set, &kset);
	}

	if (osetp) {
		sigset_t set;

		sigktou(&curthread->t_hold, &set);
		if (copyout((caddr_t)&set, (caddr_t)osetp, sizeof (sigset_t))) {
			return (set_errno(EFAULT));
		}
	}

	if (setp) {
		p = curproc;
		mutex_enter(&p->p_lock);
		sigdiffset(&kset, &cantmask);
		switch (how) {
		case SIG_BLOCK:
			sigorset(&curthread->t_hold, &kset);
			break;
		case SIG_UNBLOCK:
			sigdiffset(&curthread->t_hold, &kset);
			aston(curthread);
			break;
		case SIG_SETMASK:
			curthread->t_hold = kset;
			aston(curthread);
			break;
		default:
			mutex_exit(&p->p_lock);
			return (set_errno(EINVAL));
		}
		mutex_exit(&p->p_lock);
	}
	return (0);
}
