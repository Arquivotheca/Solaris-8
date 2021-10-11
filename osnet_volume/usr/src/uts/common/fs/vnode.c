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
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vnode.c	1.51	99/07/01 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/pathname.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/mode.h>
#include <sys/conf.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <c2/audit.h>
#include <sys/acl.h>

/*
 * Convert stat(2) formats to vnode types and vice versa.  (Knows about
 * numerical order of S_IFMT and vnode types.)
 */
enum vtype iftovt_tab[] = {
	VNON, VFIFO, VCHR, VNON, VDIR, VNON, VBLK, VNON,
	VREG, VNON, VLNK, VNON, VSOCK, VNON, VNON, VNON
};

ushort_t vttoif_tab[] = {
	0, S_IFREG, S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK, S_IFIFO,
	S_IFDOOR, 0, S_IFSOCK, 0
};


static int isrofile(struct vnode *);

/*
 * Read or write a vnode.  Called from kernel code.
 */
int
vn_rdwr(
	enum uio_rw rw,
	struct vnode *vp,
	caddr_t base,
	ssize_t len,
	offset_t offset,
	enum uio_seg seg,
	int ioflag,
	rlim64_t ulimit,	/* meaningful only if rw is UIO_WRITE */
	cred_t *cr,
	ssize_t *residp)
{
	struct uio uio;
	struct iovec iov;
	int error;

	if (rw == UIO_WRITE && isrofile(vp))
		return (EROFS);

	if (len < 0)
		return (EIO);

	iov.iov_base = base;
	iov.iov_len = len;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_loffset = offset;
	uio.uio_segflg = (short)seg;
	uio.uio_resid = len;
	uio.uio_llimit = ulimit;
	VOP_RWLOCK(vp, rw == UIO_WRITE ? 1 : 0);
	if (rw == UIO_WRITE) {
		uio.uio_fmode = FWRITE;
		error = VOP_WRITE(vp, &uio, ioflag, cr);
	} else {
		uio.uio_fmode = FREAD;
		error = VOP_READ(vp, &uio, ioflag, cr);
	}
	VOP_RWUNLOCK(vp, rw == UIO_WRITE ? 1 : 0);
	if (residp)
		*residp = uio.uio_resid;
	else if (uio.uio_resid)
		error = EIO;
	return (error);
}

/*
 * Release a vnode.  Call VOP_INACTIVE on last reference or
 * decrement reference count.
 *
 * To avoid race conditions, the v_count is left at 1 for
 * the call to VOP_INACTIVE. This prevents another thread
 * from reclaiming and releasing the vnode *before* the
 * VOP_INACTIVE routine has a chance to destroy the vnode.
 * We can't have more than 1 thread calling VOP_INACTIVE
 * on a vnode.
 */
void
vn_rele(vnode_t *vp)
{
	if (vp->v_count == 0)
		cmn_err(CE_PANIC, "vn_rele: vnode ref count 0");
	mutex_enter(&vp->v_lock);
	if (vp->v_count == 1) {
		mutex_exit(&vp->v_lock);
		VOP_INACTIVE(vp, CRED());
	} else {
		vp->v_count--;
		mutex_exit(&vp->v_lock);
	}
}

/*
 * Like vn_rele() except that it clears v_stream under v_lock.
 * This is used by sockfs when it dismantels the association between
 * the sockfs node and the vnode in the underlaying file system.
 * v_lock has to be held to prevent a thread coming through the lookupname
 * path from accessing a stream head that is going away.
 */
void
vn_rele_stream(vnode_t *vp)
{
	if (vp->v_count == 0)
		cmn_err(CE_PANIC, "vn_rele: vnode ref count 0");
	mutex_enter(&vp->v_lock);
	vp->v_stream = NULL;
	if (vp->v_count == 1) {
		mutex_exit(&vp->v_lock);
		VOP_INACTIVE(vp, CRED());
	} else {
		vp->v_count--;
		mutex_exit(&vp->v_lock);
	}
}

