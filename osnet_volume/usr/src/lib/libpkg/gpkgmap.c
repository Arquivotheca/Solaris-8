/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1994,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*LINTLIBRARY*/
#pragma ident	"@(#)gpkgmap.c	1.15	98/12/19 SMI"	/* SVr4.0  1.5.1.1 */

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pkgstrct.h>
#include "pkglib.h"
#include "pkglocale.h"

#define	ERR_READLINK	"unable to read link specification."
#define	ERR_NOVAR	"no value defined for%s variable <%s>."
#define	ERR_OWNTOOLONG	"owner string is too long."
#define	ERR_GRPTOOLONG	"group string is too long."
#define	ERR_IMODE	"mode must not be parametric at install time."
#define	ERR_BASEINVAL	"invalid base for mode."
#define	ERR_MODELONG	"mode string is too long."
#define	ERR_MODEALPHA	"mode is not numeric."
#define	ERR_MODEBITS	"invalid bits set in mode."

extern	char	errbuf[];

char	*errstr = NULL; 		/* WHERE? */

static int	eatwhite(FILE *fp);
static int	getend(FILE *fp);
static int	getstr(FILE *fp, char *sep, int n, char *str);
static int	getnum(FILE *fp, int base, long *d, long bad);
static int	getvalmode(FILE *fp, mode_t *d, long bad, int map);

static char	mypath[PATH_MAX];
static char	mylocal[PATH_MAX];
static int	mapmode = MAPNONE;
static char	*maptype = "";
static mode_t	d_mode = BADMODE;
static char 	*d_owner = BADOWNER;
static char	*d_group = BADGROUP;

/*
 * These determine how gpkgmap() deals with mode, owner and group defaults.
 * It is assumed that the owner and group arguments represent static fields
 * which will persist until attrdefault() is called.
 */
void
attrpreset(int mode, char *owner, char *group)
{
	d_mode = mode;
	d_owner = owner;
	d_group = group;
}

void
attrdefault()
{
	d_mode = NOMODE;
	d_owner = NOOWNER;
	d_group = NOGROUP;
}

/*
 * This determines how gpkgmap() deals with environment variables in the
 * mode, owner and group. Path is evaluated at a higher level based upon
 * other circumstances.
 */
void
setmapmode(int mode)
{
	if (mode >= 0 || mode <= 3) {
		mapmode = mode;
		if (mode == MAPBUILD)
			maptype = " build";
		else if (mode == MAPINSTALL)
			maptype = " install";
		else
			maptype = "";
	}
}

/* This is the external query interface for mapmode. */
int
getmapmode()
{
	return (mapmode);
}

/*
 * Unpack the pkgmap or the contents file or whatever file is in that format.
 * Based upon mapmode, environment parameters will be resolved for mode,
 * owner and group.
 */
