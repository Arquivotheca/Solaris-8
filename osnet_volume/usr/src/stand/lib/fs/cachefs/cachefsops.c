/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)cachefsops.c	1.14	97/06/30 SMI"

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/fs/cachefs_fs.h>
#include <sys/fs/cachefs_dir.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/bootdebug.h>
#include <sys/bootcfs.h>
#include <sys/salib.h>
#include <sys/sacache.h>

/*
 * Cachefs block cache structure
 * Cache blocks of recently used cfs data through this structure
 * using the set_cfsb/get_cfsb cache routines.
 */
typedef struct c_dirinfo {
	int	dir_type;	/* see CDIR_* definition */
	int	dir_fd;		/* file descriptor */
	ino64_t	dir_fileno;	/* file number (in cachefs) */
	off_t	dir_loc;	/* offset in the directory file */
	int	dir_off;	/* offset in block (next to use) */
	char	*dir_buf;	/* one directory block, or symlink contents */
	int	dir_size;	/* amount of valid data in dir_buf */
} CDIR;

/*
 * The front (UFS) file system slice (/) has some non-cachefs
 *	files and directories: (minimally) platform/`uname -i`/ufsboot,
 *	.cachefsinfo, and lost+found.
 * Under ROOTCACHE, a cachefs file system resides here with
 *	.cfs_label, .cfs_resource, and some (symlink) cacheid
 *	directories.
 * Under ROOTCACHEID, the root cachefs resides.  In here,
 *	you can find .cfs_fsinfo file and .cfs_attrcache directory.
 */

struct boot_fs_ops *frontfs_ops = NULL;
struct boot_fs_ops *backfs_ops = NULL;

static struct cachefs_fsinfo fsinfo;
static struct cache_label lab;
static struct cache_usage usage;

static char *cachedir = ROOTCACHE;
static char *cacheid = ROOTCACHEID;
static ino64_t cachefsrootino = 0;
static CDIR *rootdirp;

static int boot_cachefs_nocache = 0;
static int do_cachevalidation = 1;

/*
 * Local Prototypes
 */

#ifdef CFS_OPS_DEBUG
static char	*prt_mdflags(int);
static char	*prt_dirflags(int);
#endif CFS_OPS_DEBUG

static CDIR *cachefs_opendir(ino64_t fileno);
static int cachefs_closedir(CDIR * dirp);
static struct c_dirent	*cachefs_readdir(CDIR * dirp);
static void fnum2cnam(ino64_t fileno, char *cname);
static ino64_t unix2cachefs(char *pathname);
static ino64_t cachefs_dlook(CDIR *dirp, char *pathname);
static int get_lab(char *fscname, struct cache_label *lab);
static int get_fsinfo(char *fscname, struct cachefs_fsinfo *fsinfo);
static int get_usage(char *fscname, struct cache_usage *usage);
static int get_attrc(ino64_t fileno, char **sympp);

/*
 *  Function prototypes	(Global/Exported)
 */
int		boot_cachefs_mountroot(char *str);
static int	boot_cachefs_unmountroot(void);
static int	boot_cachefs_open(char *filename, int flags);
static int	boot_cachefs_close(int fd);
static ssize_t	boot_cachefs_read(int fd, caddr_t buf, size_t size);
static off_t	boot_cachefs_lseek(int, off_t, int);
static int	boot_cachefs_fstat(int fd, struct stat *stp);
static void	boot_cachefs_closeall(int flag);

struct boot_fs_ops boot_cachefs_ops = {
	"cachefs",
	boot_cachefs_mountroot,
	boot_cachefs_unmountroot,
	boot_cachefs_open,
	boot_cachefs_close,
	boot_cachefs_read,
	boot_cachefs_lseek,
	boot_cachefs_fstat,
	boot_cachefs_closeall,
};

/*
 * mount cachefs root filesystem
 *
 * Will first mount the frontfs root, then check the label and the cfs
 * options/usage.  If the "-f" boot option is set
 * or if CUSAGE_ACTIVE is off, we set boot_cachefs_nocache = 1
 * so all files will be read from the backfs.
 *
 * return SUCCESS if cachefs root is mounted.
 * return FAILURE if mountroot() failed.
 */

