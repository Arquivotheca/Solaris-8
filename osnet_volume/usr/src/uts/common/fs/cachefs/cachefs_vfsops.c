/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident   "@(#)cachefs_vfsops.c 1.145     99/12/13 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/uio.h>
#include <sys/tiuser.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/bootconf.h>
#include <sys/reboot.h>
#include <sys/modctl.h>
#include <sys/fs/cachefs_fs.h>
#include <sys/fs/cachefs_log.h>
#include <sys/mkdev.h>
#include <sys/dnlc.h>
#include "fs/fs_subr.h"

extern int cachefs_module_keepcnt;
extern kmutex_t cachefs_kmem_lock;
kmutex_t cachefs_kstat_key_lock;

/* forward declarations */
static int cachefs_remount(struct vfs *, struct mounta *);
static void cachefs_delete_cachep(cachefscache_t *);

#define	CFS_MAPSIZE	256

kmutex_t cachefs_cachelock;			/* Cache list mutex */
cachefscache_t *cachefs_cachelist = NULL;		/* Cache struct list */

int cachefs_mount_retries = 3;
kmutex_t cachefs_minor_lock;		/* Lock for minor device map */
major_t cachefs_major = 0;
minor_t cachefs_minor = 0;
cachefs_kstat_key_t *cachefs_kstat_key = NULL;
int cachefs_kstat_key_n = 0;
/*
 * cachefs vfs operations.
 */
static	int cachefs_mount(vfs_t *, vnode_t *, struct mounta *, cred_t *);
static	int cachefs_unmount(vfs_t *, int, cred_t *);
static	int cachefs_root(vfs_t *, vnode_t **);
static	int cachefs_statvfs(register vfs_t *, struct statvfs64 *);
static	int cachefs_sync(vfs_t *, short, cred_t *);
static	int cachefs_mountroot(vfs_t *, whymountroot_t);
static	int cachefs_swapvp(vfs_t *, vnode_t **, char *);

struct vfsops cachefs_vfsops = {
	cachefs_mount,
	cachefs_unmount,
	cachefs_root,
	cachefs_statvfs,
	cachefs_sync,
	fs_nosys,
	cachefs_mountroot,
	cachefs_swapvp,
	fs_freevfs
};

/*
 * Initialize the vfs structure
 */
int cachefsfstyp;
int cnodesize = 0;

dev_t
cachefs_mkmntdev(void)
{
	dev_t cachefs_dev;

	mutex_enter(&cachefs_minor_lock);
	do {
		cachefs_minor = (cachefs_minor + 1) & MAXMIN32;
		cachefs_dev = makedevice(cachefs_major, cachefs_minor);
	} while (vfs_devismounted(cachefs_dev));
	mutex_exit(&cachefs_minor_lock);

	return (cachefs_dev);
}

/*
 * vfs operations
 */
static int
cachefs_mount(vfs_t *vfsp, vnode_t *mvp, struct mounta *uap, cred_t *cr)
{
	char *data = uap->dataptr;
	STRUCT_DECL(cachefs_mountargs, map);
	struct cachefsoptions	*cfs_options;
	char			*backfs, *cacheid, *cachedir;
	vnode_t *cachedirvp = NULL;
	vnode_t *backrootvp = NULL;
	cachefscache_t *cachep = NULL;
	fscache_t *fscp = NULL;
	cnode_t *cp;
	struct fid *cookiep = NULL;
	struct vattr *attrp = NULL;
	dev_t cachefs_dev;			/* devid for this mount */
	int error = 0;
	int retries = cachefs_mount_retries;
	ino64_t fsid;
	cfs_cid_t cid;
	char *backmntpt;
	ino64_t backfileno;
	struct vfs *backvfsp;
	size_t strl;
	char tmpstr[MAXPATHLEN];
	vnode_t *tmpdirvp = NULL;
	u_long maxfilesizebits;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VFSOP)
		printf("cachefs_mount: ENTER cachefs_mntargs %p\n", data);
#endif

	cachefs_module_keepcnt++;
	/*
	 * Make sure we're root
	 */
	if (!suser(cr)) {
		error = EPERM;
		goto out;
	}

	/*
	 * make sure we're mounting on a directory
	 */
	if (mvp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}

	if (uap->flags & MS_REMOUNT) {
		error = cachefs_remount(vfsp, uap);
		goto out;
	}

	/*
	 * Assign a unique device id to the mount
	 */
	cachefs_dev = cachefs_mkmntdev();
#ifdef _LP64
	/*
	 * XX64: It's not a good idea to make fsid bigger since that'll
	 * have adverse effects on nfs filehandles.  For now assume that
	 * cachefs be used on devices that fit into dev32_t's.
	 */
	if (cachefs_dev == NODEV) {
		error = EOVERFLOW;
		goto out;
	}
#endif

	/*
	 * Copy in the arguments
	 */
	STRUCT_INIT(map, get_udatamodel());
	error = copyin(data, STRUCT_BUF(map),
			SIZEOF_STRUCT(cachefs_mountargs, DATAMODEL_NATIVE));
	if (error) {
		goto out;
	}

	cfs_options = (struct cachefsoptions *)STRUCT_FADDR(map, cfs_options);
	cacheid = (char *)STRUCT_FGETP(map, cfs_cacheid);
	if ((cfs_options->opt_flags &
	    (CFS_WRITE_AROUND|CFS_NONSHARED)) == 0) {
		error = EINVAL;
		goto out;
	}
	if ((cfs_options->opt_popsize % MAXBSIZE) != 0) {
		error = EINVAL;
		goto out;
	}
	/*
	 * Get the cache directory vp
	 */
	/*LINTED 32-bit pointer casting okay*/
	cachedir = (char *)STRUCT_FGETP(map, cfs_cachedir);
	error = lookupname(cachedir, UIO_USERSPACE, FOLLOW,
			NULLVPP, &cachedirvp);
	if (error)
		goto out;

	/*
	 * Make sure the thing we just looked up is a directory
	 */
	if (cachedirvp->v_type != VDIR) {
		cmn_err(CE_WARN, "cachefs_mount: cachedir not a directory\n");
		error = EINVAL;
		goto out;
	}

	/*
	 * Make sure the cache doesn't live in cachefs!
	 */
	if (cachedirvp->v_op == cachefs_getvnodeops()) {
		cmn_err(CE_WARN, "cachefs_mount: cachedir in cachefs!\n");
		error = EINVAL;
		goto out;
	}

	/* if the backfs is mounted */
	/*LINTED 32-bit pointer casting okay*/
	if ((backfs = STRUCT_FGETP(map, cfs_backfs)) != NULL) {
		/*
		 * Get the back file system root vp
		 */
		error = lookupname(backfs, UIO_USERSPACE, FOLLOW,
			NULLVPP, &backrootvp);
		if (error)
			goto out;

		/*
		 * Make sure the thing we just looked up is a directory
		 * and a root of a file system
		 */
		if (backrootvp->v_type != VDIR ||
		    !(backrootvp->v_flag & VROOT)) {
			cmn_err(CE_WARN,
			    "cachefs_mount: backpath not a directory\n");
			error = EINVAL;
			goto out;
		}

		cookiep = cachefs_kmem_alloc(sizeof (struct fid), KM_SLEEP);
		attrp = cachefs_kmem_alloc(sizeof (struct vattr), KM_SLEEP);
		error = cachefs_getcookie(backrootvp, cookiep, attrp, cr);
		if (error)
			goto out;

		backmntpt = backfs;
		backfileno = attrp->va_nodeid;
		backvfsp = backrootvp->v_vfsp;
	} else {
		backmntpt = NULL;
		backfileno = 0;
		backvfsp = NULL;
	}

