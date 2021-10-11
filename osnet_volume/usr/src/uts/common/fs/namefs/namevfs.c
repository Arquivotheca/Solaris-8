/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)namevfs.c	1.54	99/04/15 SMI" /* from S5R4 1.28 */

/*
 * This file supports the vfs operations for the NAMEFS file system.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/inline.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mode.h>
#include <sys/pcb.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/fs/namenode.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/cmn_err.h>
#include <sys/modctl.h>
#include <fs/fs_subr.h>
#include <sys/bitmap.h>

/*
 * Define global data structures.
 */
dev_t	namedev;
int	namefstype;
struct	namenode *nm_filevp_hash[NM_FILEVP_HASH_SIZE];
struct	vfs namevfs;
kmutex_t ntable_lock;
int namenode_shift;

/*
 * Insert a namenode into the nm_filevp_hash table.
 *
 * Each link has a unique namenode with a unique nm_mountvp field.
 * The nm_filevp field of the namenode need not be unique, since a
 * file descriptor may be mounted to multiple nodes at the same time.
 * We hash on nm_filevp since that's what discriminates the searches
 * in namefind() and nm_unmountall().
 */
void
nameinsert(struct namenode *nodep)
{
	struct namenode **bucket;

	ASSERT(MUTEX_HELD(&ntable_lock));

	bucket = NM_FILEVP_HASH(nodep->nm_filevp);
	nodep->nm_nextp = *bucket;
	*bucket = nodep;
}

/*
 * Remove a namenode from the hash table, if present.
 */
void
nameremove(struct namenode *nodep)
{
	struct namenode *np, **npp;

	ASSERT(MUTEX_HELD(&ntable_lock));

	for (npp = NM_FILEVP_HASH(nodep->nm_filevp); (np = *npp) != NULL;
	    npp = &np->nm_nextp) {
		if (np == nodep) {
			*npp = np->nm_nextp;
			return;
		}
	}
}

/*
 * Search for a namenode that has a nm_filevp == vp and nm_mountpt == mnt.
 * If mnt is NULL, return the first link with nm_filevp of vp.
 * Returns namenode pointer on success, NULL on failure.
 */
struct namenode *
namefind(vnode_t *vp, vnode_t *mnt)
{
	struct namenode *np;

	ASSERT(MUTEX_HELD(&ntable_lock));
	for (np = *NM_FILEVP_HASH(vp); np != NULL; np = np->nm_nextp)
		if (np->nm_filevp == vp &&
		    (mnt == NULL || np->nm_mountpt == mnt))
			break;
	return (np);
}

/*
 * Force the unmouting of a file descriptor from ALL of the nodes
 * that it was mounted to.
 * At the present time, the only usage for this routine is in the
 * event one end of a pipe was mounted. At the time the unmounted
 * end gets closed down, the mounted end is forced to be unmounted.
 *
 * This routine searches the namenode hash list for all namenodes
 * that have a nm_filevp field equal to vp. Each time one is found,
 * the dounmount() routine is called. This causes the nm_unmount()
 * routine to be called and thus, the file descriptor is unmounted
 * from the node.
 *
 * At the start of this routine, the reference count for vp is
 * incremented to protect the vnode from being released in the
 * event the mount was the only thing keeping the vnode active.
 * If that is the case, the VOP_CLOSE operation is applied to
 * the vnode, prior to it being released.
 */
static int
nm_umountall(vnode_t *vp, cred_t *crp)
{
	vfs_t *vfsp;
	struct namenode *nodep;
	int error = 0;
	int realerr = 0;

	/*
	 * For each namenode that is associated with the file:
	 * If the v_vfsp field is not namevfs, dounmount it.  Otherwise,
	 * it was created in nm_open() and will be released in time.
	 * The following loop replicates some code from nm_find.  That
	 * routine can't be used as is since the list isn't strictly
	 * consumed as it is traversed.
	 */
	mutex_enter(&ntable_lock);
	nodep = *NM_FILEVP_HASH(vp);
	while (nodep) {
		if (nodep->nm_filevp == vp &&
		    (vfsp = NMTOV(nodep)->v_vfsp) != NULL && vfsp != &namevfs) {

			/*
			 * If the vn_vfslock fails, skip the vfs since
			 * somebody else may be unmounting it.
			 */
			if (vn_vfslock(vfsp->vfs_vnodecovered)) {
				realerr = EBUSY;
				nodep = nodep->nm_nextp;
				continue;
			}

			/*
			 * Can't hold ntable_lock across call to do_unmount
			 * because nm_unmount tries to acquire it.  This means
			 * there is a window where another mount of vp can
			 * happen so it is possible that after nm_unmountall
			 * there are still some mounts.  This situation existed
			 * without MT locking because dounmount can sleep
			 * so another mount could happen during that time.
			 * This situation is unlikely and doesn't really cause
			 * any problems.
			 */
			mutex_exit(&ntable_lock);
			if ((error = dounmount(vfsp, 0, crp)) != 0)
				realerr = error;
			mutex_enter(&ntable_lock);
			/*
			 * Since we dropped the ntable_lock, we
			 * have to start over from the beginning.
			 * If for some reasons dounmount() fails,
			 * start from beginning means that we will keep on
			 * trying unless another thread unmounts it for us.
			 */
			nodep = *NM_FILEVP_HASH(vp);
		} else
			nodep = nodep->nm_nextp;
	}
	mutex_exit(&ntable_lock);
	return (realerr);
}