/*
 * Open/create a vnode.
 * This may be callable by the kernel, the only known use
 * of user context being that the current user credentials
 * are used for permissions.  crwhy is defined iff filemode & FCREAT.
 */
int
vn_open(
	char *pnamep,
	enum uio_seg seg,
	int filemode,
	int createmode,
	struct vnode **vpp,
	enum create crwhy,
	mode_t umask)
{
	struct vnode *vp;
	int mode;
	int error;
	struct vattr vattr;

	mode = 0;
	if (filemode & FREAD)
		mode |= VREAD;
	if (filemode & (FWRITE|FTRUNC))
		mode |= VWRITE;

top:
	if (filemode & FCREAT) {
		enum vcexcl excl;

		/*
		 * Wish to create a file.
		 */
		vattr.va_type = VREG;
		vattr.va_mode = createmode;
		vattr.va_mask = AT_TYPE|AT_MODE;
		if (filemode & FTRUNC) {
			vattr.va_size = 0;
			vattr.va_mask |= AT_SIZE;
		}
		if (filemode & FEXCL)
			excl = EXCL;
		else
			excl = NONEXCL;

		if (error =
		    vn_create(pnamep, seg, &vattr, excl, mode, &vp, crwhy,
					(filemode & ~(FTRUNC|FEXCL)), umask))
			return (error);
	} else {
		/*
		 * Wish to open a file.  Just look it up.
		 */
		if (error = lookupname(pnamep, seg, FOLLOW, NULLVPP, &vp))
			return (error);

		/*
		 * Get the attributes to check whether file is large.
		 * We do this only if the FOFFMAX flag is not set and
		 * only for regular files.
		 */

		if (!(filemode & FOFFMAX) && (vp->v_type == VREG)) {
			vattr.va_mask = AT_SIZE;
			if ((error = VOP_GETATTR(vp, &vattr, 0, CRED()))) {
				goto out;
			}
			if (vattr.va_size > (u_offset_t)MAXOFF32_T) {
				/*
				 * Large File API - regular open fails
				 * if FOFFMAX flag is set in file mode
				 */
				error = EOVERFLOW;
				goto out;
			}
		}
		/*
		 * Can't write directories, active texts, or
		 * read-only filesystems.  Can't truncate files
		 * on which mandatory locking is in effect.
		 */
		if (filemode & (FWRITE|FTRUNC)) {
			/*
			 * Allow writable directory if VDIROPEN flag is set.
			 */
			if (vp->v_type == VDIR && !(vp->v_flag & VDIROPEN)) {
				error = EISDIR;
				goto out;
			}
			if (isrofile(vp)) {
				error = EROFS;
				goto out;
			}
			/*
			 * Can't truncate files on which mandatory locking
			 * is in effect.
			 */
			if ((filemode & FTRUNC) && vp->v_filocks != NULL) {
				vattr.va_mask = AT_MODE;
				if ((error =
				    VOP_GETATTR(vp, &vattr, 0, CRED())) == 0 &&
				    MANDLOCK(vp, vattr.va_mode))
					error = EAGAIN;
			}
			if (error)
				goto out;
		}
		/*
		 * Check permissions.
		 */
		if (error = VOP_ACCESS(vp, mode, 0, CRED()))
			goto out;
	}

	/*
	 * Opening a socket corresponding to the AF_UNIX pathname
	 * in the filesystem name space is not supported.
	 * However, VSOCK nodes in namefs are supported in order
	 * to make fattach work for sockets.
	 *
	 * XXX This uses VOP_REALVP to distinguish between
	 * an unopened namefs node (where VOP_REALVP returns a
	 * different VSOCK vnode) and a VSOCK created by vn_create
	 * in some file system (where VOP_REALVP would never return
	 * a different vnode).
	 */
	if (vp->v_type == VSOCK) {
		struct vnode *nvp;

		error = VOP_REALVP(vp, &nvp);
		if (error != 0 || nvp == NULL || nvp == vp ||
		    nvp->v_type != VSOCK) {
			error = EOPNOTSUPP;
			goto out;
		}
	}
	/*
	 * Do opening protocol.
	 */
	error = VOP_OPEN(&vp, filemode, CRED());
	/*
	 * Truncate if required.
	 */
	if (error == 0 && (filemode & FTRUNC) && !(filemode & FCREAT)) {
		vattr.va_size = 0;
		vattr.va_mask = AT_SIZE;
		if ((error = VOP_SETATTR(vp, &vattr, 0, CRED())) != 0)
			(void) VOP_CLOSE(vp, filemode, 1, (offset_t)0, CRED());
	}
out:
	ASSERT(vp->v_count > 0);
	if (error) {
		ushort_t	vflag;
		/*
		 * The following clause was added to handle a problem
		 * with NFS consistency.  It is possible that a lookup
		 * of the file to be opened succeeded, but the file
		 * itself doesn't actually exist on the server.  This
		 * is chiefly due to the DNLC containing an entry for
		 * the file which has been removed on the server.  In
		 * this case, we just start over.  If there was some
		 * other cause for the ESTALE error, then the lookup
		 * of the file will fail and the error will be returned
		 * above instead of looping around from here. We make
		 * an exception for the root of the NFS mounted filesystem
		 * as it could potentially loop forever.
		 */
		vflag = vp->v_flag;
		VN_RELE(vp);

		if ((error == ESTALE) && !(vflag & VROOT)) {
			goto top;
		}
	} else
		*vpp = vp;
	return (error);
}

