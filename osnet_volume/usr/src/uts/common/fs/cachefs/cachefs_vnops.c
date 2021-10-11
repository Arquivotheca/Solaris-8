/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cachefs_vnops.c	1.233	99/07/09 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/mman.h>
#include <sys/tiuser.h>
#include <sys/pathname.h>
#include <sys/dirent.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/vmsystm.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/swap.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/disp.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/mount.h>
#include <sys/bootconf.h>
#include <sys/dnlc.h>
#include <sys/stat.h>
#include <sys/acl.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/rm.h>
#include <sys/fs/cachefs_fs.h>
#include <sys/fs/cachefs_dir.h>
#include <sys/fs/cachefs_dlog.h>
#include <sys/fs/cachefs_ioctl.h>
#include <sys/fs/cachefs_log.h>
#include <fs/fs_subr.h>

int cachefs_dnlc;	/* use dnlc, debugging */

static void cachefs_attr_setup(vattr_t *srcp, vattr_t *targp, cnode_t *cp,
    cred_t *cr);
static void cachefs_creategid(cnode_t *dcp, cnode_t *newcp, vattr_t *vap,
    cred_t *cr);
static void cachefs_createacl(cnode_t *dcp, cnode_t *newcp);
static int cachefs_getaclfromcache(cnode_t *cp, vsecattr_t *vsec);
static int cachefs_getacldirvp(cnode_t *cp);
static void cachefs_acl2perm(cnode_t *cp, vsecattr_t *vsec);
static int cachefs_access_local(cnode_t *cp, int mode, cred_t *cr);
static int cachefs_acl_access(struct cnode *cp, int mode, cred_t *cr);
static int cachefs_push_connected(vnode_t *vp, struct buf *bp, size_t iolen,
    u_offset_t iooff, cred_t *cr);
static int cachefs_push_front(vnode_t *vp, struct buf *bp, size_t iolen,
    u_offset_t iooff, cred_t *cr);
static int cachefs_setattr_connected(vnode_t *vp, vattr_t *vap, int flags,
    cred_t *cr);
static int cachefs_setattr_disconnected(vnode_t *vp, vattr_t *vap,
    int flags, cred_t *cr);
static int cachefs_access_connected(struct vnode *vp, int mode,
    int flags, cred_t *cr);
static int cachefs_lookup_back(vnode_t *dvp, char *nm, vnode_t **vpp,
    cred_t *cr);
static int cachefs_symlink_connected(vnode_t *dvp, char *lnm, vattr_t *tva,
    char *tnm, cred_t *cr);
static int cachefs_symlink_disconnected(vnode_t *dvp, char *lnm,
    vattr_t *tva, char *tnm, cred_t *cr);
static int cachefs_link_connected(vnode_t *tdvp, vnode_t *fvp, char *tnm,
    cred_t *cr);
static int cachefs_link_disconnected(vnode_t *tdvp, vnode_t *fvp,
    char *tnm, cred_t *cr);
static int cachefs_mkdir_connected(vnode_t *dvp, char *nm, vattr_t *vap,
    vnode_t **vpp, cred_t *cr);
static int cachefs_mkdir_disconnected(vnode_t *dvp, char *nm, vattr_t *vap,
    vnode_t **vpp, cred_t *cr);
static int cachefs_stickyrmchk(struct cnode *dcp, struct cnode *cp, cred_t *cr);
static int cachefs_rmdir_connected(vnode_t *dvp, char *nm,
    vnode_t *cdir, cred_t *cr, vnode_t *vp);
static int cachefs_rmdir_disconnected(vnode_t *dvp, char *nm,
    vnode_t *cdir, cred_t *cr, vnode_t *vp);
static char *cachefs_newname(void);
static int cachefs_remove_dolink(vnode_t *dvp, vnode_t *vp, char *nm,
    cred_t *cr);
static int cachefs_rename_connected(vnode_t *odvp, char *onm,
    vnode_t *ndvp, char *nnm, cred_t *cr, vnode_t *delvp);
static int cachefs_rename_disconnected(vnode_t *odvp, char *onm,
    vnode_t *ndvp, char *nnm, cred_t *cr, vnode_t *delvp);
static int cachefs_readdir_connected(vnode_t *vp, uio_t *uiop, cred_t *cr,
    int *eofp);
static int cachefs_readdir_disconnected(vnode_t *vp, uio_t *uiop,
    cred_t *cr, int *eofp);
static int cachefs_readback_translate(cnode_t *cp, uio_t *uiop,
	cred_t *cr, int *eofp);
static int cachefs_convert_mount(struct fscache *fscp,
    struct cachefs_cnvt_mnt *ccmp);
static int cachefs_setattr_common(vnode_t *vp, vattr_t *vap, int flags,
    cred_t *cr);

static	int	cachefs_open(struct vnode **, int, cred_t *);
static	int	cachefs_close(struct vnode *, int, int, offset_t,
			cred_t *);
static	int	cachefs_read(struct vnode *, struct uio *, int, cred_t *);
static	int	cachefs_write(struct vnode *, struct uio *, int, cred_t *);
static	int	cachefs_ioctl(struct vnode *, int, intptr_t, int, cred_t *,
			int *);
static	int	cachefs_getattr(struct vnode *, struct vattr *, int,
			cred_t *);
static	int	cachefs_setattr(struct vnode *, struct vattr *,
			int, cred_t *);
static	int	cachefs_access(struct vnode *, int, int, cred_t *);
static	int	cachefs_lookup(struct vnode *, char *, struct vnode **,
			struct pathname *, int, struct vnode *, cred_t *);
static	int	cachefs_create(struct vnode *, char *, struct vattr *,
			enum vcexcl, int, struct vnode **, cred_t *, int);
static	int	cachefs_create_connected(vnode_t *dvp, char *nm,
			vattr_t *vap, enum vcexcl exclusive, int mode,
			vnode_t **vpp, cred_t *cr);
static	int	cachefs_create_disconnected(vnode_t *dvp, char *nm,
			vattr_t *vap, enum vcexcl exclusive, int mode,
			vnode_t **vpp, cred_t *cr);
static	int	cachefs_remove(struct vnode *, char *, cred_t *);
static	int	cachefs_link(struct vnode *, struct vnode *, char *,
			cred_t *);
static	int	cachefs_rename(struct vnode *, char *, struct vnode *,
			char *, cred_t *);
static	int	cachefs_mkdir(struct vnode *, char *, struct
			vattr *, struct vnode **, cred_t *);
static	int	cachefs_rmdir(struct vnode *, char *, struct vnode *,
			cred_t *);
static	int	cachefs_readdir(struct vnode *, struct uio *,
			cred_t *, int *);
static	int	cachefs_symlink(struct vnode *, char *, struct vattr *,
			char *, cred_t *);
static	int	cachefs_readlink(struct vnode *, struct uio *, cred_t *);
static int cachefs_readlink_connected(vnode_t *vp, uio_t *uiop, cred_t *cr);
static int cachefs_readlink_disconnected(vnode_t *vp, uio_t *uiop);
static	int	cachefs_fsync(struct vnode *, int, cred_t *);
static	void	cachefs_inactive(struct vnode *, cred_t *);
static	int	cachefs_fid(struct vnode *, struct fid *);
static	void	cachefs_rwlock(struct vnode *, int);
static	void	cachefs_rwunlock(struct vnode *, int);
static	int	cachefs_seek(struct vnode *, offset_t, offset_t *);
static	int	cachefs_cmp(struct vnode *, struct vnode *);
static	int	cachefs_frlock(struct vnode *, int, struct flock64 *,
			int, offset_t, cred_t *);
static	int	cachefs_space(struct vnode *, int, struct flock64 *, int,
			offset_t, cred_t *);
static	int	cachefs_realvp(struct vnode *, struct vnode **);
static	int	cachefs_getpage(struct vnode *, offset_t, size_t, uint_t *,
			struct page *[], size_t, struct seg *, caddr_t,
			enum seg_rw, cred_t *);
static	int	cachefs_getapage(struct vnode *, u_offset_t, size_t, uint_t *,
			struct page *[], size_t, struct seg *, caddr_t,
			enum seg_rw, cred_t *);
static	int	cachefs_getapage_back(struct vnode *, u_offset_t, size_t,
		uint_t *, struct page *[], size_t, struct seg *, caddr_t,
			enum seg_rw, cred_t *);
static	int	cachefs_putpage(struct vnode *, offset_t, size_t, int,
			cred_t *);
static	int	cachefs_map(struct vnode *, offset_t, struct as *,
			caddr_t *, size_t, uchar_t, uchar_t, uint_t, cred_t *);
static	int	cachefs_addmap(struct vnode *, offset_t, struct as *,
			caddr_t, size_t, uchar_t, uchar_t, uint_t, cred_t *);
static	int	cachefs_delmap(struct vnode *, offset_t, struct as *,
			caddr_t, size_t, uint_t, uint_t, uint_t, cred_t *);
static int	cachefs_setsecattr(vnode_t *vp, vsecattr_t *vsec,
			int flag, cred_t *cr);
static int	cachefs_getsecattr(vnode_t *vp, vsecattr_t *vsec,
			int flag, cred_t *cr);
static	int	cachefs_shrlock(vnode_t *, int, struct shrlock *, int);
static int cachefs_getsecattr_connected(vnode_t *vp, vsecattr_t *vsec, int flag,
    cred_t *cr);
static int cachefs_getsecattr_disconnected(vnode_t *vp, vsecattr_t *vsec,
    int flag, cred_t *cr);

static int	cachefs_dump(struct vnode *, caddr_t, int, int);
static int	cachefs_pageio(struct vnode *, page_t *,
		    u_offset_t, size_t, int, cred_t *);
static int	cachefs_writepage(struct vnode *vp, caddr_t base,
		    int tcount, struct uio *uiop);
static int	cachefs_pathconf(vnode_t *, int, ulong_t *, cred_t *);

#ifdef COC
static int	cachefs_convert_mount(struct fscache *fscp,
    struct cachefs_cnvt_mnt *ccmp);
#endif /* COC */

struct vnodeops cachefs_vnodeops = {
	cachefs_open,
	cachefs_close,
	cachefs_read,
	cachefs_write,
	cachefs_ioctl,
	fs_setfl,
	cachefs_getattr,
	cachefs_setattr,
	cachefs_access,
	cachefs_lookup,
	cachefs_create,
	cachefs_remove,
	cachefs_link,
	cachefs_rename,
	cachefs_mkdir,
	cachefs_rmdir,
	cachefs_readdir,
	cachefs_symlink,
	cachefs_readlink,
	cachefs_fsync,
	cachefs_inactive,
	cachefs_fid,
	cachefs_rwlock,
	cachefs_rwunlock,
	cachefs_seek,
	cachefs_cmp,
	cachefs_frlock,
	cachefs_space,
	cachefs_realvp,
	cachefs_getpage,
	cachefs_putpage,
	cachefs_map,
	cachefs_addmap,
	cachefs_delmap,
	fs_poll,
	cachefs_dump,
	cachefs_pathconf,
	cachefs_pageio,
	fs_nosys,
	fs_dispose,
	cachefs_setsecattr,
	cachefs_getsecattr,
	cachefs_shrlock
};

/* forward declarations of statics */
static void cachefs_modified(cnode_t *cp);
static int cachefs_modified_alloc(cnode_t *cp);

struct vnodeops *
cachefs_getvnodeops(void)
{
	return (&cachefs_vnodeops);
}

static int
cachefs_open(vnode_t **vpp, int flag, cred_t *cr)
{
	int error = 0;
	cnode_t *cp = VTOC(*vpp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int held = 0;
	int type;
	int connected = 0;
	int cachefs_cred_cmp();

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_open: ENTER vpp %p flag %x\n",
		    (void *)vpp, flag);
#endif
	if ((flag & FWRITE) &&
	    ((*vpp)->v_type == VDIR || (*vpp)->v_type == VLNK)) {
		error = EISDIR;
		goto out;
	}

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			goto out;
		held = 1;

		mutex_enter(&cp->c_statelock);

		/* grab creds if we do not have any yet */
		if (cp->c_cred == NULL) {
			crhold(cr);
			cp->c_cred = cr;
		}
		cp->c_flags |= CN_NEEDOPEN;

		/* if we are disconnected */
		if (fscp->fs_cdconnected != CFS_CD_CONNECTED) {
			/* if we cannot write to the file system */
			if ((flag & FWRITE) && CFS_ISFS_WRITE_AROUND(fscp)) {
				mutex_exit(&cp->c_statelock);
				connected = 1;
				continue;
			}
			/*
			 * Allow read only requests to continue
			 */
			if ((flag & (FWRITE|FREAD)) == FREAD) {
				mutex_exit(&cp->c_statelock);
				error = 0;
				break;
			}

			/*
			 * check credentials  - if this procs
			 * credentials don't match the creds in the
			 * cnode disallow writing while disconnected.
			 */
			if ((!suser(CRED())) &&
			    cachefs_cred_cmp(cp->c_cred, CRED()) == 0) {
				mutex_exit(&cp->c_statelock);
				connected = 1;
				continue;
			}
		}

		/* else if we are connected */
		else {
			/* if cannot use the cached copy of the file */
			if ((flag & FWRITE) && CFS_ISFS_WRITE_AROUND(fscp) &&
			    ((cp->c_flags & CN_NOCACHE) == 0))
				cachefs_nocache(cp);

			/* pass open to the back file */
			if (cp->c_backvp) {
				cp->c_flags &= ~CN_NEEDOPEN;
				error = VOP_OPEN(&cp->c_backvp, flag, cr);
				if (CFS_TIMEOUT(fscp, error)) {
					mutex_exit(&cp->c_statelock);
					cachefs_cd_release(fscp);
					held = 0;
					cachefs_cd_timedout(fscp);
					continue;
				} else if (error) {
					mutex_exit(&cp->c_statelock);
					break;
				}
			}

			/*
			 * Now perform a consistency check on the file.
			 * If strict consistency then force a check to
			 * the backfs even if the timeout has not expired
			 * for close-to-open consistency.
			 */
			type = 0;
			if (fscp->fs_consttype == CFS_FS_CONST_STRICT)
				type = C_BACK_CHECK;
			error = CFSOP_CHECK_COBJECT(fscp, cp, type, cr);
			if (CFS_TIMEOUT(fscp, error)) {
				mutex_exit(&cp->c_statelock);
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				continue;
			}
		}
		mutex_exit(&cp->c_statelock);
		break;
	}
	if (held)
		cachefs_cd_release(fscp);
out:
#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_open: EXIT vpp %p error %d\n",
		    (void *)vpp, error);
#endif
	return (error);
}

/* ARGSUSED */
static int
cachefs_close(vnode_t *vp, int flag, int count, offset_t offset, cred_t *cr)
{
	int error = 0;
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int held = 0;
	int connected = 0;
	cachefscache_t *cachep;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_close: ENTER vp %p\n", (void *)vp);
#endif

	/* get rid of any local locks */
	if (CFS_ISFS_LLOCK(fscp)) {
		(void) cleanlocks(vp, ttoproc(curthread)->p_pid, 0);
	}

	/* clean up if this is the daemon closing down */
	if ((fscp->fs_cddaemonid == ttoproc(curthread)->p_pid) &&
	    ((ttoproc(curthread)->p_pid) != 0) &&
	    (vp == fscp->fs_rootvp) &&
	    (count == 1)) {
		mutex_enter(&fscp->fs_cdlock);
		fscp->fs_cddaemonid = 0;
		if (fscp->fs_dlogfile)
			fscp->fs_cdconnected = CFS_CD_DISCONNECTED;
		else
			fscp->fs_cdconnected = CFS_CD_CONNECTED;
		cv_broadcast(&fscp->fs_cdwaitcv);
		mutex_exit(&fscp->fs_cdlock);
		if (fscp->fs_flags & CFS_FS_ROOTFS) {
			cachep = fscp->fs_cache;
			mutex_enter(&cachep->c_contentslock);
			ASSERT(cachep->c_rootdaemonid != 0);
			cachep->c_rootdaemonid = 0;
			mutex_exit(&cachep->c_contentslock);
		}
		return (0);
	}

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			goto out;
		held = 1;
		connected = 0;

		/* if not the last close */
		if (count > 1) {
			if (fscp->fs_cdconnected != CFS_CD_CONNECTED)
				goto out;
			mutex_enter(&cp->c_statelock);
			if (cp->c_backvp) {
				error = VOP_CLOSE(cp->c_backvp, flag, count,
				    offset, cr);
				if (CFS_TIMEOUT(fscp, error)) {
					cachefs_cd_release(fscp);
					held = 0;
					cachefs_cd_timedout(fscp);
					continue;
				}
			}
			mutex_exit(&cp->c_statelock);
			goto out;
		}

		/*
		 * If the file is an unlinked file, then flush the lookup
		 * cache so that inactive will be called if this is
		 * the last reference.  It will invalidate all of the
		 * cached pages, without writing them out.  Writing them
		 * out is not required because they will be written to a
		 * file which will be immediately removed.
		 */
		if (cp->c_unldvp != NULL) {
			dnlc_purge_vp(vp);
			mutex_enter(&cp->c_statelock);
			error = cp->c_error;
			cp->c_error = 0;
			mutex_exit(&cp->c_statelock);
			goto out;
		}

		/* force dirty data to stable storage */
		if ((vp->v_type == VREG) && (flag & FWRITE)) {
			/* clean the cachefs pages synchronously */
			error = cachefs_putpage_common(vp, (offset_t)0,
			    0, 0, cr);
			if (CFS_TIMEOUT(fscp, error)) {
				if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
					cachefs_cd_release(fscp);
					held = 0;
					cachefs_cd_timedout(fscp);
					continue;
				} else {
					connected = 1;
					continue;
				}
			}

			/* if no space left in cache, wait until connected */
			if ((error == ENOSPC) &&
			    (fscp->fs_cdconnected != CFS_CD_CONNECTED)) {
				connected = 1;
				continue;
			}

			/* clear the cnode error if putpage worked */
			if ((error == 0) && cp->c_error) {
				mutex_enter(&cp->c_statelock);
				cp->c_error = 0;
				mutex_exit(&cp->c_statelock);
			}

			/* if any other important error */
			if (cp->c_error) {
				/* get rid of the pages */
				(void) cachefs_putpage_common(vp,
				    (offset_t)0, 0, B_INVAL | B_FORCE, cr);
				dnlc_purge_vp(vp);
			}
		}

		mutex_enter(&cp->c_statelock);
		if (cp->c_backvp &&
		    (fscp->fs_cdconnected == CFS_CD_CONNECTED)) {
			error = VOP_CLOSE(cp->c_backvp, flag, count,
			    offset, cr);
			if (CFS_TIMEOUT(fscp, error)) {
				mutex_exit(&cp->c_statelock);
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				continue;
			}
		}
		mutex_exit(&cp->c_statelock);
		break;
	}

	mutex_enter(&cp->c_statelock);
	if (!error)
		error = cp->c_error;
	cp->c_error = 0;
	mutex_exit(&cp->c_statelock);

out:
	if (held)
		cachefs_cd_release(fscp);
#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_close: EXIT vp %p\n", (void *)vp);
#endif
	return (error);
}

/*ARGSUSED*/
static int
cachefs_read(vnode_t *vp, uio_t *uiop, int ioflag, cred_t *cr)
{
	struct cnode *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	register u_offset_t off;
	register int mapoff;
	register caddr_t base;
	int n;
	offset_t diff;
	uint_t flags = 0;
	int error = 0;

#if 0
	if (vp->v_flag & VNOCACHE)
		flags = SM_INVAL;
#endif
	if (vp->v_type != VREG)
		return (EISDIR);

	ASSERT(RW_READ_HELD(&cp->c_rwlock));

	if (uiop->uio_resid == 0)
		return (0);


	if (uiop->uio_loffset < (offset_t)0)
		return (EINVAL);

	if (MANDLOCK(vp, cp->c_attr.va_mode)) {
		error = chklock(vp, FREAD, (offset_t)uiop->uio_loffset,
			    uiop->uio_resid, uiop->uio_fmode);
		if (error)
			return (error);
	}

	/*
	 * Sit in a loop and transfer (uiomove) the data in up to
	 * MAXBSIZE chunks. Each chunk is mapped into the kernel's
	 * address space as needed and then released.
	 */
	do {
		/*
		 *	off	Offset of current MAXBSIZE chunk
		 *	mapoff	Offset within the current chunk
		 *	n	Number of bytes to move from this chunk
		 *	base	kernel address of mapped in chunk
		 */
		off = uiop->uio_loffset & (offset_t)MAXBMASK;
		mapoff = uiop->uio_loffset & MAXBOFFSET;
		n = MAXBSIZE - mapoff;
		if (n > uiop->uio_resid)
			n = (uint_t)uiop->uio_resid;

		/* perform consistency check */
		error = cachefs_cd_access(fscp, 0, 0);
		if (error)
			break;
		mutex_enter(&cp->c_statelock);
		error = CFSOP_CHECK_COBJECT(fscp, cp, 0, cr);
		diff = cp->c_size - uiop->uio_loffset;
		mutex_exit(&cp->c_statelock);
		if (CFS_TIMEOUT(fscp, error)) {
			cachefs_cd_release(fscp);
			cachefs_cd_timedout(fscp);
			error = 0;
			continue;
		}
		cachefs_cd_release(fscp);

		if (error)
			break;

		if (diff <= (offset_t)0)
			break;
		if (diff < (offset_t)n)
			n = diff;

		base = segmap_getmap(segkmap, vp, off);
		error = segmap_fault(kas.a_hat, segkmap, base, n,
			F_SOFTLOCK, S_READ);
		if (error) {
			(void) segmap_release(segkmap, base, 0);
			if (FC_CODE(error) == FC_OBJERR)
				error =  FC_ERRNO(error);
			else
				error = EIO;
			break;
		}
		error = uiomove(base+mapoff, n, UIO_READ, uiop);
		(void) segmap_fault(kas.a_hat, segkmap, base, n,
				F_SOFTUNLOCK, S_READ);
		if (error == 0) {
			/*
			 * if we read a whole page(s), or to eof,
			 *  we won't need this page(s) again soon.
			 */
			if (n + mapoff == MAXBSIZE ||
				uiop->uio_loffset == cp->c_size)
				flags |= SM_DONTNEED;
		}
		(void) segmap_release(segkmap, base, flags);
	} while (error == 0 && uiop->uio_resid > 0);

out:
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_read: EXIT error %d resid %ld\n", error,
			uiop->uio_resid);
#endif
	return (error);
}

/*ARGSUSED*/
static int
cachefs_write(vnode_t *vp, uio_t *uiop, int ioflag, cred_t *cr)
{
	struct cnode *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;
	u_offset_t off;
	caddr_t base;
	uint_t bsize;
	uint_t flags;
	int n, on;
	rlim64_t limit = uiop->uio_llimit;
	ssize_t resid;
	offset_t offset;
	offset_t remainder;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf(
		"cachefs_write: ENTER vp %p offset %llu count %ld cflags %x\n",
			(void *)vp, uiop->uio_loffset, uiop->uio_resid,
			cp->c_flags);
#endif
	if (vp->v_type != VREG) {
		error = EISDIR;
		goto out;
	}

	ASSERT(RW_WRITE_HELD(&cp->c_rwlock));

	if (uiop->uio_resid == 0) {
		goto out;
	}

	if (MANDLOCK(vp, cp->c_attr.va_mode)) {
		error = chklock(vp, FWRITE, (offset_t)uiop->uio_loffset,
			    uiop->uio_resid, uiop->uio_fmode);
		if (error)
			goto out;
	}

	if (ioflag & FAPPEND) {
		for (;;) {
			/* do consistency check to get correct file size */
			error = cachefs_cd_access(fscp, 0, 1);
			if (error)
				goto out;
			mutex_enter(&cp->c_statelock);
			error = CFSOP_CHECK_COBJECT(fscp, cp, 0, cr);
			uiop->uio_loffset = cp->c_size;
			mutex_exit(&cp->c_statelock);
			if (CFS_TIMEOUT(fscp, error)) {
				cachefs_cd_release(fscp);
				cachefs_cd_timedout(fscp);
				continue;
			}
			cachefs_cd_release(fscp);
			if (error)
				goto out;
			break;
		}
	}

	if (limit == RLIM64_INFINITY || limit > MAXOFFSET_T)
		limit = MAXOFFSET_T;

	if (uiop->uio_loffset >= limit) {
		psignal(ttoproc(curthread), SIGXFSZ);
		error = EFBIG;
		goto out;
	}
	if (uiop->uio_loffset > fscp->fs_offmax) {
		error = EFBIG;
		goto out;
	}

	if (limit > fscp->fs_offmax)
		limit = fscp->fs_offmax;

	if (uiop->uio_loffset < (offset_t)0) {
		error = EINVAL;
		goto out;
	}

	offset = uiop->uio_loffset + uiop->uio_resid;
	/*
	 * Check to make sure that the process will not exceed
	 * its limit on file size.  It is okay to write up to
	 * the limit, but not beyond.  Thus, the write which
	 * reaches the limit will be short and the next write
	 * will return an error.
	 */
	remainder = 0;
	if (offset > limit) {
		remainder = (int)(offset - (u_offset_t)limit);
		uiop->uio_resid = limit - uiop->uio_loffset;
		if (uiop->uio_resid <= 0) {
			uiop->uio_resid += remainder;
			tsignal(curthread, SIGXFSZ);
			error = EFBIG;
			goto out;
		}
	}

	resid = uiop->uio_resid;
	offset = uiop->uio_loffset;
	bsize = vp->v_vfsp->vfs_bsize;

	/* loop around and do the write in MAXBSIZE chunks */
	do {
		/* mapping offset */
		off = uiop->uio_loffset & (offset_t)MAXBMASK;
		on = uiop->uio_loffset & MAXBOFFSET; /* Rel. offset */
		n = MAXBSIZE - on;
		if (n > uiop->uio_resid)
			n = (int)uiop->uio_resid;
		base = segmap_getmap(segkmap, vp, off);
		error = cachefs_writepage(vp, (base + on), n, uiop);
		if (error == 0) {
			flags = 0;
			/*
			* Have written a whole block.Start an
			* asynchronous write and mark the buffer to
			* indicate that it won't be needed again
			* soon.
			*/
			if (n + on == bsize) {
				flags = SM_WRITE |SM_ASYNC |SM_DONTNEED;
			}
#if 0
			/* XXX need to understand this */
			if ((ioflag & (FSYNC|FDSYNC)) ||
			    (cp->c_backvp && cp->c_backvp->v_filocks)) {
				flags &= ~SM_ASYNC;
				flags |= SM_WRITE;
			}
#else
			if (ioflag & (FSYNC|FDSYNC)) {
				flags &= ~SM_ASYNC;
				flags |= SM_WRITE;
			}
#endif
			error = segmap_release(segkmap, base, flags);
		} else {
			(void) segmap_release(segkmap, base, 0);
		}
	} while (error == 0 && uiop->uio_resid > 0);

out:
	if (error == EINTR && (ioflag & (FSYNC|FDSYNC))) {
		uiop->uio_resid = resid;
		uiop->uio_loffset = offset;
	} else
		uiop->uio_resid += remainder;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_write: EXIT error %d\n", error);
#endif
	return (error);
}

/*
 * see if we've charged ourselves for frontfile data at
 * the given offset.  If not, allocate a block for it now.
 */
static int
cachefs_charge_page(struct cnode *cp, u_offset_t offset)
{
	u_offset_t blockoff;
	int error;
	int inc;

	ASSERT(MUTEX_HELD(&cp->c_statelock));
	ASSERT(PAGESIZE <= MAXBSIZE);

	error = 0;
	blockoff = offset & (offset_t)MAXBMASK;

	/* get the front file if necessary so allocblocks works */
	if ((cp->c_frontvp == NULL) &&
	    ((cp->c_flags & CN_NOCACHE) == 0)) {
		(void) cachefs_getfrontfile(cp);
	}
	if (cp->c_flags & CN_NOCACHE)
		return (1);

	if (cachefs_check_allocmap(cp, blockoff))
		return (0);

	for (inc = PAGESIZE; inc < MAXBSIZE; inc += PAGESIZE)
		if (cachefs_check_allocmap(cp, blockoff+inc))
			return (0);

	error = cachefs_allocblocks(C_TO_FSCACHE(cp)->fs_cache, 1,
	    cp->c_metadata.md_rltype);
	if (error == 0) {
		cp->c_metadata.md_frontblks++;
		cp->c_flags |= CN_UPDATED;
	}
	return (error);
}

/*
 * Called only by cachefs_write to write 1 page or less of data.
 *	base   - base address kernel addr space
 *	tcount - Total bytes to move - < MAXBSIZE
 */
static int
cachefs_writepage(vnode_t *vp, caddr_t base, int tcount, uio_t *uiop)
{
	struct cnode *cp =  VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	register int n;
	register u_offset_t offset;
	int error = 0, terror;
	extern struct as kas;
	u_offset_t lastpage_off;
	int pagecreate = 0;
	int newpage;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf(
		    "cachefs_writepage: ENTER vp %p offset %llu len %ld\\\n",
		    (void *)vp, uiop->uio_loffset, uiop->uio_resid);
#endif

	/*
	 * Move bytes in PAGESIZE chunks. We must avoid spanning pages in
	 * uiomove() because page faults may cause the cache to be invalidated
	 * out from under us.
	 */
	do {
		offset = uiop->uio_loffset;
		lastpage_off = (cp->c_size - 1) & (offset_t)PAGEMASK;

		/*
		 * If not connected then need to make sure we have space
		 * to perform the write.  We could make this check
		 * a little tighter by only doing it if we are growing the file.
		 */
		if (fscp->fs_cdconnected != CFS_CD_CONNECTED) {
			error = cachefs_allocblocks(fscp->fs_cache, 1,
			    cp->c_metadata.md_rltype);
			if (error)
				break;
			cachefs_freeblocks(fscp->fs_cache, 1,
			    cp->c_metadata.md_rltype);
		}

		/*
		 * n is the number of bytes required to satisfy the request
		 * or the number of bytes to fill out the page.
		 */
		n = (int)(PAGESIZE - ((uintptr_t)base & PAGEOFFSET));
		if (n > tcount)
			n = tcount;

		/*
		 * The number of bytes of data in the last page can not
		 * be accurately be determined while page is being
		 * uiomove'd to and the size of the file being updated.
		 * Thus, inform threads which need to know accurately
		 * how much data is in the last page of the file.  They
		 * will not do the i/o immediately, but will arrange for
		 * the i/o to happen later when this modify operation
		 * will have finished.
		 *
		 * in similar NFS code, this is done right before the
		 * uiomove(), which is best.  but here in cachefs, we
		 * have two uiomove()s, so we must do it here.
		 */
		ASSERT(!(cp->c_flags & CN_CMODINPROG));
		mutex_enter(&cp->c_statelock);
		cp->c_flags |= CN_CMODINPROG;
		cp->c_modaddr = (offset & (offset_t)MAXBMASK);
		mutex_exit(&cp->c_statelock);

		/*
		 * Check to see if we can skip reading in the page
		 * and just allocate the memory.  We can do this
		 * if we are going to rewrite the entire mapping
		 * or if we are going to write to or beyond the current
		 * end of file from the beginning of the mapping.
		 */
		if ((offset > (lastpage_off + PAGEOFFSET)) ||
			((cp->c_size == 0) && (offset < PAGESIZE)) ||
			((uintptr_t)base & PAGEOFFSET) == 0 && (n == PAGESIZE ||
			((offset + n) >= cp->c_size))) {
			pagecreate = 1;

			/*
			 * segmap_pagecreate() returns 1 if it calls
			 * page_create_va() to allocate any pages.
			 */
			newpage = segmap_pagecreate(segkmap,
			    (caddr_t)((uintptr_t)base & (offset_t)PAGEMASK),
			    PAGESIZE, 0);
			/* do not zero page if we are overwriting all of it */
			if (!((((uintptr_t)base & PAGEOFFSET) == 0) &&
			    (n == PAGESIZE))) {
				(void) kzero((void *)
				    ((uintptr_t)base & (offset_t)PAGEMASK),
				    PAGESIZE);
			}
			error = uiomove(base, n, UIO_WRITE, uiop);

			/*
			 * Unlock the page allocated by page_create_va()
			 * in segmap_pagecreate()
			 */
			if (newpage)
				segmap_pageunlock(segkmap,
					(caddr_t)((uintptr_t)base &
						(offset_t)PAGEMASK),
					PAGESIZE, S_WRITE);
		} else {
			/*
			 * KLUDGE ! Use segmap_fault instead of faulting and
			 * using as_fault() to avoid a recursive readers lock
			 * on kas.
			 */
			error = segmap_fault(kas.a_hat, segkmap,
				(caddr_t)((uintptr_t)base & (offset_t)PAGEMASK),
				PAGESIZE, F_SOFTLOCK, S_WRITE);
			if (error) {
				if (FC_CODE(error) == FC_OBJERR)
					error =  FC_ERRNO(error);
				else
					error = EIO;
				break;
			}
			error = uiomove(base, n, UIO_WRITE, uiop);
			(void) segmap_fault(kas.a_hat, segkmap,
					(caddr_t)((uintptr_t)base &
						(offset_t)PAGEMASK),
					PAGESIZE, F_SOFTUNLOCK, S_WRITE);
		}
		n = (int)(uiop->uio_loffset - offset); /* n = # bytes written */
		base += n;
		tcount -= n;

		/* get access to the file system */
		if ((terror = cachefs_cd_access(fscp, 0, 1)) != 0) {
			error = terror;
			break;
		}

		/*
		 * cp->c_attr.va_size is the maximum number of
		 * bytes known to be in the file.
		 * Make sure it is at least as high as the
		 * last byte we just wrote into the buffer.
		 */
		mutex_enter(&cp->c_statelock);
		if (cp->c_size < uiop->uio_loffset) {
			cp->c_size = uiop->uio_loffset;
		}
		if (cp->c_size != cp->c_attr.va_size) {
			cp->c_attr.va_size = cp->c_size;
			cp->c_flags |= CN_UPDATED;
		}
		/* c_size is now correct, so we can clear modinprog */
		cp->c_flags &= ~CN_CMODINPROG;
		if (error == 0) {
			cp->c_flags |= CDIRTY;
			if (pagecreate && (cp->c_flags & CN_NOCACHE) == 0) {
				/*
				 * if we're not in NOCACHE mode
				 * (i.e., single-writer), we update the
				 * allocmap here rather than waiting until
				 * cachefspush is called.  This prevents
				 * getpage from clustering up pages from
				 * the backfile and stomping over the changes
				 * we make here.
				 */
				if (cachefs_charge_page(cp, offset) == 0) {
					cachefs_update_allocmap(cp,
					    offset & (offset_t)PAGEMASK,
							(size_t)PAGESIZE);
				}

				/* else we ran out of space */
				else {
					/* nocache file if connected */
					if (fscp->fs_cdconnected ==
					    CFS_CD_CONNECTED)
						cachefs_nocache(cp);
					/*
					 * If disconnected then cannot
					 * nocache the file.  Let it have
					 * the space.
					 */
					else {
						cp->c_metadata.md_frontblks++;
						cp->c_flags |= CN_UPDATED;
						cachefs_update_allocmap(cp,
						    offset & (offset_t)PAGEMASK,
						    (size_t)PAGESIZE);
					}
				}
			}
		}
		mutex_exit(&cp->c_statelock);
		cachefs_cd_release(fscp);
	} while (tcount > 0 && error == 0);

	if (cp->c_flags & CN_CMODINPROG) {
		/* XXX assert error != 0?  FC_ERRNO() makes this more risky. */
		mutex_enter(&cp->c_statelock);
		cp->c_flags &= ~CN_CMODINPROG;
		mutex_exit(&cp->c_statelock);
	}

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_writepage: EXIT error %d\n", error);
#endif

	return (error);
}

