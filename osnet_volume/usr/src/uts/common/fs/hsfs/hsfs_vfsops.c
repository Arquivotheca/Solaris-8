/*
 * Copyright (c) 1990, 1994, 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hsfs_vfsops.c	1.64	99/08/09 SMI"

/*
 * VFS operations for High Sierra filesystem
 */

#include <sys/types.h>
#include <sys/isa_defs.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/buf.h>
#include <sys/pathname.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/conf.h>

#include <vm/page.h>

#include <sys/fs/snode.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/hsfs_spec.h>
#include <sys/fs/hsfs_isospec.h>
#include <sys/fs/hsfs_node.h>
#include <sys/fs/hsfs_impl.h>
#include <sys/fs/hsfs_susp.h>
#include <sys/fs/hsfs_rrip.h>

#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/mntent.h>
#include <sys/swap.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include "fs/fs_subr.h"
#include <sys/cmn_err.h>
#include <sys/bootconf.h>

/*
 * These are needed for the CDROMREADOFFSET Code
 */
#include <sys/cdio.h>
#include <sys/sunddi.h>

#define	HSFS_CLKSET

#include <sys/modctl.h>

/*
 * Options for mount.
 */
#define	HOPT_GLOBAL	MNTOPT_GLOBAL
#define	HOPT_NOGLOBAL	MNTOPT_NOGLOBAL
#define	HOPT_MAPLCASE	"maplcase"
#define	HOPT_NOMAPLCASE	"nomaplcase"
#define	HOPT_SUID	MNTOPT_SUID
#define	HOPT_NOSUID	MNTOPT_NOSUID
#define	HOPT_NOTRAILDOT	"notraildot"
#define	HOPT_TRAILDOT	"traildot"
#define	HOPT_NRR	"nrr"
#define	HOPT_RR		"rr"
#define	HOPT_RO		MNTOPT_RO
#define	HOPT_REMOUNT	MNTOPT_REMOUNT

static char *global_cancel[] = { HOPT_NOGLOBAL, NULL };
static char *mapl_cancel[] = { HOPT_NOMAPLCASE, NULL };
static char *nomapl_cancel[] = { HOPT_MAPLCASE, NULL };
static char *noglobal_cancel[] = { HOPT_GLOBAL, NULL };
static char *nosuid_cancel[] = { HOPT_SUID, NULL };
static char *notrail_cancel[] = { HOPT_TRAILDOT, NULL };
static char *nrr_cancel[] = { HOPT_RR, NULL };
static char *rr_cancel[] = { HOPT_NRR, NULL };
static char *suid_cancel[] = { HOPT_NOSUID, NULL };
static char *trail_cancel[] = { HOPT_NOTRAILDOT, NULL };

static mntopt_t hsfs_options[] = {
	{ HOPT_REMOUNT, NULL, NULL, 0, NULL },
	{ HOPT_GLOBAL, global_cancel, NULL, 0, NULL },
	{ HOPT_MAPLCASE, mapl_cancel, NULL, MO_DEFAULT, NULL },
	{ HOPT_NOGLOBAL, noglobal_cancel, NULL, MO_DEFAULT, NULL },
	{ HOPT_NOMAPLCASE, nomapl_cancel, NULL, 0, NULL },
	{ HOPT_NOSUID, nosuid_cancel, NULL, 0, NULL },
	{ HOPT_NOTRAILDOT, notrail_cancel, NULL, 0, NULL },
	{ HOPT_NRR, nrr_cancel, NULL, 0, NULL },
	{ HOPT_RO, NULL, NULL, MO_DEFAULT, NULL },
	{ HOPT_RR, rr_cancel, NULL, MO_DEFAULT, NULL },
	{ HOPT_SUID, suid_cancel, NULL, 0, NULL },
	{ HOPT_TRAILDOT, trail_cancel, NULL, MO_DEFAULT, NULL },
};
static mntopts_t hsfs_proto_opttbl = {
	sizeof (hsfs_options) / sizeof (mntopt_t),
	hsfs_options
};
static struct vfsops hsfs_vfsops;
static int hsfsinit(struct vfssw *, int);

static struct vfssw vfw = {
	"hsfs",
	hsfsinit,
	&hsfs_vfsops,
	VSW_HASPROTO,
	&hsfs_proto_opttbl
};

static struct modlfs modlfs = {
	&mod_fsops, "filesystem for HSFS", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};

char _depends_on[] = "fs/specfs";

int
_init()
{
	return (mod_install(&modlinkage));
}

