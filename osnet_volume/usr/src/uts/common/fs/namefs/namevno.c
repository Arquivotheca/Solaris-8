/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)namevno.c	1.58	98/05/14 SMI"

/*
 * This file defines the vnode operations for mounted file descriptors.
 * The routines in this file act as a layer between the NAMEFS file
 * system and SPECFS/FIFOFS.  With the exception of nm_open(), nm_setattr(),
 * nm_getattr() and nm_access(), the routines simply apply the VOP operation
 * to the vnode representing the file descriptor.  This switches control
 * to the underlying file system to which the file descriptor belongs.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pcb.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <vm/seg.h>
#include <sys/fs/namenode.h>
#include <sys/stream.h>
#include <fs/fs_subr.h>
#include <sys/bitmap.h>

/*
 * Create a reference to the vnode representing the file descriptor.
 * Then, apply the VOP_OPEN operation to that vnode.
 *
 * The vnode for the file descriptor may be switched under you.
 * If it is, search the hash list for an nodep - nodep->nm_filevp
 * pair. If it exists, return that nodep to the user.
 * If it does not exist, create a new namenode to attach
 * to the nodep->nm_filevp then place the pair on the hash list.
 *
 * Newly created objects are like children/nodes in the mounted
 * file system, with the parent being the initial mount.
 */
int
nm_open(vnode_t **vpp, int flag, cred_t *crp)
{
	struct namenode *nodep = VTONM(*vpp);
	int error = 0;
	struct namenode *newnamep;
	struct vnode *newvp;
	struct vnode *infilevp;
	struct vnode *outfilevp;
	extern int namenode_shift;

	/*
	 * If the vnode is switched under us, the corresponding
	 * VN_RELE for this VN_HOLD will be done by the file system
	 * performing the switch. Otherwise, the corresponding
	 * VN_RELE will be done by nm_close().
	 */
	infilevp = outfilevp = nodep->nm_filevp;
	VN_HOLD(outfilevp);

	if ((error = VOP_OPEN(&outfilevp, flag, crp)) != 0) {
		VN_RELE(outfilevp);
		return (error);
	}
	if (infilevp != outfilevp) {
		ASSERT(outfilevp->v_flag != 0);
		/*
		 * See if the new filevp (outfilevp) is already associated
		 * with the mount point. If it is, then it already has a
		 * namenode associated with it.
		 */
		mutex_enter(&ntable_lock);
		if ((newnamep =
		    namefind(outfilevp, nodep->nm_mountpt)) != NULL) {
			struct vnode *vp = NMTOV(newnamep);

			VN_HOLD(vp);
			goto gotit;
		}

		newnamep = kmem_zalloc(sizeof (struct namenode), KM_SLEEP);
		newvp = NMTOV(newnamep);

		mutex_init(&newnamep->nm_lock, NULL, MUTEX_DEFAULT, NULL);
		mutex_init(&newvp->v_lock, NULL, MUTEX_DEFAULT, NULL);
		cv_init(&newvp->v_cv, NULL, CV_DEFAULT, NULL);

		mutex_enter(&nodep->nm_lock);
		newvp->v_flag = ((*vpp)->v_flag | VNOMAP | VNOSWAP) & ~VROOT;
		newvp->v_op = (*vpp)->v_op;
		newvp->v_vfsp = &namevfs;
		newvp->v_stream = outfilevp->v_stream;
		newvp->v_type = outfilevp->v_type;
		newvp->v_rdev = outfilevp->v_rdev;
		newvp->v_data = (caddr_t)newnamep;
		bcopy(&nodep->nm_vattr, &newnamep->nm_vattr, sizeof (vattr_t));
		newnamep->nm_vattr.va_type = outfilevp->v_type;
		/*
		 * If the va_nodeid is > MAX_USHORT, then i386 stats
		 * might fail.  So we shift down the sonode pointer to
		 * try and get the most uniqueness into 16-bits.
		 */
		ASSERT(namenode_shift > 0);
		newnamep->nm_vattr.va_nodeid =
		    ((ino64_t)newnamep >> namenode_shift) & 0xffff;
		newnamep->nm_vattr.va_size = (u_offset_t)0;
		newnamep->nm_vattr.va_rdev = outfilevp->v_rdev;
		newnamep->nm_flag = NMNMNT;
		newnamep->nm_filevp = outfilevp;
		newnamep->nm_filep = nodep->nm_filep;
		newnamep->nm_mountpt = nodep->nm_mountpt;
		mutex_exit(&nodep->nm_lock);

		/*
		 * Insert the new namenode into the hash list.
		 */
		VN_HOLD(newvp);
		nameinsert(newnamep);
gotit:
		mutex_exit(&ntable_lock);
		/*
		 * Release the above reference to the infilevp, the reference
		 * to the NAMEFS vnode, create a reference to the new vnode
		 * and return the new vnode to the user.
		 */
		VN_RELE(*vpp);
		*vpp = NMTOV(newnamep);
	}
	return (0);
}

