/*	Copyright (c) 1994, by Syn Microsystems, Inc */
/*	  All Rights Reserved	*/

#ident	"@(#)signotify.c	1.9	99/07/06 SMI"

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
#include <vm/as.h>
#include <sys/debug.h>

/*ARGSUSED*/
static int
copyin_siginfo(model_t datamodel, void *uaddr, k_siginfo_t *ksip)
{
#ifdef _SYSCALL32_IMPL
	int ret;

	if (datamodel == DATAMODEL_NATIVE) {
#endif
		return (copyin(uaddr, ksip, sizeof (k_siginfo_t)));
#ifdef _SYSCALL32_IMPL
	} else {
		siginfo32_t si32;

		if (ret = copyin(uaddr, &si32, sizeof (si32)))
			return (ret);

		siginfo_32tok(&si32, ksip);
	}

	return (0);
#endif
}

/*
 * To find secured 64 bit id for signotify() call
 * This depends upon as_getmemid() which returns
 * unique vnode/offset for a user virtual address.
 */
static u_longlong_t
get_sigid(proc_t *p, caddr_t addr)
{
	u_longlong_t snid = 0;
	memid_t memid;
	quad_t *tquad = (quad_t *)&snid;

	if (!as_getmemid(p->p_as, addr, &memid)) {
		tquad->val[0] = (int)memid.val[0];
		tquad->val[1] = (int)memid.val[1];
	}
	return (snid);
}

#define	SIGN_PTR(p, n)	&((signotifyq_t *)(&p->p_signhdr[1]))[n];

int
signotify(int cmd, siginfo_t *siginfo, signotify_id_t *sn_id)
{
	k_siginfo_t	info;
	signotify_id_t	id;
	proc_t		*p;
	proc_t		*cp = curproc;
	signotifyq_t	*snqp;
	struct cred	*cr;
	sigqueue_t	*sqp;
	sigqhdr_t	*sqh;
	u_longlong_t	sid;
	model_t 	datamodel = get_udatamodel();

	if (copyin(sn_id, &id, sizeof (signotify_id_t)))
		return (set_errno(EFAULT));

	if (id.sn_index >= _SIGNOTIFY_MAX || id.sn_index < 0)
		return (set_errno(EINVAL));

	switch (cmd) {
	case SN_PROC:
		/* get snid for the given user address of signotifyid_t */
		sid = get_sigid(cp, (caddr_t)sn_id);

		if (id.sn_pid > 0) {
			mutex_enter(&pidlock);
			if ((p = prfind(id.sn_pid)) != NULL) {
				mutex_enter(&p->p_lock);
				if (p->p_signhdr != NULL) {
					snqp = SIGN_PTR(p, id.sn_index);
					if (snqp->sn_snid == sid) {
						mutex_exit(&p->p_lock);
						mutex_exit(&pidlock);
						return (set_errno(EBUSY));
					}
				}
				mutex_exit(&p->p_lock);
			}
			mutex_exit(&pidlock);
		}

		if (copyin_siginfo(datamodel, siginfo, &info))
			return (set_errno(EFAULT));

		if (info.si_signo < 0 || info.si_signo >= NSIG)
			return (set_errno(EINVAL));

		if (cp->p_signhdr == NULL) {
			if (sigqhdralloc(&sqh, sizeof (signotifyq_t),
			    _SIGNOTIFY_MAX) < 0) {
				return (set_errno(EAGAIN));
			} else {

				mutex_enter(&cp->p_lock);
				if (cp->p_signhdr == NULL) {
					cp->p_signhdr = sqh;
				} else {
					sigqhdrfree(&sqh);
				}
			}
		} else {
			mutex_enter(&cp->p_lock);
		}

		sqp = sigqalloc(&cp->p_signhdr);

		if (sqp == NULL) {
			mutex_exit(&cp->p_lock);
			return (set_errno(EAGAIN));
		}

		cr = CRED();
		sqp->sq_info = info;
		sqp->sq_info.si_pid = cp->p_pid;
		sqp->sq_info.si_uid = cr->cr_ruid;
		sqp->sq_func = sigqrel;
		sqp->sq_next = NULL;

		/* fill the signotifyq_t fields */
		((signotifyq_t *)sqp)->sn_snid = sid;

		mutex_exit(&cp->p_lock);

		/* complete the signotify_id_t fields */
		id.sn_index = (signotifyq_t *)sqp - SIGN_PTR(cp, 0);
		id.sn_pid = cp->p_pid;

		break;

	case SN_CANCEL:
	case SN_SEND:

		mutex_enter(&pidlock);
		if ((id.sn_pid <= 0) || ((p = prfind(id.sn_pid)) == NULL)) {
			mutex_exit(&pidlock);
			return (set_errno(EINVAL));
		}
		mutex_enter(&p->p_lock);
		mutex_exit(&pidlock);

		if (p->p_signhdr == NULL) {
			mutex_exit(&p->p_lock);
			return (set_errno(EINVAL));
		}

		snqp = SIGN_PTR(p, id.sn_index);

		if (snqp->sn_snid == 0) {
			mutex_exit(&p->p_lock);
			return (set_errno(EINVAL));
		}

		if (snqp->sn_snid != get_sigid(cp, (caddr_t)sn_id)) {
			mutex_exit(&p->p_lock);
			return (set_errno(EINVAL));
		}

		snqp->sn_snid = 0;

		if (cmd == SN_SEND &&
		    ((sigqueue_t *)snqp)->sq_info.si_signo > 0) {
			sigaddqa(p, 0, (sigqueue_t *)snqp);
		} else {
			/* cmd == SN_CANCEL or signo == 0 (SIGEV_NONE) */
			sigqrel((sigqueue_t *)snqp);
		}
		mutex_exit(&p->p_lock);

		id.sn_pid = 0;
		id.sn_index = 0;

		break;

	default :
		return (set_errno(EINVAL));
	}

	if (copyout(&id, sn_id, sizeof (signotify_id_t)))
		return (set_errno(EFAULT));

	return (0);
}
