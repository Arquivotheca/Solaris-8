/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)l64a.c	1.10	99/03/02 SMI"	/* SVr4.0 1.13 */

/*LINTLIBRARY*/
/*
 * convert long int to base 64 ascii
 * char set is [./0-9A-Za-z]
 * two's complement negatives are assumed,
 * but no assumptions are made about sign propagation on right shift
 *
 */

#pragma weak l64a = _l64a

#include "synonyms.h"
#include "mtlib.h"
#include "libc.h"
#include <values.h>
#include <synch.h>
#include <thread.h>
#include <stdlib.h>
#include <sys/types.h>
#include "tsd.h"

#define	BITSPERCHAR	6 /* to hold entire character set */
#define	BITSUSED	(BITSPERBYTE * sizeof (int))
#define	NMAX		((BITSUSED + BITSPERCHAR - 1)/BITSPERCHAR)
#define	SIGN		(-(1 << (BITSUSED - BITSPERCHAR - 1)))
#define	CHARMASK	((1 << BITSPERCHAR) - 1)
#define	WORDMASK	((1 << ((NMAX - 1) * BITSPERCHAR)) - 1)

static char buf_st[NMAX + 1];

char *
l64a(long value)
{
	/* XPG4: only the lower 32 bits are used */
	int lg = (int)value;
	char *buf = (_thr_main() ? buf_st :
				(char *)_tsdbufalloc(_T_L64A,
				(size_t)1, sizeof (char) * (NMAX+1)));
	char *s = buf;

	while (lg != 0) {

		int c = (lg & CHARMASK) + ('0' - 2);

		if (c > '9')
			c += 'A' - '9' - 1;
		if (c > 'Z')
			c += 'a' - 'Z' - 1;
		*s++ = (char)c;
		/* fill high-order CHAR if negative */
		/* but suppress sign propagation */
		lg = ((lg < 0) ? (lg >> BITSPERCHAR) | SIGN :
			lg >> BITSPERCHAR) & WORDMASK;
	}
	*s = '\0';
	return (buf);
}
