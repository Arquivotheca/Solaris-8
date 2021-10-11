/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)strtok.c	1.10	96/10/15 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/
#pragma weak strtok_r = _strtok_r

#include "synonyms.h"
#include <mtlib.h>
#include <string.h>
#include <stddef.h>
#include <thread.h>
#include <synch.h>
#include <sys/types.h>

/*
 * uses strpbrk and strspn to break string into tokens on
 * sequentially subsequent calls.  returns NULL when no
 * non-separator characters remain.
 * `subsequent' calls are calls with first argument NULL.
 */
char *
_strtok_r(char *string, const char *sepset, char **lasts)
{
	char	*q, *r;

	/* first or subsequent call */
	if (string == NULL)
		string = *lasts;

	if (string == 0)		/* return if no tokens remaining */
		return (NULL);

	q = string + strspn(string, sepset);	/* skip leading separators */

	if (*q == '\0')		/* return if no tokens remaining */
		return (NULL);

	if ((r = strpbrk(q, sepset)) == NULL)	/* move past token */
		*lasts = 0;	/* indicate this is last token */
	else {
		*r = '\0';
		*lasts = r+1;
	}
	return (q);
}


char *
strtok(char *string, const char *sepset)
{
	static char *lasts;

	return (_strtok_r(string, sepset, &lasts));
}
