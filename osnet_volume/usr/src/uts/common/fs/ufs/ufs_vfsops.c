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
 */

/*
 * Copyright (c) 1986-1989,1993,1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ufs_vfsops.c	2.194	99/08/08 SMI"

/* From  "ufs_vfsops.c 2.55     90/01/08 SMI"  */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitmap.h>
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
#include <sys/dnlc.h>
#include <sys/kstat.h>
#include <sys/acl.h>
#include <sys/fs/ufs_fsdir.h>
#include <sys/fs/ufs_fs.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_mount.h>
#include <sys/fs/ufs_acl.h>
#include <sys/fs/ufs_panic.h>
#include <sys/fs/ufs_bio.h>
#include <sys/fs/ufs_quota.h>
#include <sys/fs/ufs_log.h>
#undef NFS
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/mntent.h>
#include <sys/swap.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include "fs/fs_subr.h"
#include <sys/cmn_err.h>
#include <sys/dnlc.h>
#include <sys/sunddi.h>
#include <sys/bootconf.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern struct vfsops	ufs_vfsops;
extern int		ufsinit();
static int		mountfs();
extern int		highbit();
extern struct instats	ins;
extern struct vnode *common_specvp(struct vnode *vp);

struct  dquot *dquot, *dquotNDQUOT;

/*
 * UFS Mount options table
 */
static char *intr_cancel[] = { MNTOPT_NOINTR, NULL };
static char *nointr_cancel[] = { MNTOPT_INTR, NULL };
static char *forcedirectio_cancel[] = { MNTOPT_NOFORCEDIRECTIO, NULL };
static char *noforcedirectio_cancel[] = { MNTOPT_FORCEDIRECTIO, NULL };
static char *largefiles_cancel[] = { MNTOPT_NOLARGEFILES, NULL };
static char *nolargefiles_cancel[] = { MNTOPT_LARGEFILES, NULL };
static char *logging_cancel[] = { MNTOPT_NOLOGGING, NULL };
static char *nologging_cancel[] = { MNTOPT_LOGGING, NULL };
static char *quota_cancel[] = { MNTOPT_NOQUOTA, NULL };
static char *noquota_cancel[] = { MNTOPT_QUOTA, NULL };
static char *dfratime_cancel[] = { MNTOPT_NODFRATIME, NULL };
static char *nodfratime_cancel[] = { MNTOPT_DFRATIME, NULL };
static char *suid_cancel[] = { MNTOPT_NOSUID, NULL };
static char *nosuid_cancel[] = { MNTOPT_SUID, NULL };
static char *ro_cancel[] = { MNTOPT_RW, NULL };
static char *rw_cancel[] = { MNTOPT_RO, NULL };

static mntopt_t mntopts[] = {
/*
 *	option name		cancel option	default arg	flags
 *		ufs arg flag
 */
	{ MNTOPT_RO,		ro_cancel,	NULL,		0,
		(void *)0 },
	{ MNTOPT_RW,		rw_cancel,	NULL,		MO_DEFAULT,
		(void *)0 },
	{ MNTOPT_INTR,		intr_cancel,	NULL,		MO_DEFAULT,
		(void *)0 },
	{ MNTOPT_NOINTR,	nointr_cancel,	NULL,		0,
		(void *)UFSMNT_NOINTR },
	{ MNTOPT_SYNCDIR,	NULL,		NULL,		0,
		(void *)UFSMNT_SYNCDIR },
	{ MNTOPT_FORCEDIRECTIO,	forcedirectio_cancel, NULL,	0,
		(void *)UFSMNT_FORCEDIRECTIO },
	{ MNTOPT_NOFORCEDIRECTIO, noforcedirectio_cancel, NULL, 0,
		(void *)UFSMNT_NOFORCEDIRECTIO },
	{ MNTOPT_NOSETSEC,	NULL,		NULL,		0,
		(void *)UFSMNT_NOSETSEC },
	{ MNTOPT_LARGEFILES,	largefiles_cancel, NULL,	MO_DEFAULT,
		(void *)UFSMNT_LARGEFILES },
	{ MNTOPT_NOLARGEFILES,	nolargefiles_cancel, NULL,	0,
		(void *)0 },
	{ MNTOPT_LOGGING,	logging_cancel, NULL,		MO_TAG,
		(void *)UFSMNT_LOGGING },
	{ MNTOPT_NOLOGGING,	nologging_cancel, NULL,
		MO_NODISPLAY|MO_DEFAULT|MO_TAG, (void *)0 },
	{ MNTOPT_QUOTA,		quota_cancel, NULL,		MO_IGNORE,
		(void *)0 },
	{ MNTOPT_NOQUOTA,	noquota_cancel,	NULL,
		MO_NODISPLAY|MO_DEFAULT, (void *)0 },
	{ MNTOPT_GLOBAL,	NULL,		NULL,		0,
		(void *)0 },
	{ MNTOPT_NOATIME,	NULL,		NULL,		0,
		(void *)UFSMNT_NOATIME },
	{ MNTOPT_DFRATIME,	dfratime_cancel, NULL,		0,
		(void *)0 },
	{ MNTOPT_NODFRATIME,	nodfratime_cancel, NULL,
		MO_NODISPLAY|MO_DEFAULT, (void *)UFSMNT_NODFRATIME },
	{ MNTOPT_ONERROR,	NULL,		UFSMNT_ONERROR_PANIC_STR,
		MO_DEFAULT|MO_HASVALUE,	(void *)0 },
	{ MNTOPT_NOSUID,	nosuid_cancel,	NULL,		0,
		(void *)0 },
	{ MNTOPT_SUID,		suid_cancel,	NULL,		MO_DEFAULT,
		(void *)0 },
};

static mntopts_t ufs_mntopts = {
	sizeof (mntopts) / sizeof (mntopt_t),
	mntopts
};

