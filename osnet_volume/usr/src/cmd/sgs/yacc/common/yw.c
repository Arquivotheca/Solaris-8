/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)yw.c	1.2	93/06/07 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
int
wsprintf(wchar_t *s, char *format,...)
{
	va_list ap;
	char	buf[BUFSIZ];

	va_start(ap, format);
	(void)vsprintf(buf, format, ap);
	va_end(ap);
	return mbstowcs(s, buf, BUFSIZ/*We don't worry...*/);
}

wchar_t *
wscpy(wchar_t *s1, const wchar_t *s2)
{
	wchar_t *os1 = s1;

	while (*s1++ = *s2++)
	    ;
	return(os1);
}

int
wslen(const wchar_t *s)
/* Returns the number of elements (not bytes!). */
{
	const wchar_t *s0 = s + 1;
	
	while (*s++)
	    ;
	return(s - s0);
}

int
wscmp(const wchar_t *s1, const wchar_t *s2)
{
        if (s1 == s2)
	    return(0);
	
	while (*s1 == *s2++)
	    if (*s1++ == 0)
		return(0);
	return(*s1 - *--s2);
	
}

size_t
wcstombs(char *s, const wchar_t *pwcs, size_t n)
/* Simple-minded wcstombs().
 * Will work only for single-byte chars.
 * Needed since wcstombs() of 4.1 libc conflicts
 * with 4.1 ANSI-C compiler(acc)'s wchar_t definition.
 * libc thinks wchar_t is unsigned short and acc believes
 * in 4-byte wchar_t.
 */
{
	int	i=0;
	char 	c;

	while((c=(char)*pwcs++) && i++<n)
	    *s++=c;
	if(i<n) *s=(char)0;
	return i;
}

size_t
mbstowcs(wchar_t *pwcs, const char *s, size_t n)
/* Simple-minded mbstowcs().*/
{
	int	i=0;
	wchar_t w;

	while((w=(wchar_t)*s++) && i++<n)
	    *pwcs++=w;
	if(i<n) *pwcs=(wchar_t)0;
	return i;
}

