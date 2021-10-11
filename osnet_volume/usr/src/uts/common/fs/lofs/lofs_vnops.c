/*
 * Copyright (c) 1987-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lofs_vnops.c	1.46	99/05/24 SMI"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#include <sys/fs/lofs_node.h>
#include <sys/fs/lofs_info.h>
#include <fs/fs_subr.h>
#include <vm/as.h>
#include <vm/seg.h>


/*
 * These are the vnode ops routines which implement the vnode interface to
 * the looped-back file system.  These routines just take their parameters,
 * and then calling the appropriate real vnode routine(s) to do the work.
 */

static int
lo_open(vnode_t **vpp, int flag, struct cred *cr)
{
	vnode_t *vp = *vpp;
	vnode_t *rvp;
	vnode_t *oldvp;
	int error;

#ifdef LODEBUG
	lo_dprint(4, "lo_open vp %p cnt=%d realvp %p cnt=%d\n",
		vp, vp->v_count, realvp(vp), realvp(vp)->v_count);
#endif
	oldvp = vp;
	vp = rvp = realvp(vp);
	/*
	 * Need to hold new reference to vp since VOP_OPEN() may
	 * decide to release it.
	 */
	VN_HOLD(vp);
	error = VOP_OPEN(&rvp, flag, cr);

	if (!error && rvp != vp) {
		/*
		 * the FS which we called should have released the
		 * new reference on vp
		 */
		*vpp = rvp;
		VN_RELE(oldvp);
	} else {
		ASSERT(vp->v_count > 1);
		VN_RELE(vp);
	}

	return (error);
}

static int
lo_close(
	vnode_t *vp,
	int flag,
	int count,
	offset_t offset,
	struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_close vp %p realvp %p\n", vp, realvp(vp));
#endif
	vp = realvp(vp);
	return (VOP_CLOSE(vp, flag, count, offset, cr));
}

static int
lo_read(vnode_t *vp, struct uio *uiop, int ioflag, struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_read vp %p realvp %p\n", vp, realvp(vp));
#endif
	vp = realvp(vp);
	return (VOP_READ(vp, uiop, ioflag, cr));
}

static int
lo_write(vnode_t *vp, struct uio *uiop, int ioflag, struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_write vp %p realvp %p\n", vp, realvp(vp));
#endif
	vp = realvp(vp);
	return (VOP_WRITE(vp, uiop, ioflag, cr));
}

static int
lo_ioctl(
	vnode_t *vp,
	int cmd,
	intptr_t arg,
	int flag,
	struct cred *cr,
	int *rvalp)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_ioctl vp %p realvp %p\n", vp, realvp(vp));
#endif
	vp = realvp(vp);
	return (VOP_IOCTL(vp, cmd, arg, flag, cr, rvalp));
}

static int
lo_setfl(vnode_t *vp, int oflags, int nflags, cred_t *cr)
{
	vp = realvp(vp);
	return (VOP_SETFL(vp, oflags, nflags, cr));
}

static int
lo_getattr(
	vnode_t *vp,
	struct vattr *vap,
	int flags,
	struct cred *cr)
{
	vnode_t *xvp;
	int error;
	struct loinfo *lop;

#ifdef LODEBUG
	lo_dprint(4, "lo_getattr vp %p realvp %p\n", vp, realvp(vp));
#endif
	/*
	 * If we are at the root of a mounted lofs filesystem
	 * and the underlying mount point is within the same
	 * filesystem, then return the attributes of the
	 * underlying mount point rather than the attributes
	 * of the mounted directory.  This prevents /bin/pwd
	 * and the C library function getcwd() from getting
	 * confused and returning failures.
	 */
	lop = (struct loinfo *)(vp->v_vfsp->vfs_data);
	if ((vp->v_flag & VROOT) &&
	    (xvp = vp->v_vfsp->vfs_vnodecovered) != NULL &&
	    vp->v_vfsp->vfs_dev == xvp->v_vfsp->vfs_dev)
		vp = xvp;
	else
		vp = realvp(vp);
	if (!(error = VOP_GETATTR(vp, vap, flags, cr))) {
		/*
		 * report lofs rdev instead of real vp's
		 */
		vap->va_rdev = lop->li_rdev;
	}
	return (error);
}

static int
lo_setattr(vnode_t *vp, struct vattr *vap, int flags, struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_setattr vp %p realvp %p\n", vp, realvp(vp));
#endif
	vp = realvp(vp);
	return (VOP_SETATTR(vp, vap, flags, cr));
}