again:

	/*
	 * In SVR4 it's not acceptable to stack up mounts
	 * unless MS_OVERLAY specified.
	 */
	mutex_enter(&mvp->v_lock);
	if (((uap->flags & MS_OVERLAY) == 0) &&
	    ((mvp->v_count != 1) || (mvp->v_flag & VROOT))) {
		mutex_exit(&mvp->v_lock);
		error = EBUSY;
		goto out;
	}
	mutex_exit(&mvp->v_lock);

	/*
	 * Lock out other mounts and unmounts until we safely have
	 * a mounted fscache object.
	 */
	mutex_enter(&cachefs_cachelock);

	/*
	 * Find the cache structure
	 */
	for (cachep = cachefs_cachelist; cachep != NULL;
		cachep = cachep->c_next) {
		if (cachep->c_dirvp == cachedirvp)
			break;
	}

	/* if the cache object does not exist, then create it */
	if (cachep == NULL) {
		cachep = cachefs_cache_create();
		if (cachep == NULL) {
			mutex_exit(&cachefs_cachelock);
			error = ENOMEM;
			goto out;
		}
		error = cachefs_cache_activate_ro(cachep, cachedirvp);
		if (error) {
			cachefs_cache_destroy(cachep);
			cachep = NULL;
			mutex_exit(&cachefs_cachelock);
			goto out;
		}
		if ((cfs_options->opt_flags & CFS_NOFILL) == 0)
			cachefs_cache_activate_rw(cachep);
		else
			cfs_options->opt_flags &= ~CFS_NOFILL;

		cachep->c_next = cachefs_cachelist;
		cachefs_cachelist = cachep;
	} else if (cfs_options->opt_flags & CFS_NOFILL) {
		mutex_exit(&cachefs_cachelock);
		cmn_err(CE_WARN,
		    "CacheFS: attempt to convert nonempty cache "
		    "to NOFILL mode");
		error = EINVAL;
		goto out;
	}

	/* get the fscache id for this name */
	error = fscache_name_to_fsid(cachep, cacheid, &fsid);
	if (error) {
		fsid = 0;
	}

	/* find the fscache object for this mount point or create it */
	mutex_enter(&cachep->c_fslistlock);
	fscp = fscache_list_find(cachep, fsid);
	if (fscp == NULL) {
		fscp = fscache_create(cachep);
		error = fscache_activate(fscp, fsid, cacheid,
			cfs_options, backfileno);
		if (error) {
			fscache_destroy(fscp);
			fscp = NULL;
			mutex_exit(&cachep->c_fslistlock);
			mutex_exit(&cachefs_cachelock);
			if ((error == ENOSPC) && (retries-- > 0)) {
				delay(6 * hz);
				goto again;
			}
			goto out;
		}
		fscache_list_add(cachep, fscp);
	} else {
		/* compare the options to make sure they are compatable */
		error = fscache_compare_options(fscp, cfs_options);
		if (error) {
			cmn_err(CE_WARN,
				"CacheFS: mount failed, options do not match.");
			fscp = NULL;
			mutex_exit(&cachep->c_fslistlock);
			mutex_exit(&cachefs_cachelock);
			goto out;
		}

		/* copy options into the fscache */
		mutex_enter(&fscp->fs_fslock);
		fscp->fs_info.fi_mntflags = cfs_options->opt_flags;
		fscp->fs_info.fi_popsize = cfs_options->opt_popsize;
		fscp->fs_info.fi_fgsize = cfs_options->opt_fgsize;
		fscp->fs_flags |= CFS_FS_DIRTYINFO;
		mutex_exit(&fscp->fs_fslock);
	}
	fscache_hold(fscp);

	error = 0;
	if (fscp->fs_fscdirvp) {
		error = VOP_LOOKUP(fscp->fs_fscdirvp, CACHEFS_DLOG_FILE,
		    &tmpdirvp, NULL, 0, NULL, kcred);

		/*
		 * If a log file exists and the cache is being mounted without
		 * the snr (aka disconnectable) option, return an error.  */
		if ((error == 0) &&
		    !(cfs_options->opt_flags & CFS_DISCONNECTABLE)) {
			mutex_exit(&cachep->c_fslistlock);
			mutex_exit(&cachefs_cachelock);
			cmn_err(CE_WARN, "cachefs: log exists and "
			    "disconnectable option not specified\n");
			error = EINVAL;
			goto out;
		}
	}

	/*
	 * Acquire the name of the mount point
	 */
	if (fscp->fs_mntpt == NULL) {
		/*
		 * the string length returned by copystr includes the
		 * terminating NULL character, unless a NULL string is
		 * passed in, then the string length is unchanged.
		 */
		strl = 0;
		tmpstr[0] = '\0';
		(void) copyinstr(uap->dir, tmpstr, MAXPATHLEN, &strl);
		if (strl > 1) {
			fscp->fs_mntpt = kmem_alloc(strl, KM_SLEEP);
			(void) strncpy(fscp->fs_mntpt, tmpstr, strl);
		}
		/*
		 * else fscp->fs_mntpt is unchanged(still NULL) try again
		 * next time
		 */
	}

	/*
	 * Acquire the name of the server
	 */
	if (fscp->fs_hostname == NULL) {
		strl = 0;
		tmpstr[0] = '\0';
		/*LINTED 32-bit pointer casting okay*/
		(void) copyinstr((char *)STRUCT_FGETP(map, cfs_hostname),
				tmpstr, MAXPATHLEN, &strl);
		if (strl > 1) {
			fscp->fs_hostname = kmem_alloc(strl, KM_SLEEP);
			(void) strncpy(fscp->fs_hostname, tmpstr, strl);
		}
		/*
		 * else fscp->fs_hostname remains unchanged (is still NULL)
		 */
	}

	/*
	 * Acquire name of the back filesystem
	 */
	if (fscp->fs_backfsname == NULL) {
		strl = 0;
		tmpstr[0] = '\0';
		/*LINTED 32-bit pointer casting okay*/
		(void) copyinstr((char *)STRUCT_FGETP(map, cfs_backfsname),
				tmpstr, MAXPATHLEN, &strl);
		if (strl > 1) {
			fscp->fs_backfsname = kmem_alloc(strl, KM_SLEEP);
			(void) strncpy(fscp->fs_backfsname, tmpstr, strl);
		}
		/*
		 * else fscp->fs_backfsname remains unchanged (is still NULL)
		 */
	}

	backfileno = fscp->fs_info.fi_root;
	mutex_exit(&cachep->c_fslistlock);

	/* see if fscache object is already mounted, it not, make it so */
	error = fscache_mounted(fscp, vfsp, backvfsp);
	if (error) {
		/* fs cache was already mounted */
		mutex_exit(&cachefs_cachelock);
		error = EBUSY;
		goto out;
	}

	/* allow other mounts and unmounts to proceed */
	mutex_exit(&cachefs_cachelock);

	cachefs_kstat_mount(fscp, uap->dir, backmntpt, cachedir, cacheid);

	/* set nfs style time out parameters */
	fscache_acset(fscp, STRUCT_FGET(map, cfs_acregmin),
	    STRUCT_FGET(map, cfs_acregmax),
	    STRUCT_FGET(map, cfs_acdirmin), STRUCT_FGET(map, cfs_acdirmax));

	vfsp->vfs_dev = cachefs_dev;
	vfsp->vfs_data = (caddr_t)fscp;
	vfs_make_fsid(&vfsp->vfs_fsid, cachefs_dev, cachefsfstyp);
	vfsp->vfs_fstype = cachefsfstyp;
	if (backvfsp)
		vfsp->vfs_bsize = backvfsp->vfs_bsize;
	else
		vfsp->vfs_bsize = MAXBSIZE;	/* XXX */

	/* make a cnode for the root of the file system */
	cid.cid_flags = 0;
	cid.cid_fileno = backfileno;
	error = cachefs_cnode_make(&cid, fscp, cookiep, attrp,
	    backrootvp, cr, CN_ROOT, &cp);

	if (error) {
		cmn_err(CE_WARN, "cachefs_mount: can't create root cnode\n");
		goto out;
	}

	/* stick the root cnode in the fscache object */
	mutex_enter(&fscp->fs_fslock);
	fscp->fs_rootvp = CTOV(cp);
	fscp->fs_rootvp->v_flag |= VROOT;
	fscp->fs_rootvp->v_type |= cp->c_attr.va_type;
	ASSERT(fscp->fs_rootvp->v_type == VDIR);

	/*
	 * Get the maxfilesize bits of the back file system.
	 */

	error = VOP_PATHCONF(backrootvp, _PC_FILESIZEBITS, &maxfilesizebits,
		    kcred);

	if (error) {
		cmn_err(CE_WARN,
	"cachefs_mount: Can't get the FILESIZEBITS of the back root vnode \n");
		goto out;
	}

	fscp->fs_offmax = (1LL << (maxfilesizebits - 1)) - 1;
	mutex_exit(&fscp->fs_fslock);


	/* remove the unmount file if it is there */
	VOP_REMOVE(fscp->fs_fscdirvp, CACHEFS_UNMNT_FILE, kcred);

	/* wake up the cache worker if ANY packed pending work */
	mutex_enter(&cachep->c_contentslock);
	if (cachep->c_flags & CACHE_PACKED_PENDING)
		cv_signal(&cachep->c_cwcv);
	mutex_exit(&cachep->c_contentslock);

