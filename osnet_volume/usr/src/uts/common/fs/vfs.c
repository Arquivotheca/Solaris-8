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
 * 	(c) 1986,1987,1988,1989,1993  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vfs.c	1.93	99/12/03 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/user.h>
#include <sys/fstyp.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#include <sys/mntent.h>
#include <sys/statvfs.h>
#include <sys/statfs.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/dnlc.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/atomic.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>
#include <sys/swap.h>
#include <sys/debug.h>
#include <sys/vnode.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/pathname.h>
#include <sys/bootconf.h>
#include <sys/dumphdr.h>
#include <sys/dc_ki.h>
#include <sys/poll.h>
#include <sys/sunddi.h>

#include <vm/page.h>
#include <sys/rce.h>

static void vfs_freemnttab(struct vfs *);
static void vfs_freeopttbl(mntopts_t *);
static void vfs_mnttab_modtimeupd();

/*
 * VFS global data.
 */
vnode_t *rootdir;		/* pointer to root vnode. */

static struct vfs root;
struct vfs *rootvfs = &root;	/* pointer to root vfs; head of VFS list. */
struct vfs **rvfs_head;		/* array of vfs ptrs for vfs hash list */
kmutex_t *rvfs_lock;		/* array of locks for vfs hash list */
int vfshsz = 512;		/* # of heads/locks in vfs hash arrays */
				/* must be power of 2!	*/
timespec_t vfs_mnttab_ctime;	/* mnttab created time */
timespec_t vfs_mnttab_mtime;	/* mnttab last modified time */
char *vfs_dummyfstype = "\0";
struct pollhead vfs_pollhd;	/* for mnttab pollers */

/*
 * Table for generic options recognized in the VFS layer and acted
 * on at this level before parsing file system specific options.
 */
/*
 * VFS Mount options table
 */
static char *ro_cancel[] = { MNTOPT_RW, NULL };
static char *rw_cancel[] = { MNTOPT_RO, NULL };
static char *suid_cancel[] = { MNTOPT_NOSUID, NULL };
static char *nosuid_cancel[] = { MNTOPT_SUID, NULL };

static const mntopt_t mntopts[] = {
/*
 *	option name		cancel options		default arg	flags
 */
	{ MNTOPT_REMOUNT,	NULL,			NULL,		0,
		(void *)0 },
	{ MNTOPT_RO,		ro_cancel,		NULL,		0,
		(void *)0 },
	{ MNTOPT_RW,		rw_cancel,		NULL,		0,
		(void *)0 },
	{ MNTOPT_SUID,		suid_cancel,		NULL,		0,
		(void *)0 },
	{ MNTOPT_NOSUID,	nosuid_cancel,		NULL,		0,
		(void *)0 },
};

static mntopts_t vfs_mntopts = {
	sizeof (mntopts) / sizeof (mntopt_t),
	(mntopt_t *)&mntopts[0]
};

/*
 * VFS system calls: mount, umount, syssync, statfs, fstatfs, statvfs,
 * fstatvfs, and sysfs moved to common/syscall.
 */

/*
 * Update every mounted file system.  We call the vfs_sync operation of
 * each file system type, passing it a NULL vfsp to indicate that all
 * mounted file systems of that type should be updated.
 */
void
vfs_sync(int flag)
{
	int i;

	/*
	 * SRM hook: for flush of cached SRM accounting data when a vfs_sync
	 * operation is issued.
	 * vfs_sync() is called to perform a general flush (vfsops->vfs_sync)
	 * on all vfs types, for example due to a sync() system call.
	 * Calling SRM_FLUSH(SH_NOW) before the loop means any recently changed
	 * SRM data will be promptly flushed to a disk thus minimizing any
	 * potential loss of the accounting information.
	 */
	SRM_FLUSH(SH_NOW);

	for (i = 1; i < nfstype; i++) {
		RLOCK_VFSSW();
		if (vfssw[i].vsw_vfsops) {
		    (void) (*vfssw[i].vsw_vfsops->vfs_sync)(NULL, flag, CRED());
		}
		RUNLOCK_VFSSW();
	}
}

void
sync(void)
{
	vfs_sync(0);
}

/*
 * External routines.
 */

krwlock_t vfssw_lock;	/* lock accesses to vfssw */

/*
 * Lock for accessing the vfs linked list.  Initialized in vfs_mountroot(),
 * but otherwise should be accessed only via vfs_list_lock() and
 * vfs_list_unlock().  Also used to protect the timestamp for mods to the list.
 */
static kmutex_t vfslist;

/*
 * vfs_mountroot is called by main() to mount the root filesystem.
 */
void
vfs_mountroot(void)
{

	rw_init(&vfssw_lock, NULL, RW_DEFAULT, NULL);

	/*
	 * Alloc the vfs hash bucket array and locks
	 */
	rvfs_head = kmem_zalloc(vfshsz * sizeof (struct vfs *), KM_SLEEP);
	rvfs_lock = kmem_zalloc(vfshsz * sizeof (kmutex_t), KM_SLEEP);

	/*
	 * Call machine-dependent routine "rootconf" to choose a root
	 * file system type.
	 */
	if (rootconf())
		cmn_err(CE_PANIC, "vfs_mountroot: cannot mount root");
	/*
	 * Get vnode for '/'.  Set up rootdir, u.u_rdir and u.u_cdir
	 * to point to it.  These are used by lookuppn() so that it
	 * knows where to start from ('/' or '.').
	 */
	if (VFS_ROOT(rootvfs, &rootdir))
		cmn_err(CE_PANIC, "vfs_mountroot: no root vnode");
	u.u_cdir = rootdir;
	VN_HOLD(u.u_cdir);
	u.u_rdir = NULL;
	/*
	 * Notify the module code that it can begin using the
	 * root filesystem instead of the boot program's services.
	 */
	modrootloaded = 1;
	/*
	 * Set up mnttab information for root
	 */
	vfs_setresource(rootvfs, rootfs.bo_name);
	vfs_setmntpoint(rootvfs, "/");

	/*
	 * Notify cluster software that the root filesystem is available.
	 */
	clboot_mountroot();
}

/*
 * Common mount code.  Called from the system call entry point, from autofs,
 * and from pxfs.
 *
 * Takes the effective file system type, mount arguments, the mount point
 * vnode, flags specifying whether the mount is a remount and whether it
 * should be entered into the vfs list, and credentials.  Fills in its vfspp
 * parameter with the mounted file system instance's vfs.
 *
 * Note that the effective file system type is specified as a string.  It may
 * be null, in which case it's determined from the mount arguments, and may
 * differ from the type specified in the mount arguments; this is a hook to
 * allow interposition when instantiating file system instances.
 *
 * The caller is responsible for releasing its own hold on the mount point
 * vp (this routine does its own hold when necessary).
 * Also note that for remounts, the mount point vp should be the vnode for
 * the root of the file system rather than the vnode that the file system
 * is mounted on top of.
 */
int
domount(char *fsname, struct mounta *uap, vnode_t *vp, struct cred *credp,
	struct vfs **vfspp)
{
	struct vfssw	*vswp = NULL;
	struct vfsops	*vfsops;
	struct vfs	*vfsp;
	struct pathname	pn;
	mntopts_t	global_mntopts, old_mntopts;
	int		error = 0;
	int		ovflags;
	char		*opts = uap->optptr;
	char		*inargs = opts;
	int		optlen = uap->optlen;
	int		remount;
	int		rdonly;
	int		hasproto = 0;
	int		splice = ((uap->flags & MS_NOSPLICE) == 0);
	int		fromspace = (uap->flags & MS_SYSSPACE) ?
				UIO_SYSSPACE : UIO_USERSPACE;


