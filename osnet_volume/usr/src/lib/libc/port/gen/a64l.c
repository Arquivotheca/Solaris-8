/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)a64l.c	1.9	96/10/23 SMI"	/* SVr4.0 1.13	*/

/*LINTLIBRARY*/
/*
 * convert base 64 ascii to long int
 * char set is [./0-9A-Za-z]
 *
 */
#pragma weak a64l = _a64l
#include "synonyms.h"
#include <sys/types.h>
#include <stdlib.h>

#define	BITSPERCHAR	6 /* to hold entire character set */
#define	MAXBITS		(6 * BITSPERCHAR) /* maximum number */
			/* of 6 chars converted */

long
a64l(const char *s)
{
	int i, c;
	int lg = 0;

	for (i = 0; ((c = *s++) != '\0') && (i < MAXBITS); i += BITSPERCHAR) {
		if (c > 'Z')
			c -= 'a' - 'Z' - 1;
		if (c > '9')
			c -= 'A' - '9' - 1;
		lg |= (c - ('0' - 2)) << i;
	}
	return ((long)lg);
}