static struct vfssw vfw = {
	"ufs",
	ufsinit,
	&ufs_vfsops,
	VSW_HASPROTO,
	&ufs_mntopts
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_fsops;

static struct modlfs modlfs = {
	&mod_fsops, "filesystem for ufs", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};

/*
 * An attempt has been made to make this module unloadable.  In order to
 * test it, we need a system in which the root fs is NOT ufs.  THIS HAS NOT
 * BEEN DONE
 */

char _depends_on[] = "fs/specfs strmod/rpcmod";

extern kstat_t *ufs_inode_kstat;

int
_init(void)
{
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

extern struct vnode *makespecvp(dev_t dev, vtype_t type);

extern int ufsfstype;

static int mountfs(struct vfs *, enum whymountroot, dev_t, char *,
		struct cred *, int, void *, int);


/*
 * XXX - this appears only to be used by the VM code to handle the case where
 * UNIX is running off the mini-root.  That probably wants to be done
 * differently.
 */
struct vnode *rootvp;

static int
ufs_mount(vfsp, mvp, uap, cr)
	struct vfs *vfsp;
	struct vnode *mvp;
	struct mounta *uap;
	struct cred *cr;
{
	char *data = uap->dataptr;
	int datalen = uap->datalen;
	dev_t dev;
	struct vnode *bvp;
	struct pathname dpn;
	int error;
	enum whymountroot why;
	struct ufs_args args;
	int fromspace = (uap->flags & MS_SYSSPACE) ?
	    UIO_SYSSPACE : UIO_USERSPACE;

	if (!suser(cr))
		return (EPERM);

	if (mvp->v_type != VDIR)
		return (ENOTDIR);

	mutex_enter(&mvp->v_lock);
	if ((uap->flags & MS_REMOUNT) == 0 &&
	    (uap->flags & MS_OVERLAY) == 0 &&
	    (mvp->v_count != 1 || (mvp->v_flag & VROOT))) {
		mutex_exit(&mvp->v_lock);
		return (EBUSY);
	}
	mutex_exit(&mvp->v_lock);

	/*
	 * Get arguments
	 */
	bzero(&args, sizeof (args));
	if ((uap->flags & MS_DATA) && data != NULL && datalen != 0) {
		int copy_result = 0;

		if (uap->flags & MS_SYSSPACE)
			bcopy(data, &args, datalen);
		else
			copy_result = copyin(data, &args, datalen);
		if (copy_result)
			return (EFAULT);
		datalen = sizeof (struct ufs_args);
	} else {
		datalen = 0;
	}
	/*
	 * Read in the mount point pathname
	 * (so we can record the direcory the file system was last mounted on).
	 */
	if (error = pn_get(uap->dir, fromspace, &dpn))
		return (error);

	/*
	 * Resolve path name of special file being mounted.
	 */
	if (error = lookupname(uap->spec, fromspace, FOLLOW, NULL, &bvp)) {
		pn_free(&dpn);
		return (error);
	}
	if (bvp->v_type != VBLK) {
		VN_RELE(bvp);
		pn_free(&dpn);
		return (ENOTBLK);
	}
	dev = bvp->v_rdev;
	VN_RELE(bvp);
	/*
	 * Ensure that this device isn't already mounted,
	 * unless this is a REMOUNT request or we are told to suppress
	 * mount checks.
	 */
	if ((uap->flags & MS_NOCHECK) == 0 && vfs_devismounted(dev)) {
		if (uap->flags & MS_REMOUNT)
			why = ROOT_REMOUNT;
		else {
			pn_free(&dpn);
			return (EBUSY);
		}
	} else {
		why = ROOT_INIT;
	}
	if (getmajor(dev) >= devcnt) {
		pn_free(&dpn);
		return (ENXIO);
	}

	/*
	 * If the device is a tape, mount it read only
	 */
	if (devopsp[getmajor(dev)]->devo_cb_ops->cb_flag & D_TAPE) {
		vfsp->vfs_flag |= VFS_RDONLY;
		vfs_setmntopt(&vfsp->vfs_mntopts, MNTOPT_RO, NULL, 0);
	}

	if (uap->flags & MS_RDONLY)
		vfsp->vfs_flag |= VFS_RDONLY;

	/*
	 * Mount the filesystem.
	 */
	error = mountfs(vfsp, why, dev, dpn.pn_path, cr, 0, &args, datalen);
	pn_free(&dpn);
	return (error);
}

/*
 * Mount root file system.
 * "why" is ROOT_INIT on initial call, ROOT_FRONTMOUNT to mount
 * the front filesystem (cachefs), ROOT_BACKMOUNT to mount the
 * back filesystem (cachefs) ROOT_REMOUNT if called to
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
ufs_mountroot(vfsp, why)
	struct vfs *vfsp;
	enum whymountroot why;
{
	struct fs *fsp;
	int error;
	static int ufsrootdone = 0;
	dev_t rootdev;
	struct vnode *vp;
	struct vnode *devvp = 0;
	int ovflags;
	int doclkset;

	if (why == ROOT_INIT || why == ROOT_BACKMOUNT ||
	    why == ROOT_FRONTMOUNT) {
		if (why == ROOT_INIT) {
			if (ufsrootdone++)
				return (EBUSY);
			rootdev = getrootdev();
			if (rootdev == (dev_t)NODEV)
				return (ENODEV);
		} else if (why == ROOT_BACKMOUNT) {
			rootdev = ddi_pathname_to_dev_t(backfs.bo_name);
			if (rootdev == (dev_t)NODEV) {
				cmn_err(CE_CONT,
				    "Cannot assemble drivers for %s\n",
				    backfs.bo_name);
				return (ENODEV);
			}
		} else {
			rootdev = ddi_pathname_to_dev_t(frontfs.bo_name);
			if (rootdev == (dev_t)NODEV) {
				cmn_err(CE_CONT,
				    "Cannot assemble drivers for %s\n",
				    frontfs.bo_name);
				return (ENODEV);
			}
		}
		vfsp->vfs_dev = rootdev;
#ifdef notneeded
		if ((boothowto & RB_WRITABLE) == 0) {
			/*
			 * We mount a ufs root file system read-only to
			 * avoid problems during fsck.   After fsck runs,
			 * we remount it read-write.
			 */
			vfsp->vfs_flag |= VFS_RDONLY;
		}
#endif
		/* we have problems remounting cachefs rw later on */
		if (why != ROOT_FRONTMOUNT)
			vfsp->vfs_flag |= VFS_RDONLY;

	} else if (why == ROOT_REMOUNT) {
		vp = ((struct ufsvfs *)vfsp->vfs_data)->vfs_devvp;
		(void) dnlc_purge_vfsp(vfsp, 0);
		vp = common_specvp(vp);
		(void) VOP_PUTPAGE(vp, (offset_t)0, (size_t)0, B_INVAL, CRED());
		(void) bfinval(vfsp->vfs_dev, 0);
		fsp = getfs(vfsp);

		ovflags = vfsp->vfs_flag;
		vfsp->vfs_flag &= ~VFS_RDONLY;
		vfsp->vfs_flag |= VFS_REMOUNT;
		rootdev = vfsp->vfs_dev;
	} else if (why == ROOT_UNMOUNT) {
		if (vfs_lock(vfsp) == 0) {
			(void) ufs_flush(vfsp);
			vfs_unlock(vfsp);
		} else {
			ufs_update(0);
		}
		vp = ((struct ufsvfs *)vfsp->vfs_data)->vfs_devvp;
		(void) VOP_CLOSE(vp, FREAD|FWRITE, 1,
			(offset_t)0, CRED());
		return (0);
	}
	error = vfs_lock(vfsp);
	if (error)
		return (error);

	/* If RO media, don't call clkset() (see below) */
	doclkset = 1;
	if (why == ROOT_INIT || why == ROOT_BACKMOUNT ||
	    why == ROOT_FRONTMOUNT) {
		devvp = makespecvp(rootdev, VBLK);
		error = VOP_OPEN(&devvp, FREAD|FWRITE, CRED());
		if (error == 0) {
			(void) VOP_CLOSE(devvp, FREAD|FWRITE, 1,
				(offset_t)0, CRED());
		} else {
			doclkset = 0;
		}
		VN_RELE(devvp);
	}

	error = mountfs(vfsp, why, rootdev, "/", CRED(), 1, NULL, 0);
	/*
	 * XXX - assumes root device is not indirect, because we don't set
	 * rootvp.  Is rootvp used for anything?  If so, make another arg
	 * to mountfs (in S5 case too?)
	 */
	if (error) {
		vfs_unlock(vfsp);
		if (why == ROOT_REMOUNT)
			vfsp->vfs_flag = ovflags;
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
	fsp = getfs(vfsp);
	clkset(doclkset ? fsp->fs_time : -1);
	return (0);
}

