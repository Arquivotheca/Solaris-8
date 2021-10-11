/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma  ident	"@(#)nftw.c	1.22	97/02/12 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/
/*
 *	nftw - new file tree walk
 *
 *	int nftw(char *path, int (*fn)(), int depth, int flags);
 *
 *	Derived from System V ftw() by David Korn
 *
 *	nftw visits each file and directory in the tree starting at
 *	path. It uses the generic directory reading library so it works
 *	for any file system type.  The flags field is used to specify:
 *		FTW_PHYS  Physical walk, does not follow symblolic links
 *			  Otherwise, nftw will follow links but will not
 *			  walk down any path the crosses itself.
 *		FTW_MOUNT The walk will not cross a mount point.
 *		FTW_DEPTH All subdirectories will be visited before the
 *			  directory itself.
 *		FTW_CHDIR The walk will change to each directory before
 *			  reading it.  This is faster but core dumps
 *			  may not get generated.
 *
 *	fn is called with four arguments at each file and directory.
 *	The first argument is the pathname of the object, the second
 *	is a pointer to the stat buffer and the third is an integer
 *	giving additional information as follows:
 *
 *		FTW_F	The object is a file.
 *		FTW_D	The object is a directory.
 *		FTW_DP	The object is a directory and subdirectories
 *			have been visited.
 *		FTW_SL	The object is a symbolic link.
 *		FTW_SLN The object is a symbolic link pointing at a
 *		        non-existing file.
 *		FTW_DNR	The object is a directory that cannot be read.
 *			fn will not be called for any of its descendants.
 *		FTW_NS	Stat failed on the object because of lack of
 *			appropriate permission. The stat buffer passed to fn
 *			is undefined.  Stat failure for any reason is
 *			considered an error and nftw will return -1.
 *	The fourth argument is a struct FTW* which contains the depth
 *	and the offset into pathname to the base name.
 *	If fn returns nonzero, nftw returns this value to its caller.
 *
 *	depth limits the number of open directories that ftw uses
 *	before it starts recycling file descriptors.  In general,
 *	a file descriptor is used for each level.
 *
 */

#include	<sys/feature_tests.h>

#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#pragma weak nftw64 = _nftw64
#define	lstat64		_lstat64
#define	nftw64		_nftw64
#define	readdir64	_readdir64
#define	stat64		_stat64
#else
#pragma weak nftw = _nftw
#define	lstat		_lstat
#define	nftw		_nftw
#define	readdir		_readdir
#define	stat		_stat
#endif /* !_LP64 && _FILE_OFFSET_BITS == 64 */

#define	chdir		_chdir
#define	closedir	_closedir
#define	fprintf		_fprintf
#define	getcwd		_getcwd
#define	opendir		_opendir
#define	seekdir		_seekdir
#define	telldir		_telldir

#if defined(ABI)
#define	strlen		_abi_strlen
#endif

#include	<mtlib.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<dirent.h>
#include	<errno.h>
#include	<limits.h>
#include	<ftw.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<thread.h>
#include	<synch.h>
#include	<stdio.h>

#ifndef PATH_MAX
#define	PATH_MAX	1023
#endif

static char *fullpath;
static char *tmppath;
static int curflags;
static dev_t cur_mount;
static struct FTW *state;
#ifdef _REENTRANT
static mutex_t nftw_lock = DEFAULTMUTEX;
#endif _REENTRANT

struct Save {
	struct Save *last;
	DIR	*fd;
	char	*comp;
	long	here;
	dev_t	dev;
	ino_t	inode;
};

static int oldclose(struct Save *);
static int (*statf)(const char *, struct stat *);


/*
 * This is the recursive walker.
 */