/*
 * Pushes out pages to the back and/or front file system.
 */
static int
cachefs_push(vnode_t *vp, page_t *pp, u_offset_t *offp, size_t *lenp,
    int flags, cred_t *cr)
{
	struct cnode *cp = VTOC(vp);
	struct buf *bp;
	int error;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	u_offset_t iooff;
	size_t iolen;
	u_offset_t lbn;
	u_offset_t lbn_off;
	uint_t bsize;

	ASSERT((flags & B_ASYNC) == 0);
	ASSERT(!(vp->v_vfsp->vfs_flag & VFS_RDONLY));
	ASSERT(pp != NULL);
	ASSERT(cr != NULL);

	bsize = MAX(vp->v_vfsp->vfs_bsize, PAGESIZE);
	lbn = pp->p_offset / bsize;
	lbn_off = lbn * bsize;

	/*
	 * Find a kluster that fits in one block, or in
	 * one page if pages are bigger than blocks.  If
	 * there is less file space allocated than a whole
	 * page, we'll shorten the i/o request below.
	 */

	pp = pvn_write_kluster(vp, pp, &iooff, &iolen, lbn_off,
			roundup(bsize, PAGESIZE), flags);

	/*
	 * The CN_CMODINPROG flag makes sure that we use a correct
	 * value of c_size, below.  CN_CMODINPROG is set in
	 * cachefs_writepage().  When CN_CMODINPROG is set it
	 * indicates that a uiomove() is in progress and the c_size
	 * has not been made consistent with the new size of the
	 * file. When the uiomove() completes the c_size is updated
	 * and the CN_CMODINPROG flag is cleared.
	 *
	 * The CN_CMODINPROG flag makes sure that cachefs_push_front
	 * and cachefs_push_connected see a consistent value of
	 * c_size.  Without this handshaking, it is possible that
	 * these routines will pick up the old value of c_size before
	 * the uiomove() in cachefs_writepage() completes.  This will
	 * result in the vn_rdwr() being too small, and data loss.
	 *
	 * More precisely, there is a window between the time the
	 * uiomove() completes and the time the c_size is updated. If
	 * a VOP_PUTPAGE() operation intervenes in this window, the
	 * page will be picked up, because it is dirty; it will be
	 * unlocked, unless it was pagecreate'd. When the page is
	 * picked up as dirty, the dirty bit is reset
	 * (pvn_getdirty()). In cachefs_push_connected(), c_size is
	 * checked.  This will still be the old size.  Therefore, the
	 * page will not be written out to the correct length, and the
	 * page will be clean, so the data may disappear.
	 */
	if (cp->c_flags & CN_CMODINPROG) {
		mutex_enter(&cp->c_statelock);
		if ((cp->c_flags & CN_CMODINPROG) &&
		    cp->c_modaddr + MAXBSIZE > iooff &&
		    cp->c_modaddr < iooff + iolen) {
			page_t *plist;

			/*
			 * A write is in progress for this region of
			 * the file.  If we did not detect
			 * CN_CMODINPROG here then this path through
			 * cachefs_push_connected() would eventually
			 * do the vn_rdwr() and may not write out all
			 * of the data in the pages.  We end up losing
			 * data. So we decide to set the modified bit
			 * on each page in the page list and mark the
			 * cnode with CDIRTY.  This push will be
			 * restarted at some later time.
			 */

			plist = pp;
			while (plist != NULL) {
				pp = plist;
				page_sub(&plist, pp);
				hat_setmod(pp);
				page_io_unlock(pp);
				page_unlock(pp);
			}
			cp->c_flags |= CDIRTY;
			mutex_exit(&cp->c_statelock);
			if (offp)
				*offp = iooff;
			if (lenp)
				*lenp = iolen;
			return (0);
		}
		mutex_exit(&cp->c_statelock);
	}

	/*
	 * Set the pages up for pageout.
	 */
	bp = pageio_setup(pp, iolen, CTOV(cp), B_WRITE | flags);
	if (bp == NULL) {

		/*
		 * currently, there is no way for pageio_setup() to
		 * return NULL, since it uses its own scheme for
		 * kmem_alloc()ing that shouldn't return NULL, and
		 * since pageio_setup() itself dereferences the thing
		 * it's about to return.  still, we need to be ready
		 * in case this ever does start happening.
		 */

		error = ENOMEM;
		goto writedone;
	}
	/*
	 * pageio_setup should have set b_addr to 0.  This
	 * is correct since we want to do I/O on a page
	 * boundary.  bp_mapin will use this addr to calculate
	 * an offset, and then set b_addr to the kernel virtual
	 * address it allocated for us. */
	bp->b_edev = 0;
	bp->b_dev = 0;
	bp->b_lblkno = (diskaddr_t)lbtodb(iooff);
	bp_mapin(bp);

	iolen  = cp->c_size - ldbtob(bp->b_blkno);
	if (iolen > bp->b_bcount)
		iolen  = bp->b_bcount;

	/* if connected */
	if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
		/* write to the back file first */
		error = cachefs_push_connected(vp, bp, iolen, iooff, cr);

		/* write to the front file if allowed */
		if ((error == 0) && CFS_ISFS_NONSHARED(fscp) &&
		    ((cp->c_flags & CN_NOCACHE) == 0)) {
			/* try to write to the front file */
			(void) cachefs_push_front(vp, bp, iolen, iooff, cr);
		}
	}

	/* else if disconnected */
	else {
		/* try to write to the front file */
		error = cachefs_push_front(vp, bp, iolen, iooff, cr);
	}

	bp_mapout(bp);
	pageio_done(bp);

writedone:

	pvn_write_done(pp, ((error) ? B_ERROR : 0) | B_WRITE | flags);
	if (offp)
		*offp = iooff;
	if (lenp)
		*lenp = iolen;

	/* XXX ask bob mastors how to fix this someday */
	mutex_enter(&cp->c_statelock);
	if (error) {
		if (error == ENOSPC) {
			if ((fscp->fs_cdconnected == CFS_CD_CONNECTED) ||
			    CFS_ISFS_SOFT(fscp)) {
				CFSOP_INVALIDATE_COBJECT(fscp, cp, cr);
				cp->c_error = error;
			}
		} else if ((CFS_TIMEOUT(fscp, error) == 0) &&
		    (error != EINTR)) {
			CFSOP_INVALIDATE_COBJECT(fscp, cp, cr);
			cp->c_error = error;
		}
	} else if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
		CFSOP_MODIFY_COBJECT(fscp, cp, cr);
	}
	mutex_exit(&cp->c_statelock);

	return (error);
}

/*
 * Pushes out pages to the back file system.
 */
static int
cachefs_push_connected(vnode_t *vp, struct buf *bp, size_t iolen,
    u_offset_t iooff, cred_t *cr)
{
	struct cnode *cp = VTOC(vp);
	int error = 0;
	int mode = 0;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	ssize_t resid;
	vnode_t *backvp;

	/* get the back file if necessary */
	mutex_enter(&cp->c_statelock);
	if (cp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, cp);
		if (error) {
			mutex_exit(&cp->c_statelock);
			goto out;
		}
	}
	backvp = cp->c_backvp;
	VN_HOLD(backvp);
	mutex_exit(&cp->c_statelock);

	if (CFS_ISFS_NONSHARED(fscp) && CFS_ISFS_SNR(fscp))
		mode = FSYNC;

	/* write to the back file */
	error = bp->b_error = vn_rdwr(UIO_WRITE, backvp, bp->b_un.b_addr,
	    iolen, iooff, UIO_SYSSPACE, mode,
	    RLIM64_INFINITY, cr, &resid);
	if (error) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_VOPS | CFSDEBUG_BACK)
			printf("cachefspush: error %d cr %p\n",
				error, (void *)cr);
#endif
		bp->b_flags |= B_ERROR;
	}
	VN_RELE(backvp);
out:
	return (error);
}

/*
 * Pushes out pages to the front file system.
 * Called for both connected and disconnected states.
 */
static int
cachefs_push_front(vnode_t *vp, struct buf *bp, size_t iolen,
    u_offset_t iooff, cred_t *cr)
{
	struct cnode *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;
	ssize_t resid;
	u_offset_t popoff;
	off_t commit = 0;
	uint_t seq;
	enum cachefs_rl_type type;
	vnode_t *frontvp = NULL;

	mutex_enter(&cp->c_statelock);

	if (!CFS_ISFS_NONSHARED(fscp)) {
		error = ETIMEDOUT;
		goto out;
	}

	/* get the front file if necessary */
	if ((cp->c_frontvp == NULL) &&
	    ((cp->c_flags & CN_NOCACHE) == 0)) {
		(void) cachefs_getfrontfile(cp);
	}
	if (cp->c_flags & CN_NOCACHE) {
		error = ETIMEDOUT;
		goto out;
	}

	/* if disconnected, needs to be populated and have good attributes */
	if ((fscp->fs_cdconnected != CFS_CD_CONNECTED) &&
	    (((cp->c_metadata.md_flags & MD_POPULATED) == 0) ||
	    (cp->c_metadata.md_flags & MD_NEEDATTRS))) {
		error = ETIMEDOUT;
		goto out;
	}

	for (popoff = iooff; popoff < (iooff + iolen); popoff += MAXBSIZE) {
		if (cachefs_charge_page(cp, popoff)) {
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
				cachefs_nocache(cp);
				goto out;
			} else {
				error = ENOSPC;
				goto out;
			}
		}
	}

	if (fscp->fs_cdconnected != CFS_CD_CONNECTED) {
		/* log the first putpage to a file */
		if ((cp->c_metadata.md_flags & MD_PUTPAGE) == 0) {
			/* uses open's creds if we have them */
			if (cp->c_cred)
				cr = cp->c_cred;

			if ((cp->c_metadata.md_flags & MD_MAPPING) == 0) {
				error = cachefs_dlog_cidmap(fscp);
				if (error) {
					error = ENOSPC;
					goto out;
				}
				cp->c_metadata.md_flags |= MD_MAPPING;
			}

			commit = cachefs_dlog_modify(fscp, cp, cr, &seq);
			if (commit == 0) {
				/* out of space */
				error = ENOSPC;
				goto out;
			}

			cp->c_metadata.md_seq = seq;
			type = cp->c_metadata.md_rltype;
			cachefs_modified(cp);
			cp->c_metadata.md_flags |= MD_PUTPAGE;
			cp->c_metadata.md_flags &= ~MD_PUSHDONE;
			cp->c_flags |= CN_UPDATED;
		}

		/* subsequent putpages just get a new sequence number */
		else {
			/* but only if it matters */
			if (cp->c_metadata.md_seq != fscp->fs_dlogseq) {
				seq = cachefs_dlog_seqnext(fscp);
				if (seq == 0) {
					error = ENOSPC;
					goto out;
				}
				cp->c_metadata.md_seq = seq;
				cp->c_flags |= CN_UPDATED;
				/* XXX maybe should do write_metadata here */
			}
		}
	}

	frontvp = cp->c_frontvp;
	VN_HOLD(frontvp);
	mutex_exit(&cp->c_statelock);
	error = bp->b_error = vn_rdwr(UIO_WRITE, frontvp,
	    bp->b_un.b_addr, iolen, iooff, UIO_SYSSPACE, 0,
	    RLIM64_INFINITY, kcred, &resid);
	mutex_enter(&cp->c_statelock);
	VN_RELE(frontvp);
	frontvp = NULL;
	if (error) {
		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			cachefs_nocache(cp);
			error = 0;
			goto out;
		} else {
			goto out;
		}
	}

	(void) cachefs_update_allocmap(cp, iooff, iolen);
	cp->c_flags |= (CN_UPDATED | CN_NEED_FRONT_SYNC |
		CN_POPULATION_PENDING);
	if (fscp->fs_cdconnected != CFS_CD_CONNECTED) {
		cp->c_metadata.md_localmtime = hrestime;
		cp->c_metadata.md_flags |= MD_LOCALMTIME;
	}

out:
	if (commit) {
		/* commit the log record */
		ASSERT(fscp->fs_cdconnected == CFS_CD_DISCONNECTED);
		if (cachefs_dlog_commit(fscp, commit, error)) {
			/*EMPTY*/
			/* XXX fix on panic */
		}
	}

	if (error && commit) {
		cp->c_metadata.md_flags &= ~MD_PUTPAGE;
		cachefs_rlent_moveto(fscp->fs_cache, type,
		    cp->c_metadata.md_rlno, cp->c_metadata.md_frontblks);
		cp->c_metadata.md_rltype = type;
		cp->c_flags |= CN_UPDATED;
	}
	mutex_exit(&cp->c_statelock);
	return (error);
}

/*ARGSUSED*/
static int
cachefs_dump(struct vnode *vp, caddr_t foo1, int foo2, int foo3)
{
	return (ENOSYS); /* should we panic if we get here? */
}

/*ARGSUSED*/
static int
cachefs_ioctl(struct vnode *vp, int cmd, intptr_t arg, int flag, cred_t *cred,
	int *rvalp)
{
	int error;
	struct cnode *cp = VTOC(vp);
	struct fscache *fscp = C_TO_FSCACHE(cp);
	struct cachefscache *cachep;
	extern kmutex_t cachefs_cachelock;
	extern cachefscache_t *cachefs_cachelist;
	cachefsio_pack_t *packp;
	struct cachefs_cnvt_mnt	kccm;	/* LP64: in-kernel data struct */
	STRUCT_DECL(cachefs_cnvt_mnt, ccm);
	STRUCT_DECL(cachefs_boinfo, cboi);
	struct bootobj *bootobjp;
	char *fname, *s;
	STRUCT_DECL(cachefsio_dcmd, dcmd);
	int	inlen, outlen;	/* LP64: generic int for struct in/out len */
	void *dinp, *doutp;
	int (*dcmd_routine)(vnode_t *, void *, void *);

	switch (cmd) {
	case CACHEFSIO_PACK:
		packp = cachefs_kmem_alloc(sizeof (cachefsio_pack_t), KM_SLEEP);
		error = xcopyin((void *)arg, packp, sizeof (cachefsio_pack_t));
		if (!error)
			error = cachefs_pack(vp, packp->p_name, cred);
		cachefs_kmem_free(packp, sizeof (cachefsio_pack_t));
		break;

	case CACHEFSIO_UNPACK:
		packp = cachefs_kmem_alloc(sizeof (cachefsio_pack_t), KM_SLEEP);
		error = xcopyin((void *)arg, packp, sizeof (cachefsio_pack_t));
		if (!error)
			error = cachefs_unpack(vp, packp->p_name, cred);
		cachefs_kmem_free(packp, sizeof (cachefsio_pack_t));
		break;

	case CACHEFSIO_PACKINFO:
		packp = cachefs_kmem_alloc(sizeof (cachefsio_pack_t), KM_SLEEP);
		error = xcopyin((void *)arg, packp, sizeof (cachefsio_pack_t));
		if (!error)
			error = cachefs_packinfo(vp, packp->p_name,
			    &packp->p_status, cred);
		if (!error)
			error = xcopyout(packp, (void *)arg,
					sizeof (cachefsio_pack_t));
		cachefs_kmem_free(packp, sizeof (cachefsio_pack_t));
		break;

	case CACHEFSIO_UNPACKALL:
		error = cachefs_unpackall(vp);
		break;

	case CACHEFSIO_DCMD:
		/*
		 * This is a private interface between the cachefsd and
		 * this file system.
		 */

		/* must be root to use these commands */
		if (!suser(cred))
			return (EPERM);

		/* get the command packet */
		STRUCT_INIT(dcmd, get_udatamodel());
		error = xcopyin((void *)arg, STRUCT_BUF(dcmd),
			SIZEOF_STRUCT(cachefsio_dcmd, DATAMODEL_NATIVE));
		if (error)
			return (error);

		/* copy in the data for the operation */
		dinp = NULL;
		if ((inlen = STRUCT_FGET(dcmd, d_slen)) > 0) {
			dinp = cachefs_kmem_alloc(inlen, KM_SLEEP);
			error = xcopyin(STRUCT_FGETP(dcmd, d_sdata), dinp,
					inlen);
			if (error)
				return (error);
		}

		/* allocate space for the result */
		doutp = NULL;
		if ((outlen = STRUCT_FGET(dcmd, d_rlen)) > 0)
			doutp = cachefs_kmem_alloc(outlen, KM_SLEEP);

		/* get the routine to execute */
		dcmd_routine = NULL;
		switch (STRUCT_FGET(dcmd, d_cmd)) {
		case CFSDCMD_DAEMONID:
			dcmd_routine = cachefs_io_daemonid;
			break;
		case CFSDCMD_STATEGET:
			dcmd_routine = cachefs_io_stateget;
			break;
		case CFSDCMD_STATESET:
			dcmd_routine = cachefs_io_stateset;
			break;
		case CFSDCMD_XWAIT:
			dcmd_routine = cachefs_io_xwait;
			break;
		case CFSDCMD_EXISTS:
			dcmd_routine = cachefs_io_exists;
			break;
		case CFSDCMD_LOSTFOUND:
			dcmd_routine = cachefs_io_lostfound;
			break;
		case CFSDCMD_GETINFO:
			dcmd_routine = cachefs_io_getinfo;
			break;
		case CFSDCMD_CIDTOFID:
			dcmd_routine = cachefs_io_cidtofid;
			break;
		case CFSDCMD_GETATTRFID:
			dcmd_routine = cachefs_io_getattrfid;
			break;
		case CFSDCMD_GETATTRNAME:
			dcmd_routine = cachefs_io_getattrname;
			break;
		case CFSDCMD_GETSTATS:
			dcmd_routine = cachefs_io_getstats;
			break;
		case CFSDCMD_ROOTFID:
			dcmd_routine = cachefs_io_rootfid;
			break;
		case CFSDCMD_CREATE:
			dcmd_routine = cachefs_io_create;
			break;
		case CFSDCMD_REMOVE:
			dcmd_routine = cachefs_io_remove;
			break;
		case CFSDCMD_LINK:
			dcmd_routine = cachefs_io_link;
			break;
		case CFSDCMD_RENAME:
			dcmd_routine = cachefs_io_rename;
			break;
		case CFSDCMD_MKDIR:
			dcmd_routine = cachefs_io_mkdir;
			break;
		case CFSDCMD_RMDIR:
			dcmd_routine = cachefs_io_rmdir;
			break;
		case CFSDCMD_SYMLINK:
			dcmd_routine = cachefs_io_symlink;
			break;
		case CFSDCMD_SETATTR:
			dcmd_routine = cachefs_io_setattr;
			break;
		case CFSDCMD_SETSECATTR:
			dcmd_routine = cachefs_io_setsecattr;
			break;
		case CFSDCMD_PUSHBACK:
			dcmd_routine = cachefs_io_pushback;
			break;
		default:
			error = ENOTTY;
			break;
		}

		/* execute the routine */
		if (dcmd_routine)
			error = (*dcmd_routine)(vp, dinp, doutp);

		/* copy out the result */
		if ((error == 0) && doutp)
			error = xcopyout(doutp, STRUCT_FGETP(dcmd, d_rdata),
					outlen);

		/* free allocated memory */
		if (dinp)
			cachefs_kmem_free(dinp, inlen);
		if (doutp)
			cachefs_kmem_free(doutp, outlen);

		break;

	case _FIOCOD:
		if (!suser(cred)) {
			error = EPERM;
			break;
		}

		error = EBUSY;
		if (arg) {
			/* non-zero arg means do all filesystems */
			mutex_enter(&cachefs_cachelock);
			for (cachep = cachefs_cachelist; cachep != NULL;
			    cachep = cachep->c_next) {
				mutex_enter(&cachep->c_fslistlock);
				for (fscp = cachep->c_fslist;
				    fscp != NULL;
				    fscp = fscp->fs_next) {
					if (CFS_ISFS_CODCONST(fscp)) {
						fscp->fs_cod_time =
						    hrestime;
						error = 0;
					}
				}
				mutex_exit(&cachep->c_fslistlock);
			}
			mutex_exit(&cachefs_cachelock);
		} else {
			if (CFS_ISFS_CODCONST(fscp)) {
				fscp->fs_cod_time = hrestime;
				error = 0;
			}
		}
		break;

	case _FIOCNVTMNT:
		if (!suser(cred)) {
			error = EPERM;
			break;
		}

		/* Now copyin the convert structure */
		STRUCT_INIT(ccm, get_udatamodel());
		error = xcopyin((void *)arg, STRUCT_BUF(ccm),
			SIZEOF_STRUCT(cachefs_cnvt_mnt, DATAMODEL_NATIVE));

		if (error)
			break;

		kccm.cm_op = STRUCT_FGET(ccm, cm_op);
		if ((kccm.cm_op != CFS_CM_BACK) &&
		    (kccm.cm_op != CFS_CM_FRONT)) {
			error = EINVAL;
			break;
		}

		if ((inlen = STRUCT_FGET(ccm, cm_namelen)) == 0) {
			error = EINVAL;
			break;
		}

		/* Now copyin the name */
		fname = cachefs_kmem_alloc(inlen, KM_SLEEP);
		if (copyin(STRUCT_FGETP(ccm, cm_name), fname, inlen)) {
			cachefs_kmem_free(fname, inlen);
			error = EINVAL;
			break;
		}
		kccm.cm_name = fname;
		kccm.cm_namelen = inlen;
		error = cachefs_convert_mount(fscp, &kccm);
		cachefs_kmem_free(fname, inlen);
		break;

	case _FIOBOINFO:
		if (!suser(cred)) {
			error = EPERM;
			break;
		}

		/* Now copyin the bootobj info structure */
		STRUCT_INIT(cboi, get_udatamodel());
		error = xcopyin((void *)arg, STRUCT_BUF(cboi),
			SIZEOF_STRUCT(cachefs_boinfo, DATAMODEL_NATIVE));
		if (error)
			break;

		/* get the piece we're interested in and copy it back out */
		switch (STRUCT_FGET(cboi, boi_which)) {
		case CFS_BOI_ROOTFS:
			bootobjp = &rootfs;
			break;
		case CFS_BOI_FRONTFS:
			bootobjp = &frontfs;
			break;
		case CFS_BOI_BACKFS:
			bootobjp = &backfs;
			break;
		default:
			error = EINVAL;
			break;
		}
		if (error)
			break;

		s = STRUCT_FGET(cboi, boi_device) ? bootobjp->bo_name :
			bootobjp->bo_fstype;
		inlen = (int)strlen(s) + 1;
		inlen = MIN(inlen, STRUCT_FGET(cboi, boi_len));
		error = xcopyout(s, STRUCT_FGETP(cboi, boi_value), inlen);
		break;

	case _FIOSTOPCACHE:
		error = cachefs_stop_cache(cp);
		break;

	default:
		error = ENOTTY;
		break;
	}

	/* return the result */
	return (error);
}

ino64_t
cachefs_fileno_conflict(fscache_t *fscp, ino64_t old)
{
	ino64_t new;

	ASSERT(MUTEX_HELD(&fscp->fs_fslock));

	for (;;) {
		fscp->fs_info.fi_localfileno++;
		if (fscp->fs_info.fi_localfileno == 0)
			fscp->fs_info.fi_localfileno = 3;
		fscp->fs_flags |= CFS_FS_DIRTYINFO;

		new = fscp->fs_info.fi_localfileno;
		if (! cachefs_fileno_inuse(fscp, new))
			break;
	}

	cachefs_inum_register(fscp, old, new);
	cachefs_inum_register(fscp, new, 0);
	return (new);
}

/*ARGSUSED*/
static int
cachefs_getattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *cr)
{
	struct cnode *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;
	int held = 0;
	int connected = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_getattr: ENTER vp %p\n", (void *)vp);
#endif

	/*
	 * If it has been specified that the return value will
	 * just be used as a hint, and we are only being asked
	 * for size, fsid or rdevid, then return the client's
	 * notion of these values without checking to make sure
	 * that the attribute cache is up to date.
	 * The whole point is to avoid an over the wire GETATTR
	 * call.
	 */
	if (flags & ATTR_HINT) {
		if (vap->va_mask ==
		    (vap->va_mask & (AT_SIZE | AT_FSID | AT_RDEV))) {
			if (vap->va_mask | AT_SIZE)
				vap->va_size = cp->c_size;
			/*
			 * Return the FSID of the cachefs filesystem,
			 * not the back filesystem
			 */
			if (vap->va_mask | AT_FSID)
				vap->va_fsid = vp->v_vfsp->vfs_dev;
			if (vap->va_mask | AT_RDEV)
				vap->va_rdev = cp->c_attr.va_rdev;
			return (0);
		}
	}

	/*
	 * Only need to flush pages if asking for the mtime
	 * and if there any dirty pages.
	 */
	if (vap->va_mask & AT_MTIME) {
		/*EMPTY*/
#if 0
		/*
		 * XXX bob: stolen from nfs code, need to do something similar
		 */
		rp = VTOR(vp);
		if ((rp->r_flags & RDIRTY) || rp->r_iocnt > 0)
			(void) nfs3_putpage(vp, (offset_t)0, 0, 0, cr);
#endif
	}

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			goto out;
		held = 1;

		/*
		 * If it has been specified that the return value will
		 * just be used as a hint, and we are only being asked
		 * for size, fsid or rdevid, then return the client's
		 * notion of these values without checking to make sure
		 * that the attribute cache is up to date.
		 * The whole point is to avoid an over the wire GETATTR
		 * call.
		 */
		if (flags & ATTR_HINT) {
			if (vap->va_mask ==
			    (vap->va_mask & (AT_SIZE | AT_FSID | AT_RDEV))) {
				if (vap->va_mask | AT_SIZE)
					vap->va_size = cp->c_size;
				/*
				 * Return the FSID of the cachefs filesystem,
				 * not the back filesystem
				 */
				if (vap->va_mask | AT_FSID)
					vap->va_fsid = vp->v_vfsp->vfs_dev;
				if (vap->va_mask | AT_RDEV)
					vap->va_rdev = cp->c_attr.va_rdev;
				goto out;
			}
		}

		mutex_enter(&cp->c_statelock);
		if ((cp->c_metadata.md_flags & MD_NEEDATTRS) &&
		    (fscp->fs_cdconnected != CFS_CD_CONNECTED)) {
			mutex_exit(&cp->c_statelock);
			connected = 1;
			continue;
		}

		error = CFSOP_CHECK_COBJECT(fscp, cp, 0, cr);
		if (CFS_TIMEOUT(fscp, error)) {
			mutex_exit(&cp->c_statelock);
			cachefs_cd_release(fscp);
			held = 0;
			cachefs_cd_timedout(fscp);
			continue;
		}
		if (error) {
			mutex_exit(&cp->c_statelock);
			break;
		}

		/* check for fileno conflict */
		if ((fscp->fs_inum_size > 0) &&
		    ((cp->c_metadata.md_flags & MD_LOCALFILENO) == 0)) {
			ino64_t fakenum;

			mutex_exit(&cp->c_statelock);
			mutex_enter(&fscp->fs_fslock);
			fakenum = cachefs_inum_real2fake(fscp,
			    cp->c_attr.va_nodeid);
			if (fakenum == 0) {
				fakenum = cachefs_fileno_conflict(fscp,
				    cp->c_attr.va_nodeid);
			}
			mutex_exit(&fscp->fs_fslock);

			mutex_enter(&cp->c_statelock);
			cp->c_metadata.md_flags |= MD_LOCALFILENO;
			cp->c_metadata.md_localfileno = fakenum;
			cp->c_flags |= CN_UPDATED;
		}

		/* copy out the attributes */
		*vap = cp->c_attr;

		/*
		 * return the FSID of the cachefs filesystem,
		 * not the back filesystem
		 */
		vap->va_fsid = vp->v_vfsp->vfs_dev;

		/* return our idea of the size */
		if (cp->c_size > vap->va_size)
			vap->va_size = cp->c_size;

		/* overwrite with our version of fileno and timestamps */
		vap->va_nodeid = cp->c_metadata.md_localfileno;
		vap->va_mtime = cp->c_metadata.md_localmtime;
		vap->va_ctime = cp->c_metadata.md_localctime;

		mutex_exit(&cp->c_statelock);
		break;
	}
out:
	if (held)
		cachefs_cd_release(fscp);
#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_getattr: EXIT error = %d\n", error);
#endif
	return (error);
}

static int
cachefs_setattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error;
	int connected;
	int held = 0;

	connected = 0;
	for (;;) {
		/* drop hold on file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}

		/* aquire access to the file system */
		error = cachefs_cd_access(fscp, connected, 1);
		if (error)
			break;
		held = 1;

		/* perform the setattr */
		error = cachefs_setattr_common(vp, vap, flags, cr);
		if (error) {
			/* if connected */
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
				if (CFS_TIMEOUT(fscp, error)) {
					cachefs_cd_release(fscp);
					held = 0;
					cachefs_cd_timedout(fscp);
					connected = 0;
					continue;
				}
			}

			/* else must be disconnected */
			else {
				if (CFS_TIMEOUT(fscp, error)) {
					connected = 1;
					continue;
				}
			}
		}
		break;
	}

	if (held) {
		cachefs_cd_release(fscp);
	}
#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
	return (error);
}

static int
cachefs_setattr_common(vnode_t *vp, vattr_t *vap, int flags, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	cachefscache_t *cachep = fscp->fs_cache;
	uint_t mask = vap->va_mask;
	int error = 0;
	uint_t bcnt;

	/* Cannot set these attributes. */
	if (mask & AT_NOSET)
		return (EINVAL);

	/*
	 * Truncate file.  Must have write permission and not be a directory.
	 */
	if (mask & AT_SIZE) {
		if (vp->v_type == VDIR) {
			if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_TRUNCATE))
				cachefs_log_truncate(cachep, EISDIR,
				    fscp->fs_cfsvfsp,
				    &cp->c_metadata.md_cookie,
				    cp->c_id.cid_fileno,
				    cr->cr_uid, vap->va_size);
			return (EISDIR);
		}
	}

	/*
	 * Gotta deal with one special case here, where we're setting the
	 * size of the file. First, we zero out part of the page after the
	 * new size of the file. Then we toss (not write) all pages after
	 * page in which the new offset occurs. Note that the NULL passed
	 * in instead of a putapage() fn parameter is correct, since
	 * no dirty pages will be found (B_TRUNC | B_INVAL).
	 */

	rw_enter(&cp->c_rwlock, RW_WRITER);

	/* sync dirty pages */
	error = cachefs_putpage_common(vp, (offset_t)0, 0, 0, cr);
	if (error == EINTR)
		goto out;
	error = 0;

	/* if connected */
	if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
		error = cachefs_setattr_connected(vp, vap, flags, cr);
	}
	/* else must be disconnected */
	else {
		error = cachefs_setattr_disconnected(vp, vap, flags, cr);
	}
	if (error)
		goto out;

	/*
	 * If the file size has been changed then
	 * toss whole pages beyond the end of the file and zero
	 * the portion of the last page that is beyond the end of the file.
	 */
	if (mask & AT_SIZE) {
		bcnt = (uint_t)(cp->c_size & PAGEOFFSET);
		if (bcnt)
			pvn_vpzero(vp, cp->c_size, PAGESIZE - bcnt);
		(void) pvn_vplist_dirty(vp, cp->c_size, cachefs_push,
			B_TRUNC | B_INVAL, cr);
	}

out:
	rw_exit(&cp->c_rwlock);

	if ((mask & AT_SIZE) &&
	    (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_TRUNCATE)))
		cachefs_log_truncate(cachep, error, fscp->fs_cfsvfsp,
		    &cp->c_metadata.md_cookie, cp->c_id.cid_fileno, cr->cr_uid,
		    vap->va_size);

	return (error);
}