static int
remountfs(struct vfs *vfsp, dev_t dev, void *raw_argsp, int args_len)
{
	struct ufsvfs *ufsvfsp = (struct ufsvfs *)vfsp->vfs_data;
	struct ulockfs *ulp = &ufsvfsp->vfs_ulockfs;
	struct buf *bp = ufsvfsp->vfs_bufp;
	struct fs *fsp = (struct fs *)bp->b_un.b_addr;
	struct fs *fspt;
	struct buf *tpt = 0;
	int error = 0;
	int flags = 0;

	if (args_len == sizeof (struct ufs_args) && raw_argsp)
		flags = ((struct ufs_args *)raw_argsp)->flags;

	/* cannot remount to RDONLY */
	if (vfsp->vfs_flag & VFS_RDONLY)
		return (EINVAL);

	/* woops, wrong dev */
	if (vfsp->vfs_dev != dev)
		return (EINVAL);

	/*
	 * Load up the ufs logging module before we lock the
	 * file system. Otherwise we get a hang when we remount root
	 * to read/write during boot.
	 */
	if (fsp->fs_logbno) {
		(void) modload("misc", "ufs_log");
	}

	/*
	 * synchronize w/ufs ioctls
	 */
	mutex_enter(&ulp->ul_lock);

	/*
	 * reset options
	 */
	ufsvfsp->vfs_nointr  = flags & UFSMNT_NOINTR;
	ufsvfsp->vfs_syncdir = flags & UFSMNT_SYNCDIR;
	ufsvfsp->vfs_nosetsec = flags & UFSMNT_NOSETSEC;
	ufsvfsp->vfs_noatime = flags & UFSMNT_NOATIME;
	if ((flags & UFSMNT_NODFRATIME) || ufsvfsp->vfs_noatime)
		ufsvfsp->vfs_dfritime &= ~UFS_DFRATIME;
	else	/* dfratime, default behavior */
		ufsvfsp->vfs_dfritime |= UFS_DFRATIME;
	if (flags & UFSMNT_FORCEDIRECTIO)
		ufsvfsp->vfs_forcedirectio = 1;
	else if (flags & UFSMNT_NOFORCEDIRECTIO)
		ufsvfsp->vfs_forcedirectio = 0;
	ufsvfsp->vfs_iotstamp = lbolt;

	/*
	 * set largefiles flag in ufsvfs equal to the
	 * value passed in by the mount command. If
	 * it is "nolargefiles", and the flag is set
	 * in the superblock, the mount fails.
	 */
	if (!(flags & UFSMNT_LARGEFILES)) {  /* "nolargefiles" */
		if (fsp->fs_flags & FSLARGEFILES) {
			error = EFBIG;
			goto remounterr;
		}
		ufsvfsp->vfs_lfflags &= ~UFS_LARGEFILES;
	} else	/* "largefiles" */
		ufsvfsp->vfs_lfflags |= UFS_LARGEFILES;
	/*
	 * read/write to read/write; all done
	 */
	if (fsp->fs_ronly == 0)
		goto remounterr;

	/*
	 * fix-on-panic assumes RO->RW remount implies system-critical fs
	 * if it is shortly after boot; so, don't attempt to lock and fix
	 * (unless the user explicitly asked for another action on error)
	 * XXX UFSMNT_ONERROR_RDONLY rather than UFSMNT_ONERROR_PANIC
	 */
#define	BOOT_TIME_LIMIT	(180*hz)
	if (!(flags & UFSMNT_ONERROR_FLGMASK) && lbolt < BOOT_TIME_LIMIT) {
		cmn_err(CE_WARN, "%s is required to be mounted onerror=%s",
			ufsvfsp->vfs_fs->fs_fsmnt, UFSMNT_ONERROR_PANIC_STR);
		flags |= UFSMNT_ONERROR_PANIC;
	}

	if ((error = ufsfx_mount(ufsvfsp, flags)) != 0)
		goto remounterr;

	/*
	 * Lock the file system and flush stuff from memory
	 */
	error = ufs_quiesce(ulp);
	if (error)
		goto remounterr;
	error = ufs_flush(vfsp);
	if (error)
		goto remounterr;

	tpt = UFS_BREAD(ufsvfsp, ufsvfsp->vfs_dev, SBLOCK, SBSIZE);
	if (tpt->b_flags & B_ERROR) {
		error = EIO;
		goto remounterr;
	}
	fspt = (struct fs *)tpt->b_un.b_addr;
	if (fspt->fs_magic != FS_MAGIC || fspt->fs_bsize > MAXBSIZE ||
	    fspt->fs_frag > MAXFRAG ||
	    fspt->fs_bsize < sizeof (struct fs) ||
	    fspt->fs_bsize < PAGESIZE) {
		tpt->b_flags |= B_STALE | B_AGE;
		error = EINVAL;
		goto remounterr;
	}

	/*
	 * fsck may have changed the file system while we were
	 * mounted readonly. So we must reread the log info.
	 *
	 * Chicken and egg problem. The superblock may have deltas
	 * in the log.  So after the log is scanned we reread the
	 * superblock. We guarantee that the fields needed to
	 * scan the log will not be in the log.
	 */
	LUFS_UNSNARF(ufsvfsp);
	if (fspt->fs_logbno && fspt->fs_clean == FSLOG &&
	    (fspt->fs_state + (long)fspt->fs_time == FSOKAY) &&
	    (ufs_trans_check(ufsvfsp->vfs_dev) == NULL)) {
		LUFS_SNARF(ufsvfsp, fspt, 0, error);
		if (error) {
			cmn_err(CE_WARN, "Could not access the log for"
				" %s; Please run fsck(1M)", fsp->fs_fsmnt);
			goto remounterr;
		}
		tpt->b_flags |= (B_AGE | B_STALE);
		brelse(tpt);
		tpt = UFS_BREAD(ufsvfsp, ufsvfsp->vfs_dev, SBLOCK, SBSIZE);
		fspt = (struct fs *)tpt->b_un.b_addr;
		if (tpt->b_flags & B_ERROR) {
			error = EIO;
			goto remounterr;
		}
	}
	/*
	 * check for transaction device
	 */
	ufsvfsp->vfs_trans = ufs_trans_get(dev, vfsp);
	if (TRANS_ISERROR(ufsvfsp))
		goto remounterr;
	TRANS_DOMATAMAP(ufsvfsp);

	if ((fspt->fs_state + (long)fspt->fs_time == FSOKAY) &&
	    fspt->fs_clean == FSLOG && !TRANS_ISTRANS(ufsvfsp)) {
		ufsvfsp->vfs_trans = NULL;
		ufsvfsp->vfs_domatamap = 0;
		error = ENOSPC;
		goto remounterr;
	}

	if (fspt->fs_state + (long)fspt->fs_time == FSOKAY &&
	    (fspt->fs_clean == FSCLEAN ||
	    fspt->fs_clean == FSSTABLE ||
	    fspt->fs_clean == FSLOG)) {

		if (error = ufs_getsummaryinfo(vfsp->vfs_dev, ufsvfsp, fspt))
			goto remounterr;

		/* preserve mount name */
		(void) strncpy(fspt->fs_fsmnt, fsp->fs_fsmnt, MAXMNTLEN);
		/* free the old cg space */
		kmem_free(fsp->fs_u.fs_csp, fsp->fs_cssize);
		/* switch in the new superblock */
		bcopy(tpt->b_un.b_addr, bp->b_un.b_addr, fspt->fs_sbsize);

		fsp->fs_clean = FSSTABLE;
	} /* superblock updated in memory */
	tpt->b_flags |= B_STALE | B_AGE;
	brelse(tpt);
	tpt = 0;

	if (fsp->fs_clean != FSSTABLE) {
		error = ENOSPC;
		goto remounterr;
	}


	if (TRANS_ISTRANS(ufsvfsp)) {
		fsp->fs_clean = FSLOG;
		ufsvfsp->vfs_dio = 0;
	} else
		if (ufsvfsp->vfs_dio)
			fsp->fs_clean = FSSUSPEND;

	TRANS_MATA_MOUNT(ufsvfsp);

	fsp->fs_fmod = 0;
	fsp->fs_ronly = 0;

	cv_broadcast(&ulp->ul_cv);
	mutex_exit(&ulp->ul_lock);

	if (TRANS_ISTRANS(ufsvfsp)) {

		/*
		 * start the delete thread
		 */
		ufs_thread_start(&ufsvfsp->vfs_delete, ufs_thread_delete, vfsp);

		/*
		 * start the reclaim thread
		 */
		if (fsp->fs_reclaim & (FS_RECLAIM|FS_RECLAIMING)) {
			fsp->fs_reclaim &= ~FS_RECLAIM;
			fsp->fs_reclaim |=  FS_RECLAIMING;
			ufs_thread_start(&ufsvfsp->vfs_reclaim,
				ufs_thread_reclaim, vfsp);
		}
	}

	TRANS_SBWRITE(ufsvfsp, TOP_MOUNT);

	return (0);

remounterr:
	if (tpt)
		brelse(tpt);
	cv_broadcast(&ulp->ul_cv);
	mutex_exit(&ulp->ul_lock);
	return (error);
}