/*
 * Close a mounted file descriptor.
 * Remove any locks and apply the VOP_CLOSE operation to the vnode for
 * the file descriptor.
 */
static int
nm_close(vnode_t *vp, int flag, int count, offset_t offset, cred_t *crp)
{
	struct namenode *nodep = VTONM(vp);
	int error = 0;

	(void) cleanlocks(vp, ttoproc(curthread)->p_pid, 0);
	cleanshares(vp, ttoproc(curthread)->p_pid);
	error = VOP_CLOSE(nodep->nm_filevp, flag, count, offset, crp);
	if (count == 1) {
		(void) VOP_FSYNC(nodep->nm_filevp, FSYNC, crp);
		VN_RELE(nodep->nm_filevp);
	}
	return (error);
}

static int
nm_read(vnode_t *vp, struct uio *uiop, int ioflag, cred_t *crp)
{
	return (VOP_READ(VTONM(vp)->nm_filevp, uiop, ioflag, crp));
}

static int
nm_write(vnode_t *vp, struct uio *uiop, int ioflag, cred_t *crp)
{
	return (VOP_WRITE(VTONM(vp)->nm_filevp, uiop, ioflag, crp));
}

static int
nm_ioctl(vnode_t *vp, int cmd, intptr_t arg, int mode, cred_t *cr, int *rvalp)
{
	return (VOP_IOCTL(VTONM(vp)->nm_filevp, cmd, arg, mode, cr, rvalp));
}

/*
 * Return in vap the attributes that are stored in the namenode
 * structure.  Only the size is taken from the mounted object.
 */
/* ARGSUSED */
static int
nm_getattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *crp)
{
	struct namenode *nodep = VTONM(vp);
	struct vattr va;
	int error;

	mutex_enter(&nodep->nm_lock);
	bcopy(&nodep->nm_vattr, vap, sizeof (vattr_t));
	mutex_exit(&nodep->nm_lock);

	if ((va.va_mask = vap->va_mask & AT_SIZE) != 0) {
		if (error = VOP_GETATTR(nodep->nm_filevp, &va, flags, crp))
			return (error);
		vap->va_size = va.va_size;
	}

	return (0);
}

/*
 * Set the attributes of the namenode from the attributes in vap.
 */