/*
 * Create a vnode (makenode).
 */
int
vn_create(
	char *pnamep,
	enum uio_seg seg,
	struct vattr *vap,
	enum vcexcl excl,
	int mode,
	struct vnode **vpp,
	enum create why,
	int flag,
	mode_t umask)
{
	struct vnode *dvp;	/* ptr to parent dir vnode */
	struct pathname pn;
	int error;
	struct vattr vattr;

	ASSERT((vap->va_mask & (AT_TYPE|AT_MODE)) == (AT_TYPE|AT_MODE));

top:
	/*
	 * Lookup directory.
	 * If new object is a file, call lower level to create it.
	 * Note that it is up to the lower level to enforce exclusive
	 * creation, if the file is already there.
	 * This allows the lower level to do whatever
	 * locking or protocol that is needed to prevent races.
	 * If the new object is directory call lower level to make
	 * the new directory, with "." and "..".
	 */
	if (error = pn_get(pnamep, seg, &pn))
		return (error);
#ifdef  C2_AUDIT
	if (audit_active)
		audit_vncreate_start();
#endif /* C2_AUDIT */
	dvp = NULL;
	*vpp = NULL;
	/*
	 * lookup will find the parent directory for the vnode.
	 * When it is done the pn holds the name of the entry
	 * in the directory.
	 * If this is a non-exclusive create we also find the node itself.
	 */
	if (excl == EXCL)
		error = lookuppn(&pn, NULL, NO_FOLLOW, &dvp, NULLVPP);
	else
		error = lookuppn(&pn, NULL, FOLLOW, &dvp, vpp);
	if (error) {
		pn_free(&pn);
		if (why == CRMKDIR && error == EINVAL)
			error = EEXIST;		/* SVID */
		return (error);
	}

	if (why != CRMKNOD)
		vap->va_mode &= ~VSVTX;

	/*
	 * If default ACLs are defined for the directory don't apply the
	 * umask if umask is passed.
	 */

	if (umask) {

		vsecattr_t vsec;

		vsec.vsa_aclcnt = 0;
		vsec.vsa_aclentp = NULL;
		vsec.vsa_dfaclcnt = 0;
		vsec.vsa_dfaclentp = NULL;
		vsec.vsa_mask = VSA_DFACLCNT;
		if (error = VOP_GETSECATTR(dvp, &vsec, 0, CRED())) {
			if (*vpp != NULL)
				VN_RELE(*vpp);
			goto out;
		}

		/*
		 * Apply the umask if no default ACLs.
		 */
		if (vsec.vsa_dfaclcnt == 0)
			vap->va_mode &= ~umask;

		/*
		 * VOP_GETSECATTR() may have allocated memory for ACLs we
		 * didn't request, so double-check and free it if necessary.
		 */
		if (vsec.vsa_aclcnt && vsec.vsa_aclentp != NULL)
			kmem_free((caddr_t)vsec.vsa_aclentp,
				vsec.vsa_aclcnt * sizeof (aclent_t));
		if (vsec.vsa_dfaclcnt && vsec.vsa_dfaclentp != NULL)
			kmem_free((caddr_t)vsec.vsa_dfaclentp,
				vsec.vsa_dfaclcnt * sizeof (aclent_t));
	}

	/*
	 * In general we want to generate EROFS if the file system is
	 * readonly.  However, POSIX (IEEE Std. 1003.1) section 5.3.1
	 * documents the open system call, and it says that O_CREAT has no
	 * effect if the file system already exists.  Bug 1119649 states
	 * that open(path, O_CREAT, ...) fails when attempting to open an
	 * existing file on a read only file system.  Thus, the first part
	 * of the following if statement has 3 checks:
	 *	if the file exists &&
	 *		it is being open with write access &&
	 *		the file system is read only
	 *	then generate EROFS
	 */
	if ((*vpp != NULL && (mode & VWRITE) && isrofile(*vpp)) ||
	    (*vpp == NULL && dvp->v_vfsp->vfs_flag & VFS_RDONLY)) {
		if (*vpp)
			VN_RELE(*vpp);
		error = EROFS;
	} else if (excl == NONEXCL && *vpp != NULL) {
		struct vnode *vp = *vpp;

		/*
		 * Update the file type incase it isn't
		 * what was put into the prototype vap.
		 * In particular, this allows write
		 * access to special files on a read-only
		 * file system.
		 */
		vap->va_type = vp->v_type;

		/*
		 * File already exists.  If a mandatory lock has been
		 * applied, return EAGAIN.
		 */
		if (vp->v_filocks != NULL) {
			vattr.va_mask = AT_MODE;
			if (error = VOP_GETATTR(vp, &vattr, 0, CRED())) {
				VN_RELE(vp);
				goto out;
			}
			if (MANDLOCK(vp, vattr.va_mode)) {
				error = EAGAIN;
				VN_RELE(vp);
				goto out;
			}
		}

		/*
		 * If the file is the root of a VFS, we've crossed a
		 * mount point and the "containing" directory that we
		 * acquired above (dvp) is irrelevant because it's in
		 * a different file system.  We apply VOP_CREATE to the
		 * target itself instead of to the containing directory
		 * and supply a null path name to indicate (conventionally)
		 * the node itself as the "component" of interest.
		 *
		 * The intercession of the file system is necessary to
		 * ensure that the appropriate permission checks are
		 * done.
		 */
		if (vp->v_flag & VROOT) {
			ASSERT(why != CRMKDIR);
			error =
			    VOP_CREATE(vp, "", vap, excl, mode, vpp, CRED(),
				    flag);
			/*
			 * If the create succeeded, it will have created
			 * a new reference to the vnode.  Give up the
			 * original reference.
			 */
			VN_RELE(vp);
			goto out;
		}

		/*
		 * Large File API - non-large open (FOFFMAX flag not set)
		 * of regular file fails if the file size exceeds MAXOFF32_T.
		 */
		if (why != CRMKDIR &&
		    !(flag & FOFFMAX) &&
		    (vp->v_type == VREG)) {
			vattr.va_mask = AT_SIZE;
			if ((error = VOP_GETATTR(vp, &vattr, 0, CRED()))) {
				VN_RELE(vp);
				goto out;
			}
			if ((vattr.va_size > (u_offset_t)MAXOFF32_T)) {
				error = EOVERFLOW;
				VN_RELE(vp);
				goto out;
			}
		}

		/*
		 * We throw the vnode away to let VOP_CREATE
		 * truncate the file in a non-racy manner.
		 */
		VN_RELE(vp);
	}

	if (error == 0) {
		/*
		 * Call mkdir() if specified, otherwise create().
		 */
		if (why == CRMKDIR)
			error = VOP_MKDIR(dvp, pn.pn_path, vap, vpp, CRED());
		else
			error = VOP_CREATE(dvp, pn.pn_path, vap,
			    excl, mode, vpp, CRED(), flag);
	}
out:
#ifdef C2_AUDIT
	if (audit_active)
		audit_vncreate_finish(*vpp, error);
#endif  /* C2_AUDIT */
	pn_free(&pn);
	VN_RELE(dvp);
	/*
	 * The following clause was added to handle a problem
	 * with NFS consistency.  It is possible that a lookup
	 * of the file to be created succeeded, but the file
	 * itself doesn't actually exist on the server.  This
	 * is chiefly due to the DNLC containing an entry for
	 * the file which has been removed on the server.  In
	 * this case, we just start over.  If there was some
	 * other cause for the ESTALE error, then the lookup
	 * of the file will fail and the error will be returned
	 * above instead of looping around from here.
	 */
	if (error == ESTALE)
		goto top;
	return (error);
}