/*
 * The read and write cluster sizes are adjusted up by maxphys, limited
 * by ufs_maxmaxphys.
 */
int ufs_maxmaxphys = (1024 * 1024);

static int
mountfs(vfsp, why, dev, path, cr, isroot, raw_argsp, args_len)
struct vfs *vfsp;
enum whymountroot why;
dev_t dev;
char *path;
struct cred *cr;
int isroot;
void *raw_argsp;
int args_len;
{
	struct vnode *devvp = 0;
	struct fs *fsp;
	struct ufsvfs *ufsvfsp = 0;
	struct buf *bp = 0;
	struct buf *tp = 0;
	int error = 0;
	size_t len;
	int needclose = 0;
	int needtrans = 0;
	struct inode *rip;
	struct vnode *rvp = NULL;
	int flags = 0;

	if (args_len == sizeof (struct ufs_args) && raw_argsp)
		flags = ((struct ufs_args *)raw_argsp)->flags;

	ASSERT(vfs_lock_held(vfsp));

	if (why == ROOT_INIT || why == ROOT_BACKMOUNT ||
	    why == ROOT_FRONTMOUNT) {
		/*
		 * Open the device.
		 */
		devvp = makespecvp(dev, VBLK);

		/*
		 * Open block device mounted on.
		 * When bio is fixed for vnodes this can all be vnode
		 * operations.
		 */
		error = VOP_OPEN(&devvp,
		    (vfsp->vfs_flag & VFS_RDONLY) ? FREAD : FREAD|FWRITE, cr);
		if (error)
			goto out;
		needclose = 1;

		/*
		 * Refuse to go any further if this
		 * device is being used for swapping.
		 */
		if (IS_SWAPVP(devvp)) {
			error = EBUSY;
			goto out;
		}
	}

	/*
	 * check for dev already mounted on
	 */
	if (vfsp->vfs_flag & VFS_REMOUNT)
		return (remountfs(vfsp, dev, raw_argsp, args_len));

	ASSERT(devvp != 0);

	/*
	 * Flush back any dirty pages on the block device to
	 * try and keep the buffer cache in sync with the page
	 * cache if someone is trying to use block devices when
	 * they really should be using the raw device.
	 */
	(void) VOP_PUTPAGE(common_specvp(devvp), (offset_t)0,
	    (size_t)0, B_INVAL, cr);

	/*
	 * read in superblock
	 */
	ufsvfsp = kmem_zalloc(sizeof (struct ufsvfs), KM_SLEEP);
	tp = UFS_BREAD(ufsvfsp, dev, SBLOCK, SBSIZE);
	if (tp->b_flags & B_ERROR)
		goto out;
	fsp = (struct fs *)tp->b_un.b_addr;
	if (fsp->fs_magic != FS_MAGIC || fsp->fs_bsize > MAXBSIZE ||
	    fsp->fs_frag > MAXFRAG ||
	    fsp->fs_bsize < sizeof (struct fs) || fsp->fs_bsize < PAGESIZE) {
		error = EINVAL;	/* also needs translation */
		goto out;
	}

	/*
	 * Allocate VFS private data.
	 */
	vfsp->vfs_bcount = 0;
	vfsp->vfs_data = (caddr_t)ufsvfsp;
	vfsp->vfs_fstype = ufsfstype;
	vfsp->vfs_dev = dev;
	vfsp->vfs_flag |= VFS_NOTRUNC;
	vfs_make_fsid(&vfsp->vfs_fsid, dev, ufsfstype);
	ufsvfsp->vfs_devvp = devvp;

	/*
	 * Cross-link with vfs and add to instance list.
	 */
	ufsvfsp->vfs_vfs = vfsp;
	ufs_vfs_add(ufsvfsp);

	ufsvfsp->vfs_dev = dev;
	ufsvfsp->vfs_bufp = tp;

	ufsvfsp->vfs_dirsize = INODESIZE + (4 * ALLOCSIZE) + fsp->fs_fsize;
	ufsvfsp->vfs_minfrags = fsp->fs_dsize * fsp->fs_minfree / 100;
	/*
	 * if mount allows largefiles, indicate so in ufsvfs
	 */
	if (flags & UFSMNT_LARGEFILES)
		ufsvfsp->vfs_lfflags |= UFS_LARGEFILES;
	/*
	 * Initialize threads
	 */
	ufs_thread_init(&ufsvfsp->vfs_delete, 1);
	ufs_thread_init(&ufsvfsp->vfs_reclaim, 0);

	/*
	 * Chicken and egg problem. The superblock may have deltas
	 * in the log.  So after the log is scanned we reread the
	 * superblock. We guarantee that the fields needed to
	 * scan the log will not be in the log.
	 */
	if (fsp->fs_logbno && fsp->fs_clean == FSLOG &&
	    (fsp->fs_state + (long)fsp->fs_time == FSOKAY) &&
	    (ufs_trans_check(ufsvfsp->vfs_dev) == NULL)) {
		LUFS_SNARF(ufsvfsp, fsp, (vfsp->vfs_flag & VFS_RDONLY), error);
		if (error) {
			/*
			 * Allow a ro mount to continue even if the
			 * log cannot be processed - yet.
			 */
			if (!(vfsp->vfs_flag & VFS_RDONLY)) {
				cmn_err(CE_WARN, "Error accessing ufs "
					"log for %s; Please run fsck(1M)",
					path);
				goto out;
			}
		}
		tp->b_flags |= (B_AGE | B_STALE);
		brelse(tp);
		tp = UFS_BREAD(ufsvfsp, dev, SBLOCK, SBSIZE);
		fsp = (struct fs *)tp->b_un.b_addr;
		ufsvfsp->vfs_bufp = tp;
		if (tp->b_flags & B_ERROR)
			goto out;
	}

	/*
	 * Copy the super block into a buffer in its native size.
	 * Use ngeteblk to allocate the buffer
	 */
	bp = ngeteblk(fsp->fs_bsize);
	ufsvfsp->vfs_bufp = bp;
	bp->b_edev = dev;
	bp->b_dev = cmpdev(dev);
	bp->b_blkno = SBLOCK;
	bp->b_bcount = fsp->fs_sbsize;
	bcopy(tp->b_un.b_addr, bp->b_un.b_addr, fsp->fs_sbsize);
	tp->b_flags |= B_STALE | B_AGE;
	brelse(tp);
	tp = 0;

	fsp = (struct fs *)bp->b_un.b_addr;
	/*
	 * Mount fails if superblock flag indicates presence of large
	 * files and filesystem is attempted to be mounted 'nolargefiles'.
	 * The exception is for a read only mount of root, which we
	 * always want to succeed, so fsck can fix potential problems.
	 * The assumption is that we will remount root at some point,
	 * and the remount will enforce the mount option.
	 */
	if (!(isroot & (vfsp->vfs_flag & VFS_RDONLY)) &&
	    (fsp->fs_flags & FSLARGEFILES) &&
	    !(flags & UFSMNT_LARGEFILES)) {
		error = EFBIG;
		goto out;
	}

	if (vfsp->vfs_flag & VFS_RDONLY) {
		fsp->fs_ronly = 1;
		fsp->fs_fmod = 0;
		if (((fsp->fs_state + (long)fsp->fs_time) == FSOKAY) &&
		    (fsp->fs_clean == FSCLEAN ||
			fsp->fs_clean == FSSTABLE ||
			fsp->fs_clean == FSLOG))
			fsp->fs_clean = FSSTABLE;
		else
			fsp->fs_clean = FSBAD;
	} else {

		fsp->fs_fmod = 0;
		fsp->fs_ronly = 0;

		ufsvfsp->vfs_trans = ufs_trans_get(dev, vfsp);
		TRANS_DOMATAMAP(ufsvfsp);

		if ((TRANS_ISERROR(ufsvfsp)) ||
		    (((fsp->fs_state + (long)fsp->fs_time) == FSOKAY) &&
			fsp->fs_clean == FSLOG && !TRANS_ISTRANS(ufsvfsp))) {
			ufsvfsp->vfs_trans = NULL;
			ufsvfsp->vfs_domatamap = 0;
			error = ENOSPC;
			goto out;
		}

		if (((fsp->fs_state + (long)fsp->fs_time) == FSOKAY) &&
		    (fsp->fs_clean == FSCLEAN ||
			fsp->fs_clean == FSSTABLE ||
			fsp->fs_clean == FSLOG))
			fsp->fs_clean = FSSTABLE;
		else {
			if (isroot) {
				/*
				 * allow root partition to be mounted even
				 * when fs_state is not ok
				 * will be fixed later by a remount root
				 */
				fsp->fs_clean = FSBAD;
				ufsvfsp->vfs_trans = NULL;
				ufsvfsp->vfs_domatamap = 0;
			} else {
				error = ENOSPC;
				goto out;
			}
		}

		if (fsp->fs_clean == FSSTABLE && TRANS_ISTRANS(ufsvfsp))
			fsp->fs_clean = FSLOG;
	}
	TRANS_MATA_MOUNT(ufsvfsp);
	needtrans = 1;

	vfsp->vfs_bsize = fsp->fs_bsize;

	/*
	 * Read in summary info
	 */
	if (error = ufs_getsummaryinfo(dev, ufsvfsp, fsp))
		goto out;

	mutex_init(&ufsvfsp->vfs_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&ufsvfsp->vfs_rename_lock, NULL, MUTEX_DEFAULT, NULL);
	(void) copystr(path, fsp->fs_fsmnt, sizeof (fsp->fs_fsmnt) - 1, &len);
	bzero(fsp->fs_fsmnt + len, sizeof (fsp->fs_fsmnt) - len);

	/*
	 * Sanity checks for old file systems
	 */
	ufsvfsp->vfs_npsect = MAX(fsp->fs_npsect, fsp->fs_nsect);
	ufsvfsp->vfs_interleave = MAX(fsp->fs_interleave, 1);
	if (fsp->fs_postblformat == FS_42POSTBLFMT)
		ufsvfsp->vfs_nrpos = 8;
	else
		ufsvfsp->vfs_nrpos = fsp->fs_nrpos;

	/*
	 * Initialize lockfs structure to support file system locking
	 */
	bzero(&ufsvfsp->vfs_ulockfs.ul_lockfs,
	    sizeof (struct lockfs));
	ufsvfsp->vfs_ulockfs.ul_fs_lock = ULOCKFS_ULOCK;
	mutex_init(&ufsvfsp->vfs_ulockfs.ul_lock, NULL,
	    MUTEX_DEFAULT, NULL);
	cv_init(&ufsvfsp->vfs_ulockfs.ul_cv, NULL, CV_DEFAULT, NULL);

	/*
	 * We don't need to grab vfs_dqrwlock for this ufs_iget() call.
	 * We are in the process of mounting the file system so there
	 * is no need to grab the quota lock. If a quota applies to the
	 * root inode, then it will be updated when quotas are enabled.
	 *
	 * However, we have an ASSERT(RW_LOCK_HELD(&ufsvfsp->vfs_dqrwlock))
	 * in getinoquota() that we want to keep so grab it anyway.
	 */
	rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);

	error = ufs_iget(vfsp, UFSROOTINO, &rip, cr);

	rw_exit(&ufsvfsp->vfs_dqrwlock);

	if (error)
		goto out;

	/*
	 * make sure root inode is a directory.  Returning ENOTDIR might
	 * be confused with the mount point not being a directory, so
	 * we use EIO instead.
	 */
	if ((rip->i_mode & IFMT) != IFDIR) {
		/*
		 * Mark this inode as subject for cleanup
		 * to avoid stray inodes in the cache.
		 */
		rvp = ITOV(rip);
		error = EIO;
		goto out;
	}

	rvp = ITOV(rip);
	mutex_enter(&rvp->v_lock);
	rvp->v_flag |= VROOT;
	mutex_exit(&rvp->v_lock);
	ufsvfsp->vfs_root = rvp;
	/* The buffer for the root inode does not contain a valid b_vp */
	(void) bfinval(dev, 0);

	/* options */
	ufsvfsp->vfs_nosetsec = flags & UFSMNT_NOSETSEC;
	ufsvfsp->vfs_nointr  = flags & UFSMNT_NOINTR;
	ufsvfsp->vfs_syncdir = flags & UFSMNT_SYNCDIR;
	ufsvfsp->vfs_noatime = flags & UFSMNT_NOATIME;
	if ((flags & UFSMNT_NODFRATIME) || ufsvfsp->vfs_noatime)
		ufsvfsp->vfs_dfritime &= ~UFS_DFRATIME;
	else	/* dfratime, default behavior */
		ufsvfsp->vfs_dfritime |= UFS_DFRATIME;
	if (flags & UFSMNT_FORCEDIRECTIO)
		ufsvfsp->vfs_forcedirectio = 1;
	else if (flags & UFSMNT_NOFORCEDIRECTIO)
		ufsvfsp->vfs_forcedirectio = 0;
	ufsvfsp->vfs_iotstamp = lbolt;

	ufsvfsp->vfs_nindiroffset = fsp->fs_nindir - 1;
	ufsvfsp->vfs_nindirshift = highbit(ufsvfsp->vfs_nindiroffset);
	ufsvfsp->vfs_rdclustsz = fsp->fs_bsize * fsp->fs_maxcontig;
	if (ufsvfsp->vfs_rdclustsz < maxphys)
		if (maxphys < ufs_maxmaxphys)
			ufsvfsp->vfs_rdclustsz = maxphys;
		else
			ufsvfsp->vfs_rdclustsz = ufs_maxmaxphys;
	ufsvfsp->vfs_wrclustsz = ufsvfsp->vfs_rdclustsz;
	/*
	 * When logging, used to reserve log space for writes and truncs
	 */
	ufsvfsp->vfs_avgbfree = fsp->fs_cstotal.cs_nbfree / fsp->fs_ncg;

	if (TRANS_ISTRANS(ufsvfsp)) {
		/*
		 * start the delete thread
		 */
		ufs_thread_start(&ufsvfsp->vfs_delete, ufs_thread_delete, vfsp);

		/*
		 * start reclaim thread
		 */
		if (fsp->fs_reclaim & (FS_RECLAIM|FS_RECLAIMING)) {
			fsp->fs_reclaim &= ~FS_RECLAIM;
			fsp->fs_reclaim |=  FS_RECLAIMING;
			ufs_thread_start(&ufsvfsp->vfs_reclaim,
			    ufs_thread_reclaim, vfsp);
		}
	}
	if (!fsp->fs_ronly) {
		TRANS_SBWRITE(ufsvfsp, TOP_MOUNT);
		if (error = geterror(ufsvfsp->vfs_bufp))
			goto out;
	}

	/* fix-on-panic initialization */
	if (isroot && !(flags & UFSMNT_ONERROR_FLGMASK))
		flags |= UFSMNT_ONERROR_PANIC;	/* XXX ..._RDONLY */

	if ((error = ufsfx_mount(ufsvfsp, flags)) != 0)
		goto out;

	if (why == ROOT_INIT || why == ROOT_BACKMOUNT ||
	    why == ROOT_FRONTMOUNT) {
		if (isroot)
			rootvp = devvp;
	}

	return (0);
