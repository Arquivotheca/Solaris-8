/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)door_vnops.c	1.13	99/08/31 SMI"

#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/door.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <fs/fs_subr.h>


kmutex_t	door_knob;
static int	door_open(struct vnode **vpp, int flag, struct cred *cr);
static int	door_close(struct vnode *vp, int flag, int count,
			offset_t offset, struct cred *cr);
static int	door_getattr(struct vnode *vp, struct vattr *vap,
			int flags, struct cred *cr);
static void	door_inactive(struct vnode *vp, struct cred *cr);
static int	door_access(struct vnode *vp, int mode, int flags,
			struct cred *cr);
static int	door_realvp(vnode_t *vp, vnode_t **vpp);

struct vfsops door_vfsops = {
	fs_nosys,	/* mount */
	fs_nosys,	/* unmount */
	fs_nosys,	/* root */
	fs_nosys, 	/* statvfs */
	fs_sync,	/* sync */
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys,	/* swapvp */
	fs_freevfs
};

struct vfs door_vfs;

struct vnodeops door_vnodeops = {
	door_open,
	door_close,
	fs_nosys,	/* read */
	fs_nosys,	/* write */
	fs_nosys,	/* ioctl */
	fs_setfl,	/* setfl */
	door_getattr,	/* getattr */
	fs_nosys,	/* setattr */
	door_access,	/* access */
	fs_nosys,	/* lookup */
	fs_nosys,	/* create */
	fs_nosys,	/* remove */
	fs_nosys,	/* link */
	fs_nosys,	/* rename */
	fs_nosys,	/* mkdir */
	fs_nosys,	/* rmdir */
	fs_nosys,	/* readdir */
	fs_nosys,	/* symlink */
	fs_nosys,	/* readlink */
	fs_nosys, 	/* fsync */
	door_inactive,
	fs_nosys, 	/* fid */
	fs_rwlock,
	fs_rwunlock, 	/* rwunlock */
	fs_nosys, 	/* seek */
	fs_cmp,
	fs_nosys, 	/* frlock */
	fs_nosys, 	/* space */
	door_realvp,	/* realvp */
	fs_nosys, 	/* getpage */
	fs_nosys, 	/* putpage */
	fs_nosys_map,
	fs_nosys_addmap,
	fs_nosys, 	/* delmap */
	fs_nosys_poll,
	fs_nosys, 	/* dump */
	fs_nosys, 	/* l_pathconf */
	fs_nosys, 	/* pageio */
	fs_nosys, 	/* dumpctl */
	fs_nodispose, 	/* dispose */
	fs_nosys, 	/* setsecattr */
	fs_nosys, 	/* getsecatt */
	fs_nosys 	/* shrlock */
};

/* ARGSUSED */
static int
door_open(struct vnode **vpp, int flag, struct cred *cr)
{
	return (0);
}

/* ARGSUSED */
static int
door_close(
	struct vnode *vp,
	int flag,
	int count,
	offset_t offset,
	struct cred *cr
)
{
	door_node_t	*dp = VTOD(vp);

	/*
	 * If this is being called from closeall on exit, any doors created
	 * by this process should have been revoked already in door_exit.
	 */
	ASSERT(dp->door_target != curproc ||
	    ((curthread->t_proc_flag & TP_LWPEXIT) == 0));

	/*
	 * Deliver an unref if needed.
	 *
	 * If the count is equal to 2, it means that I'm doing a VOP_CLOSE
	 * on the next to last reference for *this* file struct. There may
	 * be multiple files pointing to this vnode in which case the v_count
	 * will be > 1.
	 *
	 * The door_active count is bumped during each invocation.
	 */
	if (count == 2 && vp->v_count == 1 &&
	    (dp->door_flags & (DOOR_UNREF | DOOR_UNREF_MULTI))) {
		mutex_enter(&door_knob);
		if (dp->door_active == 0) {
			/* o.k. to deliver unref now */
			door_deliver_unref(dp);
		} else {
			/* do the unref later */
			dp->door_flags |= DOOR_DELAY;
		}
		mutex_exit(&door_knob);
	}
	return (0);
}

/* ARGSUSED */
static int
door_getattr(struct vnode *vp, struct vattr *vap, int flags, struct cred *cr)
{
	static timestruc_t tzero = {0, 0};
	extern dev_t doordev;

	vap->va_mask = 0;		/* bit-mask of attributes */
	vap->va_type = vp->v_type;	/* vnode type (for create) */
	vap->va_mode = 0777;		/* file access mode */
	vap->va_uid = 0;		/* owner user id */
	vap->va_gid = 0;		/* owner group id */
	vap->va_fsid = doordev;		/* file system id (dev for now) */
	vap->va_nodeid = (ino64_t)0;		/* node id */
	vap->va_nlink = vp->v_count;	/* number of references to file */
	vap->va_size = (u_offset_t)0;		/* file size in bytes */
	vap->va_atime = tzero;		/* time of last access */
	vap->va_mtime = tzero;		/* time of last modification */
	vap->va_ctime = tzero;		/* time file ``created'' */
	vap->va_rdev = doordev;		/* device the file represents */
	vap->va_blksize = 0;		/* fundamental block size */
	vap->va_nblocks = (fsblkcnt64_t)0;	/* # of blocks allocated */
	vap->va_vcode = 0;		/* version code */

	return (0);
}

/* ARGSUSED */
static void
door_inactive(struct vnode *vp, struct cred *cr)
{
	door_node_t *dp = VTOD(vp);
	/* if not revoked, remove door from per-process list */
	if (dp->door_target) {
		mutex_enter(&door_knob);
		if (dp->door_target)	/* recheck door_target under lock */
			door_list_delete(dp);
		mutex_exit(&door_knob);
	}
	kmem_free(vp, sizeof (door_node_t));
}

/* ARGSUSED */
static int
door_access(struct vnode *vp, int mode, int flags, struct cred *cr)
{
	return (0);
}

/* ARGSUSED */
static int
door_realvp(vnode_t *vp, vnode_t **vpp)
{
	*vpp = vp;
	return (0);
}
