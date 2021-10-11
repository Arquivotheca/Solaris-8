/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strecpy.c	1.9	97/08/05 SMI"	/* SVr4.0 1.2.5.2 */

/*LINTLIBRARY*/

#pragma weak strecpy = _strecpy
#pragma weak streadd = _streadd

#include "synonyms.h"
#include <sys/types.h>
#include <libgen.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

/*
 *	strecpy(output, input, except)
 *	strecpy copies the input string to the output string expanding
 *	any non-graphic character with the C escape sequence.  Escape
 *	sequences produced are those defined in "The C Programming
 *	Language" by Kernighan and Ritchie.
 *	Characters in the `except' string will not be expanded.
 *	Returns the first argument.
 *
 *	streadd( output, input, except )
 *	Identical to strecpy() except returns address of null-byte at end
 *	of output.  Useful for concatenating strings.
 */


char *
strecpy(char *pout, const char *pin, const char *except)
{
	(void) streadd(pout, pin, except);
	return (pout);
}


char *
streadd(char *pout, const char *pin, const char *except)
{
	unsigned	c;

	while ((c = *pin++) != 0) {
		if (!isprint(c) && (!except || !strchr(except, c))) {
			*pout++ = '\\';
			switch (c) {
			case '\n':
				*pout++ = 'n';
				continue;
			case '\t':
				*pout++ = 't';
				continue;
			case '\b':
				*pout++ = 'b';
				continue;
			case '\r':
				*pout++ = 'r';
				continue;
			case '\f':
				*pout++ = 'f';
				continue;
			case '\v':
				*pout++ = 'v';
				continue;
			case '\007':
				*pout++ = 'a';
				continue;
			case '\\':
				continue;
			default:
				(void) sprintf(pout, "%.3o", c);
				pout += 3;
				continue;
			}
		}
		if (c == '\\' && (!except || !strchr(except, c)))
			*pout++ = '\\';
		*pout++ = (char)c;
	}
	*pout = '\0';
	return (pout);
}
