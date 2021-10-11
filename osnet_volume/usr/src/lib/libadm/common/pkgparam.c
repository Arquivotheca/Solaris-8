/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pkgparam.c	1.14	99/01/06 SMI"	/* SVr4.0 1.1 */
/*LINTLIBRARY*/

/*   5-20-92   newroot support added  */

#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <pkgstrct.h>
#include <pkginfo.h>
#include <pkglocs.h>
#include <stdlib.h>
#include <unistd.h>
#include "libadm.h"

#define	VALSIZ	128
#define	NEWLINE	'\n'
#define	ESCAPE	'\\'

static char sepset[] =	":=\n";
static char qset[] = 	"'\"";

char *pkgdir = NULL;
char *pkgfile = NULL;

static char Adm_pkgold[PATH_MAX] = { 0 }; /* added for newroot */
static char Adm_pkgloc[PATH_MAX] = { 0 }; /* added for newroot */
static char Adm_pkgadm[PATH_MAX] = { 0 }; /* added for newroot */

/*
 * This looks in a directory that might be the top level directory of a
 * package. It tests a temporary install directory first and then for a
 * standard directory. This looks a little confusing, so here's what's
 * happening. If this pkginfo is being openned in a script during a pkgadd
 * which is updating an existing package, the original pkginfo file is in a
 * directory that has been renamed from <pkginst> to .save.<pkginst>. If the
 * pkgadd fails it will be renamed back to <pkginst>. We are always interested
 * in the OLD pkginfo data because the new pkginfo data is already in our
 * environment. For that reason, we try to open the backup first - that has
 * the old data. This returns the first accessible path in "path" and a "1"
 * if an appropriate pkginfo file was found. It returns a 0 if no type of
 * pkginfo was located.
 */
int
pkginfofind(char *path, char *pkg_dir, char *pkginst)
{
	/* Construct the temporary pkginfo file name. */
	(void) sprintf(path, "%s/.save.%s/pkginfo", pkg_dir, pkginst);
	if (access(path, 0)) {
		/*
		 * This isn't a temporary directory, so we look for a
		 * regular one.
		 */
		(void) sprintf(path, "%s/%s/pkginfo", pkg_dir, pkginst);
		if (access(path, 0))
			return (0); /* doesn't appear to be a package */
	}

	return (1);
}

/*
 * This opens the appropriate pkginfo file for a particular package.
 */
FILE *
pkginfopen(char *pkg_dir, char *pkginst)
{
	FILE *fp = NULL;
	char temp[PATH_MAX];

	if (pkginfofind(temp, pkg_dir, pkginst))
		fp = fopen(temp, "r");

	return (fp);
}


char *
fpkgparam(FILE *fp, char *param)
{
	char	ch, buffer[VALSIZ];
	char	*mempt, *copy;
	int	c, n, escape, begline, quoted;

	if (param == NULL) {
		errno = ENOENT;
		return (NULL);
	}

	mempt = NULL;

	for (;;) {		/* for each entry in the file fp */
		copy = buffer;
		n = 0;

		/* Get the next token. */
		while ((c = getc(fp)) != EOF) {
			ch = (char) c;
			if (strchr(sepset, ch))
				break;
			if (++n < VALSIZ)
				*copy++ = ch;
		}

		/* If it's the end of the file, exit the for() loop */
		if (c == EOF) {
			errno = EINVAL;
			return (NULL); /* no more entries left */

		/* If it's end of line, look for the next parameter. */
		} else if (c == NEWLINE)
			continue;

		/* At this point copy points to the end of a valid parameter. */
		*copy = '\0';		/* Terminate the string. */
		if (buffer[0] == '#')	/* If it's a comment, drop thru. */
			copy = NULL;	/* Comments don't get buffered. */
		else {
			/* If parameter is NULL, we return whatever we got. */
			if (param[0] == '\0') {
				(void) strcpy(param, buffer);
				copy = buffer;

			/* If this doesn't match the parameter, drop thru. */
			} else if (strcmp(param, buffer))
				copy = NULL;

			/* Otherwise, this is our boy. */
			else
				copy = buffer;
		}

		n = quoted = escape = 0;
		begline = 1;

		/* Now read the parameter value. */
		while ((c = getc(fp)) != EOF) {
			ch = (char) c;
			if (begline && ((ch == ' ') || (ch == '\t')))
				continue; /* ignore leading white space */

			if (ch == NEWLINE) {
				if (!escape)
					break; /* end of entry */
				if (copy) {
					if (escape) {
						copy--; /* eat previous esc */
						n--;
					}
					*copy++ = NEWLINE;
				}
				escape = 0;
				begline = 1; /* new input line */
			} else {
				if (!escape && strchr(qset, ch)) {
					/* handle quotes */
					if (begline) {
						quoted++;
						begline = 0;
						continue;
					} else if (quoted) {
						quoted = 0;
						continue;
					}
				}
				if (ch == ESCAPE)
					escape++;
				else if (escape)
					escape = 0;
				if (copy) *copy++ = ch;
				begline = 0;
			}

			if (copy && ((++n % VALSIZ) == 0)) {
				if (mempt) {
					mempt = realloc(mempt,
						(n+VALSIZ)*sizeof (char));
					if (!mempt)
						return (NULL);
				} else {
					mempt = calloc((size_t)(2*VALSIZ),
					    sizeof (char));
					if (!mempt)
						return (NULL);
					(void) strncpy(mempt, buffer, n);
				}
				copy = &mempt[n];
			}
		}

		/*
		 * Don't allow trailing white space.
		 * NOTE : White space in the middle is OK, since this may
		 * be a list. At some point it would be a good idea to let
		 * this function know how to validate such a list. -- JST
		 *
		 * Now while there's a parametric value and it ends in a
		 * space and the actual remaining string length is still
		 * greater than 0, back over the space.
		 */
		while (copy && isspace((unsigned char)*(copy - 1)) && n-- > 0)
			copy--;

		if (quoted) {
			if (mempt)
				(void) free(mempt);
			errno = EFAULT; /* missing closing quote */
			return (NULL);
		}
		if (copy) {
			*copy = '\0';
			break;
		}
		if (c == EOF) {
			errno = EINVAL; /* parameter not found */
			return (NULL);
		}
	}

	if (!mempt)
		mempt = strdup(buffer);
	else
		mempt = realloc(mempt, (strlen(mempt)+1)*sizeof (char));
	return (mempt);
}