static int
cachefs_setattr_connected(vnode_t *vp, vattr_t *vap, int flags, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	uint_t mask = vap->va_mask;
	int error = 0;
	int setsize;

	mutex_enter(&cp->c_statelock);

	if (cp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, cp);
		if (error)
			goto out;
	}

	error = CFSOP_CHECK_COBJECT(fscp, cp, 0, cr);
	if (error)
		goto out;

	error = VOP_SETATTR(cp->c_backvp, vap, flags, cr);
	if (error) {
		goto out;
	}

	/* if the size of the file is being changed */
	if (mask & AT_SIZE) {
		cp->c_size = vap->va_size;
		error = 0;
		setsize = 0;

		/* see if okay to try to set the file size */
		if (((cp->c_flags & CN_NOCACHE) == 0) &&
		    CFS_ISFS_NONSHARED(fscp)) {
			/* okay to set size if file is populated */
			if (cp->c_metadata.md_flags & MD_POPULATED)
				setsize = 1;

			/*
			 * Okay to set size if front file exists and setting
			 * file size to zero.
			 */
			if ((cp->c_metadata.md_flags & MD_FILE) &&
			    (vap->va_size == 0))
				setsize = 1;
		}

		/* if okay to try to set the file size */
		if (setsize) {
			error = 0;
			if (cp->c_frontvp == NULL)
				error = cachefs_getfrontfile(cp);
			if (error == 0)
				error = cachefs_frontfile_size(cp, cp->c_size);
		} else if (cp->c_metadata.md_flags & MD_FILE) {
			/* make sure file gets nocached */
			error = EEXIST;
		}

		/* if we have to nocache the file */
		if (error) {
			if ((cp->c_flags & CN_NOCACHE) == 0)
				cachefs_nocache(cp);
			error = 0;
		}
	}

	cp->c_flags |= CN_UPDATED;

	/* XXX bob: given what modify_cobject does this seems unnecessary */
	cp->c_attr.va_mask = AT_ALL;
	error = VOP_GETATTR(cp->c_backvp, &cp->c_attr, 0, cr);
	if (error)
		goto out;

	cp->c_attr.va_size = MAX(cp->c_attr.va_size, cp->c_size);
	cp->c_size = cp->c_attr.va_size;

	CFSOP_MODIFY_COBJECT(fscp, cp, cr);
out:
	mutex_exit(&cp->c_statelock);
	return (error);
}

/*
 * perform the setattr on the local file system
 */
static int
cachefs_setattr_disconnected(vnode_t *vp, vattr_t *vap, int flags,
    cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int mask = vap->va_mask;
	int error = 0;
	int checksu;
	int newfile;
	off_t commit = 0;

	if (CFS_ISFS_WRITE_AROUND(fscp))
		return (ETIMEDOUT);


	/* if we do not have good attributes */
	if (cp->c_metadata.md_flags & MD_NEEDATTRS)
		return (ETIMEDOUT);

	/* primary concern is to keep this routine as much like ufs_setattr */

	mutex_enter(&cp->c_statelock);

	/* if changing the size of the file */
	if (mask & AT_SIZE) {
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		}

		/* do permissions check */
		error = cachefs_access_local(cp, VWRITE, cr);
		if (error)
			goto out;

		if (vp->v_type == VFIFO) {
			error = 0;
			goto out;
		}

		if ((vp->v_type != VREG) &&
		    !((vp->v_type == VLNK) && (vap->va_size == 0))) {
			error = EINVAL;
			goto out;
		}

		if (vap->va_size > fscp->fs_offmax) {
			error = EFBIG;
			goto out;
		}

		/* if the file is not populated and we are not truncating it */
		if (((cp->c_metadata.md_flags & MD_POPULATED) == 0) &&
		    (vap->va_size != 0)) {
			error = ETIMEDOUT;
			goto out;
		}

		if ((cp->c_metadata.md_flags & MD_MAPPING) == 0) {
			error = cachefs_dlog_cidmap(fscp);
			if (error) {
				error = ENOSPC;
				goto out;
			}
			cp->c_metadata.md_flags |= MD_MAPPING;
		}

		/* log the operation */
		commit = cachefs_dlog_setattr(fscp, vap, flags, cp, cr);
		if (commit == 0) {
			error = ENOSPC;
			goto out;
		}
		cp->c_flags &= ~CN_NOCACHE;

		/* special case truncating fast sym links */
		if ((vp->v_type == VLNK) &&
		    (cp->c_metadata.md_flags & MD_FASTSYMLNK)) {
			/* XXX how can we get here */
			/* XXX should update mtime */
			cp->c_size = 0;
			error = 0;
			goto out;
		}

		/* get the front file, this may create one */
		newfile = (cp->c_metadata.md_flags & MD_FILE) ? 0 : 1;
		if (cp->c_frontvp == NULL) {
			error = cachefs_getfrontfile(cp);
			if (error)
				goto out;
		}
		ASSERT(cp->c_frontvp);
		if (newfile && (cp->c_flags & CN_UPDATED)) {
			/* allocate space for the metadata */
			ASSERT((cp->c_flags & CN_ALLOC_PENDING) == 0);
			ASSERT((cp->c_filegrp->fg_flags & CFS_FG_ALLOC_ATTR)
			    == 0);
			error = filegrp_write_metadata(cp->c_filegrp,
			    &cp->c_id, &cp->c_metadata);
			if (error)
				goto out;
		}

		/* change the size of the front file */
		error = cachefs_frontfile_size(cp, vap->va_size);
		if (error)
			goto out;
		cp->c_attr.va_size = cp->c_size = vap->va_size;
		cp->c_metadata.md_localmtime = hrestime;
		cp->c_metadata.md_flags |= MD_POPULATED | MD_LOCALMTIME;
		cachefs_modified(cp);
		cp->c_flags |= CN_UPDATED;
	}

	if (mask & AT_MODE) {
		if (cr->cr_uid != cp->c_attr.va_uid && !suser(cr)) {
			error = EPERM;
			goto out;
		}

		/* mark as modified */
		if (cachefs_modified_alloc(cp)) {
			error = ENOSPC;
			goto out;
		}

		if ((cp->c_metadata.md_flags & MD_MAPPING) == 0) {
			error = cachefs_dlog_cidmap(fscp);
			if (error) {
				error = ENOSPC;
				goto out;
			}
			cp->c_metadata.md_flags |= MD_MAPPING;
		}

		/* log the operation if not already logged */
		if (commit == 0) {
			commit = cachefs_dlog_setattr(fscp, vap, flags, cp, cr);
			if (commit == 0) {
				error = ENOSPC;
				goto out;
			}
		}

		cp->c_attr.va_mode &= S_IFMT;
		cp->c_attr.va_mode |= vap->va_mode & ~S_IFMT;
		if (!suser(cr)) {
			if (vp->v_type != VDIR)
				cp->c_attr.va_mode &= ~S_ISVTX;
			if (!groupmember(cp->c_attr.va_gid, cr))
				cp->c_attr.va_mode &= ~S_ISGID;
		}
		cp->c_metadata.md_localctime = hrestime;
		cp->c_metadata.md_flags |= MD_LOCALCTIME;
		cp->c_flags |= CN_UPDATED;
	}

	if (mask & (AT_UID|AT_GID)) {
		/*
		 * to change ownership, the requestor must either own
		 * the file or be root.
		 */
		checksu = 0;
		if (cr->cr_uid != cp->c_attr.va_uid) {
			checksu = 1;
		} else if (rstchown) {
			/*
			 * "chown" is restricted.  A process not
			 * running as super-user cannot change the
			 * owner, and can only change the group to a
			 * group of which it's currently a member.
			 */
			if (((mask & AT_UID) &&
			    vap->va_uid != cp->c_attr.va_uid) ||
			    ((mask & AT_GID) &&
			    !groupmember(vap->va_gid, cr))) {
				checksu = 1;
			}
		}

		if (checksu && !suser(cr)) {
			error = EPERM;
			goto out;
		}

		/* mark as modified */
		if (cachefs_modified_alloc(cp)) {
			error = ENOSPC;
			goto out;
		}

		if ((cp->c_metadata.md_flags & MD_MAPPING) == 0) {
			error = cachefs_dlog_cidmap(fscp);
			if (error) {
				error = ENOSPC;
				goto out;
			}
			cp->c_metadata.md_flags |= MD_MAPPING;
		}

		/* log the operation if not already logged */
		if (commit == 0) {
			commit = cachefs_dlog_setattr(fscp, vap, flags, cp, cr);
			if (commit == 0) {
				error = ENOSPC;
				goto out;
			}
		}

		if (!suser(cr))
			cp->c_attr.va_mode &= ~(S_ISUID|S_ISGID);

		if (mask & AT_UID)
			cp->c_attr.va_uid = vap->va_uid;

		if (mask & AT_GID)
			cp->c_attr.va_gid = vap->va_gid;
		cp->c_metadata.md_localctime = hrestime;
		cp->c_metadata.md_flags |= MD_LOCALCTIME;
		cp->c_flags |= CN_UPDATED;
	}


	if (mask & (AT_MTIME|AT_ATIME)) {
		if (cr->cr_uid != cp->c_attr.va_uid && !suser(cr)) {
			if (flags & ATTR_UTIME)
				error = EPERM;
			else
				error = cachefs_access_local(cp, VWRITE, cr);
			if (error)
				goto out;
		}

		/* mark as modified */
		if (cachefs_modified_alloc(cp)) {
			error = ENOSPC;
			goto out;
		}

		if ((cp->c_metadata.md_flags & MD_MAPPING) == 0) {
			error = cachefs_dlog_cidmap(fscp);
			if (error) {
				error = ENOSPC;
				goto out;
			}
			cp->c_metadata.md_flags |= MD_MAPPING;
		}

		/* log the operation if not already logged */
		if (commit == 0) {
			commit = cachefs_dlog_setattr(fscp, vap, flags, cp, cr);
			if (commit == 0) {
				error = ENOSPC;
				goto out;
			}
		}

		if (mask & AT_MTIME) {
			cp->c_metadata.md_localmtime = vap->va_mtime;
			cp->c_metadata.md_flags |= MD_LOCALMTIME;
		}
		if (mask & AT_ATIME)
			cp->c_attr.va_atime = vap->va_atime;
		cp->c_metadata.md_localctime = hrestime;
		cp->c_metadata.md_flags |= MD_LOCALCTIME;
		cp->c_flags |= CN_UPDATED;
	}

out:
	mutex_exit(&cp->c_statelock);

	/* commit the log entry */
	if (commit) {
		if (cachefs_dlog_commit(fscp, commit, error)) {
			/*EMPTY*/
			/* XXX bob: fix on panic */
		}
	}
	return (error);
}

/* ARGSUSED */
static int
cachefs_access(vnode_t *vp, int mode, int flags, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error;
	int held = 0;
	int connected = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_access: ENTER vp %p\n", (void *)vp);
#endif
	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			break;
		held = 1;

		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			error = cachefs_access_connected(vp, mode, flags,
			    cr);
			if (CFS_TIMEOUT(fscp, error)) {
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			}
		} else {
			mutex_enter(&cp->c_statelock);
			error = cachefs_access_local(cp, mode, cr);
			mutex_exit(&cp->c_statelock);
			if (CFS_TIMEOUT(fscp, error)) {
				if (cachefs_cd_access_miss(fscp)) {
					mutex_enter(&cp->c_statelock);
					if (cp->c_backvp == NULL) {
						(void) cachefs_getbackvp(fscp,
						    cp);
					}
					mutex_exit(&cp->c_statelock);
					error = cachefs_access_connected(vp,
					    mode, flags, cr);
					if (!CFS_TIMEOUT(fscp, error))
						break;
					delay(5*hz);
					connected = 0;
					continue;
				}
				connected = 1;
				continue;
			}
		}
		break;
	}
	if (held)
		cachefs_cd_release(fscp);
#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_access: EXIT error = %d\n", error);
#endif
	return (error);
}

static int
cachefs_access_connected(struct vnode *vp, int mode, int flags, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;

	mutex_enter(&cp->c_statelock);

	/* Make sure the cnode attrs are valid first. */
	error = CFSOP_CHECK_COBJECT(fscp, cp, 0, cr);
	if (error)
		goto out;

	/* see if can do a local file system check */
	if ((fscp->fs_info.fi_mntflags & CFS_ACCESS_BACKFS) == 0) {
		error = cachefs_access_local(cp, mode, cr);
		goto out;
	}

	/* else do a remote file system check */
	else {
		if (cp->c_backvp == NULL) {
			error = cachefs_getbackvp(fscp, cp);
			if (error)
				goto out;
		}
		error = VOP_ACCESS(cp->c_backvp, mode, flags, cr);

		/*
		 * even though we don't `need' the ACL to do access
		 * via the backvp, we should cache it here to make our
		 * behavior more reasonable if we go disconnected.
		 */

		if (((fscp->fs_info.fi_mntflags & CFS_NOACL) == 0) &&
		    (cachefs_vtype_aclok(vp)) &&
		    ((cp->c_flags & CN_NOCACHE) == 0) &&
		    ((cp->c_metadata.md_flags & MD_ACL) == 0))
			(void) cachefs_cacheacl(cp, NULL);
	}
out:
	/*
	 * If NFS returned ESTALE, mark this cnode as stale, so that
	 * the vn_open retry will read the file anew from backfs
	 */
	if (error == ESTALE)
		cachefs_cnode_stale(cp);

	mutex_exit(&cp->c_statelock);
	return (error);
}

/*
 * CFS has a fastsymlink scheme. If the size of the link is < C_FSL_SIZE, then
 * the link is placed in the metadata itself (no front file is allocated).
 */
static int
cachefs_readlink(vnode_t *vp, uio_t *uiop, cred_t *cr)
{
	int error = 0;
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	cachefscache_t *cachep = fscp->fs_cache;
	int held = 0;
	int connected = 0;

	if (vp->v_type != VLNK)
		return (EINVAL);

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			break;
		held = 1;

		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			error = cachefs_readlink_connected(vp, uiop, cr);
			if (CFS_TIMEOUT(fscp, error)) {
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			}
		} else {
			error = cachefs_readlink_disconnected(vp, uiop);
			if (CFS_TIMEOUT(fscp, error)) {
				if (cachefs_cd_access_miss(fscp)) {
					error = cachefs_readlink_connected(vp,
					    uiop, cr);
					if (!CFS_TIMEOUT(fscp, error))
						break;
					delay(5*hz);
					connected = 0;
					continue;
				}
				connected = 1;
				continue;
			}
		}
		break;
	}
	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_READLINK))
		cachefs_log_readlink(cachep, error, fscp->fs_cfsvfsp,
		    &cp->c_metadata.md_cookie, cp->c_id.cid_fileno,
		    cr->cr_uid, cp->c_size);

	if (held)
		cachefs_cd_release(fscp);
#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif

	/*
	 * The over the wire error for attempting to readlink something
	 * other than a symbolic link is ENXIO.  However, we need to
	 * return EINVAL instead of ENXIO, so we map it here.
	 */
	return (error == ENXIO ? EINVAL : error);
}

static int
cachefs_readlink_connected(vnode_t *vp, uio_t *uiop, cred_t *cr)
{
	int error;
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	caddr_t buf;
	int buflen;
	int readcache = 0;

	mutex_enter(&cp->c_statelock);

	error = CFSOP_CHECK_COBJECT(fscp, cp, 0, cr);
	if (error)
		goto out;

	/* if the sym link is cached as a fast sym link */
	if (cp->c_metadata.md_flags & MD_FASTSYMLNK) {
		error = uiomove(cp->c_metadata.md_allocinfo,
		    MIN(cp->c_size, uiop->uio_resid), UIO_READ, uiop);
#ifdef CFSDEBUG
		readcache = 1;
		goto out;
#else /* CFSDEBUG */
		/* XXX KLUDGE! correct for insidious 0-len symlink */
		if (cp->c_size != 0) {
			readcache = 1;
			goto out;
		}
#endif /* CFSDEBUG */
	}

	/* if the sym link is cached in a front file */
	if (cp->c_metadata.md_flags & MD_POPULATED) {
		ASSERT(cp->c_metadata.md_flags & MD_FILE);
		if (cp->c_frontvp == NULL) {
			(void) cachefs_getfrontfile(cp);
		}
		if (cp->c_metadata.md_flags & MD_POPULATED) {
			/* read symlink data from frontfile */
			uiop->uio_offset = 0;
			VOP_RWLOCK(cp->c_frontvp, 0);
			error = VOP_READ(cp->c_frontvp, uiop, 0, kcred);
			VOP_RWUNLOCK(cp->c_frontvp, 0);

			/* XXX KLUDGE! correct for insidious 0-len symlink */
			if (cp->c_size != 0) {
				readcache = 1;
				goto out;
			}
		}
	}

	/* get the sym link contents from the back fs */
	error = cachefs_readlink_back(cp, cr, &buf, &buflen);
	if (error)
		goto out;

	/* copy the contents out to the user */
	error = uiomove(buf, MIN(buflen, uiop->uio_resid), UIO_READ, uiop);

	/* try to cache the sym link */
	if (cachefs_stuffsymlink(cp, buf, buflen)) {
		cachefs_nocache(cp);
	}

	cachefs_kmem_free(buf, MAXPATHLEN);

out:
	mutex_exit(&cp->c_statelock);
	if (error == 0) {
		if (readcache)
			fscp->fs_stats.st_hits++;
		else
			fscp->fs_stats.st_misses++;
	}
	return (error);
}

static int
cachefs_readlink_disconnected(vnode_t *vp, uio_t *uiop)
{
	int error;
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int readcache = 0;

	mutex_enter(&cp->c_statelock);

	/* if the sym link is cached as a fast sym link */
	if (cp->c_metadata.md_flags & MD_FASTSYMLNK) {
		error = uiomove(cp->c_metadata.md_allocinfo,
		    MIN(cp->c_size, uiop->uio_resid), UIO_READ, uiop);
		readcache = 1;
		goto out;
	}

	/* if the sym link is cached in a front file */
	if (cp->c_metadata.md_flags & MD_POPULATED) {
		ASSERT(cp->c_metadata.md_flags & MD_FILE);
		if (cp->c_frontvp == NULL) {
			(void) cachefs_getfrontfile(cp);
		}
		if (cp->c_metadata.md_flags & MD_POPULATED) {
			/* read symlink data from frontfile */
			uiop->uio_offset = 0;
			VOP_RWLOCK(cp->c_frontvp, 0);
			error = VOP_READ(cp->c_frontvp, uiop, 0, kcred);
			VOP_RWUNLOCK(cp->c_frontvp, 0);
			readcache = 1;
			goto out;
		}
	}
	error = ETIMEDOUT;

out:
	mutex_exit(&cp->c_statelock);
	if (error == 0) {
		if (readcache)
			fscp->fs_stats.st_hits++;
		else
			fscp->fs_stats.st_misses++;
	}
	return (error);
}

/*ARGSUSED*/
static int
cachefs_fsync(vnode_t *vp, int syncflag, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int held = 0;
	int connected = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_fsync: ENTER vp %p\n", (void *)vp);
#endif

	if (fscp->fs_backvfsp && fscp->fs_backvfsp->vfs_flag & VFS_RDONLY)
		goto out;

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 1);
		if (error)
			break;
		held = 1;
		connected = 0;

		/* if a regular file, write out the pages */
		if ((vp->v_type == VREG) && (vp->v_pages)) {
			error = cachefs_putpage_common(vp, (offset_t)0,
			    0, 0, cr);
			if (CFS_TIMEOUT(fscp, error)) {
				if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
					cachefs_cd_release(fscp);
					held = 0;
					cachefs_cd_timedout(fscp);
					continue;
				} else {
					connected = 1;
					continue;
				}
			}

			/* if no space left in cache, wait until connected */
			if ((error == ENOSPC) &&
			    (fscp->fs_cdconnected != CFS_CD_CONNECTED)) {
				connected = 1;
				continue;
			}

			/* clear the cnode error if putpage worked */
			if ((error == 0) && cp->c_error) {
				mutex_enter(&cp->c_statelock);
				cp->c_error = 0;
				mutex_exit(&cp->c_statelock);
			}

			if (error)
				break;
		}

		/* if connected, sync the backvp */
		if ((fscp->fs_cdconnected == CFS_CD_CONNECTED) &&
		    cp->c_backvp) {
			mutex_enter(&cp->c_statelock);
			if (cp->c_backvp) {
				error = VOP_FSYNC(cp->c_backvp, syncflag, cr);
				if (CFS_TIMEOUT(fscp, error)) {
					mutex_exit(&cp->c_statelock);
					cachefs_cd_release(fscp);
					held = 0;
					cachefs_cd_timedout(fscp);
					continue;
				} else if (error && (error != EINTR))
					cp->c_error = error;
			}
			mutex_exit(&cp->c_statelock);
		}

		/* sync the metadata and the front file to the front fs */
		error = cachefs_sync_metadata(cp);
		if (error && (fscp->fs_cdconnected == CFS_CD_CONNECTED))
			error = 0;
		break;
	}

	if (error == 0)
		error = cp->c_error;

	if (held)
		cachefs_cd_release(fscp);

out:
#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_fsync: EXIT vp %p\n", (void *)vp);
#endif
	return (error);
}

/*
 * Called from cachefs_inactive(), to make sure all the data goes out to disk.
 */
int
cachefs_sync_metadata(cnode_t *cp)
{
	int error = 0;
	struct filegrp *fgp;
	struct vattr va;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("c_sync_metadata: ENTER cp %p cflag %x\n",
			(void *)cp, cp->c_flags);
#endif

	mutex_enter(&cp->c_statelock);
	if ((cp->c_flags & CN_UPDATED) == 0)
		goto out;
	if (cp->c_flags & (CN_STALE | CN_DESTROY))
		goto out;
	fgp = cp->c_filegrp;
	if ((fgp->fg_flags & CFS_FG_WRITE) == 0)
		goto out;

	if (fgp->fg_flags & CFS_FG_ALLOC_ATTR) {
		mutex_exit(&cp->c_statelock);
		error = filegrp_allocattr(fgp);
		mutex_enter(&cp->c_statelock);
		if (error) {
			error = 0;
			goto out;
		}
	}

	if (cp->c_flags & CN_ALLOC_PENDING) {
		error = filegrp_create_metadata(fgp, &cp->c_metadata,
		    &cp->c_id);
		if (error)
			goto out;
		cp->c_flags &= ~CN_ALLOC_PENDING;
	}

	if (cp->c_flags & CN_NEED_FRONT_SYNC) {
		if (cp->c_frontvp != NULL) {
			error = VOP_FSYNC(cp->c_frontvp, FSYNC, kcred);
			if (error) {
				cp->c_metadata.md_timestamp.tv_sec = 0;
			} else {
				va.va_mask = AT_MTIME;
				error = VOP_GETATTR(cp->c_frontvp, &va, 0,
				    kcred);
				if (error)
					goto out;
				cp->c_metadata.md_timestamp = va.va_mtime;
				cp->c_flags &=
				~(CN_NEED_FRONT_SYNC | CN_POPULATION_PENDING);
			}
		} else {
			cp->c_flags &=
				~(CN_NEED_FRONT_SYNC | CN_POPULATION_PENDING);
		}
	}

	/*
	 * XXX tony: How can CN_ALLOC_PENDING still be set??
	 * XXX tony: How can CN_UPDATED not be set?????
	 */
	if ((cp->c_flags & CN_ALLOC_PENDING) == 0 &&
			(cp->c_flags & CN_UPDATED)) {
		error = filegrp_write_metadata(fgp, &cp->c_id,
				&cp->c_metadata);
		if (error)
			goto out;
	}
out:
	if (error) {
		/* XXX modified files? */
		if (cp->c_metadata.md_rlno) {
			cachefs_removefrontfile(&cp->c_metadata,
			    &cp->c_id, fgp);
			cachefs_rlent_moveto(C_TO_FSCACHE(cp)->fs_cache,
			    CACHEFS_RL_FREE, cp->c_metadata.md_rlno, 0);
			cp->c_metadata.md_rlno = 0;
			cp->c_metadata.md_rltype = CACHEFS_RL_NONE;
			if (cp->c_frontvp) {
				VN_RELE(cp->c_frontvp);
				cp->c_frontvp = NULL;
			}
		}
		if ((cp->c_flags & CN_ALLOC_PENDING) == 0)
			(void) filegrp_destroy_metadata(fgp, &cp->c_id);
		cp->c_flags |= CN_ALLOC_PENDING;
		cachefs_nocache(cp);
	}
	/*
	 * we clear the updated bit even on errors because a retry
	 * will probably fail also.
	 */
	cp->c_flags &= ~CN_UPDATED;
	mutex_exit(&cp->c_statelock);

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("c_sync_metadata: EXIT cp %p cflag %x\n",
			(void *)cp, cp->c_flags);
#endif

	return (error);
}

/*
 * This is the vop entry point for inactivating a vnode.
 * It just queues the request for the async thread which
 * calls cachefs_inactive.
 * Because of the dnlc, it is not safe to grab most locks here.
 */
static void
cachefs_inactive(struct vnode *vp, cred_t *cr)
{
	cnode_t *cp;
	struct cachefs_req *rp;
	int error;
	struct cachefscache *cachep;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_inactive: ENTER vp %p\n", (void *)vp);
#endif

	cp = VTOC(vp);

	ASSERT((cp->c_flags & CN_IDLE) == 0);

	/* vn_rele() set the v_count == 1 */

	cp->c_ipending = 1;

	rp = kmem_cache_alloc(cachefs_req_cache, KM_SLEEP);
	rp->cfs_cmd = CFS_IDLE;
	rp->cfs_cr = cr;
	crhold(rp->cfs_cr);
	rp->cfs_req_u.cu_idle.ci_vp = vp;
	error = cachefs_addqueue(rp, &(C_TO_FSCACHE(cp)->fs_workq));
	if (error) {
		cachep = C_TO_FSCACHE(cp)->fs_cache;
		error = cachefs_addqueue(rp, &cachep->c_workq);
		ASSERT(error == 0);
	}


#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_inactive: EXIT vp %p\n", (void *)vp);
#endif
}

/* ARGSUSED */
static int
cachefs_lookup(vnode_t *dvp, char *nm, vnode_t **vpp,
    struct pathname *pnp, int flags, vnode_t *rdir, cred_t *cr)
{
	int error = 0;
	cnode_t *dcp = VTOC(dvp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	int held = 0;
	int connected = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_lookup: ENTER dvp %p nm %s\n", (void *)dvp, nm);
#endif

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			break;
		held = 1;

		error = cachefs_lookup_common(dvp, nm, vpp, pnp,
			flags, rdir, cr);
		if (CFS_TIMEOUT(fscp, error)) {
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			} else {
				if (cachefs_cd_access_miss(fscp)) {
					rw_enter(&dcp->c_rwlock, RW_READER);
					error = cachefs_lookup_back(dvp, nm,
					    vpp, cr);
					rw_exit(&dcp->c_rwlock);
					if (!CFS_TIMEOUT(fscp, error))
						break;
					delay(5*hz);
					connected = 0;
					continue;
				}
				connected = 1;
				continue;
			}
		}
		break;
	}
	if (held)
		cachefs_cd_release(fscp);

	if (error == 0 && CFS_ISVDEV((*vpp)->v_type)) {
		struct vnode *newvp;
		newvp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type, cr);
		VN_RELE(*vpp);
		if (newvp == NULL) {
			error = ENOSYS;
		} else {
			*vpp = newvp;
		}
	}

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_lookup: EXIT error = %d\n", error);
#endif

	return (error);
}

/* ARGSUSED */
int
cachefs_lookup_common(vnode_t *dvp, char *nm, vnode_t **vpp,
    struct pathname *pnp, int flags, vnode_t *rdir, cred_t *cr)
{
	int error = 0;
	cnode_t *cp, *dcp = VTOC(dvp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	cachefscache_t *cachep = fscp->fs_cache;
	struct fid cookie;
	u_offset_t d_offset;
	struct cachefs_req *rp;
	cfs_cid_t cid, dircid;
	uint_t flag;
	uint_t uncached = 0;

	*vpp = NULL;

	/*
	 * If lookup is for "", just return dvp.  Don't need
	 * to send it over the wire, look it up in the dnlc,
	 * or perform any access checks.
	 */
	if (*nm == '\0') {
		VN_HOLD(dvp);
		*vpp = dvp;
		return (0);
	}

	/* can't do lookups in non-directories */
	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	/* perform access check, also does consistency check if connected */
	if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
		error = cachefs_access_connected(dvp, VEXEC, 0, cr);
	} else {
		mutex_enter(&dcp->c_statelock);
		error = cachefs_access_local(dcp, VEXEC, cr);
		mutex_exit(&dcp->c_statelock);
	}
	if (error)
		return (error);

	/*
	 * If lookup is for ".", just return dvp.  Don't need
	 * to send it over the wire or look it up in the dnlc,
	 * just need to check access.
	 */
	if (strcmp(nm, ".") == 0) {
		VN_HOLD(dvp);
		*vpp = dvp;
		return (0);
	}

	/* check the dnlc */
	*vpp = (vnode_t *)dnlc_lookup(dvp, nm, cr);
	if (*vpp)
		return (0);

	/* read lock the dir before starting the search */
	rw_enter(&dcp->c_rwlock, RW_READER);

	mutex_enter(&dcp->c_statelock);
	dircid = dcp->c_id;

	dcp->c_usage++;

	/* if front file is not usable, lookup on the back fs */
	if ((dcp->c_flags & (CN_NOCACHE | CN_ASYNC_POPULATE)) ||
	    ((dcp->c_filegrp->fg_flags & CFS_FG_READ) == 0)) {
		mutex_exit(&dcp->c_statelock);
		if (fscp->fs_cdconnected == CFS_CD_CONNECTED)
			error = cachefs_lookup_back(dvp, nm, vpp, cr);
		else
			error = ETIMEDOUT;
		goto out;
	}

	/* if the front file is not populated, try to populate it */
	if ((dcp->c_metadata.md_flags & MD_POPULATED) == 0) {
		if (fscp->fs_cdconnected != CFS_CD_CONNECTED) {
			error = ETIMEDOUT;
			mutex_exit(&dcp->c_statelock);
			goto out;
		}

		if (cachefs_async_okay()) {
			/* cannot populate if cache is not writable */
			ASSERT((dcp->c_flags &
			    (CN_ASYNC_POPULATE | CN_NOCACHE)) == 0);
			dcp->c_flags |= CN_ASYNC_POPULATE;

			rp = kmem_cache_alloc(cachefs_req_cache, KM_SLEEP);
			rp->cfs_cmd = CFS_POPULATE;
			rp->cfs_req_u.cu_populate.cpop_vp = dvp;
			rp->cfs_cr = cr;

			crhold(cr);
			VN_HOLD(dvp);

			error = cachefs_addqueue(rp, &fscp->fs_workq);
			if (error) {
				error = cachefs_addqueue(rp, &cachep->c_workq);
				ASSERT(error == 0);
			}
		} else if (fscp->fs_info.fi_mntflags & CFS_NOACL) {
			error = cachefs_dir_fill(dcp, cr);
			if (error != 0) {
				mutex_exit(&dcp->c_statelock);
				goto out;
			}
		}
		/* no populate if too many asyncs and we have to cache ACLs */

		mutex_exit(&dcp->c_statelock);

		if (fscp->fs_cdconnected == CFS_CD_CONNECTED)
			error = cachefs_lookup_back(dvp, nm, vpp, cr);
		else
			error = ETIMEDOUT;
		goto out;
	}

	/* by now we have a valid cached front file that we can search */

	ASSERT((dcp->c_flags & CN_ASYNC_POPULATE) == 0);
	error = cachefs_dir_look(dcp, nm, &cookie, &flag,
			&d_offset, &cid);
	mutex_exit(&dcp->c_statelock);

	if (error) {
		/* if the entry does not have the fid, go get it */
		if (error == EINVAL) {
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED)
				error = cachefs_lookup_back(dvp, nm, vpp, cr);
			else
				error = ETIMEDOUT;
		}

		/* errors other than does not exist */
		else if (error != ENOENT) {
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED)
				error = cachefs_lookup_back(dvp, nm, vpp, cr);
			else
				error = ETIMEDOUT;
		}
		goto out;
	}

	/*
	 * Else we found the entry in the cached directory.
	 * Make a cnode for it.
	 */
	error = cachefs_cnode_make(&cid, fscp, &cookie, NULL, NULL,
	    cr, 0, &cp);
	if (error == ESTALE) {
		mutex_enter(&dcp->c_statelock);
		cachefs_nocache(dcp);
		mutex_exit(&dcp->c_statelock);
		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			error = cachefs_lookup_back(dvp, nm, vpp, cr);
			uncached = 1;
		} else
			error = ETIMEDOUT;
	} else if (error == 0) {
		*vpp = CTOV(cp);
	}

out:
	if (error == 0) {
		/* put the entry in the dnlc */
		if (cachefs_dnlc)
			dnlc_enter(dvp, nm, *vpp, cr);

		/* save the cid of the parent so can find the name */
		cp = VTOC(*vpp);
		if (bcmp(&cp->c_metadata.md_parent, &dircid,
		    sizeof (cfs_cid_t)) != 0) {
			mutex_enter(&cp->c_statelock);
			cp->c_metadata.md_parent = dircid;
			cp->c_flags |= CN_UPDATED;
			mutex_exit(&cp->c_statelock);
		}
	}

	rw_exit(&dcp->c_rwlock);
	if (uncached && dcp->c_metadata.md_flags & MD_PACKED)
		(void) cachefs_pack_common(dvp, cr);
	return (error);
}

