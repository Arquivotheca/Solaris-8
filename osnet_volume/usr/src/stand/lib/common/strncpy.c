/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)strncpy.c	1.3	97/03/10 SMI"

#include <sys/salib.h>

/*
 * Copy s2 to s1, truncating or null-padding to always
 * copy n bytes.  Return s1.
 */
char *
strncpy(char *s1, const char *s2, size_t n)
{
	char *os1 = s1;

	n++;
	while (--n != 0 && (*s1++ = *s2++) != '\0')
		;
	if (n != 0)
		while (--n != 0)
			*s1++ = '\0';
	return (os1);
}