	global_mntopts.mo_count = 0;
	old_mntopts.mo_count = 0;
	/*
	 * Find the ops vector to use to invoke the file system-specific mount
	 * method.  If the fsname argument is non-NULL, use it directly.
	 * Otherwise, dig the file system type information out of the mount
	 * arguments.
	 *
	 * A side effect is to lock the vfssw.
	 *
	 * Mount arguments can be specified in several ways, which are
	 * distinguished by flag bit settings.  The preferred way is to set
	 * MS_OPTIONSTR, indicating an 8 argument mount with the file system
	 * type supplied as a character string and the last two arguments
	 * being a pointer to a character buffer and the size of the buffer.
	 * On entry, the buffer holds a null terminated list of options; on
	 * return, the string is the list of options the file system
	 * recognized. If MS_DATA is set arguments five and six point to a
	 * block of binary data which the file system interprets.
	 * A further wrinkle is that some callers don't set MS_FSS and MS_DATA
	 * consistently with these conventions.  To handle them, we check to
	 * see whether the pointer to the file system name has a numeric value
	 * less than 256.  If so, we treat it as an index.
	 */
	if (fsname != NULL) {
		if ((vswp = vfs_getvfssw(fsname)) == NULL) {
			if (splice)
				vn_vfsunlock(vp);
			return (EINVAL);
		} else {
			vfsops = vswp->vsw_vfsops;
		}
	} else if (uap->flags & (MS_OPTIONSTR | MS_DATA | MS_FSS)) {
		size_t n;
		uint_t fstype;
		char name[FSTYPSZ];

		if ((fstype = (uint_t)uap->fstype) < 256) {
			if (fstype == 0 || fstype >= nfstype ||
			    !ALLOCATED_VFSSW(&vfssw[fstype])) {
				return (EINVAL);
			}
			(void) strcpy(name, vfssw[fstype].vsw_name);
		} else {
			/*
			 * Handle either kernel or user address space.
			 */
			if (uap->flags & MS_SYSSPACE) {
				error = copystr(uap->fstype, name,
				    FSTYPSZ, &n);
			} else {
				error = copyinstr(uap->fstype, name,
				    FSTYPSZ, &n);
			}
			if (error) {
				if (error == ENAMETOOLONG)
					return (EINVAL);
				return (error);
			}
		}

		if ((vswp = vfs_getvfssw(name)) == NULL) {
			return (EINVAL);
		} else {
			vfsops = vswp->vsw_vfsops;
		}
	} else {
		vfsops = rootvfs->vfs_op;
		RLOCK_VFSSW();
	}
	/* vfssw was implicitly locked in vfs_getvfssw or explicitly here */
	ASSERT(VFSSW_LOCKED());
	vfs_copyopttbl(&vfs_mntopts, &global_mntopts);
	/*
	 * Fetch mount options and parse them for generic vfs options
	 */
	if (uap->flags & MS_OPTIONSTR) {
		/*
		 * Limit the buffer size
		 */
		if (optlen < 0 || optlen > MAX_MNTOPT_STR) {
			error = EINVAL;
			goto errout;
		}
		if ((uap->flags & MS_SYSSPACE) == 0) {
			inargs = kmem_alloc(MAX_MNTOPT_STR, KM_SLEEP);
			inargs[0] = '\0';
			if (optlen) {
				error = copyinstr(opts, inargs, (size_t)optlen,
					NULL);
				if (error) {
					goto errout;
				}
			}
		}
		vfs_parsemntopts(&global_mntopts, inargs, 0);
	}
	/*
	 * Flag bits override the options string.
	 */
	if (uap->flags & MS_REMOUNT)
		vfs_setmntopt(&global_mntopts, MNTOPT_REMOUNT, NULL, 0);
	if (uap->flags & MS_RDONLY)
		vfs_setmntopt(&global_mntopts, MNTOPT_RO, NULL, 0);
	if (uap->flags & MS_NOSUID)
		vfs_setmntopt(&global_mntopts, MNTOPT_NOSUID, NULL, 0);
	/*
	 * Check if this is a remount, must be set in the option string and
	 * the file system must support a remount option.
	 */
	hasproto = vswp != NULL && (vswp->vsw_flag & VSW_HASPROTO);
	remount = (uap->flags & MS_REMOUNT) ||
		(vfs_optionisset(&global_mntopts, MNTOPT_REMOUNT, NULL) &&
		hasproto &&
		vfs_hasopt(vswp->vsw_optproto, MNTOPT_REMOUNT) != NULL);
	rdonly = vfs_optionisset(&global_mntopts, MNTOPT_RO, NULL);
	ASSERT(splice || !remount);
	/*
	 * If we are splicing the fs into the namespace,
	 * perform mount point checks.
	 */
	if (splice) {
		ASSERT(vp->v_count > 0);

		/*
		 * Prevent path name resolution from proceeding past
		 * the mount point.
		 */
		if (vn_vfswlock(vp) != 0) {
			error = EBUSY;
			goto errout;
		}

		/*
		 * Verify that it's legitimate to establish a mount on
		 * the prospective mount point.
		 */
		if (vp->v_vfsmountedhere != NULL) {
			/*
			 * The mount point lock was obtained after some
			 * other thread raced through and established a mount.
			 */
			vn_vfsunlock(vp);
			error = EBUSY;
			goto errout;
		}
		if (vp->v_flag & VNOMOUNT) {
			vn_vfsunlock(vp);
			error = EINVAL;
			goto errout;
		}
	}
	if ((uap->flags & (MS_DATA | MS_OPTIONSTR)) == 0) {
		uap->dataptr = NULL;
		uap->datalen = 0;
	}

	/*
	 * If this is a remount, we don't want to create a new VFS.
	 * Instead, we pass the existing one with a remount flag.
	 */
	if (remount) {
		/*
		 * Confirm that the mount point is the root vnode of the
		 * file system that is being remounted.
		 * This can happen if the user specifies a different
		 * mount point directory pathname in the (re)mount command.
		 */
		if ((vp->v_flag & VROOT) == 0) {
			vn_vfsunlock(vp);
			error = ENOENT;
			goto errout;
		}
		/*
		 * Disallow making file systems read-only unless file system
		 * explicitly allows it in its vfssw.  Ignore other flags.
		 */
		if (rdonly && (vp->v_vfsp->vfs_flag & VFS_RDONLY) == 0 &&
			(vswp->vsw_flag & VSW_CANRWRO) == 0) {
			vn_vfsunlock(vp);
			error = EINVAL;
			goto errout;
		}
		vfsp = vp->v_vfsp;
		ovflags = vfsp->vfs_flag;
		vfsp->vfs_flag |= VFS_REMOUNT;
		vfsp->vfs_flag &= ~VFS_RDONLY;
	} else {
		vfsp = kmem_alloc(sizeof (vfs_t), KM_SLEEP);
		VFS_INIT(vfsp, vfsops, NULL);
	}

	VFS_HOLD(vfsp);
	/*
	 * Lock the vfs.
	 */
	if (error = vfs_lock(vfsp)) {
		if (splice)
			vn_vfsunlock(vp);
		if (!remount)
			kmem_free(vfsp, sizeof (struct vfs));
		else
			VFS_RELE(vfsp);
		goto errout;
	}

	/*
	 * Invalidate cached entry for the mount point.
	 */
	if (splice)
		dnlc_purge_vp(vp);

	/*
	 * Save away old options if it's a remount
	 */
	if (remount)
		vfs_copyopttbl(&vfsp->vfs_mntopts, &old_mntopts);
	/*
	 * If have an option string parse the options. If the file system
	 * supplies a prototype options table, use it to parse the option
	 * string, otherwise create a blank table and accept all the options
	 * in the string.
	 */
	if (hasproto)
		vfs_copyopttbl(vswp->vsw_optproto, &vfsp->vfs_mntopts);
	if (uap->flags & MS_OPTIONSTR) {
		if (hasproto) {
			vfs_parsemntopts(&vfsp->vfs_mntopts, inargs, 0);
		} else {
			vfs_createopttbl(&vfsp->vfs_mntopts, inargs);
			vfs_parsemntopts(&vfsp->vfs_mntopts, inargs, 1);
		}
	}
	/*
	 * Instantiate (or reinstantiate) the file system.  If appropriate,
	 * splice it into the file system name space.
	 */
	error = VFS_MOUNT(vfsp, vp, uap, credp);
	if (uap->flags & MS_RDONLY)
		vfs_setmntopt(&vfsp->vfs_mntopts, MNTOPT_RO, NULL, 0);
	if (uap->flags & MS_NOSUID)
		vfs_setmntopt(&vfsp->vfs_mntopts, MNTOPT_NOSUID, NULL, 0);

