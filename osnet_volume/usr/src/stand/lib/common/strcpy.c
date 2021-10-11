/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)strcpy.c	1.3	97/03/10 SMI"

#include <sys/salib.h>

char *
strcpy(char *s1, const char *s2)
{
	char *os1 = s1;

	while (*s1++ = *s2++)
		;
	return (os1);
}
