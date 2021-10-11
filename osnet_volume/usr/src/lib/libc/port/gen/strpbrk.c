/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)strpbrk.c	1.9	96/10/15 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <string.h>
#include <stddef.h>
#include <sys/types.h>

/*
 * Return ptr to first occurrence of any character from `brkset'
 * in the character string `string'; NULL if none exists.
 */
char *
strpbrk(const char *string, const char *brkset)
{
	const char *p;

	do {
		for (p = brkset; *p != '\0' && *p != *string; ++p)
			;
		if (*p != '\0')
			return ((char *)string);
	} while (*string++);

	return (NULL);
}