/*
 * Force the unmouting of a file descriptor from ALL of the nodes
 * that it was mounted to.  XXX: fifo_close() calls this routine.
 *
 * nm_umountall() may return EBUSY.
 * nm_unmountall() will keep on trying until it succeeds.
 */
int
nm_unmountall(vnode_t *vp, cred_t *crp)
{
	int error;

	/*
	 * Nm_umuontall() returns only if it succeeds or
	 * return with error EBUSY.  If EBUSY, that means
	 * it cannot acquire the lock on the covered vnode,
	 * and we will keep on trying.
	 */
	for (;;) {
		error = nm_umountall(vp, crp);
		if (error != EBUSY)
			break;
		delay(1);	/* yield cpu briefly, then try again */
	}
	return (error);
}

/*
 * Mount a file descriptor onto the node in the file system.
 * Create a new vnode, update the attributes with info from the
 * file descriptor and the mount point.  The mask, mode, uid, gid,
 * atime, mtime and ctime are taken from the mountpt.  Link count is
 * set to one, the file system id is namedev and nodeid is unique
 * for each mounted object.  Other attributes are taken from mount point.
 * Make sure user is owner (or root) with write permissions on mount point.
 * Hash the new vnode and return 0.
 * Upon entry to this routine, the file descriptor is in the
 * fd field of a struct namefd.  Copy that structure from user
 * space and retrieve the file descriptor.
 */
