/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pgrpsys.c	1.6	99/07/07 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/session.h>
#include <sys/debug.h>

/* ARGSUSED */
int
setpgrp(int flag, int pid, int pgid)
{
	register proc_t *p =  ttoproc(curthread);
	register int	retval = 0;

	switch (flag) {

	case 1: /* setpgrp() */
		mutex_enter(&pidlock);
		if (p->p_sessp->s_sidp != p->p_pidp && !pgmembers(p->p_pid)) {
			mutex_exit(&pidlock);
			sess_create();
		} else
			mutex_exit(&pidlock);
		return (p->p_sessp->s_sid);

	case 3: /* setsid() */
		mutex_enter(&pidlock);
		if (p->p_pgidp == p->p_pidp || pgmembers(p->p_pid)) {
			mutex_exit(&pidlock);
			return (set_errno(EPERM));
		}
		mutex_exit(&pidlock);
		sess_create();
		return (p->p_sessp->s_sid);

	case 5: /* setpgid() */
	{
		mutex_enter(&pidlock);
		if (pid == 0)
			pid = p->p_pid;
		else if (pid < 0 || pid >= maxpid) {
			mutex_exit(&pidlock);
			return (set_errno(EINVAL));
		} else if (pid != p->p_pid) {
			for (p = p->p_child; /* empty */; p = p->p_sibling) {
				if (p == NULL) {
					mutex_exit(&pidlock);
					return (set_errno(ESRCH));
				}
				if (p->p_pid == pid)
					break;
			}
			if (p->p_flag & SEXECED) {
				mutex_exit(&pidlock);
				return (set_errno(EACCES));
			}
			if (p->p_sessp != ttoproc(curthread)->p_sessp) {
				mutex_exit(&pidlock);
				return (set_errno(EPERM));
			}
		}

		if (p->p_sessp->s_sid == pid) {
			mutex_exit(&pidlock);
			return (set_errno(EPERM));
		}

		if (pgid == 0)
			pgid = p->p_pid;
		else if (pgid < 0 || pgid >= maxpid) {
			mutex_exit(&pidlock);
			return (set_errno(EINVAL));
		}

		if (p->p_pgrp == pgid) {
			mutex_exit(&pidlock);
			break;
		} else if (p->p_pid == pgid) {
			/*
			 * We need to protect p_pgidp with p_lock because
			 * /proc looks at it while holding only p_lock.
			 */
			mutex_enter(&p->p_lock);
			pgexit(p);
			pgjoin(p, p->p_pidp);
			mutex_exit(&p->p_lock);
		} else {
			register proc_t *q;

			if ((q = pgfind(pgid)) == NULL ||
			    q->p_sessp != p->p_sessp) {
				mutex_exit(&pidlock);
				return (set_errno(EPERM));
			}
			/*
			 * See comment above about p_lock and /proc
			 */
			mutex_enter(&p->p_lock);
			pgexit(p);
			pgjoin(p, q->p_pgidp);
			mutex_exit(&p->p_lock);
		}
		mutex_exit(&pidlock);
		break;
	}

	case 0: /* getpgrp() */
		mutex_enter(&pidlock);
		retval = p->p_pgrp;
		mutex_exit(&pidlock);
		break;

	case 2: /* getsid() */
	case 4: /* getpgid() */
		if (pid < 0 || pid >= maxpid) {
			return (set_errno(EINVAL));
		}
		mutex_enter(&pidlock);
		if (pid != 0 && p->p_pid != pid &&
		    ((p = prfind(pid)) == NULL || p->p_stat == SIDL)) {
			mutex_exit(&pidlock);
			return (set_errno(ESRCH));
		}
		if (flag == 2)
			retval = p->p_sessp->s_sid;
		else
			retval = p->p_pgrp;
		mutex_exit(&pidlock);
		break;

	}
	return (retval);
}