/*
 * Called from cachefs_lookup_common when the back file system needs to be
 * examined to perform the lookup.
 */
static int
cachefs_lookup_back(vnode_t *dvp, char *nm, vnode_t **vpp,
    cred_t *cr)
{
	int error = 0;
	cnode_t *cp, *dcp = VTOC(dvp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	vnode_t *backvp = NULL;
	struct vattr va;
	struct fid cookie;
	cfs_cid_t cid;

	mutex_enter(&dcp->c_statelock);

	/* do a lookup on the back FS to get the back vnode */
	if (dcp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, dcp);
		if (error)
			goto out;
	}
	error = VOP_LOOKUP(dcp->c_backvp, nm, &backvp, (struct pathname *)NULL,
				0, (vnode_t *)NULL, cr);
	if (error)
		goto out;
	if (CFS_ISVDEV(backvp->v_type)) {
		struct vnode *devvp = backvp;

		if (VOP_REALVP(devvp, &backvp) == 0) {
			VN_HOLD(backvp);
			VN_RELE(devvp);
		}
	}

	/* get the cookie from the backvp */
	error = cachefs_getcookie(backvp, &cookie, &va, cr);
	if (error)
		goto out;

	cid.cid_fileno = va.va_nodeid;
	cid.cid_flags = 0;

#if 0
	/* XXX bob: this is probably no longer necessary */
	/* if the directory entry was incomplete, we can complete it now */
	if ((dcp->c_metadata.md_flags & MD_POPULATED) &&
	    ((dcp->c_flags & CN_ASYNC_POPULATE) == 0) &&
	    (dcp->c_filegrp->fg_flags & CFS_FG_WRITE)) {
		cachefs_dir_modentry(dcp, d_offset, &cookie, &cid);
	}
#endif

out:
	mutex_exit(&dcp->c_statelock);

	/* create the cnode */
	if (error == 0) {
		error = cachefs_cnode_make(&cid, fscp, &cookie, &va,
			backvp, cr, 0, &cp);
		if (error == 0) {
			*vpp = CTOV(cp);
		}
	}

	if (backvp)
		VN_RELE(backvp);

	return (error);
}

/*ARGSUSED7*/
static int
cachefs_create(vnode_t *dvp, char *nm, vattr_t *vap,
    vcexcl_t exclusive, int mode, vnode_t **vpp, cred_t *cr, int flag)
{
	cnode_t *dcp = VTOC(dvp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	cachefscache_t *cachep = fscp->fs_cache;
	int error;
	int connected = 0;
	int held = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_create: ENTER dvp %p, nm %s\n",
		    (void *)dvp, nm);
#endif
	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 1);
		if (error)
			break;
		held = 1;

		/*
		 * if we are connected, perform the remote portion of the
		 * create.
		 */
		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			error = cachefs_create_connected(dvp, nm, vap,
			    exclusive, mode, vpp, cr);
			if (CFS_TIMEOUT(fscp, error)) {
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			} else if (error) {
				break;
			}
		}

		/* else we must be disconnected */
		else {
			error = cachefs_create_disconnected(dvp, nm, vap,
			    exclusive, mode, vpp, cr);
			if (CFS_TIMEOUT(fscp, error)) {
				connected = 1;
				continue;
			} else if (error) {
				break;
			}
		}
		break;
	}

	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_CREATE)) {
		fid_t *fidp = NULL;
		ino64_t fileno = 0;
		cnode_t *cp = NULL;
		if (error == 0)
			cp = VTOC(*vpp);

		if (cp != NULL) {
			fidp = &cp->c_metadata.md_cookie;
			fileno = cp->c_id.cid_fileno;
		}
		cachefs_log_create(cachep, error, fscp->fs_cfsvfsp,
		    fidp, fileno, cr->cr_uid);
	}

	if (held)
		cachefs_cd_release(fscp);

	if (error == 0 && CFS_ISFS_NONSHARED(fscp))
		(void) cachefs_pack(dvp, nm, cr);
	if (error == 0 && CFS_ISVDEV((*vpp)->v_type)) {
		struct vnode *spcvp;

		spcvp = specvp(*vpp, (*vpp)->v_rdev, (*vpp)->v_type, cr);
		VN_RELE(*vpp);
		if (spcvp == NULL) {
			error = ENOSYS;
		} else {
			*vpp = spcvp;
		}
	}

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_create: EXIT error %d\n", error);
#endif
	return (error);
}


static int
cachefs_create_connected(vnode_t *dvp, char *nm, vattr_t *vap,
    enum vcexcl exclusive, int mode, vnode_t **vpp, cred_t *cr)
{
	cnode_t *dcp = VTOC(dvp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	int error;
	vnode_t *tvp = NULL;
	vnode_t *devvp;
	fid_t cookie;
	vattr_t va;
	cnode_t *ncp;
	cfs_cid_t cid;
	vnode_t *vp;

	/* special case if file already exists */
	error = cachefs_lookup_common(dvp, nm, &vp, NULL, 0, NULL, cr);
	if (CFS_TIMEOUT(fscp, error))
		return (error);
	if (error == 0) {
		if (exclusive == EXCL)
			error = EEXIST;
		else if (vp->v_type == VDIR && (mode & VWRITE))
			error = EISDIR;
		else if ((error =
		    cachefs_access_connected(vp, mode, 0, cr)) == 0) {
			if ((vap->va_mask & AT_SIZE) && (vp->v_type == VREG)) {
				vap->va_mask = AT_SIZE;
				error = cachefs_setattr_common(vp, vap, 0, cr);
			}
		}
		if (error) {
			VN_RELE(vp);
		} else
			*vpp = vp;
		return (error);
	}

	rw_enter(&dcp->c_rwlock, RW_WRITER);
	mutex_enter(&dcp->c_statelock);

	/* consistency check the directory */
	error = CFSOP_CHECK_COBJECT(fscp, dcp, 0, cr);
	if (error) {
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	/* get the backvp if necessary */
	if (dcp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, dcp);
		if (error) {
			mutex_exit(&dcp->c_statelock);
			goto out;
		}
	}

	/* create the file on the back fs */
	error = VOP_CREATE(dcp->c_backvp, nm, vap, exclusive, mode,
						&devvp, cr, 0);
	mutex_exit(&dcp->c_statelock);
	if (error)
		goto out;
	if (VOP_REALVP(devvp, &tvp) == 0) {
		VN_HOLD(tvp);
		VN_RELE(devvp);
	} else {
		tvp = devvp;
	}

	/* get the fid and attrs from the back fs */
	error = cachefs_getcookie(tvp, &cookie, &va, cr);
	if (error)
		goto out;

	/* make the cnode */
	cid.cid_fileno = va.va_nodeid;
	cid.cid_flags = 0;
	error = cachefs_cnode_make(&cid, fscp, &cookie, &va, tvp, cr, 0, &ncp);
	if (error)
		goto out;

	*vpp = CTOV(ncp);

	/* enter it in the parent directory */
	mutex_enter(&dcp->c_statelock);
	if (CFS_ISFS_NONSHARED(fscp) &&
	    (dcp->c_metadata.md_flags & MD_POPULATED)) {
		/* see if entry already exists */
		error = cachefs_dir_look(dcp, nm, NULL, NULL, NULL, NULL);
		if (error == ENOENT) {
			/* entry, does not exist, add the new file */
			error = cachefs_dir_enter(dcp, nm, &ncp->c_cookie,
			    &ncp->c_id, SM_ASYNC);
			if (error) {
				cachefs_nocache(dcp);
				error = 0;
			}
			/* XXX should this be done elsewhere, too? */
			dnlc_enter(dvp, nm, *vpp, cr);
		} else {
			/* entry exists or some other problem */
			cachefs_nocache(dcp);
			error = 0;
		}
	}
	CFSOP_MODIFY_COBJECT(fscp, dcp, cr);
	mutex_exit(&dcp->c_statelock);

out:
	rw_exit(&dcp->c_rwlock);
	if (tvp)
		VN_RELE(tvp);

	return (error);
}

static int
cachefs_create_disconnected(vnode_t *dvp, char *nm, vattr_t *vap,
	enum vcexcl exclusive, int mode, vnode_t **vpp, cred_t *cr)
{
	cnode_t *dcp = VTOC(dvp);
	cnode_t *cp;
	cnode_t *ncp = NULL;
	vnode_t *vp;
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	int error = 0;
	struct vattr va;
	timestruc_t current_time;
	off_t commit = 0;
	fid_t cookie;
	cfs_cid_t cid;

	rw_enter(&dcp->c_rwlock, RW_WRITER);
	mutex_enter(&dcp->c_statelock);

	/* give up if the directory is not populated */
	if ((dcp->c_metadata.md_flags & MD_POPULATED) == 0) {
		mutex_exit(&dcp->c_statelock);
		rw_exit(&dcp->c_rwlock);
		return (ETIMEDOUT);
	}

	/* special case if file already exists */
	error = cachefs_dir_look(dcp, nm, &cookie, NULL, NULL, &cid);
	if (error == EINVAL) {
		mutex_exit(&dcp->c_statelock);
		rw_exit(&dcp->c_rwlock);
		return (ETIMEDOUT);
	}
	if (error == 0) {
		mutex_exit(&dcp->c_statelock);
		rw_exit(&dcp->c_rwlock);
		error = cachefs_cnode_make(&cid, fscp, &cookie, NULL, NULL,
		    cr, 0, &cp);
		if (error) {
			return (error);
		}
		vp = CTOV(cp);

		if (cp->c_metadata.md_flags & MD_NEEDATTRS)
			error = ETIMEDOUT;
		else if (exclusive == EXCL)
			error = EEXIST;
		else if (vp->v_type == VDIR && (mode & VWRITE))
			error = EISDIR;
		else {
			mutex_enter(&cp->c_statelock);
			error = cachefs_access_local(cp, mode, cr);
			mutex_exit(&cp->c_statelock);
			if (!error) {
				if ((vap->va_mask & AT_SIZE) &&
				    (vp->v_type == VREG)) {
					vap->va_mask = AT_SIZE;
					error =
					    cachefs_setattr_common(vp, vap,
						0, cr);
				}
			}
		}
		if (error) {
			VN_RELE(vp);
		} else
			*vpp = vp;
		return (error);
	}

	/* give up if cannot modify the cache */
	if (CFS_ISFS_WRITE_AROUND(fscp)) {
		mutex_exit(&dcp->c_statelock);
		error = ETIMEDOUT;
		goto out;
	}

	/* check access */
	if (error = cachefs_access_local(dcp, VWRITE, cr)) {
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	/* mark dir as modified */
	cachefs_modified(dcp);
	mutex_exit(&dcp->c_statelock);

	/* must be super-user to set sticky bit */
	if (cr->cr_uid != 0)
		vap->va_mode &= ~VSVTX;

	/* make up a reasonable set of attributes */
	cachefs_attr_setup(vap, &va, dcp, cr);

	/* create the cnode */
	error = cachefs_cnode_create(fscp, &va, 0, &ncp);
	if (error)
		goto out;

	mutex_enter(&ncp->c_statelock);

	/* get the front file now instead of later */
	if (vap->va_type == VREG) {
		error = cachefs_getfrontfile(ncp);
		if (error) {
			mutex_exit(&ncp->c_statelock);
			goto out;
		}
		ASSERT(ncp->c_frontvp != NULL);
		ASSERT((ncp->c_flags & CN_ALLOC_PENDING) == 0);
		ncp->c_metadata.md_flags |= MD_POPULATED;
	} else {
		ASSERT(ncp->c_flags & CN_ALLOC_PENDING);
		if (ncp->c_filegrp->fg_flags & CFS_FG_ALLOC_ATTR) {
			(void) filegrp_allocattr(ncp->c_filegrp);
		}
		error = filegrp_create_metadata(ncp->c_filegrp,
		    &ncp->c_metadata, &ncp->c_id);
		if (error) {
			mutex_exit(&ncp->c_statelock);
			goto out;
		}
		ncp->c_flags &= ~CN_ALLOC_PENDING;
	}
	mutex_enter(&dcp->c_statelock);
	cachefs_creategid(dcp, ncp, vap, cr);
	cachefs_createacl(dcp, ncp);
	mutex_exit(&dcp->c_statelock);

	/* set times on the file */
	current_time = hrestime;
	ncp->c_metadata.md_vattr.va_atime = current_time;
	ncp->c_metadata.md_localctime = current_time;
	ncp->c_metadata.md_localmtime = current_time;
	ncp->c_metadata.md_flags |= MD_LOCALMTIME | MD_LOCALCTIME;

	/* reserve space for the daemon cid mapping */
	error = cachefs_dlog_cidmap(fscp);
	if (error) {
		mutex_exit(&ncp->c_statelock);
		goto out;
	}
	ncp->c_metadata.md_flags |= MD_MAPPING;

	/* mark the new file as modified */
	if (cachefs_modified_alloc(ncp)) {
		mutex_exit(&ncp->c_statelock);
		error = ENOSPC;
		goto out;
	}
	ncp->c_flags |= CN_UPDATED;

	/*
	 * write the metadata now rather than waiting until
	 * inactive so that if there's no space we can let
	 * the caller know.
	 */
	ASSERT((ncp->c_flags & CN_ALLOC_PENDING) == 0);
	ASSERT((ncp->c_filegrp->fg_flags & CFS_FG_ALLOC_ATTR) == 0);
	error = filegrp_write_metadata(ncp->c_filegrp,
	    &ncp->c_id, &ncp->c_metadata);
	if (error) {
		mutex_exit(&ncp->c_statelock);
		goto out;
	}

	/* log the operation */
	commit = cachefs_dlog_create(fscp, dcp, nm, vap, exclusive,
	    mode, ncp, 0, cr);
	if (commit == 0) {
		mutex_exit(&ncp->c_statelock);
		error = ENOSPC;
		goto out;
	}

	mutex_exit(&ncp->c_statelock);

	mutex_enter(&dcp->c_statelock);

	/* update parent dir times */
	dcp->c_metadata.md_localmtime = current_time;
	dcp->c_metadata.md_flags |= MD_LOCALMTIME;
	dcp->c_flags |= CN_UPDATED;

	/* enter new file name in the parent directory */
	if (dcp->c_metadata.md_flags & MD_POPULATED) {
		error = cachefs_dir_enter(dcp, nm, &ncp->c_cookie,
		    &ncp->c_id, 0);
		if (error) {
			cachefs_nocache(dcp);
			mutex_exit(&dcp->c_statelock);
			error = ETIMEDOUT;
			goto out;
		}
		dnlc_enter(dvp, nm, CTOV(ncp), cr);
	} else {
		mutex_exit(&dcp->c_statelock);
		error = ETIMEDOUT;
		goto out;
	}
	mutex_exit(&dcp->c_statelock);

out:
	rw_exit(&dcp->c_rwlock);

	if (commit) {
		if (cachefs_dlog_commit(fscp, commit, error)) {
			/*EMPTY*/
			/* XXX bob: fix on panic */
		}
	}
	if (error) {
		/* destroy the cnode we created */
		if (ncp) {
			mutex_enter(&ncp->c_statelock);
			ncp->c_flags |= CN_DESTROY;
			mutex_exit(&ncp->c_statelock);
			VN_RELE(CTOV(ncp));
		}
	} else {
		*vpp = CTOV(ncp);
	}
	return (error);
}

static int
cachefs_remove(vnode_t *dvp, char *nm, cred_t *cr)
{
	cnode_t *dcp = VTOC(dvp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	cachefscache_t *cachep = fscp->fs_cache;
	int error = 0;
	int held = 0;
	int connected = 0;
	size_t namlen;
	vnode_t *vp = NULL;
	int vfslock = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_remove: ENTER dvp %p name %s\n",
		    (void *)dvp, nm);
#endif

	if (fscp->fs_cache->c_flags & (CACHE_NOFILL | CACHE_NOCACHE))
		ASSERT(dcp->c_flags & CN_NOCACHE);

	for (;;) {
		if (vfslock) {
			vn_vfsunlock(vp);
			vfslock = 0;
		}
		if (vp) {
			VN_RELE(vp);
			vp = NULL;
		}

		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 1);
		if (error)
			break;
		held = 1;

		/* if disconnected, do some extra error checking */
		if (fscp->fs_cdconnected != CFS_CD_CONNECTED) {
			/* check permissions */
			mutex_enter(&dcp->c_statelock);
			error = cachefs_access_local(dcp, (VEXEC|VWRITE), cr);
			mutex_exit(&dcp->c_statelock);
			if (CFS_TIMEOUT(fscp, error)) {
				connected = 1;
				continue;
			}
			if (error)
				break;

			namlen = strlen(nm);
			if (namlen == 0) {
				error = EINVAL;
				break;
			}

			/* cannot remove . and .. */
			if (nm[0] == '.') {
				if (namlen == 1) {
					error = EINVAL;
					break;
				} else if (namlen == 2 && nm[1] == '.') {
					error = EEXIST;
					break;
				}
			}

		}

		/* get the cnode of the file to delete */
		error = cachefs_lookup_common(dvp, nm, &vp, NULL, 0, NULL, cr);
		if (error) {
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
				if (CFS_TIMEOUT(fscp, error)) {
					cachefs_cd_release(fscp);
					held = 0;
					cachefs_cd_timedout(fscp);
					connected = 0;
					continue;
				}
			} else {
				if (CFS_TIMEOUT(fscp, error)) {
					connected = 1;
					continue;
				}
			}
			if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_REMOVE)) {
				struct fid foo;

				bzero(&foo, sizeof (foo));
				cachefs_log_remove(cachep, error,
				    fscp->fs_cfsvfsp, &foo, 0, cr->cr_uid);
			}
			break;
		}

		/* must be root to remove dirs */
		if (vp->v_type == VDIR && !suser(cr)) {
			error = EPERM;
			break;
		}

		if (vp->v_type == VDIR) {
			/* see ufs_dirremove for why this is done, mount race */
			if (vn_vfslock(vp)) {
				error = EBUSY;
				break;
			}
			vfslock = 1;
			if (vp->v_vfsmountedhere != NULL) {
				error = EBUSY;
				break;
			}
		}

		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			error = cachefs_remove_connected(dvp, nm, cr, vp);
			if (CFS_TIMEOUT(fscp, error)) {
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			}
		} else {
			error = cachefs_remove_disconnected(dvp, nm, cr,
				vp);
			if (CFS_TIMEOUT(fscp, error)) {
				connected = 1;
				continue;
			}
		}
		break;
	}

#if 0
	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_REMOVE))
		cachefs_log_remove(cachep, error, fscp->fs_cfsvfsp,
		    &cp->c_metadata.md_cookie, cp->c_id.cid_fileno, cr->cr_uid);
#endif

	if (held)
		cachefs_cd_release(fscp);

	if (vfslock)
		vn_vfsunlock(vp);

	if (vp)
		VN_RELE(vp);

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_remove: EXIT dvp %p\n", (void *)dvp);
#endif

	return (error);
}

int
cachefs_remove_connected(vnode_t *dvp, char *nm, cred_t *cr, vnode_t *vp)
{
	cnode_t *dcp = VTOC(dvp);
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	int error = 0;

	/*
	 * Acquire the rwlock (WRITER) on the directory to prevent other
	 * activity on the directory.
	 */
	rw_enter(&dcp->c_rwlock, RW_WRITER);

	/* purge dnlc of this entry so can get accurate vnode count */
	dnlc_purge_vp(vp);

	/*
	 * If the cnode is active, make a link to the file
	 * so operations on the file will continue.
	 */
	if ((vp->v_type != VDIR) &&
	    !((vp->v_count == 1) || ((vp->v_count == 2) && cp->c_ipending))) {
		error = cachefs_remove_dolink(dvp, vp, nm, cr);
		if (error)
			goto out;
	}

	/* else drop the backvp so nfs does not do rename */
	else if (cp->c_backvp) {
		mutex_enter(&cp->c_statelock);
		if (cp->c_backvp) {
			VN_RELE(cp->c_backvp);
			cp->c_backvp = NULL;
		}
		mutex_exit(&cp->c_statelock);
	}

	mutex_enter(&dcp->c_statelock);

	/* get the backvp */
	if (dcp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, dcp);
		if (error) {
			mutex_exit(&dcp->c_statelock);
			goto out;
		}
	}

	/* check directory consistency */
	error = CFSOP_CHECK_COBJECT(fscp, dcp, 0, cr);
	if (error) {
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	/* perform the remove on the back fs */
	error = VOP_REMOVE(dcp->c_backvp, nm, cr);
	if (error) {
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	/* the dir has been modified */
	CFSOP_MODIFY_COBJECT(fscp, dcp, cr);

	/* remove the entry from the populated directory */
	if (CFS_ISFS_NONSHARED(fscp) &&
	    (dcp->c_metadata.md_flags & MD_POPULATED)) {
		error = cachefs_dir_rmentry(dcp, nm);
		if (error) {
			cachefs_nocache(dcp);
			error = 0;
		}
	}
	mutex_exit(&dcp->c_statelock);

	/* fix up the file we deleted */
	mutex_enter(&cp->c_statelock);
	if (cp->c_attr.va_nlink == 1)
		cp->c_flags |= CN_DESTROY;
	else
		cp->c_flags |= CN_UPDATED;

	cp->c_attr.va_nlink--;
	CFSOP_MODIFY_COBJECT(fscp, cp, cr);
	mutex_exit(&cp->c_statelock);

out:
	rw_exit(&dcp->c_rwlock);
	return (error);
}

int
cachefs_remove_disconnected(vnode_t *dvp, char *nm, cred_t *cr,
    vnode_t *vp)
{
	cnode_t *dcp = VTOC(dvp);
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	int error = 0;
	off_t commit = 0;
	timestruc_t current_time;

	if (CFS_ISFS_WRITE_AROUND(fscp))
		return (ETIMEDOUT);

	if (cp->c_metadata.md_flags & MD_NEEDATTRS)
		return (ETIMEDOUT);

	/*
	 * Acquire the rwlock (WRITER) on the directory to prevent other
	 * activity on the directory.
	 */
	rw_enter(&dcp->c_rwlock, RW_WRITER);

	/* dir must be populated */
	if ((dcp->c_metadata.md_flags & MD_POPULATED) == 0) {
		error = ETIMEDOUT;
		goto out;
	}

	mutex_enter(&dcp->c_statelock);
	mutex_enter(&cp->c_statelock);

	error = cachefs_stickyrmchk(dcp, cp, cr);

	mutex_exit(&cp->c_statelock);
	mutex_exit(&dcp->c_statelock);
	if (error)
		goto out;

	/* purge dnlc of this entry so can get accurate vnode count */
	dnlc_purge_vp(vp);

	/*
	 * If the cnode is active, make a link to the file
	 * so operations on the file will continue.
	 */
	if ((vp->v_type != VDIR) &&
	    !((vp->v_count == 1) || ((vp->v_count == 2) && cp->c_ipending))) {
		error = cachefs_remove_dolink(dvp, vp, nm, cr);
		if (error)
			goto out;
	}

	if (cp->c_attr.va_nlink > 1) {
		mutex_enter(&cp->c_statelock);
		if (cachefs_modified_alloc(cp)) {
			mutex_exit(&cp->c_statelock);
			error = ENOSPC;
			goto out;
		}
		if ((cp->c_metadata.md_flags & MD_MAPPING) == 0) {
			error = cachefs_dlog_cidmap(fscp);
			if (error) {
				mutex_exit(&cp->c_statelock);
				error = ENOSPC;
				goto out;
			}
			cp->c_metadata.md_flags |= MD_MAPPING;
			cp->c_flags |= CN_UPDATED;
		}
		mutex_exit(&cp->c_statelock);
	}

	/* log the remove */
	commit = cachefs_dlog_remove(fscp, dcp, nm, cp, cr);
	if (commit == 0) {
		error = ENOSPC;
		goto out;
	}

	/* remove the file from the dir */
	mutex_enter(&dcp->c_statelock);
	if ((dcp->c_metadata.md_flags & MD_POPULATED) == 0) {
		mutex_exit(&dcp->c_statelock);
		error = ETIMEDOUT;
		goto out;

	}
	cachefs_modified(dcp);
	error = cachefs_dir_rmentry(dcp, nm);
	if (error) {
		mutex_exit(&dcp->c_statelock);
		if (error == ENOTDIR)
			error = ETIMEDOUT;
		goto out;
	}

	/* update parent dir times */
	current_time = hrestime;
	dcp->c_metadata.md_localctime = current_time;
	dcp->c_metadata.md_localmtime = current_time;
	dcp->c_metadata.md_flags |= MD_LOCALCTIME | MD_LOCALMTIME;
	dcp->c_flags |= CN_UPDATED;
	mutex_exit(&dcp->c_statelock);

	/* adjust file we are deleting */
	mutex_enter(&cp->c_statelock);
	cp->c_attr.va_nlink--;
	cp->c_metadata.md_localctime = current_time;
	cp->c_metadata.md_flags |= MD_LOCALCTIME;
	if (cp->c_attr.va_nlink == 0) {
		cp->c_flags |= CN_DESTROY;
	} else {
		cp->c_flags |= CN_UPDATED;
	}
	mutex_exit(&cp->c_statelock);

out:
	if (commit) {
		/* commit the log entry */
		if (cachefs_dlog_commit(fscp, commit, error)) {
			/*EMPTY*/
			/* XXX bob: fix on panic */
		}
	}

	rw_exit(&dcp->c_rwlock);
	return (error);
}

static int
cachefs_link(vnode_t *tdvp, vnode_t *fvp, char *tnm, cred_t *cr)
{
	fscache_t *fscp = VFS_TO_FSCACHE(tdvp->v_vfsp);
	cnode_t *tdcp = VTOC(tdvp);
	struct vnode *realvp;
	int error = 0;
	int held = 0;
	int connected = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_link: ENTER fvp %p tdvp %p tnm %s\n",
			(void *)fvp, (void *)tdvp, tnm);
#endif

	if (fscp->fs_cache->c_flags & (CACHE_NOFILL | CACHE_NOCACHE))
		ASSERT(tdcp->c_flags & CN_NOCACHE);

	if (VOP_REALVP(fvp, &realvp) == 0) {
		fvp = realvp;
	}

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			rw_exit(&tdcp->c_rwlock);
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 1);
		if (error)
			break;
		rw_enter(&tdcp->c_rwlock, RW_WRITER);
		held = 1;

		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			error = cachefs_link_connected(tdvp, fvp, tnm, cr);
			if (CFS_TIMEOUT(fscp, error)) {
				rw_exit(&tdcp->c_rwlock);
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			}
		} else {
			error = cachefs_link_disconnected(tdvp, fvp, tnm,
				cr);
			if (CFS_TIMEOUT(fscp, error)) {
				connected = 1;
				continue;
			}
		}
		break;
	}

	if (held) {
		rw_exit(&tdcp->c_rwlock);
		cachefs_cd_release(fscp);
	}

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_link: EXIT fvp %p tdvp %p tnm %s\n",
			(void *)fvp, (void *)tdvp, tnm);
#endif
	return (error);
}

static int
cachefs_link_connected(vnode_t *tdvp, vnode_t *fvp, char *tnm, cred_t *cr)
{
	cnode_t *tdcp = VTOC(tdvp);
	cnode_t *fcp = VTOC(fvp);
	fscache_t *fscp = VFS_TO_FSCACHE(tdvp->v_vfsp);
	int error = 0;
	vnode_t *backvp = NULL;

	if (tdcp != fcp) {
		mutex_enter(&fcp->c_statelock);

		if (fcp->c_backvp == NULL) {
			error = cachefs_getbackvp(fscp, fcp);
			if (error) {
				mutex_exit(&fcp->c_statelock);
				goto out;
			}
		}

		error = CFSOP_CHECK_COBJECT(fscp, fcp, 0, cr);
		if (error) {
			mutex_exit(&fcp->c_statelock);
			goto out;
		}
		backvp = fcp->c_backvp;
		VN_HOLD(backvp);
		mutex_exit(&fcp->c_statelock);
	}

	mutex_enter(&tdcp->c_statelock);

	/* get backvp of target directory */
	if (tdcp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, tdcp);
		if (error) {
			mutex_exit(&tdcp->c_statelock);
			goto out;
		}
	}

	/* consistency check target directory */
	error = CFSOP_CHECK_COBJECT(fscp, tdcp, 0, cr);
	if (error) {
		mutex_exit(&tdcp->c_statelock);
		goto out;
	}
	if (backvp == NULL) {
		backvp = tdcp->c_backvp;
		VN_HOLD(backvp);
	}

	/* perform the link on the back fs */
	error = VOP_LINK(tdcp->c_backvp, backvp, tnm, cr);
	if (error) {
		mutex_exit(&tdcp->c_statelock);
		goto out;
	}

	CFSOP_MODIFY_COBJECT(fscp, tdcp, cr);

	/* if the dir is populated, add the new link */
	if (CFS_ISFS_NONSHARED(fscp) &&
	    (tdcp->c_metadata.md_flags & MD_POPULATED)) {
		error = cachefs_dir_enter(tdcp, tnm, &fcp->c_cookie,
		    &fcp->c_id, SM_ASYNC);
		if (error) {
			cachefs_nocache(tdcp);
			error = 0;
		}
	}
	mutex_exit(&tdcp->c_statelock);

	/* get the new link count on the file */
	mutex_enter(&fcp->c_statelock);
	fcp->c_flags |= CN_UPDATED;
	CFSOP_MODIFY_COBJECT(fscp, fcp, cr);
	if (fcp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, fcp);
		if (error) {
			mutex_exit(&fcp->c_statelock);
			goto out;
		}
	}

	/* XXX bob: given what modify_cobject does this seems unnecessary */
	fcp->c_attr.va_mask = AT_ALL;
	error = VOP_GETATTR(fcp->c_backvp, &fcp->c_attr, 0, cr);
	mutex_exit(&fcp->c_statelock);
out:
	if (backvp)
		VN_RELE(backvp);

	return (error);
}

static int
cachefs_link_disconnected(vnode_t *tdvp, vnode_t *fvp, char *tnm,
    cred_t *cr)
{
	cnode_t *tdcp = VTOC(tdvp);
	cnode_t *fcp = VTOC(fvp);
	fscache_t *fscp = VFS_TO_FSCACHE(tdvp->v_vfsp);
	int error = 0;
	timestruc_t current_time;
	off_t commit = 0;

	if (fvp->v_type == VDIR && !suser(cr))
		return (EPERM);

	if (CFS_ISFS_WRITE_AROUND(fscp))
		return (ETIMEDOUT);

	if (fcp->c_metadata.md_flags & MD_NEEDATTRS)
		return (ETIMEDOUT);

	mutex_enter(&tdcp->c_statelock);

	/* check permissions */
	if (error = cachefs_access_local(tdcp, (VEXEC|VWRITE), cr)) {
		mutex_exit(&tdcp->c_statelock);
		goto out;
	}

	/* the directory front file must be populated */
	if ((tdcp->c_metadata.md_flags & MD_POPULATED) == 0) {
		error = ETIMEDOUT;
		mutex_exit(&tdcp->c_statelock);
		goto out;
	}

	/* make sure tnm does not already exist in the directory */
	error = cachefs_dir_look(tdcp, tnm, NULL, NULL, NULL, NULL);
	if (error == ENOTDIR) {
		error = ETIMEDOUT;
		mutex_exit(&tdcp->c_statelock);
		goto out;
	}
	if (error != ENOENT) {
		error = EEXIST;
		mutex_exit(&tdcp->c_statelock);
		goto out;
	}

	mutex_enter(&fcp->c_statelock);

	/* create a mapping for the file if necessary */
	if ((fcp->c_metadata.md_flags & MD_MAPPING) == 0) {
		error = cachefs_dlog_cidmap(fscp);
		if (error) {
			mutex_exit(&fcp->c_statelock);
			mutex_exit(&tdcp->c_statelock);
			error = ENOSPC;
			goto out;
		}
		fcp->c_metadata.md_flags |= MD_MAPPING;
		fcp->c_flags |= CN_UPDATED;
	}

	/* mark file as modified */
	if (cachefs_modified_alloc(fcp)) {
		mutex_exit(&fcp->c_statelock);
		mutex_exit(&tdcp->c_statelock);
		error = ENOSPC;
		goto out;
	}
	mutex_exit(&fcp->c_statelock);

	/* log the operation */
	commit = cachefs_dlog_link(fscp, tdcp, tnm, fcp, cr);
	if (commit == 0) {
		mutex_exit(&tdcp->c_statelock);
		error = ENOSPC;
		goto out;
	}

	current_time = hrestime;

	/* make the new link */
	cachefs_modified(tdcp);
	error = cachefs_dir_enter(tdcp, tnm, &fcp->c_cookie,
	    &fcp->c_id, SM_ASYNC);
	if (error) {
		error = 0;
		mutex_exit(&tdcp->c_statelock);
		goto out;
	}

	/* Update mtime/ctime of parent dir */
	tdcp->c_metadata.md_localmtime = current_time;
	tdcp->c_metadata.md_localctime = current_time;
	tdcp->c_metadata.md_flags |= MD_LOCALCTIME | MD_LOCALMTIME;
	tdcp->c_flags |= CN_UPDATED;
	mutex_exit(&tdcp->c_statelock);

	/* update the file we linked to */
	mutex_enter(&fcp->c_statelock);
	fcp->c_attr.va_nlink++;
	fcp->c_metadata.md_localctime = current_time;
	fcp->c_metadata.md_flags |= MD_LOCALCTIME;
	fcp->c_flags |= CN_UPDATED;
	mutex_exit(&fcp->c_statelock);

out:
	if (commit) {
		/* commit the log entry */
		if (cachefs_dlog_commit(fscp, commit, error)) {
			/*EMPTY*/
			/* XXX bob: fix on panic */
		}
	}

	return (error);
}