static int
nm_mount(vfs_t *vfsp, vnode_t *mvp, struct mounta *uap, cred_t *crp)
{
	struct namefd namefdp;
	struct vnode *filevp;		/* file descriptor vnode */
	struct file *fp;
	struct vnode *newvp;		/* vnode representing this mount */
	struct namenode *nodep;		/* namenode for this mount */
	struct vattr filevattr;		/* attributes of file dec.  */
	struct vattr *vattrp;		/* attributes of this mount */
	int error = 0;

	/*
	 * Get the file descriptor from user space.
	 * Make sure the file descriptor is valid and has an
	 * associated file pointer.
	 * If so, extract the vnode from the file pointer.
	 */
	if (uap->datalen != sizeof (struct namefd))
		return (EINVAL);

	if (copyin(uap->dataptr, &namefdp, uap->datalen))
		return (EFAULT);

	if ((fp = getf(namefdp.fd)) == NULL)
		return (EBADF);

	/*
	 * If the mount point already has something mounted
	 * on it, disallow this mount.  (This restriction may
	 * be removed in a later release).
	 * Or unmount has completed but the namefs ROOT vnode
	 * count has not decremented to zero, disallow this mount.
	 */
	mutex_enter(&mvp->v_lock);
	if ((mvp->v_flag & VROOT) || (mvp->v_vfsp == &namevfs)) {
		mutex_exit(&mvp->v_lock);
		releasef(namefdp.fd);
		return (EBUSY);
	}
	mutex_exit(&mvp->v_lock);

	filevp = fp->f_vnode;
	if (filevp->v_type == VDIR) {
		releasef(namefdp.fd);
		return (EINVAL);
	}

	/*
	 * Make sure the file descriptor is not the root of some
	 * file system.
	 * If it's not, create a reference and allocate a namenode
	 * to represent this mount request.
	 */
	if (filevp->v_flag & VROOT) {
		releasef(namefdp.fd);
		return (EBUSY);
	}

	nodep = kmem_zalloc(sizeof (struct namenode), KM_SLEEP);

	mutex_init(&nodep->nm_lock, NULL, MUTEX_DEFAULT, NULL);
	vattrp = &nodep->nm_vattr;
	vattrp->va_mask = AT_ALL;
	if (error = VOP_GETATTR(mvp, vattrp, 0, crp))
		goto out;

	filevattr.va_mask = AT_ALL;
	if (error = VOP_GETATTR(filevp, &filevattr, 0, crp))
		goto out;
	/*
	 * Make sure the user is the owner of the mount point (or
	 * is the super-user) and has write permission.
	 */
	if (vattrp->va_uid != crp->cr_uid && !suser(crp)) {
		error = EPERM;
		goto out;
	}
	if (error = VOP_ACCESS(mvp, VWRITE, 0, crp))
		goto out;

	/*
	 * If the file descriptor has file/record locking, don't
	 * allow the mount to succeed.
	 */
	if (filevp->v_filocks) {
		error = EACCES;
		goto out;
	}

	/*
	 * Initialize the namenode.
	 */
	if (filevp->v_stream) {
		struct stdata *stp = filevp->v_stream;
		mutex_enter(&stp->sd_lock);
		stp->sd_flag |= STRMOUNT;
		mutex_exit(&stp->sd_lock);
	}
	nodep->nm_filevp = filevp;
	mutex_enter(&fp->f_tlock);
	fp->f_count++;
	mutex_exit(&fp->f_tlock);

	releasef(namefdp.fd);
	nodep->nm_filep = fp;
	nodep->nm_mountpt = mvp;

	/*
	 * The attributes for the mounted file descriptor were initialized
	 * above by applying VOP_GETATTR to the mount point.  Some of
	 * the fields of the attributes structure will be overwritten
	 * by the attributes from the file descriptor.
	 */
	vattrp->va_type    = filevattr.va_type;
	vattrp->va_fsid    = namedev;
	/*
	 * If the va_nodeid is > MAX_USHORT, then i386 stats might fail.
	 * So we shift down the sonode pointer to try and get the most
	 * uniqueness into 16-bits.
	 */
	ASSERT(namenode_shift > 0);
	vattrp->va_nodeid  = ((ino64_t)nodep >> namenode_shift) & 0xffff;
	vattrp->va_nlink   = 1;
	vattrp->va_size    = filevattr.va_size;
	vattrp->va_rdev    = filevattr.va_rdev;
	vattrp->va_blksize = filevattr.va_blksize;
	vattrp->va_nblocks = filevattr.va_nblocks;
	vattrp->va_vcode   = filevattr.va_vcode;

	/*
	 * Initialize new vnode structure for the mounted file descriptor.
	 */
	newvp = NMTOV(nodep);
	mutex_init(&newvp->v_lock, NULL, MUTEX_DEFAULT, NULL);

	newvp->v_flag = filevp->v_flag | VROOT | VNOMAP | VNOSWAP;
	newvp->v_count = 1;
	newvp->v_op = &nm_vnodeops;
	newvp->v_vfsp = vfsp;
	newvp->v_stream = filevp->v_stream;
	newvp->v_type = filevp->v_type;
	newvp->v_rdev = filevp->v_rdev;
	newvp->v_data = (caddr_t)nodep;

	/*
	 * Initialize the vfs structure.
	 */
	vfsp->vfs_vnodecovered = NULL;
	vfsp->vfs_flag |= VFS_UNLINKABLE;
	vfsp->vfs_bsize = 1024;
	vfsp->vfs_fstype = namefstype;
	vfs_make_fsid(&vfsp->vfs_fsid, namedev, namefstype);
	vfsp->vfs_data = (caddr_t)nodep;
	vfsp->vfs_dev = namedev;
	vfsp->vfs_bcount = 0;

	mutex_enter(&ntable_lock);
	nameinsert(nodep);
	mutex_exit(&ntable_lock);
	return (0);
out:
	releasef(namefdp.fd);
	kmem_free(nodep, sizeof (struct namenode));
	return (error);
}

/*
 * Unmount a file descriptor from a node in the file system.
 * If the user is not the owner of the file and is not super user,
 * the request is denied.
 * Otherwise, remove the namenode from the hash list.
 * If the mounted file descriptor was that of a stream and this
 * was the last mount of the stream, turn off the STRMOUNT flag.
 * If the rootvp is referenced other than through the mount,
 * nm_inactive will clean up.
 */
