/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)echo.c	1.6	93/03/09 SMI"	/* SVr4.0 1.2	*/
#include <stdio.h>
#include <stdarg.h>

extern int	nointeract;

/*VARARGS*/
void
echo(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if (nointeract)
		return;

	(void) vfprintf(stderr, fmt, ap);

	va_end(ap);

	(void) putc('\n', stderr);
}
