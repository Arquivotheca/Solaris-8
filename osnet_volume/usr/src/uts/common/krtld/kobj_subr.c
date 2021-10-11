/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)kobj_subr.c	1.4	96/05/17 SMI"

#include <sys/types.h>
#include <sys/param.h>

/*
 * Standalone copies of some basic routines.
 */

int
strcmp(register const char *s1, register const char *s2)
{
	if (s1 == s2)
		return (0);
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*s1 - s2[-1]);
}

/*
 * Compare strings (at most n bytes): return *s1-*s2 for the last
 * characters in s1 and s2 which were compared.
 */
int
strncmp(const char *s1, const char *s2, size_t n)
{
	if (s1 == s2)
		return (0);
	n++;
	while (--n != 0 && *s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return ((n == 0) ? 0 : *s1 - *--s2);
}

size_t
strlen(register const char *s)
{
	register const char *s0 = s + 1;

	while (*s++ != '\0')
		;
	return (s - s0);
}

char *
strcpy(register char *s1, register const char *s2)
{
	char *os1 = s1;

	while (*s1++ = *s2++)
		;
	return (os1);
}

char *
strcat(register char *s1, register const char *s2)
{
	char *os1 = s1;

	while (*s1++)
		;
	--s1;
	while (*s1++ = *s2++)
		;
	return (os1);
}

char *
strchr(const char *sp, int c)
{

	do {
		if (*sp == (char)c)
			return ((char *)sp);
	} while (*sp++);
	return (NULL);
}

void
bzero(void *p_arg, size_t count)
{
	register char zero = 0;
	caddr_t p = p_arg;

	while (count != 0)
		*p++ = zero, count--;	/* Avoid clr for 68000, still... */
}

void
bcopy(const void *src_arg, void *dest_arg, size_t count)
{
	register caddr_t src = (caddr_t)src_arg;
	register caddr_t dest = dest_arg;

	if (src < dest && (src + count) > dest) {
		/* overlap copy */
		while (--count != -1)
			*(dest + count) = *(src + count);
	} else {
		while (--count != -1)
			*dest++ = *src++;
	}
}