int
gpkgmap(struct cfent *ept, FILE *fp)
{
	int	c;

	errstr = NULL;
	ept->volno = 0;
	ept->ftype = BADFTYPE;
	(void) strcpy(ept->pkg_class, BADCLASS);
	ept->pkg_class_idx = -1;
	ept->path = NULL;
	ept->ainfo.local = NULL;
	/* default attributes were supplied, so don't reset */
	ept->ainfo.mode = d_mode;
	(void) strcpy(ept->ainfo.owner, d_owner);
	(void) strcpy(ept->ainfo.group, d_group);
#ifdef SUNOS41
	ept->ainfo.xmajor = BADMAJOR;
	ept->ainfo.xminor = BADMINOR;
#else
	ept->ainfo.major = BADMAJOR;
	ept->ainfo.minor = BADMINOR;
#endif
	ept->cinfo.cksum = ept->cinfo.modtime = ept->cinfo.size = (-1L);

	ept->npkgs = 0;

	if (!fp)
		return (-1);
readline:
	switch (c = eatwhite(fp)) {
	    case EOF:
		return (0);

	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		if (ept->volno) {
			errstr = pkg_gt("bad volume number");
			goto error;
		}
		do {
			ept->volno = (ept->volno*10)+c-'0';
			c = getc(fp);
		} while (isdigit(c));
		if (ept->volno == 0)
			ept->volno = 1;

		goto readline;

	    case ':':
	    case '#':
		(void) getend(fp);
		/*FALLTHRU*/
	    case '\n':
		goto readline;

	    case 'i':
		ept->ftype = (char) c;
		c = eatwhite(fp);
		/*FALLTHRU*/
	    case '.':
	    case '/':
		(void) ungetc(c, fp);

		if (getstr(fp, "=", PATH_MAX, mypath)) {
			errstr = pkg_gt("unable to read pathname field");
			goto error;
		}
		ept->path = mypath;
		c = getc(fp);
		if (c == '=') {
			if (getstr(fp, NULL, PATH_MAX, mylocal)) {
				errstr =
				    pkg_gt("unable to read local pathname");
				goto error;
			}
			ept->ainfo.local = mylocal;
		} else
			(void) ungetc(c, fp);

		if (ept->ftype == 'i') {
			/* content info might exist */
			if (!getnum(fp, 10, (long *)&ept->cinfo.size,
			    BADCONT) &&
			    (getnum(fp, 10, (long *)&ept->cinfo.cksum,
			    BADCONT) ||
			    getnum(fp, 10, (long *)&ept->cinfo.modtime,
			    BADCONT))) {
				errstr =
				    pkg_gt("unable to read content info");
				goto error;
			}
		}

		if (getend(fp)) {
			errstr = pkg_gt("extra tokens on input line");
			return (-1);
		}
		return (1);

	    case '?':
	    case 'f':
	    case 'v':
	    case 'e':
	    case 'l':
	    case 's':
	    case 'p':
	    case 'c':
	    case 'b':
	    case 'd':
	    case 'x':
		ept->ftype = (char) c;
		if (getstr(fp, NULL, CLSSIZ, ept->pkg_class)) {
			errstr = pkg_gt("unable to read class token");
			goto error;
		}
		if (getstr(fp, "=", PATH_MAX, mypath)) {
			errstr = pkg_gt("unable to read pathname field");
			goto error;
		}
		ept->path = mypath;

		c = getc(fp);
		if (c == '=') {
			/* local path */
			if (getstr(fp, NULL, PATH_MAX, mylocal)) {
				errstr = (strchr("sl", ept->ftype) ?
				    pkg_gt(ERR_READLINK) :
				    pkg_gt("unable to read local pathname"));
				goto error;
			}
			ept->ainfo.local = mylocal;
		} else if (strchr("sl", ept->ftype)) {
			if ((c != EOF) && (c != '\n'))
				(void) getend(fp);
			errstr =
			    pkg_gt("missing or invalid link specification");
			return (-1);
		} else
			(void) ungetc(c, fp);
		break;

	    default:
		errstr = pkg_gt("unknown ftype");
error:
		(void) getend(fp);
		return (-1);
	}

	if (strchr("sl", ept->ftype) && (ept->ainfo.local == NULL)) {
		errstr = pkg_gt("no link source specified");
		goto error;
	}

	if (strchr("cb", ept->ftype)) {
#ifdef SUNOS41
		ept->ainfo.xmajor = BADMAJOR;
		ept->ainfo.xminor = BADMINOR;
		if (getnum(fp, 10, (long *)&ept->ainfo.xmajor, BADMAJOR) ||
		    getnum(fp, 10, (long *)&ept->ainfo.xminor, BADMINOR))
#else
		ept->ainfo.major = BADMAJOR;
		ept->ainfo.minor = BADMINOR;
		if (getnum(fp, 10, (long *)&ept->ainfo.major, BADMAJOR) ||
		    getnum(fp, 10, (long *)&ept->ainfo.minor, BADMINOR))
#endif
		{
			errstr =
			    pkg_gt("unable to read major/minor device numbers");
			goto error;
		}
	}

	/*
	 * Links and information files don't have attributes associated with
	 * them. The following either resolves potential variables or passes
	 * them through. Mode is tested for validity to some degree. BAD???
	 * is returned to indicate that no meaningful mode was provided. A
	 * higher authority will decide if that's OK or not. CUR??? means that
	 * the prototype file specifically requires a wildcard ('?') for
	 * that entry. We issue an error if attributes were entered wrong.
	 * We just return BAD??? if there was no entry at all.
	 */
	if (strchr("cbdxpfve", ept->ftype)) {
		int retval;

		if ((retval = getvalmode(fp, &(ept->ainfo.mode), CURMODE,
		    (mapmode != MAPNONE))) == 1)
			goto end;	/* nothing else on the line */
		else if (retval == 2)
			goto error;	/* mode is too no good */

		/* owner & group should be here */
		if ((retval = getstr(fp, NULL, ATRSIZ,
		    ept->ainfo.owner)) == 1)
			goto end;	/* no owner or group - warning */
		if (retval == -1) {
			errstr = pkg_gt(ERR_OWNTOOLONG);
			goto error;
		}

		if ((retval = getstr(fp, NULL, ATRSIZ,
		    ept->ainfo.group)) == 1)
			goto end;	/* no group - warning */
		if (retval == -1) {
			errstr = pkg_gt(ERR_GRPTOOLONG);
			goto error;
		}

		/* Resolve the parameters if required. */
		if (mapmode != MAPNONE) {
			if (mapvar(mapmode, ept->ainfo.owner)) {
				sprintf(errbuf, pkg_gt(ERR_NOVAR),
				    maptype, ept->ainfo.owner);
				errstr = errbuf;
				goto error;
			}
			if (mapvar(mapmode, ept->ainfo.group)) {
				sprintf(errbuf, pkg_gt(ERR_NOVAR),
				    maptype, ept->ainfo.group);
				errstr = errbuf;
				goto error;
			}
		}
	}

	if (strchr("ifve", ept->ftype)) {
		/* look for content description */
		if (!getnum(fp, 10, (long *)&ept->cinfo.size, BADCONT) &&
		(getnum(fp, 10, (long *)&ept->cinfo.cksum, BADCONT) ||
		getnum(fp, 10, (long *)&ept->cinfo.modtime, BADCONT))) {
			errstr = pkg_gt("unable to read content info");
			goto error;
		}
	}

	if (ept->ftype == 'i')
		goto end;

end:
	if (getend(fp) && ept->pinfo) {
		errstr = pkg_gt("extra token on input line");
		return (-1);
	}

done:
	return (1);
}

