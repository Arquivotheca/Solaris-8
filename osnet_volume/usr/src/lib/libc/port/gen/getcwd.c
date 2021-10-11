/*
 * Copyright (c) 1992-1997, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getcwd.c	1.30	99/07/29 SMI"	/* from SunOS4.1 1.20 */

/*LINTLIBRARY*/

/*
 * getcwd() returns the pathname of the current working directory. On error
 * an error message is copied to pathname and null pointer is returned.
 */

#pragma weak getcwd = _getcwd

#include "synonyms.h"
#include <mtlib.h>
#include <alloca.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/mkdev.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <synch.h>

#ifdef _REENTRANT
static mutex_t cwd_lock = DEFAULTMUTEX;
#endif _REENTRANT

struct lastpwd {	/* Info regarding the previous call to getcwd */
	dev_t dev;
	ino64_t ino;
	size_t namelen;
	char	*name;
};

static struct lastpwd *lastone = NULL;	/* Cached entry */
static int pathsize;		/* pathname length */

static char *prepend(char *, char *, size_t);
static int getdevinfo(ino64_t, dev_t, ino64_t, dev_t, char *);
static int isdevice(struct extmnttab *, dev_t);

char *
getcwd(char *pathname, size_t size)
{
	char *tmpptr;			/* temporary pointer */
	dev_t cdev, rdev;		/* current & root device number */
	ino64_t cino, rino;		/* current & root inode number */
	DIR *dirp;			/* directory stream */
	struct dirent64 *dir;		/* directory entry struct */
	struct stat64 d, dd;		/* file status struct */
	dev_t newone_dev;		/* Device number of the new pwd */
	ino64_t newone_ino;		/* Inode number of the new pwd */
	int alloc;			/* if 1, buffer was allocated */
	int saverr;			/* save errno */
	char *pnptr;			/* pathname pointer */
	char *curdir;			/* current directory buffer */
	char *dptr;			/* pointer to the current directory */
	char *tmppath;
	size_t tmp;

	if (size == 0) {
		errno = EINVAL;
		return (NULL);
	}

	alloc = 0;
	if (pathname == NULL)  {
		if ((pathname = malloc(size)) == NULL) {
			errno = ENOMEM;
			return (NULL);
		}
		alloc = 1;
	}

	pnptr = alloca(size);
	pnptr += (size - 1);  /* pnptr is pointing to the last but one byte */
	tmp = pathconf(".", _PC_PATH_MAX);
	if (tmp == (size_t)-1)
		tmp = MAXPATHLEN + 1;

	curdir = alloca(tmp);
	dptr = curdir;	/* current directory buffer */

	(void) _mutex_lock(&cwd_lock);
	pathsize = 0;
	*pnptr = '\0';
	(void) strcpy(dptr, "./");
	dptr += 2;
	if (stat64(curdir, &d) < 0) {
		goto errout;
	}

	/* Cache the pwd entry */
	if (lastone == NULL) {
		lastone = calloc(1, sizeof (struct lastpwd));
	} else if ((d.st_dev == lastone->dev) && (d.st_ino == lastone->ino)) {
		if ((stat64(lastone->name, &dd) == 0) &&
		    (d.st_dev == dd.st_dev) && (d.st_ino == dd.st_ino)) {
			/* Make sure the size is big enough */
			if (lastone->namelen < size) {   /* save length? */
				/* Cache hit. */
				(void) strcpy(pathname, lastone->name);
				(void) _mutex_unlock(&cwd_lock);
				return (pathname);
			}
			errno = ERANGE;
			goto errout;
		}
	}

	newone_dev = d.st_dev;
	newone_ino = d.st_ino;

	if (stat64("/", &dd) < 0) {
		goto errout;
	}
	rdev = dd.st_dev;
	rino = dd.st_ino;
	for (;;) {
		char cfstype[sizeof (d.st_fstype)];
		int  notlofs;
		cino = d.st_ino;
		cdev = d.st_dev;
		(void) strncpy(cfstype, d.st_fstype, sizeof (cfstype));
		notlofs = strncmp(cfstype, "lofs", sizeof (cfstype));
		(void) strcpy(dptr, "../");
		dptr += 3;
		if ((dirp = opendir(curdir)) == NULL) {
			goto errout;
		}
		if (fstat64(dirp->dd_fd, &d) == -1) {
			saverr = errno;
			(void) closedir(dirp);
			errno = saverr;
			goto errout;
		}
		/*
		 * If this is a loopback mount where source and target reside
		 * on the same file system, the st_dev will not change when we
		 * pass the mount point (bug 1220400). However, we can't use
		 * the st_rdev, because it's only defined for char or block
		 * special files (bug 1248090). So, instead we keep track of
		 * the st_fstype, and if it changes from "lofs" to anything
		 * else, then even if the st_dev remains the same, we're
		 * passing a mount point, and will have to do the appropriate
		 * stuff for mount points.
		 */
		if (cdev == d.st_dev && (notlofs ||
		    strncmp(cfstype, d.st_fstype, sizeof (cfstype)) == 0)) {
			if (cino == d.st_ino) {
				/* reached root directory */
				(void) closedir(dirp);
				break;
			}
			if (cino == rino && cdev == rdev) {
				/*
				 * This case occurs when '/' is loopback
				 * mounted on the root filesystem somewhere.
				 */
				goto do_mount_pt;
			}
			do {
				if ((dir = readdir64(dirp)) == NULL) {
					saverr = errno;
					(void) closedir(dirp);
					errno = saverr;
					goto errout;
				}
				if (!notlofs &&
				    strcmp("..", dir->d_name) == 0) {

					char *nxdir;
					DIR *ndirp;
					struct stat64 nd;

					nxdir = (char *)alloca(tmp);

					/*
					 * Check if The parent inode is
					 * inconsistent . If so, it is
					 * the root of the fs
					 */

					(void) strcpy(nxdir, curdir);
					(void) strcat(nxdir, "/..");

					if ((ndirp = opendir(nxdir)) == NULL) {
						saverr = errno;
						(void) closedir(dirp);
						errno = saverr;
						goto errout;
					}

					if (fstat64(ndirp->dd_fd, &nd) == -1) {
						saverr = errno;
						(void) closedir(ndirp);
						(void) closedir(dirp);
						errno = saverr;
						goto errout;
					}
					if (nd.st_ino != dir->d_ino) {
						(void) closedir(ndirp);
						goto do_mount_pt;
					}
					(void) closedir(ndirp);
				}
			} while (dir->d_ino != cino);
		} else { /* It is a mount point */
do_mount_pt:
			tmppath = (char *)alloca(size);

			/*
			 * Get the path name for the given dev number
			 */
			if (getdevinfo(cino, cdev, d.st_ino, d.st_dev,
			    tmppath)) {
				(void) closedir(dirp);
				pnptr = prepend(tmppath, pnptr, size);
				if (pnptr == NULL)
					goto errout;
				break;
			}

			do {
				if ((dir = readdir64(dirp)) == NULL) {
					saverr = errno;
					(void) closedir(dirp);
					errno = saverr;
					goto errout;
				}
				(void) strcpy(dptr, dir->d_name);
				(void) lstat64(curdir, &dd);
			} while (dd.st_ino != cino || dd.st_dev != cdev);
		}

		tmpptr = prepend(dir->d_name, pnptr, size);
		if (tmpptr == NULL) {
			(void) closedir(dirp);
			goto errout;
		}
		if ((pnptr = prepend("/", tmpptr, size)) == (char *)NULL) {
			(void) closedir(dirp);
			goto errout;
		}
		(void) closedir(dirp);
	}
	if (*pnptr == '\0') {	/* current dir == root dir */
		if (size > 1) {
			(void) strcpy(pathname, "/");
		} else {
			errno = ERANGE;
			goto errout;
		}
	} else {
		if (size > strlen(pnptr)) {
			(void) strcpy(pathname, pnptr);
		} else {
			errno = ERANGE;
			goto errout;
		}
	}
	lastone->dev = newone_dev;
	lastone->ino = newone_ino;
	if (lastone->name)
		free(lastone->name);
	lastone->name = strdup(pathname);
	lastone->namelen = strlen(pathname);
	(void) _mutex_unlock(&cwd_lock);
	return (pathname);

errout:
	if (alloc)
		free(pathname);
	(void) _mutex_unlock(&cwd_lock);
	return (NULL);
}

