/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sigsuspend.c	1.3	94/10/13 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/fault.h>
#include <sys/procset.h>
#include <sys/signal.h>
#include <sys/debug.h>

int
sigsuspend(sigset_t *setp)
{
	sigset_t set;
	k_sigset_t kset;
	proc_t *p = curproc;

	if (copyin((caddr_t)setp, (caddr_t)&set, sizeof (sigset_t)))
		return (set_errno(EFAULT));
	sigutok(&set, &kset);
	sigdiffset(&kset, &cantmask);
	mutex_enter(&p->p_lock);
	ttolwp(curthread)->lwp_sigoldmask = curthread->t_hold;
	curthread->t_hold = kset;
	aston(curthread);		/* so post-syscall will re-evaluate */
	curthread->t_flag |= T_TOMASK;
	/* pause() */
	while (cv_wait_sig_swap(&u.u_cv, &p->p_lock))
		;
	mutex_exit(&p->p_lock);
	return (set_errno(EINTR));
}