static int
lo_access(vnode_t *vp, int mode, int flags, struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_access vp %p realvp %p\n", vp, realvp(vp));
#endif
	vp = realvp(vp);
	return (VOP_ACCESS(vp, mode, flags, cr));
}

static int
lo_fsync(vnode_t *vp, int syncflag, struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_fsync vp %p realvp %p\n", vp, realvp(vp));
#endif
	vp = realvp(vp);
	return (VOP_FSYNC(vp, syncflag, cr));
}

/*ARGSUSED*/
static void
lo_inactive(vnode_t *vp, struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_inactive %p, realvp %p\n", vp, realvp(vp));
#endif
	freelonode(vtol(vp));
}

/* ARGSUSED */
static int
lo_fid(vnode_t *vp, struct fid *fidp)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_fid %p, realvp %p\n", vp, realvp(vp));
#endif
	vp = realvp(vp);
	return (VOP_FID(vp, fidp));
}


/*
 * Given a vnode of lofs type, lookup nm name and
 * return a shadow vnode (of lofs type) of the
 * real vnode found if the real vnode found is a
 * directory or the real vnode found itself otherwise.
 *
 * Due to the nature of lofs, there is a potential
 * looping in path traversal.
 *
 * starting from the mount point of an lofs;
 * a loop is defined to be a traversal path
 * where the mount point or the real vnode of
 * the root of this lofs is encountered twice.
 * Once at the start of traversal and second
 * when the looping is found.
 *
 * When a loop is encountered, a shadow of the
 * covered vnode is returned to stop the looping.
 *
 * This normally works, but with the advent of
 * the new automounter, returning the shadow of the
 * covered vnode (autonode, in this case) does not
 * stop the loop.  Because further lookup on this
 * lonode will cause the autonode to call lo_lookup()
 * on the lonode covering it.
 *
 * example "/net/jurassic/net/jurassic" is a loop.
 * returning the shadow of the autonode corresponding to
 * "/net/jurassic/net/jurassic" will not terminate the
 * loop.   To solve this problem we allow the loop to go
 * through one more level component lookup.  If it hit
 * "net" after the loop as in "/net/jurassic/net/jurassic/net",
 * then returning the vnode covered by the autonode "net"
 * will terminate the loop.
 *
 * A new field in lonode structure is added to denote
 * that we're crossing to another lofs.
 *
 * With this field we can detect the looping in a path
 * such as "/net/jurassic/net/localhost/net/localhost"
 * because we keep track that we cross to another lofs
 * mount point when we hit the first "localhost", and if
 * we see the node for "localhost" again, we know we have
 * a loop.
 *
 * Lookup for dot dot has to be dealt with separately.
 * It will be nice to have a "one size fits all" kind
 * of solution, so that we don't have so many ifs statement
 * in the lo_lookup() to handle dotdot.  But, since
 * there are so many special cases to handle different
 * kinds looping above, we need special codes to handle
 * dotdot lookup as well.
 */
static int
lo_lookup(
	vnode_t *dvp,
	char *nm,
	vnode_t **vpp,
	struct pathname *pnp,
	int flags,
	vnode_t *rdir,
	struct cred *cr)
{
	vnode_t *vp = NULL, *tvp;
	int error;
	vnode_t *realdvp = realvp(dvp);
	vnode_t *crossed_vp;
	struct loinfo *li = vtoli(dvp->v_vfsp);
	int	looping = 0;
	int	crossedvp = 0;
	int	doingdotdot = 0;

	if (nm[0] == '.') {
		if (nm [1] == '\0') {
			*vpp = dvp;
			VN_HOLD(dvp);
			return (0);
		} else if ((nm[1] == '.') && (nm[2] == 0)) {
			doingdotdot++;
			/*
			 * Handle ".." out of mounted filesystem
			 */
			while ((realdvp->v_flag & VROOT) &&
			    realdvp != rootdir) {
				realdvp = realdvp->v_vfsp->vfs_vnodecovered;
				ASSERT(realdvp != NULL);
			}
		}
	}

	/*
	 * traverse all of the intermediate shadow vnodes to get
	 * to the real vnode
	 */
	if (!doingdotdot)
		while (realdvp->v_vfsp->vfs_op == &lo_vfsops)
			realdvp = realvp(realdvp);

	*vpp = NULL;	/* default(error) case */

	/*
	 * Do the normal lookup
	 */
	if (error = VOP_LOOKUP(realdvp, nm, &vp, pnp, flags, rdir, cr))
		goto out;

