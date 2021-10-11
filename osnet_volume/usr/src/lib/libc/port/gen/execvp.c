/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)execvp.c	1.25	99/08/09 SMI"	/* SVr4.0 1.20	*/

/*LINTLIBRARY*/
/*
 *	execlp(name, arg,...,0)	(like execl, but does path search)
 *	execvp(name, argv)	(like execv, but does path search)
 */
#pragma weak execlp = _execlp
#pragma weak execvp = _execvp
#include "synonyms.h"
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <alloca.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>

static const char *execat(const char *, const char *, char *);

extern  int __xpg4;	/* defined in xpg4.c; 0 if not xpg4-compiled program */

/*VARARGS1*/
int
execlp(const char *name, const char *arg0, ...)
{
	char **argp;
	va_list args;
	char **argvec;
	int err;
	int nargs = 0;
	char *nextarg;

	/*
	 * count the number of arguments in the variable argument list
	 * and allocate an argument vector for them on the stack,
	 * adding space for a terminating null pointer at the end
	 * and one additional space for argv[0] which is no longer
	 * counted by the varargs loop.
	 */

	va_start(args, arg0);

	while (va_arg(args, char *) != (char *)0)
		nargs++;

	va_end(args);

	/*
	 * load the arguments in the variable argument list
	 * into the argument vector and add the terminating null pointer
	 */

	va_start(args, arg0);
	/* workaround for bugid 1242839 */
	argvec = alloca((size_t)((nargs + 2) * sizeof (char *)));
	nextarg = va_arg(args, char *);
	argp = argvec;
	*argp++ = (char *)arg0;
	while (nargs-- && nextarg != (char *)0) {
		*argp = nextarg;
		argp++;
		nextarg = va_arg(args, char *);
	}
	va_end(args);
	*argp = (char *)0;

	/*
	 * call execvp()
	 */

	err = execvp(name, argvec);
	return (err);
}

int
execvp(const char *name, char *const *argv)
{
	const char	*pathstr;
	char	fname[PATH_MAX+2];
	char	*newargs[256];
	int	i;
	const char	*cp;
	unsigned etxtbsy = 1;
	int eacces = 0;
	char *shpath, *shell, *fnp, *fnp2;

	if (*name == '\0') {
		errno = ENOENT;
		return (-1);
	}
	if ((pathstr = getenv("PATH")) == NULL) {
		/*
		 * XPG4:  pathstr is equivalent to CSPATH, except that
		 * :/usr/sbin is appended when root, and pathstr must end
		 * with a colon when not root.  Keep these paths in sync
		 * with CSPATH in confstr.c.  Note that pathstr must end
		 * with a colon when not root so that when name doesn't
		 * contain '/', the last call to execat() will result in an
		 * attempt to execv name from the current directory.
		 */
		if (geteuid() == 0 || getuid() == 0) {
			if (__xpg4 == 0) {	/* not XPG4 */
				pathstr = "/usr/sbin:/usr/ccs/bin:/usr/bin";
			} else {		/* XPG4 (CSPATH + /usr/sbin) */
		pathstr = "/usr/xpg4/bin:/usr/ccs/bin:/usr/bin:"
				"/opt/SUNWspro/bin:/usr/sbin";
			}
		} else {
			if (__xpg4 == 0) {	/* not XPG4 */
				pathstr = "/usr/ccs/bin:/usr/bin:";
			} else {		/* XPG4 (CSPATH) */
				pathstr = "/usr/xpg4/bin:/usr/ccs/bin:"
				    "/usr/bin:/opt/SUNWspro/bin:";
			}
		}
	}
	cp = strchr(name, '/')? (const char *)"": pathstr;

	do {
		cp = execat(cp, name, fname);
	retry:
		/*
		 * 4025035 and 4038378
		 * if a filename begins with a "-" prepend "./" so that
		 * the shell can't interpret it as an option
		 */
		if (*fname == '-') {
			if ((fnp = strdup(fname)) != NULL) {
				fnp2 = fname;
				*fnp2++ = '.';
				*fnp2++ = '/';
				if ((strlen(fnp) + 2) <= sizeof (fname)) {
					(void) strcpy(&fname[2], fnp);
				} else {
					errno = E2BIG;
					free(fnp);
					return (-1);
				}
			} else {
				errno = ENOMEM;
				return (-1);
			}
			free(fnp);
		}
		(void) execv(fname, argv);
		switch (errno) {
		case ENOEXEC:
			if (__xpg4 == 0) {	/* not XPG4 */
				shpath = "/bin/sh";
				shell = "sh";
			} else {
				/* XPG4 */
				shpath = "/bin/ksh";
				shell = "ksh";
			}
			newargs[0] = shell;
			newargs[1] = fname;
			for (i = 1; newargs[i + 1] = argv[i]; ++i) {
				if (i >= 254) {
					errno = E2BIG;
					return (-1);
				}
			}
			(void) execv((const char *)shpath, newargs);
			return (-1);
		case ETXTBSY:
			if (++etxtbsy > 5)
				return (-1);
			(void) sleep(etxtbsy);
			goto retry;
		case EACCES:
			++eacces;
			break;
		case ENOMEM:
		case E2BIG:
		case EFAULT:
			return (-1);
		}
	} while (cp);
	if (eacces)
		errno = EACCES;
	return (-1);
}

static const char *
execat(const char *s1, const char *s2, char *si)
{
	char	*s;
	int cnt = PATH_MAX + 1; /* number of characters in s2 */

	s = si;
	while (*s1 && *s1 != ':') {
		if (cnt > 0) {
			*s++ = *s1++;
			cnt--;
		} else
			s1++;
	}
	if (si != s && cnt > 0) {
		*s++ = '/';
		cnt--;
	}
	while (*s2 && cnt > 0) {
		*s++ = *s2++;
		cnt--;
	}
	*s = '\0';
	return (*s1 ? ++s1: 0);
}