int
boot_cachefs_mountroot(char *str)
{
	static int	cachefs_mountroot_done = 0;

	OPS_DEBUG_CK(("boot_cachefs_mountroot(): mountroot_done = %d\n",
			cachefs_mountroot_done));

	/*
	 * cachefs_mountroot is called exactly twice, once to read the
	 * .cachefsinfo data, and once after the back fileystem has been
	 * mounted.
	 */
	if (cachefs_mountroot_done)
		goto openrootino;

	if ((*frontfs_ops->fsw_mountroot)(str) != 0) {
		printf("boot_cachefs_mountroot: ufs_mountroot(%s) failed\n",
		    str);
		return (FAILURE);
	}
	cachefs_mountroot_done++;

	/* Check the cachefs label for the correct version */
	if (get_lab(cachedir, &lab) != SUCCESS) {
		OPS_DEBUG(("boot_cachefs_mountroot(): "
			"unable to get CFS label.\n"));
		return (FAILURE);
	}

	/* get cachefs information */
	if (get_fsinfo(cacheid, &fsinfo) != SUCCESS) {
		OPS_DEBUG(("boot_cachefs_mountroot(): "
			"unable to get CFS option.\n"));
		return (FAILURE);
	}

	/* Check the usage info to see if cache is clean (and safe to use) */
	if (get_usage(cachedir, &usage) != SUCCESS) {
		OPS_DEBUG(("boot_cachefs_mountroot(): "
			"unable to get CFS usage.\n"));
		return (FAILURE);
	}

	if (usage.cu_flags & CUSAGE_ACTIVE) {
		OPS_DEBUG_CK(("boot_cachefs_mountroot(): CUSAGE_ACTIVE set..."
				"cache-wide nocache on\n"));
		boot_cachefs_nocache = 1;
	}

	/* Set "Don't-Use-Cache" option if doing a "boot -f" */
	if (boothowto & RB_FLUSHCACHE) {
		OPS_DEBUG_CK(("boot_cachefs_mountroot(): -f option set..."
				"boot from network with clean cache\n"));
		boot_cachefs_nocache = 1;
	}

	/*
	 * end of first mount - OK so far
	 */

	return (SUCCESS);

openrootino:

	if (cachefs_mountroot_done == 2)
		return (SUCCESS);

	/*
	 * get the inode # for the client's root
	 * If the client's root inode has changed, cachefs_opendir()
	 * will fail.
	 */
	cachefsrootino = (ino64_t)get_root_ino();
	if ((rootdirp = cachefs_opendir(cachefsrootino)) == NULL) {
		OPS_DEBUG(("boot_cachefs_mountroot(): "
			"cachefs_opendir(%llX) failed.\n", cachefsrootino));
		/* should already be set, but just in case... */
		boot_cachefs_nocache = 1;
	}

	/*
	 * We keep root directory open at all time, it is touched so often.
	 * We rely on ufs_closeall to go through a different list to close it.
	 */
	cachefs_mountroot_done++;

	return (SUCCESS);
}

/*
 * Unmount the root fs -- not supported for this fstype.
 */

int
boot_cachefs_unmountroot(void)
{
	return (-1);
}

/*
 * Open a file under cachefs.  If file is not in cache (not in frontfs), it
 * will try to open through backfs.  If that fails also, the filename is
 * added to the negative filename list.
 *
 * To speed up file access, the filename is checked against the negative
 * filename list (list of files that known to be bogus or not in both frontfs
 * and backfs).  Also, we don't allow open of "/usr/..." in boot.
 *
 * return fd on successful open of a frontfs file.
 * return fd+BACKFS_FD_OFFSET on successful open of a backfs file (so we
 * can always tell a backfs fd from a frontfs fd.
 *
 */

/* ARGSUSED */
static int
boot_cachefs_open(char *filename, int flags)
{
	ino64_t	fileno;
	int	fd;
	char	cname[MAXPATHLEN];

	/* for boot, no /usr/kernel/... allowed */
	if (strncmp(filename, "/usr/", 5) == 0)
		return (FAILURE);

	/* check negative filename list */
	if ((get_negative_filename(CACHEFSDEV, filename)) != NULL) {
		return (FAILURE);
	}

	if ((boot_cachefs_nocache) ||
	    ((fileno = unix2cachefs(filename)) == 0)) {
		OPS_DEBUG_CK(("cachefs_open(): unix2cachefs(%s) failed.\n",
			filename));
		goto read_backfs;
	}
	fnum2cnam(fileno, cname);

	if ((fd = (*frontfs_ops->fsw_open)(cname, O_RDONLY)) < 0) {
		OPS_DEBUG_CK(("cachefs_open(%s, %s) frontfs failed.\n",
			filename, cname));
		goto read_backfs;
	}

	OPS_DEBUG_CK(("cachefs_open(%s): fileno=%llX cname=%s fd=%d "
			"returned\n", filename, fileno, cname, fd));
	return (fd);

	/*
	 * we didn't find the file in the local cache, read it from
	 * the back filesystem
	 */
read_backfs:
	if ((fd = (*backfs_ops->fsw_open)(filename, flags)) < 0) {
		OPS_DEBUG_CK(("cachefs_open(): %s (backfs failed)\n",
			filename));

		/* add to the negative filename list */
		set_negative_filename(CACHEFSDEV, filename);
		return (FAILURE);
	}

	OPS_DEBUG_CK(("cachefs_open(%s) fd=%d, cache miss filled "
			"through backFS\n", filename, fd));

	return (fd + BACKFS_FD_OFFSET);
}