	if (doingdotdot) {
		vnode_t	*rvp;

		/*
		 * get the closest lofs mount point;
		 * root vp if has not crossed mount point
		 * lo_crossedvp if it is set
		 */
		tvp = (vtol(dvp))->lo_crossedvp ?
		    (vtol(dvp))->lo_crossedvp : li->li_rootvp;
		/*
		 * get the real vp of the closest lofs mount point
		 */
		rvp = realvp(tvp);
		while (rvp->v_vfsp->vfs_op == &lo_vfsops) {
			/*
			 * skip all of intermediate shadow lonodes
			 */
			rvp = realvp(rvp);
		}

		if ((vtol(dvp))->lo_looping) {

			/*
			 * if looping get the actual found vnode
			 * instead of the vnode covered
			 */
			error = vn_vfswlock_wait(realdvp);
			if (error)
				goto out;

			if (realdvp->v_vfsmountedhere != NULL) {

				vfs_lock_wait(realdvp->v_vfsmountedhere);
				vn_vfsunlock(realdvp);

				error = VFS_ROOT(realdvp->v_vfsmountedhere,
						&tvp);
				vfs_unlock(realdvp->v_vfsmountedhere);
				if (error)
					goto out;

				if ((tvp == li->li_rootvp ||
					tvp == (vtol(dvp))->lo_crossedvp) &&
					(vp == realvp(tvp) || vp == rvp)) {
					/*
					 * we're either back at the real vnode
					 * of the rootvp or crossedvp
					 *
					 * return the rootvp if it has not
					 * crossed lofs mount point
					 * other wise make a shadow of the
					 * root vp of the crossed lofs
					 */
					if (tvp == (vtol(dvp))->lo_crossedvp) {
						*vpp = makelonode(tvp, li);
						(vtol(*vpp))->lo_crossedvp =
								tvp;
					} else {
						*vpp = tvp;
					}
					VN_RELE(vp);
					return (0);
#ifdef notyet
				} else if (vp == rvp ||
					vp == realvp(li->li_rootvp)) {
#else
				} else if (vp == rvp) {
#endif
					/*
					 * looping because parent vnode was
					 * looping.
					 *
					 * in general when a vnode is marked
					 * looping it should yield a vnode
					 * representing a directory prior to
					 * the lofs mount; which will stop
					 * the looping.
					 *
					 * But when an autofs is mixed with
					 * lofs /net/hostname, we have to allow
					 * one more looping level:
					 *
					 * "/net/localhost/net/localhost"  is
					 * looping but access to the vnode
					 * covered by the lofs node (in this
					 * case autonode) will just go back to
					 * the lonode mounted on it, and hence
					 * loop.  But we stop further looping
					 * if "net" is accessed again by
					 * returning the vnode covered by the
					 * lofs node.
					 */
					VN_RELE(vp);
					vp = (vtol(dvp))->lo_crossedvp ?
						(vtol(dvp))->lo_crossedvp :
						li->li_rootvp;
					vp = (vp)->v_vfsp->vfs_vnodecovered;
					VN_HOLD(vp);
					VN_RELE(tvp);
					*vpp = makelonode(vp, li);
					(vtol(*vpp))->lo_looping = 1;
					(vtol(*vpp))->lo_crossedvp =
					(vtol(dvp))->lo_crossedvp;
					return (0);
				}
				VN_RELE(tvp);
			} else
				vn_vfsunlock(realdvp);
		} else if (vp == rvp) {
			/*
			 * we have not crossed vp and dot dot
			 * points back to real vnode of either root of
			 * crossed lofs or root of the current lofs
			 */
			if ((vtol(dvp))->lo_crossedvp)
				tvp = li->li_rootvp;
			VN_HOLD(tvp);
			VN_RELE(vp);
			*vpp = tvp;
			return (0);
		}
	}

	/*
	 * If this vnode is mounted on, then we
	 * traverse to the vnode which is the root of
	 * the mounted file system.
	 */
	if (error = traverse(&vp))
		goto out;

	/*
	 * We only make lonode for the real vnode when
	 * real vnode is a directory.
	 *
	 * We can't do it on shared text without hacking on distpte
	 */
	if (vp->v_type != VDIR) {
		*vpp = vp;
		goto out;
	}