/*
 * Serialize all renames in CFS, to avoid deadlocks - We have to hold two
 * cnodes atomically.
 */
kmutex_t cachefs_rename_lock;

static int
cachefs_rename(vnode_t *odvp, char *onm, vnode_t *ndvp,
    char *nnm, cred_t *cr)
{
	fscache_t *fscp = C_TO_FSCACHE(VTOC(odvp));
	cachefscache_t *cachep = fscp->fs_cache;
	int error = 0;
	int held = 0;
	int connected = 0;
	vnode_t *delvp = NULL;
	vnode_t *tvp = NULL;
	int vfslock = 0;
	struct vnode *realvp;


	if (VOP_REALVP(ndvp, &realvp) == 0)
		ndvp = realvp;

	/*
	 * if the fs NOFILL or NOCACHE flags are on, then the old and new
	 * directory cnodes better indicate NOCACHE mode as well.
	 */
	ASSERT
	    ((fscp->fs_cache->c_flags & (CACHE_NOFILL | CACHE_NOCACHE)) == 0 ||
	    ((VTOC(odvp)->c_flags & CN_NOCACHE) &&
	    (VTOC(ndvp)->c_flags & CN_NOCACHE)));

	for (;;) {
		if (vfslock) {
			vn_vfsunlock(delvp);
			vfslock = 0;
		}
		if (delvp) {
			VN_RELE(delvp);
			delvp = NULL;
		}

		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 1);
		if (error)
			break;
		held = 1;

		/* sanity check */
		if ((odvp->v_type != VDIR) || (ndvp->v_type != VDIR)) {
			error = EINVAL;
			break;
		}

		/* cannot rename from or to . or .. */
		if (strcmp(onm, ".") == 0 || strcmp(onm, "..") == 0 ||
		    strcmp(nnm, ".") == 0 || strcmp(nnm, "..") == 0) {
			error = EINVAL;
			break;
		}

		if (odvp != ndvp) {
			/*
			 * if moving a directory, its notion
			 * of ".." will change
			 */
			error = cachefs_lookup_common(odvp, onm, &tvp,
			    NULL, 0, NULL, cr);
			if (error == 0) {
				ASSERT(tvp != NULL);
				if (tvp->v_type == VDIR) {
					cnode_t *cp = VTOC(tvp);

					dnlc_remove(tvp, "..");

					mutex_enter(&cp->c_statelock);
					CFSOP_MODIFY_COBJECT(fscp, cp, cr);
					mutex_exit(&cp->c_statelock);
				}
			} else {
				tvp = NULL;
				if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
					if (CFS_TIMEOUT(fscp, error)) {
						cachefs_cd_release(fscp);
						held = 0;
						cachefs_cd_timedout(fscp);
						connected = 0;
						continue;
					}
				} else {
					if (CFS_TIMEOUT(fscp, error)) {
						connected = 1;
						continue;
					}
				}
				break;
			}
		}

		/* get the cnode if file being deleted */
		error = cachefs_lookup_common(ndvp, nnm, &delvp, NULL, 0,
			NULL, cr);
		if (error) {
			delvp = NULL;
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
				if (CFS_TIMEOUT(fscp, error)) {
					cachefs_cd_release(fscp);
					held = 0;
					cachefs_cd_timedout(fscp);
					connected = 0;
					continue;
				}
			} else {
				if (CFS_TIMEOUT(fscp, error)) {
					connected = 1;
					continue;
				}
			}
			if (error != ENOENT)
				break;
		}

		if (delvp && delvp->v_type == VDIR) {
			/* see ufs_dirremove for why this is done, mount race */
			if (vn_vfslock(delvp)) {
				error = EBUSY;
				break;
			}
			vfslock = 1;
			if (delvp->v_vfsmountedhere != NULL) {
				error = EBUSY;
				break;
			}
		}

		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			error = cachefs_rename_connected(odvp, onm,
				ndvp, nnm, cr, delvp);
			if (CFS_TIMEOUT(fscp, error)) {
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			}
		} else {
			error = cachefs_rename_disconnected(odvp, onm,
				ndvp, nnm, cr, delvp);
			if (CFS_TIMEOUT(fscp, error)) {
				connected = 1;
				continue;
			}
		}
		break;
	}

	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_RENAME)) {
		struct fid gone;

		bzero(&gone, sizeof (gone));
		gone.fid_len = MAXFIDSZ;
		if (delvp != NULL)
			(void) VOP_FID(delvp, &gone);

		cachefs_log_rename(cachep, error, fscp->fs_cfsvfsp,
		    &gone, 0, (delvp != NULL), cr->cr_uid);
	}

	if (held)
		cachefs_cd_release(fscp);

	if (vfslock)
		vn_vfsunlock(delvp);

	if (delvp)
		VN_RELE(delvp);
	if (tvp)
		VN_RELE(tvp);

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
	return (error);
}

static int
cachefs_rename_connected(vnode_t *odvp, char *onm, vnode_t *ndvp,
    char *nnm, cred_t *cr, vnode_t *delvp)
{
	cnode_t *odcp = VTOC(odvp);
	cnode_t *ndcp = VTOC(ndvp);
	vnode_t *revp = NULL;
	cnode_t *recp;
	cnode_t *delcp;
	fscache_t *fscp = C_TO_FSCACHE(odcp);
	int error = 0;
	struct fid cookie;
	struct fid *cookiep;
	cfs_cid_t cid;
	int gotdirent;

	/* find the file we are renaming */
	error = cachefs_lookup_common(odvp, onm, &revp, NULL, 0, NULL, cr);
	if (error)
		return (error);
	recp = VTOC(revp);

	/*
	 * To avoid deadlock, we acquire this global rename lock before
	 * we try to get the locks for the source and target directories.
	 */
	mutex_enter(&cachefs_rename_lock);
	rw_enter(&odcp->c_rwlock, RW_WRITER);
	if (odcp != ndcp) {
		rw_enter(&ndcp->c_rwlock, RW_WRITER);
	}
	mutex_exit(&cachefs_rename_lock);

	ASSERT((odcp->c_flags & CN_ASYNC_POP_WORKING) == 0);
	ASSERT((ndcp->c_flags & CN_ASYNC_POP_WORKING) == 0);

	mutex_enter(&odcp->c_statelock);
	if (odcp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, odcp);
		if (error) {
			mutex_exit(&odcp->c_statelock);
			goto out;
		}
	}

	error = CFSOP_CHECK_COBJECT(fscp, odcp, 0, cr);
	if (error) {
		mutex_exit(&odcp->c_statelock);
		goto out;
	}
	mutex_exit(&odcp->c_statelock);

	if (odcp != ndcp) {
		mutex_enter(&ndcp->c_statelock);
		if (ndcp->c_backvp == NULL) {
			error = cachefs_getbackvp(fscp, ndcp);
			if (error) {
				mutex_exit(&ndcp->c_statelock);
				goto out;
			}
		}

		error = CFSOP_CHECK_COBJECT(fscp, ndcp, 0, cr);
		if (error) {
			mutex_exit(&ndcp->c_statelock);
			goto out;
		}
		mutex_exit(&ndcp->c_statelock);
	}

	/* if a file is being deleted because of this rename */
	if (delvp) {
		/* if src and dest file are same */
		if (delvp == revp) {
			error = 0;
			goto out;
		}

		/*
		 * If the cnode is active, make a link to the file
		 * so operations on the file will continue.
		 */
		dnlc_purge_vp(delvp);
		delcp = VTOC(delvp);
		if ((delvp->v_type != VDIR) &&
		    !((delvp->v_count == 1) ||
		    ((delvp->v_count == 2) && delcp->c_ipending))) {
			error = cachefs_remove_dolink(ndvp, delvp, nnm, cr);
			if (error)
				goto out;
		}
	}

	/* do the rename on the back fs */
	error = VOP_RENAME(odcp->c_backvp, onm, ndcp->c_backvp, nnm, cr);
	if (error)
		goto out;

	/* purge mappings to file in the old directory */
	dnlc_purge_vp(odvp);

	/* purge mappings in the new dir if we deleted a file */
	if (delvp && (odvp != ndvp))
		dnlc_purge_vp(ndvp);

	/* update the file we just deleted */
	if (delvp) {
		mutex_enter(&delcp->c_statelock);
		if (delcp->c_attr.va_nlink == 1) {
			delcp->c_flags |= CN_DESTROY;
		} else {
			delcp->c_flags |= CN_UPDATED;
		}
		delcp->c_attr.va_nlink--;
		CFSOP_MODIFY_COBJECT(fscp, delcp, cr);
		mutex_exit(&delcp->c_statelock);
	}

	/* find the entry in the old directory */
	mutex_enter(&odcp->c_statelock);
	gotdirent = 0;
	cookiep = NULL;
	if (CFS_ISFS_NONSHARED(fscp) &&
	    (odcp->c_metadata.md_flags & MD_POPULATED)) {
		error = cachefs_dir_look(odcp, onm, &cookie,
			NULL, NULL, &cid);
		if (error == 0 || error == EINVAL) {
			gotdirent = 1;
			if (error == 0)
				cookiep = &cookie;
		} else {
			cachefs_inval_object(odcp);
		}
	}
	error = 0;

	/* remove the directory entry from the old directory */
	if (gotdirent) {
		error = cachefs_dir_rmentry(odcp, onm);
		if (error) {
			cachefs_nocache(odcp);
			error = 0;
		}
	}
	CFSOP_MODIFY_COBJECT(fscp, odcp, cr);
	mutex_exit(&odcp->c_statelock);

	/* install the directory entry in the new directory */
	mutex_enter(&ndcp->c_statelock);
	if (CFS_ISFS_NONSHARED(fscp) &&
	    (ndcp->c_metadata.md_flags & MD_POPULATED)) {
		error = 1;
		if (gotdirent) {
			ASSERT(cid.cid_fileno != 0);
			error = 0;
			if (delvp) {
				error = cachefs_dir_rmentry(ndcp, nnm);
			}
			if (error == 0) {
				error = cachefs_dir_enter(ndcp, nnm, cookiep,
				    &cid, SM_ASYNC);
			}
		}
		if (error) {
			cachefs_nocache(ndcp);
			error = 0;
		}
	}
	if (odcp != ndcp)
		CFSOP_MODIFY_COBJECT(fscp, ndcp, cr);
	mutex_exit(&ndcp->c_statelock);

	/* ctime of renamed file has changed */
	mutex_enter(&recp->c_statelock);
	CFSOP_MODIFY_COBJECT(fscp, recp, cr);
	mutex_exit(&recp->c_statelock);

out:
	if (odcp != ndcp)
		rw_exit(&ndcp->c_rwlock);
	rw_exit(&odcp->c_rwlock);

	VN_RELE(revp);

	return (error);
}

static int
cachefs_rename_disconnected(vnode_t *odvp, char *onm, vnode_t *ndvp,
    char *nnm, cred_t *cr, vnode_t *delvp)
{
	cnode_t *odcp = VTOC(odvp);
	cnode_t *ndcp = VTOC(ndvp);
	cnode_t *delcp = NULL;
	vnode_t *revp = NULL;
	cnode_t *recp;
	fscache_t *fscp = C_TO_FSCACHE(odcp);
	int error = 0;
	struct fid cookie;
	struct fid *cookiep;
	cfs_cid_t cid;
	off_t commit = 0;
	timestruc_t current_time;

	if (CFS_ISFS_WRITE_AROUND(fscp))
		return (ETIMEDOUT);

	/* find the file we are renaming */
	error = cachefs_lookup_common(odvp, onm, &revp, NULL, 0, NULL, cr);
	if (error)
		return (error);
	recp = VTOC(revp);

	/*
	 * To avoid deadlock, we acquire this global rename lock before
	 * we try to get the locks for the source and target directories.
	 */
	mutex_enter(&cachefs_rename_lock);
	rw_enter(&odcp->c_rwlock, RW_WRITER);
	if (odcp != ndcp) {
		rw_enter(&ndcp->c_rwlock, RW_WRITER);
	}
	mutex_exit(&cachefs_rename_lock);

	if (recp->c_metadata.md_flags & MD_NEEDATTRS) {
		error = ETIMEDOUT;
		goto out;
	}

	if ((recp->c_metadata.md_flags & MD_MAPPING) == 0) {
		mutex_enter(&recp->c_statelock);
		if ((recp->c_metadata.md_flags & MD_MAPPING) == 0) {
			error = cachefs_dlog_cidmap(fscp);
			if (error) {
				mutex_exit(&recp->c_statelock);
				error = ENOSPC;
				goto out;
			}
			recp->c_metadata.md_flags |= MD_MAPPING;
			recp->c_flags |= CN_UPDATED;
		}
		mutex_exit(&recp->c_statelock);
	}

	/* check permissions */
	/* XXX clean up this mutex junk sometime */
	mutex_enter(&odcp->c_statelock);
	error = cachefs_access_local(odcp, (VEXEC|VWRITE), cr);
	mutex_exit(&odcp->c_statelock);
	if (error != 0)
		goto out;
	mutex_enter(&ndcp->c_statelock);
	error = cachefs_access_local(ndcp, (VEXEC|VWRITE), cr);
	mutex_exit(&ndcp->c_statelock);
	if (error != 0)
		goto out;
	mutex_enter(&odcp->c_statelock);
	error = cachefs_stickyrmchk(odcp, recp, cr);
	mutex_exit(&odcp->c_statelock);
	if (error != 0)
		goto out;

	/* dirs must be populated */
	if (((odcp->c_metadata.md_flags & MD_POPULATED) == 0) ||
	    ((ndcp->c_metadata.md_flags & MD_POPULATED) == 0)) {
		error = ETIMEDOUT;
		goto out;
	}

	/* for now do not allow moving dirs because could cause cycles */
	if ((((revp->v_type == VDIR) && (odvp != ndvp))) ||
	    (revp == odvp)) {
		error = ETIMEDOUT;
		goto out;
	}

	/* if a file is being deleted because of this rename */
	if (delvp) {
		delcp = VTOC(delvp);

		/* if src and dest file are the same */
		if (delvp == revp) {
			error = 0;
			goto out;
		}

		if (delcp->c_metadata.md_flags & MD_NEEDATTRS) {
			error = ETIMEDOUT;
			goto out;
		}

		/* if there are hard links to this file */
		if (delcp->c_attr.va_nlink > 1) {
			mutex_enter(&delcp->c_statelock);
			if (cachefs_modified_alloc(delcp)) {
				mutex_exit(&delcp->c_statelock);
				error = ENOSPC;
				goto out;
			}

			if ((delcp->c_metadata.md_flags & MD_MAPPING) == 0) {
				error = cachefs_dlog_cidmap(fscp);
				if (error) {
					mutex_exit(&delcp->c_statelock);
					error = ENOSPC;
					goto out;
				}
				delcp->c_metadata.md_flags |= MD_MAPPING;
				delcp->c_flags |= CN_UPDATED;
			}
			mutex_exit(&delcp->c_statelock);
		}

		/* make sure we can delete file */
		mutex_enter(&ndcp->c_statelock);
		error = cachefs_stickyrmchk(ndcp, delcp, cr);
		mutex_exit(&ndcp->c_statelock);
		if (error != 0)
			goto out;

		/*
		 * If the cnode is active, make a link to the file
		 * so operations on the file will continue.
		 */
		dnlc_purge_vp(delvp);
		if ((delvp->v_type != VDIR) &&
		    !((delvp->v_count == 1) ||
		    ((delvp->v_count == 2) && delcp->c_ipending))) {
			error = cachefs_remove_dolink(ndvp, delvp, nnm, cr);
			if (error)
				goto out;
		}
	}

	/* purge mappings to file in the old directory */
	dnlc_purge_vp(odvp);

	/* purge mappings in the new dir if we deleted a file */
	if (delvp && (odvp != ndvp))
		dnlc_purge_vp(ndvp);

	/* find the entry in the old directory */
	mutex_enter(&odcp->c_statelock);
	if ((odcp->c_metadata.md_flags & MD_POPULATED) == 0) {
		mutex_exit(&odcp->c_statelock);
		error = ETIMEDOUT;
		goto out;
	}
	cookiep = NULL;
	error = cachefs_dir_look(odcp, onm, &cookie, NULL, NULL, &cid);
	if (error == 0 || error == EINVAL) {
		if (error == 0)
			cookiep = &cookie;
	} else {
		mutex_exit(&odcp->c_statelock);
		if (error == ENOTDIR)
			error = ETIMEDOUT;
		goto out;
	}
	error = 0;

	/* write the log entry */
	commit = cachefs_dlog_rename(fscp, odcp, onm, ndcp, nnm, cr,
	    recp, delcp);
	if (commit == 0) {
		mutex_exit(&odcp->c_statelock);
		error = ENOSPC;
		goto out;
	}

	/* remove the directory entry from the old directory */
	cachefs_modified(odcp);
	error = cachefs_dir_rmentry(odcp, onm);
	if (error) {
		mutex_exit(&odcp->c_statelock);
		if (error == ENOTDIR)
			error = ETIMEDOUT;
		goto out;
	}
	mutex_exit(&odcp->c_statelock);

	/* install the directory entry in the new directory */
	mutex_enter(&ndcp->c_statelock);
	error = ENOTDIR;
	if (ndcp->c_metadata.md_flags & MD_POPULATED) {
		ASSERT(cid.cid_fileno != 0);
		cachefs_modified(ndcp);
		error = 0;
		if (delvp) {
			error = cachefs_dir_rmentry(ndcp, nnm);
		}
		if (error == 0) {
			error = cachefs_dir_enter(ndcp, nnm, cookiep,
			    &cid, SM_ASYNC);
		}
	}
	if (error) {
		cachefs_nocache(ndcp);
		mutex_exit(&ndcp->c_statelock);
		mutex_enter(&odcp->c_statelock);
		cachefs_nocache(odcp);
		mutex_exit(&odcp->c_statelock);
		if (error == ENOTDIR)
			error = ETIMEDOUT;
		goto out;
	}
	mutex_exit(&ndcp->c_statelock);

	current_time = hrestime;

	/* update the file we just deleted */
	if (delvp) {
		mutex_enter(&delcp->c_statelock);
		delcp->c_attr.va_nlink--;
		delcp->c_metadata.md_localctime = current_time;
		delcp->c_metadata.md_flags |= MD_LOCALCTIME;
		if (delcp->c_attr.va_nlink == 0) {
			delcp->c_flags |= CN_DESTROY;
		} else {
			delcp->c_flags |= CN_UPDATED;
		}
		mutex_exit(&delcp->c_statelock);
	}

	/* update the file we renamed */
	mutex_enter(&recp->c_statelock);
	recp->c_metadata.md_localctime = current_time;
	recp->c_metadata.md_flags |= MD_LOCALCTIME;
	recp->c_flags |= CN_UPDATED;
	mutex_exit(&recp->c_statelock);

	/* update the source directory */
	mutex_enter(&odcp->c_statelock);
	odcp->c_metadata.md_localctime = current_time;
	odcp->c_metadata.md_localmtime = current_time;
	odcp->c_metadata.md_flags |= MD_LOCALCTIME | MD_LOCALMTIME;
	odcp->c_flags |= CN_UPDATED;
	mutex_exit(&odcp->c_statelock);

	/* update the destination directory */
	if (odcp != ndcp) {
		mutex_enter(&ndcp->c_statelock);
		ndcp->c_metadata.md_localctime = current_time;
		ndcp->c_metadata.md_localmtime = current_time;
		ndcp->c_metadata.md_flags |= MD_LOCALCTIME | MD_LOCALMTIME;
		ndcp->c_flags |= CN_UPDATED;
		mutex_exit(&ndcp->c_statelock);
	}

out:
	if (commit) {
		/* commit the log entry */
		if (cachefs_dlog_commit(fscp, commit, error)) {
			/*EMPTY*/
			/* XXX bob: fix on panic */
		}
	}

	if (odcp != ndcp)
		rw_exit(&ndcp->c_rwlock);
	rw_exit(&odcp->c_rwlock);

	VN_RELE(revp);

	return (error);
}

static int
cachefs_mkdir(vnode_t *dvp, char *nm, vattr_t *vap, vnode_t **vpp,
    cred_t *cr)
{
	cnode_t *dcp = VTOC(dvp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	cachefscache_t *cachep = fscp->fs_cache;
	int error = 0;
	int held = 0;
	int connected = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_mkdir: ENTER dvp %p\n", (void *)dvp);
#endif

	if (fscp->fs_cache->c_flags & (CACHE_NOFILL | CACHE_NOCACHE))
		ASSERT(dcp->c_flags & CN_NOCACHE);

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			rw_exit(&dcp->c_rwlock);
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 1);
		if (error)
			break;
		rw_enter(&dcp->c_rwlock, RW_WRITER);
		held = 1;

		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			error = cachefs_mkdir_connected(dvp, nm, vap,
				vpp, cr);
			if (CFS_TIMEOUT(fscp, error)) {
				rw_exit(&dcp->c_rwlock);
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			}
		} else {
			error = cachefs_mkdir_disconnected(dvp, nm, vap,
				vpp, cr);
			if (CFS_TIMEOUT(fscp, error)) {
				connected = 1;
				continue;
			}
		}
		break;
	}

	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_MKDIR)) {
		fid_t *fidp = NULL;
		ino64_t fileno = 0;
		cnode_t *cp = NULL;
		if (error == 0)
			cp = VTOC(*vpp);

		if (cp != NULL) {
			fidp = &cp->c_metadata.md_cookie;
			fileno = cp->c_id.cid_fileno;
		}

		cachefs_log_mkdir(cachep, error, fscp->fs_cfsvfsp,
		    fidp, fileno, cr->cr_uid);
	}

	if (held) {
		rw_exit(&dcp->c_rwlock);
		cachefs_cd_release(fscp);
	}
	if (error == 0 && CFS_ISFS_NONSHARED(fscp))
		(void) cachefs_pack(dvp, nm, cr);

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_mkdir: EXIT error = %d\n", error);
#endif
	return (error);
}

static int
cachefs_mkdir_connected(vnode_t *dvp, char *nm, vattr_t *vap,
    vnode_t **vpp, cred_t *cr)
{
	cnode_t *newcp = NULL, *dcp = VTOC(dvp);
	struct vnode *vp = NULL;
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	struct fid cookie;
	struct vattr attr;
	cfs_cid_t cid, dircid;

	if (fscp->fs_cache->c_flags & (CACHE_NOFILL | CACHE_NOCACHE))
		ASSERT(dcp->c_flags & CN_NOCACHE);

	mutex_enter(&dcp->c_statelock);

	/* get backvp of dir */
	if (dcp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, dcp);
		if (error) {
			mutex_exit(&dcp->c_statelock);
			goto out;
		}
	}

	/* consistency check the directory */
	error = CFSOP_CHECK_COBJECT(fscp, dcp, 0, cr);
	if (error) {
		mutex_exit(&dcp->c_statelock);
		goto out;
	}
	dircid = dcp->c_id;

	/* make the dir on the back fs */
	error = VOP_MKDIR(dcp->c_backvp, nm, vap, &vp, cr);
	mutex_exit(&dcp->c_statelock);
	if (error) {
		goto out;
	}

	/* get the cookie and make the cnode */
	attr.va_mask = AT_ALL;
	error = cachefs_getcookie(vp, &cookie, &attr, cr);
	if (error) {
		goto out;
	}
	cid.cid_flags = 0;
	cid.cid_fileno = attr.va_nodeid;
	error = cachefs_cnode_make(&cid, fscp, &cookie, &attr,
	    vp, cr, 0, &newcp);
	if (error) {
		goto out;
	}
	ASSERT(CTOV(newcp)->v_type == VDIR);
	*vpp = CTOV(newcp);

	/* if the dir is populated, add the new entry */
	mutex_enter(&dcp->c_statelock);
	if (CFS_ISFS_NONSHARED(fscp) &&
	    (dcp->c_metadata.md_flags & MD_POPULATED)) {
		error = cachefs_dir_enter(dcp, nm, &cookie, &newcp->c_id,
		    SM_ASYNC);
		if (error) {
			cachefs_nocache(dcp);
			error = 0;
		}
	}
	dcp->c_attr.va_nlink++;
	dcp->c_flags |= CN_UPDATED;
	CFSOP_MODIFY_COBJECT(fscp, dcp, cr);
	mutex_exit(&dcp->c_statelock);

	/* XXX bob: should we do a filldir here? or just add . and .. */
	/* maybe should kick off an async filldir so caller does not wait */

	/* put the entry in the dnlc */
	if (cachefs_dnlc)
		dnlc_enter(dvp, nm, *vpp, cr);

	/* save the fileno of the parent so can find the name */
	if (bcmp(&newcp->c_metadata.md_parent, &dircid,
	    sizeof (cfs_cid_t)) != 0) {
		mutex_enter(&newcp->c_statelock);
		newcp->c_metadata.md_parent = dircid;
		newcp->c_flags |= CN_UPDATED;
		mutex_exit(&newcp->c_statelock);
	}
out:
	if (vp)
		VN_RELE(vp);

	return (error);
}

static int
cachefs_mkdir_disconnected(vnode_t *dvp, char *nm, vattr_t *vap,
    vnode_t **vpp, cred_t *cr)
{
	cnode_t *dcp = VTOC(dvp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	int error;
	cnode_t *newcp = NULL;
	struct vattr va;
	timestruc_t current_time;
	off_t commit = 0;
	char *s;
	int namlen;

	/* don't allow '/' characters in pathname component */
	for (s = nm, namlen = 0; *s; s++, namlen++)
		if (*s == '/')
			return (EACCES);
	if (namlen == 0)
		return (EINVAL);

	if (CFS_ISFS_WRITE_AROUND(fscp))
		return (ETIMEDOUT);

	mutex_enter(&dcp->c_statelock);

	/* check permissions */
	if (error = cachefs_access_local(dcp, (VEXEC|VWRITE), cr)) {
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	/* the directory front file must be populated */
	if ((dcp->c_metadata.md_flags & MD_POPULATED) == 0) {
		error = ETIMEDOUT;
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	/* make sure nm does not already exist in the directory */
	error = cachefs_dir_look(dcp, nm, NULL, NULL, NULL, NULL);
	if (error == ENOTDIR) {
		error = ETIMEDOUT;
		mutex_exit(&dcp->c_statelock);
		goto out;
	}
	if (error != ENOENT) {
		error = EEXIST;
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	/* make up a reasonable set of attributes */
	cachefs_attr_setup(vap, &va, dcp, cr);
	va.va_type = VDIR;
	va.va_mode |= S_IFDIR;
	va.va_nlink = 2;

	mutex_exit(&dcp->c_statelock);

	/* create the cnode */
	error = cachefs_cnode_create(fscp, &va, 0, &newcp);
	if (error)
		goto out;

	mutex_enter(&newcp->c_statelock);

	error = cachefs_dlog_cidmap(fscp);
	if (error) {
		mutex_exit(&newcp->c_statelock);
		goto out;
	}

	cachefs_creategid(dcp, newcp, vap, cr);
	mutex_enter(&dcp->c_statelock);
	cachefs_createacl(dcp, newcp);
	mutex_exit(&dcp->c_statelock);
	current_time = hrestime;
	newcp->c_metadata.md_vattr.va_atime = current_time;
	newcp->c_metadata.md_localctime = current_time;
	newcp->c_metadata.md_localmtime = current_time;
	newcp->c_metadata.md_flags |= MD_MAPPING | MD_LOCALMTIME |
	    MD_LOCALCTIME;
	newcp->c_flags |= CN_UPDATED;

	/* make a front file for the new directory, add . and .. */
	error = cachefs_dir_new(dcp, newcp);
	if (error) {
		mutex_exit(&newcp->c_statelock);
		goto out;
	}
	cachefs_modified(newcp);

	/*
	 * write the metadata now rather than waiting until
	 * inactive so that if there's no space we can let
	 * the caller know.
	 */
	ASSERT(newcp->c_frontvp);
	ASSERT((newcp->c_filegrp->fg_flags & CFS_FG_ALLOC_ATTR) == 0);
	ASSERT((newcp->c_flags & CN_ALLOC_PENDING) == 0);
	error = filegrp_write_metadata(newcp->c_filegrp,
	    &newcp->c_id, &newcp->c_metadata);
	if (error) {
		mutex_exit(&newcp->c_statelock);
		goto out;
	}
	mutex_exit(&newcp->c_statelock);

	/* log the operation */
	commit = cachefs_dlog_mkdir(fscp, dcp, newcp, nm, &va, cr);
	if (commit == 0) {
		error = ENOSPC;
		goto out;
	}

	mutex_enter(&dcp->c_statelock);

	/* make sure directory is still populated */
	if ((dcp->c_metadata.md_flags & MD_POPULATED) == 0) {
		mutex_exit(&dcp->c_statelock);
		error = ETIMEDOUT;
		goto out;
	}
	cachefs_modified(dcp);

	/* enter the new file in the directory */
	error = cachefs_dir_enter(dcp, nm, &newcp->c_metadata.md_cookie,
		&newcp->c_id, SM_ASYNC);
	if (error) {
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	/* update parent dir times */
	dcp->c_metadata.md_localctime = current_time;
	dcp->c_metadata.md_localmtime = current_time;
	dcp->c_metadata.md_flags |= MD_LOCALCTIME | MD_LOCALMTIME;
	dcp->c_attr.va_nlink++;
	dcp->c_flags |= CN_UPDATED;
	mutex_exit(&dcp->c_statelock);

out:
	if (commit) {
		/* commit the log entry */
		if (cachefs_dlog_commit(fscp, commit, error)) {
			/*EMPTY*/
			/* XXX bob: fix on panic */
		}
	}
	if (error) {
		if (newcp) {
			mutex_enter(&newcp->c_statelock);
			newcp->c_flags |= CN_DESTROY;
			mutex_exit(&newcp->c_statelock);
			VN_RELE(CTOV(newcp));
		}
	} else {
		*vpp = CTOV(newcp);
	}
	return (error);
}

static int
cachefs_rmdir(vnode_t *dvp, char *nm, vnode_t *cdir, cred_t *cr)
{
	cnode_t *dcp = VTOC(dvp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	cachefscache_t *cachep = fscp->fs_cache;
	int error = 0;
	int held = 0;
	int connected = 0;
	size_t namlen;
	vnode_t *vp = NULL;
	int vfslock = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_rmdir: ENTER vp %p\n", (void *)dvp);
#endif

	if (fscp->fs_cache->c_flags & (CACHE_NOFILL | CACHE_NOCACHE))
		ASSERT(dcp->c_flags & CN_NOCACHE);

	for (;;) {
		if (vfslock) {
			vn_vfsunlock(vp);
			vfslock = 0;
		}
		if (vp) {
			VN_RELE(vp);
			vp = NULL;
		}

		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 1);
		if (error)
			break;
		held = 1;

		/* if disconnected, do some extra error checking */
		if (fscp->fs_cdconnected != CFS_CD_CONNECTED) {
			/* check permissions */
			mutex_enter(&dcp->c_statelock);
			error = cachefs_access_local(dcp, (VEXEC|VWRITE), cr);
			mutex_exit(&dcp->c_statelock);
			if (CFS_TIMEOUT(fscp, error)) {
				connected = 1;
				continue;
			}
			if (error)
				break;

			namlen = strlen(nm);
			if (namlen == 0) {
				error = EINVAL;
				break;
			}

			/* cannot remove . and .. */
			if (nm[0] == '.') {
				if (namlen == 1) {
					error = EINVAL;
					break;
				} else if (namlen == 2 && nm[1] == '.') {
					error = EEXIST;
					break;
				}
			}

		}

		/* get the cnode of the dir to remove */
		error = cachefs_lookup_common(dvp, nm, &vp, NULL, 0, NULL, cr);
		if (error) {
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
				if (CFS_TIMEOUT(fscp, error)) {
					cachefs_cd_release(fscp);
					held = 0;
					cachefs_cd_timedout(fscp);
					connected = 0;
					continue;
				}
			} else {
				if (CFS_TIMEOUT(fscp, error)) {
					connected = 1;
					continue;
				}
			}
			break;
		}

		/* must be a dir */
		if (vp->v_type != VDIR) {
			error = ENOTDIR;
			break;
		}

		/* must not be current dir */
		if (VOP_CMP(vp, cdir)) {
			error = EINVAL;
			break;
		}

		/* see ufs_dirremove for why this is done, mount race */
		if (vn_vfslock(vp)) {
			error = EBUSY;
			break;
		}
		vfslock = 1;
		if (vp->v_vfsmountedhere != NULL) {
			error = EBUSY;
			break;
		}

		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			error = cachefs_rmdir_connected(dvp, nm, cdir,
				cr, vp);
			if (CFS_TIMEOUT(fscp, error)) {
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			}
		} else {
			error = cachefs_rmdir_disconnected(dvp, nm, cdir,
				cr, vp);
			if (CFS_TIMEOUT(fscp, error)) {
				connected = 1;
				continue;
			}
		}
		break;
	}

	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_RMDIR)) {
		ino64_t fileno = 0;
		fid_t *fidp = NULL;
		cnode_t *cp = NULL;
		if (vp)
			cp = VTOC(vp);

		if (cp != NULL) {
			fidp = &cp->c_metadata.md_cookie;
			fileno = cp->c_id.cid_fileno;
		}

		cachefs_log_rmdir(cachep, error, fscp->fs_cfsvfsp,
		    fidp, fileno, cr->cr_uid);
	}

	if (held) {
		cachefs_cd_release(fscp);
	}

	if (vfslock)
		vn_vfsunlock(vp);

	if (vp)
		VN_RELE(vp);

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_rmdir: EXIT error = %d\n", error);
#endif

	return (error);
}

static int
cachefs_rmdir_connected(vnode_t *dvp, char *nm, vnode_t *cdir, cred_t *cr,
    vnode_t *vp)
{
	cnode_t *dcp = VTOC(dvp);
	cnode_t *cp = VTOC(vp);
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(dcp);

	rw_enter(&dcp->c_rwlock, RW_WRITER);
	mutex_enter(&dcp->c_statelock);
	mutex_enter(&cp->c_statelock);

	if (dcp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, dcp);
		if (error) {
			mutex_exit(&dcp->c_statelock);
			goto out;
		}
	}

	error = CFSOP_CHECK_COBJECT(fscp, dcp, 0, cr);
	if (error)
		goto out;

	/* rmdir on the back fs */
	error = VOP_RMDIR(dcp->c_backvp, nm, cdir, cr);
	if (error)
		goto out;

	/* if the dir is populated, remove the entry from it */
	if (CFS_ISFS_NONSHARED(fscp) &&
	    (dcp->c_metadata.md_flags & MD_POPULATED)) {
		error = cachefs_dir_rmentry(dcp, nm);
		if (error) {
			cachefs_nocache(dcp);
			error = 0;
		}
	}

	/*
	 * *if* the (hard) link count goes to 0, then we set the CDESTROY
	 * flag on the cnode. The cached object will then be destroyed
	 * at inactive time where the chickens come home to roost :-)
	 * The link cnt for directories is bumped down by 2 'cause the "."
	 * entry has to be elided too ! The link cnt for the parent goes down
	 * by 1 (because of "..").
	 */
	cp->c_attr.va_nlink -= 2;
	dcp->c_attr.va_nlink--;
	if (cp->c_attr.va_nlink == 0) {
		cp->c_flags |= CN_DESTROY;
	} else {
		cp->c_flags |= CN_UPDATED;
	}
	dcp->c_flags |= CN_UPDATED;

	dnlc_purge_vp(vp);
	CFSOP_MODIFY_COBJECT(fscp, dcp, cr);

out:
	mutex_exit(&cp->c_statelock);
	mutex_exit(&dcp->c_statelock);
	rw_exit(&dcp->c_rwlock);

	return (error);
}

