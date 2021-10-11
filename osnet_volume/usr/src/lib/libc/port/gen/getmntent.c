/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getmntent.c	1.29	99/07/31 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/
#pragma weak getmntany = _getmntany
#pragma weak getmntent = _getmntent
#pragma weak getextmntent = _getextmntent
#pragma weak resetmnttab = _resetmnttab
#pragma weak hasmntopt = _hasmntopt

#include	"synonyms.h"
#include	<mtlib.h>
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/mnttab.h>
#include	<sys/mntio.h>
#include 	<string.h>
#include 	<ctype.h>
#include 	<errno.h>
#include	<stdlib.h>
#include	<thread.h>
#include	<synch.h>
#include	<thr_int.h>
#include	<libc.h>
#include	<unistd.h>
#include	"tsd.h"

static int	getline(char *, FILE *);

#define	GETTOK(xx, ll)\
	if ((mp->xx = strtok(ll, sepstr)) == NULL)\
		return (MNT_TOOFEW);\
	if (strcmp(mp->xx, dash) == 0)\
		mp->xx = NULL
#define	GETTOK_R(xx, ll, tmp)\
	if ((mp->xx = (char *)strtok_r(ll, sepstr, tmp)) == NULL)\
		return (MNT_TOOFEW);\
	if (strcmp(mp->xx, dash) == 0)\
		mp->xx = NULL
#define	DIFF(xx)\
	(mrefp->xx != NULL && (mgetp->xx == NULL ||\
	    strcmp(mrefp->xx, mgetp->xx) != 0))
#define	SDIFF(xx, typem, typer)\
	((mgetp->xx == NULL) || (stat64(mgetp->xx, &statb) == -1) ||\
	((statb.st_mode & S_IFMT) != typem) ||\
	    (statb.st_rdev != typer))

static char	*statline = NULL;
static const char	sepstr[] = " \t\n";
static const char	dash[] = "-";
static mutex_t	mnt_mutex;
static int	devno = 0;
static uint_t	*devs = NULL;
static uint_t	ndevs = 0;

int
getmntany(FILE *fd, struct mnttab *mgetp, struct mnttab *mrefp)
{
	int	ret, bstat;
	mode_t	bmode;
	dev_t	brdev;
	struct stat64	statb;

	if (mrefp->mnt_special && stat64(mrefp->mnt_special, &statb) == 0 &&
	    ((bmode = (statb.st_mode & S_IFMT)) == S_IFBLK ||
	    bmode == S_IFCHR)) {
		bstat = 1;
		brdev = statb.st_rdev;
	} else
		bstat = 0;

	while ((ret = getmntent(fd, mgetp)) == 0 &&
		((bstat == 0 && DIFF(mnt_special)) ||
		(bstat == 1 && SDIFF(mnt_special, bmode, brdev)) ||
		DIFF(mnt_mountp) ||
		DIFF(mnt_fstype) ||
		DIFF(mnt_mntopts) ||
		DIFF(mnt_time)))
		;

	return (ret);
}

int
getmntent(FILE *fd, struct mnttab *mp)
{
	int	ret;
	char	*tmp, *line;

	if (_thr_main()) {
		if (statline == NULL)
			statline = (char *)malloc(MNT_LINE_MAX);
		line = statline;
	} else {
		line = (char *)_tsdbufalloc(_T_GETMNTENT,
		    (size_t)1, sizeof (char) * MNT_LINE_MAX);
	}

	if (line == NULL) {
		errno = ENOMEM;
		return (0);
	}

	/* skip leading spaces and comments */
	if ((ret = getline(line, fd)) != 0)
		return (ret);

	/* split up each field */
	GETTOK_R(mnt_special, line, &tmp);
	GETTOK_R(mnt_mountp, NULL, &tmp);
	GETTOK_R(mnt_fstype, NULL, &tmp);
	GETTOK_R(mnt_mntopts, NULL, &tmp);
	GETTOK_R(mnt_time, NULL, &tmp);

	/* check for too many fields */
	if ((char *)strtok_r(NULL, sepstr, &tmp) != NULL)
		return (MNT_TOOMANY);

	return (0);
}