	if (error) {
		if (remount) {
			vfs_copyopttbl(&old_mntopts, &vfsp->vfs_mntopts);
			vfs_freeopttbl(&old_mntopts);
			vfsp->vfs_flag = ovflags;
			vfs_unlock(vfsp);
			VFS_RELE(vfsp);
		} else {
			vfs_unlock(vfsp);
			vfs_freemnttab(vfsp);
			kmem_free(vfsp, sizeof (struct vfs));
		}
	} else {
		/*
		 * Pick up mount point and device from appropriate space.
		 */
		if (pn_get(uap->spec, fromspace, &pn) == 0) {
			vfs_setresource(vfsp, pn.pn_path);
			pn_free(&pn);
		}
		if (vfsp->vfs_resource == NULL || *vfsp->vfs_resource == '\0')
			vfs_setresource(vfsp, VFS_NORESOURCE);
		if (pn_get(uap->dir, fromspace, &pn) == 0) {
			vfs_setmntpoint(vfsp, pn.pn_path);
			pn_free(&pn);
		}
		if (vfsp->vfs_mntpt == NULL || *vfsp->vfs_mntpt == '\0')
			vfs_setmntpoint(vfsp, VFS_NOMNTPT);
		/*
		 * Set the mount time to now
		 */
		vfsp->vfs_mtime = ddi_get_time();
		if (remount) {
			vfsp->vfs_flag &= ~VFS_REMOUNT;
			vfs_freeopttbl(&old_mntopts);
		} else if (splice) {
			/*
			 * Link vfsp into the name space at the mount
			 * point. Vfs_add() is responsible for
			 * holding the mount point which will be
			 * released when vfs_remove() is called.
			 */
			vfs_add(vp, vfsp, uap->flags);
			vp->v_vfsp->vfs_nsubmounts++;
		} else {
			/*
			 * Hold the reference to file system which is
			 * not linked into the name space.
			 */
			VFS_HOLD(vfsp);
			vfsp->vfs_vnodecovered = NULL;
		}
		/*
		 * Set flags for global options encountered
		 */
		if (rdonly)
			vfsp->vfs_flag |= VFS_RDONLY;
		else
			vfsp->vfs_flag &= ~VFS_RDONLY;
		if (vfs_optionisset(&global_mntopts, MNTOPT_NOSUID, NULL))
			vfsp->vfs_flag |= VFS_NOSUID;
		else
			vfsp->vfs_flag &= ~VFS_NOSUID;
		/*
		 * Now construct the output option string of options
		 * we recognized.
		 */
		if (uap->flags & MS_OPTIONSTR) {
			if ((error = vfs_buildoptionstr(&vfsp->vfs_mntopts,
				inargs, optlen)) != 0) {
				vfs_unlock(vfsp);
				goto errout;
			}
			if ((uap->flags & MS_SYSSPACE) == 0)
				error = copyoutstr(inargs, opts, optlen, NULL);
		}
		vfs_unlock(vfsp);
	}
	if (splice)
		vn_vfsunlock(vp);

	/*
	 * Return vfsp to caller.
	 */
	if (error == 0) {
		*vfspp = vfsp;
	}
errout:
	RUNLOCK_VFSSW();
	if (inargs != opts)
		kmem_free(inargs, MAX_MNTOPT_STR);
	vfs_freeopttbl(&global_mntopts);
	return (error);
}

/*
 * Record a mounted resource name in a vfs structure
 */
void
vfs_setresource(struct vfs *vfsp, const char *resource)
{
	char *sp;

	sp = kmem_alloc(strlen(resource) + 1, KM_SLEEP);
	(void) strcpy(sp, resource);
	if (vfsp->vfs_resource != NULL)
		kmem_free(vfsp->vfs_resource, strlen(vfsp->vfs_resource) + 1);
	vfsp->vfs_resource = sp;
}

/*
 * Record a mount point name in a vfs structure
 */
void
vfs_setmntpoint(struct vfs *vfsp, const char *mp)
{
	char *sp;

	sp = kmem_alloc(strlen(mp) + 1, KM_SLEEP);
	(void) strcpy(sp, mp);
	if (vfsp->vfs_mntpt != NULL)
		kmem_free(vfsp->vfs_mntpt, strlen(vfsp->vfs_mntpt) + 1);
	vfsp->vfs_mntpt = sp;
}

/*
 * Create an empty options table with enough empty slots to hold all
 * The options in the options string passed as an argument.
 */
void
vfs_createopttbl(mntopts_t *mops, const char *opts)
{
	const char *s = opts;
	int i, count;

	vfs_freeopttbl(mops);
	if (opts == NULL || *opts == '\0')
		return;
	count = 1;

	/*
	 * Count number of options in the string
	 */
	for (s = strchr(s, ','); s != NULL; s = strchr(s, ',')) {
		count++;
		s++;
	}
	mops->mo_count = count;
	mops->mo_list = kmem_zalloc(count * sizeof (mntopt_t), KM_SLEEP);
	for (i = 0; i < count; i++) {
		mops->mo_list[i].mo_flags = MO_EMPTY;
	}
}

/*
 * Copy a mount options table
 */
void
vfs_copyopttbl(mntopts_t *smo, mntopts_t *dmo)
{
	int i, j, count;
	mntopt_t *mop, *motbl;
	char *sp, *dp;

	/*
	 * Clear out any existing stuff in the options table being initialized
	 */
	vfs_freeopttbl(dmo);
	count = smo->mo_count;
	dmo->mo_count = count;
	motbl = kmem_alloc(count * sizeof (mntopt_t), KM_SLEEP);
	dmo->mo_list = motbl;
	for (i = 0; i < count; i++) {
		mop = &motbl[i];
		mop->mo_flags = smo->mo_list[i].mo_flags;
		mop->mo_data = smo->mo_list[i].mo_data;
		sp = smo->mo_list[i].mo_name;
		if (sp != NULL) {
			dp = kmem_alloc(strlen(sp) + 1, KM_SLEEP);
			(void) strcpy(dp, sp);
			mop->mo_name = dp;
		} else {
			mop->mo_name = NULL; /* should never happen */
		}
		if (smo->mo_list[i].mo_cancel != NULL) {
			for (j = 0; smo->mo_list[i].mo_cancel[j]; j++)
				/* count number of options to cancel */;
			mop->mo_cancel = kmem_alloc((j + 1) * sizeof (char *),
				KM_SLEEP);
			for (j = 0; (sp = smo->mo_list[i].mo_cancel[j]) != NULL;
				j++) {
				dp = kmem_alloc(strlen(sp) + 1, KM_SLEEP);
				(void) strcpy(dp, sp);
				mop->mo_cancel[j] = dp;
			}
			mop->mo_cancel[j] = NULL;
		} else {
			mop->mo_cancel = NULL;
		}
		sp = smo->mo_list[i].mo_arg;
		if (sp != NULL) {
			dp = kmem_alloc(strlen(sp) + 1, KM_SLEEP);
			(void) strcpy(dp, sp);
			mop->mo_arg = dp;
		} else {
			mop->mo_arg = NULL;
		}
	}
}

/*
 * Functions to set and clear mount options in a mount options table.
 */
void
vfs_clearmntopt(mntopts_t *mops, const char *opt)
{
	struct mntopt *mop;
	int i, count;

	count = mops->mo_count;
	for (i = 0; i < count; i++) {
		mop = &mops->mo_list[i];

		if (strcmp(opt, mop->mo_name))
			continue;
		vfs_list_lock();
		mop->mo_flags &= ~MO_SET;
		if (mop->mo_arg != NULL) {
			kmem_free(mop->mo_arg, strlen(mop->mo_arg) + 1);
		}
		mop->mo_arg = NULL;
		vfs_mnttab_modtimeupd();
		vfs_list_unlock();
		break;
	}
}

