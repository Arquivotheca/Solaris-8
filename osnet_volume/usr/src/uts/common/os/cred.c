/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cred.c	1.22	98/03/17 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/acct.h>
#include <sys/syscall.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/atomic.h>
#include <c2/audit.h>

struct credlist {
	union {
		cred_t		cru_cred;
		struct credlist *cru_next;
	} cl_U;
#define	cl_cred	cl_U.cru_cred
#define	cl_next	cl_U.cru_next
};

static struct kmem_cache *cred_cache;
static size_t crsize = 0;
cred_t *kcred;

/*
 * Initialize credentials data structures.
 */

void
cred_init()
{
	crsize = sizeof (cred_t) + sizeof (uid_t) * (ngroups_max - 1);
	/*
	 * Make sure it's word-aligned.
	 */
	crsize = (crsize + sizeof (int) - 1) & ~(sizeof (int) - 1);

	cred_cache = kmem_cache_create("cred_cache", crsize, 0,
		NULL, NULL, NULL, NULL, NULL, 0);
	/*
	 * kcred is used by anything that needs root permission.
	 */
	kcred = crget();

	/*
	 * Set up credentials of p0.
	 */
	ttoproc(curthread)->p_cred = kcred;
	curthread->t_cred = kcred;
}

/*
 * Allocate a zeroed cred structure and crhold() it.
 */
cred_t *
crget(void)
{
	cred_t *cr = kmem_cache_alloc(cred_cache, KM_SLEEP);

	bzero(cr, crsize);
	cr->cr_ref = 1;
	return (cr);
}

/*
 * Broadcast the cred to all the threads in the process.
 * The current thread's credentials can be set right away, but other
 * threads must wait until the start of the next system call or trap.
 * This avoids changing the cred in the middle of a system call.
 *
 * The cred has already been held for the process and the thread (2 holds),
 * and p->p_cred set.
 *
 * p->p_crlock shouldn't be held here, since p_lock must be acquired.
 */
void
crset(proc_t *p, cred_t *cr)
{
	kthread_id_t	t;
	kthread_id_t	first;

	ASSERT(p == curproc);	/* assumes p_lwpcnt can't change */

	t = curthread;
	crfree(t->t_cred);	/* free the old cred for the thread */
	t->t_cred = cr;		/* the cred is held by caller for this thread */

	/*
	 * Broadcast to other threads, if any.
	 */
	if (p->p_lwpcnt > 1) {
		mutex_enter(&p->p_lock);	/* to keep thread list safe */
		first = curthread;
		for (t = first->t_forw; t != first; t = t->t_forw)
			t->t_pre_sys = 1; /* so syscall will get new cred */
		mutex_exit(&p->p_lock);
	}
}

/*
 * Put a hold on a cred structure.
 */
void
crhold(cred_t *cr)
{
	atomic_add_32(&cr->cr_ref, 1);
}

/*
 * Release previous hold on a cred structure.  Free it if refcnt == 0.
 */
void
crfree(cred_t *cr)
{
	if (atomic_add_32_nv(&cr->cr_ref, -1) == 0) {
		ASSERT(cr != kcred);
		kmem_cache_free(cred_cache, cr);
	}
}

/*
 * Copy a cred structure to a new one and free the old one.
 *	The new cred will have two references.  One for the calling process,
 * 	and one for the thread.
 */
cred_t *
crcopy(cred_t *cr)
{
	cred_t *newcr;

	newcr = crget();
	bcopy(cr, newcr, crsize);
	crfree(cr);
	newcr->cr_ref = 2;		/* caller gets two references */
	return (newcr);
}

/*
 * Copy a cred structure to a new one and free the old one.
 *	The new cred will have two references.  One for the calling process,
 * 	and one for the thread.
 * This variation on crcopy uses a pre-allocated structure for the
 * "new" cred.
 */
void
crcopy_to(cred_t *oldcr, cred_t *newcr)
{
	bcopy(oldcr, newcr, crsize);
	crfree(oldcr);
	newcr->cr_ref = 2;		/* caller gets two references */
}

/*
 * Dup a cred struct to a new held one.
 *	The old cred is not freed.
 */
cred_t *
crdup(cred_t *cr)
{
	cred_t *newcr;

	newcr = crget();
	bcopy(cr, newcr, crsize);
	newcr->cr_ref = 1;
	return (newcr);
}

/*
 * Dup a cred struct to a new held one.
 *	The old cred is not freed.
 * This variation on crdup uses a pre-allocated structure for the
 * "new" cred.
 */
void
crdup_to(cred_t *oldcr, cred_t *newcr)
{
	bcopy(oldcr, newcr, crsize);
	newcr->cr_ref = 1;
}

/*
 * Return the (held) credentials for the current running process.
 */
cred_t *
crgetcred()
{
	cred_t *cr;
	proc_t *p;

	p = ttoproc(curthread);
	mutex_enter(&p->p_crlock);
	crhold(cr = p->p_cred);
	mutex_exit(&p->p_crlock);
	return (cr);
}

/*
 * Test if the supplied credentials identify the super-user.
 * Distasteful side-effect: set an accounting flag in the
 * caller's u-block if the answer is yes.
 */
int
suser(cred_t *cr)
{
	if (cr->cr_uid == 0) {
#ifdef C2_AUDIT
		if (audit_active)	/* suser success */
			audit_suser(1);
#endif
		u.u_acflag |= ASU;	/* XXX */
		return (1);
	}
#ifdef C2_AUDIT
	if (audit_active)		/* suser success */
		audit_suser(0);
#endif
	return (0);
}

/*
 * Determine whether the supplied group id is a member of the group
 * described by the supplied credentials.
 */
int
groupmember(gid_t gid, cred_t *cr)
{
	gid_t *gp, *endgp;

	if (gid == cr->cr_gid)
		return (1);
	endgp = &cr->cr_groups[cr->cr_ngroups];
	for (gp = cr->cr_groups; gp < endgp; gp++)
		if (*gp == gid)
			return (1);
	return (0);
}

/*
 * This function is called to check whether the credentials set
 * "scrp" has permission to act on credentials set "tcrp".  It enforces the
 * permission requirements needed to send a signal to a process.
 * The same requirements are imposed by other system calls, however.
 */
int
hasprocperm(cred_t *tcrp, cred_t *scrp)
{
	return (scrp->cr_uid == 0 ||
	    scrp->cr_uid  == tcrp->cr_ruid ||
	    scrp->cr_ruid == tcrp->cr_ruid ||
	    scrp->cr_uid  == tcrp->cr_suid ||
	    scrp->cr_ruid == tcrp->cr_suid);
}

/*
 * This routine is used to compare two credentials to determine if
 * they refer to the same "user".  If the pointers are equal, then
 * they must refer to the same user.  Otherwise, the contents of
 * the credentials are compared to see whether they are equivalent.
 *
 * This routine returns 0 if the credentials refer to the same user,
 * 1 if they do not.
 */
int
crcmp(cred_t *cr1, cred_t *cr2)
{

	if (cr1 == cr2)
		return (0);

	if (cr1->cr_uid == cr2->cr_uid &&
	    cr1->cr_gid == cr2->cr_gid &&
	    cr1->cr_ruid == cr2->cr_ruid &&
	    cr1->cr_rgid == cr2->cr_rgid &&
	    cr1->cr_ngroups == cr2->cr_ngroups) {
		return (bcmp((caddr_t)cr1->cr_groups, (caddr_t)cr2->cr_groups,
			    cr1->cr_ngroups * sizeof (gid_t)));
	}
	return (1);
}