out:
	/*
	 * make a log entry, if appropriate
	 */

	if ((cachep != NULL) &&
	    CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_MOUNT))
		cachefs_log_mount(cachep, error, vfsp, fscp,
		    uap->dir, UIO_USERSPACE,
		    (STRUCT_BUF(map) != NULL) ? cacheid : NULL);

	/*
	 * Cleanup our mess
	 */
	if (cookiep != NULL)
		cachefs_kmem_free(cookiep, sizeof (struct fid));
	if (cachedirvp != NULL)
		VN_RELE(cachedirvp);
	if (backrootvp != NULL)
		VN_RELE(backrootvp);
	if (fscp)
		fscache_rele(fscp);
	if (attrp)
		cachefs_kmem_free(attrp, sizeof (struct vattr));

	if (error) {
		cachefs_module_keepcnt--;
		if (cachep) {
			int xx;

			/* lock out mounts and umounts */
			mutex_enter(&cachefs_cachelock);

			/* lock the cachep's fslist */
			mutex_enter(&cachep->c_fslistlock);

			/*
			 * gc isn't necessary for list_mounted(), but
			 * we want to do it anyway.
			 */

			fscache_list_gc(cachep);
			xx = fscache_list_mounted(cachep);

			mutex_exit(&cachep->c_fslistlock);

			/* if no more references to this cachep, punt it. */
			if (xx == 0)
				cachefs_delete_cachep(cachep);
			mutex_exit(&cachefs_cachelock);
		}
	}

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VFSOP)
		printf("cachefs_mount: EXIT\n");
#endif
	return (error);
}