/*
 * Get and validate the mode attribute. This returns an error if
 *	1. the mode string is too long
 *	2. the mode string includes alpha characters
 *	3. the mode string is not octal
 *	4. mode string is an install parameter
 *	5. mode is an unresolved build parameter and MAPBUILD is
 *	   in effect.
 * If the mode is a build parameter, it is
 *	1. returned as is if MAPNONE is in effect
 *	2. evaluated if MAPBUILD is in effect
 *
 * NOTE : We use "mapmode!=MAPBUILD" to gather that it is install
 * time. At install time we just fix a mode with bad bits set by
 * setting it to CURMODE. This should be an error in a few releases
 * (2.8 maybe) but faulty modes are so common in existing packages
 * that this is a reasonable exception. -- JST 1994-11-9
 *
 * RETURNS
 *	0 if mode is being returned as a valid value
 *	1 if no attributes are present on the line
 *	2 if there was a fundamental error
 */
static int
getvalmode(FILE *fp, mode_t *d, long bad, int map)
{
	char tempmode[20];
	mode_t tempmode_t;
	int retval;

	if ((retval = getstr(fp, NULL, ATRSIZ, tempmode)) == 1)
		return (1);
	else if (retval == -1) {
		errstr = pkg_gt(ERR_MODELONG);
		return (2);
	} else {
		/*
		 * If it isn't a '?' (meaning go with whatever mode is
		 * there), validate the mode and convert it to a mode_t. The
		 * "bad" variable here is a misnomer. It doesn't necessarily
		 * mean bad.
		 */
		if (tempmode[0] == '?') {
			*d = bad;
		} else {
			/*
			 * Mode may not be an install parameter or a
			 * non-build parameter.
			 */
			if (tempmode[0] == '$' &&
			    (isupper(tempmode[1]) || !islower(tempmode[1]))) {
				errstr = pkg_gt(ERR_IMODE);
				return (2);
			}
			if (map) {
				if (mapvar(mapmode, tempmode)) {
					sprintf(errbuf,
					    pkg_gt(ERR_NOVAR),
					    maptype, tempmode);
					errstr = errbuf;
					return (2);
				}
			}

			if (tempmode[0] == '$') {
				*d = BADMODE;	/* may be a problem */
			} else {
				/*
				 * At this point it's supposed to be
				 * something we can convert to a number.
				 */
				int n = 0;

				/*
				 * We reject it if it contains nonnumbers or
				 * it's not octal.
				 */
				while (tempmode[n] && !isspace(tempmode[n])) {
					if (!isdigit(tempmode[n])) {
						errstr = pkg_gt(ERR_MODEALPHA);
						return (2);
					}

					if (strchr("89abcdefABCDEF",
					    tempmode[n])) {
						errstr = pkg_gt(ERR_BASEINVAL);
						return (2);
					}
					n++;
				}

				tempmode_t = strtol(tempmode, NULL, 8);

				/*
				 * We reject it if it contains inappropriate
				 * bits.
				 */
				if (tempmode_t & ~(S_IAMB |
				    S_ISUID | S_ISGID | S_ISVTX)) {
					if (mapmode != MAPBUILD) {
						tempmode_t = bad;
					} else {
						errstr = pkg_gt(ERR_MODEBITS);
						return (2);
					}
				}
				*d = tempmode_t;
			}
		}
		return (0);
	}
}

