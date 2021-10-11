/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)strcat.c	1.4	97/03/10 SMI"

#include <sys/salib.h>

char *
strcat(char *s1, const char *s2)
{
	char *os1 = s1;

	while (*s1)
		s1++;

	strcpy(s1, s2);
	return (os1);
}
