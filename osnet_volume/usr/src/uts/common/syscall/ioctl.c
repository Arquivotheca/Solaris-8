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

#pragma ident	"@(#)ioctl.c	1.13	98/03/01 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ttold.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mode.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/filio.h>
#include <sys/sunddi.h>
#include <sys/debug.h>
#include <sys/int_limits.h>
#include <sys/model.h>

/*
 * I/O control.
 */

int
ioctl(int fdes, int cmd, intptr_t arg)
{
	file_t *fp;
	int error = 0;
	vnode_t *vp;
	struct vattr vattr;
	int32_t flag;
	int rv = 0;

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	vp = fp->f_vnode;

	if (vp->v_type == VREG || vp->v_type == VDIR) {
		/*
		 * Handle these two ioctls for regular files and
		 * directories.  All others will usually be failed
		 * with ENOTTY by the VFS-dependent code.  System V
		 * always failed all ioctls on regular files, but SunOS
		 * supported these.
		 */
		switch (cmd) {
		case FIONREAD: {
			/*
			 * offset is int32_t because that is what FIONREAD
			 * is defined in terms of.  We cap at INT_MAX as in
			 * other cases for this ioctl.
			 */
			int32_t offset;

			vattr.va_mask = AT_SIZE;
			error = VOP_GETATTR(vp, &vattr, 0, fp->f_cred);
			if (error) {
				releasef(fdes);
				return (set_errno(error));
			}
			offset = MIN(vattr.va_size - fp->f_offset, INT_MAX);
			if (copyout(&offset, (caddr_t)arg, sizeof (offset))) {
				releasef(fdes);
				return (set_errno(EFAULT));
			}
			releasef(fdes);
			return (0);
			}

		case FIONBIO:
			if (copyin((caddr_t)arg, &flag, sizeof (flag))) {
				releasef(fdes);
				return (set_errno(EFAULT));
			}
			mutex_enter(&fp->f_tlock);
			if (flag)
				fp->f_flag |= FNDELAY;
			else
				fp->f_flag &= ~FNDELAY;
			mutex_exit(&fp->f_tlock);
			releasef(fdes);
			return (0);

		default:
			break;
		}
	}

	/*
	 * ioctl() now passes in the model information in some high bits.
	 */
	flag = fp->f_flag | get_udatamodel();
	error = VOP_IOCTL(fp->f_vnode, cmd, arg, flag, fp->f_cred, &rv);
	if (error != 0) {
		releasef(fdes);
		return (set_errno(error));
	}
	switch (cmd) {
	case FIONBIO:
		if (copyin((caddr_t)arg, &flag, sizeof (flag))) {
			releasef(fdes);
			return (set_errno(EFAULT));
		}
		mutex_enter(&fp->f_tlock);
		if (flag)
			fp->f_flag |= FNDELAY;
		else
			fp->f_flag &= ~FNDELAY;
		mutex_exit(&fp->f_tlock);
		break;

	default:
		break;
	}
	releasef(fdes);
	return (rv);
}

/*
 * Old stty and gtty.  (Still.)
 */
int
stty(int fdes, intptr_t arg)
{
	return (ioctl(fdes, TIOCSETP, arg));
}

int
gtty(int fdes, intptr_t arg)
{
	return (ioctl(fdes, TIOCGETP, arg));
}