static int
/*ARGSUSED*/
cachefs_rmdir_disconnected(vnode_t *dvp, char *nm, vnode_t *cdir,
    cred_t *cr, vnode_t *vp)
{
	cnode_t *dcp = VTOC(dvp);
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	int error = 0;
	off_t commit = 0;
	timestruc_t current_time;

	if (CFS_ISFS_WRITE_AROUND(fscp))
		return (ETIMEDOUT);

	rw_enter(&dcp->c_rwlock, RW_WRITER);
	mutex_enter(&dcp->c_statelock);
	mutex_enter(&cp->c_statelock);

	/* both directories must be populated */
	if (((dcp->c_metadata.md_flags & MD_POPULATED) == 0) ||
	    ((cp->c_metadata.md_flags & MD_POPULATED) == 0)) {
		error = ETIMEDOUT;
		goto out;
	}

	/* if sticky bit set on the dir, more access checks to perform */
	if (error = cachefs_stickyrmchk(dcp, cp, cr)) {
		goto out;
	}

	/* make sure dir is empty */
	if (cp->c_attr.va_nlink > 2) {
		error = cachefs_dir_empty(cp);
		if (error) {
			if (error == ENOTDIR)
				error = ETIMEDOUT;
			goto out;
		}
		cachefs_modified(cp);
	}
	cachefs_modified(dcp);

	/* log the operation */
	commit = cachefs_dlog_rmdir(fscp, dcp, nm, cp, cr);
	if (commit == 0) {
		error = ENOSPC;
		goto out;
	}

	/* remove name from parent dir */
	error = cachefs_dir_rmentry(dcp, nm);
	if (error == ENOTDIR) {
		error = ETIMEDOUT;
		goto out;
	}
	if (error)
		goto out;

	current_time = hrestime;

	/* update deleted dir values */
	cp->c_attr.va_nlink -= 2;
	if (cp->c_attr.va_nlink == 0)
		cp->c_flags |= CN_DESTROY;
	else {
		cp->c_metadata.md_localctime = current_time;
		cp->c_metadata.md_flags |= MD_LOCALCTIME;
		cp->c_flags |= CN_UPDATED;
	}

	/* update parent values */
	dcp->c_metadata.md_localctime = current_time;
	dcp->c_metadata.md_localmtime = current_time;
	dcp->c_metadata.md_flags |= MD_LOCALCTIME | MD_LOCALMTIME;
	dcp->c_attr.va_nlink--;
	dcp->c_flags |= CN_UPDATED;

out:
	mutex_exit(&cp->c_statelock);
	mutex_exit(&dcp->c_statelock);
	rw_exit(&dcp->c_rwlock);
	if (commit) {
		/* commit the log entry */
		if (cachefs_dlog_commit(fscp, commit, error)) {
			/*EMPTY*/
			/* XXX bob: fix on panic */
		}
		dnlc_purge_vp(vp);
	}
	return (error);
}

static int
cachefs_symlink(vnode_t *dvp, char *lnm, vattr_t *tva,
    char *tnm, cred_t *cr)
{
	cnode_t *dcp = VTOC(dvp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	cachefscache_t *cachep = fscp->fs_cache;
	int error = 0;
	int held = 0;
	int connected = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_symlink: ENTER dvp %p lnm %s tnm %s\n",
		    (void *)dvp, lnm, tnm);
#endif

	if (fscp->fs_cache->c_flags & CACHE_NOCACHE)
		ASSERT(dcp->c_flags & CN_NOCACHE);

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			rw_exit(&dcp->c_rwlock);
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 1);
		if (error)
			break;
		rw_enter(&dcp->c_rwlock, RW_WRITER);
		held = 1;

		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			error = cachefs_symlink_connected(dvp, lnm, tva,
				tnm, cr);
			if (CFS_TIMEOUT(fscp, error)) {
				rw_exit(&dcp->c_rwlock);
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			}
		} else {
			error = cachefs_symlink_disconnected(dvp, lnm, tva,
				tnm, cr);
			if (CFS_TIMEOUT(fscp, error)) {
				connected = 1;
				continue;
			}
		}
		break;
	}

	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_SYMLINK))
		cachefs_log_symlink(cachep, error, fscp->fs_cfsvfsp,
		    &dcp->c_metadata.md_cookie, dcp->c_id.cid_fileno,
		    cr->cr_uid, (uint_t)strlen(tnm));

	if (held) {
		rw_exit(&dcp->c_rwlock);
		cachefs_cd_release(fscp);
	}

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_symlink: EXIT error = %d\n", error);
#endif
	return (error);
}

static int
cachefs_symlink_connected(vnode_t *dvp, char *lnm, vattr_t *tva,
    char *tnm, cred_t *cr)
{
	cnode_t *dcp = VTOC(dvp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	int error = 0;
	vnode_t *backvp = NULL;
	cnode_t *newcp = NULL;
	struct vattr va;
	struct fid cookie;
	cfs_cid_t cid;

	mutex_enter(&dcp->c_statelock);

	if (dcp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, dcp);
		if (error) {
			cachefs_nocache(dcp);
			mutex_exit(&dcp->c_statelock);
			goto out;
		}
	}

	error = CFSOP_CHECK_COBJECT(fscp, dcp, 0, cr);
	if (error) {
		mutex_exit(&dcp->c_statelock);
		goto out;
	}
	error = VOP_SYMLINK(dcp->c_backvp, lnm, tva, tnm, cr);
	if (error) {
		mutex_exit(&dcp->c_statelock);
		goto out;
	}
	if ((dcp->c_filegrp->fg_flags & CFS_FG_WRITE) == 0) {
		cachefs_nocache(dcp);
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	CFSOP_MODIFY_COBJECT(fscp, dcp, cr);

	/* lookup the symlink we just created and get its fid and attrs */
	(void) VOP_LOOKUP(dcp->c_backvp, lnm, &backvp, NULL, 0, NULL, cr);
	if (backvp == NULL) {
		cachefs_nocache(dcp);
		mutex_exit(&dcp->c_statelock);
		goto out;
	}
	error = cachefs_getcookie(backvp, &cookie, &va, cr);
	if (error) {
		error = 0;
		cachefs_nocache(dcp);
		mutex_exit(&dcp->c_statelock);
		goto out;
	}
	cid.cid_fileno = va.va_nodeid;
	cid.cid_flags = 0;

	/* if the dir is cached, add the symlink to it */
	if (CFS_ISFS_NONSHARED(fscp) &&
	    (dcp->c_metadata.md_flags & MD_POPULATED)) {
		error = cachefs_dir_enter(dcp, lnm, &cookie, &cid, SM_ASYNC);
		if (error) {
			cachefs_nocache(dcp);
			error = 0;
		}
	}
	mutex_exit(&dcp->c_statelock);

	/* make the cnode for the sym link */
	error = cachefs_cnode_make(&cid, fscp, &cookie, &va, backvp,
	    cr, 0, &newcp);
	if (error) {
		cachefs_nocache(dcp);
		error = 0;
		goto out;
	}

	/* try to cache the symlink contents */
	mutex_enter(&newcp->c_statelock);
	error = cachefs_stuffsymlink(newcp, tnm, (int)newcp->c_size);
	if (error) {
		cachefs_nocache(newcp);
		error = 0;
	}
	mutex_exit(&newcp->c_statelock);

out:
	if (backvp)
		VN_RELE(backvp);
	if (newcp)
		VN_RELE(CTOV(newcp));
	return (error);
}

static int
cachefs_symlink_disconnected(vnode_t *dvp, char *lnm, vattr_t *tva,
    char *tnm, cred_t *cr)
{
	cnode_t *dcp = VTOC(dvp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	int error;
	cnode_t *newcp = NULL;
	struct vattr va;
	timestruc_t current_time;
	off_t commit = 0;

	if (CFS_ISFS_WRITE_AROUND(fscp))
		return (ETIMEDOUT);

	mutex_enter(&dcp->c_statelock);

	/* check permissions */
	if (error = cachefs_access_local(dcp, (VEXEC|VWRITE), cr)) {
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	/* the directory front file must be populated */
	if ((dcp->c_metadata.md_flags & MD_POPULATED) == 0) {
		error = ETIMEDOUT;
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	/* make sure lnm does not already exist in the directory */
	error = cachefs_dir_look(dcp, lnm, NULL, NULL, NULL, NULL);
	if (error == ENOTDIR) {
		error = ETIMEDOUT;
		mutex_exit(&dcp->c_statelock);
		goto out;
	}
	if (error != ENOENT) {
		error = EEXIST;
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	/* make up a reasonable set of attributes */
	cachefs_attr_setup(tva, &va, dcp, cr);
	va.va_type = VLNK;
	va.va_mode |= S_IFLNK;
	va.va_size = strlen(tnm);

	mutex_exit(&dcp->c_statelock);

	/* create the cnode */
	error = cachefs_cnode_create(fscp, &va, 0, &newcp);
	if (error)
		goto out;

	mutex_enter(&newcp->c_statelock);

	error = cachefs_dlog_cidmap(fscp);
	if (error) {
		mutex_exit(&newcp->c_statelock);
		error = ENOSPC;
		goto out;
	}

	cachefs_creategid(dcp, newcp, tva, cr);
	mutex_enter(&dcp->c_statelock);
	cachefs_createacl(dcp, newcp);
	mutex_exit(&dcp->c_statelock);
	current_time = hrestime;
	newcp->c_metadata.md_vattr.va_atime = current_time;
	newcp->c_metadata.md_localctime = current_time;
	newcp->c_metadata.md_localmtime = current_time;
	newcp->c_metadata.md_flags |= MD_MAPPING | MD_LOCALMTIME |
	    MD_LOCALCTIME;
	newcp->c_flags |= CN_UPDATED;

	/* log the operation */
	commit = cachefs_dlog_symlink(fscp, dcp, newcp, lnm, tva, tnm, cr);
	if (commit == 0) {
		mutex_exit(&newcp->c_statelock);
		error = ENOSPC;
		goto out;
	}

	/* store the symlink contents */
	error = cachefs_stuffsymlink(newcp, tnm, (int)newcp->c_size);
	if (error) {
		mutex_exit(&newcp->c_statelock);
		goto out;
	}
	if (cachefs_modified_alloc(newcp)) {
		mutex_exit(&newcp->c_statelock);
		error = ENOSPC;
		goto out;
	}

	/*
	 * write the metadata now rather than waiting until
	 * inactive so that if there's no space we can let
	 * the caller know.
	 */
	if (newcp->c_flags & CN_ALLOC_PENDING) {
		if (newcp->c_filegrp->fg_flags & CFS_FG_ALLOC_ATTR) {
			(void) filegrp_allocattr(newcp->c_filegrp);
		}
		error = filegrp_create_metadata(newcp->c_filegrp,
		    &newcp->c_metadata, &newcp->c_id);
		if (error) {
			mutex_exit(&newcp->c_statelock);
			goto out;
		}
		newcp->c_flags &= ~CN_ALLOC_PENDING;
	}
	error = filegrp_write_metadata(newcp->c_filegrp,
	    &newcp->c_id, &newcp->c_metadata);
	if (error) {
		mutex_exit(&newcp->c_statelock);
		goto out;
	}
	mutex_exit(&newcp->c_statelock);

	mutex_enter(&dcp->c_statelock);

	/* enter the new file in the directory */
	if ((dcp->c_metadata.md_flags & MD_POPULATED) == 0) {
		error = ETIMEDOUT;
		mutex_exit(&dcp->c_statelock);
		goto out;
	}
	cachefs_modified(dcp);
	error = cachefs_dir_enter(dcp, lnm, &newcp->c_metadata.md_cookie,
		&newcp->c_id, SM_ASYNC);
	if (error) {
		mutex_exit(&dcp->c_statelock);
		goto out;
	}

	/* update parent dir times */
	dcp->c_metadata.md_localctime = current_time;
	dcp->c_metadata.md_localmtime = current_time;
	dcp->c_metadata.md_flags |= MD_LOCALMTIME | MD_LOCALCTIME;
	dcp->c_flags |= CN_UPDATED;
	mutex_exit(&dcp->c_statelock);

out:
	if (commit) {
		/* commit the log entry */
		if (cachefs_dlog_commit(fscp, commit, error)) {
			/*EMPTY*/
			/* XXX bob: fix on panic */
		}
	}

	if (error) {
		if (newcp) {
			mutex_enter(&newcp->c_statelock);
			newcp->c_flags |= CN_DESTROY;
			mutex_exit(&newcp->c_statelock);
		}
	}
	if (newcp) {
		VN_RELE(CTOV(newcp));
	}

	return (error);
}

static int
cachefs_readdir(vnode_t *vp, uio_t *uiop, cred_t *cr, int *eofp)
{
	cnode_t *dcp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	cachefscache_t *cachep = fscp->fs_cache;
	int error = 0;
	int held = 0;
	int connected = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_readdir: ENTER vp %p\n", (void *)vp);
#endif

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			rw_exit(&dcp->c_rwlock);
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			break;
		rw_enter(&dcp->c_rwlock, RW_READER);
		held = 1;

		/* quit if link count of zero (posix) */
		if (dcp->c_attr.va_nlink == 0) {
			if (eofp)
				*eofp = 1;
			error = 0;
			break;
		}

		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			error = cachefs_readdir_connected(vp, uiop, cr,
			    eofp);
			if (CFS_TIMEOUT(fscp, error)) {
				rw_exit(&dcp->c_rwlock);
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			}
		} else {
			error = cachefs_readdir_disconnected(vp, uiop, cr,
			    eofp);
			if (CFS_TIMEOUT(fscp, error)) {
				if (cachefs_cd_access_miss(fscp)) {
					error = cachefs_readdir_connected(vp,
					    uiop, cr, eofp);
					if (!CFS_TIMEOUT(fscp, error))
						break;
					delay(5*hz);
					connected = 0;
					continue;
				}
				connected = 1;
				continue;
			}
		}
		break;
	}

	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_READDIR))
		cachefs_log_readdir(cachep, error, fscp->fs_cfsvfsp,
		&dcp->c_metadata.md_cookie, dcp->c_id.cid_fileno,
		cr->cr_uid, uiop->uio_loffset, *eofp);

	if (held) {
		rw_exit(&dcp->c_rwlock);
		cachefs_cd_release(fscp);
	}

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_readdir: EXIT error = %d\n", error);
#endif

	return (error);
}

static int
cachefs_readdir_connected(vnode_t *vp, uio_t *uiop, cred_t *cr, int *eofp)
{
	cnode_t *dcp = VTOC(vp);
	int error;
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	cachefscache_t *cachep = fscp->fs_cache;
	struct cachefs_req *rp;

	mutex_enter(&dcp->c_statelock);

	/* check directory consistency */
	error = CFSOP_CHECK_COBJECT(fscp, dcp, 0, cr);
	if (error)
		goto out;
	dcp->c_usage++;

	/* if dir was modified, toss old contents */
	if (dcp->c_metadata.md_flags & MD_INVALREADDIR)
		cachefs_inval_object(dcp);

	error = 0;
	if (((dcp->c_metadata.md_flags & MD_POPULATED) == 0) &&
	    ((dcp->c_flags & (CN_ASYNC_POPULATE | CN_NOCACHE)) == 0) &&
	    (fscp->fs_cdconnected == CFS_CD_CONNECTED)) {

		if (cachefs_async_okay()) {

			/*
			 * Set up asynchronous request to fill this
			 * directory.
			 */

			dcp->c_flags |= CN_ASYNC_POPULATE;

			rp = kmem_cache_alloc(cachefs_req_cache, KM_SLEEP);
			rp->cfs_cmd = CFS_POPULATE;
			rp->cfs_req_u.cu_populate.cpop_vp = vp;
			rp->cfs_cr = cr;

			crhold(cr);
			VN_HOLD(vp);

			error = cachefs_addqueue(rp, &fscp->fs_workq);
			if (error) {
				error = cachefs_addqueue(rp, &cachep->c_workq);
				ASSERT(error == 0);
			}
		} else {
			error = cachefs_dir_fill(dcp, cr);
			if (error != 0)
				cachefs_nocache(dcp);
		}
	}

	/* if front file is populated */
	if (((dcp->c_flags & (CN_NOCACHE | CN_ASYNC_POPULATE)) == 0) &&
	    (dcp->c_metadata.md_flags & MD_POPULATED)) {
		error = cachefs_dir_read(dcp, uiop, eofp);
		if (error == 0)
			fscp->fs_stats.st_hits++;
	}

	/* if front file could not be used */
	if ((error != 0) ||
	    (dcp->c_flags & (CN_NOCACHE | CN_ASYNC_POPULATE)) ||
	    ((dcp->c_metadata.md_flags & MD_POPULATED) == 0)) {
		if (error && !(dcp->c_flags & CN_NOCACHE))
			cachefs_nocache(dcp);

		/* get the back vp */
		if (dcp->c_backvp == NULL) {
			error = cachefs_getbackvp(fscp, dcp);
			if (error)
				goto out;
		}

		if (fscp->fs_inum_size > 0) {
			error = cachefs_readback_translate(dcp, uiop, cr, eofp);
		} else {
			/* do the dir read from the back fs */
			VOP_RWLOCK(dcp->c_backvp, 0);
			error = VOP_READDIR(dcp->c_backvp, uiop, cr, eofp);
			VOP_RWUNLOCK(dcp->c_backvp, 0);
		}

		if (error == 0)
			fscp->fs_stats.st_misses++;
	}

out:
	mutex_exit(&dcp->c_statelock);

	return (error);
}

static int
cachefs_readback_translate(cnode_t *cp, uio_t *uiop, cred_t *cr, int *eofp)
{
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	caddr_t buffy = NULL;
	int buffysize = MAXBSIZE;
	caddr_t chrp, end;
	ino64_t newinum;
	struct dirent64 *de;
	uio_t uioin;
	iovec_t iov;

	ASSERT(cp->c_backvp != NULL);
	ASSERT(fscp->fs_inum_size > 0);

	if (uiop->uio_resid < buffysize)
		buffysize = (int)uiop->uio_resid;
	buffy = cachefs_kmem_alloc(buffysize, KM_SLEEP);

	iov.iov_base = buffy;
	iov.iov_len = buffysize;
	uioin.uio_iov = &iov;
	uioin.uio_iovcnt = 1;
	uioin.uio_segflg = UIO_SYSSPACE;
	uioin.uio_fmode = 0;
	uioin.uio_loffset = uiop->uio_loffset;
	uioin.uio_resid = buffysize;

	VOP_RWLOCK(cp->c_backvp, 0);
	error = VOP_READDIR(cp->c_backvp, &uioin, cr, eofp);
	VOP_RWUNLOCK(cp->c_backvp, 0);

	if (error != 0)
		goto out;

	end = buffy + buffysize - uioin.uio_resid;

	mutex_exit(&cp->c_statelock);
	mutex_enter(&fscp->fs_fslock);


	for (chrp = buffy; chrp < end; chrp += de->d_reclen) {
		de = (dirent64_t *)chrp;
		newinum = cachefs_inum_real2fake(fscp, de->d_ino);
		if (newinum == 0)
			newinum = cachefs_fileno_conflict(fscp, de->d_ino);
		de->d_ino = newinum;
	}
	mutex_exit(&fscp->fs_fslock);
	mutex_enter(&cp->c_statelock);

	error = uiomove(buffy, end - buffy, UIO_READ, uiop);
	uiop->uio_loffset = uioin.uio_loffset;

out:

	if (buffy != NULL)
		cachefs_kmem_free(buffy, buffysize);

	return (error);
}

static int
/*ARGSUSED*/
cachefs_readdir_disconnected(vnode_t *vp, uio_t *uiop, cred_t *cr,
    int *eofp)
{
	cnode_t *dcp = VTOC(vp);
	int error;

	mutex_enter(&dcp->c_statelock);
	if ((dcp->c_metadata.md_flags & MD_POPULATED) == 0) {
		error = ETIMEDOUT;
	} else {
		error = cachefs_dir_read(dcp, uiop, eofp);
		if (error == ENOTDIR)
			error = ETIMEDOUT;
	}
	mutex_exit(&dcp->c_statelock);

	return (error);
}

static int
cachefs_fid(struct vnode *vp, struct fid *fidp)
{
	int error = 0;
	struct cnode *cp = VTOC(vp);

	mutex_enter(&cp->c_statelock);
	if (fidp->fid_len < cp->c_metadata.md_cookie.fid_len) {
		fidp->fid_len = cp->c_metadata.md_cookie.fid_len;
		error = ENOSPC;
	} else {
		bcopy(cp->c_metadata.md_cookie.fid_data, fidp->fid_data,
		    cp->c_metadata.md_cookie.fid_len);
		fidp->fid_len = cp->c_metadata.md_cookie.fid_len;
	}
	mutex_exit(&cp->c_statelock);
	return (error);
}

static void
cachefs_rwlock(struct vnode *vp, int write_lock)
{
	cnode_t *cp = VTOC(vp);

	/*
	 * XXX - This is ifdef'ed out for now. The problem -
	 * getdents() acquires the read version of rwlock, then we come
	 * into cachefs_readdir() and that wants to acquire the write version
	 * of this lock (if its going to populate the directory). This is
	 * a problem, this can be solved by introducing another lock in the
	 * cnode.
	 */
/* XXX */
	if (vp->v_type != VREG)
		return;
	if (write_lock)
		rw_enter(&cp->c_rwlock, RW_WRITER);
	else
		rw_enter(&cp->c_rwlock, RW_READER);
}

/* ARGSUSED */
static void
cachefs_rwunlock(struct vnode *vp, int write_lock)
{
	cnode_t *cp = VTOC(vp);
	if (vp->v_type != VREG)
		return;
	rw_exit(&cp->c_rwlock);
}

/* ARGSUSED */
static int
cachefs_seek(struct vnode *vp, offset_t ooff, offset_t *noffp)
{
	return (0);
}

static int cachefs_lostpage = 0;
/*
 * Return all the pages from [off..off+len] in file
 */
static int
cachefs_getpage(struct vnode *vp, offset_t off, size_t len,
	uint_t *protp, struct page *pl[], size_t plsz, struct seg *seg,
	caddr_t addr, enum seg_rw rw, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	int error;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	cachefscache_t *cachep = fscp->fs_cache;
	int held = 0;
	int connected = 0;

#ifdef CFSDEBUG
	u_offset_t offx = (u_offset_t)off;

	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_getpage: ENTER vp %p off %lld len %lu rw %d\n",
		    (void *)vp, offx, len, rw);
#endif
	/* XXX sam: make this do an async populate? */
	if (pl == NULL) {
		error = 0;
		goto out;
	}
	if (vp->v_flag & VNOMAP) {
		error = ENOSYS;
		goto out;
	}
	if (protp != NULL)
		*protp = PROT_ALL;

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			break;
		held = 1;

		/*
		 * If we are getting called as a side effect of a
		 * cachefs_write()
		 * operation the local file size might not be extended yet.
		 * In this case we want to be able to return pages of zeroes.
		 */
		if ((u_offset_t)off + len >
			((cp->c_size + PAGEOFFSET) & (offset_t)PAGEMASK)) {
			if (seg != segkmap) {
				error = EFAULT;
				break;
			}
		}
		if (len <= PAGESIZE)
			error = cachefs_getapage(vp, (u_offset_t)off, len,
			    protp, pl, plsz, seg, addr, rw, cr);
		else
			error = pvn_getpages(cachefs_getapage, vp,
			    (u_offset_t)off, len, protp, pl, plsz, seg, addr,
			    rw, cr);
		if (error == 0)
			break;

		if (((cp->c_flags & CN_NOCACHE) && (error == ENOSPC)) ||
		    error == EAGAIN) {
			connected = 0;
			continue;
		}
		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			if (CFS_TIMEOUT(fscp, error)) {
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			}
		} else {
			if (CFS_TIMEOUT(fscp, error)) {
				if (cachefs_cd_access_miss(fscp)) {
					if (len <= PAGESIZE)
						error = cachefs_getapage_back(
						    vp, (u_offset_t)off,
						    len, protp, pl,
						    plsz, seg, addr, rw, cr);
					else
						error = pvn_getpages(
						    cachefs_getapage_back, vp,
						    (u_offset_t)off, len,
						    protp, pl,
						    plsz, seg, addr, rw, cr);
					if (!CFS_TIMEOUT(fscp, error) &&
					    (error != EAGAIN))
						break;
					delay(5*hz);
					connected = 0;
					continue;
				}
				connected = 1;
				continue;
			}
		}
		break;
	}

	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_GETPAGE))
		cachefs_log_getpage(cachep, error, vp->v_vfsp,
		    &cp->c_metadata.md_cookie, cp->c_id.cid_fileno,
		    cr->cr_uid, off, len);

	if (held) {
		cachefs_cd_release(fscp);
	}

out:
#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_getpage: EXIT vp %p error %d\n",
		    (void *)vp, error);
#endif
	return (error);
}

/*
 * Called from pvn_getpages or cachefs_getpage to get a particular page.
 */
/*ARGSUSED*/
static int
cachefs_getapage(struct vnode *vp, u_offset_t off, size_t len, uint_t *protp,
	struct page *pl[], size_t plsz, struct seg *seg, caddr_t addr,
	enum seg_rw rw, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	page_t **ppp, *pp = NULL;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	cachefscache_t *cachep = fscp->fs_cache;
	int error = 0;
	struct page **ourpl;
	struct page *ourstackpl[17]; /* see ASSERT() below for 17 */
	int index = 0;
	int downgrade;
	int have_statelock = 0;
	u_offset_t popoff;
	size_t popsize = 0;

	ASSERT(((DEF_POP_SIZE / PAGESIZE) + 1) <= 17);

	if (fscp->fs_info.fi_popsize > DEF_POP_SIZE)
		ourpl = cachefs_kmem_alloc(sizeof (struct page *) *
		    ((fscp->fs_info.fi_popsize / PAGESIZE) + 1), KM_SLEEP);
	else
		ourpl = ourstackpl;

	ourpl[0] = NULL;
	off = off & (offset_t)PAGEMASK;
again:
	/*
	 * Look for the page
	 */
	if (page_exists(vp, off) == 0) {
		/*
		* Need to do work to get the page.
		* Grab our lock because we are going to
		* modify the state of the cnode.
		*/
		if (! have_statelock) {
			mutex_enter(&cp->c_statelock);
			have_statelock = 1;
		}
		/*
		* If we're in NOCACHE mode, we will need a backvp
		*/
		if (cp->c_flags & CN_NOCACHE) {
			if (fscp->fs_cdconnected != CFS_CD_CONNECTED) {
				error = ETIMEDOUT;
				goto out;
			}
			if (cp->c_backvp == NULL) {
				error = cachefs_getbackvp(fscp, cp);
				if (error)
					goto out;
			}
			error = VOP_GETPAGE(cp->c_backvp, off,
					PAGESIZE, protp, ourpl, PAGESIZE, seg,
					addr, S_READ, cr);
			/*
			 * backfs returns EFAULT when we are trying for a
			 * page beyond EOF but cachefs has the knowledge that
			 * it is not beyond EOF be cause cp->c_size is
			 * greater then the offset requested.
			 */
			if (error == EFAULT) {
				error = 0;
				pp = page_create_va(vp, off, PAGESIZE,
				    PG_EXCL | PG_WAIT, seg, addr);
				if (pp == NULL)
					goto again;
				pagezero(pp, 0, PAGESIZE);
				pvn_plist_init(pp, pl, plsz, off, PAGESIZE, rw);
				goto out;
			}
			if (error)
				goto out;
			goto getpages;
		}
		/*
		* We need a front file. If we can't get it,
		* put the cnode in NOCACHE mode and try again.
		*/
		if (cp->c_frontvp == NULL) {
			error = cachefs_getfrontfile(cp);
			if (error) {
				cachefs_nocache(cp);
				error = EAGAIN;
				goto out;
			}
		}
		/*
		* Check if the front file needs population.
		* If population is necessary, make sure we have a
		* backvp as well. We will get the page from the backvp.
		* bug 4152459-
		* But if the file system is in disconnected mode
		* and the file is a local file then do not check the
		* allocmap.
		*/
		if (((fscp->fs_cdconnected == CFS_CD_CONNECTED) ||
		    ((cp->c_metadata.md_flags & MD_LOCALFILENO) == 0)) &&
		    (cachefs_check_allocmap(cp, off) == 0)) {
			if (fscp->fs_cdconnected != CFS_CD_CONNECTED) {
				error = ETIMEDOUT;
				goto out;
			}
			if (cp->c_backvp == NULL) {
				error = cachefs_getbackvp(fscp, cp);
				if (error)
					goto out;
			}
			if (cp->c_filegrp->fg_flags & CFS_FG_WRITE) {
				cachefs_cluster_allocmap(off, &popoff,
				    &popsize,
				    fscp->fs_info.fi_popsize, cp);
				if (popsize != 0) {
					error = cachefs_populate(cp,
					    popoff, popsize,
					    cp->c_frontvp, cp->c_backvp,
					    cp->c_size, cr);
					if (error) {
						cachefs_nocache(cp);
						error = EAGAIN;
						goto out;
					} else {
						cp->c_flags |=
						    CN_UPDATED |
						    CN_NEED_FRONT_SYNC |
						    CN_POPULATION_PENDING;
					}
					popsize = popsize - (off - popoff);
				} else {
					popsize = PAGESIZE;
				}
			}
			/* else XXX assert CN_NOCACHE? */
			error = VOP_GETPAGE(cp->c_backvp, (offset_t)off,
					PAGESIZE, protp, ourpl, popsize,
					seg, addr, S_READ, cr);
			if (error)
				goto out;
			fscp->fs_stats.st_misses++;
		} else {
			if (cp->c_flags & CN_POPULATION_PENDING) {
				error = VOP_FSYNC(cp->c_frontvp, FSYNC, cr);
				cp->c_flags &= ~CN_POPULATION_PENDING;
				if (error) {
					cachefs_nocache(cp);
					error = EAGAIN;
					goto out;
				}
			}
			/*
			* File was populated so we get the page from the
			* frontvp
			*/
			error = VOP_GETPAGE(cp->c_frontvp, (offset_t)off,
			    PAGESIZE, protp, ourpl, PAGESIZE, seg, addr,
			    rw, cr);
			if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_GPFRONT))
				cachefs_log_gpfront(cachep, error,
				    fscp->fs_cfsvfsp,
				    &cp->c_metadata.md_cookie, cp->c_fileno,
				    cr->cr_uid, off, PAGESIZE);
			if (error) {
				cachefs_nocache(cp);
				error = EAGAIN;
				goto out;
			}
			fscp->fs_stats.st_hits++;
		}
