/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)strspn.c	1.9	96/10/15 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <string.h>
#include <sys/types.h>

/*
 * Return the number of characters in the maximum leading segment
 * of string which consists solely of characters from charset.
 */
size_t
strspn(const char *string, const char *charset)
{
	const char *p, *q;

	for (q = string; *q != '\0'; ++q) {
		for (p = charset; *p != '\0' && *p != *q; ++p)
			;
		if (*p == '\0')
			break;
	}
	return (q - string);
}
