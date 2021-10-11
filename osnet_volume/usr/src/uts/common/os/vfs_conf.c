/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)vfs_conf.c	1.42	99/08/07 SMI"	/* SunOS-4.1 1.16	*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/vfs.h>
#include <sys/t_lock.h>

extern	struct vfsops vfs_strayops;	/* XXX move here from vfs.c ? */

extern int swapinit(struct vfssw *vswp, int fstype);
extern struct vfsops swap_vfsops;

/*
 * WARNING: THE POSITIONS OF FILESYSTEM TYPES IN THIS TABLE SHOULD NOT
 * BE CHANGED. These positions are used in generating fsids and fhandles.
 * Thus, changing positions will cause a server to change the fhandle it
 * gives out for a file.
 *
 * XXX - is this still true?  AT&T's code doesn't appear to try to make
 * sure it is so.
 */

struct vfssw vfssw[] = {
	"BADVFS", NULL, &vfs_strayops, 0, NULL,	/* invalid */
	"specfs", NULL, NULL, 0, NULL,		/* SPECFS */
	"ufs", NULL, NULL, 0, NULL,		/* UFS */
	"fifofs", NULL, NULL, 0, NULL,		/* FIFOFS */
	"namefs", NULL, NULL, 0, NULL,		/* NAMEFS */
	"proc", NULL, NULL, 0, NULL,		/* PROCFS */
	"s5fs", NULL, NULL, 0, NULL,		/* S5FS */
	"nfs", NULL, NULL, 0, NULL,		/* NFS Version 2 */
	"", NULL, NULL, 0, NULL,		/* was RFS before */
	"hsfs", NULL, NULL, 0, NULL,		/* HSFS */
	"lofs", NULL, NULL, 0, NULL,		/* LOFS */
	"tmpfs", NULL, NULL, 0, NULL,		/* TMPFS */
	"fd", NULL, NULL, 0, NULL,		/* FDFS */
	"pcfs", NULL, NULL, 0, NULL,		/* PCFS */
	"swapfs", swapinit, NULL, 0, NULL,	/* SWAPFS */
	"mntfs", NULL, NULL, 0, NULL,		/* MNTFS */
	"", NULL, NULL, 0, NULL,		/* reserved for loadable fs */
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL,
	"", NULL, NULL, 0, NULL
};

int nfstype = (sizeof (vfssw) / sizeof (vfssw[0]));
