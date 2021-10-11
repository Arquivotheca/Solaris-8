/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)strrchr.c	1.3	97/03/10 SMI"

#include <sys/salib.h>

/*
 * Return the ptr in sp at which the character c last
 * appears, or NULL if not found.
 */
char *
strrchr(const char *sp, int c)
{
	const char *r	= (char *)0;

	do {
		if (*sp == c)
			r = sp;
	} while (*sp++);
	return ((char *)r);
}
