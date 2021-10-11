/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)atoll.c	1.4	99/04/09 SMI"

/*LINTLIBRARY*/

#pragma weak atoll = _atoll

#include "synonyms.h"
#include "shlib.h"
#include <ctype.h>
#include <sys/types.h>
#include <stdlib.h>

#define	ATOLL

#if defined	ATOI
typedef int TYPE;
#define	NAME	atoi
#elif defined ATOL
typedef long TYPE;
#define	NAME	atol
#else
typedef longlong_t TYPE;
#define	NAME	atoll
#endif

TYPE
NAME(const char *p)
{
	TYPE n;
	int c, neg = 0;
	unsigned char	*up = (unsigned char *)p;

	if (!isdigit(c = *up)) {
		while (isspace(c))
			c = *++up;
		switch (c) {
		case '-':
			neg++;
			/* FALLTHROUGH */
		case '+':
			c = *++up;
		}
		if (!isdigit(c))
			return (0);
	}
	for (n = '0' - c; isdigit(c = *++up); ) {
		n *= 10; /* two steps to avoid unnecessary overflow */
		n += '0' - c; /* accum neg to avoid surprises at MAX */
	}
	return (neg ? n : -n);
}