/* ARGSUSED */
static int
nm_setattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *crp)
{
	struct namenode *nodep = VTONM(vp);
	struct vattr *nmvap = &nodep->nm_vattr;
	long mask = vap->va_mask;
	int error = 0;

	/*
	 * Cannot set these attributes.
	 */
	if (mask & (AT_NOSET|AT_SIZE))
		return (EINVAL);

	VOP_RWLOCK(nodep->nm_filevp, 1);
	mutex_enter(&nodep->nm_lock);

	/*
	 * Change ownership/group/time/access mode of mounted file
	 * descriptor.  Must be owner or super user.
	 */
	if (crp->cr_uid != nmvap->va_uid && !suser(crp)) {
		error = EPERM;
		goto out;
	}
	/*
	 * If request to change mode, copy new
	 * mode into existing attribute structure.
	 */
	if (mask & AT_MODE) {
		nmvap->va_mode = vap->va_mode & ~VSVTX;
		if (crp->cr_uid != 0 && !groupmember(nmvap->va_gid, crp))
			nmvap->va_mode &= ~VSGID;
	}
	/*
	 * If request was to change user or group, turn off suid and sgid
	 * bits.
	 * If the system was configured with the "rstchown" option, the
	 * owner is not permitted to give away the file, and can change
	 * the group id only to a group of which he or she is a member.
	 */
	if (mask & (AT_UID|AT_GID)) {
		int checksu = 0;

		if (rstchown) {
			if (((mask & AT_UID) && vap->va_uid != nmvap->va_uid) ||
			    ((mask & AT_GID) && !groupmember(vap->va_gid, crp)))
				checksu = 1;
		} else if (crp->cr_uid != nmvap->va_uid)
			checksu = 1;

		if (checksu && !suser(crp)) {
			error = EPERM;
			goto out;
		}
		if (crp->cr_uid != 0)
			nmvap->va_mode &= ~(VSUID|VSGID);
		if (mask & AT_UID)
			nmvap->va_uid = vap->va_uid;
		if (mask & AT_GID)
			nmvap->va_gid = vap->va_gid;
	}
	/*
	 * If request is to modify times, make sure user has write
	 * permissions on the file.
	 */
	if (mask & (AT_ATIME|AT_MTIME)) {
		if (crp->cr_uid != 0 && !(nmvap->va_mode & VWRITE)) {
			error = EACCES;
			goto out;
		}
		if (mask & AT_ATIME)
			nmvap->va_atime = vap->va_atime;
		if (mask & AT_MTIME) {
			nmvap->va_mtime = vap->va_mtime;
			nmvap->va_ctime = hrestime;
		}
	}
out:
	mutex_exit(&nodep->nm_lock);
	VOP_RWUNLOCK(nodep->nm_filevp, 1);
	return (error);
}

/*
 * Check mode permission on the namenode.  The mode is shifted to select
 * the owner/group/other fields.  The super user is granted all permissions
 * on the namenode.  In addition an access check is performed on the
 * mounted file.
 */
/* ARGSUSED */
static int
nm_access(vnode_t *vp, int mode, int flags, cred_t *crp)
{
	struct namenode *nodep = VTONM(vp);
	int error, omode = mode;

	if (crp->cr_uid != 0) {
		mutex_enter(&nodep->nm_lock);
		if (crp->cr_uid != nodep->nm_vattr.va_uid) {
			mode >>= 3;
			if (!groupmember(nodep->nm_vattr.va_gid, crp))
				mode >>= 3;
		}
		if ((nodep->nm_vattr.va_mode & mode) != mode) {
			mutex_exit(&nodep->nm_lock);
			return (EACCES);
		}
		mutex_exit(&nodep->nm_lock);
	}
	if (error = VOP_ACCESS(nodep->nm_filevp, omode, flags, crp))
		return (error);
	return (0);
}

/*
 * We can get here if a creat or open with O_CREAT is done on a namefs
 * mount point, for example, as the object of a shell output redirection to
 * the mount point.
 */
/*ARGSUSED*/
static int
nm_create(vnode_t *dvp, char *name, vattr_t *vap, enum vcexcl excl,
	int mode, vnode_t **vpp, cred_t *cr, int flag)
{
	int error;

	ASSERT(dvp && *name == '\0');
	if (excl == NONEXCL) {
		if (mode && (error = nm_access(dvp, mode, 0, cr)) != 0)
			return (error);
		VN_HOLD(dvp);
		return (0);
	}
	return (EEXIST);
}

/*
 * Links are not allowed on mounted file descriptors.
 */
/*ARGSUSED*/
static int
nm_link(vnode_t *tdvp, vnode_t *vp, char *tnm, cred_t *crp)
{
	return (EXDEV);
}

