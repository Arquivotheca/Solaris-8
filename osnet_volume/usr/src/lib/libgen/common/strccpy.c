/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strccpy.c	1.8	97/03/28 SMI"	/* SVr4.0 1.2.5.2 */

/*LINTLIBRARY*/

#pragma weak strccpy = _strccpy
#pragma weak strcadd = _strcadd

#include "synonyms.h"
#include <sys/types.h>
#include <libgen.h>

/*
 *	strccpy(output, input)
 *	strccpy copys the input string to the output string compressing
 *	any C-like escape sequences to the real character.
 *	Escape sequences recognized are those defined in "The C Programming
 *	Language" by Kernighan and Ritchie.  strccpy returns the output
 *	argument.
 *
 *	strcadd(output, input)
 *	Identical to strccpy() except returns address of null-byte at end
 *	of output.  Useful for concatenating strings.
 */

char *
strccpy(char *pout, const char *pin)
{
	(void) strcadd(pout, pin);
	return (pout);
}


char *
strcadd(char *pout, const char *pin)
{
	char	c;
	int	count;
	int	wd;

	while (c = *pin++) {
		if (c == '\\')
			switch (c = *pin++) {
			case 'n':
				*pout++ = '\n';
				continue;
			case 't':
				*pout++ = '\t';
				continue;
			case 'b':
				*pout++ = '\b';
				continue;
			case 'r':
				*pout++ = '\r';
				continue;
			case 'f':
				*pout++ = '\f';
				continue;
			case 'v':
				*pout++ = '\v';
				continue;
			case 'a':
				*pout++ = '\007';
				continue;
			case '\\':
				*pout++ = '\\';
				continue;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				wd = c - '0';
				count = 0;
				while ((c = *pin++) >= '0' && c <= '7') {
					wd <<= 3;
					wd |= (c - '0');
					if (++count > 1) {   /* 3 digits max */
						pin++;
						break;
					}
				}
				*pout++ = (char)wd;
				--pin;
				continue;
			default:
				*pout++ = c;
				continue;
		}
		*pout++ = c;
	}
	*pout = '\0';
	return (pout);
}