out:
	if (error == 0)
		error = EIO;
	if (rvp) {
		/* the following sequence is similar to ufs_unmount() */
		remque(rip);
		ufs_rmidle(rip);
		ufs_si_del(rip);
		rip->i_ufsvfs = NULL;
		rvp->v_vfsp = NULL;
		rvp->v_type = VBAD;
		VN_RELE(rvp);
	}
	if (bp) {
		bp->b_flags |= (B_STALE|B_AGE);
		brelse(bp);
	}
	if (tp) {
		tp->b_flags |= (B_STALE|B_AGE);
		brelse(tp);
	}
	if (needtrans) {
		TRANS_MATA_UMOUNT(ufsvfsp);
	}
	if (ufsvfsp) {
		ufs_vfs_remove(ufsvfsp);
		ufs_thread_exit(&ufsvfsp->vfs_delete);
		ufs_thread_exit(&ufsvfsp->vfs_reclaim);
		mutex_destroy(&ufsvfsp->vfs_lock);
		mutex_destroy(&ufsvfsp->vfs_rename_lock);
		if (ufsvfsp->vfs_log) {
			LUFS_UNSNARF(ufsvfsp);
		}
		kmem_free(ufsvfsp, sizeof (struct ufsvfs));
	}
	if (needclose) {
		(void) VOP_CLOSE(devvp, (vfsp->vfs_flag & VFS_RDONLY) ?
		    FREAD : FREAD|FWRITE, 1, (offset_t)0, cr);
		bflush(dev);
		(void) bfinval(dev, 1);
	}
	VN_RELE(devvp);
	return (error);
}

