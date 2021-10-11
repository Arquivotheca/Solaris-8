/*
 * Copyright (c) 1994-1997 by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strncmp.c	1.4	97/05/19 SMI"

#include <sys/salib.h>

int
strncmp(const char *s1, const char *s2, size_t n)
{
	if (s1 == s2)
		return (0);
	n++;
	while (--n != 0 && *s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (n == 0 ? 0 : *(unsigned char *)s1 - *(unsigned char *)--s2);
}
