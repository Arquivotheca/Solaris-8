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
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986, 1987, 1988, 1989  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *			All rights reserved.
 *
 */

/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)acl.c	1.13	98/03/01 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/pathname.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mode.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/filio.h>
#include <sys/acl.h>
#include <sys/cmn_err.h>

#include <sys/unistd.h>
#include <sys/debug.h>

static int cacl(int cmd, int nentries, aclent_t *aclbufp,
    vnode_t *vp, int *rv);

/*
 * Get/Set ACL of a file.
 */
int
acl(const char *fname, int cmd, int nentries, aclent_t *aclbufp)
{
	struct vnode *vp;
	int error;
	int rv = 0;

	/* Sanity check arguments */
	if (fname == NULL)
		return (set_errno(EINVAL));
	error = lookupname((char *)fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp);
	if (error)
		return (set_errno(error));

	error = cacl(cmd, nentries, aclbufp, vp, &rv);
	VN_RELE(vp);
	if (error)
		return (set_errno(error));
	return (rv);
}

/*
 * Get/Set ACL of a file with facl system call.
 */
int
facl(int fdes, int cmd, int nentries, aclent_t *aclbufp)
{
	file_t *fp;
	int error;
	int rv = 0;

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
#ifdef C2_AUDIT
	if (fp->f_flag & FREVOKED) {
		releasef(fdes);
		return (set_errno(EBADF));
	}
#endif /* C2_AUDIT */

	error = cacl(cmd, nentries, aclbufp, fp->f_vnode, &rv);
	releasef(fdes);

	if (error)
		return (set_errno(error));
	return (rv);
}


/*
 * Common code for acl() and facl().
 */
static int
cacl(int cmd, int nentries, aclent_t *aclbufp, vnode_t *vp, int *rv)
{
	int		error;
	int		aclbsize;	/* size of acl list in bytes */
	int		dfaclbsize;	/* size of default acl list in bytes */
	int		numacls;
	caddr_t		uaddrp;
	aclent_t	*aclp, *aaclp;
	vsecattr_t	vsecattr;

	ASSERT(vp);

	bzero(&vsecattr, sizeof (vsecattr_t));

	switch (cmd) {
	case GETACLCNT:
		if (!vp->v_op->vop_getsecattr)
			return (ENOSYS);
		vsecattr.vsa_mask = VSA_ACLCNT | VSA_DFACLCNT;
		if (error = VOP_GETSECATTR(vp, &vsecattr, 0, CRED()))
			return (error);
		*rv = vsecattr.vsa_aclcnt + vsecattr.vsa_dfaclcnt;
		if (vsecattr.vsa_aclcnt && vsecattr.vsa_aclentp) {
			kmem_free(vsecattr.vsa_aclentp,
			    vsecattr.vsa_aclcnt * sizeof (aclent_t));
		}
		if (vsecattr.vsa_dfaclcnt && vsecattr.vsa_dfaclentp) {
			kmem_free(vsecattr.vsa_dfaclentp,
			    vsecattr.vsa_dfaclcnt * sizeof (aclent_t));
		}
		break;
	case GETACL:
		if (!vp->v_op->vop_getsecattr)
			return (ENOSYS);
		/*
		 * Minimum ACL size is three entries so might as well
		 * bail out here.
		 */
		if (nentries < 3)
			return (EINVAL);
		/*
		 * NULL output buffer is also a pretty easy bail out.
		 */
		if (aclbufp == NULL)
			return (EFAULT);
		vsecattr.vsa_mask = VSA_ACL | VSA_ACLCNT | VSA_DFACL |
		    VSA_DFACLCNT;
		if (error = VOP_GETSECATTR(vp, &vsecattr, 0, CRED()))
			return (error);
		/* Check user's buffer is big enough */
		numacls = vsecattr.vsa_aclcnt + vsecattr.vsa_dfaclcnt;
		aclbsize = vsecattr.vsa_aclcnt * sizeof (aclent_t);
		dfaclbsize = vsecattr.vsa_dfaclcnt * sizeof (aclent_t);
		if (numacls > nentries) {
			error = ENOSPC;
			goto errout;
		}
		/* Sort the acl & default acl lists */
		if (vsecattr.vsa_aclcnt > 1)
			ksort((caddr_t)vsecattr.vsa_aclentp,
			vsecattr.vsa_aclcnt, sizeof (aclent_t), cmp2acls);
		if (vsecattr.vsa_dfaclcnt > 1)
			ksort((caddr_t)vsecattr.vsa_dfaclentp,
			vsecattr.vsa_dfaclcnt, sizeof (aclent_t), cmp2acls);
		/* Copy out acl's */
		uaddrp = (caddr_t)aclbufp;
		if (aclbsize > 0) {	/* bug #1262490 */
			if (copyout(vsecattr.vsa_aclentp, uaddrp, aclbsize)) {
				error = EFAULT;
				goto errout;
			}
		}
		/* Copy out default acl's */
		if (dfaclbsize > 0) {
			uaddrp += aclbsize;
			if (copyout(vsecattr.vsa_dfaclentp,
			    uaddrp, dfaclbsize)) {
				error = EFAULT;
				goto errout;
			}
		}
		*rv = numacls;
		if (vsecattr.vsa_aclcnt) {
			kmem_free(vsecattr.vsa_aclentp,
			    vsecattr.vsa_aclcnt * sizeof (aclent_t));
		}
		if (vsecattr.vsa_dfaclcnt) {
			kmem_free(vsecattr.vsa_dfaclentp,
			    vsecattr.vsa_dfaclcnt * sizeof (aclent_t));
		}
		break;

	case SETACL:
		if (!vp->v_op->vop_setsecattr)
			return (ENOSYS);
		/*
		 * Minimum ACL size is three entries so might as well
		 * bail out here.  Also limit request size to prevent user
		 * from allocating too much kernel memory.  Maximum size
		 * is MAX_ACL_ENTRIES for the ACL part and MAX_ACL_ENTRIES
		 * for the default ACL part. (bug 4058667)
		 */
		if (nentries < 3 || nentries > (MAX_ACL_ENTRIES * 2))
			return (EINVAL);
		/*
		 * NULL output buffer is also an easy bail out.
		 */
		if (aclbufp == NULL)
			return (EFAULT);
		vsecattr.vsa_mask = VSA_ACL;
		aclbsize = nentries * sizeof (aclent_t);
		vsecattr.vsa_aclentp = kmem_alloc(aclbsize, KM_SLEEP);
		aaclp = vsecattr.vsa_aclentp;
		vsecattr.vsa_aclcnt = nentries;
		uaddrp = (caddr_t)aclbufp;
		if (copyin(uaddrp, vsecattr.vsa_aclentp, aclbsize)) {
			kmem_free(aaclp, aclbsize);
			return (EFAULT);
		}
		/* Sort the acl list */
		ksort((caddr_t)vsecattr.vsa_aclentp,
		    vsecattr.vsa_aclcnt, sizeof (aclent_t), cmp2acls);

		/* Break into acl and default acl lists */
		for (numacls = 0, aclp = vsecattr.vsa_aclentp;
		    numacls < vsecattr.vsa_aclcnt;
		    aclp++, numacls++) {
			if (aclp->a_type & ACL_DEFAULT)
				break;
		}

		/* Find where defaults start (if any) */
		if (numacls < vsecattr.vsa_aclcnt) {
			vsecattr.vsa_mask |= VSA_DFACL;
			vsecattr.vsa_dfaclcnt = nentries - numacls;
			vsecattr.vsa_dfaclentp = aclp;
			vsecattr.vsa_aclcnt = numacls;
		}
		/* Adjust if they're all defaults */
		if (vsecattr.vsa_aclcnt == 0) {
			vsecattr.vsa_mask &= ~VSA_ACL;
			vsecattr.vsa_aclentp = NULL;
		}
		/* Only directories can have defaults */
		if (vsecattr.vsa_dfaclcnt && vp->v_type != VDIR) {
			kmem_free(aaclp, aclbsize);
			return (ENOTDIR);
		}
		VOP_RWLOCK(vp, 1);
		if (error = VOP_SETSECATTR(vp, &vsecattr, 0, CRED())) {
			kmem_free(aaclp, aclbsize);
			VOP_RWUNLOCK(vp, 1);
			return (error);
		}

		/*
		 * Should return 0 upon success according to the man page
		 * and SVR4 semantics. (Bug #1214399: SETACL returns wrong rc)
		 */
		*rv = 0;
		kmem_free(aaclp, aclbsize);
		VOP_RWUNLOCK(vp, 1);
		break;
	default:
		return (EINVAL);
	}

	return (0);

errout:
	if (aclbsize && vsecattr.vsa_aclentp)
		kmem_free(vsecattr.vsa_aclentp, aclbsize);
	if (dfaclbsize && vsecattr.vsa_dfaclentp)
		kmem_free(vsecattr.vsa_dfaclentp, dfaclbsize);
	return (error);
}