static int
nm_fsync(vnode_t *vp, int syncflag, cred_t *crp)
{
	return (VOP_FSYNC(VTONM(vp)->nm_filevp, syncflag, crp));
}

/*
 * Inactivate a vnode/namenode by...
 * clearing its unique node id, removing it from the hash list
 * and freeing the memory allocated for it.
 */
/* ARGSUSED */
static void
nm_inactive(vnode_t *vp, cred_t *crp)
{
	struct file *fp = NULL;
	struct namenode *nodep = VTONM(vp);

	mutex_enter(&ntable_lock);
	mutex_enter(&vp->v_lock);
	ASSERT(vp->v_count >= 1);
	if (--vp->v_count != 0) {
		mutex_exit(&vp->v_lock);
		mutex_exit(&ntable_lock);
		return;
	}
	mutex_exit(&vp->v_lock);
	if (!(nodep->nm_flag & NMNMNT)) {
		ASSERT(nodep->nm_filep->f_vnode == nodep->nm_filevp);
		fp = nodep->nm_filep;
	}
	nameremove(nodep);
	mutex_exit(&ntable_lock);
	if (fp != NULL)
		(void) closef(fp);
	kmem_free(nodep, sizeof (struct namenode));
}

static int
nm_fid(vnode_t *vp, struct fid *fidnodep)
{
	return (VOP_FID(VTONM(vp)->nm_filevp, fidnodep));
}

static void
nm_rwlock(vnode_t *vp, int write)
{
	VOP_RWLOCK(VTONM(vp)->nm_filevp, write);
}

static void
nm_rwunlock(vnode_t *vp, int write)
{
	VOP_RWUNLOCK(VTONM(vp)->nm_filevp, write);
}

static int
nm_seek(vnode_t *vp, offset_t ooff, offset_t *noffp)
{
	return (VOP_SEEK(VTONM(vp)->nm_filevp, ooff, noffp));
}

/*
 * Return the vnode representing the file descriptor in vpp.
 */
static int
nm_realvp(vnode_t *vp, vnode_t **vpp)
{
	struct vnode *rvp;

	vp = VTONM(vp)->nm_filevp;
	if (VOP_REALVP(vp, &rvp) == 0)
		vp = rvp;
	*vpp = vp;
	return (0);
}

static int
nm_poll(vnode_t *vp, short events, int anyyet, short *reventsp,
	pollhead_t **phpp)
{
	return (VOP_POLL(VTONM(vp)->nm_filevp, events, anyyet, reventsp, phpp));
}

struct vnodeops nm_vnodeops = {
	nm_open,
	nm_close,
	nm_read,
	nm_write,
	nm_ioctl,
	fs_setfl,
	nm_getattr,
	nm_setattr,
	nm_access,
	fs_nosys,	/* lookup */
	nm_create,
	fs_nosys,	/* remove */
	nm_link,
	fs_nosys,	/* rename */
	fs_nosys,	/* mkdir */
	fs_nosys,	/* rmdir */
	fs_nosys,	/* readdir */
	fs_nosys,	/* symlink */
	fs_nosys,	/* readlink */
	nm_fsync,
	nm_inactive,
	nm_fid,
	nm_rwlock,
	nm_rwunlock,
	nm_seek,
	fs_cmp,
	fs_frlock,
	fs_nosys,	/* space */
	nm_realvp,
	fs_nosys,	/* getpages */
	fs_nosys,	/* putpages */
	fs_nosys_map,	/* map */
	fs_nosys_addmap,	/* addmap */
	fs_nosys,	/* delmap */
	nm_poll,
	fs_nosys,	/* dump */
	fs_pathconf,
	fs_nosys,	/* pageio */
	fs_nosys,	/* dumpctl */
	fs_nodispose,
	fs_nosys,	/* setsecattr */
	fs_fab_acl,	/* getsecattr */
	fs_shrlock	/* shrlock */
};
