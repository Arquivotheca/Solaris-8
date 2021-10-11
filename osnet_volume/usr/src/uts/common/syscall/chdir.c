/*
 * Copyright (c) 1986-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

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
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *			All rights reserved.
 *
 */

#ident	"@(#)chdir.c	1.5	98/03/01 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/pathname.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mode.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/ioreq.h>
#include <sys/poll.h>
#include <sys/kmem.h>
#include <sys/filio.h>
#include <sys/cmn_err.h>

#include <sys/debug.h>
#include <c2/audit.h>

/*
 * Change current working directory (".").
 */
static int	chdirec(vnode_t *, int ischroot, int do_traverse);

int
chdir(char *fname)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));

	return (chdirec(vp, 0, 1));
}

/*
 * File-descriptor based version of 'chdir'.
 */
int
fchdir(int fd)
{
	vnode_t *vp;
	file_t *fp;

	if ((fp = getf(fd)) == NULL)
		return (set_errno(EBADF));
	vp = fp->f_vnode;
	VN_HOLD(vp);
	releasef(fd);
	return (chdirec(vp, 0, 0));
}

/*
 * Change notion of root ("/") directory.
 */
int
chroot(char *fname)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));

	return (chdirec(vp, 1, 1));
}

/*
 *	++++++++++++++++++++++++
 *	++  SunOS4.1 Buyback  ++
 *	++++++++++++++++++++++++
 * Change root directory with a user given fd
 */
int
fchroot(int fd)
{
	vnode_t *vp;
	file_t *fp;

	if ((fp = getf(fd)) == NULL)
		return (set_errno(EBADF));
	vp = fp->f_vnode;
	VN_HOLD(vp);
	releasef(fd);
	return (chdirec(vp, 1, 0));
}

static int
chdirec(vnode_t *vp, int ischroot, int do_traverse)
{
	int error;
	vnode_t *oldvp;
	proc_t *pp = curproc;
	vnode_t **vpp;

	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto bad;
	}
	if (error = VOP_ACCESS(vp, VEXEC, 0, CRED()))
		goto bad;

	/*
	 * The VOP_ACCESS() may have covered 'vp' with a new filesystem,
	 * if 'vp' is an autoFS vnode. Traverse the mountpoint so
	 * that we don't end up with a covered current directory.
	 */
	if (vp->v_vfsmountedhere != NULL && do_traverse) {
		if (error = traverse(&vp))
			goto bad;
	}

	/*
	 * Special chroot semantics:
	 * chroot is allowed if root or if the target is really
	 * a loopback mount of the root as determined by comparing
	 * dev and inode numbers
	 */
	if (ischroot) {
		struct vattr tattr;
		struct vattr rattr;

		tattr.va_mask = AT_FSID|AT_NODEID;
		if (error = VOP_GETATTR(vp, &tattr, 0, CRED()))
			goto bad;

		rattr.va_mask = AT_FSID|AT_NODEID;
		if (error = VOP_GETATTR(rootdir, &rattr, 0, CRED()))
			goto bad;

		if ((tattr.va_fsid != rattr.va_fsid ||
		    tattr.va_nodeid != rattr.va_nodeid) && !suser(CRED())) {
			error = EPERM;
			goto bad;
		}
		vpp = &PTOU(pp)->u_rdir;
	} else {
		vpp = &PTOU(pp)->u_cdir;
	}

#ifdef C2_AUDIT
	if (audit_active)	/* update abs cwd/root path see c2audit.c */
		audit_chdirec(vp, vpp);
#endif

	mutex_enter(&pp->p_lock);
	oldvp = *vpp;
	*vpp = vp;
	mutex_exit(&pp->p_lock);
	if (oldvp)
		VN_RELE(oldvp);
	return (0);

bad:
	VN_RELE(vp);
	return (set_errno(error));
}
