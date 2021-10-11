/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)strchr.c	1.3	97/03/10 SMI"

#include <sys/salib.h>

/*
 * Return the ptr in sp at which the character c first appears;
 * NULL if not found
 */
char *
strchr(const char *sp, int c)
{
	do {
		if (*sp == (char)c)
			return ((char *)sp);
	} while (*sp++);
	return (0);
}
