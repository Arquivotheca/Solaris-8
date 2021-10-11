/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*LINTLIBRARY*/
#ident	"@(#)putparam.c	1.10	99/10/07 SMI"	/* SVr4.0 1.2.1.1	*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>

static char *localeNames[] = {
	"LC_CTYPE",
	"LC_NUMERIC",
	"LC_TIME",
	"LC_COLLATE",
	"LC_MESSAGES",
	"LC_MONETARY",
	"LC_ALL",
	"LANG",
	"TZ",
	NULL
};

#define	NUM_LOCALE_TYPES	9

#define	ERR_MEMORY	"memory allocation failure, errno=%d"

char	*envPtr[NUM_LOCALE_TYPES];

extern char	**environ;

extern void	quit(int exitval);

#define	MALSIZ	64

void
putparam(char *param, char *value)
{
	char	*pt;
	int	i, n;

	/*
	 * If the environment is NULL, allocate space for the
	 * character pointers.
	 */
	if (environ == NULL) {
		environ = (char **) calloc(MALSIZ, sizeof (char *));
		if (environ == NULL) {
			progerr(gettext(ERR_MEMORY), errno);
			quit(99);
		}
	}

	/*
	 * If this parameter is already in place and it has a different
	 * value, clear the old value by freeing the memory previously
	 * allocated. Otherwise, we leave well-enough alone.
	 */
	n = strlen(param);
	for (i = 0; environ[i]; i++) {
		if (strncmp(environ[i], param, n) == 0 &&
		    (environ[i][n] == '=')) {
			if (strcmp((environ[i]) + n + 1, value) == 0)
				return;
			else {
				free(environ[i]);
				break;
			}
		}
	}

	/* Allocate space for the new environment entry. */
	pt = (char *) calloc(strlen(param)+strlen(value)+2, sizeof (char));
	if (pt == NULL) {
		progerr(gettext(ERR_MEMORY), errno);
		quit(99);
	}

	/*
	 * Put the statement into the allocated space and point the
	 * environment entry at it.
	 */
	(void) sprintf(pt, "%s=%s", param, value);
	if (environ[i]) {
		environ[i] = pt;
		return;
	}

	/*
	 * With this parameter in place, if we're at the end of the
	 * allocated environment then allocate more space.
	 */
	environ[i++] = pt;
	if ((i % MALSIZ) == 0) {
		environ = (char **) realloc((void *)environ,
			(i+MALSIZ)*sizeof (char *));
		if (environ == NULL) {
			progerr(gettext(ERR_MEMORY), errno);
			quit(1);
		}
	}

	/* Terminate the environment properly. */
	environ[i] = (char *)NULL;
}

/* bugid 4279039 */
void
getuserlocale(void)
{
	int i;

	for (i = 0; localeNames[i] != NULL; i++) {
		envPtr[i] = getenv(localeNames[i]);
		if (envPtr[i])
			putparam(localeNames[i], envPtr[i]);
	}
}

/* bugid 4279039 */
void
putuserlocale(void)
{
	int i;

	for (i = 0; localeNames[i] != NULL; i++) {
		if (envPtr[i])
			putparam(localeNames[i], envPtr[i]);
	}
}
