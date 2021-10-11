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


#pragma	ident	"@(#)pathconf.c	1.7	98/04/14 SMI"	/* SVr4 1.103	*/

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
#include <sys/debug.h>

/*
 * Common code for pathconf(), fpathconf() system calls
 */
static long
cpathconf(register vnode_t *vp, int cmd, struct cred *cr)
{
	int error;
	u_long val;

	switch (cmd) {
	case _PC_ASYNC_IO:
		return (1l);

	case _PC_PRIO_IO:
		return ((long)set_errno(EINVAL));

	case _PC_SYNC_IO:
		if ((error = VOP_FSYNC(vp, FSYNC, cr)) == 0)
			return (1l);
		else
			return ((long)set_errno(error));

	default:
		if (error = VOP_PATHCONF(vp, cmd, &val, cr))
			return ((long)set_errno(error));
		return (val);
	}
}

/* fpathconf/pathconf interfaces */

long
fpathconf(int fdes, int name)
{
	file_t *fp;
	long retval;

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	retval = cpathconf(fp->f_vnode, name, fp->f_cred);
	releasef(fdes);
	return (retval);
}

long
pathconf(char *fname, int name)
{
	vnode_t *vp;
	long	retval;
	int	error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return ((long)set_errno(error));
	retval = cpathconf(vp, name, CRED());
	VN_RELE(vp);
	return (retval);
}