/*
 * Set a mount option on.  If it's not found in the table, it's silently
 * ignored.  If the option has MO_IGNORE set, it is still set unless the
 * VFS_NOFORCEOPT bit is set in the flags.  Also, VFS_DISPLAY/VFS_NODISPLAY flag
 * bits can be used to toggle the MO_NODISPLAY bit for the option.
 * If the VFS_CREATEOPT flag bit is set then the first option slot with
 * MO_EMPTY set is created as the option passed in.
 */
void
vfs_setmntopt(mntopts_t *mops, const char *opt, const char *arg, int flags)
{
	mntopt_t *mop;
	int i, count;
	char *sp;

	if (flags & VFS_CREATEOPT) {
		if (vfs_hasopt(mops, opt) != NULL) {
			flags &= ~VFS_CREATEOPT;
		}
	}
	count = mops->mo_count;
	for (i = 0; i < count; i++) {
		mop = &mops->mo_list[i];

		if (mop->mo_flags & MO_EMPTY) {
			if ((flags & VFS_CREATEOPT) == 0)
				continue;
			sp = kmem_alloc(strlen(opt) + 1, KM_SLEEP);
			(void) strcpy(sp, opt);
			mop->mo_name = sp;
			if (arg != NULL)
				mop->mo_flags = MO_HASVALUE;
			else
				mop->mo_flags = 0;
		} else if (strcmp(opt, mop->mo_name)) {
			continue;
		}
		if ((mop->mo_flags & MO_IGNORE) && (flags & VFS_NOFORCEOPT))
			break;
		if (arg != NULL && (mop->mo_flags & MO_HASVALUE) != 0) {
			sp = kmem_alloc(strlen(arg) + 1, KM_SLEEP);
			(void) strcpy(sp, arg);
		} else {
			sp = NULL;
		}
		vfs_list_lock();
		mop->mo_arg = sp;
		if (flags & VFS_DISPLAY)
			mop->mo_flags &= ~MO_NODISPLAY;
		if (flags & VFS_NODISPLAY)
			mop->mo_flags |= MO_NODISPLAY;
		mop->mo_flags |= MO_SET;
		vfs_mnttab_modtimeupd();
		vfs_list_unlock();
		if (mop->mo_cancel != NULL) {
			char **cp;

			for (cp = mop->mo_cancel; *cp != NULL; cp++)
				vfs_clearmntopt(mops, *cp);
		}
		break;
	}
}

/*
 * Add a "tag" option to a mounted file system's options list
 */
static mntopt_t *
vfs_addtag(mntopts_t *mops, const char *tag)
{
	int count;
	mntopt_t *mop, *motbl;

	count = mops->mo_count + 1;
	motbl = kmem_zalloc(count * sizeof (mntopt_t), KM_SLEEP);
	if (mops->mo_count) {
		size_t len = (count - 1) * sizeof (mntopt_t);

		bcopy(mops->mo_list, motbl, len);
		kmem_free(mops->mo_list, len);
	}
	mops->mo_count = count;
	mops->mo_list = motbl;
	mop = &motbl[count - 1];
	mop->mo_flags = MO_TAG;
	mop->mo_name = kmem_alloc(strlen(tag) + 1, KM_SLEEP);
	(void) strcpy(mop->mo_name, tag);
	return (mop);
}

/*
 * Set a tag in a mounted file system's option list
 */
int
vfs_settag(uint_t major, uint_t minor, const char *mntpt, const char *tag)
{
	vfs_t *vfsp;
	mntopts_t *mops;
	mntopt_t *mop;
	dev_t dev = makedevice(major, minor);

	/*
	 * Find the desired mounted file system
	 */
	vfs_list_lock();
	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next)
		if (vfsp->vfs_dev == dev && strcmp(mntpt, vfsp->vfs_mntpt) == 0)
			break;
	vfs_list_unlock();
	if (vfsp == NULL)
		return (EINVAL);
	mops = &vfsp->vfs_mntopts;
	/*
	 * Add tag if it doesn't already exist
	 */
	if ((mop = vfs_hasopt(mops, tag)) == NULL) {
		char *buf;
		int len;

		buf = kmem_alloc(MAX_MNTOPT_STR, KM_SLEEP);
		(void) vfs_buildoptionstr(mops, buf, MAX_MNTOPT_STR);
		len = strlen(buf);
		kmem_free(buf, MAX_MNTOPT_STR);
		if (len + strlen(tag) + 2 > MAX_MNTOPT_STR)
			return (ENAMETOOLONG);
		mop = vfs_addtag(mops, tag);
	}
	if ((mop->mo_flags & MO_TAG) == 0)
		return (EINVAL);
	vfs_setmntopt(mops, tag, NULL, 0);
	return (0);
}

/*
 * Clear a tag in a mounted file system's option list
 */
int
vfs_clrtag(uint_t major, uint_t minor, const char *mntpt, const char *tag)
{
	vfs_t *vfsp;
	mntopt_t *mop;
	dev_t dev = makedevice(major, minor);

	/*
	 * Find the desired mounted file system
	 */
	vfs_list_lock();
	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next)
		if (vfsp->vfs_dev == dev && strcmp(mntpt, vfsp->vfs_mntpt) == 0)
			break;
	vfs_list_unlock();
	if (vfsp == NULL)
		return (EINVAL);
	if ((mop = vfs_hasopt(&vfsp->vfs_mntopts, tag)) == NULL)
		return (EINVAL);
	if ((mop->mo_flags & MO_TAG) == 0)
		return (EINVAL);
	vfs_clearmntopt(&vfsp->vfs_mntopts, tag);
	return (0);
}

/*
 * Function to parse an option string and fill in a mount options table.
 * Unknown options are silently ignored.  The input option string is modified
 * by replacing separators with nulls.  If the create flag is set, options
 * not found in the table are just added on the fly.  The table must have
 * an option slot marked MO_EMPTY to add an option on the fly.
 */
void
vfs_parsemntopts(mntopts_t *mops, char *osp, int create)
{
	char *s = osp, *p, *nextop, *valp, *cp, *ep;
	int setflg = VFS_NOFORCEOPT;

	if (osp == NULL)
		return;
	while (*s != '\0') {
		p = strchr(s, ',');	/* find next option */
		if (p == NULL) {
			cp = NULL;
			p = s + strlen(s);
		} else {
			cp = p;		/* save location of comma */
			*p++ = '\0';	/* mark end and point to next option */
		}
		nextop = p;
		p = strchr(s, '=');	/* look for value */
		if (p == NULL) {
			valp = NULL;	/* no value supplied */
		} else {
			ep = p;		/* save location of equals */
			*p++ = '\0';	/* end option and point to value */
			valp = p;
		}
		/*
		 * set option into options table
		 */
		if (create)
			setflg |= VFS_CREATEOPT;
		vfs_setmntopt(mops, s, valp, setflg);
		if (cp != NULL)
			*cp = ',';	/* restore the comma */
		if (valp != NULL)
			*ep = '=';	/* restore the equals */
		s = nextop;
	}
}

/*
 * Function to inquire if an option exists in a mount options table.
 * Returns a pointer to the option if it exists, else NULL.
 */
struct mntopt *
vfs_hasopt(mntopts_t *mops, const char *opt)
{
	struct mntopt *mop;
	int i, count;

	count = mops->mo_count;
	for (i = 0; i < count; i++) {
		mop = &mops->mo_list[i];

		if (mop->mo_flags & MO_EMPTY)
			continue;
		if (strcmp(opt, mop->mo_name) == 0)
			return (mop);
	}
	return (NULL);
}

/*
 * Function to inquire if an option is set in a mount options table.
 * Returns non-zero if set and fills in the arg pointer with a pointer to
 * the argument string or NULL if there is no argument string.
 */
int
vfs_optionisset(mntopts_t *mops, const char *opt, char **argp)
{
	struct mntopt *mop;
	int i, count;

	count = mops->mo_count;
	for (i = 0; i < count; i++) {
		mop = &mops->mo_list[i];

		if (mop->mo_flags & MO_EMPTY)
			continue;
		if (strcmp(opt, mop->mo_name))
			continue;
		if ((mop->mo_flags & MO_SET) == 0)
			return (0);
		if (argp != NULL && (mop->mo_flags & MO_HASVALUE) != 0)
			*argp = mop->mo_arg;
		return (1);
	}
	return (0);
}