void
cachefs_kstat_mount(struct fscache *fscp,
    char *umountpoint, char *ubackfs, char *ucachedir, char *cacheid)
{
	cachefscache_t *cachep = fscp->fs_cache;
	cachefs_kstat_key_t *key;
	char *mountpoint = NULL, *backfs = NULL, *cachedir = NULL;
	size_t len;
	kstat_t *ksp;
	int i, rc;

	mountpoint = cachefs_kmem_alloc(MAXPATHLEN, KM_SLEEP);
	if (copyinstr(umountpoint, mountpoint, MAXPATHLEN, &len) != 0)
		goto out;

	cachedir = cachefs_kmem_alloc(MAXPATHLEN, KM_SLEEP);
	if (copyinstr(ucachedir, cachedir, MAXPATHLEN, &len) != 0)
		goto out;

	backfs = cachefs_kmem_alloc(MAXPATHLEN, KM_SLEEP);
	if (backfs) {
		if (copyinstr(ubackfs, backfs, MAXPATHLEN, &len) != 0)
			goto out;
	} else {
		(void) strcpy(backfs, "no back file system");
	}

	ASSERT(strlen(mountpoint) < MAXPATHLEN);
	ASSERT(strlen(backfs) < MAXPATHLEN);
	ASSERT(strlen(cachedir) < MAXPATHLEN);

	/* protect cachefs_kstat_key */
	mutex_enter(&cachefs_kstat_key_lock);
	/*
	 * XXXX If already there, why not go straight to it?
	 * We know that fscp->fs_kstat_id == i + 1
	 */
	i = fscp->fs_kstat_id - 1;
	if ((i >= 0) && (i < cachefs_kstat_key_n))
		rc = 1;
	else
		rc = i = 0;
	for (; i < cachefs_kstat_key_n; i++) {
		key = cachefs_kstat_key + i;
		if ((strcmp((char *)key->ks_mountpoint, mountpoint) == 0) &&
		    (strcmp((char *)key->ks_cachedir, cachedir) == 0) &&
		    (strcmp((char *)key->ks_cacheid, cacheid) == 0))
			break;
		if (rc) {	/* direct key did not work - check all */
			i = -1;	/* will increment to zero in loop */
			rc = 0;
		}
	}

	if (i >= cachefs_kstat_key_n) {
		key = cachefs_kmem_alloc((cachefs_kstat_key_n + 1) *
		    sizeof (cachefs_kstat_key_t), KM_SLEEP);
		if (cachefs_kstat_key != NULL) {
			bcopy(cachefs_kstat_key, key,
			    cachefs_kstat_key_n * sizeof (*key));
			cachefs_kmem_free(cachefs_kstat_key,
			    cachefs_kstat_key_n * sizeof (*key));
		}
		cachefs_kstat_key = key;
		key = cachefs_kstat_key + cachefs_kstat_key_n;
		++cachefs_kstat_key_n;
		rc = key->ks_id = cachefs_kstat_key_n; /* offset + 1 */

		key->ks_mountpoint = (uint64_t)cachefs_strdup(mountpoint);
		key->ks_backfs = (uint64_t)cachefs_strdup(backfs);
		key->ks_cachedir = (uint64_t)cachefs_strdup(cachedir);
		key->ks_cacheid = (uint64_t)cachefs_strdup(cacheid);
	} else
		rc = key->ks_id;

	mutex_enter(&fscp->fs_fslock); /* protect fscp */

	fscp->fs_kstat_id = rc;

	mutex_exit(&fscp->fs_fslock); /* finished with fscp */
	/* finished cachefs_kstat_key */
	mutex_exit(&cachefs_kstat_key_lock);

	key->ks_vfsp = (uint64_t)fscp->fs_cfsvfsp;
	key->ks_mounted = 1;

	/*
	 * we must not be holding any mutex that is a ks_lock field
	 * for one of the kstats when we invoke kstat_create,
	 * kstat_install, and friends.
	 */
	ASSERT(MUTEX_NOT_HELD(&cachefs_kstat_key_lock));
	/* really should be EVERY cachep's c_log_mutex */
	ASSERT(MUTEX_NOT_HELD(&cachep->c_log_mutex));

	/* cachefs.#.log */
	ksp = kstat_create("cachefs", fscp->fs_kstat_id, "log",
	    "misc", KSTAT_TYPE_RAW, 1,
	    KSTAT_FLAG_WRITABLE | KSTAT_FLAG_VIRTUAL);
	if (ksp != NULL) {
		ksp->ks_data = cachep->c_log_ctl;
		ksp->ks_data_size = sizeof (cachefs_log_control_t);
		ksp->ks_lock = &cachep->c_log_mutex;
		ksp->ks_snapshot = cachefs_log_kstat_snapshot;
		kstat_install(ksp);
	}
	/* cachefs.#.stats */
	ksp = kstat_create("cachefs", fscp->fs_kstat_id, "stats",
	    "misc", KSTAT_TYPE_RAW, 1,
	    KSTAT_FLAG_WRITABLE | KSTAT_FLAG_VIRTUAL);
	if (ksp != NULL) {
		ksp->ks_data = fscp;
		ksp->ks_data_size = sizeof (cachefs_stats_t);
		ksp->ks_snapshot = cachefs_stats_kstat_snapshot;
		kstat_install(ksp);
	}

out:
	if (mountpoint != NULL)
		cachefs_kmem_free(mountpoint, MAXPATHLEN);
	if (backfs != NULL)
		cachefs_kmem_free(backfs, MAXPATHLEN);
	if (cachedir != NULL)
		cachefs_kmem_free(cachedir, MAXPATHLEN);
}

void
cachefs_kstat_umount(int ksid)
{
	cachefs_kstat_key_t *k = cachefs_kstat_key + (ksid - 1);
	kstat_t *stats, *log;

	ASSERT(k->ks_id == ksid);

	k->ks_mounted = 0;

	mutex_enter(&kstat_chain_lock);
	stats = kstat_lookup_byname("cachefs", ksid, "stats");
	log = kstat_lookup_byname("cachefs", ksid, "log");
	mutex_exit(&kstat_chain_lock);

	if (stats != NULL)
		kstat_delete(stats);
	if (log != NULL)
		kstat_delete(log);
}

int
cachefs_kstat_key_update(kstat_t *ksp, int rw)
{
	cachefs_kstat_key_t *key = *((cachefs_kstat_key_t **)ksp->ks_data);
	cachefs_kstat_key_t *k;
	int i;

	if (rw == KSTAT_WRITE)
		return (EIO);
	if (key == NULL)
		return (EIO);

	ksp->ks_data_size = cachefs_kstat_key_n * sizeof (*key);
	for (i = 0; i < cachefs_kstat_key_n; i++) {
		k = key + i;

		ksp->ks_data_size += strlen((char *)k->ks_mountpoint) + 1;
		ksp->ks_data_size += strlen((char *)k->ks_backfs) + 1;
		ksp->ks_data_size += strlen((char *)k->ks_cachedir) + 1;
		ksp->ks_data_size += strlen((char *)k->ks_cacheid) + 1;
	}

	ksp->ks_ndata = cachefs_kstat_key_n;

	return (0);
}

int
cachefs_kstat_key_snapshot(kstat_t *ksp, void *buf, int rw)
{
	cachefs_kstat_key_t *key = *((cachefs_kstat_key_t **)ksp->ks_data);
	cachefs_kstat_key_t *k;
	caddr_t s;
	int i;

	if (rw == KSTAT_WRITE)
		return (EIO);

	if (key == NULL)
		return (0); /* paranoid */

	bcopy(key, buf, cachefs_kstat_key_n * sizeof (*key));
	key = buf;
	s = (caddr_t)(key + cachefs_kstat_key_n);

	for (i = 0; i < cachefs_kstat_key_n; i++) {
		k = key + i;

		(void) strcpy(s, (char *)k->ks_mountpoint);
		k->ks_mountpoint = (uint64_t)(s - (uintptr_t)buf);
		s += strlen(s) + 1;
		(void) strcpy(s, (char *)k->ks_backfs);
		k->ks_backfs = (uint64_t)(s - (uintptr_t)buf);
		s += strlen(s) + 1;
		(void) strcpy(s, (char *)k->ks_cachedir);
		k->ks_cachedir = (uint64_t)(s - (uintptr_t)buf);
		s += strlen(s) + 1;
		(void) strcpy(s, (char *)k->ks_cacheid);
		k->ks_cacheid = (uint64_t)(s - (uintptr_t)buf);
		s += strlen(s) + 1;
	}

	return (0);
}