getpages:
		ASSERT(have_statelock);
		if (have_statelock) {
			mutex_exit(&cp->c_statelock);
			have_statelock = 0;
		}
		downgrade = 0;
		for (ppp = ourpl; *ppp; ppp++) {
			if ((*ppp)->p_offset < off) {
				index++;
				page_unlock(*ppp);
				continue;
			}
			if (PAGE_SHARED(*ppp)) {
				if (page_tryupgrade(*ppp) == 0) {
					for (ppp = &ourpl[index]; *ppp; ppp++)
						page_unlock(*ppp);
					error = EAGAIN;
					goto out;
				}
				downgrade = 1;
			}
			ASSERT(PAGE_EXCL(*ppp));
			(void) hat_pageunload((*ppp), HAT_FORCE_PGUNLOAD);
			page_rename(*ppp, vp, (*ppp)->p_offset);
		}
		pl[0] = ourpl[index];
		pl[1] = NULL;
		if (downgrade) {
			page_downgrade(ourpl[index]);
		}
		/* Unlock the rest of the pages from the cluster */
		for (ppp = &ourpl[index+1]; *ppp; ppp++)
			page_unlock(*ppp);
	} else {
		ASSERT(! have_statelock);
		if (have_statelock) {
			mutex_exit(&cp->c_statelock);
			have_statelock = 0;
		}
		/* XXX SE_SHARED probably isn't what we *always* want */
		if ((pp = page_lookup(vp, off, SE_SHARED)) == NULL) {
			cachefs_lostpage++;
			goto again;
		}
		pl[0] = pp;
		pl[1] = NULL;
		/* XXX increment st_hits?  i don't think so, but... */
	}

out:
	if (have_statelock) {
		mutex_exit(&cp->c_statelock);
		have_statelock = 0;
	}
	if (fscp->fs_info.fi_popsize > DEF_POP_SIZE)
		cachefs_kmem_free(ourpl, sizeof (struct page *) *
		    ((fscp->fs_info.fi_popsize / PAGESIZE) + 1));
	return (error);
}

/* gets a page but only from the back fs */
/*ARGSUSED*/
static int
cachefs_getapage_back(struct vnode *vp, u_offset_t off, size_t len,
    uint_t *protp, struct page *pl[], size_t plsz, struct seg *seg,
    caddr_t addr, enum seg_rw rw, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	page_t **ppp, *pp = NULL;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;
	struct page *ourpl[17];
	int index = 0;
	int have_statelock = 0;
	int downgrade;

	/*
	 * Grab the cnode statelock so the cnode state won't change
	 * while we're in here.
	 */
	ourpl[0] = NULL;
	off = off & (offset_t)PAGEMASK;
again:
	if (page_exists(vp, off) == 0) {
		if (! have_statelock) {
			mutex_enter(&cp->c_statelock);
			have_statelock = 1;
		}

		if (cp->c_backvp == NULL) {
			error = cachefs_getbackvp(fscp, cp);
			if (error)
				goto out;
		}
		error = VOP_GETPAGE(cp->c_backvp, (offset_t)off,
			PAGESIZE, protp, ourpl, PAGESIZE, seg,
			addr, S_READ, cr);
		if (error)
			goto out;

		if (have_statelock) {
			mutex_exit(&cp->c_statelock);
			have_statelock = 0;
		}
		downgrade = 0;
		for (ppp = ourpl; *ppp; ppp++) {
			if ((*ppp)->p_offset < off) {
				index++;
				page_unlock(*ppp);
				continue;
			}
			if (PAGE_SHARED(*ppp)) {
				if (page_tryupgrade(*ppp) == 0) {
					for (ppp = &ourpl[index]; *ppp; ppp++)
						page_unlock(*ppp);
					error = EAGAIN;
					goto out;
				}
				downgrade = 1;
			}
			ASSERT(PAGE_EXCL(*ppp));
			(void) hat_pageunload((*ppp), HAT_FORCE_PGUNLOAD);
			page_rename(*ppp, vp, (*ppp)->p_offset);
		}
		pl[0] = ourpl[index];
		pl[1] = NULL;
		if (downgrade) {
			page_downgrade(ourpl[index]);
		}
		/* Unlock the rest of the pages from the cluster */
		for (ppp = &ourpl[index+1]; *ppp; ppp++)
			page_unlock(*ppp);
	} else {
		ASSERT(! have_statelock);
		if (have_statelock) {
			mutex_exit(&cp->c_statelock);
			have_statelock = 0;
		}
		if ((pp = page_lookup(vp, off, SE_SHARED)) == NULL) {
			cachefs_lostpage++;
			goto again;
		}
		pl[0] = pp;
		pl[1] = NULL;
	}

out:
	if (have_statelock) {
		mutex_exit(&cp->c_statelock);
		have_statelock = 0;
	}
	return (error);
}

static int
cachefs_putpage(vnode_t *vp, offset_t off, size_t len, int flags, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int held = 0;
	int connected = 0;

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 1);
		if (error)
			break;
		held = 1;

		error = cachefs_putpage_common(vp, off, len, flags, cr);
		if (error == 0)
			break;

		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			if (CFS_TIMEOUT(fscp, error)) {
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			}
		} else {
			if (NOMEMWAIT()) {
				error = 0;
				goto out;
			}
			if (CFS_TIMEOUT(fscp, error)) {
				connected = 1;
				continue;
			}
		}
		break;
	}

out:

	if (held) {
		cachefs_cd_release(fscp);
	}

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
	return (error);
}

/*
 * Flags are composed of {B_INVAL, B_FREE, B_DONTNEED, B_FORCE}
 * If len == 0, do from off to EOF.
 *
 * The normal cases should be len == 0 & off == 0 (entire vp list),
 * len == MAXBSIZE (from segmap_release actions), and len == PAGESIZE
 * (from pageout).
 */

/*ARGSUSED*/
int
cachefs_putpage_common(struct vnode *vp, offset_t off, size_t len,
    int flags, cred_t *cr)
{
	struct cnode *cp  = VTOC(vp);
	struct page *pp;
	size_t io_len;
	u_offset_t eoff, io_off;
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	cachefscache_t *cachep = fscp->fs_cache;

	if (len == 0 && (flags & B_INVAL) == 0 &&
	    (vp->v_vfsp->vfs_flag & VFS_RDONLY)) {
		return (0);
	}
	if (vp->v_pages == NULL || (off >= cp->c_size &&
	    (flags & B_INVAL) == 0))
		return (0);

	/*
	 * If this is an async putpage let a thread handle it.
	 */
	if (flags & B_ASYNC) {
		struct cachefs_req *rp;
		int tflags = (flags & ~(B_ASYNC|B_DONTNEED));

		if (ttoproc(curthread) == proc_pageout) {
			/*
			 * If this is the page daemon we
			 * do the push synchronously (Dangerous!) and hope
			 * we can free enough to keep running...
			 */
			flags &= ~B_ASYNC;
			goto again;
		}

		if (! cachefs_async_okay()) {

			/*
			 * this is somewhat like NFS's behavior.  keep
			 * the system from thrashing.  we've seen
			 * cases where async queues get out of
			 * control, especially if
			 * madvise(MADV_SEQUENTIAL) is done on a large
			 * mmap()ed file that is read sequentially.
			 */

			flags &= ~B_ASYNC;
			goto again;
		}

		/*
		 * if no flags other than B_ASYNC were set,
		 * we coalesce putpage requests into a single one for the
		 * whole file (len = off = 0).  If such a request is
		 * already queued, we're done.
		 *
		 * If there are other flags set (e.g., B_INVAL), we don't
		 * attempt to coalesce and we use the specified length and
		 * offset.
		 */
		rp = kmem_cache_alloc(cachefs_req_cache, KM_SLEEP);
		mutex_enter(&cp->c_iomutex);
		if ((cp->c_ioflags & CIO_PUTPAGES) == 0 || tflags != 0) {
			rp->cfs_cmd = CFS_PUTPAGE;
			rp->cfs_req_u.cu_putpage.cp_vp = vp;
			if (tflags == 0) {
				off = len = 0;
				cp->c_ioflags |= CIO_PUTPAGES;
			}
			rp->cfs_req_u.cu_putpage.cp_off = off;
			rp->cfs_req_u.cu_putpage.cp_len = (uint_t)len;
			rp->cfs_req_u.cu_putpage.cp_flags = flags & ~B_ASYNC;
			rp->cfs_cr = cr;
			crhold(rp->cfs_cr);
			VN_HOLD(vp);
			cp->c_nio++;
			error = cachefs_addqueue(rp,
				&(C_TO_FSCACHE(cp)->fs_workq));
			if (error) {
				error = cachefs_addqueue(rp, &cachep->c_workq);
				ASSERT(error == 0);
			}
		} else {
		    kmem_cache_free(cachefs_req_cache, rp);
		}

		mutex_exit(&cp->c_iomutex);
		return (0);
	}


again:
	if (len == 0) {
		/*
		* Search the entire vp list for pages >= off
		*/
		error = pvn_vplist_dirty(vp, off, cachefs_push, flags, cr);
	} else {
		/*
		* Do a range from [off...off + len] looking for pages
		* to deal with.
		*/
		eoff = (u_offset_t)off + len;
		for (io_off = off; io_off < eoff && io_off < cp->c_size;
			io_off += io_len) {
			/*
			 * If we are not invalidating, synchronously
			 * freeing or writing pages use the routine
			 * page_lookup_nowait() to prevent reclaiming
			 * them from the free list.
			 */
			if ((flags & B_INVAL) || ((flags & B_ASYNC) == 0)) {
				pp = page_lookup(vp, io_off,
					(flags & (B_INVAL | B_FREE)) ?
					    SE_EXCL : SE_SHARED);
			} else {
				/* XXX this looks like dead code */
				pp = page_lookup_nowait(vp, io_off,
					(flags & B_FREE) ? SE_EXCL : SE_SHARED);
			}

			if (pp == NULL || pvn_getdirty(pp, flags) == 0)
				io_len = PAGESIZE;
			else {
				error = cachefs_push(vp, pp, &io_off,
					&io_len, flags, cr);
				if (error != 0)
					break;
				/*
				* "io_off" and "io_len" are returned as
				* the range of pages we actually wrote.
				* This allows us to skip ahead more quickly
				* since several pages may've been dealt
				* with by this iteration of the loop.
				*/
			}
		}
	}

	if (error == 0 && off == 0 && (len == 0 || len >= cp->c_size)) {
		cp->c_flags &= ~CDIRTY;
	}

	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_PUTPAGE))
		cachefs_log_putpage(cachep, error, fscp->fs_cfsvfsp,
		    &cp->c_metadata.md_cookie, cp->c_id.cid_fileno,
		    cr->cr_uid, off, len);

	return (error);

}

/*ARGSUSED*/
static int
cachefs_map(struct vnode *vp, offset_t off, struct as *as, caddr_t *addrp,
    size_t len, uchar_t prot, uchar_t maxprot, uint_t flags, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	struct segvn_crargs vn_a;
	int error;
	int held = 0;
	int writing;
	int connected = 0;

#ifdef CFSDEBUG
	u_offset_t offx = (u_offset_t)off;

	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_map: ENTER vp %p off %lld len %lu flags %d\n",
			(void *)vp, offx, len, flags);
#endif
	if (vp->v_flag & VNOMAP) {
		error = ENOSYS;
		goto out;
	}
	if (off < 0 || (offset_t)(off + len) < 0) {
		error = EINVAL;
		goto out;
	}
	if (vp->v_type != VREG) {
		error = ENODEV;
		goto out;
	}

	/*
	 * Check to see if the vnode is currently marked as not cachable.
	 * If so, we have to refuse the map request as this violates the
	 * don't cache attribute.
	 */
	if (vp->v_flag & VNOCACHE)
		return (EAGAIN);

#ifdef OBSOLETE
	/*
	 * If file is being locked, disallow mapping.
	 */
	if (vp->v_filocks != NULL) {
		error = EAGAIN;
		goto out;
	}
#endif

	writing = (prot & PROT_WRITE && ((flags & MAP_PRIVATE) == 0));

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, writing);
		if (error)
			break;
		held = 1;

		if (writing) {
			mutex_enter(&cp->c_statelock);
			if (CFS_ISFS_WRITE_AROUND(fscp)) {
				if (fscp->fs_cdconnected != CFS_CD_CONNECTED) {
					connected = 1;
					continue;
				} else {
					cachefs_nocache(cp);
				}
			}

			/*
			 * CN_MAPWRITE is for an optimization in cachefs_delmap.
			 * If CN_MAPWRITE is not set then cachefs_delmap does
			 * not need to try to push out any pages.
			 * This bit gets cleared when the cnode goes inactive.
			 */
			cp->c_flags |= CN_MAPWRITE;

			mutex_exit(&cp->c_statelock);
		}
		break;
	}

	if (held) {
		cachefs_cd_release(fscp);
	}

	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {
		map_addr(addrp, len, off, 1, flags);
		if (*addrp == NULL) {
			as_rangeunlock(as);
			error = ENOMEM;
			goto out;
		}
	} else {
		/*
		* User specified address - blow away any previous mappings
		*/
		(void) as_unmap(as, *addrp, len);
	}

	/*
	 * package up all the data passed in into a segvn_args struct and
	 * call as_map with segvn_create function to create a new segment
	 * in the address space.
	 */
	vn_a.vp = vp;
	vn_a.offset = off;
	vn_a.type = flags & MAP_TYPE;
	vn_a.prot = (uchar_t)prot;
	vn_a.maxprot = (uchar_t)maxprot;
	vn_a.cred = cr;
	vn_a.amp = NULL;
	vn_a.flags = flags & ~MAP_TYPE;
	error = as_map(as, *addrp, len, segvn_create, &vn_a);
	as_rangeunlock(as);
out:

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_map: EXIT vp %p error %d\n", (void *)vp, error);
#endif
	return (error);
}

/*ARGSUSED*/
static int
cachefs_addmap(struct vnode *vp, offset_t off, struct as *as,
    caddr_t addr, size_t len, uchar_t prot, uchar_t maxprot, uint_t flags,
    cred_t *cr)
{
	cnode_t *cp = VTOC(vp);

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	mutex_enter(&cp->c_statelock);
	cp->c_mapcnt += btopr(len);
	mutex_exit(&cp->c_statelock);
	return (0);
}

/*ARGSUSED*/
static int
cachefs_delmap(struct vnode *vp, offset_t off, struct as *as,
	caddr_t addr, size_t len, uint_t prot, uint_t maxprot, uint_t flags,
	cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error;
	int connected = 0;
	int held = 0;

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	mutex_enter(&cp->c_statelock);
	cp->c_mapcnt -= btopr(len);
	ASSERT(cp->c_mapcnt >= 0);
	mutex_exit(&cp->c_statelock);

	if (cp->c_mapcnt || (vp->v_pages == NULL) ||
	    ((cp->c_flags & CN_MAPWRITE) == 0))
		return (0);

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 1);
		if (error)
			break;
		held = 1;
		connected = 0;

		error = cachefs_putpage_common(vp, (offset_t)0,
		    (uint_t)0, 0, cr);
		if (CFS_TIMEOUT(fscp, error)) {
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				continue;
			} else {
				connected = 1;
				continue;
			}
		}

		/* if no space left in cache, wait until connected */
		if ((error == ENOSPC) &&
		    (fscp->fs_cdconnected != CFS_CD_CONNECTED)) {
			connected = 1;
			continue;
		}

		mutex_enter(&cp->c_statelock);
		if (!error)
			error = cp->c_error;
		cp->c_error = 0;
		mutex_exit(&cp->c_statelock);
		break;
	}

	if (held)
		cachefs_cd_release(fscp);

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
	return (error);
}

static int
cachefs_cmp(struct vnode *vp1, struct vnode *vp2)
{
	return (vp1 == vp2);
}

/* ARGSUSED */
static int
cachefs_frlock(struct vnode *vp, int cmd, struct flock64 *bfp, int flag,
	offset_t offset, cred_t *cr)
{
	struct cnode *cp = VTOC(vp);
	int error;
	struct fscache *fscp = C_TO_FSCACHE(cp);
	vnode_t *backvp;
	int held = 0;
	int connected = 0;

	if ((cmd != F_GETLK) && (cmd != F_SETLK) && (cmd != F_SETLKW))
		return (EINVAL);

	/* Disallow locking of files that are currently mapped */
	if (((cmd == F_SETLK) || (cmd == F_SETLKW)) && (cp->c_mapcnt > 0))
		return (EAGAIN);

	/* XXX bob: nfs does a bunch more checks than we do */
	if (CFS_ISFS_LLOCK(fscp)) {
		return (fs_frlock(vp, cmd, bfp, flag, offset, cr));
	}

	for (;;) {
		/* get (or renew) access to the file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			break;
		held = 1;

		/* if not connected, quit or wait */
		if (fscp->fs_cdconnected != CFS_CD_CONNECTED) {
			connected = 1;
			continue;
		}

		/* nocache the file */
		if ((cp->c_flags & CN_NOCACHE) == 0) {
			mutex_enter(&cp->c_statelock);
			cachefs_nocache(cp);
			mutex_exit(&cp->c_statelock);
		}

		/* XXX bob: probably should do a consistency check */
		if (bfp->l_whence == 2) {
			bfp->l_start += cp->c_size;
			bfp->l_whence = 0;
		}

		/* get the back vp */
		mutex_enter(&cp->c_statelock);
		if (cp->c_backvp == NULL) {
			error = cachefs_getbackvp(fscp, cp);
			if (error) {
				mutex_exit(&cp->c_statelock);
				break;
			}
		}
		backvp = cp->c_backvp;
		VN_HOLD(backvp);
		mutex_exit(&cp->c_statelock);

		/*
		 * make sure we can flush currently dirty pages before
		 * allowing the lock
		 */
		if (bfp->l_type != F_UNLCK && cmd != F_GETLK) {
			error = cachefs_putpage(
			    vp, (offset_t)0, 0, B_INVAL, cr);
			if (error) {
				error = ENOLCK;
				break;
			}
		}

		/* do lock on the back file */
		error = VOP_FRLOCK(backvp, cmd, bfp, flag, offset, cr);
		VN_RELE(backvp);
		if (CFS_TIMEOUT(fscp, error)) {
			connected = 1;
			continue;
		}
		break;
	}

	if (held) {
		cachefs_cd_release(fscp);
	}

	/*
	 * If we are setting a lock mark the vnode VNOCACHE so the page
	 * cache does not give inconsistent results on locked files shared
	 * between clients.  The VNOCACHE flag is never turned off as long
	 * as the vnode is active because it is hard to figure out when the
	 * last lock is gone.
	 * XXX - what if some already has the vnode mapped in?
	 * XXX bob: see nfs3_frlock, do not allow locking if vnode mapped in.
	 */
	if ((error == 0) && (bfp->l_type != F_UNLCK) && (cmd != F_GETLK))
		vp->v_flag |= VNOCACHE;

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
	return (error);
}

/*
 * Free storage space associated with the specified vnode.  The portion
 * to be freed is specified by bfp->l_start and bfp->l_len (already
 * normalized to a "whence" of 0).
 *
 * This is an experimental facility whose continued existence is not
 * guaranteed.  Currently, we only support the special case
 * of l_len == 0, meaning free to end of file.
 */
/* ARGSUSED */
static int
cachefs_space(struct vnode *vp, int cmd, struct flock64 *bfp, int flag,
	offset_t offset, cred_t *cr)
{
	int error;

	ASSERT(vp->v_type == VREG);
	if (cmd != F_FREESP)
		return (EINVAL);

	if ((error = convoff(vp, bfp, 0, offset)) == 0) {
		ASSERT(bfp->l_start >= 0);
		if (bfp->l_len == 0) {
			struct vattr va;

			va.va_size = bfp->l_start;
			va.va_mask = AT_SIZE;
			error = cachefs_setattr(vp, &va, 0, cr);
		} else
			error = EINVAL;
	}

	return (error);
}

/*ARGSUSED*/
static int
cachefs_realvp(struct vnode *vp, struct vnode **vpp)
{
	return (EINVAL);
}

/*ARGSUSED*/
static int
cachefs_pageio(struct vnode *vp, page_t *pp, u_offset_t io_off, size_t io_len,
	int flags, cred_t *cr)
{
	return (ENOSYS);
}

static int
cachefs_convert_mount(struct fscache *fscp, struct cachefs_cnvt_mnt *ccmp)
{
	int error;
	struct vnode *vp;
	struct vfs *vfsp;
	int mflag = 0;
	extern struct vfs *cachefs_frontrootvfsp;

	/* First, check that the specified file system is not mounted */
	if (ccmp->cm_op == CFS_CM_FRONT) {
		/*
		 * XXX: c_dirvp could be NULL if we booted off the
		 * disk that did not have a cache!
		 */
		ASSERT(cachefs_frontrootvfsp != NULL);
		vfsp = cachefs_frontrootvfsp;
		if (fscp->fs_cache->c_dirvp)
			ASSERT(cachefs_frontrootvfsp ==
			    fscp->fs_cache->c_dirvp->v_vfsp);
	} else {
		vfsp = fscp->fs_backvfsp;
	}

	if (vfsp->vfs_vnodecovered != NULL)
		return (EINVAL);

	/* First, attempt to lookup the name */
	error = lookupname(ccmp->cm_name, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp);
	if (error)
		return (error);

	/*
	 * So, we now have the correct vfsp as well as the vnode that
	 * we are going to cover.
	 * We fake the mount by performing a subset of the
	 * operations that a normal mount would perform.
	 */

	if (vn_vfslock(vp)) {
		VN_RELE(vp);
		return (EBUSY);
	}
	if (vp->v_vfsmountedhere != NULL) {
		vn_vfsunlock(vp);
		VN_RELE(vp);
		return (EBUSY);
	}
	if (vp->v_flag & VNOMOUNT) {
		vn_vfsunlock(vp);
		VN_RELE(vp);
		return (EINVAL);
	}
	RLOCK_VFSSW();
	if (error = vfs_lock(vfsp)) {
		vn_vfsunlock(vp);
		VN_RELE(vp);
		RUNLOCK_VFSSW();
		return (error);
	}
	dnlc_purge_vp(vp);
	vfs_add(vp, vfsp, mflag);
	vp->v_vfsp->vfs_nsubmounts++;
	vfs_unlock(vfsp);
	vn_vfsunlock(vp);
	RUNLOCK_VFSSW();
	return (0);
}

static int
cachefs_setsecattr_connected(cnode_t *cp,
    vsecattr_t *vsec, int flag, cred_t *cr)
{
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;

	ASSERT((fscp->fs_info.fi_mntflags & CFS_NOACL) == 0);

	mutex_enter(&cp->c_statelock);

	if (cp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, cp);
		if (error) {
			cachefs_nocache(cp);
			goto out;
		}
	}

	error = CFSOP_CHECK_COBJECT(fscp, cp, 0, cr);
	if (error)
		goto out;

	/* only owner can set acl */
	if (cp->c_metadata.md_vattr.va_uid != cr->cr_uid) {
		error = EINVAL;
		goto out;
	}

	error = VOP_SETSECATTR(cp->c_backvp, vsec, flag, cr);
	if (error) {
		goto out;
	}

	if ((cp->c_filegrp->fg_flags & CFS_FG_WRITE) == 0) {
		cachefs_nocache(cp);
		goto out;
	}

	CFSOP_MODIFY_COBJECT(fscp, cp, cr);

	/* acl may have changed permissions -- handle this. */
	cachefs_acl2perm(cp, vsec);

	if ((cp->c_flags & CN_NOCACHE) == 0) {
		error = cachefs_cacheacl(cp, vsec);
		if (error != 0) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_VOPS)
				printf("cachefs_setacl: cacheacl: error %d\n",
				    error);
#endif /* CFSDEBUG */
			error = 0;
			cachefs_nocache(cp);
		}
	}

out:
	mutex_exit(&cp->c_statelock);

	return (error);
}

static int
cachefs_setsecattr_disconnected(cnode_t *cp,
    vsecattr_t *vsec, int flag, cred_t *cr)
{
	fscache_t *fscp = C_TO_FSCACHE(cp);
	mode_t failmode = cp->c_metadata.md_vattr.va_mode;
	off_t commit = 0;
	int error = 0;

	ASSERT((fscp->fs_info.fi_mntflags & CFS_NOACL) == 0);

	if (CFS_ISFS_WRITE_AROUND(fscp))
		return (ETIMEDOUT);

	mutex_enter(&cp->c_statelock);

	/* only owner can set acl */
	if (cp->c_metadata.md_vattr.va_uid != cr->cr_uid) {
		error = EINVAL;
		goto out;
	}

	if (cp->c_metadata.md_flags & MD_NEEDATTRS) {
		error = ETIMEDOUT;
		goto out;
	}

	/* XXX do i need this?  is this right? */
	if (cp->c_flags & CN_ALLOC_PENDING) {
		if (cp->c_filegrp->fg_flags & CFS_FG_ALLOC_ATTR) {
			(void) filegrp_allocattr(cp->c_filegrp);
		}
		error = filegrp_create_metadata(cp->c_filegrp,
		    &cp->c_metadata, &cp->c_id);
		if (error) {
			goto out;
		}
		cp->c_flags &= ~CN_ALLOC_PENDING;
	}

	/* XXX is this right? */
	if ((cp->c_metadata.md_flags & MD_MAPPING) == 0) {
		error = cachefs_dlog_cidmap(fscp);
		if (error) {
			error = ENOSPC;
			goto out;
		}
		cp->c_metadata.md_flags |= MD_MAPPING;
		cp->c_flags |= CN_UPDATED;
	}

	commit = cachefs_dlog_setsecattr(fscp, vsec, flag, cp, cr);
	if (commit == 0)
		goto out;

	/* fix modes in metadata */
	cachefs_acl2perm(cp, vsec);

	if ((cp->c_flags & CN_NOCACHE) == 0) {
		error = cachefs_cacheacl(cp, vsec);
		if (error != 0) {
			goto out;
		}
	}

	/* XXX is this right? */
	if (cachefs_modified_alloc(cp)) {
		error = ENOSPC;
		goto out;
	}

out:
	if (error != 0)
		cp->c_metadata.md_vattr.va_mode = failmode;

	mutex_exit(&cp->c_statelock);

	if (commit) {
		if (cachefs_dlog_commit(fscp, commit, error)) {
			/*EMPTY*/
			/* XXX fix on panic? */
		}
	}

	return (error);
}

static int
cachefs_setsecattr(vnode_t *vp, vsecattr_t *vsec, int flag, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int connected = 0;
	int held = 0;
	int error = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
	    printf("cachefs_setsecattr: ENTER vp %p\n", (void *)vp);
#endif

	if (fscp->fs_info.fi_mntflags & CFS_NOACL) {
		error = ENOSYS;
		goto out;
	}

	if (! cachefs_vtype_aclok(vp)) {
		error = EINVAL;
		goto out;
	}

	for (;;) {
		/* drop hold on file system */
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}

		/* aquire access to the file system */
		error = cachefs_cd_access(fscp, connected, 1);
		if (error)
			break;
		held = 1;

		/* perform the setattr */
		if (fscp->fs_cdconnected == CFS_CD_CONNECTED)
			error = cachefs_setsecattr_connected(cp,
			    vsec, flag, cr);
		else
			error = cachefs_setsecattr_disconnected(cp,
			    vsec, flag, cr);
		if (error) {
			/* if connected */
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
				if (CFS_TIMEOUT(fscp, error)) {
					cachefs_cd_release(fscp);
					held = 0;
					cachefs_cd_timedout(fscp);
					connected = 0;
					continue;
				}
			}

			/* else must be disconnected */
			else {
				if (CFS_TIMEOUT(fscp, error)) {
					connected = 1;
					continue;
				}
			}
		}
		break;
	}

	if (held) {
		cachefs_cd_release(fscp);
	}
	return (error);

out:
#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_setsecattr: EXIT error = %d\n", error);
#endif
	return (error);
}

/*
 * call this BEFORE calling cachefs_cacheacl(), as the latter will
 * sanitize the acl.
 */

static void
cachefs_acl2perm(cnode_t *cp, vsecattr_t *vsec)
{
	aclent_t *aclp;
	int i;

	for (i = 0; i < vsec->vsa_aclcnt; i++) {
		aclp = ((aclent_t *)vsec->vsa_aclentp) + i;
		switch (aclp->a_type) {
		case USER_OBJ:
			cp->c_metadata.md_vattr.va_mode &= (~0700);
			cp->c_metadata.md_vattr.va_mode |= (aclp->a_perm << 6);
			break;

		case GROUP_OBJ:
			cp->c_metadata.md_vattr.va_mode &= (~070);
			cp->c_metadata.md_vattr.va_mode |= (aclp->a_perm << 3);
			break;

		case OTHER_OBJ:
			cp->c_metadata.md_vattr.va_mode &= (~07);
			cp->c_metadata.md_vattr.va_mode |= (aclp->a_perm);
			break;

		case CLASS_OBJ:
			cp->c_metadata.md_aclclass = aclp->a_perm;
			break;
		}
	}

	cp->c_flags |= CN_UPDATED;
}

static int
cachefs_getsecattr(vnode_t *vp, vsecattr_t *vsec, int flag, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int held = 0, connected = 0;
	int error = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_getsecattr: ENTER vp %p\n", (void *)vp);
#endif

	if (fscp->fs_info.fi_mntflags & CFS_NOACL) {
		error = fs_fab_acl(vp, vsec, flag, cr);
		goto out;
	}

	for (;;) {
		if (held) {
			cachefs_cd_release(fscp);
			held = 0;
		}
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			break;
		held = 1;

		if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
			error = cachefs_getsecattr_connected(vp, vsec, flag,
			    cr);
			if (CFS_TIMEOUT(fscp, error)) {
				cachefs_cd_release(fscp);
				held = 0;
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			}
		} else {
			error = cachefs_getsecattr_disconnected(vp, vsec, flag,
			    cr);
			if (CFS_TIMEOUT(fscp, error)) {
				if (cachefs_cd_access_miss(fscp)) {
					error = cachefs_getsecattr_connected(vp,
					    vsec, flag, cr);
					if (!CFS_TIMEOUT(fscp, error))
						break;
					delay(5*hz);
					connected = 0;
					continue;
				}
				connected = 1;
				continue;
			}
		}
		break;
	}

out:
	if (held)
		cachefs_cd_release(fscp);

#ifdef CFS_CD_DEBUG
	ASSERT((curthread->t_flag & T_CD_HELD) == 0);
#endif
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_getsecattr: EXIT error = %d\n", error);
#endif
	return (error);
}

static int
cachefs_shrlock(vnode_t *vp, int cmd, struct shrlock *shr, int flag)
{
	cnode_t *cp = VTOC(vp);
	int error = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_shrlock: ENTER vp %p\n", (void *)vp);
#endif

	mutex_enter(&cp->c_statelock);
	if (cp->c_backvp == NULL)
		error = cachefs_getbackvp(C_TO_FSCACHE(cp), cp);
	mutex_exit(&cp->c_statelock);
	ASSERT((error != 0) || (cp->c_backvp != NULL));

	if (error == 0)
		error = VOP_SHRLOCK(cp->c_backvp, cmd, shr, flag);

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_shrlock: EXIT error = %d\n", error);
#endif
	return (error);
}

static int
cachefs_getsecattr_connected(vnode_t *vp, vsecattr_t *vsec, int flag,
    cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int hit = 0;
	int error = 0;


	mutex_enter(&cp->c_statelock);
	error = CFSOP_CHECK_COBJECT(fscp, cp, 0, cr);
	if (error)
		goto out;

	/* read from the cache if we can */
	if ((cp->c_metadata.md_flags & MD_ACL) &&
	    ((cp->c_flags & CN_NOCACHE) == 0)) {
		ASSERT((cp->c_flags & CN_NOCACHE) == 0);
		error = cachefs_getaclfromcache(cp, vsec);
		if (error) {
			cachefs_nocache(cp);
			ASSERT((cp->c_metadata.md_flags & MD_ACL) == 0);
			error = 0;
		} else {
			hit = 1;
			goto out;
		}
	}

	ASSERT(error == 0);
	if (cp->c_backvp == NULL)
		error = cachefs_getbackvp(fscp, cp);
	if (error)
		goto out;
	error = VOP_GETSECATTR(cp->c_backvp, vsec, flag, cr);
	if (error)
		goto out;

	if (((fscp->fs_info.fi_mntflags & CFS_NOACL) == 0) &&
	    (cachefs_vtype_aclok(vp)) &&
	    ((cp->c_flags & CN_NOCACHE) == 0)) {
		error = cachefs_cacheacl(cp, vsec);
		if (error) {
			error = 0;
			cachefs_nocache(cp);
		}
	}

out:
	if (error == 0) {
		if (hit)
			fscp->fs_stats.st_hits++;
		else
			fscp->fs_stats.st_misses++;
	}
	mutex_exit(&cp->c_statelock);

	return (error);
}

static int
/*ARGSUSED*/
cachefs_getsecattr_disconnected(vnode_t *vp, vsecattr_t *vsec, int flag,
    cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int hit = 0;
	int error = 0;


	mutex_enter(&cp->c_statelock);

	/* read from the cache if we can */
	if (((cp->c_flags & CN_NOCACHE) == 0) &&
	    (cp->c_metadata.md_flags & MD_ACL)) {
		error = cachefs_getaclfromcache(cp, vsec);
		if (error) {
			cachefs_nocache(cp);
			ASSERT((cp->c_metadata.md_flags & MD_ACL) == 0);
			error = 0;
		} else {
			hit = 1;
			goto out;
		}
	}
	error = ETIMEDOUT;

out:
	if (error == 0) {
		if (hit)
			fscp->fs_stats.st_hits++;
		else
			fscp->fs_stats.st_misses++;
	}
	mutex_exit(&cp->c_statelock);

	return (error);
}

/*
 * cachefs_cacheacl() -- cache an ACL, which we do by applying it to
 * the frontfile if possible; otherwise, the adjunct directory.
 *
 * inputs:
 * cp - the cnode, with its statelock already held
 * vsecp - a pointer to a vsecattr_t you'd like us to cache as-is,
 *  or NULL if you want us to do the VOP_GETSECATTR(backvp).
 *
 * returns:
 * 0 - all is well
 * nonzero - errno
 */

