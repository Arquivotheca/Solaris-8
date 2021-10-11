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

#pragma	ident	"@(#)readlink.c	1.5	97/12/22 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/pathname.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/ioreq.h>
#include <sys/debug.h>

/*
 * Read the contents of a symbolic link.
 */
int
readlink(char *name, char *buf, size_t count)
{
	vnode_t *vp;
	struct iovec aiov;
	struct uio auio;
	int error;
	struct vattr vattr;

	if (error = lookupname(name, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp))
		return (set_errno(error));

	if (vp->v_type != VLNK) {
		/*
		 * Ask the underlying filesystem if it wants this
		 * object to look like a symlink at user-level.
		 */
		vattr.va_mask = AT_TYPE;
		if (VOP_GETATTR(vp, &vattr, 0, CRED()) != 0 ||
		    vattr.va_type != VLNK) {
			VN_RELE(vp);
			return (set_errno(EINVAL));
		}
	}
	aiov.iov_base = buf;
	aiov.iov_len = count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_loffset = 0;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_resid = count;
	if (error = VOP_READLINK(vp, &auio, CRED())) {
		VN_RELE(vp);
		return (set_errno(error));
	}
	VN_RELE(vp);
	return ((int)(count - auio.uio_resid));
}