/*
 * Set filesystem private data field in a mount option
 */
int
vfs_setoptprivate(mntopts_t *mops, const char *opt, void *private)
{
	struct mntopt *mop;
	int i, count;

	count = mops->mo_count;
	for (i = 0; i < count; i++) {
		mop = &mops->mo_list[i];

		if (mop->mo_flags & MO_EMPTY)
			continue;
		if (strcmp(opt, mop->mo_name) == 0) {
			mop->mo_data = private;
			return (0);
		}
	}
	return (1);
}

/*
 * Get filesystem private data field in a mount option
 */
int
vfs_getoptprivate(mntopts_t *mops, const char *opt, void **private)
{
	struct mntopt *mop;
	int i, count;

	count = mops->mo_count;
	for (i = 0; i < count; i++) {
		mop = &mops->mo_list[i];

		if (mop->mo_flags & MO_EMPTY)
			continue;
		if (strcmp(opt, mop->mo_name) == 0) {
			*private = mop->mo_data;
			return (0);
		}
	}
	return (1);
}

/*
 * Construct a comma seperated string of the options set in the given
 * mount table, return the string in the given buffer.  Return non-zero if
 * the buffer would overflow.
 */
int
vfs_buildoptionstr(mntopts_t *mp, char *buf, int len)
{
	char *cp;
	int i;

	buf[0] = '\0';
	cp = buf;
	for (i = 0; i < mp->mo_count; i++) {
		struct mntopt *mop;

		mop = &mp->mo_list[i];
		if (mop->mo_flags & MO_SET) {
			int optlen, comma = 0;

			if (buf[0] != '\0')
				comma = 1;
			optlen = strlen(mop->mo_name);
			if (strlen(buf) + comma + optlen + 1 > len)
				goto err;
			if (comma)
				*cp++ = ',';
			(void) strcpy(cp, mop->mo_name);
			cp += optlen;
			/*
			 * Append option value if there is one
			 */
			if (mop->mo_arg != NULL) {
				int arglen;

				arglen = strlen(mop->mo_arg);
				if (strlen(buf) + arglen + 2 > len)
					goto err;
				*cp++ = '=';
				(void) strcpy(cp, mop->mo_arg);
				cp += arglen;
			}
		}
	}
	return (0);
err:
	return (EOVERFLOW);
}

/*
 * Free a mount options table
 */
static void
vfs_freeopttbl(mntopts_t *mp)
{
	struct mntopt *mop;
	int i, count;

	count = mp->mo_count;
	for (i = 0; i < count; i++) {
		mop = &(mp->mo_list[i]);
		if (mop->mo_name != NULL)
			kmem_free(mop->mo_name, strlen(mop->mo_name) + 1);
		if (mop->mo_cancel != NULL) {
			int ccnt = 0;
			char **cp;

			for (cp = mop->mo_cancel; *cp != NULL; cp++) {
				kmem_free(*cp, strlen(*cp) + 1);
				ccnt++;
			}
			kmem_free(mop->mo_cancel, (ccnt + 1) * sizeof (char *));
		}
		if (mop->mo_arg != NULL)
			kmem_free(mop->mo_arg, strlen(mop->mo_arg) + 1);
	}
	if (count) {
		kmem_free(mp->mo_list, sizeof (mntopt_t) * count);
		mp->mo_count = 0;
		mp->mo_list = NULL;
	}
}

/*
 * Free any mnttab information recorded in the vfs struct
 */
static void
vfs_freemnttab(struct vfs *vfsp)
{
	/*
	 * Free device and mount point information
	 */
	if (vfsp->vfs_mntpt != NULL) {
		kmem_free(vfsp->vfs_mntpt, strlen(vfsp->vfs_mntpt) + 1);
	}
	if (vfsp->vfs_resource != NULL) {
		kmem_free(vfsp->vfs_resource, strlen(vfsp->vfs_resource) + 1);
	}
	/*
	 * Now free mount options information
	 */
	vfs_freeopttbl(&vfsp->vfs_mntopts);
}

/*
 * Return the last mnttab modification time
 */
void
vfs_mnttab_modtime(timespec_t *ts)
{
	ASSERT(MUTEX_HELD(&vfslist));
	*ts = vfs_mnttab_mtime;
}

/*
 * See if mnttab is changed
 */
void
vfs_mnttab_poll(timespec_t *old, struct pollhead **phpp)
{
	int changed;

	*phpp = (struct pollhead *)NULL;
	vfs_list_lock();
	changed = old->tv_sec != vfs_mnttab_mtime.tv_sec ||
		old->tv_nsec != vfs_mnttab_mtime.tv_nsec;
	vfs_list_unlock();
	if (!changed) {
		*phpp = &vfs_pollhd;
	}
}

/*
 * Update the mnttab modification time and wake up any waiters for
 * mnttab changes
 */
static void
vfs_mnttab_modtimeupd()
{
	hrtime_t oldhrt, newhrt;

	ASSERT(MUTEX_HELD(&vfslist));
	oldhrt = ts2hrt(&vfs_mnttab_mtime);
	gethrestime(&vfs_mnttab_mtime);
	newhrt = ts2hrt(&vfs_mnttab_mtime);
	if (oldhrt == (hrtime_t)0)
		vfs_mnttab_ctime = vfs_mnttab_mtime;
	/*
	 * Attempt to provide unique mtime (like uniqtime but not).
	 */
	if (newhrt == oldhrt) {
		newhrt++;
		hrt2ts(newhrt, &vfs_mnttab_mtime);
	}
	pollwakeup(&vfs_pollhd, (short)POLLRDBAND);
}

int
dounmount(struct vfs *vfsp, int flag, cred_t *cr)
{
	vnode_t *coveredvp;
	int error;

	/*
	 * Get covered vnode. This will be NULL if the vfs is not linked
	 * into the file system name space (i.e., domount() with MNT_NOSPICE).
	 */
	coveredvp = vfsp->vfs_vnodecovered;
	ASSERT(coveredvp == NULL || vn_vfswlock_held(coveredvp));

	/*
	 * Purge all dnlc entries for this vfs.
	 */
	(void) dnlc_purge_vfsp(vfsp, 0);

	(void) VFS_SYNC(vfsp, 0, cr);

	/*
	 * Lock the vfs to maintain fs status quo during unmount.  This
	 * has to be done after the sync because ufs_update tries to acquire
	 * the vfs_reflock.
	 */
	vfs_lock_wait(vfsp);

	if (error = VFS_UNMOUNT(vfsp, flag, cr)) {
		vfs_unlock(vfsp);
		if (coveredvp != NULL)
			vn_vfsunlock(coveredvp);
	} else if (coveredvp != NULL) {
		--coveredvp->v_vfsp->vfs_nsubmounts;
		/*
		 * vfs_remove() will do a VN_RELE(vfsp->vfs_vnodecovered)
		 * when it frees vfsp so we do a VN_HOLD() so we can
		 * continue to use coveredvp afterwards.
		 */
		VN_HOLD(coveredvp);
		vfs_remove(vfsp);
		vn_vfsunlock(coveredvp);
		VN_RELE(coveredvp);
	} else {
		/*
		 * Release the reference to vfs that is not linked
		 * into the name space.
		 */
		/*
		 * Deallocate mnttab information
		 */
		vfs_freemnttab(vfsp);
		vfs_unlock(vfsp);
		VFS_RELE(vfsp);
	}
	return (error);
}


/*
 * Vfs_unmountall() is called by uadmin() to unmount all
 * mounted file systems (except the root file system) during shutdown.
 * It follows the existing locking protocol when traversing the vfs list
 * to sync and unmount vfses. Even though there should be no
 * other thread running while the system is shutting down, it is prudent
 * to still follow the locking protocol.
 */