/*
 * For a given (inode number) fileno, construct the name by
 * which it is known to the cache front filesystem
 * (fscache/filegrp/file).
 */
static void
fnum2cnam(ino64_t fileno, char *cname)
{
	ino64_t	fgrpno;

	fgrpno = (fileno / fsinfo.fi_fgsize) * fsinfo.fi_fgsize;
	(void) sprintf(cname, "%s/%016llX/%016llX", cacheid, fgrpno, fileno);
}

/*
 * Look up given directory path and return a pointer for a hit in cache.
 * Return inode of directory pointed to by cache
 * Return root inode directory if not in cache.
 * Return NULL only when opendir of a hit failed.
 */
static CDIR *
lookup_dncache(char **pathpp, ino64_t *filenoptr)
{
	char	*q;
	char	savec;
	CDIR	*dirp;

	/* exclude the last component */
	if ((q = strrchr(*pathpp, '/')) != NULL) {
		savec = *q;
		*q = '\0';
	}

	if ((q != NULL) && (q != *pathpp) &&
		    (*filenoptr = get_dnlc(CACHEFSDEV, (void *)*pathpp))) {
		dirp = cachefs_opendir(*filenoptr);
		if (dirp == NULL && ((boothowto & DBFLAGS) == DBFLAGS)) {
			/* no symlink expected */
			OPS_DEBUG_CK(("lookup_dncache(): cachefs_opendir(%llx)"
				" failed.\n", *filenoptr));
			return (NULL);
		}
		OPS_DEBUG_CK(("lookup_dncache(): <%s> in DNLC.\n", *pathpp));
		*pathpp = q;
		*q = savec;
	} else {
		/* not in cache, get rootdir */
		if (q != NULL)
			*q = savec;
		/* root is always open, we have to reset buffer and loc */
		dirp = rootdirp;
		dirp->dir_loc = 0;
		dirp->dir_size = 0;
		dirp->dir_off = 0;
		*filenoptr = cachefsrootino;
	}

	return (dirp);
}

/*
 * Add an entry to the DNLC cache
 */
static void
add_dncache(char *pathbuf, ino64_t inode)
{
	char	c;
	char 	*q;

	if ((q = strrchr(pathbuf, '/')) != NULL) {
		c = *q;
		*q = '\0';
		if (q != pathbuf) {
			set_dnlc(CACHEFSDEV, pathbuf, inode);
			OPS_DEBUG_CK(("add_dncache(%s): added\n", pathbuf));
		}
		/*
		 * restore the "/" in pathbuf
		 */
		*q = c;
	}
}

/*
 * Look up a filename in the UNIX name space and convert it
 * to a filename the CFS name space (an inode number)
 *
 * return (ino_t)0 if failed.
 * return inode number == fileno  if successful
 */