int
vn_link(char *from, char *to, enum uio_seg seg)
{
	struct vnode *fvp;		/* from vnode ptr */
	struct vnode *tdvp;		/* to directory vnode ptr */
	struct pathname pn;
	int error;
	struct vattr vattr;
	dev_t fsid;

	fvp = tdvp = NULL;
	if (error = pn_get(to, seg, &pn))
		return (error);
	if (error = lookupname(from, seg, NO_FOLLOW, NULLVPP, &fvp))
		goto out;

	if (error = lookuppn(&pn, NULL, NO_FOLLOW, &tdvp, NULLVPP))
		goto out;

	/*
	 * Make sure both source vnode and target directory vnode are
	 * in the same vfs and that it is writeable.
	 */
	vattr.va_mask = AT_FSID;
	if (error = VOP_GETATTR(fvp, &vattr, 0, CRED()))
		goto out;
	fsid = vattr.va_fsid;
	vattr.va_mask = AT_FSID;
	if (error = VOP_GETATTR(tdvp, &vattr, 0, CRED()))
		goto out;
	if (fsid != vattr.va_fsid) {
		error = EXDEV;
		goto out;
	}
	if (tdvp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		goto out;
	}
	/*
	 * Do the link.
	 */
	error = VOP_LINK(tdvp, fvp, pn.pn_path, CRED());
out:
	pn_free(&pn);
	if (fvp)
		VN_RELE(fvp);
	if (tdvp)
		VN_RELE(tdvp);
	return (error);
}

