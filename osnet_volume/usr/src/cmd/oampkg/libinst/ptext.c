/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)ptext.c	1.6	93/03/09 SMI"	/* SVr4.0 1.2	*/
#include <stdio.h>
#include <stdarg.h>
#include "libadm.h"

/*VARARGS*/
void
ptext(FILE *fp, char *fmt, ...)
{
	va_list ap;
	char	buffer[2048];

	va_start(ap, fmt);

	(void) vsprintf(buffer, fmt, ap);

	va_end(ap);

	(void) puttext(fp, buffer, 0, 70);
	(void) putc('\n', fp);
}