static ino64_t
unix2cachefs(char *pathname)
{
	char  		*q;
	char		savec;
	ino64_t		last_fileno;
	ino64_t		fileno;
	int		in_dnlc = 0;
	char		lpath[MAXPATHLEN];
	char		*lpathp = lpath;
	CDIR		*dirp;

	if (pathname == NULL || *pathname == '\0') {
		printf("unix2cachefs(): null pathname\n");
		return (0);
	}

	OPS_DEBUG_CK(("unix2cachefs(): <%s>\n", pathname));

	(void) strcpy(lpath, pathname);

	while (*lpathp) {
		/*
		 * look in the DNLC the cache first if we are at
		 * the beginning of the pathname
		 */
		if (lpathp == lpath) {
			dirp = lookup_dncache(&lpathp, &fileno);
			if (dirp == rootdirp)
				in_dnlc = 0;
			else
				in_dnlc = 1;
		}
		last_fileno = fileno;

		while (*lpathp == '/')
			lpathp++;	/* skip leading slashes */
		q = lpathp;
		while (*q != '/' && *q != '\0')
			q++;	/* find end of component */
		savec = *q;
		*q = '\0';	/* terminate component */

		/*
		 * Once reaching the end, if lpathp is not in the DNLC, add it.
		 * Note that the directory has already passed the
		 * cachefs_opendir() test on last while-loop pass.
		 */
		if ((savec == '\0') && (in_dnlc == 0)) {
			/* use original pathname */
			add_dncache(pathname, last_fileno);
		}

		if ((fileno = cachefs_dlook(dirp, lpathp)) != 0) {
			CDIR	*newdirp;
			char	*symp;
			int	len;

			if ((newdirp = cachefs_opendir(fileno)) == NULL) {
				OPS_DEBUG_CK(("unix2cachefs():cachefs_opendir"
						"(%x) failed.\n", fileno));
				cachefs_closedir(dirp);
				return (0);
			}

			if (newdirp->dir_type == CDIR_SYMLINKENT) {
					symp = newdirp->dir_buf;
					len = strlen(symp);
					if (symp[0] == '/') /* absolute link */
						lpathp = lpath;
					/*
					 * copy unprocessed path
					 * (overlapped copy up or down)
					 */
					bcopy(q, (char *)lpathp+len,
					    strlen(q+1) + 2);

					*(lpathp+len) = savec;

					/*
					 * prepend link before
					 * the unprocessed path.
					 */
					bcopy(symp, lpathp, len);
					/* directory search: start over */
					if (lpathp != lpath) {
						dirp->dir_loc = 0;
						dirp->dir_size = 0;
					} else
						cachefs_closedir(dirp);
					continue;
			}

			if (cachefs_closedir(dirp) == -1) {
				printf("unix2cachefs(): cachefs_closedir(%s)"
					" failed.\n", lpathp);
				return (0);
			}
			dirp = newdirp;
			*q = savec;
			lpathp = q;
			continue;
		} else {
			/* cannot find the name in the directory */
			cachefs_closedir(dirp);
			return (0);
		}
	}
	cachefs_closedir(dirp);
	return (fileno);
}

/*
 * directory lookup using parent directory information *dirp and
 *	"dirname" (component).
 *
 * return (ino64_t)0	if failed.
 * return inode number of directory from cache if successful
 */
static ino64_t
cachefs_dlook(CDIR *dirp, char *dirname)
{
	struct c_dirent *ep;
	int    len;

	if (dirname == NULL || *dirname == '\0')
		return (0);

	if (dirp->dir_type != CDIR_DIRENT) {
		printf("cachefs_dlook(%s): Not a directory.\n", dirname);
		return (0);
	}

	len = strlen(dirname);

	for (ep = cachefs_readdir(dirp); ep != NULL;
	    ep = cachefs_readdir(dirp)) {
		if ((ep->d_namelen == 1 && ep->d_name[0] == '.') ||
		    (ep->d_namelen == 2 && strncmp(ep->d_name, "..", 2) == 0))
			continue;
		if (ep->d_namelen == len && strcmp(dirname, ep->d_name) == 0) {
			if ((ep->d_flag & CDE_VALID) == CDE_VALID) {
				return (ep->d_id.cid_fileno);
			} else {
				OPS_DEBUG_CK(("cachefs_dlook(): %s:flags=%s\n",
					ep->d_name, prt_dirflags(ep->d_flag)));
				return (0);
			}
		}
	}
	return (0);
}

/*
 * Open a actual directory/file from the cache (frontfs).  If the fileno
 * given is pointing to a symbolic link, the symlink is returned instead.
 *
 * Note that the parameter fileno can be a directory, symlink, or a regular
 * file.  Call fnum2cnam() to do the fileno (inode number) translation
 *
 * return CDIR entry of symbolic link, directory or file if SUCCESSFUL
 * return NULL if not found or out of memory
 */
