/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)progerr.c	1.11	93/03/09 SMI"	/* SVr4.0  1.5	*/
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include "pkglocale.h"

static char	*ProgName = NULL; 	/* Set via set_prog_name() */

char *
set_prog_name(char *name)
{
	if (name == NULL)
		return (NULL);
	if ((name = strdup(name)) == NULL) {
		(void) fprintf(stderr,
		    "set_prog_name(): strdup(name) failed.\n");
		exit(1);
	}
	ProgName = strrchr(name, '/');
	if (!ProgName++)
		ProgName = name;

	return (ProgName);
}

char *
get_prog_name(void)
{
	return (ProgName);
}


/*VARARGS*/
void
progerr(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if (ProgName && *ProgName)
		(void) fprintf(stderr, pkg_gt("%s: ERROR: "), ProgName);
	else
		(void) fprintf(stderr, pkg_gt(" ERROR: "));

	(void) vfprintf(stderr, fmt, ap);

	va_end(ap);

	(void) fprintf(stderr, "\n");
}
