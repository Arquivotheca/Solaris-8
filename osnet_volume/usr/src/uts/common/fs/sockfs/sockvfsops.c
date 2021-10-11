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

#pragma	ident	"@(#)sockvfsops.c	1.8	99/05/28 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/vfs.h>
#include <sys/swap.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/thread.h>

#include <fs/fs_subr.h>

#include <sys/stream.h>
#include <sys/socket.h>
#include <sys/stropts.h>
#include <sys/socketvar.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

static struct vfssw vfw = {
	"sockfs",
	sockinit,
	&sock_vfsops,
	0
};

extern struct mod_ops mod_fsops;

/*
 * Module linkage information for the kernel.
 */
static struct modlfs modlfs = {
	&mod_fsops, "filesystem for sockfs", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	return (EBUSY);
}

/*
 * N.B.
 * No _fini routine. This module cannot be unloaded once loaded.
 * The NO_UNLOAD_STUB in modstub.s must change if this module ever
 * is modified to become unloadable.
 */

struct vfsops sock_vfsops = {
	fs_nosys,		/* mount */
	fs_nosys,		/* unmount */
	fs_nosys,		/* root */
	fs_nosys,		/* statvfs */
	fs_sync,		/* sync */
	fs_nosys,		/* vget */
	fs_nosys,		/* mountroot */
	fs_nosys,		/* swapvp */
	fs_freevfs
};