/*
 * vfs operations
 */
static int
ufs_unmount(struct vfs *vfsp, int fflag, struct cred *cr)
{
	dev_t 		dev		= vfsp->vfs_dev;
	struct ufsvfs	*ufsvfsp	= (struct ufsvfs *)vfsp->vfs_data;
	struct fs	*fs		= ufsvfsp->vfs_fs;
	struct ulockfs	*ulp		= &ufsvfsp->vfs_ulockfs;
	struct vnode 	*bvp, *vp;
	struct buf	*bp;
	struct inode	*ip, *inext, *rip;
	union ihead	*ih;
	int 		error, flag, i;
	intptr_t	istrans;
	extern vfs_t    EIO_vfs;
	struct lockfs	lockfs;
	int		poll_events = POLLPRI;
	extern struct pollhead ufs_pollhd;

	ASSERT(vfs_lock_held(vfsp));

	if (!suser(cr))
		return (EPERM);
	/*
	 * Forced unmount is now supported through the
	 * lockfs protocol.
	 */
	if (fflag & MS_FORCE) {
		ufs_thread_suspend(&ufsvfsp->vfs_delete);
		mutex_enter(&ulp->ul_lock);
		/*
		 * If file system is already hard locked,
		 * unmount the file system, otherwise
		 * hard lock it before unmounting.
		 */
		if (!ULOCKFS_IS_HLOCK(ulp)) {
			lockfs.lf_lock = LOCKFS_HLOCK;
			lockfs.lf_flags = 0;
			lockfs.lf_key = ulp->ul_lockfs.lf_key + 1;
			lockfs.lf_comlen = 0;
			lockfs.lf_comment = NULL;
			ufs_freeze(ulp, &lockfs);
			ULOCKFS_SET_BUSY(ulp);
			LOCKFS_SET_BUSY(&ulp->ul_lockfs);
			(void) ufs_quiesce(ulp);
			(void) ufs_flush(vfsp);
			(void) ufs_thaw(vfsp, ufsvfsp, ulp);
			ULOCKFS_CLR_BUSY(ulp);
			LOCKFS_CLR_BUSY(&ulp->ul_lockfs);
			poll_events |= POLLERR;
			pollwakeup(&ufs_pollhd, poll_events);
			ufs_thread_continue(&ufsvfsp->vfs_delete);

		}
		mutex_exit(&ulp->ul_lock);
	}

	/* let all types of writes go through */
	ufsvfsp->vfs_iotstamp = lbolt;

	/* coordinate with global hlock thread */
	istrans = (intptr_t)TRANS_ISTRANS(ufsvfsp);
	if (istrans && ufs_trans_put(dev))
		return (EAGAIN);

	/* kill the reclaim thread */
	ufs_thread_exit(&ufsvfsp->vfs_reclaim);

	/* suspend the delete thread */
	ufs_thread_suspend(&ufsvfsp->vfs_delete);

	/*
	 * drain the delete and idle queues
	 */
	ufs_delete_drain(vfsp, 0, 1);
	ufs_idle_drain(vfsp);

	/*
	 * use the lockfs protocol to prevent new ops from starting
	 */
	mutex_enter(&ulp->ul_lock);

	/*
	 * if the file system is busy; return EBUSY
	 */
	if (ulp->ul_vnops_cnt || ULOCKFS_IS_SLOCK(ulp)) {
		error = EBUSY;
		goto out;
	}

	/*
	 * if this is not a forced unmount (!hard/error locked), then
	 * get rid of every inode except the root and quota inodes
	 * also, commit any outstanding transactions
	 */
	if (!ULOCKFS_IS_HLOCK(ulp) && !ULOCKFS_IS_ELOCK(ulp))
		if (error = ufs_flush(vfsp))
			goto out;

	/*
	 * ignore inodes in the cache if fs is hard locked or error locked
	 */
	rip = VTOI(ufsvfsp->vfs_root);
	if (!ULOCKFS_IS_HLOCK(ulp) && !ULOCKFS_IS_ELOCK(ulp)) {
		/*
		 * Otherwise, only the quota and root inodes are in the cache
		 */
		for (i = 0, ih = ihead; i < inohsz; i++, ih++) {
			mutex_enter(&ih_lock[i]);
			for (ip = ih->ih_chain[0];
					ip != (struct inode *)ih;
					ip = ip->i_forw) {
				if (ip->i_ufsvfs != ufsvfsp)
					continue;
				if (ip == ufsvfsp->vfs_qinod)
					continue;
				if (ip == rip && ITOV(ip)->v_count == 1)
					continue;
				mutex_exit(&ih_lock[i]);
				error = EBUSY;
				goto out;
			}
			mutex_exit(&ih_lock[i]);
		}
	}
	/*
	 * Close the quota file and invalidate anything left in the quota
	 * cache for this file system.
	 */
	(void) closedq(ufsvfsp, cr);
	invalidatedq(ufsvfsp);
	/*
	 * drain the delete and idle queues
	 */
	ufs_delete_drain(vfsp, 0, 0);
	ufs_idle_drain(vfsp);

	/*
	 * discard the inodes for this fs (including root, shadow, and quota)
	 */
	for (i = 0, ih = ihead; i < inohsz; i++, ih++) {
		mutex_enter(&ih_lock[i]);
		for (inext = 0, ip = ih->ih_chain[0];
				ip != (struct inode *)ih;
				ip = inext) {
			inext = ip->i_forw;
			if (ip->i_ufsvfs != ufsvfsp)
				continue;
			vp = ITOV(ip);
			VN_HOLD(vp)
			remque(ip);
			ufs_rmidle(ip);
			ufs_si_del(ip);
			/*
			 * rip->i_ufsvfsp is needed by bflush()
			 */
			if (ip != rip)
				ip->i_ufsvfs = NULL;
			/*
			 * Set vnode's vfsops to dummy ops, which return
			 * EIO. This is needed to forced unmounts to work
			 * with lofs properly. See bug id 1203132.
			 */
			if (ULOCKFS_IS_HLOCK(ulp))
				vp->v_vfsp = &EIO_vfs;
			else
				vp->v_vfsp = NULL;
			vp->v_type = VBAD;
			VN_RELE(vp);
		}
		mutex_exit(&ih_lock[i]);
	}
	ufs_si_cache_flush(dev);

	/*
	 * kill the delete thread and drain the idle queue
	 */
	ufs_thread_exit(&ufsvfsp->vfs_delete);
	ufs_idle_drain(vfsp);

	bp = ufsvfsp->vfs_bufp;
	bvp = ufsvfsp->vfs_devvp;
	flag = !fs->fs_ronly;
	if (flag) {
		bflush(dev);
		if (fs->fs_clean != FSBAD) {
			if (fs->fs_clean == FSSTABLE)
				fs->fs_clean = FSCLEAN;
			fs->fs_reclaim &= ~FS_RECLAIM;
		}
		TRANS_SBUPDATE(ufsvfsp, vfsp, TOP_SBUPDATE_UNMOUNT);
		/*
		 * push this last transaction
		 */
		curthread->t_flag |= T_DONTBLOCK;
		TRANS_BEGIN_SYNC(ufsvfsp, TOP_COMMIT_UNMOUNT, TOP_COMMIT_SIZE);
		TRANS_END_SYNC(ufsvfsp, error, TOP_COMMIT_UNMOUNT,
				TOP_COMMIT_SIZE);
		curthread->t_flag &= ~T_DONTBLOCK;
	}

	TRANS_MATA_UMOUNT(ufsvfsp);
	LUFS_UNSNARF(ufsvfsp);		/* Release the in-memory structs */
	ufsfx_unmount(ufsvfsp);		/* fix-on-panic bookkeeping */
	kmem_free(fs->fs_u.fs_csp, fs->fs_cssize);

	bp->b_flags |= B_STALE|B_AGE;
	ufsvfsp->vfs_bufp = NULL;	/* don't point at free'd buf */
	brelse(bp);			/* free the superblock buf */

	(void) VOP_PUTPAGE(common_specvp(bvp), (offset_t)0, (size_t)0,
	    B_INVAL, cr);
	(void) VOP_CLOSE(bvp, flag, 1, (offset_t)0, cr);
	bflush(dev);
	(void) bfinval(dev, 1);
	VN_RELE(bvp);

	/*
	 * It is now safe to NULL out the ufsvfs pointer and discard
	 * the root inode.
	 */
	rip->i_ufsvfs = NULL;
	VN_RELE(ITOV(rip));

	/* free up lockfs comment structure, if any */
	if (ulp->ul_lockfs.lf_comlen && ulp->ul_lockfs.lf_comment)
		kmem_free(ulp->ul_lockfs.lf_comment, ulp->ul_lockfs.lf_comlen);

	/*
	 * Remove from instance list.
	 */
	ufs_vfs_remove(ufsvfsp);

	/*
	 * For a forcible unmount, threads may be asleep in
	 * ufs_lockfs_begin/ufs_check_lockfs.  These threads will need
	 * the ufsvfs structure so we don't free it, yet.  ufs_update
	 * will free it up after awhile.
	 */
	if (ULOCKFS_IS_HLOCK(ulp) || ULOCKFS_IS_ELOCK(ulp)) {
		extern kmutex_t		ufsvfs_mutex;
		extern struct ufsvfs	*ufsvfslist;

		mutex_enter(&ufsvfs_mutex);
		ufsvfsp->vfs_dontblock = 1;
		ufsvfsp->vfs_next = ufsvfslist;
		ufsvfslist = ufsvfsp;
		mutex_exit(&ufsvfs_mutex);
		/* wakeup any suspended threads */
		cv_broadcast(&ulp->ul_cv);
		mutex_exit(&ulp->ul_lock);
	} else {
		mutex_destroy(&ufsvfsp->vfs_lock);
		mutex_destroy(&ufsvfsp->vfs_rename_lock);
		kmem_free(ufsvfsp, sizeof (struct ufsvfs));
	}

	return (0);
out:

	/* open the fs to new ops */
	cv_broadcast(&ulp->ul_cv);
	mutex_exit(&ulp->ul_lock);

	if (istrans) {
		/* allow the delete thread to continue */
		ufs_thread_continue(&ufsvfsp->vfs_delete);
		/* restart the reclaim thread */
		ufs_thread_start(&ufsvfsp->vfs_reclaim, ufs_thread_reclaim,
				vfsp);
		/* coordinate with global hlock thread */
		(void) ufs_trans_get(dev, vfsp);
		/* check for trans errors during umount */
		ufs_trans_onerror();
	}

	return (error);
}

