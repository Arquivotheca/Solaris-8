/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)splpkgmap.c	1.21	98/12/19 SMI"	/* SVr4.0  1.15.2.1	*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <pkgdev.h>
#include <pkgstrct.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"

extern void	quit(int exitval);

extern struct pkgdev pkgdev;

#define	MALSIZ	500
#define	EFACTOR	128L	/* typical size of a single entry in a pkgmap file */

#define	WRN_LIMIT	"WARNING: -l limit (%ld blocks) exceeds device " \
			"capacity (%ld blocks)"
#define	ERR_MEMORY	"memory allocation failure, errno=%d"
#define	ERR_TOOBIG	"%s (%ld blocks) does not fit on a volume"
#define	ERR_INFOFIRST	"information file <%s> must appear on first part"
#define	ERR_INFOSPACE	"all install files must appear on first part"
#define	ERR_VOLBLKS	"Objects selected for part %d require %ld blocks, " \
			"limit=%ld."
#define	ERR_VOLFILES	"Objects selected for part %d require %d files, " \
			"limit=%d."
#define	ERR_FREE	"package does not fit space currently available in <%s>"

struct data {
	long	blks;
	struct cfent *ept;
};

struct class_type {
	char *name;
	int first;
	int last;
};

static long	btotal, 	/* blocks stored on current part */
		bmax; 		/* maximum number of blocks on any part */

static int	ftotal, 	/* files stored on current part */
		fmax; 		/* maximum number of files on any part */
static long	bpkginfo = 0; 	/* blocks used by pkginfo file */
static char	**dirlist;
static int	volno = 0; 	/* current part */
static int	nparts = -1; 	/* total number of parts */
static int	nclass;
static ulong 	DIRSIZE = 0;
static struct	class_type *cl;

static int	nodecount(char *path);
static int	store(struct data **sf, int eptnum, char *aclass, long limit,
			int ilimit);

static void	addclass(char *aclass, int vol);
static void	allocnode(char *path);
static void	newvolume(struct data **sf, int eptnum, long limit, int ilimit);
static void	sortsize(struct data *f, struct data **sf, int eptnum);

