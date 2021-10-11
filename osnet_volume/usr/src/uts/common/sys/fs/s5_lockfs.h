/*
 * Copyright (c) 1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FS_S5_LOCKFS_H
#define	_SYS_FS_S5_LOCKFS_H

#pragma ident	"@(#)s5_lockfs.h	1.4	98/01/06 SMI"

#include <sys/lockfs.h>
#include <vm/seg.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The S5 file system does not support filesystem locking ioctls.
 * This is a minimal implementation of locking code derived from
 * Solaris UFS filesystem code.
 */

struct ulockfs {
	struct vfs	*ul_vfsp;
	ulong_t		ul_vnops_cnt;	/* no. of active vnops outstanding */
	kmutex_t	ul_lock;	/* protects ulockfs structure */
	kcondvar_t 	ul_vnops_cnt_cv;
};

#define	VTOUL(VP)	((struct ulockfs *)&			\
		((struct s5vfs *)((VP)->v_vfsp->vfs_data))	\
		->vfs_ulockfs)
#define	ITOUL(IP)	((struct ulockfs *)&((IP)->i_s5vfs->vfs_ulockfs))

#if defined(_KERNEL) && defined(__STDC__)

extern	int	s5_lockfs_begin(struct ulockfs *);
extern	void	s5_lockfs_end(struct ulockfs *);
extern	int	s5_lockfs_vp_begin(struct vnode *, struct ulockfs **);
extern	void	s5_lockfs_vp_end(struct ulockfs *);

#endif	/* defined(_KERNEL) && defined(__STDC__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_S5_LOCKFS_H */