static int
getline(char *lp, FILE *fd)
{
	char	*cp;

	while ((lp = fgets(lp, MNT_LINE_MAX, fd)) != NULL) {
		if (strlen(lp) == MNT_LINE_MAX-1 && lp[MNT_LINE_MAX-2] != '\n')
			return (MNT_TOOLONG);

		for (cp = lp; *cp == ' ' || *cp == '\t'; cp++)
			;

		if (*cp != '#' && *cp != '\n')
			return (0);
	}
	return (-1);
}

char *
mntopt(char **p)
{
	char *cp = *p;
	char *retstr;

	while (*cp && isspace(*cp))
		cp++;

	retstr = cp;
	while (*cp && *cp != ',')
		cp++;

	if (*cp) {
		*cp = '\0';
		cp++;
	}

	*p = cp;
	return (retstr);
}

char *
hasmntopt(struct mnttab *mnt, char *opt)
{
	char tmpopts[MNT_LINE_MAX];
	char *f, *opts = tmpopts;

	if (mnt->mnt_mntopts == NULL)
		return (NULL);
	(void) strcpy(opts, mnt->mnt_mntopts);
	f = mntopt(&opts);
	for (; *f; f = mntopt(&opts)) {
		if (strncmp(opt, f, strlen(opt)) == 0)
			return (f - tmpopts + mnt->mnt_mntopts);
	}
	return (NULL);
}

int
getextmntent(FILE *fd, struct extmnttab *mp, size_t len)
{
	int	ret;
	char	*tmp, *line;
	uint_t	major, minor;
	int	rddevs = 0;

	if (_thr_main()) {
		if (statline == NULL)
			statline = (char *)malloc(MNT_LINE_MAX);
		line = statline;
	} else {
		line = (char *)_tsdbufalloc(_T_GETMNTENT,
		    (size_t)1, sizeof (char) * MNT_LINE_MAX);
	}

	if (line == NULL) {
		errno = ENOMEM;
		return (-1);
	}

	(void) mutex_lock(&mnt_mutex);
	/*
	 * if reading beginning of file, get device major/minor numbers
	 */
	if (ndevs == 0) {
		rddevs = 1;
	}
	/*
	 * skip leading spaces and comments, really shouldn't happen with
	 * mntfs mnttab implementation
	 */
	if ((ret = getline(line, fd)) != 0) {
		(void) mutex_unlock(&mnt_mutex);
		return (ret);
	}
	if (rddevs) {
		devno = 0;
		if (devs != NULL)
			free(devs);
		/*
		 * get number of devices and device major/minor numbers from
		 * the mntfs.
		 */
		if (ioctl(fileno(fd), MNTIOC_NMNTS, &ndevs) == -1) {
			(void) mutex_unlock(&mnt_mutex);
			return (-1);
		}
		if ((devs = (uint_t *)malloc(ndevs * 2 * sizeof (uint_t)))
			== NULL) {
			errno = ENOMEM;
			(void) mutex_unlock(&mnt_mutex);
			return (-1);
		}
		if (ioctl(fileno(fd), MNTIOC_GETDEVLIST, devs) == -1) {
			(void) mutex_unlock(&mnt_mutex);
			return (-1);
		}
	}
	major = devs[devno * 2];
	minor = devs[devno * 2 + 1];
	devno++;
	(void) mutex_unlock(&mnt_mutex);

	/* split up each field */
	GETTOK_R(mnt_special, line, &tmp);
	GETTOK_R(mnt_mountp, NULL, &tmp);
	GETTOK_R(mnt_fstype, NULL, &tmp);
	GETTOK_R(mnt_mntopts, NULL, &tmp);
	GETTOK_R(mnt_time, NULL, &tmp);
	mp->mnt_major = major;
	mp->mnt_minor = minor;

	/* check for too many fields */
	if ((char *)strtok_r(NULL, sepstr, &tmp) != NULL)
		return (MNT_TOOMANY);

	return (0);
}

/* ARGSUSED */
void
resetmnttab(FILE *fd)
{
	(void) mutex_lock(&mnt_mutex);
	ndevs = 0;
	(void) mutex_unlock(&mnt_mutex);
}
