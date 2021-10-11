/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)strstr.c	1.3	97/03/10 SMI"

#include <sys/salib.h>

/*
 * strstr() locates the first occurrence in the string as1 of
 * the sequence of characters (excluding the terminating null
 * character) in the string as2. strstr() returns a pointer
 * to the located string, or a null pointer if the string is
 * not found. If as2 is "", the function returns as1.
 *
 */
char *
strstr(const char *as1, const char *as2)
{
	const char *s1;
	const char *s2;
	const char *tptr;
	char c;

	s1 = as1;
	s2 = as2;

	if (s2 == (char *)0 || *s2 == '\0')
		return ((char *)s1);
	c = *s2;

	while (*s1)
		if (*s1++ == c) {
			tptr = s1;
			while ((c = *++s2) == *s1++ && c)
				/* loop */;

			if (c == 0)
				return ((char *)tptr - 1);
			s1 = tptr;
			s2 = as2;
			c = *s2;
		}
	return ((char *)0);
}