static int
walk(char *component,
    int (*fn)(const char *, const struct stat *, int, struct FTW *),
    int depth, struct Save *last)
{
	struct stat statb;
	char *p;
	int type;
	char *comp;
	struct dirent *dir;
	char *q;
	int rc = 0;
	int val = -1;
	int cdval = -1;
	int oldbase;
	int skip;
	struct Save this;

	this.last = last;
	this.fd = 0;
	if ((curflags & FTW_CHDIR) && last)
		comp = last->comp;
	else
		comp = tmppath;

	/*
	 * Determine the type of the component.
	 */
	if ((*statf)(comp, &statb) >= 0) {
		if ((statb.st_mode & S_IFMT) == S_IFDIR) {
			type = FTW_D;
			if (depth <= 1)
				(void) oldclose(last);
			if ((this.fd = opendir(comp)) == 0) {
				if (errno == EMFILE && oldclose(last) &&
				    (this.fd = opendir(comp)) != 0) {
					depth = 1;
				} else {
					type = FTW_DNR;
					goto fail;
				}
			}
		} else	if ((statb.st_mode & S_IFMT) == S_IFLNK)
				type = FTW_SL;
			else
				type = FTW_F;
	} else {
		/*
		 * Statf has failed. If stat was used instead of lstat,
		 * try using lstat. If lstat doesn't fail, "comp"
		 * must be a symbolic link pointing to a non-existent
		 * file. Such a symbolic link should be ignored.
		 * Also check the file type, if possible, for symbolic
		 * link.
		 */

		if ((statf == stat) && (lstat(comp, &statb) >= 0) &&
		    ((statb.st_mode & S_IFMT) == S_IFLNK)) {

			/*
			 * Ignore bad symbolic link, let "fn"
			 * report it.
			 */

			errno = ENOENT;
			type = FTW_SLN;
		} else {
			type = FTW_NS;
	fail:
			if (errno != EACCES)
				return (-1);
		}
	}

	/*
	 * If the walk is not supposed to cross a mount point,
	 * and it did, get ready to return.
	 */
	if ((curflags & FTW_MOUNT) && type != FTW_NS &&
	    statb.st_dev != cur_mount)
		goto quit;
	state->quit = 0;

	/*
	 * If the walk has followed a symbolic link, traverse
	 * the walk back to make sure there is not a loop.
	 */
	if ((curflags & FTW_PHYS) == 0) {
		struct Save *sp = last;
		while (sp) {
			/*
			 * If the same node has already been visited, there
			 * is a loop. Get ready to return.
			 */
			if (sp->dev == statb.st_dev &&
			    sp->inode == statb.st_ino)
				goto quit;
			sp = sp->last;
		}
	}

	/*
	 * If current component is not a directory, call user
	 * specified function and get ready to return.
	 */
	if (type != FTW_D || (curflags & FTW_DEPTH) == 0)
		rc = (*fn)(tmppath, &statb, type, state);
	if (rc > 0)
		val = rc;
	skip = (state->quit & FTW_SKD);
	if (rc != 0 || type != FTW_D || state->quit & FTW_PRUNE)
		goto quit;

	if (tmppath[0] != '\0' && component[-1] != '/')
		*component++ = '/';
	if (curflags & FTW_CHDIR) {
		*component = 0;
		if ((cdval = chdir(comp)) >= 0)
			this.comp = component;
		else {
			type = FTW_DNR;
			rc = (*fn)(tmppath, &statb, type, state);
			goto quit;
		}
	}

	this.dev = statb.st_dev;
	this.inode = statb.st_ino;
	oldbase = state->base;
	state->base = (int)(component-tmppath);
	while (dir = readdir(this.fd)) {
		if (dir->d_ino == 0)
			continue;
		q = dir->d_name;
		if (*q == '.') {
			if (q[1] == 0)
				continue;
			else if (q[1] == '.' && q[2] == 0)
				continue;
		}
		p = component;
		while (p < &tmppath[PATH_MAX] && *q != '\0')
			*p++ = *q++;
		*p = '\0';
		state->level++;

		/* Call walk() recursively.  */
		rc = walk(p, fn, depth-1, &this);
		state->level--;
		if (this.fd == 0) {
			*component = 0;
			if (curflags & FTW_CHDIR) {
				this.fd = opendir(".");
			} else {
				this.fd = opendir(comp);
			}
			if (this.fd == 0) {
				rc = -1;
				goto quit;
			}
			seekdir(this.fd, this.here);
		}
		if (rc != 0) {
			if (errno == ENOENT) {
				(void) fprintf(stderr, "cannot open %s: %s\n",
				    tmppath, strerror(errno));
				val = rc;
				continue;
			}
			goto quit;	/* this seems extreme */
		}
	}
	state->base = oldbase;
	*--component = 0;
	type = FTW_DP;
	if ((tmppath[0] != '\0') && (curflags&(FTW_DEPTH)) && !skip)
		rc = (*fn)(tmppath, &statb, type, state);
quit:
	if (cdval >= 0 && last) {
		/* try to change back to previous directory */
		if ((cdval = chdir("..")) >= 0) {
			if ((*statf)(".", &statb) < 0 ||
			    statb.st_ino != last->inode ||
			    statb.st_dev != last->dev)
				cdval = -1;
		}
		*comp = 0;
		if (cdval < 0 && chdir(fullpath) < 0)
			rc = -1;
	}
	if (this.fd)
		(void) closedir(this.fd);
	if (val > rc)
		return (val);
	else
		return (rc);
}


