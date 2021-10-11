/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MNTFS_MNTDATA_H
#define	_SYS_MNTFS_MNTDATA_H

#pragma ident	"@(#)mntdata.h	1.2	99/08/13 SMI"

#include <sys/vnode.h>
#include <sys/poll.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct mntnode {
	timespec_t mnt_time;	/* time stamp of this snapshot */
	size_t mnt_size;	/* size of mnttab image */
	caddr_t mnt_base;	/* base address of image */
	uint_t mnt_nres;	/* number of mounted resources */
	uint_t *mnt_devlist;	/* device major/minor numbers */
	ino_t mnt_ino;		/* node id (for stat(2)) */
	vnode_t *mnt_mountvp;	/* vnode mounted on */
	vnode_t mnt_vnode;	/* embedded vnode */
} mntnode_t;

/*
 * Conversion macros.
 */
#define	VTOM(vp)	((struct mntnode *)(vp)->v_data)
#define	MTOV(pnp)	(&(pnp)->mnt_vnode)


#if defined(_KERNEL)

extern	struct vnodeops	mntvnodeops;
extern	uint_t mnt_nopen;	/* count of vnodes open on mnttab info */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MNTFS_MNTDATA_H */