static CDIR *
cachefs_opendir(ino64_t fileno)
{
	int		fd;
	CDIR		*dirp;
	int		dtype;
	static char	namebuf[MAXPATHLEN];
	char		*symp;

	symp = NULL;

	if ((dtype = get_attrc(fileno, &symp)) == CDIR_INVALID) {
		OPS_DEBUG_CK(("cachefs_opendir(): get_attrc(%llX) failed.\n",
			fileno));
		return (NULL);
	}

	/* It's a symbolic link */
	if (symp != NULL) {
		if ((dirp = (CDIR *)bkmem_alloc(sizeof (CDIR))) == NULL) {
			printf("cachefs_opendir(): bkmem_alloc() failed.\n");
			return (NULL);
		}
		dirp->dir_type = CDIR_SYMLINKENT;
		dirp->dir_buf = symp;

		OPS_DEBUG_CK(("cachefs_opendir(%llX, %s) SYM returned\n",
			fileno, symp));
		return (dirp);
	}

	/*
	 * convert the file number to the filename in the front filesystem
	 */
	fnum2cnam(fileno, namebuf);

	if ((fd = (*frontfs_ops->fsw_open)(namebuf, O_RDONLY)) == -1) {
		printf("cachefs_opendir(%s) failed.\n", namebuf);
		return (NULL);
	}

	if ((dirp = (CDIR *)bkmem_alloc(sizeof (CDIR) + CDIRBUFSIZE)) ==
	    NULL) {
		printf("cachefs_opendir(): bkmem_alloc() failed.\n");
		return (NULL);
	}
	dirp->dir_fd = fd;
	dirp->dir_fileno = fileno;
	dirp->dir_loc = 0;
	dirp->dir_buf = (char *)dirp + sizeof (CDIR);
	dirp->dir_type = dtype;
	dirp->dir_off = 0;
	dirp->dir_size = 0;	/* need to fill */

	OPS_DEBUG_CK(("cachefs_opendir(%s) returned\n", namebuf));

	return (dirp);
}

/*
 * Fill in the request CDIR entry.
 * Use get_cfsb() to locate the first block
 *
 * return SUCCESS the requested block is available
 * return FAILURE otherwise
 */
static int
get_dirblock(CDIR *dirp)
{
	char *p;

	if (dirp->dir_loc == 0) {	/* right now, only the first block */
		p = get_cfsb(CACHEFSDEV, dirp->dir_fileno, CDIRBUFSIZE);
		if (p != NULL) {
			bcopy(p, (char *)dirp->dir_buf, CDIRBUFSIZE);
			dirp->dir_size = CDIRBUFSIZE;
			return (SUCCESS);
		}
	}
	return (FAILURE);
}


/*
 * Given a CDIR structure, return the next c_dirent entry from
 * the cache/frontfs
 *
 * return c_dirent entry from cache/frontfs if found
 * return NULL if frontfs lseek()/read() failed.
 */
static struct c_dirent *
cachefs_readdir(CDIR * dirp)
{
	struct c_dirent *ep;
	int		ret;

getnext_cdir:
	if (dirp->dir_size != 0) {
		ep = (struct c_dirent *)&dirp->dir_buf[dirp->dir_off];
		if ((dirp->dir_off + sizeof (struct c_dirent) >
		    dirp->dir_size) ||
		    (dirp->dir_off + C_DIRSIZ(ep) > dirp->dir_size))
			dirp->dir_size = 0;	/* to refill */
	}

	if (dirp->dir_size == 0) {
		/* refill */
		if (get_dirblock(dirp) != SUCCESS) {
			if ((*frontfs_ops->fsw_lseek)(dirp->dir_fd,
			    dirp->dir_loc, 0) == -1)
				return (NULL);

			ret = (*frontfs_ops->fsw_read)(dirp->dir_fd,
				dirp->dir_buf, CDIRBUFSIZE);
			if (ret == -1 || ret == 0)
				return (NULL);

			if (dirp->dir_loc == 0 && ret == CDIRBUFSIZE)
				set_cfsb(CACHEFSDEV, dirp->dir_fileno,
				    (void *)dirp->dir_buf, CDIRBUFSIZE);
			dirp->dir_size = ret;
		}
		dirp->dir_off = 0;
		ep = (struct c_dirent *)dirp->dir_buf;
		/* assume the whole record fits in this buffer */
	}
	dirp->dir_loc += ep->d_length;	/* logical */
	dirp->dir_off += ep->d_length;
	/* the record length can be huge */
	if (dirp->dir_off >= dirp->dir_size)
		dirp->dir_size = 0;	/* for next time */

	if (ep->d_id.cid_fileno == 0)
		goto getnext_cdir;

	return (ep);
}

/*
 * Given a CDIR structure, close the directory fd.  If the CDIR points to
 * a symbolic link, we simple free it, otherwise, close the directory
 * block in the front filesystem.
 *
 * return SUCCESS if structure is closed and memory freed.
 * return FAILURE if frontfs close failed.
 */