	/*
	 * if the found vnode (vp) is not of type lofs
	 * then we're just going to make a shadow of that
	 * vp and get out.
	 *
	 * If the found vnode (vp) is of lofs type, and
	 * we're not doing dotdot, we have to check if
	 * we're looping or crossing lofs mount point
	 */
	if (!doingdotdot && vp->v_vfsp->vfs_op == &lo_vfsops) {

#ifdef notyet
		if ((vtol(vp))->lo_crossedvp &&
			(vtol(vp))->lo_crossedvp == li->li_rootvp) {
			looping++;
			tvp = vp;
			vp = realvp(vp);
			VN_HOLD(vp);
			VN_RELE(tvp);
			goto get_covered_vnode;
		}
#endif

		/*
		 * if the parent vp has the lo_looping set
		 * then the child will have lo_looping as
		 * well and we're going to return the
		 * shadow of the vnode covered by vp
		 *
		 * Otherwise check if we're looping, i.e.
		 * vp equals the root vp of the lofs or
		 * vp equals the root vp of the lofs we have
		 * crossed.
		 */
		if (!(vtol(dvp))->lo_looping) {
			if (vp == li->li_rootvp ||
			    vp == (vtol(dvp))->lo_crossedvp) {
				looping++;
				goto get_covered_vnode;
			} else if ((vtoli(vp->v_vfsp))->li_rootvp !=
			    li->li_rootvp) {
				/*
				 * just cross another lofs mount point.
				 * remember the root vnode of the new lofs
				 */
				crossedvp++;
				crossed_vp = (vtoli(vp->v_vfsp))->li_rootvp;
				if (vp == realvp(li->li_rootvp) ||
				    vp == li->li_rootvp) {
					looping++;
					goto get_covered_vnode;
				}
			}
		} else {
			/*
			 * come here only because of the interaction between
			 * the autofs and lofs.
			 *
			 * Lookup of "/net/X/net/X" will return a shadow of
			 * an autonode X_a which we call X_l.
			 *
			 * Lookup of anything under X_l, will trigger a call to
			 * auto_lookup(X_a,nm) which will eventually call
			 * lo_lookup(X_lr,nm) where X_lr is the root vnode of
			 * the current lofs.
			 *
			 * We come here only when we are called with X_l as dvp
			 * and look for something underneath.
			 *
			 * We need to find out if the vnode, which vp is
			 * shadowing, is the rootvp of the autofs.
			 *
			 */
			error = VFS_ROOT((realvp(dvp))->v_vfsp, &tvp);
			if (error)
				goto out;
			/*
			 * tvp now contains the rootvp of the vfs of the
			 * real vnode of dvp
			 */
			if ((realvp(dvp))->v_vfsp == (realvp(vp))->v_vfsp &&
				tvp == realvp(vp)) {

				/*
				 * vp is the shadow of "net",
				 * the rootvp of autofs
				 */
				VN_RELE(vp);
				vp = tvp;	/* this is an autonode */
				/*
				 * Need to find the covered vnode
				 */
get_covered_vnode:
				tvp = vp;
				vp = vp->v_vfsp->vfs_vnodecovered;
				ASSERT(vp);
				VN_HOLD(vp);
				VN_RELE(tvp);
			} else
				VN_RELE(tvp);
		}
	}
	*vpp = makelonode(vp, li);

	if (looping || ((vtol(dvp))->lo_looping && !doingdotdot))
		(vtol(*vpp))->lo_looping = 1;

	if (crossedvp)
		(vtol(*vpp))->lo_crossedvp = crossed_vp;

	if ((vtol(dvp))->lo_crossedvp)
		(vtol(*vpp))->lo_crossedvp = (vtol(dvp))->lo_crossedvp;

out:
#ifdef LODEBUG
	lo_dprint(4,
	"lo_lookup dvp %p realdvp %p nm '%s' newvp %p real vp %p error %d\n",
		dvp, realvp(dvp), nm, *vpp, vp, error);
#endif
	return (error);
}

/*ARGSUSED*/
static int
lo_create(
	vnode_t *dvp,
	char *nm,
	struct vattr *va,
	enum vcexcl exclusive,
	int mode,
	vnode_t **vpp,
	struct cred *cr,
	int flag)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_create vp %p realvp %p\n", dvp, realvp(dvp));
#endif
	dvp = realvp(dvp);
	return (VOP_CREATE(dvp, nm, va, exclusive, mode, vpp, cr, flag));
}

static int
lo_remove(vnode_t *dvp, char *nm, struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_remove vp %p realvp %p\n", dvp, realvp(dvp));
#endif
	dvp = realvp(dvp);
	return (VOP_REMOVE(dvp, nm, cr));
}