char *
pkgparam(char *pkg, char *param)
{
	static char lastfname[PATH_MAX];
	static FILE *fp = NULL;
	char *pt, *copy, *value, line[PATH_MAX];

	if (!pkgdir)
		pkgdir = get_PKGLOC();

	if (!pkg) {
		/* request to close file */
		if (fp) {
			(void) fclose(fp);
			fp = NULL;
		}
		return (NULL);
	}

	if (!param) {
		errno = ENOENT;
		return (NULL);
	}

	if (pkgfile)
		(void) strcpy(line, pkgfile); /* filename was passed */
	else
		(void) pkginfofind(line, pkgdir, pkg);

	if (fp && strcmp(line, lastfname)) {
		/* different filename implies need for different fp */
		(void) fclose(fp);
		fp = NULL;
	}
	if (!fp) {
		(void) strcpy(lastfname, line);
		if ((fp = fopen(lastfname, "r")) == NULL)
			return (NULL);
	}

	/*
	 * if parameter is a null string, then the user is requesting us
	 * to find the value of the next available parameter for this
	 * package and to copy the parameter name into the provided string;
	 * if it is not, then it is a request for a specified parameter, in
	 * which case we rewind the file to start search from beginning
	 */
	if (param[0]) {
		/* new parameter request, so reset file position */
		if (fseek(fp, 0L, 0))
			return (NULL);
	}

	if (pt = fpkgparam(fp, param)) {
		if (strcmp(param, "ARCH") == NULL ||
		    strcmp(param, "CATEGORY") == NULL) {
			/* remove all whitespace from value */
			value = copy = pt;
			while (*value) {
				if (!isspace((unsigned char)*value))
					*copy++ = *value;
				value++;
			}
			*copy = '\0';
		}
		return (pt);
	}
	return (NULL);
}
/*
 * This routine sets adm_pkgloc and adm_pkgadm which are the
 * replacement location for PKGLOC and PKGADM.
 */

static void canonize_name(char *);

void
set_PKGpaths(char *path)
{
	if (path && *path) {
		(void) sprintf(Adm_pkgloc, "%s%s", path, PKGLOC);
		(void) sprintf(Adm_pkgold, "%s%s", path, PKGOLD);
		(void) sprintf(Adm_pkgadm, "%s%s", path, PKGADM);
	} else {
		(void) sprintf(Adm_pkgloc, "%s", PKGLOC);
		(void) sprintf(Adm_pkgold, "%s", PKGOLD);
		(void) sprintf(Adm_pkgadm, "%s", PKGADM);
	}
	canonize_name(Adm_pkgloc);
	canonize_name(Adm_pkgold);
	canonize_name(Adm_pkgadm);
	pkgdir = Adm_pkgloc;
}

char *
get_PKGLOC(void)
{
	if (Adm_pkgloc[0] == NULL)
		return (PKGLOC);
	else
		return (Adm_pkgloc);
}

char *
get_PKGOLD(void)
{
	if (Adm_pkgold[0] == NULL)
		return (PKGOLD);
	else
		return (Adm_pkgold);
}

char *
get_PKGADM(void)
{
	if (Adm_pkgadm[0] == NULL)
		return (PKGADM);
	else
		return (Adm_pkgadm);
}

void
set_PKGADM(char *newpath)
{
	(void) strcpy(Adm_pkgadm, newpath);
}

void
set_PKGLOC(char *newpath)
{
	(void) strcpy(Adm_pkgloc, newpath);
}

#define	isdot(x)	((x[0] == '.')&&(!x[1]||(x[1] == '/')))
#define	isdotdot(x)	((x[0] == '.')&&(x[1] == '.')&&(!x[2]||(x[2] == '/')))

static void
canonize_name(char *file)
{
	char *pt, *last;
	int level;

	/* Remove references such as "./" and "../" and "//" */

	for (pt = file; *pt; ) {
		if (isdot(pt))
			(void) strcpy(pt, pt[1] ? pt+2 : pt+1);
		else if (isdotdot(pt)) {
			level = 0;
			last = pt;
			do {
				level++;
				last += 2;
				if (*last)
					last++;
			} while (isdotdot(last));
			--pt; /* point to previous '/' */
			while (level--) {
				if (pt <= file)
					return;
				while ((*--pt != '/') && (pt > file))
					;
			}
			if (*pt == '/')
				pt++;
			(void) strcpy(pt, last);
		} else {
			while (*pt && (*pt != '/'))
				pt++;
			if (*pt == '/') {
				while (pt[1] == '/')
					(void) strcpy(pt, pt+1);
				pt++;
			}
		}
	}
	if ((--pt > file) && (*pt == '/'))
		*pt = '\0';
}