static int
cachefs_closedir(CDIR *dirp)
{
	if (dirp == rootdirp)
		return (SUCCESS);

	if (dirp->dir_type == CDIR_SYMLINKENT) {
		bkmem_free((caddr_t)dirp, sizeof (CDIR));
		return (SUCCESS);
	}

	if ((*frontfs_ops->fsw_close)(dirp->dir_fd) == -1) {
		printf("cachefs_closedir(): close(%d) failed.\n", dirp->dir_fd);
		return (FAILURE);
	}

	bkmem_free((caddr_t)dirp, sizeof (CDIR) + CDIRBUFSIZE);
	return (SUCCESS);
}

/*
 * Get the .cfs_attrcache information for an entry. Compare the file
 * attributes stored in the cache with those on the server.
 *
 * return CDIR_NONDIRENT  if entry is not a directory
 * return CDIR_INVALID    if frontfs/cache failed or attributes have changed
 * return CDIR_SYMLINKENT if directory is a symbolic link
 * return CDIR_DIRENT     if valid directory is found
 */
static int
get_attrc(ino64_t fileno, char **sympp)
{
	struct cachefs_metadata *mp = NULL;
	char namebuf[256];
	struct attrcache_index offset;
	ino64_t fgrpno;
	uint_t i;
	uint_t off;
	int md_fd;
	int flag;
	int dtype = CDIR_INVALID;

	/*
	 * return INVALID if not using the local cache
	 */
	if (boot_cachefs_nocache)
		return (CDIR_INVALID);

	/*
	 * Calculate offset number
	 * Truncate offset to 32 bits for use by the hash function
	 */
	fgrpno = (fileno / fsinfo.fi_fgsize) * fsinfo.fi_fgsize;

	/* Truncate inode offset to 32 bits */
	i = (uint_t)(fileno - fgrpno);

	(void) sprintf(namebuf, "%s/%s/%016llX", cacheid, ATTRCACHE_NAME,
	    fgrpno);

	if ((md_fd = (*frontfs_ops->fsw_open)(namebuf, O_RDONLY)) == -1) {
		OPS_DEBUG_CK(("cachefs_get_attrc(): open(%s) failed.\n",
		    namebuf));
		return (CDIR_INVALID);
	}

	/*
	 * Skip header and go to the correct offset location
	 */
	off = (i * sizeof (struct attrcache_index) +
		sizeof (struct attrcache_header));

	if ((*frontfs_ops->fsw_lseek)(md_fd, off, 0) == -1) {
		printf("get_attrc(): lseek at offset %d failed.\n", off);
		goto done;
	}

	if ((*frontfs_ops->fsw_read)(md_fd, (char *)&offset,
	    sizeof (offset)) != sizeof (offset)) {
		printf("get_attrc(): read failed.\n");
		goto done;
	}

	if (offset.ach_written == 0) {
		OPS_DEBUG_CK(("get_attrc(): No attribute cache for %s (%x)\n",
			namebuf, i));
		goto done;
	}

	/*
	 * Seek to the offset of the metadata structure
	 */
	if ((*frontfs_ops->fsw_lseek)(md_fd, offset.ach_offset, 0) == -1) {
		printf("get_attrc(): lseek metadata at %d failed.\n",
			offset.ach_offset);
		goto done;
	}

	if ((mp = (struct cachefs_metadata *)bkmem_alloc(sizeof (*mp))) ==
	    NULL) {
		printf("get_attrc(): bkmem_alloc(metadata) failed.\n");
		goto done;
	}

	if ((*frontfs_ops->fsw_read)(md_fd, (char *)mp, sizeof (*mp)) !=
	    sizeof (*mp)) {
		printf("get_attrc(): read of metadata failed.\n");
		goto done;
	}

	if (do_cachevalidation) {
		struct vattr va;
		extern int boot_nfs_getattr(fid_t *fidp, struct vattr *vap);

		/*
		 * we could use fileno/mp information to see whether the
		 * attribute value has been validated already (during this
		 * boot session).
		 */
		/* use fid to get and check attribute value (mtime) */
		va.va_mask = AT_MTIME;
		boot_nfs_getattr(&mp->md_cookie, &va);
		if ((va.va_mtime.tv_sec != mp->md_vattr.va_mtime.tv_sec) ||
		    (va.va_mtime.tv_nsec != mp->md_vattr.va_mtime.tv_nsec)) {
			OPS_DEBUG_CK(("get_attrc(): cache invalidated"
					" (file %llx)\n", fileno));
			goto done;
		}
	}

	flag = mp->md_flags;
	if ((flag & CACHEFSMD_VALID) != CACHEFSMD_VALID &&
	    !(flag & MD_FASTSYMLNK)) {
		OPS_DEBUG_CK(("get_attrc(): md_flags=%s\n", prt_mdflags(flag)));
		goto done;
	}

	if (flag & MD_FASTSYMLNK) {
		if ((*sympp = get_string_cache(CACHEFSDEV,
			(char *)mp->md_allocinfo)) == NULL) {
			OPS_DEBUG_CK(("get_attrc(): string cache failed\n"));
			return (CDIR_INVALID);
		}
		bkmem_free((caddr_t)mp, sizeof (*mp));
		return (CDIR_SYMLINKENT);
	}

	if (mp->md_vattr.va_type != VDIR) {
		dtype = CDIR_NONDIRENT;
	} else if (mp->md_vattr.va_size != 0) {
		dtype = CDIR_DIRENT;
	} else {
		printf("get_attrc(): zero-length directory\n");
		dtype = CDIR_INVALID;
	}

done:
	if (mp != NULL)
		bkmem_free((caddr_t)mp, sizeof (*mp));

	if ((*frontfs_ops->fsw_close)(md_fd) == -1) {
		printf("get_attrc(): close(%d) failed.\n", md_fd);
		return (CDIR_INVALID);	/* rare case */
	}

	return (dtype);
}

