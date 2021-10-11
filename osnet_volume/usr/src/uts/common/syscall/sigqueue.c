/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sigqueue.c	1.8	98/05/04 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/procset.h>
#include <sys/fault.h>
#include <sys/signal.h>
#include <sys/siginfo.h>
#include <sys/debug.h>

static int
sigqkill(pid_t pid, int signo, sigsend_t *sigsend)
{
	register proc_t *p;
	int error;

	if (signo < 0 || signo >= NSIG)
		return (set_errno(EINVAL));

	if (pid == -1) {
		procset_t set;

		setprocset(&set, POP_AND, P_ALL, P_MYID, P_ALL, P_MYID);
		error = sigsendset(&set, sigsend);
	} else if (pid > 0) {
		mutex_enter(&pidlock);
		if ((p = prfind(pid)) == NULL || p->p_stat == SIDL)
			error = ESRCH;
		else {
			error = sigsendproc(p, sigsend);
			if (error == 0 && sigsend->perm == 0)
				error = EPERM;
		}
		mutex_exit(&pidlock);
	} else {
		int nfound = 0;
		pid_t pgid;

		if (pid == 0)
			pgid = ttoproc(curthread)->p_pgrp;
		else
			pgid = -pid;

		error = 0;
		mutex_enter(&pidlock);
		for (p = pgfind(pgid); p && !error; p = p->p_pglink) {
			if (p->p_stat != SIDL) {
				nfound++;
				error = sigsendproc(p, sigsend);
			}
		}
		mutex_exit(&pidlock);
		if (nfound == 0)
			error = ESRCH;
		else if (error == 0 && sigsend->perm == 0)
			error = EPERM;
	}

	if (error)
		return (set_errno(error));
	return (0);
}


/*
 * for implementations that don't require binary compatibility,
 * the kill system call may be made into a library call to the
 * sigsend system call
 */
int
kill(pid_t pid, int sig)
{
	sigsend_t v;

	bzero(&v, sizeof (v));
	v.sig = sig;
	v.checkperm = 1;
	v.sicode = SI_USER;

	return (sigqkill(pid, sig, &v));
}

/*
 * The handling of small unions, like the sigval argument to sigqueue,
 * is architecture dependent.  We have adapted the convention that the
 * value itself is passed in the storage which crosses the kernel
 * protection boundary.  This procedure will accept a scalar argument,
 * and store it in the appropriate value member of the sigsend_t structure.
 */
int
sigqueue(pid_t pid, int signo, /* union sigval */ void *value, int si_code)
{
	sigsend_t v;
	sigqhdr_t *sqh;
	proc_t *p = curproc;

	if (pid < 0)
		return (set_errno(EINVAL));

	if (p->p_sigqhdr == NULL) {
		/* Allocate sigqueue pool first time */
		if (sigqhdralloc(&sqh,
				sizeof (sigqueue_t), _SIGQUEUE_MAX) < 0) {
			return (set_errno(EAGAIN));
		} else {
			mutex_enter(&p->p_lock);
			if (p->p_sigqhdr == NULL) {
				/* hang the pool head on proc */
				p->p_sigqhdr = sqh;
			} else {
				/* other lwp allocated pool, so free it */
				sigqhdrfree(&sqh);
			}
			mutex_exit(&p->p_lock);
		}
	}

	bzero(&v, sizeof (v));
	v.sig = signo;
	v.checkperm = 1;
	v.sicode = si_code;
	v.value.sival_ptr = value;

	return (sigqkill(pid, signo, &v));
}

#ifdef _SYSCALL32_IMPL
/*
 * sigqueue32 - System call entry point for 32-bit callers on LP64 kernel,
 * needed to handle the 32-bit sigvals as correctly as we can.  We always
 * assume that a 32-bit caller is passing an int. A 64-bit recipient
 * that expects an int will therefore get it correctly.  A 32-bit
 * recipient will also get it correctly since siginfo_kto32() uses
 * sival_int in the conversion.  Since a 32-bit pointer has the same
 * size and address in the sigval, it also converts correctly so that
 * two 32-bit apps can exchange a pointer value.  However, this means
 * that a pointer sent by a 32-bit caller will be seen in the upper half
 * by a 64-bit recipient, and only the upper half of a 64-bit pointer will
 * be seen by a 32-bit recipient.  This is the best solution that does
 * not require severe hacking of the sigval union.  Anyways, what it
 * means to be sending pointers between processes with dissimilar
 * models is unclear.
 */
int
sigqueue32(pid_t pid, int signo, /* union sigval32 */ caddr32_t value,
    int si_code)
{
	union sigval sv;

	bzero(&sv, sizeof (sv));
	sv.sival_int = (int)value;
	return (sigqueue(pid, signo, sv.sival_ptr, si_code));
}
#endif