int
vn_rename(char *from, char *to, enum uio_seg seg)
{
	struct vnode *fdvp;		/* from directory vnode ptr */
	struct vnode *fvp;		/* from vnode ptr */
	struct vnode *tdvp;		/* to directory vnode ptr */
	struct pathname fpn;		/* from pathname */
	struct pathname tpn;		/* to pathname */
	int error;
	struct vattr vattr;
	dev_t fsid;

	fdvp = tdvp = fvp = NULL;
	/*
	 * Get to and from pathnames.
	 */
	if (error = pn_get(from, seg, &fpn))
		return (error);
	if (error = pn_get(to, seg, &tpn)) {
		pn_free(&fpn);
		return (error);
	}
	/*
	 * Lookup to and from directories.
	 */
	if (error = lookuppn(&fpn, NULL, NO_FOLLOW, &fdvp, &fvp))
		goto out;
	/*
	 * Make sure there is an entry.
	 */
	if (fvp == NULL) {
		error = ENOENT;
		goto out;
	}
	if (error = lookuppn(&tpn, NULL, NO_FOLLOW, &tdvp, NULLVPP))
		goto out;
	/*
	 * Make sure both the from vnode directory and the to directory
	 * are in the same vfs and the to directory is writable.
	 * We check fsid's, not vfs pointers, so loopback fs works.
	 */
	vattr.va_mask = AT_FSID;
	if (error = VOP_GETATTR(fdvp, &vattr, 0, CRED()))
		goto out;
	fsid = vattr.va_fsid;
	vattr.va_mask = AT_FSID;
	if (error = VOP_GETATTR(tdvp, &vattr, 0, CRED()))
		goto out;
	if (fsid != vattr.va_fsid) {
		error = EXDEV;
		goto out;
	}
	if (tdvp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		goto out;
	}
	/*
	 * Do the rename.
	 */
	error = VOP_RENAME(fdvp, fpn.pn_path, tdvp, tpn.pn_path, CRED());
out:
	pn_free(&fpn);
	pn_free(&tpn);
	if (fvp)
		VN_RELE(fvp);
	if (fdvp)
		VN_RELE(fdvp);
	if (tdvp)
		VN_RELE(tdvp);
	return (error);
}

