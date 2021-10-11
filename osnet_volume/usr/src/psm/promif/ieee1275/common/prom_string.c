/*
 * Copyright (c) 1991-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_string.c	1.11	95/08/15 SMI"

/*
 * The routines are NOT part of the interface, merely internal
 * utilities which assist in making the interface standalone.
 */

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * a version of string copy that is bounded
 */
char *
prom_strncpy(register char *s1, register char *s2, size_t n)
{
	register char *os1 = s1;

	n++;
	while (--n != 0 && (*s1++ = *s2++) != '\0')
		;
	if (n != 0)
		while (--n != 0)
			*s1++ = '\0';
	return (os1);
}

/*
 * and one that knows no bounds
 */
char *
prom_strcpy(register char *s1, register char *s2)
{
	register char *os1;

	os1 = s1;
	while (*s1++ = *s2++)
		;
	return (os1);
}

/*
 * a copy of string compare that is bounded
 */
int
prom_strncmp(register char *s1, register char *s2, register size_t n)
{
	n++;
	if (s1 == s2)
		return (0);
	while (--n != 0 && *s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return ((n == 0) ? 0: (*s1 - s2[-1]));
}

/*
 * and one that knows no bounds
 */
int
prom_strcmp(register char *s1, register char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*s1 - *--s2);
}

/*
 * finds the length of a succession of non-NULL chars
 */
int
prom_strlen(register char *s)
{
	register int n = 0;

	while (*s++)
		n++;

	return (n);
}

/*
 * return the ptr in sp at which the character c last
 * appears; 0 if not found
 */
char *
prom_strrchr(register char *sp, register char c)
{
	register char *r;

	for (r = (char *)0; *sp != (char)0; ++sp)
		if (*sp == c)
			r = sp;
	return (r);
}

/*
 * Concatenate string s2 to string s1
 */
char *
prom_strcat(register char *s1, register char *s2)
{
	char *os1 = s1;

	while ((*s1) != ((char)0))
		s1++;		/* find the end of string s1 */

	while (*s1++ = *s2++)	/* Concatenate s2 */
		;
	return (os1);
}

/*
 * Return the ptr in sp at which the character c first
 * appears; NULL if not found
 */
char *
prom_strchr(register const char *sp, register int c)
{

	do {
		if (*sp == (char)c)
			return ((char *)sp);
	} while (*sp++);
	return (NULL);
}