int
splpkgmap(struct cfent **eptlist, int eptnum, char *order[], ulong bsize,
		ulong frsize, long *plimit, int *pilimit, long *pllimit)
{
	struct data	*f, **sf;
	struct cfent	*ept;
	register int	i, j;
	int		new_vol_set;
	int		new_vol;
	int		flag, errflg, total;
	long		btemp;
	int		ftemp;

	f = (struct data *) calloc((unsigned) eptnum, sizeof (struct data));
	if (f == NULL) {
		progerr(gettext(ERR_MEMORY), errno);
		quit(99);
	}

	sf = (struct data **) calloc((unsigned) eptnum, sizeof (struct data *));
	if (sf == NULL) {
		progerr(gettext(ERR_MEMORY), errno);
		quit(99);
	}

	nclass = 0;
	cl = (struct class_type *) calloc(MALSIZ, sizeof (struct class_type));
	if (cl == NULL) {
		progerr(gettext(ERR_MEMORY), errno);
		quit(99);
	}

	errflg = 0;

	/*
	 * The next bit of code checks to see if, when creating a package
	 * on a directory, there are enough free blocks and inodes before
	 * continuing.
	 */
	total = 0;
	/*
	 * DIRSIZE takes up 1 logical block, iff we have no frags, else
	 * it just takes a frag
	 */
	DIRSIZE = ((long) frsize > 0) ?
	    howmany(frsize, DEV_BSIZE) :
	    howmany(bsize, DEV_BSIZE);

	if (!pkgdev.mount) {
		allocnode(NULL);
		if ((*pilimit >= 0) && (eptnum+1 > *pilimit)) {
			progerr(gettext(ERR_FREE), pkgdev.dirname);
			quit(1);
		}
		for (i = 0; i < eptnum; i++) {
			if (strchr("dxslcbp", eptlist[i]->ftype))
				continue;
			else {
				total +=
				    (nodecount(eptlist[i]->path) * DIRSIZE);
				total +=
				    nblk(eptlist[i]->cinfo.size, bsize, frsize);
				if (total > *plimit) {
					progerr(gettext(ERR_FREE),
						pkgdev.dirname);
					quit(1);
				}
				allocnode(eptlist[i]->path);
			}
		}
	}
	/*
	 * if there is a value in pllimit (-l specified limit), use that for
	 * the limit from now on.
	 */
	if (*pllimit) {
		if (pkgdev.mount && *pllimit > *plimit)
			logerr(gettext(WRN_LIMIT), *pllimit, *plimit);
		*plimit = *pllimit;
	}

	/*
	 * calculate number of physical blocks used by each object
	 */
	for (i = 0; i < eptnum; i++) {
		f[i].ept = ept = eptlist[i];
		if (ept->volno > nparts)
			nparts = ept->volno;
		addclass(ept->pkg_class, 0);
		if (strchr("dxslcbp", ept->ftype))
			/*
			 * virtual object (no contents)
			 */
			f[i].blks = 0;
		else
			/*
			 * space consumers
			 *
			 * (directories are space consumers as well, but they
			 * get accounted for later).
			 *
			 */
			f[i].blks = nblk(ept->cinfo.size, bsize, frsize);
		if (!bpkginfo && !strcmp(f[i].ept->path, "pkginfo"))
			bpkginfo = f[i].blks;
	}

	/*
	 * Make sure that items slated for a given 'part' do not exceed a single
	 * volume.
	 */
	for (i = 1; i <= nparts; i++) {
		btemp = (bpkginfo + 2L);
		ftemp = 2;
		if (i == 1) {
			/*
			 * save room for install directory
			 */
			ftemp += 2;
			btemp += nblk(eptnum * EFACTOR, bsize, frsize);
			btemp += 2;
		}
		allocnode(NULL);
		for (j = 0; j < eptnum; j++) {
			if (i == 1 && f[j].ept->ftype == 'i' &&
			    (!strcmp(f[j].ept->path, "pkginfo") ||
			    !strcmp(f[j].ept->path, "pkgmap")))
				continue;
			if (f[j].ept->volno == i ||
			    (f[j].ept->ftype == 'i' && i == 1)) {
				ftemp += nodecount(f[j].ept->path);
				btemp += f[j].blks;
				btemp += (ftemp * DIRSIZE);
				allocnode(f[j].ept->path);
			}
		}
		if (btemp > *plimit) {
			progerr(gettext(ERR_VOLBLKS), i, btemp, *plimit);
			errflg++;
		} else if ((*pilimit >= 0) && (ftemp+1 > *pilimit)) {
			progerr(gettext(ERR_VOLFILES), i, ftemp + 1, *pilimit);
			errflg++;
		}
	}
	if (errflg)
		quit(1);

	/*
	 * "sf" - array sorted in decreasing file size order, based on "f".
	 */
	sortsize(f, sf, eptnum);

	/*
	 * initialize first volume
	 */
	newvolume(sf, eptnum, *plimit, *pilimit);

	/*
	 * reserve room on first volume for pkgmap
	 */
	btotal += nblk(eptnum * EFACTOR, bsize, frsize);
	ftotal++;

	/*
	 * initialize directory info
	 */
	allocnode(NULL);

	/*
	 * place installation files on first volume!
	 */
	flag = 0;
	for (j = 0; j < eptnum; ++j) {
		if (f[j].ept->ftype != 'i')
			continue;
		else if (!flag++) {
			/*
			 * save room for install directory
			 */
			ftotal++;
			btotal += 2;
		}
		if (!f[j].ept->volno) {
			f[j].ept->volno = 1;
			ftotal++;
			btotal += f[j].blks;
		} else if (f[j].ept->volno != 1) {
			progerr(gettext(ERR_INFOFIRST), f[j].ept->path);
			errflg++;
		}
	}
	if (errflg)
		quit(1);
	if (btotal > *plimit) {
		progerr(gettext(ERR_INFOSPACE));
		quit(1);
	}

	/*
	 * Make sure that any given file will fit on a single volume, this
	 * calculation has to take into account packaging overhead, otherwise
	 * the function store() will go into a severe recursive plunge.
	 */
	for (j = 0; j < eptnum; ++j) {
		/*
		 * directory overhead.
		 */
		btemp = nodecount(f[j].ept->path) * DIRSIZE;
		/*
		 * packaging overhead.
		 */
		btemp += (bpkginfo + 2L); 	/* from newvolume() */
		if ((f[j].blks + btemp) > *plimit) {
			errflg++;
			progerr(gettext(ERR_TOOBIG), f[j].ept->path, f[j].blks);
		}
	}
	if (errflg)
		quit(1);

	/*
	 * place classes listed on command line
	 */
	if (order) {
		for (i = 0; order[i]; ++i)  {
			while (store(sf, eptnum, order[i], *plimit, *pilimit))
				/* stay in loop until store is complete */
				/* void */;
		}
	}

	while (store(sf, eptnum, (char *)0, *plimit, *pilimit))
		/* stay in loop until store is complete */
		/* void */;

	/*
	 * place all virtual objects, e.g. links and spec devices
	 */
	for (i = 0; i < nclass; ++i) {
		/*
		 * if no objects were associated, attempt to
		 * distribute in order of class list
		 */
		if (cl[i].first == 0)
			cl[i].last = cl[i].first = (i ? cl[i-1].last : 1);
		for (j = 0; j < eptnum; j++) {
			if ((f[j].ept->volno == 0) &&
			    !strcmp(f[j].ept->pkg_class, cl[i].name)) {
				if (strchr("sl", f[j].ept->ftype))
					f[j].ept->volno = cl[i].last;
				else
					f[j].ept->volno = cl[i].first;
			}
		}
	}

	if (btotal)
		newvolume(sf, eptnum, *plimit, *pilimit);

	if (nparts > (volno - 1)) {
		new_vol = volno;
		for (i = volno; i <= nparts; i++) {
			new_vol_set = 0;
			for (j = 0; j < eptnum; j++) {
				if (f[j].ept->volno == i) {
					f[j].ept->volno = new_vol;
					new_vol_set = 1;
				}
			}
			new_vol += new_vol_set;
		}
		nparts = new_vol - 1;
	} else
		nparts = volno - 1;

	*plimit = bmax;
	*pilimit = fmax;

	/*
	 * free up dynamic space used by this module
	 */
	free(f);
	free(sf);
	for (i = 0; i < nclass; ++i)
		free(cl[i].name);
	free(cl);
	for (i = 0; dirlist[i]; i++)
		free(dirlist[i]);
	free(dirlist);

	return (errflg ? -1 : nparts);
}