void
vfs_unmountall(void)
{
	struct vfs *vfsp, *head_vfsp;
	int nvfs, i;
	struct vfs **unmount_list;

	/*
	 * Construct a list of vfses that we plan to unmount.
	 * Write lock the covered vnode to avoid the race condiiton
	 * caused by another unmount. Skip those vfses that we cannot
	 * lock.
	 */
	vfs_list_lock();

	for (vfsp = rootvfs->vfs_next, head_vfsp = NULL,
	    nvfs = 0; vfsp != NULL; vfsp = vfsp->vfs_next) {
		/*
		 * skip any vfs that we cannot acquire the vfslock()
		 */
		if (vfs_lock(vfsp) == 0) {
			if (vn_vfswlock(vfsp->vfs_vnodecovered) == 0) {

				/*
				 * put in the list of vfses to be unmounted
				 */
				vfsp->vfs_list = head_vfsp;
				head_vfsp = vfsp;

				nvfs++;
			} else
				vfs_unlock(vfsp);
		}
	}

	if (nvfs == 0) {
		vfs_list_unlock();
		return;
	}

	unmount_list = kmem_alloc(nvfs * sizeof (struct vfs *), KM_SLEEP);

	for (vfsp = head_vfsp, i = 0; vfsp != NULL; vfsp = vfsp->vfs_list) {
		unmount_list[i++] = vfsp;
		vfs_unlock(vfsp);
	}

	/*
	 * Once covered vnode is locked, no one can unmount the vfs.
	 * It is now safe to unlock the vfs list.
	 */
	vfs_list_unlock();

	/*
	 * Toss all dnlc entries now so that the per-vfs sync
	 * and unmount operations don't have to slog through
	 * a bunch of uninteresting vnodes over and over again.
	 */
	dnlc_purge();

	ASSERT(i == nvfs);

	for (i = 0; i < nvfs; i++)
		(void) VFS_SYNC(unmount_list[i], SYNC_CLOSE, CRED());

	for (i = 0; i < nvfs; i++) {
		(void) dounmount(unmount_list[i], 0, CRED());
	}

	kmem_free(unmount_list, nvfs * sizeof (struct vfs *));
}

/*
 * vfs_add is called by a specific filesystem's mount routine to add
 * the new vfs into the vfs list/hash and to cover the mounted-on vnode.
 * The vfs should already have been locked by the caller.
 *
 * coveredvp is NULL if this is the root.
 */
void
vfs_add(vnode_t *coveredvp, struct vfs *vfsp, int mflag)
{
	int newflag;

	ASSERT(vfs_lock_held(vfsp));
	VFS_HOLD(vfsp);
	newflag = vfsp->vfs_flag;
	if (mflag & MS_RDONLY)
		newflag |= VFS_RDONLY;
	else
		newflag &= ~VFS_RDONLY;
	if (mflag & MS_NOSUID)
		newflag |= VFS_NOSUID;
	else
		newflag &= ~VFS_NOSUID;
	if (mflag & MS_NOMNTTAB)
		newflag |= VFS_NOMNTTAB;
	else
		newflag &= ~VFS_NOMNTTAB;

	vfs_list_add(vfsp);

	if (coveredvp != NULL) {
		ASSERT(vn_vfswlock_held(coveredvp));
		coveredvp->v_vfsmountedhere = vfsp;
		VN_HOLD(coveredvp);
	}
	vfsp->vfs_vnodecovered = coveredvp;

	vfsp->vfs_flag = newflag;
}

/*
 * Remove a vfs from the vfs list, destroy pointers to it, and then destroy
 * the vfs itself.  Called from dounmount after it's confirmed with the file
 * system that the unmount is legal.
 */
void
vfs_remove(struct vfs *vfsp)
{
	vnode_t *vp;

	ASSERT(vfs_lock_held(vfsp));

	/*
	 * Can't unmount root.  Should never happen because fs will
	 * be busy.
	 */
	if (vfsp == rootvfs)
		cmn_err(CE_PANIC, "vfs_remove: unmounting root");

	vfs_list_remove(vfsp);

	/*
	 * Unhook from the file system name space.
	 */
	vp = vfsp->vfs_vnodecovered;
	ASSERT(vn_vfswlock_held(vp));
	vp->v_vfsmountedhere = NULL;
	VN_RELE(vp);

	/*
	 * Deallocate mnttab information
	 */
	vfs_freemnttab(vfsp);
	/*
	 * Release lock and wakeup anybody waiting.
	 */
	vfs_unlock(vfsp);
	VFS_RELE(vfsp);
}

/*
 * Lock a filesystem to prevent access to it while mounting,
 * unmounting and syncing.  Return EBUSY immediately if lock
 * can't be acquired.
 */
int
vfs_lock(vfs_t *vfsp)
{
	if (sema_tryp(&vfsp->vfs_reflock) == 0)
		return (EBUSY);
	return (0);
}

void
vfs_lock_wait(vfs_t *vfsp)
{
	sema_p(&vfsp->vfs_reflock);
}

/*
 * Unlock a locked filesystem.
 */
void
vfs_unlock(vfs_t *vfsp)
{
	sema_v(&vfsp->vfs_reflock);
}

/*
 * Utility routine that allows a filesystem to construct its
 * fsid in "the usual way" - by munging some underlying dev_t and
 * the filesystem type number into the 64-bit fsid.  Note that
 * this implicitly relies on dev_t persistence to make filesystem
 * id's persistent.
 *
 * There's nothing to prevent an individual fs from constructing its
 * fsid in a different way, and indeed they should.
 *
 * Since we want fsids to be 32-bit quantities (so that they can be
 * exported identically by either 32-bit or 64-bit APIs, as well as
 * the fact that fsid's are "known" to NFS), we compress the device
 * number given down to 32-bits, and panic if that isn't possible.
 */
void
vfs_make_fsid(fsid_t *fsi, dev_t dev, int val)
{
	if (!cmpldev((dev32_t *)&fsi->val[0], dev))
		panic("device number too big for fsid!");
	fsi->val[1] = val;
}

int
vfs_lock_held(vfs_t *vfsp)
{
	return (sema_held(&vfsp->vfs_reflock));
}

/*
 * vfs list locking.
 *
 * Rather than manipulate the vfslist mutex directly, we abstract into lock
 * and unlock routines to allow the locking implementation to be changed for
 * clustering.
 *
 * Whenever the vfs list is modified through its hash links, the overall list
 * lock must be obtained before locking the relevant hash bucket.  But to see
 * whether a given vfs is on the list, it suffices to obtain the lock for the
 * hash bucket without getting the overall list lock.  (See getvfs() below.)
 */

void
vfs_list_lock()
{
	mutex_enter(&vfslist);
}

void
vfs_list_unlock()
{
	mutex_exit(&vfslist);
}

/*
 * Low level worker routines for adding entries to and removing entries from
 * the vfs list.
 */

void
vfs_list_add(struct vfs *vfsp)
{
	int vhno = VFSHASH(vfsp->vfs_fsid.val[0], vfsp->vfs_fsid.val[1]);
	struct vfs **hp;

	/*
	 * Special casing for the root vfs.  This structure is allocated
	 * statically and hooked onto rootvfs at link time.  During the
	 * vfs_mountroot call at system startup time, the root file system's
	 * VFS_MOUNTROOT routine will call vfs_add with this root vfs struct
	 * as argument.  The code below must detect and handle this sepcial
	 * case.  The only apparent justification for this special casing is
	 * to ensure that the root file system appears at the head of the
	 * list.  (Other than that, the list is unordered; the implementation
	 * below places new entries immediately past the first entry.)
	 *
	 * XXX:	I'm assuming that it's ok to do normal list locking when
	 *	adding the entry for the root file system (this used to be
	 *	done with no locks held).
	 */
	vfs_list_lock();
	mutex_enter(&rvfs_lock[vhno]);
	/*
	 * Link into the vfs list proper.
	 */
	if (vfsp == &root) {
		/*
		 * Assert: This vfs is already on the list as its first entry.
		 * Thus, there's nothing to do.
		 */
		ASSERT(rootvfs == vfsp);
	} else {
		struct vfs *vp;

		/*
		 * Link to end of list so list is in mount order for
		 * mnttab use.
		 */
		vp = rootvfs;
		for (vp = rootvfs; vp->vfs_next != NULL; vp = vp->vfs_next)
			continue;
		vp->vfs_next = vfsp;
		vfsp->vfs_next = NULL;
	}
	/*
	 * Link into the hash table, inserting it at the end, so that LOFS
	 * with the same fsid as UFS (or other) file systems will not hide the
	 * UFS.
	 */
	for (hp = &rvfs_head[vhno]; *hp != NULL; hp = &(*hp)->vfs_hash)
		continue;
	/*
	 * hp now contains the address of the pointer to update to effect the
	 * insertion.
	 */
	vfsp->vfs_hash = NULL;
	*hp = vfsp;

	mutex_exit(&rvfs_lock[vhno]);
	/*
	 * update the mnttab modification time
	 */
	vfs_mnttab_modtimeupd();
	vfs_list_unlock();
}

