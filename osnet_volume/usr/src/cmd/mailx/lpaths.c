/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)lpaths.c	1.12	98/08/06 SMI"	/* from SVr4.0 1.4.2.1 */

/*
 * mailx -- a modified version of a University of California at Berkeley
 *	mail program
 */

/*
 *	libpath(file) - return the full path to the library file
 */

#include <stdio.h>	/* for declaration of sprintf */
#include <unistd.h>	/* for declaration of access */
#include "uparm.h"
#include <locale.h>

#define	PATHSIZE	1024

char *
libpath(char *file)
{
	static char	buf[PATHSIZE];

	snprintf(buf, sizeof (buf), "%s/%s", LIBPATH, file);
	return (buf);
}

/*
 * Return the path to a potentially locale-specific help file.
 */
char *
helppath(char *file)
{
	static char	buf[PATHSIZE];
	char *loc;

	loc = setlocale(LC_MESSAGES, NULL);
	if (loc != NULL) {
		snprintf(buf, sizeof (buf), "%s/%s/LC_MESSAGES/%s",
			LOCALEPATH, loc, file);
		if (access(buf, 0) == 0)
			return (buf);
	}
	snprintf(buf, sizeof (buf), "%s/%s", LIBPATH, file);
	return (buf);
}