/*
 *  Close the opened file no matter if it's from frontfs or backfs
 */

static int
boot_cachefs_close(int fd)
{
	if (fd >= BACKFS_FD_OFFSET)
		return ((*backfs_ops->fsw_close)(fd - BACKFS_FD_OFFSET));
	else
		return ((*frontfs_ops->fsw_close)(fd));
}

/*
 *  Read file no matter if it's from frontfs/cache or backfs
 */

static ssize_t
boot_cachefs_read(int fd, caddr_t buf, size_t size)
{
	if (fd >= BACKFS_FD_OFFSET)
		return ((*backfs_ops->fsw_read)(fd - BACKFS_FD_OFFSET, buf,
			size));
	else
		return ((*frontfs_ops->fsw_read)(fd, buf, size));
}

/*
 *  Close all opened files and free cached items
 */
static void
boot_cachefs_closeall(int flag)
{
	/*
	 * free all cached items here
	 */
	(*backfs_ops->fsw_closeall)(flag);
	(*frontfs_ops->fsw_closeall)(flag);
	release_cache(CACHEFSDEV);
}

/*
 *  Do a fstat on the specified fd
 */
static int
boot_cachefs_fstat(int fd, struct stat *stp)
{
	if (fd >= BACKFS_FD_OFFSET)
		return ((*backfs_ops->fsw_fstat)(fd - BACKFS_FD_OFFSET, stp));
	else
		return ((*frontfs_ops->fsw_fstat)(fd, stp));
}

/*
 *  Do a lseek on the specified fd
 */
static off_t
boot_cachefs_lseek(int filefd, off_t addr, int whence)
{
	if (filefd >= BACKFS_FD_OFFSET)
		return ((*backfs_ops->fsw_lseek)(filefd - BACKFS_FD_OFFSET,
			addr, whence));
	else
		return ((*frontfs_ops->fsw_lseek)(filefd, addr, whence));
}

/*
 * Get cachefs label information
 * The label information contains, among other things, the version
 * of cachefs that initialized this client's local cache.
 *
 * Read the cachefs label file and compare the version with the one
 * we expect. If the versions do not match, the cache must be
 * reinitialized
 */
static int
get_lab(char *fscname, struct cache_label *lab)
{
	char	fname[MAXPATHLEN];
	int	lab_fd;
	int	ret = SUCCESS;

	(void) sprintf(fname, "%s/%s", fscname, CACHELABEL_NAME);
	if ((lab_fd = (*frontfs_ops->fsw_open)(fname, O_RDONLY)) == -1) {
		printf("get_lab(): open(%s) failed.\n", fname);
		return (FAILURE);
	}

	if ((*frontfs_ops->fsw_read)(lab_fd, (char *)lab, sizeof (*lab)) !=
	    sizeof (*lab)) {
		printf("get_lab(): read(%s) failed.\n", fname);
		ret = FAILURE;
	}
	(void) (*frontfs_ops->fsw_close)(lab_fd);

	if (lab->cl_cfsversion != CFSVERSION) {
		printf("get_lab(): cachefs cache version mismatch %d != %d\n",
		    lab->cl_cfsversion, CFSVERSION);
		ret = FAILURE;
	}

	OPS_DEBUG_CK(("get_lab(): lab->cl_maxinodes = %d\n",
		lab->cl_maxinodes));

	return (ret);
}

