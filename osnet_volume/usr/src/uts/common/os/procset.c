/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)procset.c	1.18	98/06/30 SMI"	/* from SVr4.0 1.25 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/var.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/siginfo.h>
#include <sys/acct.h>
#include <sys/procset.h>
#include <sys/cmn_err.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include <sys/procfs.h>
#include <sys/session.h>


id_t	getmyid(idtype_t);
int	checkprocset(procset_t *);
static	kthread_id_t getlwpptr(id_t);
int	dotolwp(procset_t *, int (*funcp)(), char *);
int	procinset(proc_t *, procset_t *);
static	int lwpinset(proc_t *, procset_t *, kthread_id_t, int *);

/*
 * The dotoprocs function locates the process(es) specified
 * by the procset structure pointed to by psp.  If funcp
 * is non-NULL then it points to a function which dotoprocs
 * will call for each process in the specified set.  The
 * arguments to this function will be a pointer to the
 * current process from the set and arg.
 * If the called function returns -1, it means that processing of the
 * procset should stop and a normal (non-error) return should be made
 * to the caller of dotoprocs.
 * If the called function returns any other non-zero value the search
 * is terminated and the function's return value is returned to
 * the caller of dotoprocs.  This will normally be an error code.
 * Otherwise, dotoprocs will return zero after processing the entire
 * process set unless no processes were found in which case ESRCH will
 * be returned.
 */
int
dotoprocs(procset_t *psp, int (*funcp)(), char *arg)
{
	register proc_t	*prp;	/* A process from the set */
	register int	error;
	register int	nfound;	/* Nbr of processes found.	*/
	register proc_t	*lastprp;	/* Last proc found.	*/

	/*
	 * Check that the procset_t is valid.
	 */
	error = checkprocset(psp);
	if (error) {
		return (error);
	}
	/*
	 * Check for the special value P_MYID in either operand
	 * and replace it with the correct value.  We don't check
	 * for an error return from getmyid() because the idtypes
	 * have been validated by the checkprocset() call above.
	 */
	if (psp->p_lid == P_MYID) {
		psp->p_lid = getmyid(psp->p_lidtype);
	}
	if (psp->p_rid == P_MYID) {
		psp->p_rid = getmyid(psp->p_ridtype);
	}

	nfound = 0;
	error  = 0;

	mutex_enter(&pidlock);
	for (prp = practive; prp != NULL; prp = prp->p_next) {
		if (prp->p_stat == SIDL || prp->p_stat == SZOMB)
			continue;
		if (procinset(prp, psp)) {
			nfound++;
			lastprp = prp;
			if (funcp != NULL && prp != proc_init) {
				error = (*funcp)(prp, arg);
				if (error == -1) {
					mutex_exit(&pidlock);
					return (0);
				} else if (error) {
					mutex_exit(&pidlock);
					return (error);
				}
			}
		}
	}
	if (nfound == 0) {
		mutex_exit(&pidlock);
		return (ESRCH);
	}
	if (nfound == 1 && lastprp == proc_init && funcp != NULL)
		error = (*funcp)(lastprp, arg);
	if (error == -1)
		error = 0;
	mutex_exit(&pidlock);
	return (error);
}

/*
 * Check if a procset_t is valid.  Return zero or an errno.
 */