static int
lo_link(vnode_t *tdvp, vnode_t *vp, char *tnm, struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_link vp %p realvp %p\n", vp, realvp(vp));
#endif
	while (vp->v_op == &lo_vnodeops)
		vp = realvp(vp);
	while (tdvp->v_op == &lo_vnodeops)
		tdvp = realvp(tdvp);
	if (vp->v_vfsp != tdvp->v_vfsp)
		return (EXDEV);
	return (VOP_LINK(tdvp, vp, tnm, cr));
}

static int
lo_rename(
	vnode_t *odvp,
	char *onm,
	vnode_t *ndvp,
	char *nnm,
	struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_rename vp %p realvp %p\n", odvp, realvp(odvp));
#endif
	while (odvp->v_op == &lo_vnodeops)
		odvp = realvp(odvp);
	while (ndvp->v_op == &lo_vnodeops)
		ndvp = realvp(ndvp);
	if (odvp->v_vfsp != ndvp->v_vfsp)
		return (EXDEV);
	return (VOP_RENAME(odvp, onm, ndvp, nnm, cr));
}

static int
lo_mkdir(
	vnode_t *dvp,
	char *nm,
	struct vattr *va,
	vnode_t **vpp,
	struct cred *cr)
{
	vnode_t *vp;
	int error;

#ifdef LODEBUG
	lo_dprint(4, "lo_mkdir vp %p realvp %p\n", dvp, realvp(dvp));
#endif
	error = VOP_MKDIR(realvp(dvp), nm, va, &vp, cr);
	if (!error)
		*vpp = makelonode(vp, vtoli(dvp->v_vfsp));
	return (error);
}

static int
lo_rmdir(
	vnode_t *dvp,
	char *nm,
	vnode_t *cdir,
	struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_rmdir vp %p realvp %p\n", dvp, realvp(dvp));
#endif
	dvp = realvp(dvp);
	return (VOP_RMDIR(dvp, nm, cdir, cr));
}

static int
lo_symlink(
	vnode_t *dvp,
	char *lnm,
	struct vattr *tva,
	char *tnm,
	struct cred *cr)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_symlink vp %p realvp %p\n", dvp, realvp(dvp));
#endif
	dvp = realvp(dvp);
	return (VOP_SYMLINK(dvp, lnm, tva, tnm, cr));
}

static int
lo_readlink(vnode_t *vp, struct uio *uiop, struct cred *cr)
{
	vp = realvp(vp);
	return (VOP_READLINK(vp, uiop, cr));
}

static int
lo_readdir(vnode_t *vp, struct uio *uiop, struct cred *cr, int *eofp)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_readdir vp %p realvp %p\n", vp, realvp(vp));
#endif
	vp = realvp(vp);
	return (VOP_READDIR(vp, uiop, cr, eofp));
}

static void
lo_rwlock(vnode_t *vp, int write_lock)
{
	vp = realvp(vp);
	VOP_RWLOCK(vp, write_lock);
}

static void
lo_rwunlock(vnode_t *vp, int write_lock)
{
	vp = realvp(vp);
	VOP_RWUNLOCK(vp, write_lock);
}

static int
lo_seek(vnode_t *vp, offset_t ooff, offset_t *noffp)
{
	vp = realvp(vp);
	return (VOP_SEEK(vp, ooff, noffp));
}

static int
lo_cmp(vnode_t *vp1, vnode_t *vp2)
{
	while (vp1->v_op == &lo_vnodeops)
		vp1 = realvp(vp1);
	while (vp2->v_op == &lo_vnodeops)
		vp2 = realvp(vp2);
	return (VOP_CMP(vp1, vp2));
}

static int
lo_frlock(
	vnode_t *vp,
	int cmd,
	struct flock64 *bfp,
	int flag,
	offset_t offset,
	cred_t *cr)
{
	vp = realvp(vp);
	return (VOP_FRLOCK(vp, cmd, bfp, flag, offset, cr));
}

static int
lo_space(
	vnode_t *vp,
	int cmd,
	struct flock64 *bfp,
	int flag,
	offset_t offset,
	struct cred *cr)
{
	vp = realvp(vp);
	return (VOP_SPACE(vp, cmd, bfp, flag, offset, cr));
}

static int
lo_realvp(vnode_t *vp, vnode_t **vpp)
{
#ifdef LODEBUG
	lo_dprint(4, "lo_realvp %p\n", vp);
#endif
	while (vp->v_op == &lo_vnodeops)
		vp = realvp(vp);

	if (VOP_REALVP(vp, vpp) != 0)
		*vpp = vp;
	return (0);
}

