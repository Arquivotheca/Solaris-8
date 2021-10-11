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

#ident	"@(#)chown.c	1.4	95/01/23 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
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
#include <sys/filio.h>
#include <sys/debug.h>

extern int	namesetattr(char *, enum symfollow, vattr_t *, int);
extern int	fdsetattr(int, vattr_t *);

/*
 * Change ownership of file given file name.
 */
int
chown(char *fname, uid_t uid, gid_t gid)
{
	struct vattr vattr;

	if (uid < -1 || uid > MAXUID || gid < -1 || gid > MAXUID)
		return (set_errno(EINVAL));
	vattr.va_uid = uid;
	vattr.va_gid = gid;
	vattr.va_mask = 0;
	if (vattr.va_uid != -1)
		vattr.va_mask |= AT_UID;
	if (vattr.va_gid != -1)
		vattr.va_mask |= AT_GID;
	return (namesetattr(fname, FOLLOW, &vattr, 0));
}

int
lchown(char *fname, uid_t uid, gid_t gid)
{
	struct vattr vattr;

	if (uid < -1 || uid > MAXUID || gid < -1 || gid > MAXUID)
		return (set_errno(EINVAL));
	vattr.va_uid = uid;
	vattr.va_gid = gid;
	vattr.va_mask = 0;
	if (vattr.va_uid != -1)
		vattr.va_mask |= AT_UID;
	if (vattr.va_gid != -1)
		vattr.va_mask |= AT_GID;
	return (namesetattr(fname, NO_FOLLOW, &vattr, 0));
}

/*
 * Change ownership of file given file descriptor.
 */
int
fchown(int fd, uid_t uid, uid_t gid)
{
	struct vattr vattr;

	if (uid < -1 || uid > MAXUID || gid < -1 || gid > MAXUID)
		return (set_errno(EINVAL));
	vattr.va_uid = uid;
	vattr.va_gid = gid;
	vattr.va_mask = 0;
	if (vattr.va_uid != -1)
		vattr.va_mask |= AT_UID;
	if (vattr.va_gid != -1)
		vattr.va_mask |= AT_GID;
	return (fdsetattr(fd, &vattr));
}
