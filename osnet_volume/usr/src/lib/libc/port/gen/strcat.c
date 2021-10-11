/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)strcat.c	1.9	96/10/15 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/
#include "synonyms.h"

#include <string.h>
#include <sys/types.h>

/*
 * Concatenate s2 on the end of s1.  S1's space must be large enough.
 * Return s1.
 */
char *
strcat(char *s1, const char *s2)
{
	char *os1 = s1;

	while (*s1++)
		;
	--s1;
	while (*s1++ = *s2++)
		;
	return (os1);
}
