/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)gid.c	1.10	97/09/22 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/debug.h>

int
setgid(gid_t gid)
{
	register proc_t *p;
	int error = 0;
	int do_nocd = 0;
	cred_t	*cr, *newcr;

	if (gid < 0 || gid > MAXUID)
		return (set_errno(EINVAL));

	/*
	 * Need to pre-allocate the new cred structure before grabbing
	 * the p_crlock mutex.
	 */
	newcr = crget();
	p = ttoproc(curthread);
	mutex_enter(&p->p_crlock);
	cr = p->p_cred;
	if (cr->cr_uid && (gid == cr->cr_rgid || gid == cr->cr_sgid)) {
		crcopy_to(cr, newcr);
		p->p_cred = newcr;
		newcr->cr_gid = gid;
	} else if (suser(cr)) {
		/*
		 * A super-user process that makes itself look like a
		 * set-gid process must be marked to produce no core dump.
		 */
		if (cr->cr_gid != gid ||
		    cr->cr_rgid != gid ||
		    cr->cr_sgid != gid)
			do_nocd = 1;
		crcopy_to(cr, newcr);
		p->p_cred = newcr;
		newcr->cr_gid = gid;
		newcr->cr_rgid = gid;
		newcr->cr_sgid = gid;
	} else {
		error = EPERM;
		crfree(newcr);
	}
	mutex_exit(&p->p_crlock);

	if (error == 0) {
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
getgid(void)
{
	rval_t	r;
	cred_t	*cr;

	cr = curthread->t_cred;
	r.r_val1 = cr->cr_rgid;
	r.r_val2 = cr->cr_gid;
	return (r.r_vals);
}

int
setegid(gid_t gid)
{
	register proc_t *p;
	register cred_t	*cr, *newcr;
	int error = 0;
	int do_nocd = 0;

	if (gid < 0 || gid > MAXUID)
		return (set_errno(EINVAL));

	/*
	 * Need to pre-allocate the new cred structure before grabbing
	 * the p_crlock mutex.
	 */
	newcr = crget();
	p = ttoproc(curthread);
	mutex_enter(&p->p_crlock);
	cr = p->p_cred;
	if (gid == cr->cr_rgid || gid == cr->cr_gid || gid == cr->cr_sgid ||
	    suser(cr)) {
		/*
		 * A super-user process that makes itself look like a
		 * set-gid process must be marked to produce no core dump.
		 */
		if (cr->cr_gid != gid && suser(cr))
			do_nocd = 1;
		crcopy_to(cr, newcr);
		p->p_cred = newcr;
		newcr->cr_gid = gid;
	} else {
		error = EPERM;
		crfree(newcr);
	}
	mutex_exit(&p->p_crlock);

	if (error == 0) {
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
 * Like setgid() and setegid() combined -except- that non-root users
 * can change cr_rgid to cr_gid, and the semantics of cr_sgid are
 * subtly different.
 */
int
setregid(gid_t rgid, gid_t egid)
{
	proc_t *p;
	int error = 0;
	int do_nocd = 0;
	cred_t *cr, *newcr;

	if ((rgid != -1 && (rgid < 0 || rgid > MAXUID)) ||
	    (egid != -1 && (egid < 0 || egid > MAXUID)))
		return (set_errno(EINVAL));

	/*
	 * Need to pre-allocate the new cred structure before grabbing
	 * the p_crlock mutex.
	 */
	newcr = crget();

	p = ttoproc(curthread);
	mutex_enter(&p->p_crlock);
	cr = p->p_cred;

	if (rgid != -1 &&
	    rgid != cr->cr_rgid && rgid != cr->cr_gid &&
	    rgid != cr->cr_sgid && !suser(cr)) {
		error = EPERM;
	} else if (egid != -1 &&
	    egid != cr->cr_rgid && egid != cr->cr_gid &&
	    egid != cr->cr_sgid && !suser(cr)) {
		error = EPERM;
	} else {
		crhold(cr);
		crcopy_to(cr, newcr);
		p->p_cred = newcr;

		if (egid != -1)
			newcr->cr_gid = egid;
		if (rgid != -1)
			newcr->cr_rgid = rgid;
		/*
		 * "If the real gid is being changed, or the effective gid is
		 * being changed to a value not equal to the real gid, the
		 * saved gid is set to the new effective gid."
		 */
		if (rgid != -1 ||
		    (egid != -1 && newcr->cr_gid != newcr->cr_rgid))
			newcr->cr_sgid = newcr->cr_gid;
		/*
		 * A super-user process that makes itself look like a
		 * set-gid process must be marked to produce no core dump.
		 */
		if ((cr->cr_gid != newcr->cr_gid ||
		    cr->cr_rgid != newcr->cr_rgid ||
		    cr->cr_sgid != newcr->cr_sgid) && suser(cr))
			do_nocd = 1;
		crfree(cr);
	}
	mutex_exit(&p->p_crlock);

	if (error == 0) {
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