void
vfs_list_remove(struct vfs *vfsp)
{
	int vhno = VFSHASH(vfsp->vfs_fsid.val[0], vfsp->vfs_fsid.val[1]);
	struct vfs *tvfsp;

	/*
	 * Callers are responsible for preventing attempts to unmount the
	 * root.
	 */
	ASSERT(vfsp != rootvfs);

	vfs_list_lock();
	mutex_enter(&rvfs_lock[vhno]);

	/*
	 * Remove from hash.
	 */
	if (rvfs_head[vhno] == vfsp) {
		rvfs_head[vhno] = vfsp->vfs_hash;
		goto foundit;
	}
	for (tvfsp = rvfs_head[vhno]; tvfsp != NULL; tvfsp = tvfsp->vfs_hash) {
		if (tvfsp->vfs_hash == vfsp) {
			tvfsp->vfs_hash = vfsp->vfs_hash;
			goto foundit;
		}
	}
	cmn_err(CE_WARN, "vfs_list_remove: vfs not found in hash");

foundit:
	/*
	 * Remove from list.
	 */
	for (tvfsp = rootvfs; tvfsp != NULL; tvfsp = tvfsp->vfs_next) {
		if (tvfsp->vfs_next != vfsp)
			continue;
		tvfsp->vfs_next = vfsp->vfs_next;
		vfsp->vfs_next = NULL;
		break;
	}

	mutex_exit(&rvfs_lock[vhno]);
	/*
	 * update the mnttab modification time
	 */
	if (tvfsp != NULL)
		vfs_mnttab_modtimeupd();
	vfs_list_unlock();

	if (tvfsp == NULL) {
		/*
		 * Couldn't find vfs to remove.
		 */
		cmn_err(CE_PANIC, "vfs_list_remove: vfs not found");
	}
}

struct vfs *
getvfs(fsid_t *fsid)
{
	struct vfs *vfsp;
	int val0 = fsid->val[0];
	int val1 = fsid->val[1];
	int vhno = VFSHASH(val0, val1);
	kmutex_t *hmp = &rvfs_lock[vhno];

	mutex_enter(hmp);
	for (vfsp = rvfs_head[vhno]; vfsp; vfsp = vfsp->vfs_hash) {
		if (vfsp->vfs_fsid.val[0] == val0 &&
		    vfsp->vfs_fsid.val[1] == val1) {
			VFS_HOLD(vfsp);
			mutex_exit(hmp);
			return (vfsp);
		}
	}
	mutex_exit(hmp);
	return (NULL);
}

/*
 * Search the vfs list for a specified device.  Returns 1, if entry is found
 * or 0 if no suitable entry is found.
 */

int
vfs_devismounted(dev_t dev)
{
	struct vfs *vfsp;
	int found;

	vfs_list_lock();
	found = 0;
	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next)
		if (vfsp->vfs_dev == dev) {
			found = 1;
			break;
		}
	vfs_list_unlock();
	return (found);
}

/*
 * Search the vfs list for a specified device.  Returns a pointer to it
 * or NULL if no suitable entry is found. The caller of this routine
 * is responsible for releasing the returned vfs pointer.
 */
struct vfs *
vfs_dev2vfsp(dev_t dev)
{
	struct vfs *vfsp;

	vfs_list_lock();
	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next)
		if (vfsp->vfs_dev == dev) {
			VFS_HOLD(vfsp);
			break;
		}
	vfs_list_unlock();
	return (vfsp);
}

/*
 * Search the vfs list for a specified vfsops.
 * if vfs entry is found then return 1, else 0.
 */
int
vfs_opsinuse(struct vfsops *ops)
{
	struct vfs *vfsp;
	int found;

	vfs_list_lock();
	found = 0;
	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next)
		if (vfsp->vfs_op == ops) {
			found = 1;
			break;
		}
	vfs_list_unlock();
	return (found);
}

/*
 * Allocate an entry in vfssw for a file system type
 */
struct vfssw *
allocate_vfssw(char *type)
{
	struct vfssw *vswp;

	if (type[0] == '\0') {
		/*
		 * The vfssw table uses the empty string to identify an
		 * available entry; we cannot add any type which has
		 * a leading NUL.
		 */
		return (NULL);
	}

	ASSERT(VFSSW_WRITE_LOCKED());
	for (vswp = &vfssw[1]; vswp < &vfssw[nfstype]; vswp++)
		if (!ALLOCATED_VFSSW(vswp)) {
			vswp->vsw_name = kmem_alloc(strlen(type) + 1, KM_SLEEP);
			(void) strcpy(vswp->vsw_name, type);
			return (vswp);
		}
	return (NULL);
}

/*
 * Impose additional layer of translation between vfstype names
 * and module names in the filesystem.
 */
static char *
vfs_to_modname(char *vfstype)
{
	if (strcmp(vfstype, "proc") == 0) {
		vfstype = "procfs";
	} else if (strcmp(vfstype, "fd") == 0) {
		vfstype = "fdfs";
	} else if (strncmp(vfstype, "nfs", 3) == 0) {
		vfstype = "nfs";
	}

	return (vfstype);
}

/*
 * Find a vfssw entry given a file system type name.
 * Try to autoload the filesystem if it's not found.
 * If it's installed, return the vfssw locked to prevent unloading.
 */
struct vfssw *
vfs_getvfssw(char *type)
{
	struct vfssw *vswp;
	char	*modname;
	int rval;
	int new_entry = 0;

	RLOCK_VFSSW();
	if ((vswp = vfs_getvfsswbyname(type)) == NULL) {
		RUNLOCK_VFSSW();
		WLOCK_VFSSW();
		if ((vswp = vfs_getvfsswbyname(type)) == NULL) {
			if ((vswp = allocate_vfssw(type)) == NULL) {
				WUNLOCK_VFSSW();
				return (NULL);
			} else {
				new_entry = 1;
			}
		}
		WUNLOCK_VFSSW();
		RLOCK_VFSSW();
	}

	modname = vfs_to_modname(type);

	/*
	 * Try to load the filesystem.  Before calling modload(), we drop
	 * our lock on the VFS switch table, and pick it up after the
	 * module is loaded.  However, there is a potential race:  the
	 * module could be unloaded after the call to modload() completes
	 * but before we pick up the lock and drive on.  Therefore,
	 * we keep reloading the module until we've loaded the module
	 * _and_ we have the lock on the VFS switch table.
	 */
	while (!VFS_INSTALLED(vswp)) {
		RUNLOCK_VFSSW();
		if (rootdir != NULL)
			rval = modload("fs", modname);
		else {
			/*
			 * If we haven't yet loaded the root file
			 * system, then our _init won't be called until
			 * later; don't bother looping.
			 */
			rval = modloadonly("fs", modname);
			RLOCK_VFSSW();
			break;
		}
		if (rval == -1) {
			if (new_entry) {
				/*
				 * Could not load the module for that vfssw
				 * entry. Free the entry in case we didn't
				 * know about that fs type before.
				 */
				WLOCK_VFSSW();
				kmem_free(vswp->vsw_name,
					strlen(vswp->vsw_name) + 1);
				vswp->vsw_name = vfs_dummyfstype;
				WUNLOCK_VFSSW();
			}
			return (NULL);
		}
		RLOCK_VFSSW();
	}

	return (vswp);
}

/*
 * Find a vfssw entry given a file system type name.
 */