/*
 * Remove a file or directory.
 */
int
vn_remove(char *fnamep, enum uio_seg seg, enum rm dirflag)
{
	struct vnode *vp;		/* entry vnode */
	struct vnode *dvp;		/* ptr to parent dir vnode */
	struct vnode *coveredvp;
	struct pathname pn;		/* name of entry */
	enum vtype vtype;
	int error;
	struct vfs *vfsp;
	struct vfs *dvfsp;	/* ptr to parent dir vfs */

	if (error = pn_get(fnamep, seg, &pn))
		return (error);
	vp = NULL;
	if (error = lookuppn(&pn, NULL, NO_FOLLOW, &dvp, &vp)) {
		pn_free(&pn);
		return (error);
	}

	/*
	 * Make sure there is an entry.
	 */
	if (vp == NULL) {
		error = ENOENT;
		goto out;
	}

	vfsp = vp->v_vfsp;
	dvfsp = dvp->v_vfsp;

	/*
	 * If the named file is the root of a mounted filesystem, fail,
	 * unless it's marked unlinkable.  In that case, unmount the
	 * filesystem and proceed with the covered vnode.
	 */
	if (vp->v_flag & VROOT) {
		if (vfsp->vfs_flag & VFS_UNLINKABLE) {
			if (dirflag == RMDIRECTORY) {
				/*
				 * User called rmdir(2) on a file that has
				 * been namefs mounted on top of.  Since
				 * namefs doesn't allow directories to
				 * be mounted on other files we know
				 * vp is not of type VDIR so fail to operation.
				 */
				error = ENOTDIR;
				goto out;
			}
			coveredvp = vfsp->vfs_vnodecovered;
			VN_HOLD(coveredvp);
			VN_RELE(vp);
			if ((error = vn_vfswlock(coveredvp)) == 0)
				if ((error = dounmount(vfsp, 0, CRED())) == 0) {
					vp = coveredvp;
					vfsp = vp->v_vfsp;
				} else {
					vp = NULL;
					VN_RELE(coveredvp);
					goto out;
				}
			else
				goto out;
		} else {
			error = EBUSY;
			goto out;
		}
	}

	/*
	 * Make sure filesystem is writeable.
	 * We check the parent directory's vfs in case this is an lofs vnode.
	 */
	if (dvfsp && dvfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		goto out;
	}

	/*
	 * Release vnode before removing.
	 */
	vtype = vp->v_type;
	VN_RELE(vp);
	vp = NULL;
	if (dirflag == RMDIRECTORY) {
		/*
		 * Caller is using rmdir(2), which can only be applied to
		 * directories.
		 */
		if (vtype != VDIR) {
			error = ENOTDIR;
		} else {
			vnode_t *cwd;
			proc_t *pp = curproc;

			mutex_enter(&pp->p_lock);
			cwd = PTOU(pp)->u_cdir;
			VN_HOLD(cwd);
			mutex_exit(&pp->p_lock);
			error = VOP_RMDIR(dvp, pn.pn_path, cwd, CRED());
			VN_RELE(cwd);
		}
	} else {
		/*
		 * Unlink(2) can be applied to anything.
		 */
		error = VOP_REMOVE(dvp, pn.pn_path, CRED());
	}

out:
	pn_free(&pn);
	if (vp != NULL)
		VN_RELE(vp);
	VN_RELE(dvp);
	return (error);
}

