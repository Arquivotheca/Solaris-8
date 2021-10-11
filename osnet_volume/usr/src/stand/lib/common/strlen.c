/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)strlen.c	1.3	97/03/10 SMI"

#include <sys/salib.h>

size_t
strlen(const char *s)
{
	size_t n;

	n = 0;
	while (*s++)
		n++;
	return (n);
}