static int
getnum(FILE *fp, int base, long *d, long bad)
{
	int c, b;

	/* leading white space ignored */
	c = eatwhite(fp);
	if (c == '?') {
		*d = bad;
		return (0);
	}

	if ((c == EOF) || (c == '\n') || !isdigit(c)) {
		(void) ungetc(c, fp);
		return (1);
	}

	*d = 0;
	while (isdigit(c)) {
		b = (c & 017);
		if (b >= base)
			return (2);
		*d = (*d * base) + b;
		c = getc(fp);
	}
	(void) ungetc(c, fp);
	return (0);
}

/*
 *  Get a string from the file. Returns
 *	0 if all OK
 *	1 if nothing there
 *	-1 if string is too long
 */
static int
getstr(FILE *fp, char *sep, int n, char *str)
{
	int c;

	/* leading white space ignored */
	c = eatwhite(fp);
	if ((c == EOF) || (c == '\n')) {
		(void) ungetc(c, fp);
		return (1); /* nothing there */
	}

	/* fill up string until space, tab, or separator */
	while (!strchr(" \t", c) && (!sep || !strchr(sep, c))) {
		if (n-- < 1) {
			*str = '\0';
			return (-1); /* too long */
		}
		*str++ = (char) c;
		c = getc(fp);
		if ((c == EOF) || (c == '\n'))
			break; /* no more on this line */
	}
	*str = '\0';
	(void) ungetc(c, fp);

	return (0);
}

static int
getend(FILE *fp)
{
	int c;
	int n;

	n = 0;
	do {
		if ((c = getc(fp)) == EOF)
			return (n);
		if (!isspace(c))
			n++;
	} while (c != '\n');
	return (n);
}

static int
eatwhite(FILE *fp)
{
	int c;

	/* this test works around a side effect of getc() */
	if (feof(fp))
		return (EOF);
	do
		c = getc(fp);
	while ((c == ' ') || (c == '\t'));
	return (c);
}

/*
 * THIS ROUTINE TAKEN FROM LIB/LIBC/PORT/GEN/CFTIME.C
 */

#ifdef SUNOS41
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
/*
 * This routine converts time as follows.  The epoch is 0000  Jan  1
 * 1970  GMT.   The  argument  time  is  in seconds since then.  The
 * localtime(t) entry returns a pointer to an array containing:
 *
 *		  seconds (0-59)
 *		  minutes (0-59)
 *		  hours (0-23)
 *		  day of month (1-31)
 *		  month (0-11)
 *		  year
 *		  weekday (0-6, Sun is 0)
 *		  day of the year
 *		  daylight savings flag
 *
 * The routine corrects for daylight saving time and  will  work  in
 * any  time  zone provided "timezone" is adjusted to the difference
 * between Greenwich and local standard time (measured in seconds).
 *
 *	 ascftime(buf, format, t)	->  where t is produced by localtime
 *					    and returns a ptr to a character
 *					    string that has the ascii time in
 *					    the format specified by the format
 *					    argument (see date(1) for format
 *					    syntax).
 *
 *	 cftime(buf, format, t) 	->  just calls ascftime.
 */

#ifdef __STDC__
#pragma weak ascftime = _ascftime
#pragma weak cftime = _cftime
#endif
#ifndef SUNOS41
#include	"synonyms.h"
#else
#define	const
#endif
#include	<stddef.h>
#include	<time.h>
#include	<limits.h>
#include	<stdlib.h>

int
cftime(char *buf, char *format, const time_t *t)
{
	return (ascftime(buf, format, localtime(t)));
}

int
ascftime(char *buf, const char *format, const struct tm *tm)
{
	/* Set format string, if not already set */
	if (format == NULL || *format == '\0')
		if (((format = getenv("CFTIME")) == 0) || *format == 0)
			format =  "%C";

	return (strftime(buf, LONG_MAX, format, tm));
}
#endif
