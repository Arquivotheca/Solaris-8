/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)basename.c	1.8	96/10/15 SMI"	/* SVr4.0 1.2.3.2 */

/*LINTLIBRARY*/

/*
	Return pointer to the last element of a pathname.
*/

#pragma weak basename = _basename
#include "synonyms.h"
#include <libgen.h>
#include <string.h>
#include <sys/types.h>


char *
basename(char *s)
{
	char	*p;

	if (!s || !*s)			/* zero or empty argument */
		return (".");

	p = s + strlen(s);
	while (p != s && *--p == '/')	/* skip trailing /s */
		*p = '\0';

	if (p == s && *p == '\0')		/* all slashes */
		return ("/");

	while (p != s)
		if (*--p == '/')
			return (++p);

	return (p);
}
