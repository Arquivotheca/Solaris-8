/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strrspn.c	1.8	97/03/28 SMI"	/* SVr4.0 1.1.2.2 */

/*LINTLIBRARY*/

#pragma weak strrspn = _strrspn

#include "synonyms.h"
#include <sys/types.h>
#include <string.h>

/*
 *	Trim trailing characters from a string.
 *	Returns pointer to the first character in the string
 *	to be trimmed (tc).
 */

char *
strrspn(const char *string, const char *tc)
{
	char	*p;

	p = (char *)string + strlen(string);
	while (p != (char *)string)
		if (!strchr(tc, *--p))
			return (++p);

	return (p);
}