extern void  cachefs_inactivate();

static int
cachefs_unmount(vfs_t *vfsp, int flag, cred_t *cr)
{
	fscache_t *fscp = VFS_TO_FSCACHE(vfsp);
	struct cachefscache *cachep = fscp->fs_cache;
	int error;
	int xx;
	vnode_t *nmvp;
	struct vattr attr;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VFSOP)
		printf("cachefs_unmount: ENTER fscp %p\n", fscp);
#endif

	if (!suser(cr)) {
		error = EPERM;
		goto out;
	}
	/*
	 * forced unmount is not supported by this file system
	 * and thus, ENOTSUP, is being returned.
	 */
	if (flag & MS_FORCE) {
		error = ENOTSUP;
		goto out;
	}
	/* if a log file exists don't allow the unmount */
	if (fscp->fs_dlogfile) {
		error = EBUSY;
		goto out;
	}

	/*
	 * wait for the cache-wide async queue to drain.  Someone
	 * here may be trying to sync our fscache...
	 */
	while (cachefs_async_halt(&fscp->fs_cache->c_workq, 0) == EBUSY) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_VFSOP)
			printf("unmount: waiting for cache async queue...\n");
#endif
	}

	error = cachefs_async_halt(&fscp->fs_workq, 1);
	if (error) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_VFSOP)
			printf("cachefs_unmount: "
			    "cachefs_async_halt error %d\n", error);
#endif
		goto out;
	}

	/*
	 * No active cnodes on this cache && rootvp refcnt == 1
	 */
	mutex_enter(&fscp->fs_fslock);
	xx = fscp->fs_cnodecnt - fscp->fs_idlecnt;
	ASSERT(xx >= 1);
	if (xx > 1 || fscp->fs_rootvp->v_count != 1) {
		mutex_exit(&fscp->fs_fslock);
		error = EBUSY;
		goto out;
	}
	mutex_exit(&fscp->fs_fslock);

	/* get rid of anything on the idle list */
	ASSERT(fscp->fs_idleclean == 0);
	cachefs_cnode_idleclean(fscp, 1);
	if (fscp->fs_cnodecnt > 1) {
		error = EBUSY;
		goto out;
	}

	fscache_hold(fscp);

	/* get rid of the root cnode */
	if (cachefs_cnode_inactive(fscp->fs_rootvp, cr) == EBUSY) {
		fscache_rele(fscp);
		error = EBUSY;
		goto out;
	}

	/* create the file indicating not mounted */
	attr.va_mode = S_IFREG | 0666;
	attr.va_uid = 0;
	attr.va_gid = 0;
	attr.va_type = VREG;
	attr.va_mask = AT_TYPE | AT_MODE | AT_UID | AT_GID;
	if (fscp->fs_fscdirvp != NULL)
		xx = VOP_CREATE(fscp->fs_fscdirvp, CACHEFS_UNMNT_FILE, &attr,
		    NONEXCL, 0600, &nmvp, kcred, 0);
	else
		xx = ENOENT; /* for unmounting when NOCACHE */
	if (xx == 0) {
		VN_RELE(nmvp);
	} else {
		printf("could not create %s %d\n", CACHEFS_UNMNT_FILE, xx);
	}

	ASSERT(fscp->fs_cnodecnt == 0);

	/* sync the file system just in case */
	fscache_sync(fscp);

	/* lock out other unmounts and mount */
	mutex_enter(&cachefs_cachelock);

	/* mark the file system as not mounted */
	mutex_enter(&fscp->fs_fslock);
	fscp->fs_flags &= ~CFS_FS_MOUNTED;
	fscp->fs_rootvp = NULL;
	if (fscp->fs_kstat_id > 0)
		cachefs_kstat_umount(fscp->fs_kstat_id);
	fscp->fs_kstat_id = 0;

	/* drop the inum translation table */
	if (fscp->fs_inum_size > 0) {
		cachefs_kmem_free(fscp->fs_inum_trans,
		    fscp->fs_inum_size * sizeof (cachefs_inum_trans_t));
		fscp->fs_inum_size = 0;
		fscp->fs_inum_trans = NULL;
		fscp->fs_flags &= ~CFS_FS_HASHPRINT;
	}
	mutex_exit(&fscp->fs_fslock);

	fscache_rele(fscp);

	/* get rid of any unused fscache objects */
	mutex_enter(&cachep->c_fslistlock);
	fscache_list_gc(cachep);
	mutex_exit(&cachep->c_fslistlock);

	/* get the number of mounts on this cache */
	mutex_enter(&cachep->c_fslistlock);
	xx = fscache_list_mounted(cachep);
	mutex_exit(&cachep->c_fslistlock);

	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_UMOUNT))
		cachefs_log_umount(cachep, 0, vfsp);

	/* if no mounts left, deactivate the cache */
	if (xx == 0) {
		cachep->c_usage.cu_flags &= ~CUSAGE_ACTIVE;
		(void) cachefs_cache_rssync(cachep);
		cachefs_delete_cachep(cachep);
	}

	cachefs_module_keepcnt--;
	mutex_exit(&cachefs_cachelock);

out:
	if (error) {
		if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_UMOUNT))
			cachefs_log_umount(cachep, error, vfsp);
	}
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VFSOP)
		printf("cachefs_unmount: EXIT\n");
#endif
	return (error);
}

/*
 * remove the cache from the list of caches
 */

static void
cachefs_delete_cachep(cachefscache_t *cachep)
{
	struct cachefscache **cachepp;
	int found = 0;

	ASSERT(MUTEX_HELD(&cachefs_cachelock));

	for (cachepp = &cachefs_cachelist;
	    *cachepp != NULL;
	    cachepp = &(*cachepp)->c_next) {
		if (*cachepp == cachep) {
			*cachepp = cachep->c_next;
			found++;
			break;
		}
	}
	ASSERT(found);

	/* shut down the cache */
	cachefs_cache_destroy(cachep);
}

static int
cachefs_root(vfs_t *vfsp, vnode_t **vpp)
{
	/*LINTED alignment okay*/
	struct fscache *fscp = (struct fscache *)vfsp->vfs_data;

	ASSERT(fscp != NULL);
	ASSERT(fscp->fs_rootvp != NULL);
	*vpp = fscp->fs_rootvp;
	VN_HOLD(*vpp);
	return (0);
}

/*
 * Get file system statistics.
 */
