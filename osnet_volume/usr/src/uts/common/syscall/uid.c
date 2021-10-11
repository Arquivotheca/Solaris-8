/*
 * Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)uid.c	1.12	99/07/29 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/debug.h>
#include <sys/rce.h>

int
setuid(uid_t uid)
{
	register proc_t *p;
	int error = 0;
	int do_nocd = 0;
	cred_t	*cr, *newcr;
	uid_t oldruid = uid;
	int uidtype;

	if (uid < 0 || uid > MAXUID)
		return (set_errno(EINVAL));

	/*
	 * Need to pre-allocate the new cred structure before grabbing
	 * the p_crlock mutex.
	 */
	newcr = crget();
	p = ttoproc(curthread);
	mutex_enter(&p->p_crlock);
	cr = p->p_cred;
	if (cr->cr_uid && (uid == cr->cr_ruid || uid == cr->cr_suid)) {
		crcopy_to(cr, newcr);
		p->p_cred = newcr;
		newcr->cr_uid = uid;
		uidtype = SH_EUID;
	} else if (suser(cr)) {
		/*
		 * A super-user process that gives up its privilege
		 * must be marked to produce no core dump.
		 */
		if (cr->cr_uid != uid ||
		    cr->cr_ruid != uid ||
		    cr->cr_suid != uid)
			do_nocd = 1;
		oldruid = cr->cr_ruid;
		crcopy_to(cr, newcr);
		p->p_cred = newcr;
		newcr->cr_ruid = uid;
		newcr->cr_suid = uid;
		newcr->cr_uid = uid;
		uidtype = SH_RUID;
	} else {
		error = EPERM;
		crfree(newcr);
	}
	mutex_exit(&p->p_crlock);

	if (oldruid != uid) {
		mutex_enter(&pidlock);
		upcount_dec(oldruid);
		upcount_inc(uid);
		mutex_exit(&pidlock);
	}

	if (error == 0) {
		/*
		 * SRM hook: Provides SRM with notification of the process
		 * credentials change.
		 * The new credentials of curproc are given by newcr. Only a
		 * real uid change if root made the call (suser(cr) != 0).
		 * Even if there is no change in newcr->cr_uid the hook must
		 * still be called so SRM can track the successful setuid(),
		 * seteuid(), and setreuid() calls.
		 * SRM never needs to cause a setuid() system call to return
		 * error; the hook must only be called after all error returns
		 * have been processed.
		 */
		SRM_SETUID(uidtype, newcr);

		if (do_nocd) {
			mutex_enter(&p->p_lock);
			p->p_flag |= NOCD;
			mutex_exit(&p->p_lock);
		}
		crset(p, newcr);	/* broadcast to process threads */
		return (0);
	}
	return (set_errno(error));
}

int64_t
getuid(void)
{
	rval_t	r;
	cred_t *cr;

	cr = curthread->t_cred;
	r.r_val1 = cr->cr_ruid;
	r.r_val2 = cr->cr_uid;
	return (r.r_vals);
}

int
seteuid(uid_t uid)
{
	register proc_t *p;
	int error = 0;
	int do_nocd = 0;
	cred_t	*cr, *newcr;

	if (uid < 0 || uid > MAXUID)
		return (set_errno(EINVAL));

	/*
	 * Need to pre-allocate the new cred structure before grabbing
	 * the p_crlock mutex.
	 */
	newcr = crget();
	p = ttoproc(curthread);
	mutex_enter(&p->p_crlock);
	cr = p->p_cred;
	if (uid == cr->cr_ruid || uid == cr->cr_uid || uid == cr->cr_suid ||
	    suser(cr)) {
		/*
		 * A super-user process that makes itself look like a
		 * set-uid process must be marked to produce no core dump.
		 */
		if (cr->cr_uid != uid && suser(cr))
			do_nocd = 1;
		crcopy_to(cr, newcr);
		p->p_cred = newcr;
		newcr->cr_uid = uid;
	} else {
		error = EPERM;
		crfree(newcr);
	}
	mutex_exit(&p->p_crlock);

	if (error == 0) {
		/*
		 * SRM hook: always do change effective uid here.
		 */
		SRM_SETUID(SH_EUID, newcr);

		if (do_nocd) {
			mutex_enter(&p->p_lock);
			p->p_flag |= NOCD;
			mutex_exit(&p->p_lock);
		}
		crset(p, newcr);	/* broadcast to process threads */
		return (0);
	}
	return (set_errno(error));
}

/*
 * Buy-back from SunOS 4.x
 *
 * Like setuid() and seteuid() combined -except- that non-root users
 * can change cr_ruid to cr_uid, and the semantics of cr_suid are
 * subtly different.
 */
int
setreuid(uid_t ruid, uid_t euid)
{
	proc_t *p;
	int error = 0;
	int do_nocd = 0;
	uid_t oldruid = ruid;
	cred_t *cr, *newcr;

	if ((ruid != -1 && (ruid < 0 || ruid > MAXUID)) ||
	    (euid != -1 && (euid < 0 || euid > MAXUID)))
		return (set_errno(EINVAL));

	/*
	 * Need to pre-allocate the new cred structure before grabbing
	 * the p_crlock mutex.
	 */
	newcr = crget();

	p = ttoproc(curthread);
	mutex_enter(&p->p_crlock);
	cr = p->p_cred;

	if (ruid != -1 &&
	    ruid != cr->cr_ruid && ruid != cr->cr_uid && !suser(cr)) {
		error = EPERM;
	} else if (euid != -1 &&
	    euid != cr->cr_ruid && euid != cr->cr_uid &&
	    euid != cr->cr_suid && !suser(cr)) {
		error = EPERM;
	} else {
		crhold(cr);
		crcopy_to(cr, newcr);
		p->p_cred = newcr;

		if (euid != -1)
			newcr->cr_uid = euid;
		if (ruid != -1) {
			oldruid = newcr->cr_ruid;
			newcr->cr_ruid = ruid;
		}
		/*
		 * "If the real uid is being changed, or the effective uid is
		 * being changed to a value not equal to the real uid, the
		 * saved uid is set to the new effective uid."
		 */
		if (ruid != -1 ||
		    (euid != -1 && newcr->cr_uid != newcr->cr_ruid))
			newcr->cr_suid = newcr->cr_uid;
		/*
		 * A super-user process that gives up its privilege
		 * must be marked to produce no core dump.
		 */
		if ((cr->cr_uid != newcr->cr_uid ||
		    cr->cr_ruid != newcr->cr_ruid ||
		    cr->cr_suid != newcr->cr_suid) && suser(cr))
			do_nocd = 1;
		crfree(cr);
	}
	mutex_exit(&p->p_crlock);

	if (oldruid != ruid) {
		ASSERT(oldruid != -1 && ruid != -1);
		mutex_enter(&pidlock);
		upcount_dec(oldruid);
		upcount_inc(ruid);
		mutex_exit(&pidlock);
	}

	if (error == 0) {
		/*
		 * SRM hook: If ruid == -1 this is only an effective uid
		 * change.
		 */
		if (ruid == -1)
			SRM_SETUID(SH_EUID, newcr);
		else
			SRM_SETUID(SH_RUID, newcr);

		if (do_nocd) {
			mutex_enter(&p->p_lock);
			p->p_flag |= NOCD;
			mutex_exit(&p->p_lock);
		}
		crset(p, newcr);	/* broadcast to process threads */
		return (0);
	}
	crfree(newcr);
	return (set_errno(error));
}