int
_fini()
{
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

#define	BDEVFLAG(dev)	((devopsp[getmajor(dev)])->devo_cb_ops->cb_flag)

kmutex_t hs_mounttab_lock;
static struct hsfs *hs_mounttab = NULL;

/* default mode, uid, gid */
mode_t hsfs_default_mode = 0555;
uid_t hsfs_default_uid = 0;
gid_t hsfs_default_gid = 3;

static int hsfs_mount(struct vfs *vfsp, struct vnode *mvp,
	struct mounta *uap, struct cred *cr);
static int hsfs_unmount(struct vfs *vfsp, int, struct cred *cr);
static int hsfs_root(struct vfs *vfsp, struct vnode **vpp);
static int hsfs_statvfs(struct vfs *vfsp, struct statvfs64 *sbp);
static int hsfs_vget(struct vfs *vfsp, struct vnode **vpp, struct fid *fidp);
static int hsfs_mountroot(struct vfs *, enum whymountroot);

static int hs_mountfs(struct vfs *vfsp, dev_t dev, char *path,
	mode_t mode, int use_rrip, struct cred *cr, int);
static int hs_findhsvol(struct hsfs *fsp, struct vnode *vp,
	struct hs_volume *hvp);
static int hs_parsehsvol(struct hsfs *fsp, uchar_t *volp,
	struct hs_volume *hvp);
static int hs_findisovol(struct hsfs *fsp, struct vnode *vp,
	struct hs_volume *hvp);
static int hs_parseisovol(struct hsfs *fsp, uchar_t *volp,
	struct hs_volume *hvp);
static void hs_copylabel(struct hs_volume *, unsigned char *);
static int hs_getmdev(char *fspec, int flags, dev_t *pdev, mode_t *mode);
static int hs_findvoldesc(dev_t rdev, int desc_sec);

static struct vfsops hsfs_vfsops = {
	hsfs_mount,
	hsfs_unmount,
	hsfs_root,
	hsfs_statvfs,
	fs_sync,	/* sync */
	hsfs_vget,
	hsfs_mountroot,	/* mountroot */
	fs_nosys,	/* swapvp */
	fs_freevfs
};

int hsfsfstype;

static int
hsfsinit(struct vfssw *vswp, int fstype)
{
	vswp->vsw_vfsops = &hsfs_vfsops;
	hsfsfstype = fstype;
	mutex_init(&hs_mounttab_lock, NULL, MUTEX_DEFAULT, NULL);
	return (0);
}

/*ARGSUSED*/
static int
hsfs_mount(struct vfs *vfsp, struct vnode *mvp,
    struct mounta *uap, struct cred *cr)
{
	int		vnode_busy;
	dev_t		dev;
	struct pathname dpn;
	register int	error;
	mode_t		mode;
	int		flags;	/* this will hold the mount specific data */

	if (!suser(cr))
		return (EPERM);

	if (mvp->v_type != VDIR)
		return (ENOTDIR);

	/* mount option must be read only, else mount will be rejected */
	if (!(uap->flags & MS_RDONLY))
		return (EROFS);

	/*
	 * We don't support remounting.
	 */
	if ((uap->flags & MS_REMOUNT) ||
	    vfs_optionisset(&vfsp->vfs_mntopts, HOPT_REMOUNT, NULL)) {
		return (ENOTSUP);
	}

	mutex_enter(&mvp->v_lock);
	vnode_busy = (mvp->v_count != 1) || (mvp->v_flag & VROOT);
	mutex_exit(&mvp->v_lock);

	if ((uap->flags & MS_OVERLAY) == 0 && vnode_busy) {
		return (EBUSY);
	}

	/*
	 * Check for the options that actually affect things
	 * at our level.
	 */
	flags = 0;
	if (vfs_optionisset(&vfsp->vfs_mntopts, HOPT_NOMAPLCASE, NULL))
	    flags |= HSFSMNT_NOMAPLCASE;
	if (vfs_optionisset(&vfsp->vfs_mntopts, HOPT_NOTRAILDOT, NULL))
	    flags |= HSFSMNT_NOTRAILDOT;
	if (vfs_optionisset(&vfsp->vfs_mntopts, HOPT_NRR, NULL))
	    flags |= HSFSMNT_NORRIP;

	error = pn_get(uap->dir, (uap->flags & MS_SYSSPACE) ?
	    UIO_SYSSPACE : UIO_USERSPACE, &dpn);
	if (error)
		return (error);

	if ((error = hs_getmdev(uap->spec, uap->flags, &dev, &mode)) != 0) {
		pn_free(&dpn);
		return (error);
	}

	/*
	 * If the device is a tape, return error
	 */
	if ((BDEVFLAG(dev) & D_TAPE) == D_TAPE)  {
		pn_free(&dpn);
		return (ENOTBLK);
	}

	/*
	 * Mount the filesystem.
	 */
	error = hs_mountfs(vfsp, dev, dpn.pn_path, mode, flags, cr, 0);
	pn_free(&dpn);
	return (error);
}

/*ARGSUSED*/
static int
hsfs_unmount(
	struct vfs *vfsp,
	int flag,
	struct cred *cr)
{
	struct hsfs **tspp;
	struct hsfs *fsp;
	struct hsnode *rhp;

	if (!suser(cr))
		return (EPERM);

	/*
	 * forced unmount is not supported by this file system
	 * and thus, ENOTSUP is being returned.
	 */
	if (flag & MS_FORCE)
		return (ENOTSUP);

	fsp = VFS_TO_HSFS(vfsp);

	if (fsp->hsfs_rootvp->v_count != 1)
		return (EBUSY);

	/* destroy all old pages for this vfs */
	if (hs_synchash(vfsp))
		return (EBUSY);

	rhp = VTOH(fsp->hsfs_rootvp);
	/* release the root hsnode */
	VN_RELE(fsp->hsfs_rootvp);
	/* destroy the root hsnode lock */
	mutex_destroy(&rhp->hs_contents_lock);

	mutex_enter(&hs_mounttab_lock);
	for (tspp = &hs_mounttab; *tspp != NULL; tspp = &(*tspp)->hsfs_next) {
		if (*tspp == fsp)
			break;
	}
	if (*tspp == NULL) {
		mutex_exit(&hs_mounttab_lock);
		cmn_err(CE_PANIC, "hsfs_unmount: vfs not mounted?");
	}

	*tspp = fsp->hsfs_next;

	mutex_exit(&hs_mounttab_lock);

	(void) VOP_CLOSE(fsp->hsfs_devvp, FREAD, 1, (offset_t)0, cr);
	VN_RELE(fsp->hsfs_devvp);
	/* free path table space */
	if (fsp->hsfs_ptbl != NULL)
		kmem_free(fsp->hsfs_ptbl,
			(size_t)fsp->hsfs_vol.ptbl_len);
	/* free path table index table */
	if (fsp->hsfs_ptbl_idx != NULL)
		kmem_free(fsp->hsfs_ptbl_idx, (size_t)
			(fsp->hsfs_ptbl_idx_size * sizeof (struct ptable_idx)));

	/* free "mounted on" pathame */
	if (fsp->hsfs_fsmnt != NULL)
		kmem_free(fsp->hsfs_fsmnt, strlen(fsp->hsfs_fsmnt) + 1);

	/* free the incore path table */
	hs_freehstbl(vfsp);

	kmem_free(fsp, sizeof (*fsp));
	return (0);
}

/*ARGSUSED*/
static int
hsfs_root(struct vfs *vfsp, struct vnode **vpp)
{
	*vpp = (VFS_TO_HSFS(vfsp))->hsfs_rootvp;
	VN_HOLD(*vpp);
	return (0);
}

/*ARGSUSED*/
static int
hsfs_statvfs(struct vfs *vfsp, struct statvfs64 *sbp)
{
	struct hsfs *fsp;
	dev32_t d32;

	fsp = VFS_TO_HSFS(vfsp);
	if (fsp->hsfs_magic != HSFS_MAGIC)
		return (EINVAL);
	bzero(sbp, sizeof (*sbp));
	sbp->f_bsize = vfsp->vfs_bsize;
	sbp->f_frsize = sbp->f_bsize; /* no fragment, same as block size */
	sbp->f_blocks = (fsblkcnt64_t)fsp->hsfs_vol.vol_size;

	sbp->f_bfree = (fsblkcnt64_t)0;
	sbp->f_bavail = (fsblkcnt64_t)0;
	sbp->f_files = (fsfilcnt64_t)-1;
	sbp->f_ffree = (fsfilcnt64_t)0;
	sbp->f_favail = (fsfilcnt64_t)0;
	(void) cmpldev(&d32, vfsp->vfs_dev);
	sbp->f_fsid = d32;
	(void) strcpy(sbp->f_basetype, vfssw[vfsp->vfs_fstype].vsw_name);
	sbp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sbp->f_namemax = fsp->hsfs_namemax;
	(void) strcpy(sbp->f_fstr, fsp->hsfs_vol.vol_id);

	return (0);
}

static int
hsfs_vget(struct vfs *vfsp, struct vnode **vpp, struct fid *fidp)
{
	struct hsfid *fid;
	struct hstable *htp;
	uint32_t nodeid;
	int error;

	htp = ((struct hsfs *)VFS_TO_HSFS(vfsp))->hsfs_hstbl;
	fid = (struct hsfid *)fidp;

	/*
	 * Look for vnode on hashlist.
	 * If found, it's now active and the refcnt was incremented.
	 */

	rw_enter(&htp->hshash_lock, RW_READER);

	nodeid = MAKE_NODEID(fid->hf_dir_lbn, fid->hf_dir_off, vfsp);

	if ((*vpp = hs_findhash(nodeid, vfsp)) == NULL) {
		/*
		 * Not in cache, so we need to remake it.
		 * hs_remakenode() will read the directory entry
		 * and then check again to see if anyone else has
		 * put it in the cache.
		 */
		rw_exit(&htp->hshash_lock);
		error = hs_remakenode(fid->hf_dir_lbn, (uint_t)fid->hf_dir_off,
		    vfsp, vpp);
		return (error);
	}
	rw_exit(&htp->hshash_lock);
	return (0);
}


#define	CHECKSUM_SIZE				(64 * 1024)

/*
 * Compute a CD-ROM fsid by checksumming the first 64K of data on the CD
 * We use the 'fsp' argument to determine the location of the root
 * directory entry, and we start reading from there.
 */
static int
compute_cdrom_id(struct hsfs *fsp, vnode_t *devvp)
{
	uint_t		secno;
	struct hs_volume *hsvp = &fsp->hsfs_vol;
	struct buf	*bp;
	int		error;
	int		fsid;
	int		size = CHECKSUM_SIZE;

	secno = hsvp->root_dir.ext_lbn >> hsvp->lbn_secshift;
	bp = bread(devvp->v_rdev, secno * 4, size);
	error = geterror(bp);
	if (!error) {
		int *ibuf = (int *)bp->b_un.b_addr;
		int isize = size / sizeof (int);
		int i;

		fsid = 0;

		for (i = 0; i < isize; i++)
			fsid ^= ibuf[ i ];
	} else	/* use creation date */
		fsid = hsvp->cre_date.tv_sec;

	brelse(bp);

	return (fsid);
}


/*ARGSUSED*/
static int
hs_mountfs(
	struct vfs	*vfsp,
	dev_t		dev,
	char		*path,
	mode_t		mode,
	int		mount_flags,
	struct cred	*cr,
	int		isroot)
{
	struct vnode		*devvp = NULL;
	register struct hsfs	*tsp;
	register struct hsfs	*fsp = NULL;
	struct vattr		vap;
	struct hsnode		*hp;
	int			error;
	struct timeval		tv;
	int			fsid;
	int			use_rrip = (mount_flags & HSFSMNT_NORRIP) == 0;

	/*
	 * Open the device
	 */
	devvp = makespecvp(dev, VBLK);
	ASSERT(devvp != 0);

	/*
	 * Open the target device (file) for read only.
	 */
	if (error = VOP_OPEN(&devvp, FREAD, cr)) {
		VN_RELE(devvp);
		return (error);
	}

	/*
	 * Refuse to go any further if this
	 * device is being used for swapping
	 */
	if (IS_SWAPVP(common_specvp(devvp))) {
		error = EBUSY;
		goto cleanup;
	}
	/*
	 * Init a new hsfs structure.
	 */
	fsp = kmem_zalloc(sizeof (*fsp), KM_SLEEP);

	vap.va_mask = AT_SIZE;
	if ((error = VOP_GETATTR(devvp, &vap, ATTR_COMM, cr)) != 0) {
		cmn_err(CE_NOTE, "Cannot get attributes of the CD-ROM driver");
		goto cleanup;
	}

	/*
	 * Make sure we have a nonzero size partition.
	 * The current version of the SD driver will *not* fail the open
	 * of such a partition so we have to check for it here.
	 */
	if (vap.va_size == 0) {
		error = ENXIO;
		goto cleanup;
	}

	/* hardwire perms, uid, gid */
	fsp->hsfs_vol.vol_uid = hsfs_default_uid;
	fsp->hsfs_vol.vol_gid =  hsfs_default_gid;
	fsp->hsfs_vol.vol_prot = hsfs_default_mode;

	/*
	 * Look for a Standard File Structure Volume Descriptor,
	 * of which there must be at least one.
	 * If found, check for volume size consistency.
	 *
	 * XXX - va_size may someday not be large enough to do this correctly.
	 */
	error = hs_findhsvol(fsp, devvp, &fsp->hsfs_vol);
	if (error == EINVAL) /* not in hs format, try iso 9660 format */
		error = hs_findisovol(fsp, devvp, &fsp->hsfs_vol);

	if (error)
		goto cleanup;

	/*
	 * Generate a file system ID from the CD-ROM,
	 * and check it for uniqueness.
	 *
	 * What we are aiming for is some chance of integrity
	 * across disk change.  That is, if a client has an fhandle,
	 * it will be valid as long as the same disk is mounted.
	 */
	fsid = compute_cdrom_id(fsp, devvp);

	mutex_enter(&hs_mounttab_lock);

	if (fsid == 0 || fsid == -1) {
		uniqtime(&tv);
		fsid = tv.tv_sec;
	} else	/* make sure that the fsid is unique */
		for (tsp = hs_mounttab; tsp != NULL; tsp = tsp->hsfs_next) {
			if (fsid == tsp->hsfs_vfs->vfs_fsid.val[0]) {
				uniqtime(&tv);
				fsid = tv.tv_sec;
				break;
			}
		}

	fsp->hsfs_next = hs_mounttab;
	hs_mounttab = fsp;

	fsp->hsfs_devvp = devvp;
	fsp->hsfs_vfs = vfsp;
	fsp->ext_impl_bits = 0L;
	fsp->hsfs_err_flags = 0L;
	fsp->hsfs_fsmnt = kmem_alloc(strlen(path) + 1, KM_SLEEP);
	(void) strcpy(fsp->hsfs_fsmnt, path);

	vfsp->vfs_data = (caddr_t)fsp;
	vfsp->vfs_dev = dev;
	vfsp->vfs_fstype = hsfsfstype;
	vfsp->vfs_bsize = fsp->hsfs_vol.lbn_size; /* %% */
	vfsp->vfs_fsid.val[0] = fsid;
	vfsp->vfs_fsid.val[1] =  hsfsfstype;

	/* initialize incore hsnode table */
	fsp->hsfs_hstbl = hs_inithstbl(vfsp);

	/*
	 * If the root directory does not appear to be
	 * valid, use what it points to as "." instead.
	 * Some Defense Mapping Agency disks are non-conformant
	 * in this way.
	 */
	if (!hsfs_valid_dir(&fsp->hsfs_vol.root_dir)) {
		hs_log_bogus_disk_warning(fsp, HSFS_ERR_BAD_ROOT_DIR, 0);
		if (hs_remakenode(fsp->hsfs_vol.root_dir.ext_lbn,
			    (uint_t)0, vfsp, &fsp->hsfs_rootvp)) {
			error = EINVAL;
			goto cleanup;
		}
	} else {
		fsp->hsfs_rootvp = hs_makenode(&fsp->hsfs_vol.root_dir,
			fsp->hsfs_vol.root_dir.ext_lbn, 0, vfsp);
	}

	/* mark vnode as VROOT */
	fsp->hsfs_rootvp->v_flag |= VROOT;

	/* Here we take care of some special case stuff for mountroot */
	if (isroot) {
		fsp->hsfs_rootvp->v_rdev = devvp->v_rdev;
		rootvp = fsp->hsfs_rootvp;
	}

	/* XXX - ignore the path table for now */
	fsp->hsfs_ptbl = NULL;
	hp = VTOH(fsp->hsfs_rootvp);
	hp->hs_ptbl_idx = NULL;

	if (use_rrip)
		hs_check_root_dirent(fsp->hsfs_rootvp, &(hp->hs_dirent));

	fsp->hsfs_namemax = IS_RRIP_IMPLEMENTED(fsp)
					? RRIP_FILE_NAMELEN
					: ISO_FILE_NAMELEN;
	/*
	 * if RRIP, don't copy NOMAPLCASE or NOTRAILDOT to hsfs_flags
	 */
	if (IS_RRIP_IMPLEMENTED(fsp))
		mount_flags &= ~(HSFSMNT_NOMAPLCASE | HSFSMNT_NOTRAILDOT);

	fsp->hsfs_flags = mount_flags;

	/* set the magic word */
	fsp->hsfs_magic = HSFS_MAGIC;
	mutex_exit(&hs_mounttab_lock);

	return (0);

cleanup:
	(void) VOP_CLOSE(devvp, FREAD, 1, (offset_t)0, cr);
	VN_RELE(devvp);
	if (fsp)
		kmem_free(fsp, sizeof (*fsp));
	return (error);
}

/*
 * hs_findhsvol()
 *
 * Locate the Standard File Structure Volume Descriptor and
 * parse it into an hs_volume structure.
 *
 * XXX - May someday want to look for Coded Character Set FSVD, too.
 */
static int
hs_findhsvol(struct hsfs *fsp, struct vnode *vp, struct hs_volume *hvp)
{
	register struct buf *secbp;
	register int i;
	uchar_t *volp;
	int error;
	uint_t secno;

	secno = hs_findvoldesc(vp->v_rdev, HS_VOLDESC_SEC);
	secbp = bread(vp->v_rdev, secno * 4, HS_SECTOR_SIZE);
	error = geterror(secbp);

	if (error != 0) {
		cmn_err(CE_NOTE, "hs_findhsvol: bread: error=(%d)", error);
		brelse(secbp);
		return (error);
	}

	volp = (uchar_t *)secbp->b_un.b_addr;

	while (HSV_DESC_TYPE(volp) != VD_EOV) {
		for (i = 0; i < HSV_ID_STRLEN; i++)
			if (HSV_STD_ID(volp)[i] != HSV_ID_STRING[i])
				goto cantfind;
		if (HSV_STD_VER(volp) != HSV_ID_VER)
			goto cantfind;
		switch (HSV_DESC_TYPE(volp)) {
		case VD_SFS:
			/* Standard File Structure */
			fsp->hsfs_vol_type = HS_VOL_TYPE_HS;
			error = hs_parsehsvol(fsp, volp, hvp);
			brelse(secbp);
			return (error);

		case VD_CCFS:
			/* Coded Character File Structure */
		case VD_BOOT:
		case VD_UNSPEC:
			break;
		}
		brelse(secbp);
		++secno;
		secbp = bread(vp->v_rdev, secno * 4, HS_SECTOR_SIZE);

		error = geterror(secbp);

		if (error != 0) {
			cmn_err(CE_NOTE, "hs_findhsvol: bread: error=(%d)",
				error);
			brelse(secbp);
			return (error);
		}

		volp = (uchar_t *)secbp->b_un.b_addr;
	}
cantfind:
	brelse(secbp);
	return (EINVAL);
}

/*
 * hs_parsehsvol
 *
 * Parse the Standard File Structure Volume Descriptor into
 * an hs_volume structure.  We can't just bcopy it into the
 * structure because of byte-ordering problems.
 *
 */
static int
hs_parsehsvol(struct hsfs *fsp, uchar_t *volp, struct hs_volume *hvp)
{
	hvp->vol_size = HSV_VOL_SIZE(volp);
	hvp->lbn_size = HSV_BLK_SIZE(volp);
	hvp->lbn_shift = ffs((long)hvp->lbn_size) - 1;
	hvp->lbn_secshift = ffs((long)howmany(HS_SECTOR_SIZE,
				(int)hvp->lbn_size)) - 1;
	hvp->lbn_maxoffset = hvp->lbn_size - 1;
	hs_parse_longdate(HSV_cre_date(volp), &hvp->cre_date);
	hs_parse_longdate(HSV_mod_date(volp), &hvp->mod_date);
	hvp->file_struct_ver = HSV_FILE_STRUCT_VER(volp);
	hvp->ptbl_len = HSV_PTBL_SIZE(volp);
	hvp->vol_set_size = (ushort_t)HSV_SET_SIZE(volp);
	hvp->vol_set_seq = (ushort_t)HSV_SET_SEQ(volp);
#if defined(_LITTLE_ENDIAN)
	hvp->ptbl_lbn = HSV_PTBL_MAN_LS(volp);
#else
	hvp->ptbl_lbn = HSV_PTBL_MAN_MS(volp);
#endif
	hs_copylabel(hvp, HSV_VOL_ID(volp));

	/*
	 * Make sure that lbn_size is a power of two and otherwise valid.
	 */
	if (hvp->lbn_size & ~(1 << hvp->lbn_shift)) {
		cmn_err(CE_NOTE,
			"hsfs: %d-byte logical block size not supported",
			hvp->lbn_size);
		return (EINVAL);
	}
	return (hs_parsedir(fsp, HSV_ROOT_DIR(volp), &hvp->root_dir,
			(char *)NULL, (int *)NULL));
}

/*
 * hs_findisovol()
 *
 * Locate the Primary Volume Descriptor
 * parse it into an hs_volume structure.
 *
 * XXX - Supplementary, Partition not yet done
 */
static int
hs_findisovol(struct hsfs *fsp, struct vnode *vp,
    struct hs_volume *hvp)
{
	register struct buf *secbp;
	register int i;
	uchar_t *volp;
	int error;
	uint_t secno;
	int found = 0;

	secno = hs_findvoldesc(vp->v_rdev, ISO_VOLDESC_SEC);
	secbp = bread(vp->v_rdev, secno * 4, ISO_SECTOR_SIZE);
	error = geterror(secbp);

	if (error != 0) {
		cmn_err(CE_NOTE, "hs_findisovol: bread: error=(%d)", error);
		brelse(secbp);
		return (error);
	}

	volp = (uchar_t *)secbp->b_un.b_addr;

	while ((enum iso_voldesc_type) ISO_DESC_TYPE(volp) != ISO_VD_EOV) {
		for (i = 0; i < ISO_ID_STRLEN; i++)
			if (ISO_STD_ID(volp)[i] != ISO_ID_STRING[i])
				goto cantfind;
		if (ISO_STD_VER(volp) != ISO_ID_VER)
			goto cantfind;
		switch (ISO_DESC_TYPE(volp)) {
		case ISO_VD_PVD:
			/* Standard File Structure */
			fsp->hsfs_vol_type = HS_VOL_TYPE_ISO;
			if (error = hs_parseisovol(fsp, volp, hvp)) {
				brelse(secbp);
				return (error);
			}
			found = 1;
			break;
		case ISO_VD_SVD:
			/* Supplementary Volume Descriptor */
			break;
		case ISO_VD_BOOT:
			break;
		case ISO_VD_VPD:
			/* currently cannot handle partition */
			break;
		}
		brelse(secbp);
		++secno;
		secbp = bread(vp->v_rdev, secno * 4, HS_SECTOR_SIZE);
		error = geterror(secbp);

		if (error != 0) {
			cmn_err(CE_NOTE, "hs_findisovol: bread: error=(%d)",
				    error);
			brelse(secbp);
			return (error);
		}

		volp = (uchar_t *)secbp->b_un.b_addr;
	}
	if (found) {
		brelse(secbp);
		return (0);
	}
cantfind:
	brelse(secbp);
	return (EINVAL);
}
/*
 * hs_parseisovol
 *
 * Parse the Primary Volume Descriptor into an hs_volume structure.
 *
 */
static int
hs_parseisovol(struct hsfs *fsp, uchar_t *volp, struct hs_volume *hvp)
{
	hvp->vol_size = ISO_VOL_SIZE(volp);
	hvp->lbn_size = ISO_BLK_SIZE(volp);
	hvp->lbn_shift = ffs((long)hvp->lbn_size) - 1;
	hvp->lbn_secshift = ffs((long)howmany(ISO_SECTOR_SIZE,
				(int)hvp->lbn_size)) - 1;
	hvp->lbn_maxoffset = hvp->lbn_size - 1;
	hs_parse_longdate(ISO_cre_date(volp), &hvp->cre_date);
	hs_parse_longdate(ISO_mod_date(volp), &hvp->mod_date);
	hvp->file_struct_ver = ISO_FILE_STRUCT_VER(volp);
	hvp->ptbl_len = ISO_PTBL_SIZE(volp);
	hvp->vol_set_size = (ushort_t)ISO_SET_SIZE(volp);
	hvp->vol_set_seq = (ushort_t)ISO_SET_SEQ(volp);
#if defined(_LITTLE_ENDIAN)
	hvp->ptbl_lbn = ISO_PTBL_MAN_LS(volp);
#else
	hvp->ptbl_lbn = ISO_PTBL_MAN_MS(volp);
#endif
	hs_copylabel(hvp, ISO_VOL_ID(volp));

	/*
	 * Make sure that lbn_size is a power of two and otherwise valid.
	 */
	if (hvp->lbn_size & ~(1 << hvp->lbn_shift)) {
		cmn_err(CE_NOTE,
			"hsfs: %d-byte logical block size not supported",
			hvp->lbn_size);
		return (EINVAL);
	}
	return (hs_parsedir(fsp, ISO_ROOT_DIR(volp), &hvp->root_dir,
			(char *)NULL, (int *)NULL));
}

/*
 * Common code for mount and umount.
 * Check that the user's argument is a reasonable
 * thing on which to mount, and return the device number if so.
 */
static int
hs_getmdev(char *fspec, int flags, dev_t *pdev, mode_t *mode)
{
	register int error;
	struct vnode *vp;
	struct vattr vap;
	register dev_t dev;

	/*
	 * Get the device to be mounted
	 */
	error = lookupname(fspec, (flags & MS_SYSSPACE) ?
	    UIO_SYSSPACE : UIO_USERSPACE, FOLLOW, NULLVPP, &vp);
	if (error) {
		if (error == ENOENT) {
			return (ENODEV);	/* needs translation */
		}
		return (error);
	}
	if (vp->v_type != VBLK) {
		VN_RELE(vp);
		return (ENOTBLK);
	}
	vap.va_mask = AT_MODE;		/* get protection mode */
	(void) VOP_GETATTR(vp, &vap, 0, CRED());
	*mode = vap.va_mode;

	dev = *pdev = vp->v_rdev;
	VN_RELE(vp);

	/*
	 * Ensure that this device isn't already mounted,
	 * unless this is a REMOUNT request or we are told to suppress
	 * mount checks.
	 */
	if ((flags & MS_NOCHECK) == 0 && vfs_devismounted(dev)) {
		if (!(flags & MS_REMOUNT))
			return (EBUSY);
	}

	if (getmajor(*pdev) >= devcnt)
		return (ENXIO);
	return (0);
}

static void
hs_copylabel(struct hs_volume *hvp, unsigned char *label)
{
	/* cdrom volid is at most 32 bytes */
	bcopy(label, hvp->vol_id, 32);
	hvp->vol_id[31] = NULL;
}

/*
 * Mount root file system.
 * "why" is ROOT_INIT on initial call, ROOT_REMOUNT if called to
 * remount the root file system, and ROOT_UNMOUNT if called to
 * unmount the root (e.g., as part of a system shutdown).
 *
 * XXX - this may be partially machine-dependent; it, along with the VFS_SWAPVP
 * operation, goes along with auto-configuration.  A mechanism should be
 * provided by which machine-INdependent code in the kernel can say "get me the
 * right root file system" and "get me the right initial swap area", and have
 * that done in what may well be a machine-dependent fashion.
 * Unfortunately, it is also file-system-type dependent (NFS gets it via
 * bootparams calls, UFS gets it from various and sundry machine-dependent
 * mechanisms, as SPECFS does for swap).
 */
static int
hsfs_mountroot(struct vfs *vfsp, enum whymountroot why)
{
	register int error;
	struct hsfs *fsp;
	struct hs_volume *fvolp;
	static int hsfsrootdone = 0;
	dev_t rootdev;
	mode_t mode = 0;

	if (why == ROOT_INIT) {
		if (hsfsrootdone++)
			return (EBUSY);
		rootdev = getrootdev();
		if (rootdev == (dev_t)NODEV)
			return (ENODEV);
		vfsp->vfs_dev = rootdev;
		vfsp->vfs_flag |= VFS_RDONLY;
	} else if (why == ROOT_REMOUNT) {
		cmn_err(CE_NOTE, "hsfs_mountroot: ROOT_REMOUNT");
		return (0);
	} else if (why == ROOT_UNMOUNT) {
		return (0);
	}
	error = vfs_lock(vfsp);
	if (error) {
		cmn_err(CE_NOTE, "hsfs_mountroot: couldn't get vfs_lock");
		return (error);
	}

	error = hs_mountfs(vfsp, rootdev, "/", mode, 1, CRED(), 1);
	/*
	 * XXX - assumes root device is not indirect, because we don't set
	 * rootvp.  Is rootvp used for anything?  If so, make another arg
	 * to mountfs (in S5 case too?)
	 */
	if (error) {
		vfs_unlock(vfsp);
		if (rootvp) {
			VN_RELE(rootvp);
			rootvp = (struct vnode *)0;
		}
		return (error);
	}
	if (why == ROOT_INIT)
		vfs_add((struct vnode *)0, vfsp,
		    (vfsp->vfs_flag & VFS_RDONLY) ? MS_RDONLY : 0);
	vfs_unlock(vfsp);
	fsp = VFS_TO_HSFS(vfsp);
	fvolp = &fsp->hsfs_vol;
#ifdef HSFS_CLKSET
	if (fvolp->cre_date.tv_sec == 0) {
	    cmn_err(CE_NOTE, "hsfs_mountroot: cre_date.tv_sec == 0");
	    if (fvolp->mod_date.tv_sec == 0) {
		cmn_err(CE_NOTE, "hsfs_mountroot: mod_date.tv_sec == 0");
		cmn_err(CE_NOTE, "hsfs_mountroot: clkset(-1L)");
		clkset(-1L);
	    } else
		clkset(fvolp->mod_date.tv_sec);
	} else
	    clkset(fvolp->mod_date.tv_sec);
#else	/* HSFS_CLKSET */
	clkset(-1L);
#endif	/* HSFS_CLKSET */
	return (0);
}

/*
 * XXX There is currently a bug, where e_ddi_getprop does not pass
 * XXX rdev properly through layered device drivers.  This is
 * XXX bugid 1109020.
 *
 * XXX Our solution to this problem is to not check the "kernel-ioctl"
 * XXX property, and just try the ioctl.  The worst that happens
 * XXX is that we get an EFAULT.  When 1109020 is fixed and the
 * XXX vol driver properly implements this stuff, we'll switch
 * XXX to using the first (and more correct) implementation of this
 * XXX function. (halt)
 */
#ifdef notdef
/*
 * hs_findvoldesc()
 *
 * Return the sector where the volume descriptor lives.  This is
 * a fixed value for "normal" cd-rom's, but can change for
 * multisession cd's.
 *
 * desc_sec is the same for high-sierra and iso 9660 formats, why
 * there are two differnt #defines used in the code for this is
 * beyond me.  These are standards, cast in concrete, right?
 * To be general, however, this function supports passing in different
 * values.
 *
 * XXX WARNING: this function requires testing before removing the notdef!
 */
static int
hs_findvoldesc(dev_t rdev, int desc_sec)
{
	int secno;
	int error;
	int rval;	/* ignored */

	/*
	 * Test the driver property to see if t it supports layered
	 * ioctl calls.
	 */
	if (e_ddi_getprop(rdev, VCHR, DDI_KERNEL_IOCTL,
	    DDI_PROP_NOTPROM|DDI_PROP_DONTPASS, 0) == 1) {
		/*
		 * Issue the Read Offset ioctl directly to the
		 * device. Ignore any errors and set starting
		 * secno to the default, otherwise add the
		 * VOLDESC sector number to the offset.
		 */
		error = cdev_ioctl(rdev, CDROMREADOFFSET, (intptr_t)&secno,
		    FKIOCTL|FREAD, CRED(), &rval);
		if (error) {
			secno = desc_sec;
		} else {
			secno += desc_sec;
		}
	} else {
		secno = desc_sec;
	}
	return (secno);
}
#else
/*
 * hs_findvoldesc()
 *
 * Return the sector where the volume descriptor lives.  This is
 * a fixed value for "normal" cd-rom's, but can change for
 * multisession cd's.
 *
 * desc_sec is the same for high-sierra and iso 9660 formats, why
 * there are two differnt #defines used in the code for this is
 * beyond me.  These are standards, cast in concrete, right?
 * To be general, however, this function supports passing in different
 * values.
 */
static int
hs_findvoldesc(dev_t rdev, int desc_sec)
{
	int secno;
	int error;
	int rval;	/* ignored */

#ifdef CDROMREADOFFSET
	/*
	 * Issue the Read Offset ioctl directly to the
	 * device. Ignore any errors and set starting
	 * secno to the default, otherwise add the
	 * VOLDESC sector number to the offset.
	 */
	error = cdev_ioctl(rdev, CDROMREADOFFSET, (intptr_t)&secno,
	    FKIOCTL|FREAD, CRED(), &rval);
	if (error) {
		secno = desc_sec;
	} else {
		secno += desc_sec;
	}
#else
	secno = desc_sec;
#endif

	return (secno);
}
#endif notdef