int
checkprocset(procset_t *psp)
{
	switch (psp->p_lidtype) {
	case P_LWPID:
	case P_PID:
	case P_PPID:
	case P_PGID:
	case P_SID:
	case P_CID:
	case P_UID:
	case P_GID:
	case P_ALL:
		break;
	default:
		return (EINVAL);
	}

	switch (psp->p_ridtype) {
	case P_LWPID:
	case P_PID:
	case P_PPID:
	case P_PGID:
	case P_SID:
	case P_CID:
	case P_UID:
	case P_GID:
	case P_ALL:
		break;
	default:
		return (EINVAL);
	}

	switch (psp->p_op) {
	case POP_DIFF:
	case POP_AND:
	case POP_OR:
	case POP_XOR:
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

/*
 * procinset returns 1 if the process pointed to
 * by pp is in the process set specified by psp and is not in
 * the sys scheduling class - otherwise 0 is returned.
 *
 * This function expects to be called with a valid procset_t.
 * The set should be checked using checkprocset() before calling
 * this function.
 */
int
procinset(proc_t *pp, procset_t *psp)
{
	register int	loperand = 0;
	register int	roperand = 0;
	register int	lwplinproc  = 0;
	register int	lwprinproc  = 0;
	register kthread_id_t	tp = proctot(pp);

	/*
	 * If process is in the sys class or is exiting, return (0).
	 */
	if (tp == NULL || tp->t_cid == 0) {
		return (0);
	}

	switch (psp->p_lidtype) {

	case P_LWPID:
		if (pp == ttoproc(curthread))
			if (getlwpptr(psp->p_lid) != NULL)
				lwplinproc++;
		break;

	case P_PID:
		if (pp->p_pid == psp->p_lid)
			loperand++;
		break;

	case P_PPID:
		if (pp->p_ppid == psp->p_lid)
			loperand++;
		break;

	case P_PGID:
		if (pp->p_pgrp == psp->p_lid)
			loperand++;
		break;

	case P_SID:
		if (pp->p_sessp->s_sid == psp->p_lid)
			loperand++;
		break;

	case P_CID:
		/* This case is broken for now. Need to be fixed XXX */
		if (tp->t_cid == psp->p_lid)
/*
		if (checkcid(psp->p_lid))
*/
			loperand++;
		break;

	case P_UID:
		mutex_enter(&pp->p_crlock);
		if (pp->p_cred->cr_uid == psp->p_lid)
			loperand++;
		mutex_exit(&pp->p_crlock);
		break;

	case P_GID:
		mutex_enter(&pp->p_crlock);
		if (pp->p_cred->cr_gid == psp->p_lid)
			loperand++;
		mutex_exit(&pp->p_crlock);
		break;

	case P_ALL:
		loperand++;
		break;

	default:
#ifdef DEBUG
		cmn_err(CE_WARN, "procinset called with bad set");
		return (0);
#else
		return (0);
#endif
	}

	switch (psp->p_ridtype) {

	case P_LWPID:
		if (pp == ttoproc(curthread))
			if (getlwpptr(psp->p_rid) != NULL)
				lwprinproc++;
		break;

	case P_PID:
		if (pp->p_pid == psp->p_rid)
			roperand++;
		break;

	case P_PPID:
		if (pp->p_ppid == psp->p_rid)
			roperand++;
		break;

	case P_PGID:
		if (pp->p_pgrp == psp->p_rid)
			roperand++;
		break;

	case P_SID:
		if (pp->p_sessp->s_sid == psp->p_rid)
			roperand++;
		break;

	case P_CID:
		/* This case is broken for now. Need to be fixed XXX */
		if (tp->t_cid == psp->p_rid)
/*
		if (checkcid(psp->p_rid))
*/
			roperand++;
		break;

	case P_UID:
		mutex_enter(&pp->p_crlock);
		if (pp->p_cred->cr_uid == psp->p_rid)
			roperand++;
		mutex_exit(&pp->p_crlock);
		break;

	case P_GID:
		mutex_enter(&pp->p_crlock);
		if (pp->p_cred->cr_gid == psp->p_rid)
			roperand++;
		mutex_exit(&pp->p_crlock);
		break;

	case P_ALL:
		roperand++;
		break;

	default:
#ifdef DEBUG
		cmn_err(CE_WARN, "procinset called with bad set");
		return (0);
#else
		return (0);
#endif
	}

	switch (psp->p_op) {

	case POP_DIFF:
		if (loperand && !lwprinproc && !roperand)
			return (1);
		else
			return (0);

	case POP_AND:
		if (loperand && roperand)
			return (1);
		else
			return (0);

	case POP_OR:
		if (loperand || roperand)
			return (1);
		else
			return (0);

	case POP_XOR:
		if ((loperand && !lwprinproc && !roperand) ||
			(roperand && !lwplinproc && !loperand))
			return (1);
		else
			return (0);

	default:
#ifdef DEBUG
		cmn_err(CE_WARN, "procinset called with bad set");
		return (0);
#else
		return (0);
#endif
	}
	/* NOTREACHED */
}

/*
 * lwpinset returns 1 if the thread pointed to
 * by tp is in the process set specified by psp and is not in
 * the sys scheduling class - otherwise 0 is returned.
 *
 * This function expects to be called with a valid procset_t.
 * The set should be checked using checkprocset() before calling
 * this function.
 */
int
lwpinset(proc_t *pp, procset_t *psp, kthread_id_t tp, int *done)
{
	register int	loperand = 0;
	register int	roperand = 0;
	register int	lwplinset  = 0;
	register int	lwprinset  = 0;

	ASSERT(ttoproc(tp) == pp);

	/*
	 * If process is in the sys class return (0).
	 */
	if (proctot(pp)->t_cid == 0) {
		return (0);
	}

	switch (psp->p_lidtype) {

	case P_LWPID:
		if (tp->t_tid == psp->p_lid)
			lwplinset ++;
		break;

	case P_PID:
		if (pp->p_pid == psp->p_lid)
			loperand++;
		break;

	case P_PPID:
		if (pp->p_ppid == psp->p_lid)
			loperand++;
		break;

	case P_PGID:
		if (pp->p_pgrp == psp->p_lid)
			loperand++;
		break;

	case P_SID:
		if (pp->p_sessp->s_sid == psp->p_lid)
			loperand++;
		break;

	case P_CID:
		if (tp->t_cid == psp->p_lid)
			loperand++;
		break;

	case P_UID:
		mutex_enter(&pp->p_crlock);
		if (pp->p_cred->cr_uid == psp->p_lid)
			loperand++;
		mutex_exit(&pp->p_crlock);
		break;

	case P_GID:
		mutex_enter(&pp->p_crlock);
		if (pp->p_cred->cr_gid == psp->p_lid)
			loperand++;
		mutex_exit(&pp->p_crlock);
		break;

	case P_ALL:
		loperand++;
		break;

	default:
#ifdef DEBUG
		cmn_err(CE_WARN, "lwpinset called with bad set");
		return (0);
#else
		return (0);
#endif
	}

	switch (psp->p_ridtype) {

	case P_LWPID:
		if (tp->t_tid == psp->p_rid)
			lwprinset ++;
		break;

	case P_PID:
		if (pp->p_pid == psp->p_rid)
			roperand++;
		break;

	case P_PPID:
		if (pp->p_ppid == psp->p_rid)
			roperand++;
		break;

	case P_PGID:
		if (pp->p_pgrp == psp->p_rid)
			roperand++;
		break;

	case P_SID:
		if (pp->p_sessp->s_sid == psp->p_rid)
			roperand++;
		break;

	case P_CID:
		if (tp->t_cid == psp->p_rid)
			roperand++;
		break;

	case P_UID:
		mutex_enter(&pp->p_crlock);
		if (pp->p_cred->cr_uid == psp->p_rid)
			roperand++;
		mutex_exit(&pp->p_crlock);
		break;

	case P_GID:
		mutex_enter(&pp->p_crlock);
		if (pp->p_cred->cr_gid == psp->p_rid)
			roperand++;
		mutex_exit(&pp->p_crlock);
		break;

	case P_ALL:
		roperand++;
		break;

	default:
#ifdef DEBUG
		cmn_err(CE_WARN, "lwpinset called with bad set");
		return (0);
#else
		return (0);
#endif
	}

	if (lwplinset && lwprinset)
		*done = 1;

	switch (psp->p_op) {

	case POP_DIFF:
		if ((loperand || lwplinset) && !(lwprinset || roperand))
			return (1);
		else
			return (0);

	case POP_AND:
		if ((loperand || lwplinset) && (roperand || lwprinset))
			return (1);
		else
			return (0);

	case POP_OR:
		if (loperand || roperand || lwplinset || lwprinset)
			return (1);
		else
			return (0);

	case POP_XOR:
		if (((loperand || lwplinset) &&
					!(lwprinset || roperand)) ||
		    ((roperand || lwprinset) &&
					!(lwplinset || loperand)))
			return (1);
		else
			return (0);

	default:
#ifdef DEBUG
		cmn_err(CE_WARN, "lwpinset called with bad set");
		return (0);
#else
		return (0);
#endif
	}
	/* NOTREACHED */
}
/*
 * Check for common cases of procsets which specify only the
 * current process.  cur_inset_only() returns B_TRUE when
 * the current process is the only one in the set.  B_FALSE
 * is returned to indicate that this may not be the case.
 */
boolean_t
cur_inset_only(procset_t *psp)
{
	if (((psp->p_lidtype == P_PID &&
		(psp->p_lid == P_MYID ||
		psp->p_lid == ttoproc(curthread)->p_pid)) ||
		((psp->p_lidtype == P_LWPID) &&
		(psp->p_lid == P_MYID ||
		psp->p_lid == curthread->t_tid))) &&
		psp->p_op == POP_AND && psp->p_ridtype == P_ALL)
			return (B_TRUE);

	if (((psp->p_ridtype == P_PID &&
		(psp->p_rid == P_MYID ||
		psp->p_rid == ttoproc(curthread)->p_pid)) ||
		((psp->p_ridtype == P_LWPID) &&
		(psp->p_rid == P_MYID ||
		psp->p_rid == curthread->t_tid))) &&
		psp->p_op == POP_AND && psp->p_lidtype == P_ALL)
			return (B_TRUE);

	return (B_FALSE);
}

id_t
getmyid(idtype_t idtype)
{
	register proc_t	*pp;
	uid_t uid;
	gid_t gid;

	pp = ttoproc(curthread);

	switch (idtype) {
	case P_LWPID:
		return (curthread->t_tid);

	case P_PID:
		return (pp->p_pid);

	case P_PPID:
		return (pp->p_ppid);

	case P_PGID:
		return (pp->p_pgrp);

	case P_SID:
		return (pp->p_sessp->s_sid);

	case P_CID:
		return (curthread->t_cid);

	case P_UID:
		mutex_enter(&pp->p_crlock);
		uid = pp->p_cred->cr_uid;
		mutex_exit(&pp->p_crlock);
		return (uid);

	case P_GID:
		mutex_enter(&pp->p_crlock);
		gid = pp->p_cred->cr_gid;
		mutex_exit(&pp->p_crlock);
		return (gid);

	case P_ALL:
		/*
		 * The value doesn't matter for P_ALL.
		 */
		return (0);

	default:
		return (-1);
	}
}

static kthread_id_t
getlwpptr(id_t id)
{
	register proc_t		*pt;
	register kthread_id_t    tp;

	if (id == P_MYID)
		return (curthread);

	pt = ttoproc(curthread);
	mutex_enter(&pt->p_lock);
	tp = pt->p_tlist;
	do {

		if (tp->t_tid == id) {
			mutex_exit(&pt->p_lock);
			return (tp);
		}

	} while ((tp = tp->t_forw) != pt->p_tlist);
	mutex_exit(&pt->p_lock);

	return (NULL);
}

int
dotolwp(procset_t *psp, int (*funcp)(), char *arg)
{
	register int    error = 0;
	register int    nfound = 0;
	register kthread_id_t    tp;
	register proc_t		*pp;
	int		done = 0;

	/*
	 * Check that the procset_t is valid.
	 */
	error = checkprocset(psp);
	if (error) {
		return (error);
	}
	/*
	 * Check for the special value P_MYID in either operand
	 * and replace it with the correct value.  We don't check
	 * for an error return from getmyid() because the idtypes
	 * have been validated by the checkprocset() call above.
	 */
	if (psp->p_lid == P_MYID) {
		psp->p_lid = getmyid(psp->p_lidtype);
	}
	if (psp->p_rid == P_MYID) {
		psp->p_rid = getmyid(psp->p_ridtype);
	}


	pp = ttoproc(curthread);

	/*
	 * Note that there are two separate cases where this routine
	 * returns zero. In the first case no mutex is grabbed, in
	 * the second the p_lock mutex is NOT RELEASED. the priocntl
	 * code is expecting this behaviour.
	 */

	if (procinset(pp, psp)) {
		return (0);
	}
	mutex_enter(&pp->p_lock);
	if ((tp = pp->p_tlist) == NULL) {
		mutex_exit(&pp->p_lock);
		return (0);
	}
	do {
		if (lwpinset(pp, psp, tp, &done)) {
			nfound ++;
			error = (*funcp)(arg, tp);
			if (error) {
				mutex_exit(&pp->p_lock);
				return (error);
			}
		}
	} while (((tp = tp->t_forw) != pp->p_tlist) && !done);

	if (nfound == 0) {
		mutex_exit(&pp->p_lock);
		return (ESRCH);
	}
	return (error);
}