/*
 * Generic shellsort, from K&R (1st ed, p 58.), somewhat modified.
 * v = Ptr to array/vector of objs
 * n = # objs in the array
 * s = size of each obj (must be multiples of a word size)
 * f = ptr to function to compare two objs
 *	returns (-1 = less than, 0 = equal, 1 = greater than
 */
void
ksort(caddr_t v, int n, int s, int (*f)())
{
	int g, i, j, ii;
	unsigned int *p1, *p2;
	unsigned int tmp;

	/* No work to do */
	if (v == NULL || n <= 1)
		return;

	/* Sanity check on arguments */
	ASSERT(((uintptr_t)v & 0x3) == 0 && (s & 0x3) == 0);
	ASSERT(s > 0);
	for (g = n / 2; g > 0; g /= 2) {
		for (i = g; i < n; i++) {
			for (j = i - g; j >= 0 &&
				(*f)(v + j * s, v + (j + g) * s) == 1;
					j -= g) {
				p1 = (unsigned *)(v + j * s);
				p2 = (unsigned *)(v + (j + g) * s);
				for (ii = 0; ii < s / 4; ii++) {
					tmp = *p1;
					*p1++ = *p2;
					*p2++ = tmp;
				}
			}
		}
	}
}

/*
 * Compare two acls, all fields.  Returns:
 * -1 (less than)
 *  0 (equal)
 * +1 (greater than)
 */
int
cmp2acls(aclent_t *x, aclent_t *y)
{
	/* Compare types */
	if (x->a_type < y->a_type)
		return (-1);
	if (x->a_type > y->a_type)
		return (1);
	/* Equal types; compare id's */
	if (x->a_id < y->a_id)
		return (-1);
	if (x->a_id > y->a_id)
		return (1);
	/* Equal ids; compare perms */
	if (x->a_perm < y->a_perm)
		return (-1);
	if (x->a_perm > y->a_perm)
		return (1);
	/* Totally equal */
	return (0);
}
