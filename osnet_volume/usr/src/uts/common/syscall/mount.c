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
 * 	Copyright (c) 1986-1989,1993,1997,1999 by Sun Microsystems, Inc.
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)mount.c	1.15	99/09/27 SMI"	/* SVr4 1.42	*/

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/user.h>
#include <sys/fstyp.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/dnlc.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/cmn_err.h>
#include <sys/swap.h>
#include <sys/debug.h>
#include <sys/pathname.h>
#include <sys/cladm.h>

/*
 * System calls.
 */

/*
 * "struct mounta" defined in sys/vfs.h.
 */

/* ARGSUSED */
int
mount(long *lp, rval_t *rp)
{
	vnode_t *vp = NULL;
	struct vfs *vfsp;	/* dummy argument */
	int error;
	struct mounta *uap;
#if defined(_LP64)
	struct mounta native;

	/*
	 * Make a struct mounta if we are DATAMODEL_LP64
	 */
	uap = &native;
	uap->spec = (char *)*lp++;
	uap->dir = (char *)*lp++;
	uap->flags = (int)*lp++;
	uap->fstype = (char *)*lp++;
	uap->dataptr = (char *)*lp++;
	uap->datalen = (int)*lp++;
	uap->optptr = (char *)*lp++;
	uap->optlen = (int)*lp++;
#else	/* !defined(_LP64) */
	/*
	 * 32 bit kernels can take a shortcut and just cast
	 * the args array to the structure.
	 */
	uap = (struct mounta *)lp;
#endif	/* _LP64 */
	/*
	 * Resolve second path name (mount point).
	 */
	if (error = lookupname(uap->dir, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));

	/*
	 * Some mount flags are disallowed through the system call interface.
	 */
	uap->flags &= MS_MASK;

	if ((vp->v_flag & VPXFS) && ((uap->flags & MS_GLOBAL) != MS_GLOBAL)) {
		/*
		 * Clustering: if we're doing a mount onto the global
		 * namespace, and the mount is not a global mount, return
		 * an error.
		 */
		error = ENOTSUP;
	} else if (uap->flags & MS_GLOBAL) {
		/*
		 * Clustering: global mount specified.
		 */
		if ((cluster_bootflags & CLUSTER_BOOTED) == 0) {
			/*
			 * If we're not booted as a cluster,
			 * global mounts are not allowed.
			 */
			error = ENOTSUP;
		} else {
			error = domount("pxfs", uap, vp, CRED(), &vfsp);
			if (!error)
				VFS_RELE(vfsp);
		}
	} else {
		error = domount(NULL, uap, vp, CRED(), &vfsp);
		if (!error)
			VFS_RELE(vfsp);
	}
	VN_RELE(vp);
	return (error ? set_errno(error) : 0);
}