static int
cachefs_statvfs(register vfs_t *vfsp, struct statvfs64 *sbp)
{
	struct fscache *fscp = VFS_TO_FSCACHE(vfsp);
	struct cache_label *lp = &fscp->fs_cache->c_label;
	struct cache_usage *up = &fscp->fs_cache->c_usage;
	int error;

	error = cachefs_cd_access(fscp, 0, 0);
	if (error)
		return (error);

	if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
		/*
		 * When connected return backfs stats
		 */
		error = VFS_STATVFS(fscp->fs_backvfsp, sbp);
	} else {
		/*
		 * Otherwise, just return the frontfs stats
		 */
		error = VFS_STATVFS(fscp->fs_fscdirvp->v_vfsp, sbp);
		if (!error) {
			dev32_t	d32;

			sbp->f_frsize = MAXBSIZE;
			sbp->f_blocks = lp->cl_maxblks;
			sbp->f_bfree = sbp->f_bavail =
			    lp->cl_maxblks - up->cu_blksused;
			sbp->f_files = lp->cl_maxinodes;
			sbp->f_ffree = sbp->f_favail =
			    lp->cl_maxinodes - up->cu_filesused;
			(void) cmpldev(&d32, vfsp->vfs_dev);
			sbp->f_fsid = d32;
		}
	}
	cachefs_cd_release(fscp);
	if (error)
		return (error);

	/*
	 * Make sure fstype is CFS.
	 */
	(void) strcpy(sbp->f_basetype, vfssw[vfsp->vfs_fstype].vsw_name);
	bzero(sbp->f_fstr, sizeof (sbp->f_fstr));

	return (0);
}

/*
 * queue a request to sync the given fscache
 */
static void
queue_sync(struct cachefscache *cachep, cred_t *cr)
{
	struct cachefs_req *rp;
	int error;

	rp = kmem_cache_alloc(cachefs_req_cache, KM_SLEEP);
	rp->cfs_cmd = CFS_CACHE_SYNC;
	rp->cfs_cr = cr;
	rp->cfs_req_u.cu_fs_sync.cf_cachep = cachep;
	crhold(rp->cfs_cr);
	error = cachefs_addqueue(rp, &cachep->c_workq);
	ASSERT(error == 0);
}

/*ARGSUSED*/
static int
cachefs_sync(vfs_t *vfsp, short flag, cred_t *cr)
{
	struct fscache *fscp;
	struct cachefscache *cachep;

	if (!(flag & SYNC_ATTR)) {
		/*
		 * queue an async request to do the sync.
		 * We always sync an entire cache (as opposed to an
		 * individual fscache) so that we have an opportunity
		 * to set the clean flag.
		 */
		if (vfsp) {
			/*LINTED alignment okay*/
			fscp = (struct fscache *)vfsp->vfs_data;
			queue_sync(fscp->fs_cache, cr);
		} else {
			mutex_enter(&cachefs_cachelock);
			for (cachep = cachefs_cachelist; cachep != NULL;
			    cachep = cachep->c_next) {
				queue_sync(cachep, cr);
			}
			mutex_exit(&cachefs_cachelock);
		}
	}
	return (0);
}

struct vfs *cachefs_frontrootvfsp;	/* XXX: kluge for convert_mount */

