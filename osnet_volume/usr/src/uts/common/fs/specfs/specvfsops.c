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
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma	ident	"@(#)specvfsops.c	1.24	99/05/28 SMI"
/*	From:	SVr4.0	"fs:fs/specfs/specvfsops.c	1.15"	*/
#include "sys/types.h"
#include "sys/t_lock.h"
#include "sys/param.h"
#include "sys/buf.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/vfs.h"
#include "sys/swap.h"
#include "sys/vnode.h"
#include "sys/cred.h"
#include "sys/fs/snode.h"
#include "sys/thread.h"

#include "fs/fs_subr.h"

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

static struct vfssw vfw = {
	"specfs",
	specinit,
	&spec_vfsops,
	0
};

extern struct mod_ops mod_fsops;

/*
 * Module linkage information for the kernel.
 */
static struct modlfs modlfs = {
	&mod_fsops, "filesystem for specfs", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};

_init(void)
{
	return (mod_install(&modlinkage));
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * N.B.
 * No _fini routine. This module cannot be unloaded once loaded.
 * The NO_UNLOAD_STUB in modstub.s must change if this module ever
 * is modififed to become unloadable.
 */

static	int spec_sync(struct vfs *, short, struct cred *);
static	int spec_swapvp(struct vfs *, struct vnode **, char *);

struct vfsops spec_vfsops = {
	fs_nosys,		/* mount */
	fs_nosys,		/* unmount */
	fs_nosys,		/* root */
	fs_nosys,		/* statvfs */
	spec_sync,
	fs_nosys,		/* vget */
	fs_nosys,		/* mountroot */
	spec_swapvp,
	fs_freevfs
};

kmutex_t spec_syncbusy;		/* initialized in specinit() */

/*
 * Run though all the snodes and force write-back
 * of all dirty pages on the block devices.
 */
/*ARGSUSED*/
static int
spec_sync(struct vfs *vfsp,
	short	flag,
	struct cred *cr)
{
	struct snode *sync_list;
	register struct snode **spp, *sp, *spnext;
	register struct vnode *vp;

	if (mutex_tryenter(&spec_syncbusy) == 0)
		return (0);

	if (flag & SYNC_CLOSE)
		(void) strpunlink(cr);

	if (flag & SYNC_ATTR) {
		mutex_exit(&spec_syncbusy);
		return (0);
	}
	mutex_enter(&stable_lock);
	sync_list = NULL;
	/*
	 * Find all the snodes that are dirty and add them to the sync_list
	 */
	for (spp = stable; spp < &stable[STABLESIZE]; spp++) {
		for (sp = *spp; sp != NULL; sp = sp->s_next) {
			vp = STOV(sp);
			/*
			 * Don't bother sync'ing a vp if it's
			 * part of a virtual swap device.
			 */
			if (IS_SWAPVP(vp))
				continue;

			if (vp->v_type == VBLK && vp->v_pages) {
				/*
				 * Prevent vp from going away before we
				 * we get a chance to do a VOP_PUTPAGE
				 * via sync_list processing
				 */
				VN_HOLD(vp);
				sp->s_list = sync_list;
				sync_list = sp;
			}
		}
	}
	mutex_exit(&stable_lock);
	/*
	 * Now write out all the snodes we marked asynchronously.
	 */
	for (sp = sync_list; sp != NULL; sp = spnext) {
		spnext = sp->s_list;
		vp = STOV(sp);
		(void) VOP_PUTPAGE(vp, (offset_t)0, (u_int)0, B_ASYNC, cr);
		VN_RELE(vp);		/* Release our hold on vnode */
	}
	mutex_exit(&spec_syncbusy);
	return (0);
}

#include <sys/bootconf.h>

/*ARGSUSED*/
static int
spec_swapvp(struct vfs *vfsp, struct vnode **vpp, char *name)
{
	dev_t dev;

	dev = getswapdev(name);
	if (dev == NODEV)
		return (ENODEV);

	/*
	 * XXX: Should this be created by makespecvp() ??
	 */
	*vpp = commonvp(dev, VBLK);
	return (0);
}