int
nftw(const char *path,
    int (*fn)(const char *, const struct stat *, int, struct FTW *),
    int depth, int flags)
{
	struct stat statb;
	char home[2*(PATH_MAX+1)];
	int rc = -1;
	char *dp;
	char *base;
	char *endhome;
	const char *savepath = path;

	(void)_mutex_lock(&nftw_lock);
	home[0] = 0;

	if (!state &&
	    ((state = (struct FTW *)malloc(sizeof (struct FTW))) == NULL)) {
		(void)_mutex_unlock(&nftw_lock);
		return (-1);
	}

	/*
	 * If the walk is going to change directory before
	 * reading it, save current woring directory.
	 */
	if (flags & FTW_CHDIR) {
		if (getcwd(home, PATH_MAX+1) == 0) {
			(void)_mutex_unlock(&nftw_lock);
			return (-1);
		}
	}
	endhome = dp = home + strlen(home);
	if (*path == '/')
		fullpath = dp;
	else {
		*dp++ = '/';
		fullpath = home;
	}
	tmppath =  dp;
	base = dp-1;
	while (*path && dp < &tmppath[PATH_MAX]) {
		*dp = *path;
		if (*dp == '/')
			base = dp;
		dp++, path++;
	}
	*dp = 0;
	state->base = (int)(base+1-tmppath);
	if (*path) {
		(void)_mutex_unlock(&nftw_lock);
		errno = ENAMETOOLONG;
		return (-1);
	}
	curflags = flags;

	/*
	 * If doing a physical walk (not following symbolic link),
	 * set statf to lstat(). Otherwise, set statf to stat().
	 */
	if ((flags & FTW_PHYS) == 0)
		statf = stat;
	else
		statf = lstat;

	/*
	 * If walk is not going to cross a mount point,
	 * save the current mount point.
	 */
	if (flags & FTW_MOUNT) {
		if ((*statf)(savepath, &statb) >= 0)
			cur_mount = statb.st_dev;
		else
			goto done;
	}
	state->level = 0;

	/*
	 * Call walk() which does most of the work.
	 */
	rc = walk(dp, fn, depth, (struct Save *)0);
done:
	*endhome = 0;
	if (flags & FTW_CHDIR)
		(void) chdir(home);
	(void)_mutex_unlock(&nftw_lock);
	return (rc);
}

/*
 * close the oldest directory.  It saves the seek offset.
 * return value is 0 unless it was unable to close any descriptor
 */

static int
oldclose(struct Save *sp)
{
	struct Save *spnext;
	while (sp) {
		spnext = sp->last;
		if (spnext == 0 || spnext->fd == 0)
			break;
		sp = spnext;
	}
	if (sp == 0 || sp->fd == 0)
		return (0);
	sp->here = telldir(sp->fd);
	(void) closedir(sp->fd);
	sp->fd = 0;
	return (1);
}