static int
nm_unmount(vfs_t *vfsp, int flag, cred_t *crp)
{
	struct namenode *nodep = (struct namenode *)vfsp->vfs_data;
	struct vnode *vp;
	struct file *fp = NULL;

	ASSERT((nodep->nm_flag & NMNMNT) == 0);

	/*
	 * forced unmount is not supported by this file system
	 * and thus, ENOTSUP, is being returned.
	 */
	if (flag & MS_FORCE) {
		return (ENOTSUP);
	}

	vp = nodep->nm_filevp;
	mutex_enter(&nodep->nm_lock);
	if (nodep->nm_vattr.va_uid != crp->cr_uid && !suser(crp)) {
		mutex_exit(&nodep->nm_lock);
		return (EPERM);
	}

	mutex_exit(&nodep->nm_lock);

	mutex_enter(&ntable_lock);
	nameremove(nodep);
	mutex_enter(&NMTOV(nodep)->v_lock);
	if (NMTOV(nodep)->v_count-- == 1) {
		fp = nodep->nm_filep;
		mutex_exit(&NMTOV(nodep)->v_lock);
		kmem_free(nodep, sizeof (struct namenode));
	} else {
		NMTOV(nodep)->v_flag &= ~VROOT;
		NMTOV(nodep)->v_vfsp = &namevfs;
		mutex_exit(&NMTOV(nodep)->v_lock);
	}
	if (namefind(vp, NULLVP) == NULL && vp->v_stream) {
		struct stdata *stp = vp->v_stream;
		mutex_enter(&stp->sd_lock);
		stp->sd_flag &= ~STRMOUNT;
		mutex_exit(&stp->sd_lock);
	}
	mutex_exit(&ntable_lock);
	if (fp != NULL)
		(void) closef(fp);
	return (0);
}

/*
 * Create a reference to the root of a mounted file descriptor.
 * This routine is called from lookupname() in the event a path
 * is being searched that has a mounted file descriptor in it.
 */
static int
nm_root(vfs_t *vfsp, vnode_t **vpp)
{
	struct namenode *nodep = (struct namenode *)vfsp->vfs_data;
	struct vnode *vp = NMTOV(nodep);

	VN_HOLD(vp);
	*vpp = vp;
	return (0);
}

/*
 * Return in sp the status of this file system.
 */
static int
nm_statvfs(vfs_t *vfsp, struct statvfs64 *sp)
{
	dev32_t d32;

	bzero(sp, sizeof (*sp));
	sp->f_bsize	= 1024;
	sp->f_frsize	= 1024;
	(void) cmpldev(&d32, vfsp->vfs_dev);
	sp->f_fsid = d32;
	(void) strcpy(sp->f_basetype, vfssw[vfsp->vfs_fstype].vsw_name);
	sp->f_flag	= vf_to_stf(vfsp->vfs_flag);
	return (0);
}

/*
 * Since this file system has no disk blocks of its own, apply
 * the VOP_FSYNC operation on the mounted file descriptor.
 */
static int
nm_sync(vfs_t *vfsp, short flag, cred_t *crp)
{
	struct namenode *nodep;

	if (vfsp == NULL)
		return (0);

	nodep = (struct namenode *)vfsp->vfs_data;
	if (flag & SYNC_CLOSE)
		return (nm_umountall(nodep->nm_filevp, crp));

	return (VOP_FSYNC(nodep->nm_filevp, FSYNC, crp));
}

/*
 * Define the vfs operations vector.
 */
struct vfsops nmvfsops = {
	nm_mount,
	nm_unmount,
	nm_root,
	nm_statvfs,
	nm_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys,	/* swapvp */
	fs_freevfs
};

struct vfsops dummyvfsops = {
	fs_nosys,	/* mount */
	fs_nosys,	/* unmount */
	fs_nosys,	/* root */
	nm_statvfs,
	nm_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys,	/* swapvp */
	fs_freevfs
};

/*
 * File system initialization routine. Save the file system type,
 * establish a file system device number and initialize nm_filevp_hash[].
 */
int
nameinit(struct vfssw *vswp, int fstype)
{
	int dev;

	namefstype = fstype;
	vswp->vsw_vfsops = &nmvfsops;
	if ((dev = getudev()) == (major_t)-1) {
		cmn_err(CE_WARN, "nameinit: can't get unique device");
		dev = 0;
	}
	mutex_init(&ntable_lock, NULL, MUTEX_DEFAULT, NULL);
	namedev = makedevice(dev, 0);
	bzero(nm_filevp_hash, sizeof (nm_filevp_hash));
	namevfs.vfs_op = &dummyvfsops;
	namevfs.vfs_vnodecovered = NULL;
	namevfs.vfs_bsize = 1024;
	namevfs.vfs_fstype = namefstype;
	vfs_make_fsid(&namevfs.vfs_fsid, namedev, namefstype);
	namevfs.vfs_dev = namedev;
	return (0);
}

static struct vfssw vfw = {
	"namefs",
	nameinit,
	&nmvfsops,
	0
};

/*
 * Module linkage information for the kernel.
 */
static struct modlfs modlfs = {
	&mod_fsops, "filesystem for namefs", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};

int
_init(void)
{
	/*
	 * Calculate the amount of bitshift to a namenode pointer which will
	 * still keep it unique.  See nm_mount() and nm_open().
	 */
	namenode_shift = highbit(sizeof (struct namenode));

	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
