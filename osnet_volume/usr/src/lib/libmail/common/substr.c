/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)substr.c	1.6	99/03/09 SMI" 	/* SVr4.0 1.4	*/
/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include "libmail.h"

/*
	This routine looks for string2 in string1.
	If found, it returns the position string2 is found at,
	otherwise it returns a -1.
*/
int
substr(char *string1, char *string2)
{
	int i, j, len1, len2;

	/* the size of the substring will always fit into an int */
	/*LINTED*/
	len1 = (int)strlen(string1);
	/*LINTED*/
	len2 = (int)strlen(string2);
	for (i = 0; i < len1 - len2 + 1; i++) {
		for (j = 0; j < len2 && string1[i+j] == string2[j]; j++);
		if (j == len2)
			return (i);
	}
	return (-1);
}
