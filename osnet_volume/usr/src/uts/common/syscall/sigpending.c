/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sigpending.c	1.6	98/02/10 SMI"	/* from SVr4.0 1.78 */

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
sigpending(int flag, sigset_t *setp)
{
	sigset_t set;
	k_sigset_t kset;
	proc_t *p;

	switch (flag) {
	case 1: /* sigpending */
		p = ttoproc(curthread);
		mutex_enter(&p->p_lock);
		if (p->p_aslwptp != NULL) {
			kset = p->p_notifsigs;
			sigorset(&kset, &p->p_aslwptp->t_sig);
		} else {
			kset = p->p_sig;
			sigorset(&kset, &curthread->t_sig);
		}
		sigandset(&kset, &curthread->t_hold);
		mutex_exit(&p->p_lock);
		break;
	case 2: /* sigfillset */
		kset = fillset;
		break;
	case 3:
		/*
		 * Add case for MT processes. Since signal masks are maintained
		 * at user-level, ignore curthread->t_hold in the kernel, pass
		 * up all queued signals and have a user-level wrapper do
		 * the masking with the calling (user) thread's signal mask.
		 */
		p = ttoproc(curthread);
		mutex_enter(&p->p_lock);
		if (p->p_aslwptp != NULL) {
			kset = p->p_notifsigs;
			sigorset(&kset, &p->p_aslwptp->t_sig);
			/*
			 * There are some instances where a thread's mask is
			 * pushed down to the kernel - for bound threads which
			 * take LWP-directed signals. In such cases, the t_sig
			 * signal set could contain pending signals not visible
			 * to the user-level. So add these to the set of
			 * signals.
			 */
			sigorset(&kset, &curthread->t_sig);
		} else {
			kset = p->p_sig;
			sigorset(&kset, &curthread->t_sig);
		}
		mutex_exit(&p->p_lock);
		break;
	default:
		return (set_errno(EINVAL));
	}

	sigktou(&kset, &set);
	if (copyout((caddr_t)&set, (caddr_t)setp, sizeof (sigset_t)))
		return (set_errno(EFAULT));
	return (0);
}