/*
 * vn_vfswlock_wait is used to implement a lock which is logically a writers
 * lock protecting the v_vfsmountedhere field.
 * vn_vfswlock_wait has been modified to be similar to vn_vfswlock,
 * except that it blocks to acquire the lock VVFSLOCK.
 *
 * traverse() and routines re-implementing part of traverse (e.g. autofs)
 * need to hold this lock. mount(), vn_rename(), vn_remove() and so on
 * need the non-blocking version of the writers lock i.e. vn_vfswlock
 */
int
vn_vfswlock_wait(vnode_t *vp)
{
	ASSERT(vp != NULL);

	mutex_enter(&vp->v_lock);
	while (vp->v_flag & VVFSLOCK) {
		/*
		 * Someone else is holding the write lock.
		 */
		vp->v_flag |= VVFSWAIT;
		if (!cv_wait_sig(&vp->v_cv, &vp->v_lock)) {
			mutex_exit(&vp->v_lock);
			return (EINTR);
		}
	}

	vp->v_flag |= VVFSLOCK;
	mutex_exit(&vp->v_lock);
	return (0);
}

/*
 * vn_vfswlock is used to implement a lock which is logically a writers lock
 * protecting the v_vfsmountedhere field.
 */
int
vn_vfswlock(vnode_t *vp)
{
	/*
	 * If vp is NULL then somebody is trying to lock the covered vnode
	 * of /.  (vfs_vnodecovered is NULL for /).  This situation will
	 * only happen when unmounting /.  Since that operation will fail
	 * anyway, return EBUSY here instead of in VFS_UNMOUNT.
	 */
	if (vp == NULL)
		return (EBUSY);

	mutex_enter(&vp->v_lock);
	if (vp->v_flag & VVFSLOCK) {
		mutex_exit(&vp->v_lock);
		return (EBUSY);
	}
	vp->v_flag |= VVFSLOCK;
	mutex_exit(&vp->v_lock);
	return (0);
}

/*
 * For compatibility with old (deprecated) interface, continue
 * to support vanilla mutex.
 */
int
vn_vfslock(vnode_t *vp)
{
	return (vn_vfswlock(vp));
}

void
vn_vfsunlock(vnode_t *vp)
{
	ASSERT(vp->v_flag & VVFSLOCK);
	mutex_enter(&vp->v_lock);

	/*
	 * We're holding the mutex: in the
	 * read case, we were already holding it;
	 * if the write case, we just acquired it.
	 */

	vp->v_flag &= ~VVFSLOCK;
	if (vp->v_flag & VVFSWAIT) {
		vp->v_flag &= ~VVFSWAIT;
		cv_broadcast(&vp->v_cv);
	}
	mutex_exit(&vp->v_lock);
}

int
vn_vfswlock_held(vnode_t *vp)
{
	ASSERT(vp != NULL);

	return (vp->v_flag & VVFSLOCK);
}


/*
 * Determine if this vnode is a file that is read-only
 */
static int
isrofile(vnode_t *vp)
{
	return (vp->v_type != VCHR && vp->v_type != VBLK &&
	    vp->v_type != VFIFO && (vp->v_vfsp->vfs_flag & VFS_RDONLY));
}
