/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dirname.c	1.8	97/06/21 SMI"	/* SVr4.0 1.2.3.2	*/

/* LINTLIBRARY */

/*
 *	Return pointer to the directory name, stripping off the last
 *	component of the path.
 *	Works similar to /bin/dirname
 */

#pragma weak dirname = _dirname
#include "synonyms.h"
#include <sys/types.h>
#include <string.h>

char *
dirname(char *s)
{
	char	*p;

	if (!s || !*s)			/* zero or empty argument */
		return (".");

	p = s + strlen(s);
	while (p != s && *--p == '/')	/* trim trailing /s */
		;

	if (p == s && *p == '/')
		return ("/");

	while (p != s)
		if (*--p == '/') {
			while (*p == '/' && p != s)
				p--;
			*++p = '\0';
			return (s);
		}

	return (".");
}
