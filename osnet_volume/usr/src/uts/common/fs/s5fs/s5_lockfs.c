/*	Copyright (c) 1991,1993 Sun Microsystems, Inc (SMI) */
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)s5_lockfs.c	1.3	94/04/19 SMI"
/* from "@(#)ufs_lockfs.c	1.12	92/08/13 SMI" */ /* SunOS-4.1.1 */

/* *********** SOME OF THESE INCLUDES ARE NO LONGER NEEDED *********** */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/mman.h>
#include <sys/pathname.h>
#include <sys/vtrace.h>

#include <sys/fs/s5_fs.h>
#include <sys/fs/s5_inode.h>
#include <sys/fs/s5_fsdir.h>
#include <sys/dirent.h>		/* must be AFTER <sys/fs/fsdir.h>! */
#include <sys/errno.h>
#include <sys/sysinfo.h>

/*
 * s5_lockfs_begin - start the lockfs locking protocol
 */
int
s5_lockfs_begin(ulp)
	struct ulockfs *ulp;
{
	mutex_enter(&ulp->ul_lock);
	ulp->ul_vnops_cnt++;
	mutex_exit(&ulp->ul_lock);

	curthread->t_flag |= T_DONTBLOCK;
	return (0);
}

int
s5_lockfs_vp_begin(vp, ulpp)
	struct vnode	*vp;
	struct ulockfs	**ulpp;
{
	/* file system has been forcibly unmounted */
	if (VTOI(vp)->i_s5vfs == NULL)	{
		*ulpp = NULL;
		return (EIO);
	} else if (curthread->t_flag & T_DONTBLOCK) {
		*ulpp = NULL;
		return (0);
	} else {
		/* inline s5_lockfs_begin() */
		struct ulockfs *ulp = VTOUL(vp);

		mutex_enter(&ulp->ul_lock);
		ulp->ul_vnops_cnt++;
		mutex_exit(&ulp->ul_lock);
		curthread->t_flag |= T_DONTBLOCK;
		*ulpp = ulp;
		return (0);
	}
}

/*
 * s5_lockfs_end - terminate the lockfs locking protocol
 */
void
s5_lockfs_end(ulp)
	struct ulockfs *ulp;
{
	if (ulp != NULL) {
		curthread->t_flag &= ~T_DONTBLOCK;
		mutex_enter(&ulp->ul_lock);
		if (--ulp->ul_vnops_cnt == 0)
			cv_broadcast(&ulp->ul_vnops_cnt_cv);
		mutex_exit(&ulp->ul_lock);
	}
}
