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

#ident	"@(#)symlink.c	1.3	97/05/02 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/pathname.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/debug.h>

/*
 * Create a symbolic link.  Similar to link or rename except target
 * name is passed as string argument, not converted to vnode reference.
 */
int
symlink(char *target, char *linkname)
{
	vnode_t *dvp;
	struct vattr vattr;
	struct pathname tpn;
	struct pathname lpn;
	int error;

	if (error = pn_get(linkname, UIO_USERSPACE, &lpn))
		return (set_errno(error));
	if (error = lookuppn(&lpn, NULL, NO_FOLLOW, &dvp, NULLVPP)) {
		pn_free(&lpn);
		return (set_errno(error));
	}
	if (dvp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		goto out;
	}
	if (error = pn_get(target, UIO_USERSPACE, &tpn))
		goto out;
	vattr.va_type = VLNK;
	vattr.va_mode = 0777;
	vattr.va_mask = AT_TYPE|AT_MODE;
	error = VOP_SYMLINK(dvp, lpn.pn_path, &vattr, tpn.pn_path, CRED());
	pn_free(&tpn);
out:
	pn_free(&lpn);
	VN_RELE(dvp);
	if (error)
		return (set_errno(error));
	return (0);
}
