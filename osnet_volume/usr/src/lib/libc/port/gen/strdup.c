/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)strdup.c	1.9	96/10/15 SMI"	/* SVr4.0 1.11	*/

/*LINTLIBRARY*/

#pragma weak strdup = _strdup

#include "synonyms.h"
#include "shlib.h"
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

/*
 * string duplication
 * returns pointer to a new string which is the duplicate of string
 * pointed to by s1
 * NULL is returned if new string can't be created
 */
char *
strdup(const char *s1)
{
	char *s2 = malloc(strlen(s1) + 1);

	if (s2)
		(void) strcpy(s2, s1);
	return (s2);
}