/*
 * Get filegrp size information from the fscache option file
 */
static int
get_fsinfo(char *fscname, struct cachefs_fsinfo *fsinfo)
{	int	ret = SUCCESS;
	char	fname[MAXPATHLEN];
	int	info_fd;

	(void) sprintf(fname, "%s/%s", fscname, CACHEFS_FSINFO);

	if ((info_fd = (*frontfs_ops->fsw_open)(fname, O_RDONLY)) == -1) {
		printf("get_fsinfo(): open(%s) failed.\n", fname);
		return (FAILURE);
	}

	if ((*frontfs_ops->fsw_read)(info_fd, (char *)fsinfo,
	    sizeof (*fsinfo)) != sizeof (*fsinfo)) {
		printf("get_fsinfo(): read(%s) failed.\n", fname);
		ret = FAILURE;
	}
	(void) (*frontfs_ops->fsw_close)(info_fd);

	OPS_DEBUG_CK(("get_fsinfo(): flags = %d, popsize= %d, fgsize = %d\n",
			fsinfo->fi_mntflags, fsinfo->fi_popsize,
			fsinfo->fi_fgsize));

	return (ret);
}

/*
 * Get cachefs usage information
 * The usage information indicates the last known state of the cache when
 * the system was shutdown.
 */
static int
get_usage(char *fscname, struct cache_usage *usage)
{
	int	ret = SUCCESS;
	char	fname[MAXPATHLEN];
	int	usage_fd;

	(void) sprintf(fname, "%s/%s", fscname, RESOURCE_NAME);

	if ((usage_fd = (*frontfs_ops->fsw_open)(fname, O_RDONLY)) == -1) {
		printf("get_usage(): open(%s) failed.\n", fname);
		return (FAILURE);
	}

	if ((*frontfs_ops->fsw_read)(usage_fd, (char *)usage,
	    sizeof (*usage)) != sizeof (*usage)) {
		printf("get_usage(): read(%s) failed\n", fname);
		ret = FAILURE;
	}
	(void) (*frontfs_ops->fsw_close)(usage_fd);

	return (ret);
}

/*
 * Debug routines
 */

#ifdef CFS_OPS_DEBUG
/*
 * print metadata structure flags
 */
static char    *
prt_mdflags(int flags)
{
	static char	fbuf[140];
	static struct vf_type {
		int	value;
		char	*name;
	} strflags[] = {
		{ 0x1, "MD_BLOTOUT" },
		{ 0x2, "MD_POPULATED" },
		{ 0x4, "MD_FILE" },
		{ 0x8, "MD_FASTSYMLNK" },
		{ 0x10, "MD_PINNED" },
		{ 0x20, "MD_LOCAL" },
		{ 0x40, "MD_INVALREADDIR" },
	};
	static int	nmdf = sizeof (strflags) / sizeof (strflags[0]);

	int	i;
	int	first = 0;

	fbuf[0] = '\0';

	for (i = 0; i < nmdf; i++) {
		if ((strflags[i].value & flags) == strflags[i].value) {
			if (!first) {
				(void) strcpy(fbuf, strflags[i].name);
				first++;
			} else {
				strcat(fbuf, "|");
				strcat(fbuf, strflags[i].name);
			}
		}
	}

	if (!first)
		(void) strcpy(fbuf, "None Set");

	return (fbuf);
}

/*
 * print flags for the c_direct structure.
 */
static char *
prt_dirflags(int flags)
{
	static char	fbuf[60];
	static struct vf_type {
		int	value;
		char	*name;
	} strflags[] = {
		{ 0x1, "CDE_VALID" },
		{ 0x2, "CDE_COMPLETE" },
		{ 0x4, "CDE_LOCAL" },
	};
	static int	nmdf = sizeof (strflags) / sizeof (strflags[0]);

	int	i;
	int	first = 0;

	fbuf[0] = '\0';

	for (i = 0; i < nmdf; i++) {
		if ((strflags[i].value & flags) == strflags[i].value) {
			if (!first) {
				(void) strcpy(fbuf, strflags[i].name);
				first++;
			} else {
				strcat(fbuf, "|");
				strcat(fbuf, strflags[i].name);
			}
		}
	}

	if (!first)
		(void) strcpy(fbuf, "None Set");

	return (fbuf);
}
#endif