static int
lo_getpage(
	vnode_t *vp,
	offset_t off,
	size_t len,
	uint_t *prot,
	struct page *parr[],
	size_t psz,
	struct seg *seg,
	caddr_t addr,
	enum seg_rw rw,
	struct cred *cr)
{
	vp = realvp(vp);
	return (VOP_GETPAGE(vp, off, len, prot, parr, psz, seg, addr, rw, cr));
}

static int
lo_putpage(vnode_t *vp, offset_t off, size_t len, int flags, struct cred *cr)
{
	vp = realvp(vp);
	return (VOP_PUTPAGE(vp, off, len, flags, cr));
}

static int
lo_map(
	vnode_t *vp,
	offset_t off,
	struct as *as,
	caddr_t *addrp,
	size_t len,
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	struct cred *cr)
{
	vp = realvp(vp);
	return (VOP_MAP(vp, off, as, addrp, len, prot, maxprot, flags, cr));
}

static int
lo_addmap(
	vnode_t *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	struct cred *cr)
{
	vp = realvp(vp);
	return (VOP_ADDMAP(vp, off, as, addr, len, prot, maxprot, flags, cr));
}

static int
lo_delmap(
	vnode_t *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,
	uint_t prot,
	uint_t maxprot,
	uint_t flags,
	struct cred *cr)
{
	vp = realvp(vp);
	return (VOP_DELMAP(vp, off, as, addr, len, prot, maxprot, flags, cr));
}

static int
lo_poll(
	vnode_t *vp,
	short events,
	int anyyet,
	short *reventsp,
	struct pollhead **phpp)
{
	vp = realvp(vp);
	return (VOP_POLL(vp, events, anyyet, reventsp, phpp));
}

static int
lo_dump(vnode_t *vp, caddr_t addr, int bn, int count)
{
	vp = realvp(vp);
	return (VOP_DUMP(vp, addr, bn, count));
}

static int
lo_pathconf(vnode_t *vp, int cmd, ulong_t *valp, struct cred *cr)
{
	vp = realvp(vp);
	return (VOP_PATHCONF(vp, cmd, valp, cr));
}

static int
lo_pageio(
	vnode_t *vp,
	struct page *pp,
	u_offset_t io_off,
	size_t io_len,
	int flags,
	cred_t *cr)
{
	vp = realvp(vp);
	return (VOP_PAGEIO(vp, pp, io_off, io_len, flags, cr));
}

static void
lo_dispose(vnode_t *vp, page_t *pp, int fl, int dn, cred_t *cr)
{
	vp = realvp(vp);
	if (vp != NULL && vp != &kvp)
		VOP_DISPOSE(vp, pp, fl, dn, cr);
}

static int
lo_setsecattr(vnode_t *vp, vsecattr_t *secattr, int flags, struct cred *cr)
{
	vp = realvp(vp);
	return (VOP_SETSECATTR(vp, secattr, flags, cr));
}

static int
lo_getsecattr(vnode_t *vp, vsecattr_t *secattr, int flags, struct cred *cr)
{
	vp = realvp(vp);
	return (VOP_GETSECATTR(vp, secattr, flags, cr));
}

static int
lo_shrlock(vnode_t *vp, int cmd, struct shrlock *shr, int flag)
{
	vp = realvp(vp);
	return (VOP_SHRLOCK(vp, cmd, shr, flag));
}

/*
 * Loopback vnode operations vector.
 */
struct vnodeops lo_vnodeops = {
	lo_open,
	lo_close,
	lo_read,
	lo_write,
	lo_ioctl,
	lo_setfl,
	lo_getattr,
	lo_setattr,
	lo_access,
	lo_lookup,
	lo_create,
	lo_remove,
	lo_link,
	lo_rename,
	lo_mkdir,
	lo_rmdir,
	lo_readdir,
	lo_symlink,
	lo_readlink,
	lo_fsync,
	lo_inactive,
	lo_fid,
	lo_rwlock,
	lo_rwunlock,
	lo_seek,
	lo_cmp,
	lo_frlock,
	lo_space,
	lo_realvp,
	lo_getpage,
	lo_putpage,
	lo_map,
	lo_addmap,
	lo_delmap,
	lo_poll,
	lo_dump,
	lo_pathconf,
	lo_pageio,
	fs_nosys,	/* dumpctl */
	lo_dispose,
	lo_setsecattr,
	lo_getsecattr,
	lo_shrlock
};