static int
cachefs_mountroot_init(struct vfs *vfsp)
{
	char *backfsdev, *front_dev, *front_fs;
	struct vfs *frontvfsp, *backvfsp;
	struct vnode *frontrootvp, *backrootvp, *cachedirvp;
	dev_t mydev;
	struct vfssw *fvsw, *bvsw;
	struct vattr va;
	cachefscache_t *cachep;
	fscache_t *fscp;
	cnode_t *rootcp;
	struct fid cookie;
	ino64_t fsid;
	int error = 0;
	int cnflag;
	int foundcache = 0;
	cfs_cid_t cid;
	u_long maxfilesizebits;

	cnflag = CN_ROOT;
	backfsdev = backfs.bo_name;
	front_dev = frontfs.bo_name;
	front_fs = frontfs.bo_fstype;

	frontvfsp = NULL;
	backvfsp = NULL;
	fscp = NULL;
	cachep = NULL;

	/*
	 * The rule here is as follows:
	 * If no `-f' flag and frontfs.bo_name is not null,
	 *	frontfs.bo_name and frontfs.bo_fstype are valid
	 * else
	 *	we don't have a frontfs
	 */
	if ((boothowto & RB_FLUSHCACHE) || (*front_dev == '\0')) {
		/*
		 * we don't have a cache.  Fire up CFS
		 * in "all-miss/no-fill" mode for now.
		 * Some time later, bcheckrc will give us
		 * a front filesystem and remount root as
		 * a writeable fs.
		 */
		cnflag |= CN_NOCACHE;
	} else {
		/*
		 * booted off the frontfs.  Here we use the
		 * cache in "read-only/no-fill" mode until
		 * root is remounted after cachefs_fsck runs.
		 *
		 * XXX: we assume that frontfs is always UFS for now.
		 */
		ASSERT(front_dev[0] != '\0');
		frontvfsp = cachefs_kmem_zalloc(sizeof (struct vfs), KM_SLEEP);
		RLOCK_VFSSW();
		fvsw = vfs_getvfsswbyname(front_fs);
		if (fvsw) {
			VFS_INIT(frontvfsp, fvsw->vsw_vfsops, NULL);
			error = VFS_MOUNTROOT(frontvfsp, ROOT_FRONTMOUNT);
		} else {
			error = ENOTTY;
		}
		RUNLOCK_VFSSW();
		if (error)
			goto out;
	}

	backvfsp = cachefs_kmem_zalloc(sizeof (struct vfs), KM_SLEEP);
	RLOCK_VFSSW();
	bvsw = vfs_getvfsswbyname(backfs.bo_fstype);
	if (bvsw) {
		(void) strcpy(rootfs.bo_name, backfsdev);
		VFS_INIT(backvfsp, bvsw->vsw_vfsops, NULL);
		error = VFS_MOUNTROOT(backvfsp, ROOT_BACKMOUNT);
	} else {
		error = ENOTTY;
	}
	RUNLOCK_VFSSW();
	if (error)
		goto out;

	error = VFS_ROOT(backvfsp, &backrootvp);
	if (error)
		goto out;

	error = cachefs_getcookie(backrootvp, &cookie, &va, kcred);
	if (error)
		goto out;

	cachep = NULL;
	if (frontvfsp && (VFS_ROOT(frontvfsp, &frontrootvp) == 0)) {
		/* create a cache object in NOCACHE and NOFILL mode */
		cachep = cachefs_cache_create();
		if (cachep == NULL) {
			error = ENOMEM;
			goto out;
		}

		/*
		 * find cache info from front fs and call
		 * activate_cache.
		 *
		 * For now we insist on the convention of
		 * finding the root cache at "/rootcache"
		 * in the frontfs.
		 */
		error = VOP_LOOKUP(frontrootvp, "rootcache",
		    &cachedirvp, (struct pathname *)NULL, 0,
		    (vnode_t *)NULL, kcred);
		VN_RELE(frontrootvp);
		if (error) {
			printf("can't find '/rootcache' in frontfs!\n");
			goto done;
		} else if (cachedirvp->v_type != VDIR) {
			printf("cachefs_mountroot:/rootcache not a dir!\n");
			goto done;
		}

		/* take the cache out of NOCACHE mode, leave in NOFILL */
		error = cachefs_cache_activate_ro(cachep, cachedirvp);
		if (error)
			goto done;

		/* get the fsid for the fscache */
		error = fscache_name_to_fsid(cachep, "rootcache", &fsid);
		if (error)
			goto done;

		/* create the fscache object */
		fscp = fscache_create(cachep);

		mutex_enter(&cachep->c_fslistlock);
		error = fscache_activate(fscp, fsid, NULL, NULL, 0);
		if (error) {
			mutex_exit(&cachep->c_fslistlock);
			fscache_destroy(fscp);
			fscp = NULL;
			goto done;
		}
		fscache_list_add(cachep, fscp);
		mutex_exit(&cachep->c_fslistlock);
		foundcache = 1;
		printf("cachefs: booting from a clean cache.\n");
	}
done:

	if (!foundcache) {
		/* destroy the old cache object if it exists */
		if (cachep)
			cachefs_cache_destroy(cachep);

		/* create a cache object in NOCACHE and NOFILL mode */
		cachep = cachefs_cache_create();
		if (cachep == NULL) {
			error = ENOMEM;
			goto out;
		}

		/* create the fscache object and put in on the list */
		fscp = fscache_create(cachep);
		mutex_enter(&cachep->c_fslistlock);
		fscache_list_add(cachep, fscp);
		mutex_exit(&cachep->c_fslistlock);
	}

	/* make up a set of options for root mounting */
	fscp->fs_info.fi_mntflags = CFS_NONSHARED | CFS_CODCONST_MODE;
	fscp->fs_info.fi_mntflags |= CFS_LLOCK;
	fscp->fs_info.fi_root = va.va_nodeid;
	fscp->fs_flags |= CFS_FS_DIRTYINFO;

	/* mark file system as mounted */
	error = fscache_mounted(fscp, vfsp, backvfsp);
	ASSERT(error == 0);

	vfsp->vfs_data = (caddr_t)fscp;
	mydev = cachefs_mkmntdev();
#ifdef _LP64
	/*
	 * XX64: It's not a good idea to make fsid bigger since that'll
	 * have adverse effects on nfs filehandles.  For now assume that
	 * cachefs be used on devices that fit into dev32_t's.
	 */
	if (mydev == NODEV) {
		error = EOVERFLOW;
		goto out;
	}
#endif
	vfsp->vfs_dev = mydev;
	vfs_make_fsid(&vfsp->vfs_fsid, mydev, cachefsfstyp);
	vfsp->vfs_fstype = cachefsfstyp;
	if (backvfsp)
		vfsp->vfs_bsize = backvfsp->vfs_bsize;
	else
		vfsp->vfs_bsize = MAXBSIZE;
	cid.cid_fileno = va.va_nodeid;
	cid.cid_flags = 0;
	error = cachefs_cnode_make(&cid, fscp, &cookie,
	    &va, backrootvp, kcred, cnflag, &rootcp);
	if (error)
		goto out;

	mutex_enter(&fscp->fs_fslock);
	fscp->fs_rootvp = CTOV(rootcp);
	fscp->fs_rootvp->v_flag |= VROOT;
	fscp->fs_rootvp->v_type |= rootcp->c_attr.va_type;
	ASSERT(fscp->fs_rootvp->v_type == VDIR);
	fscp->fs_flags |= CFS_FS_ROOTFS;
	/*
	 * Get the maxfilesize bits of the back file system.
	 */

	error = VOP_PATHCONF(backrootvp, _PC_FILESIZEBITS, &maxfilesizebits,
		    kcred);

	if (error) {
		cmn_err(CE_WARN,
	"cachefs_mount: Can't get the FILESIZEBITS of the back root vnode \n");
		goto out;
	}

	fscp->fs_offmax = (1LL << (maxfilesizebits - 1)) - 1;
	mutex_exit(&fscp->fs_fslock);

out:
	/* XXX bob: shouldn't we vn_rele backrootvp? */
	if (error) {
		/*
		 * XXX: do we need to unmount and stuff?  We're
		 * just going to reboot anyway aren't we?
		 */
		if (frontvfsp)
			cachefs_kmem_free(frontvfsp, sizeof (struct vfs));
		if (backvfsp)
			cachefs_kmem_free(backvfsp, sizeof (struct vfs));
		if (fscp) {
			mutex_enter(&cachep->c_fslistlock);
			fscache_list_remove(cachep, fscp);
			mutex_exit(&cachep->c_fslistlock);
			fscache_destroy(fscp);
		}
		if (cachep) {
			cachefs_cache_destroy(cachep);
		}
	} else {
		cachefs_cachelist = cachep;
		cachefs_frontrootvfsp = frontvfsp;
		cachefs_module_keepcnt++;
	}
	return (error);
}

static int
cachefs_mountroot_unmount(vfs_t *vfsp)
{
	fscache_t *fscp = VFS_TO_FSCACHE(vfsp);
	struct cachefscache *cachep = fscp->fs_cache;
	struct vfs *f_vfsp;
	int error = 0;
	clock_t tend;

	/*
	 * Try to write the cache clean flag now in case we do not
	 * get another chance.
	 */
	cachefs_cache_sync(cachep);

	/* wait for async threads to stop */
	while (fscp->fs_workq.wq_thread_count > 0)
		(void) cachefs_async_halt(&fscp->fs_workq, 1);
	while (cachep->c_workq.wq_thread_count > 0)
		(void) cachefs_async_halt(&cachep->c_workq, 1);

	/* kill off the garbage collection thread */
	mutex_enter(&cachep->c_contentslock);
	while (cachep->c_flags & CACHE_CACHEW_THREADRUN) {
		cachep->c_flags |= CACHE_CACHEW_THREADEXIT;
		cv_signal(&cachep->c_cwcv);
		tend = lbolt + (60 * hz);
		(void) cv_timedwait(&cachep->c_cwhaltcv,
			&cachep->c_contentslock, tend);
	}
	mutex_exit(&cachep->c_contentslock);

	/* if the cache is still dirty, try to make it clean */
	if (cachep->c_usage.cu_flags & CUSAGE_ACTIVE) {
		cachefs_cache_sync(cachep);
		if (cachep->c_usage.cu_flags & CUSAGE_ACTIVE) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_VFSOP)
				printf("cachefs root: cache is dirty\n");
#endif
			error = 1;
		}