static int
ufs_root(vfsp, vpp)
	struct vfs *vfsp;
	struct vnode **vpp;
{
	struct ufsvfs *ufsvfsp;
	struct vnode *vp;

	if (!vfsp)
		return (EIO);

	ufsvfsp = (struct ufsvfs *)vfsp->vfs_data;
	if (!ufsvfsp || !ufsvfsp->vfs_root)
		return (EIO);	/* forced unmount */

	vp = ufsvfsp->vfs_root;
	VN_HOLD(vp);
	*vpp = vp;
	return (0);
}

/*
 * Get file system statistics.
 */
static int
ufs_statvfs(struct vfs *vfsp, struct statvfs64 *sp)
{
	struct fs *fsp;
	struct ufsvfs *ufsvfsp;
	int blk, i;
	long max_avail, used;
	dev32_t d32;

	ufsvfsp = (struct ufsvfs *)vfsp->vfs_data;
	fsp = ufsvfsp->vfs_fs;
	if (fsp->fs_magic != FS_MAGIC)
		return (EINVAL);
	(void) bzero(sp, sizeof (*sp));
	sp->f_bsize = fsp->fs_bsize;
	sp->f_frsize = fsp->fs_fsize;
	sp->f_blocks = (fsblkcnt64_t)fsp->fs_dsize;
	sp->f_bfree = (fsblkcnt64_t)fsp->fs_cstotal.cs_nbfree * fsp->fs_frag +
	    fsp->fs_cstotal.cs_nffree;
	/*
	 * avail = MAX(max_avail - used, 0)
	 */
	max_avail = fsp->fs_dsize - ufsvfsp->vfs_minfrags;

	used = (fsp->fs_dsize - sp->f_bfree);


	if (max_avail > used)
		sp->f_bavail = (fsblkcnt64_t)max_avail - used;
	else
		sp->f_bavail = (fsblkcnt64_t)0;
	/*
	 * inodes
	 */
	sp->f_files =  (fsfilcnt64_t)fsp->fs_ncg * fsp->fs_ipg;
	sp->f_ffree = sp->f_favail = (fsfilcnt64_t)fsp->fs_cstotal.cs_nifree;
	(void) cmpldev(&d32, vfsp->vfs_dev);
	sp->f_fsid = d32;
	(void) strcpy(sp->f_basetype, vfssw[vfsp->vfs_fstype].vsw_name);
	sp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sp->f_namemax = MAXNAMLEN;
	if (fsp->fs_cpc == 0) {
		bzero(sp->f_fstr, 14);
		return (0);
	}
	blk = fsp->fs_spc * fsp->fs_cpc / NSPF(fsp);
	for (i = 0; i < blk; i += fsp->fs_frag) /* CSTYLED */
		/* void */;
	i -= fsp->fs_frag;
	blk = i / fsp->fs_frag;
/*	bcopy(&(fsp->fs_rotbl[blk]), sp->f_fstr, 14);	*/
	bcopy(&(fs_rotbl(fsp)[blk]), sp->f_fstr, 14);
	return (0);
}