static int
store(struct data **sf, int eptnum, char *aclass, long limit, int ilimit)
{
	int	i, svnodes, choice, select;
	long	btemp, ftemp;

	select = 0;
	choice = (-1);
	for (i = 0; i < eptnum; ++i) {
		if (sf[i]->ept->volno || strchr("sldxcbp", sf[i]->ept->ftype))
			continue; /* defer storage until class is selected */
		if (aclass && strcmp(aclass, sf[i]->ept->pkg_class))
			continue;
		select++; /* we need to place at least one object */
		ftemp = nodecount(sf[i]->ept->path);
		btemp = sf[i]->blks + (ftemp * DIRSIZE);
		if (((limit <= 0) || ((btotal + btemp) <= limit)) &&
		    ((ilimit <= 0) || ((ftotal + ftemp) < ilimit))) {
			/* largest object which fits on this volume */
			choice = i;
			svnodes = ftemp;
			break;
		}
	}
	if (!select)
		return (0); /* no more to objects to place */

	if (choice < 0) {
		newvolume(sf, eptnum, limit, ilimit);
		return (store(sf, eptnum, aclass, limit, ilimit));
	}
	sf[choice]->ept->volno = (char) volno;
	ftotal += svnodes + 1;
	btotal += sf[choice]->blks + (svnodes * DIRSIZE);
	allocnode(sf[i]->ept->path);
	addclass(sf[choice]->ept->pkg_class, volno);
	return (++choice); /* return non-zero if more work to do */
}

