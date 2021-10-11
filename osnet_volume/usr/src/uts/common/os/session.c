/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)session.c	1.29	98/09/30 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/pcb.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/var.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/session.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/strsubr.h>

sess_t session0 = {
	1,	/* s_ref   */
	0555,	/* s_mode  */
	0,	/* s_uid   */
	0,	/* s_gid   */
	0,	/* s_ctime */
	NODEV,	/* s_dev   */
	NULL,	/* s_vp    */
	&pid0,	/* s_sidp  */
	NULL	/* s_cred  */
};

void
sess_rele(sess_t *sp)
{
	ASSERT(MUTEX_HELD(&pidlock));

	if (--sp->s_ref == 0) {
		if (sp == &session0)
			panic("sp == &session0");
		PID_RELE(sp->s_sidp);
		mutex_destroy(&sp->s_lock);
		cv_destroy(&sp->s_wait_cv);
		kmem_free(sp, sizeof (sess_t));
	}
}

void
sess_create(void)
{
	proc_t *pp;
	sess_t *sp;

	pp = ttoproc(curthread);

	sp = kmem_zalloc(sizeof (sess_t), KM_SLEEP);

	mutex_init(&sp->s_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&sp->s_wait_cv, NULL, CV_DEFAULT, NULL);

	mutex_enter(&pidlock);

	/*
	 * We need to protect p_pgidp with p_lock because
	 * /proc looks at it while holding only p_lock.
	 */
	mutex_enter(&pp->p_lock);
	pgexit(pp);
	SESS_RELE(pp->p_sessp);

	sp->s_sidp = pp->p_pidp;
	sp->s_ref = 1;
	sp->s_dev = NODEV;

	pp->p_sessp = sp;

	pgjoin(pp, pp->p_pidp);
	mutex_exit(&pp->p_lock);

	PID_HOLD(sp->s_sidp);
	mutex_exit(&pidlock);
}

void
freectty(sess_t *sp)
{
	vnode_t *vp;
	cred_t *cred;

	vp = sp->s_vp;

	strfreectty(vp->v_stream);

	mutex_enter(&sp->s_lock);
	while (sp->s_cnt > 0) {
		cv_wait(&sp->s_wait_cv, &sp->s_lock);
	}
	ASSERT(sp->s_cnt == 0);
	ASSERT(vp->v_count >= 1);
	sp->s_vp = NULL;
	cred = sp->s_cred;

	/*
	 * It is possible for the VOP_CLOSE below to call strctty
	 * and reallocate a new tty vnode.  To prevent that the
	 * session is marked as closing here.
	 */

	sp->s_flag = SESS_CLOSE;
	sp->s_cred = NULL;
	mutex_exit(&sp->s_lock);

	/*
	 * This will be the only thread with access to
	 * this vnode, from this point on.
	 */

	VOP_CLOSE(vp, 0, 1, (offset_t)0, cred);
	VN_RELE(vp);

	crfree(cred);
}

/*
 *	++++++++++++++++++++++++
 *	++  SunOS4.1 Buyback  ++
 *	++++++++++++++++++++++++
 *
 * vhangup: Revoke access of the current tty by all processes
 * Used by super-user to give a "clean" terminal at login
 */
int
vhangup()
{
	if (!suser(CRED()))
		return (set_errno(EPERM));
	/*
	 * This routine used to call freectty() under a contition that
	 * could never happen.  So this code has never actually done
	 * anything, and evidently nobody has ever noticed.  4098399.
	 */
	return (0);
}

dev_t
cttydev(proc_t *pp)
{
	sess_t *sp = pp->p_sessp;
	if (sp->s_vp == NULL)
		return (NODEV);
	return (sp->s_dev);
}

void
alloctty(proc_t *pp, vnode_t *vp)
{
	sess_t *sp = pp->p_sessp;
	cred_t *crp;

	sp->s_vp = vp;
	sp->s_dev = vp->v_rdev;

	mutex_enter(&pp->p_crlock);
	crhold(crp = pp->p_cred);
	mutex_exit(&pp->p_crlock);
	sp->s_cred = crp;
	sp->s_uid = crp->cr_uid;
	sp->s_ctime = hrestime.tv_sec;
	if (session0.s_mode & VSGID)
		sp->s_gid = session0.s_gid;
	else
		sp->s_gid = crp->cr_gid;
	sp->s_mode = (0666 & ~(PTOU(pp)->u_cmask));
}

int
hascttyperm(sess_t *sp, cred_t *cr, mode_t mode)
{
	if (cr->cr_uid == 0)
		return (1);

	if (cr->cr_uid != sp->s_uid) {
		mode >>= 3;
		if (!groupmember(sp->s_gid, cr))
			mode >>= 3;
	}

	if ((sp->s_mode & mode) == mode)
		return (1);

	return (0);
}

int
realloctty(proc_t *frompp, pid_t sid)
{
	proc_t *topp;
	sess_t *fromsp;
	sess_t *tosp;
	cred_t *fromcr;
	cred_t *tocr;
	vnode_t *fromvp;

	fromsp = frompp->p_sessp;
	fromvp = fromsp->s_vp;
	mutex_enter(&frompp->p_crlock);
	crhold(fromcr = frompp->p_cred);
	mutex_exit(&frompp->p_crlock);

	if (!hascttyperm(&session0, fromcr, VEXEC|VWRITE)) {
		crfree(fromcr);
		return (EACCES);
	}

	if ((session0.s_mode & VSVTX) &&
	    fromcr->cr_uid != session0.s_uid &&
	    (!hascttyperm(fromsp, fromcr, VWRITE))) {
		crfree(fromcr);
		return (EACCES);
	}

	if (sid == 0) {
		crfree(fromcr);
		freectty(fromsp);
		return (0);
	}

	if (fromvp->v_stream == NULL) {
		crfree(fromcr);
		return (ENOSYS);
	}

	if ((topp = prfind(sid)) == NULL) {
		crfree(fromcr);
		return (ESRCH);
	}

	tosp = topp->p_sessp;

	mutex_enter(&topp->p_crlock);
	crhold(tocr = topp->p_cred);
	mutex_exit(&topp->p_crlock);
	if (tosp->s_sidp != topp->p_pidp || tosp->s_vp != NULL ||
	    !hasprocperm(tocr, fromcr)) {
		crfree(fromcr);
		crfree(tocr);
		return (EPERM);
	}

	crfree(fromcr);
	crfree(tocr);
	strfreectty(fromvp->v_stream);
	crfree(fromsp->s_cred);

	alloctty(topp, fromvp);
	stralloctty(tosp, fromvp->v_stream);

	fromsp->s_vp = NULL;

	return (0);
}