/*
 * prepend() tacks a directory name onto the front of a pathname.
 */
static char *
prepend(char *dirname, char *pathname, size_t size)
{
	register int i;			/* directory name size counter */

	for (i = 0; *dirname != '\0'; i++, dirname++)
		continue;
	if ((pathsize += i) < size) {
		while (i-- > 0)
			*--pathname = *--dirname;
		return (pathname);
	}
	errno = ERANGE;
	return (NULL);
}

/*
 * Gets the path name for the given device number. Returns 1 if
 * successful, else returns 0.
 */
static int
getdevinfo(ino64_t ino, dev_t dev,
    ino64_t parent_ino, dev_t parent_dev, char *path)
{
	struct extmnttab mntent;
	struct mnttab *mnt;
	FILE *mounted;
	char *strp;
	struct stat64 statb;
	int retval = 0;

	/*
	 * It reads the device id from /etc/mnttab file and compares it
	 * with the given dev/ino combination.
	 */
	if ((mounted = fopen(MNTTAB, "r")) == NULL)
		return (retval);

	mnt = (struct mnttab *)&mntent;

	resetmnttab(mounted);
	while (getextmntent(mounted, &mntent, sizeof (struct extmnttab)) == 0) {
		if (hasmntopt(mnt, MNTOPT_IGNORE) || !isdevice(&mntent, dev))
			continue;

		/* Verify once again */
		if ((lstat64(mnt->mnt_mountp, &statb) < 0) ||
		    (statb.st_dev != dev) ||
		    (statb.st_ino != ino))
			continue;
		/*
		 * verify that the parent dir is correct (may not
		 * be if there are loopback mounts around.)
		 */
		(void) strcpy(path, mnt->mnt_mountp);
		(void) strcat(path, "/..");
		if ((lstat64(path, &statb) < 0) ||
		    (statb.st_dev != parent_dev) ||
		    (statb.st_ino != parent_ino))
			continue;
		strp = strrchr(path, '/');
		*strp = '\0';	/* Delete /.. */
		retval = 1;
		break;
	}

	(void) fclose(mounted);

	return (retval);
}

static int
isdevice(struct extmnttab *mnt, dev_t dev)
{
	char *str, *equal;

	if (dev == makedev(mnt->mnt_major, mnt->mnt_minor))
		return (1);
	else
		return (0);
}