static void
allocnode(char *path)
{
	register int i;
	int	found;
	char	*pt;

	if (path == NULL) {
		if (dirlist) {
			/*
			 * free everything
			 */
			for (i = 0; dirlist[i]; i++)
				free(dirlist[i]);
			free(dirlist);
		}
		dirlist = (char **) calloc(MALSIZ, sizeof (char *));
		if (dirlist == NULL) {
			progerr(gettext(ERR_MEMORY), errno);
			quit(99);
		}
		return;
	}

	pt = path;
	if (*pt == '/')
		pt++;
	/*
	 * since the pathname supplied is never just a directory,
	 * we store only the dirname of of the path.
	 */
	while (pt = strchr(pt, '/')) {
		*pt = '\0';
		found = 0;
		for (i = 0; dirlist[i] != NULL; i++) {
			if (!strcmp(path, dirlist[i])) {
				found++;
				break;
			}
		}
		if (!found) {
			/* insert this path in node list */
			dirlist[i] = qstrdup(path);
			if ((++i % MALSIZ) == 0) {
				dirlist = (char **) realloc(dirlist,
					(i+MALSIZ) * sizeof (char *));
				if (dirlist == NULL) {
					progerr(gettext(ERR_MEMORY), errno);
					quit(99);
				}
			}
			dirlist[i] = (char *) NULL;
		}
		*pt++ = '/';
	}
}

static int
nodecount(char *path)
{
	char	*pt;
	int	i, found, count;

	pt = path;
	if (*pt == '/')
		pt++;

	/*
	 * we want to count the number of path
	 * segments that need to be created, not
	 * including the basename of the path;
	 * this works only since we are never
	 * passed a pathname which itself is a
	 * directory
	 */
	count = 0;
	while (pt = strchr(pt, '/')) {
		*pt = '\0';
		found = 0;
		for (i = 0; dirlist[i]; i++) {
			if (!strcmp(path, dirlist[i])) {
				found++;
				break;
			}
		}
		if (!found)
			count++;
		*pt++ = '/';
	}
	return (count);
}

static void
newvolume(struct data **sf, int eptnum, long limit, int ilimit)
{
	register int i;
	int	newnodes;

	if (volno) {
		(void) fprintf(stderr,
		    gettext("part %2d -- %ld blocks, %d entries\n"),
		    volno, btotal, ftotal);
		if (btotal > bmax)
			bmax = btotal;
		if (ftotal > fmax)
			fmax = ftotal;
		btotal = bpkginfo + 2L;
		ftotal = 2;
	} else {
		btotal = 2L;
		ftotal = 1;
	}
	volno++;

	/*
	 * zero out directory storage
	 */
	allocnode((char *)0);

	/*
	 * force storage of files whose volume number has already been assigned
	 */
	for (i = 0; i < eptnum; i++) {
		if (sf[i]->ept->volno == volno) {
			newnodes = nodecount(sf[i]->ept->path);
			ftotal += newnodes + 1;
			btotal += sf[i]->blks + (newnodes * DIRSIZE);
			if (btotal > limit) {
				progerr(gettext(ERR_VOLBLKS), volno, btotal,
					limit);
				quit(1);
			} else if ((ilimit >= 0) && (ftotal+1 > ilimit)) {
				progerr(gettext(ERR_VOLFILES), volno, ftotal+1,
					ilimit);
				quit(1);
			}
		}
	}
}

static void
addclass(char *aclass, int vol)
{
	int i;

	for (i = 0; i < nclass; ++i) {
		if (!strcmp(cl[i].name, aclass)) {
			if (vol <= 0)
				return;
			if (!cl[i].first || (vol < cl[i].first))
				cl[i].first = vol;
			if (vol > cl[i].last)
				cl[i].last = vol;
			return;
		}
	}
	cl[nclass].name = qstrdup(aclass);
	cl[nclass].first = vol;
	cl[nclass].last = vol;
	if ((++nclass % MALSIZ) == 0) {
		cl = (struct class_type *) realloc((char *)cl,
			sizeof (struct class_type) * (nclass+MALSIZ));
		if (!cl) {
			progerr(gettext(ERR_MEMORY), errno);
			quit(99);
		}
	}
	return;
}

static void
sortsize(struct data *f, struct data **sf, int eptnum)
{
	int	nsf;
	int	i, j, k;

	nsf = 0;
	for (i = 0; i < eptnum; i++) {
		for (j = 0; j < nsf; ++j) {
			if (f[i].blks > sf[j]->blks) {
				for (k = nsf; k > j; k--) {
					sf[k] = sf[k-1];
				}
				break;
			}
		}
		sf[j] = &f[i];
		nsf++;
	}
}