/*
 * Flush any pending I/O to file system vfsp.
 * The ufs_update() routine will only flush *all* ufs files.
 * If vfsp is non-NULL, only sync this ufs (in preparation
 * for a umount).
 */
/*ARGSUSED*/
static int
ufs_sync(struct vfs *vfsp, short flag, struct cred *cr)
{
	extern struct vfsops ufs_vfsops;
	struct ufsvfs *ufsvfsp;
	struct fs *fs;
	int cheap = flag & SYNC_ATTR;
	int error;

	/*
	 * SYNC_CLOSE means we're rebooting.  Toss everything
	 * on the idle queue so we don't have to slog through
	 * a bunch of uninteresting inodes over and over again.
	 */
	if (flag & SYNC_CLOSE)
		ufs_idle_drain(NULL);

	if (vfsp == NULL) {
		ufs_update(flag);
		return (0);
	}

	/* Flush a single ufs */
	if (vfsp->vfs_op != &ufs_vfsops || vfs_lock(vfsp) != 0)
		return (0);

	ufsvfsp = (struct ufsvfs *)vfsp->vfs_data;
	if (!ufsvfsp)
		return (EIO);
	fs = ufsvfsp->vfs_fs;
	mutex_enter(&ufsvfsp->vfs_lock);

	if (ufsvfsp->vfs_dio &&
	    fs->fs_ronly == 0 &&
	    fs->fs_clean != FSBAD &&
	    fs->fs_clean != FSLOG) {
		/* turn off fast-io on unmount, so no fsck needed (4029401) */
		ufsvfsp->vfs_dio = 0;
		fs->fs_clean = FSACTIVE;
		fs->fs_fmod = 1;
	}

	/* Write back modified superblock */
	if (fs->fs_fmod == 0) {
		mutex_exit(&ufsvfsp->vfs_lock);
	} else {
		if (fs->fs_ronly != 0) {
			mutex_exit(&ufsvfsp->vfs_lock);
			vfs_unlock(vfsp);
			return (ufs_fault(ufsvfsp->vfs_root,
					    "fs = %s update: ro fs mod\n",
					    fs->fs_fsmnt));
		}
		fs->fs_fmod = 0;
		mutex_exit(&ufsvfsp->vfs_lock);

		TRANS_SBUPDATE(ufsvfsp, vfsp, TOP_SBUPDATE_UPDATE);
	}
	vfs_unlock(vfsp);

	(void) ufs_scan_inodes(1, ufs_sync_inode, (void *)cheap);
	bflush((dev_t)vfsp->vfs_dev);

	/*
	 * commit any outstanding async transactions
	 */
	curthread->t_flag |= T_DONTBLOCK;
	TRANS_BEGIN_SYNC(ufsvfsp, TOP_COMMIT_UPDATE, TOP_COMMIT_SIZE);
	TRANS_END_SYNC(ufsvfsp, error, TOP_COMMIT_UPDATE, TOP_COMMIT_SIZE);
	curthread->t_flag &= ~T_DONTBLOCK;

	return (0);
}


void
sbupdate(struct vfs *vfsp)
{
	struct ufsvfs *ufsvfsp = (struct ufsvfs *)vfsp->vfs_data;
	struct fs *fs = ufsvfsp->vfs_fs;
	struct buf *bp;
	int blks;
	caddr_t space;
	int i;
	size_t size;

	/*
	 * for ulockfs processing, limit the superblock writes
	 */
	if ((ufsvfsp->vfs_ulockfs.ul_sbowner) &&
	    (curthread != ufsvfsp->vfs_ulockfs.ul_sbowner)) {
		/* process later */
		fs->fs_fmod = 1;
		return;
	}
	ULOCKFS_SET_MOD((&ufsvfsp->vfs_ulockfs));

	if (TRANS_ISTRANS(ufsvfsp)) {
		mutex_enter(&ufsvfsp->vfs_lock);
		ufs_sbwrite(ufsvfsp);
		mutex_exit(&ufsvfsp->vfs_lock);
		return;
	}

	blks = howmany(fs->fs_cssize, fs->fs_fsize);
	space = (caddr_t)fs->fs_u.fs_csp;
	for (i = 0; i < blks; i += fs->fs_frag) {
		size = fs->fs_bsize;
		if (i + fs->fs_frag > blks)
			size = (blks - i) * fs->fs_fsize;
		bp = UFS_GETBLK(ufsvfsp, ufsvfsp->vfs_dev,
			(daddr_t)(fsbtodb(fs, fs->fs_csaddr + i)),
			fs->fs_bsize);
		bcopy(space, bp->b_un.b_addr, size);
		space += size;
		bp->b_bcount = size;
		UFS_BRWRITE(ufsvfsp, bp);
	}
	mutex_enter(&ufsvfsp->vfs_lock);
	ufs_sbwrite(ufsvfsp);
	mutex_exit(&ufsvfsp->vfs_lock);
}

int ufs_vget_idle_count = 2;	/* Number of inodes to idle each time */
static int
ufs_vget(vfsp, vpp, fidp)
	struct vfs *vfsp;
	struct vnode **vpp;
	struct fid *fidp;
{
	int error;
	struct ufid *ufid;
	struct inode *ip;
	struct ufsvfs *ufsvfsp = (struct ufsvfs *)vfsp->vfs_data;
	struct ulockfs *ulp;

	/*
	 * Keep the idle queue from getting too long by
	 * idling an inode before attempting to allocate another.
	 *    This operation must be performed before entering
	 *    lockfs or a transaction.
	 */
	if (ufs_idle_q.uq_ne > ufs_idle_q.uq_hiwat)
		if ((curthread->t_flag & T_DONTBLOCK) == 0) {
			ins.in_vidles.value.ul += ufs_vget_idle_count;
			ufs_idle_some(ufs_vget_idle_count);
		}
	if (ufsvfsp == NULL)
		goto out;

	ufid = (struct ufid *)fidp;

	if (ufs_lockfs_begin(ufsvfsp, &ulp, ULOCKFS_VGET_MASK))
		goto out;

	rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);

	error = ufs_iget(vfsp, ufid->ufid_ino, &ip, CRED());

	rw_exit(&ufsvfsp->vfs_dqrwlock);

	ufs_lockfs_end(ulp);
	if (error) {
		goto out;
	}

	if (ip->i_gen != ufid->ufid_gen || ip->i_mode == 0 ||
	    (ip->i_flag & IDEL)) {
		VN_RELE(ITOV(ip));
		*vpp = NULL;
		return (0);
	}
	*vpp = ITOV(ip);
	return (0);
out:
	*vpp = NULL;
	return (0);
}

/* ARGSUSED */
static int
ufs_swapvp(vfsp, vpp, nm)
	struct vfs *vfsp;
	struct vnode **vpp;
	char *nm;
{
	return (ENOSYS);
}

/*
 * ufs vfs operations.
 */
struct vfsops ufs_vfsops = {
	ufs_mount,
	ufs_unmount,
	ufs_root,
	ufs_statvfs,
	ufs_sync,
	ufs_vget,
	ufs_mountroot,
	ufs_swapvp,
	fs_freevfs
};