#ifdef CFSDEBUG
		else {
			CFS_DEBUG(CFSDEBUG_VFSOP)
				printf("cachefs root: cache is clean\n");
		}
#endif
	}
#ifdef CFSDEBUG
	else {
		CFS_DEBUG(CFSDEBUG_VFSOP)
			printf("cachefs root: cache is pristine\n");
	}
#endif

	if (cachep->c_resfilevp) {
		f_vfsp = cachep->c_resfilevp->v_vfsp;
		VFS_SYNC(f_vfsp, 0, kcred);
		VFS_SYNC(f_vfsp, 0, kcred);
		VFS_SYNC(f_vfsp, 0, kcred);
	}
	cachefs_module_keepcnt--;
	return (error);
}

/*ARGSUSED*/
static int
cachefs_mountroot(vfs_t *vfsp, whymountroot_t why)
{
	int error;

	switch (why) {
	case ROOT_INIT:
		error = cachefs_mountroot_init(vfsp);
		break;
	case ROOT_UNMOUNT:
		error = cachefs_mountroot_unmount(vfsp);
		break;
	default:
		error = ENOSYS;
		break;
	}
	return (error);
}

/*ARGSUSED*/
static int
cachefs_swapvp(vfs_t *vfsp, vnode_t **vpp, char *nm)
{
	return (ENOSYS);
}

static int
cachefs_remount(struct vfs *vfsp, struct mounta *uap)
{
	fscache_t *fscp = VFS_TO_FSCACHE(vfsp);
	cachefscache_t *cachep = fscp->fs_cache;
	int error = 0;
	STRUCT_DECL(cachefs_mountargs, map);
	struct cachefsoptions	*cfs_options;
	char			*backfs, *cacheid, *cachedir;
	struct vnode *cachedirvp = NULL;
	ino64_t fsid;
	vnode_t *backrootvp = NULL;
	struct vnode *tmpdirvp = NULL;

	STRUCT_INIT(map, get_udatamodel());
	error = copyin(uap->dataptr, STRUCT_BUF(map),
			SIZEOF_STRUCT(cachefs_mountargs, DATAMODEL_NATIVE));
	if (error)
		goto out;

	/*
	 * get cache directory vp
	 */
	cachedir = (char *)STRUCT_FGETP(map, cfs_cachedir);
	error = lookupname(cachedir, UIO_USERSPACE, FOLLOW,
	    NULLVPP, &cachedirvp);
	if (error)
		goto out;
	if (cachedirvp->v_type != VDIR) {
		error = EINVAL;
		goto out;
	}

	error = 0;
	if (cachedirvp) {
		error = VOP_LOOKUP(cachedirvp, CACHEFS_DLOG_FILE,
		    &tmpdirvp, NULL, 0, NULL, kcred);
	}
	cfs_options = (struct cachefsoptions *)STRUCT_FADDR(map, cfs_options);
	cacheid = (char *)STRUCT_FGETP(map, cfs_cacheid);
/* XXX not quite right */
#if 0
	/*
	 * If a log file exists and the cache is being mounted without
	 * the snr (aka disconnectable) option, return an error.
	 */
	if ((error == 0) &&
	    !(cfs_options->opt_flags & CFS_DISCONNECTABLE)) {
		cmn_err(CE_WARN,
		    "cachefs_mount: log exists and disconnectable"
		    "option not specified\n");
		error = EINVAL;
		goto out;
	}
#endif
	error = 0;

	/* XXX need mount options "nocache" and "nofill" */

	/* if nocache is being turned off */
	if (cachep->c_flags & CACHE_NOCACHE) {
		error = cachefs_cache_activate_ro(cachep, cachedirvp);
		if (error)
			goto out;
		cachefs_cache_activate_rw(cachep);

		/* get the fsid for the fscache */
		error = fscache_name_to_fsid(cachep, cacheid, &fsid);
		if (error)
			fsid = 0;

		/* activate the fscache */
		mutex_enter(&cachep->c_fslistlock);
		error = fscache_enable(fscp, fsid, cacheid,
			cfs_options, fscp->fs_info.fi_root);
		mutex_exit(&cachep->c_fslistlock);
		if (error) {
			cmn_err(CE_WARN, "cachefs: cannot remount %s\n",
				cacheid);
			goto out;
		}

		/* enable the cache */
		cachefs_enable_caching(fscp);
		fscache_activate_rw(fscp);
	}

	/* else if nofill is being turn off */
	else if (cachep->c_flags & CACHE_NOFILL) {
		ASSERT(cachep->c_flags & CACHE_NOFILL);
		cachefs_cache_activate_rw(cachep);

		/* enable the cache */
		cachefs_enable_caching(fscp);
		fscache_activate_rw(fscp);
	}

	fscache_acset(fscp, STRUCT_FGET(map, cfs_acregmin),
	    STRUCT_FGET(map, cfs_acregmax),
	    STRUCT_FGET(map, cfs_acdirmin), STRUCT_FGET(map, cfs_acdirmax));

	/* if the backfs is mounted now or we have a new backfs */
	backfs = (char *)STRUCT_FGETP(map, cfs_backfs);
	if (backfs && (cfs_options->opt_flags & CFS_SLIDE)) {
		/* get the back file system root vp */
		error = lookupname(backfs, UIO_USERSPACE, FOLLOW,
			NULLVPP, &backrootvp);
		if (error)
			goto out;

		/*
		 * Make sure the thing we just looked up is a directory
		 * and a root of a file system
		 */
		if (backrootvp->v_type != VDIR ||
		    !(backrootvp->v_flag & VROOT)) {
			cmn_err(CE_WARN,
			    "cachefs_mount: backpath not a directory\n");
			error = EINVAL;
			goto out;
		}

		/*
		 * XXX
		 * Kind of dangerous to just set this but we do
		 * not have locks around usage of fs_backvfsp.
		 * Hope for the best for now.
		 * Probably should also spin through vnodes and fix them up.
		 * Krishna - fixed c_backvp to reflect the change.
		 */
		fscp->fs_backvfsp = backrootvp->v_vfsp;
		((cnode_t *)(fscp->fs_rootvp->v_data))->c_backvp = backrootvp;
	}

	if (fscp->fs_kstat_id > 0)
		cachefs_kstat_umount(fscp->fs_kstat_id);
	fscp->fs_kstat_id = 0;
	cachefs_kstat_mount(fscp, uap->dir, backfs, cachedir, cacheid);

	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_MOUNT))
		cachefs_log_mount(cachep, error, vfsp, fscp,
		    uap->dir, UIO_USERSPACE,
		    (STRUCT_BUF(map) != NULL) ? cacheid : NULL);

out:
	if (cachedirvp)
		VN_RELE(cachedirvp);
	if (backrootvp)
		VN_RELE(backrootvp);
	return (error);
}