struct vfssw *
vfs_getvfsswbyname(char *type)
{
	int i;

	ASSERT(VFSSW_LOCKED());
	if (type == NULL || *type == '\0')
		return (NULL);

	for (i = 1; i < nfstype; i++)
		if (strcmp(type, vfssw[i].vsw_name) == 0)
			return (&vfssw[i]);

	return (NULL);
}

static int sync_timeout = 20;	/* timeout for syncing a page during panic */
int sync_timeleft;		/* portion of sync_timeout remaining */
int sync_aborted;		/* flag set if sync aborted below */
static volatile int sync_yield = 1; /* flag to control yielding; see below */
static int sync_yielding;	/* set to a non-zero value when sync yielded */

static pgcnt_t old_pgcnt, new_pgcnt;
static int new_bufcnt, old_bufcnt;

int
sync_making_progress(int wait)
{
	old_bufcnt = new_bufcnt;
	old_pgcnt = new_pgcnt;

	new_bufcnt = bio_busy(wait);
	new_pgcnt = page_busy(wait);

	if (wait && sync_timeleft <= 0 && deadman_sync_timeleft <= 1) {
		/*
		 * We have been (or very shortly will be) timed out by the
		 * deadman.  The sync hung for a long time, but amazingly,
		 * it's still progressing (this means we were in a single
		 * call to page_busy for longer than (deadman_sync_timeout - 2)
		 * seconds).  To avoid a race with the deadman, we're going
		 * to spin forever and let the deadman time us out.
		 *
		 * If sync_timeleft is 0, but deadman_sync_timeleft is 2 or
		 * more, then we are in one of two situations:  either we are
		 * making progress (in which case, we'll reset sync_timeleft
		 * below) or we're not (in which case, we'll set sync_aborted
		 * to 1 and abort the sync ourselves).  At any rate, the
		 * deadman will see the choice that we have made, and not
		 * timeout the sync.  The implication here is that we
		 * are able to either reset sync_timeleft or set sync_aborted
		 * within one second from the time the comparison against
		 * deadman_sync_timeleft is made, above.
		 */
		sync_yielding++;
		while (sync_yield)
			continue;
	}

	/*
	 * If we're making progress, get a new lease on life.
	 */
	if (new_bufcnt < old_bufcnt || new_pgcnt < old_pgcnt)
		sync_timeleft = sync_timeout;

	if (new_bufcnt == 0 && new_pgcnt == 0) {	/* sync is complete */
		sync_timeleft = 0;
		printf(" done\n");
	} else if (wait) {
		if (new_bufcnt)
			printf(" [%d]", new_bufcnt);
		if (new_pgcnt)
			printf(" %lu", new_pgcnt);

		if (sync_timeleft > 0)
			delay(hz);

		if (--sync_timeleft <= 0) {
			sync_timeleft = 0;
			sync_aborted = 1;
			printf(" cannot sync -- giving up\n");
		}
	}

	if (sync_timeleft == 0)
		delay(hz);	/* give users a chance to read the message */
	return (sync_timeleft);
}

/*
 * "sync" all file systems, and return only when all writes have been
 * completed.  For use by the reboot code; it's verbose.
 */
void
vfs_syncall()
{
	if (rootdir == NULL && !modrootloaded)
		return; /* panic during boot - no filesystems yet */

	printf("syncing file systems...");
	sync_timeleft = sync_timeout;		/* start sync timer */
	sync();
	while (sync_making_progress(1))
		continue;
}

/*
 * Map VFS flags to statvfs flags.  These shouldn't really be separate
 * flags at all.
 */
uint_t
vf_to_stf(uint_t vf)
{
	uint_t stf = 0;

	if (vf & VFS_RDONLY)
		stf |= ST_RDONLY;
	if (vf & VFS_NOSUID)
		stf |= ST_NOSUID;
	if (vf & VFS_NOTRUNC)
		stf |= ST_NOTRUNC;

	return (stf);
}

/*
 * Use old-style function prototype for vfsstray() so
 * that we can use it anywhere in the vfsops structure.
 */
int vfsstray();

/*
 * Entries for (illegal) fstype 0.
 */
/* ARGSUSED */
int
vfsstray_sync(struct vfs *vfsp, short arg, struct cred *cr)
{
	cmn_err(CE_PANIC, "stray vfs operation");
	return (0);
}

struct vfsops vfs_strayops = {
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray_sync,
	vfsstray,
	vfsstray,
	vfsstray
};

/*
 * Entries for (illegal) fstype 0.
 */
int
vfsstray(void)
{
	cmn_err(CE_PANIC, "stray vfs operation");
	return (0);
}

int vfs_EIO();
int vfs_EIO_sync(struct vfs *, short, struct cred *);

vfsops_t EIO_vfsops = {
	vfs_EIO,
	vfs_EIO,
	vfs_EIO,
	vfs_EIO,
	vfs_EIO_sync,
	vfs_EIO,
	vfs_EIO,
	vfs_EIO
};

/*
 * Support for dealing with forced UFS unmounts and it's interaction with
 * LOFS. Could be used by any filesystem.
 * See bug 1203132.
 */
int
vfs_EIO(void)
{
	return (EIO);
}

/*
 * We've gotta define the op for sync seperately, since the compiler gets
 * confused if we mix and match ANSI and normal style prototypes when
 * a "short" argument is present and spits out a warning.
 */
/*ARGSUSED*/
int
vfs_EIO_sync(struct vfs *vfsp, short arg, struct cred *cr)
{
	return (EIO);
}

vfs_t EIO_vfs;

/*
 * Called from startup() to initialize all loaded vfs's
 */
void
vfsinit(void)
{
	int i;

	/*
	 * fstype 0 is (arbitrarily) invalid.
	 */
	vfssw[0].vsw_vfsops = &vfs_strayops;
	vfssw[0].vsw_name = "BADVFS";

	VFS_INIT(&EIO_vfs, &EIO_vfsops, (caddr_t)NULL);

	/*
	 * Call all the init routines.
	 */
	/*
	 * A mixture of loadable and non-loadable filesystems
	 * is tricky to support, because of contention over exactly
	 * when the filesystems vsw_init() routine should be
	 * run on the rootfs -- at this point in the boot sequence, the
	 * rootfs module has  been loaded into the table, but its _init()
	 * routine and the vsw_init() routine have yet to be called - this
	 * will happen when we actually do the proper modload() in rootconf().
	 *
	 * So we use the following heuristic.  For each name in the
	 * switch with a non-nil init routine, we look for a module
	 * of the appropriate name - if it exists, we infer that
	 * the loadable module code has either already vsw_init()-ed
	 * it, or will vsw_init() soon.  If it can't be found there, then
	 * we infer this is a statically configured filesystem so we get on
	 * and call its vsw_init() routine directly.
	 *
	 * Sigh.  There's got to be a better way to do this.
	 */
	ASSERT(VFSSW_LOCKED());		/* the root fs */
	RUNLOCK_VFSSW();
	for (i = 1; i < nfstype; i++) {
		RLOCK_VFSSW();
		if (vfssw[i].vsw_init) {
			char *modname;

			modname = vfs_to_modname(vfssw[i].vsw_name);
			/*
			 * XXX	Should probably hold the mod_lock here
			 */
			if (!mod_find_by_filename("fs", modname))
				(*vfssw[i].vsw_init)(&vfssw[i], i);
		}
		RUNLOCK_VFSSW();
	}
	RLOCK_VFSSW();
}

/*
 * Increments the vfs reference count by one atomically.
 */
void
vfs_hold(vfs_t *vfsp)
{
	atomic_add_32(&vfsp->vfs_count, 1);
	ASSERT(vfsp->vfs_count != 0);
}

/*
 * Decrements the vfs reference count by one atomically. When
 * vfs reference count becomes zero, it calls the file system
 * specific vfs_freevfs() to free up the resources.
 */
void
vfs_rele(vfs_t *vfsp)
{
	ASSERT(vfsp->vfs_count != 0);
	if (atomic_add_32_nv(&vfsp->vfs_count, -1) == 0) {
		VFS_FREEVFS(vfsp);
		sema_destroy(&vfsp->vfs_reflock);
		kmem_free(vfsp, sizeof (*vfsp));
	}
}
