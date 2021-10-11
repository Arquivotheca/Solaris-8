/*
 * Copyright (c) 1994-1997 by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strcmp.c	1.4	97/05/19 SMI"

#include <sys/salib.h>

/*
 * Compare strings:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */
int
strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*(unsigned char *)s1 - *(unsigned char *)--s2);
}
