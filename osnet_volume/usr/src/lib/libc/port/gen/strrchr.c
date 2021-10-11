/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)strrchr.c	1.10	96/10/15 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/
#include "synonyms.h"
#include <string.h>
#include <stddef.h>
#include <sys/types.h>

/*
 * Return the ptr in sp at which the character c last
 * appears; NULL if not found
*/
char *
strrchr(const char *sp, int c)
{
	char *r = NULL;

	do {
		if (*sp == (char)c)
			r = (char *)sp;
	} while (*sp++);

	return (r);
}
