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
 * 	(c) 1986, 1987, 1988, 1989, 1996  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *			All rights reserved.
 *
 */

#ident	"@(#)mknod.c	1.7	98/01/30 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mode.h>
#include <sys/uio.h>
#include <sys/mkdev.h>
#include <sys/debug.h>

/*
 * Create a special file, a regular file, or a FIFO.
 * fname - pathname passed by user
 * fmode - mode of pathname
 * dev = device number - b/c specials only
 */
int
mknod(char *fname, mode_t fmode, dev_t dev)
{
	vnode_t *vp;
	struct vattr vattr;
	int error;
	enum create why;

	/*
	 * Zero type is equivalent to a regular file.
	 */
	if ((fmode & S_IFMT) == 0)
		fmode |= S_IFREG;

	/*
	 * Must be the super-user unless making a FIFO node.
	 */
	if (((fmode & S_IFMT) != S_IFIFO) && !suser(CRED()))
		return (set_errno(EPERM));
	/*
	 * Set up desired attributes and vn_create the file.
	 */
	vattr.va_type = IFTOVT(fmode);
	vattr.va_mode = fmode & MODEMASK;
	vattr.va_mask = AT_TYPE|AT_MODE;
	if (vattr.va_type == VCHR || vattr.va_type == VBLK) {
		if (get_udatamodel() != DATAMODEL_NATIVE)
			dev = expldev(dev);
		if (dev == NODEV || (getemajor(dev)) == NODEV)
			return (set_errno(EINVAL));
		vattr.va_rdev = dev;
		vattr.va_mask |= AT_RDEV;
	}
	why = ((fmode & S_IFMT) == S_IFDIR) ? CRMKDIR : CRMKNOD;
	if (error = vn_create(fname, UIO_USERSPACE, &vattr, EXCL, 0, &vp,
	    why, 0, u.u_cmask))
		return (set_errno(error));
	VN_RELE(vp);
	return (0);
}

/* ARGSUSED */
int
xmknod(int version, char *fname, mode_t fmode, dev_t dev)
{
	return (mknod(fname, fmode, dev));
}