int
cachefs_cacheacl(cnode_t *cp, vsecattr_t *vsecp)
{
	fscache_t *fscp = C_TO_FSCACHE(cp);
	vsecattr_t vsec;
	aclent_t *aclp;
	int gotvsec = 0;
	int error = 0;
	vnode_t *vp = NULL;
	void *aclkeep = NULL;
	int i;

	ASSERT(MUTEX_HELD(&cp->c_statelock));
	ASSERT((cp->c_flags & CN_NOCACHE) == 0);
	ASSERT((fscp->fs_info.fi_mntflags & CFS_NOACL) == 0);
	ASSERT(cachefs_vtype_aclok(CTOV(cp)));

	if (fscp->fs_info.fi_mntflags & CFS_NOACL) {
		error = ENOSYS;
		goto out;
	}

	if (vsecp == NULL) {
		if (cp->c_backvp == NULL)
			error = cachefs_getbackvp(fscp, cp);
		if (error != 0)
			goto out;
		vsecp = &vsec;
		bzero(&vsec, sizeof (vsec));
		vsecp->vsa_mask =
		    VSA_ACL | VSA_ACLCNT | VSA_DFACL | VSA_DFACLCNT;
		error = VOP_GETSECATTR(cp->c_backvp, vsecp, 0, kcred);
		if (error != 0) {
			goto out;
		}
		gotvsec = 1;
	} else if (vsecp->vsa_mask & VSA_ACL) {
		aclkeep = vsecp->vsa_aclentp;
		vsecp->vsa_aclentp = cachefs_kmem_alloc(vsecp->vsa_aclcnt *
		    sizeof (aclent_t), KM_SLEEP);
		bcopy(aclkeep, vsecp->vsa_aclentp, vsecp->vsa_aclcnt *
		    sizeof (aclent_t));
	} else if ((vsecp->vsa_mask & (VSA_ACL | VSA_DFACL)) == 0) {
		/* unless there's real data, we can cache nothing. */
		return (0);
	}

	/*
	 * prevent the ACL from chmoding our frontfile, and
	 * snarf the class info
	 */

	if ((vsecp->vsa_mask & (VSA_ACL | VSA_ACLCNT)) ==
	    (VSA_ACL | VSA_ACLCNT)) {
		for (i = 0; i < vsecp->vsa_aclcnt; i++) {
			aclp = ((aclent_t *)vsecp->vsa_aclentp) + i;
			switch (aclp->a_type) {
			case CLASS_OBJ:
				cp->c_metadata.md_aclclass =
			    aclp->a_perm;
				/*FALLTHROUGH*/
			case USER_OBJ:
			case GROUP_OBJ:
			case OTHER_OBJ:
				aclp->a_perm = 06;
			}
		}
	}

	/*
	 * if the frontfile exists, then we always do the work.  but,
	 * if there's no frontfile, and the ACL isn't a `real' ACL,
	 * then we don't want to do the work.  otherwise, an `ls -l'
	 * will create tons of emtpy frontfiles.
	 */

	if (((cp->c_metadata.md_flags & MD_FILE) == 0) &&
	    ((vsecp->vsa_aclcnt + vsecp->vsa_dfaclcnt)
	    <= MIN_ACL_ENTRIES)) {
		cp->c_metadata.md_flags |= MD_ACL;
		cp->c_flags |= CN_UPDATED;
		goto out;
	}

	/*
	 * if we have a default ACL, then we need a
	 * real live directory in the frontfs that we
	 * can apply the ACL to.  if not, then we just
	 * use the frontfile.  we get the frontfile
	 * regardless -- that way, we know the
	 * directory for the frontfile exists.
	 */

	if (vsecp->vsa_dfaclcnt > 0) {
		if (cp->c_acldirvp == NULL)
			error = cachefs_getacldirvp(cp);
		if (error != 0)
			goto out;
		vp = cp->c_acldirvp;
	} else {
		if (cp->c_frontvp == NULL)
			error = cachefs_getfrontfile(cp);
		if (error != 0)
			goto out;
		vp = cp->c_frontvp;
	}
	ASSERT(vp != NULL);

	VOP_RWLOCK(vp, 1);
	error = VOP_SETSECATTR(vp, vsecp, 0, kcred);
	VOP_RWUNLOCK(vp, 1);
	if (error != 0) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_VOPS)
			printf("cachefs_cacheacl: setsecattr: error %d\n",
			    error);
#endif /* CFSDEBUG */
		goto out;
	}
	if (vp == cp->c_frontvp)
		cp->c_flags |= CN_NEED_FRONT_SYNC;

	cp->c_metadata.md_flags |= MD_ACL;
	cp->c_flags |= CN_UPDATED;

out:
	if ((error) && (fscp->fs_cdconnected == CFS_CD_CONNECTED))
		cachefs_nocache(cp);

	if (gotvsec) {
		if (vsec.vsa_aclcnt)
			kmem_free(vsec.vsa_aclentp,
			    vsec.vsa_aclcnt * sizeof (aclent_t));
		if (vsec.vsa_dfaclcnt)
			kmem_free(vsec.vsa_dfaclentp,
			    vsec.vsa_dfaclcnt * sizeof (aclent_t));
	} else if (aclkeep != NULL) {
		cachefs_kmem_free(vsecp->vsa_aclentp,
		    vsecp->vsa_aclcnt * sizeof (aclent_t));
		vsecp->vsa_aclentp = aclkeep;
	}

	return (error);
}

void
cachefs_purgeacl(cnode_t *cp)
{
	ASSERT(MUTEX_HELD(&cp->c_statelock));

	if (cp->c_acldirvp != NULL) {
		VN_RELE(cp->c_acldirvp);
		cp->c_acldirvp = NULL;
	}

	if (cp->c_metadata.md_flags & MD_ACLDIR) {
		char name[CFS_FRONTFILE_NAME_SIZE + 2];

		ASSERT(cp->c_filegrp->fg_dirvp != NULL);
		make_ascii_name(&cp->c_id, name);
		(void) strcat(name, ".d");

		(void) VOP_RMDIR(cp->c_filegrp->fg_dirvp, name,
		    cp->c_filegrp->fg_dirvp, kcred);
	}

	cp->c_metadata.md_flags &= ~(MD_ACL | MD_ACLDIR);
	cp->c_flags |= CN_UPDATED;
}

static int
cachefs_getacldirvp(cnode_t *cp)
{
	char name[CFS_FRONTFILE_NAME_SIZE + 2];
	int error = 0;

	ASSERT(MUTEX_HELD(&cp->c_statelock));
	ASSERT(cp->c_acldirvp == NULL);

	if (cp->c_frontvp == NULL)
		error = cachefs_getfrontfile(cp);
	if (error != 0)
		goto out;

	ASSERT(cp->c_filegrp->fg_dirvp != NULL);
	make_ascii_name(&cp->c_id, name);
	(void) strcat(name, ".d");
	error = VOP_LOOKUP(cp->c_filegrp->fg_dirvp,
	    name, &cp->c_acldirvp, NULL, 0, NULL, kcred);
	if ((error != 0) && (error != ENOENT))
		goto out;

	if (error != 0) {
		vattr_t va;

		va.va_mode = S_IFDIR | 0777;
		va.va_uid = 0;
		va.va_gid = 0;
		va.va_type = VDIR;
		va.va_mask = AT_TYPE | AT_MODE |
		    AT_UID | AT_GID;
		error =
		    VOP_MKDIR(cp->c_filegrp->fg_dirvp,
			name, &va, &cp->c_acldirvp, kcred);
		if (error != 0)
			goto out;
	}

	ASSERT(cp->c_acldirvp != NULL);
	cp->c_metadata.md_flags |= MD_ACLDIR;
	cp->c_flags |= CN_UPDATED;

out:
	if (error != 0)
		cp->c_acldirvp = NULL;
	return (error);
}

static int
cachefs_getaclfromcache(cnode_t *cp, vsecattr_t *vsec)
{
	aclent_t *aclp;
	int error = 0;
	vnode_t *vp = NULL;
	int i;

	ASSERT(cp->c_metadata.md_flags & MD_ACL);
	ASSERT(MUTEX_HELD(&cp->c_statelock));
	ASSERT(vsec->vsa_aclentp == NULL);

	if (cp->c_metadata.md_flags & MD_ACLDIR) {
		if (cp->c_acldirvp == NULL)
			error = cachefs_getacldirvp(cp);
		if (error != 0)
			goto out;
		vp = cp->c_acldirvp;
	} else if (cp->c_metadata.md_flags & MD_FILE) {
		if (cp->c_frontvp == NULL)
			error = cachefs_getfrontfile(cp);
		if (error != 0)
			goto out;
		vp = cp->c_frontvp;
	} else {

		/*
		 * if we get here, then we know that MD_ACL is on,
		 * meaning an ACL was successfully cached.  we also
		 * know that neither MD_ACLDIR nor MD_FILE are on, so
		 * this has to be an entry without a `real' ACL.
		 * thus, we forge whatever is necessary.
		 */

		if (vsec->vsa_mask & VSA_ACLCNT)
			vsec->vsa_aclcnt = MIN_ACL_ENTRIES;

		if (vsec->vsa_mask & VSA_ACL) {
			vsec->vsa_aclentp =
			    kmem_zalloc(MIN_ACL_ENTRIES *
			    sizeof (aclent_t), KM_SLEEP);
			aclp = (aclent_t *)vsec->vsa_aclentp;
			aclp->a_type = USER_OBJ;
			++aclp;
			aclp->a_type = GROUP_OBJ;
			++aclp;
			aclp->a_type = OTHER_OBJ;
			++aclp;
			aclp->a_type = CLASS_OBJ;
			ksort((caddr_t)vsec->vsa_aclentp, MIN_ACL_ENTRIES,
			    sizeof (aclent_t), cmp2acls);
		}

		ASSERT(vp == NULL);
	}

	if (vp != NULL) {
		if ((error = VOP_GETSECATTR(vp, vsec, 0, kcred)) != 0) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_VOPS)
				printf("cachefs_getaclfromcache: error %d\n",
				    error);
#endif /* CFSDEBUG */
			goto out;
		}
	}

	if (vsec->vsa_aclentp != NULL) {
		for (i = 0; i < vsec->vsa_aclcnt; i++) {
			aclp = ((aclent_t *)vsec->vsa_aclentp) + i;
			switch (aclp->a_type) {
			case USER_OBJ:
				aclp->a_id = cp->c_metadata.md_vattr.va_uid;
				aclp->a_perm =
				    cp->c_metadata.md_vattr.va_mode & 0700;
				aclp->a_perm >>= 6;
				break;

			case GROUP_OBJ:
				aclp->a_id = cp->c_metadata.md_vattr.va_gid;
				aclp->a_perm =
				    cp->c_metadata.md_vattr.va_mode & 070;
				aclp->a_perm >>= 3;
				break;

			case OTHER_OBJ:
				aclp->a_perm =
				    cp->c_metadata.md_vattr.va_mode & 07;
				break;

			case CLASS_OBJ:
				aclp->a_perm =
				    cp->c_metadata.md_aclclass;
				break;
			}
		}
	}

out:

	if (error != 0)
		cachefs_nocache(cp);

	return (error);
}

/*
 * Fills in targp with attribute information from srcp, cp
 * and if necessary the system.
 */
static void
cachefs_attr_setup(vattr_t *srcp, vattr_t *targp, cnode_t *cp, cred_t *cr)
{
	ASSERT((srcp->va_mask & (AT_TYPE | AT_MODE)) == (AT_TYPE | AT_MODE));

	/*
	 * Add code to fill in the va struct.  We use the fields from
	 * the srcp struct if they are populated, otherwise we guess
	 */

	targp->va_mask = 0;	/* initialize all fields */
	targp->va_mode = srcp->va_mode;
	targp->va_type = srcp->va_type;
	targp->va_nlink = 1;
	targp->va_nodeid = 0;

	if (srcp->va_mask & AT_UID)
		targp->va_uid = srcp->va_uid;
	else
		targp->va_uid = cr->cr_uid;

	if (srcp->va_mask & AT_GID)
		targp->va_gid = srcp->va_gid;
	else
		targp->va_gid = cr->cr_gid;

	if (srcp->va_mask & AT_FSID)
		targp->va_fsid = srcp->va_fsid;
	else
		targp->va_fsid = 0;	/* initialize all fields */

	if (srcp->va_mask & AT_ATIME)
		targp->va_atime = srcp->va_atime;
	else
		targp->va_atime.tv_sec = hrestime.tv_sec;

	if (srcp->va_mask & AT_MTIME)
		targp->va_mtime = srcp->va_mtime;
	else
		targp->va_mtime.tv_sec = hrestime.tv_sec;

	if (srcp->va_mask & AT_CTIME)
		targp->va_ctime = srcp->va_ctime;
	else
		targp->va_ctime.tv_sec = hrestime.tv_sec;


	if (srcp->va_mask & AT_SIZE)
		targp->va_size = srcp->va_size;
	else
		targp->va_size = 0;

	/*
	 * the remaing fields are set by the fs and not changable.
	 * we populate these entries useing the parent directory
	 * values.  It's a small hack, but should work.
	 */
	targp->va_blksize = cp->c_metadata.md_vattr.va_blksize;
	targp->va_rdev = cp->c_metadata.md_vattr.va_rdev;
	targp->va_nblocks = cp->c_metadata.md_vattr.va_nblocks;
	targp->va_vcode = cp->c_metadata.md_vattr.va_vcode;
}

/*
 * set the gid for a newly created file.  The algorithm is as follows:
 *
 *	1) If the gid is set in the attribute list, then use it if
 *	   the caller is super-user, belongs to the target group, or
 *	   the group is the same as the parent directory.
 *
 *	2) If the parent directory's set-gid bit is clear, then use
 *	   the process gid
 *
 *	3) Otherwise, use the gid of the parent directory.
 *
 * Note: newcp->c_attr.va_{mode,type} must already be set before calling
 * this routine.
 */
static void
cachefs_creategid(cnode_t *dcp, cnode_t *newcp, vattr_t *vap, cred_t *cr)
{
	if ((vap->va_mask & AT_GID) &&
	    (suser(cr) || (vap->va_gid == dcp->c_attr.va_gid) ||
	    groupmember(vap->va_gid, cr))) {
		newcp->c_attr.va_gid = vap->va_gid;
	} else {
		if (dcp->c_attr.va_mode & S_ISGID)
			newcp->c_attr.va_gid = dcp->c_attr.va_gid;
		else
			newcp->c_attr.va_gid = cr->cr_gid;
	}

	/*
	 * if we're creating a directory, and the parent directory has the
	 * set-GID bit set, set it on the new directory.
	 * Otherwise, if the user is neither super-user nor a member of the
	 * file's new group, clear the file's set-GID bit.
	 */
	if (dcp->c_attr.va_mode & S_ISGID && newcp->c_attr.va_type == VDIR) {
		newcp->c_attr.va_mode |= S_ISGID;
	} else {
		if ((newcp->c_attr.va_mode & S_ISGID) &&
		    !groupmember(newcp->c_attr.va_gid, cr) && !suser(cr))
			newcp->c_attr.va_mode &= ~S_ISGID;
	}
}

/*
 * create an acl for the newly created file.  should be called right
 * after cachefs_creategid.
 */

static void
cachefs_createacl(cnode_t *dcp, cnode_t *newcp)
{
	fscache_t *fscp = C_TO_FSCACHE(dcp);
	vsecattr_t vsec;
	int gotvsec = 0;
	int error = 0; /* placeholder */
	aclent_t *aclp;
	o_mode_t *classp = NULL;
	o_mode_t gunion = 0;
	int i;

	if ((fscp->fs_info.fi_mntflags & CFS_NOACL) ||
	    (! cachefs_vtype_aclok(CTOV(newcp))))
		return;

	ASSERT(dcp->c_metadata.md_flags & MD_ACL);
	ASSERT(MUTEX_HELD(&dcp->c_statelock));
	ASSERT(MUTEX_HELD(&newcp->c_statelock));

	/*
	 * XXX should probably not do VSA_ACL and VSA_ACLCNT, but that
	 * would hit code paths that isn't hit anywhere else.
	 */

	bzero(&vsec, sizeof (vsec));
	vsec.vsa_mask = VSA_ACL | VSA_ACLCNT | VSA_DFACL | VSA_DFACLCNT;
	error = cachefs_getaclfromcache(dcp, &vsec);
	if (error != 0)
		goto out;
	gotvsec = 1;

	if ((vsec.vsa_dfaclcnt > 0) && (vsec.vsa_dfaclentp != NULL)) {
		if ((vsec.vsa_aclcnt > 0) && (vsec.vsa_aclentp != NULL))
			kmem_free(vsec.vsa_aclentp,
			    vsec.vsa_aclcnt * sizeof (aclent_t));

		vsec.vsa_aclcnt = vsec.vsa_dfaclcnt;
		vsec.vsa_aclentp = vsec.vsa_dfaclentp;
		vsec.vsa_dfaclcnt = 0;
		vsec.vsa_dfaclentp = NULL;

		if (newcp->c_attr.va_type == VDIR) {
			vsec.vsa_dfaclentp = kmem_alloc(vsec.vsa_aclcnt *
			    sizeof (aclent_t), KM_SLEEP);
			vsec.vsa_dfaclcnt = vsec.vsa_aclcnt;
			bcopy(vsec.vsa_aclentp, vsec.vsa_dfaclentp,
			    vsec.vsa_aclcnt * sizeof (aclent_t));
		}

		/*
		 * this function should be called pretty much after
		 * the rest of the file creation stuff is done.  so,
		 * uid, gid, etc. should be `right'.  we'll go with
		 * that, rather than trying to determine whether to
		 * get stuff from cr or va.
		 */

		for (i = 0; i < vsec.vsa_aclcnt; i++) {
			aclp = ((aclent_t *)vsec.vsa_aclentp) + i;
			switch (aclp->a_type) {
			case DEF_USER_OBJ:
				aclp->a_type = USER_OBJ;
				aclp->a_id = newcp->c_metadata.md_vattr.va_uid;
				aclp->a_perm =
				    newcp->c_metadata.md_vattr.va_mode;
				aclp->a_perm &= 0700;
				aclp->a_perm >>= 6;
				break;

			case DEF_GROUP_OBJ:
				aclp->a_type = GROUP_OBJ;
				aclp->a_id = newcp->c_metadata.md_vattr.va_gid;
				aclp->a_perm =
				    newcp->c_metadata.md_vattr.va_mode;
				aclp->a_perm &= 070;
				aclp->a_perm >>= 3;
				gunion |= aclp->a_perm;
				break;

			case DEF_OTHER_OBJ:
				aclp->a_type = OTHER_OBJ;
				aclp->a_perm =
				    newcp->c_metadata.md_vattr.va_mode & 07;
				break;

			case DEF_CLASS_OBJ:
				aclp->a_type = CLASS_OBJ;
				classp = &(aclp->a_perm);
				break;

			case DEF_USER:
				aclp->a_type = USER;
				gunion |= aclp->a_perm;
				break;

			case DEF_GROUP:
				aclp->a_type = GROUP;
				gunion |= aclp->a_perm;
				break;
			}
		}

		/* XXX is this the POSIX thing to do? */
		if (classp != NULL)
			*classp &= gunion;

		/*
		 * we don't need to log this; rather, we clear the
		 * MD_ACL bit when we reconnect.
		 */

		error = cachefs_cacheacl(newcp, &vsec);
		if (error != 0)
			goto out;
	}

	newcp->c_metadata.md_aclclass = 07; /* XXX check posix */
	newcp->c_metadata.md_flags |= MD_ACL;
	newcp->c_flags |= CN_UPDATED;

out:

	if (gotvsec) {
		if ((vsec.vsa_aclcnt > 0) && (vsec.vsa_aclentp != NULL))
			kmem_free(vsec.vsa_aclentp,
			    vsec.vsa_aclcnt * sizeof (aclent_t));
		if ((vsec.vsa_dfaclcnt > 0) && (vsec.vsa_dfaclentp != NULL))
			kmem_free(vsec.vsa_dfaclentp,
			    vsec.vsa_dfaclcnt * sizeof (aclent_t));
	}
}

/*
 * this is translated from the UFS code for access checking.
 */

static int
cachefs_access_local(cnode_t *cp, int mode, cred_t *cr)
{
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;

	ASSERT(MUTEX_HELD(&cp->c_statelock));

	if (mode & VWRITE) {
		/*
		 * Disallow write attempts on read-only
		 * file systems, unless the file is special.
		 */
		struct vnode *vp = CTOV(cp);
		if (vp->v_vfsp->vfs_flag & VFS_RDONLY) {
			if (!CFS_ISVDEV(vp->v_type)) {
				return (EROFS);
			}
		}
	}

	if (suser(cr))
		return (0);	/* root has carte blanche */

	/*
	 * if we need to do ACLs, do it.  this works whether anyone
	 * has explicitly made an ACL or not.
	 */

	if (((fscp->fs_info.fi_mntflags & CFS_NOACL) == 0) &&
	    (cachefs_vtype_aclok(CTOV(cp))))
		return (cachefs_acl_access(cp, mode, cr));

	if (cr->cr_uid != cp->c_attr.va_uid) {
		mode >>= 3;
		if (!groupmember(cp->c_attr.va_gid, cr))
			mode >>= 3;
	}

	if ((cp->c_attr.va_mode & mode) != mode)
		error = EACCES;

	return (error);
}

/*
 * This is transcribed from ufs_acl_access().  If that changes, then
 * this should, too.
 *
 * Check the cnode's ACL's to see if this mode of access is
 * allowed; return 0 if allowed, EACCES if not.
 *
 * We follow the procedure defined in Sec. 3.3.5, ACL Access
 * Check Algorithm, of the POSIX 1003.6 Draft Standard.
 */

static int
cachefs_acl_access(struct cnode *cp, int mode, cred_t *cr)
{
	int error = 0;

	fscache_t *fscp = C_TO_FSCACHE(cp);

	int mask = ~0;
	int ismask = 0;

	int gperm = 0;
	int ngroup = 0;

	vsecattr_t vsec;
	int gotvsec = 0;
	aclent_t *aclp;

	int i;

	ASSERT(MUTEX_HELD(&cp->c_statelock));
	ASSERT((fscp->fs_info.fi_mntflags & CFS_NOACL) == 0);

	/*
	 * strictly speaking, we shouldn't set VSA_DFACL and DFACLCNT,
	 * but then i believe we'd be the only thing exercising those
	 * code paths -- probably a bad thing.
	 */

	bzero(&vsec, sizeof (vsec));
	vsec.vsa_mask = VSA_ACL | VSA_ACLCNT | VSA_DFACL | VSA_DFACLCNT;

	/* XXX KLUDGE! correct insidious 0-class problem */
	if (cp->c_metadata.md_aclclass == 0 &&
	    fscp->fs_cdconnected == CFS_CD_CONNECTED)
		cachefs_purgeacl(cp);
again:
	if (cp->c_metadata.md_flags & MD_ACL) {
		error = cachefs_getaclfromcache(cp, &vsec);
		if (error != 0) {
#ifdef CFSDEBUG
			if (error != ETIMEDOUT)
				CFS_DEBUG(CFSDEBUG_VOPS)
					printf("cachefs_acl_access():"
					    "error %d from getaclfromcache()\n",
					    error);
#endif /* CFSDEBUG */
			if ((cp->c_metadata.md_flags & MD_ACL) == 0) {
				goto again;
			} else {
				goto out;
			}
		}
	} else {
		if (cp->c_backvp == NULL) {
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED)
				error = cachefs_getbackvp(fscp, cp);
			else
				error = ETIMEDOUT;
		}
		if (error == 0)
			error = VOP_GETSECATTR(cp->c_backvp, &vsec, 0, cr);
		if (error != 0) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_VOPS)
				printf("cachefs_acl_access():"
				    "error %d from getsecattr(backvp)\n",
				    error);
#endif /* CFSDEBUG */
			goto out;
		}
		if ((cp->c_flags & CN_NOCACHE) == 0)
			(void) cachefs_cacheacl(cp, &vsec);
	}
	gotvsec = 1;

	ASSERT(error == 0);
	for (i = 0; i < vsec.vsa_aclcnt; i++) {
		aclp = ((aclent_t *)vsec.vsa_aclentp) + i;
		switch (aclp->a_type) {
		case USER_OBJ:
			/*
			 * this might look cleaner in the 2nd loop
			 * below, but we do it here as an
			 * optimization.
			 */

			if (cr->cr_uid == aclp->a_id) {
				if ((mode & (aclp->a_perm << 6)) != mode)
					error = EACCES;
				goto out;
			}
			break;

		case CLASS_OBJ:
			mask = aclp->a_perm;
			ismask = 1;
			break;
		}
	}

	ASSERT(error == 0);
	for (i = 0; i < vsec.vsa_aclcnt; i++) {
		aclp = ((aclent_t *)vsec.vsa_aclentp) + i;
		switch (aclp->a_type) {
		case USER:
			if (cr->cr_uid == aclp->a_id) {
				if ((mode & ((aclp->a_perm & mask) << 6)) !=
				    mode)
					error = EACCES;
				goto out;
			}
			break;

		case GROUP_OBJ:
			if (groupmember(aclp->a_id, cr)) {
				++ngroup;
				gperm |= aclp->a_perm;
				if (! ismask) {
					if ((mode & (aclp->a_perm << 6)) !=
					    mode)
						error = EACCES;
					goto out;
				}
			}
			break;

		case GROUP:
			if (groupmember(aclp->a_id, cr)) {
				++ngroup;
				gperm |= aclp->a_perm;
			}
			break;

		case OTHER_OBJ:
			if (ngroup == 0) {
				if ((mode & (aclp->a_perm << 6)) != mode)
					error = EACCES;
				goto out;
			}
			break;

		default:
			break;
		}
	}

	ASSERT(ngroup > 0);
	if ((mode & ((gperm & mask) << 6)) != mode)
		error = EACCES;

out:
	if (gotvsec) {
		if (vsec.vsa_aclcnt && vsec.vsa_aclentp)
			kmem_free(vsec.vsa_aclentp,
			    vsec.vsa_aclcnt * sizeof (aclent_t));
		if (vsec.vsa_dfaclcnt && vsec.vsa_dfaclentp)
			kmem_free(vsec.vsa_dfaclentp,
			    vsec.vsa_dfaclcnt * sizeof (aclent_t));
	}

	return (error);
}

/*
 * see if permissions allow for removal of the given file from
 * the given directory.
 */
static int
cachefs_stickyrmchk(struct cnode *dcp, struct cnode *cp, cred_t *cr)
{
	int error = 0;

	/*
	 * If the containing directory is sticky, the user must:
	 *  - own the directory, or
	 *  - own the file, or
	 *  - be able to write the file, or
	 *  - be the superuser
	 */
	if ((dcp->c_attr.va_mode & S_ISVTX) && !suser(cr) &&
	    (cr->cr_uid != dcp->c_attr.va_uid) &&
	    (cr->cr_uid != cp->c_attr.va_uid)) {
		error = cachefs_access_local(cp, VWRITE, cr);
	}
	return (error);
}

/*
 * Returns a new name, may even be unique.
 * Stolen from nfs code.
 */
static char cachefs_prefix[] = ".nfs";
kmutex_t cachefs_newnum_lock;

static char *
cachefs_newname(void)
{
	static uint_t newnum = 0;
	char *news;
	char *s, *p;
	uint_t id;

	mutex_enter(&cachefs_newnum_lock);
	if (newnum == 0) {
		newnum = hrestime.tv_sec & 0xfffff;
		newnum |= 0x10000;
	}
	id = newnum++;
	mutex_exit(&cachefs_newnum_lock);

	news = cachefs_kmem_alloc(MAXNAMELEN, KM_SLEEP);
	s = news;
	p = cachefs_prefix;
	while (*p != '\0')
		*s++ = *p++;
	while (id != 0) {
		*s++ = "0123456789ABCDEF"[id & 0x0f];
		id >>= 4;
	}
	*s = '\0';
	return (news);
}

/*
 * Called to rename the specified file to a temporary file so
 * operations to the file after remove work.
 * Must call this routine with the dir c_rwlock held as a writer.
 */
static int
/*ARGSUSED*/
cachefs_remove_dolink(vnode_t *dvp, vnode_t *vp, char *nm, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	char *tmpname;
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error;

	ASSERT(RW_WRITE_HELD(&(VTOC(dvp)->c_rwlock)));

	/* get the new name for the file */
	tmpname = cachefs_newname();

	/* do the link */
	if (fscp->fs_cdconnected == CFS_CD_CONNECTED)
		error = cachefs_link_connected(dvp, vp, tmpname, cr);
	else
		error = cachefs_link_disconnected(dvp, vp, tmpname, cr);
	if (error) {
		cachefs_kmem_free(tmpname, MAXNAMELEN);
		return (error);
	}

	mutex_enter(&cp->c_statelock);
	if (cp->c_unldvp) {
		VN_RELE(cp->c_unldvp);
		cachefs_kmem_free(cp->c_unlname, MAXNAMELEN);
		crfree(cp->c_unlcred);
	}

	VN_HOLD(dvp);
	cp->c_unldvp = dvp;
	crhold(cr);
	cp->c_unlcred = cr;
	cp->c_unlname = tmpname;

	/* drop the backvp so NFS does not also do a rename */
	if (cp->c_backvp) {
		VN_RELE(cp->c_backvp);
		cp->c_backvp = NULL;
	}
	mutex_exit(&cp->c_statelock);

	return (0);
}

/*
 * Marks the cnode as modified.
 */
static void
cachefs_modified(cnode_t *cp)
{
	fscache_t *fscp = C_TO_FSCACHE(cp);
	struct vattr va;
	int error;

	ASSERT(MUTEX_HELD(&cp->c_statelock));
	ASSERT(cp->c_metadata.md_rlno);

	/* if not on the modify list */
	if (cp->c_metadata.md_rltype != CACHEFS_RL_MODIFIED) {
		/* put on modified list, also marks the file as modified */
		cachefs_rlent_moveto(fscp->fs_cache, CACHEFS_RL_MODIFIED,
		    cp->c_metadata.md_rlno, cp->c_metadata.md_frontblks);
		cp->c_metadata.md_rltype = CACHEFS_RL_MODIFIED;
		cp->c_flags |= CN_UPDATED;

		/* if a modified regular file that is not local */
		if (((cp->c_id.cid_flags & CFS_CID_LOCAL) == 0) &&
		    (cp->c_metadata.md_flags & MD_FILE) &&
		    (cp->c_attr.va_type == VREG)) {

			if (cp->c_frontvp == NULL)
				(void) cachefs_getfrontfile(cp);
			if (cp->c_frontvp) {
				/* identify file so fsck knows it is modified */
				va.va_mode = 0766;
				va.va_mask = AT_MODE;
				error = VOP_SETATTR(cp->c_frontvp, &va,
				    0, kcred);
				if (error) {
					cmn_err(CE_WARN,
					    "Cannot change ff mode.\n");
				}
			}
		}
	}
}

/*
 * Marks the cnode as modified.
 * Allocates a rl slot for the cnode if necessary.
 * Returns 0 for success, !0 if cannot get an rl slot.
 */
static int
cachefs_modified_alloc(cnode_t *cp)
{
	fscache_t *fscp = C_TO_FSCACHE(cp);
	filegrp_t *fgp = cp->c_filegrp;
	int error;
	rl_entry_t rl_ent;

	ASSERT(MUTEX_HELD(&cp->c_statelock));

	/* get the rl slot if needed */
	if (cp->c_metadata.md_rlno == 0) {
		/* get a metadata slot if we do not have one yet */
		if (cp->c_flags & CN_ALLOC_PENDING) {
			if (cp->c_filegrp->fg_flags & CFS_FG_ALLOC_ATTR) {
				(void) filegrp_allocattr(cp->c_filegrp);
			}
			error = filegrp_create_metadata(cp->c_filegrp,
			    &cp->c_metadata, &cp->c_id);
			if (error)
				return (error);
			cp->c_flags &= ~CN_ALLOC_PENDING;
		}

		/* get a free rl entry */
		rl_ent.rl_fileno = cp->c_id.cid_fileno;
		rl_ent.rl_local = (cp->c_id.cid_flags & CFS_CID_LOCAL) ? 1 : 0;
		rl_ent.rl_fsid = fscp->fs_cfsid;
		rl_ent.rl_attrc = 0;
		error = cachefs_rl_alloc(fscp->fs_cache, &rl_ent,
		    &cp->c_metadata.md_rlno);
		if (error)
			return (error);
		cp->c_metadata.md_rltype = CACHEFS_RL_NONE;

		/* hold the filegrp so the attrcache file is not gc */
		error = filegrp_ffhold(fgp);
		if (error) {
			cachefs_rlent_moveto(fscp->fs_cache,
			    CACHEFS_RL_FREE, cp->c_metadata.md_rlno, 0);
			cp->c_metadata.md_rlno = 0;
			return (error);
		}
	}
	cachefs_modified(cp);
	return (0);
}

int
cachefs_vtype_aclok(vnode_t *vp)
{
	vtype_t *vtp, oktypes[] = {VREG, VDIR, VFIFO, VNON};

	if (vp->v_type == VNON)
		return (0);

	for (vtp = oktypes; *vtp != VNON; vtp++)
		if (vp->v_type == *vtp)
			break;

	return (*vtp != VNON);
}

static int
cachefs_pathconf(vnode_t *vp, int cmd, ulong_t *valp, cred_t *cr)
{
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));

	if (cmd == _PC_FILESIZEBITS) {
		u_offset_t maxsize = fscp->fs_offmax;
		(*valp) = 0;
		while (maxsize != 0) {
			maxsize >>= 1;
			(*valp)++;
		}
		(*valp)++;
	} else
		error = fs_pathconf(vp, cmd, valp, cr);

	return (error);
}
