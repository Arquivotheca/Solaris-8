/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*LINTLIBRARY*/
#ident	"@(#)logerr.c	1.10	94/11/22 SMI"	/* SVr4.0  1.6	*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "pkglocale.h"

/*VARARGS*/
void
logerr(char *fmt, ...)
{
	va_list ap;
	char	*pt, buffer[2048];
	int	flag;
	char	*estr = pkg_gt("ERROR:");
	char	*wstr = pkg_gt("WARNING:");
	char	*nstr = pkg_gt("NOTE:");

	va_start(ap, fmt);
	flag = 0;
	/* This may have to use the i18n strcmp() routines. */
	if (strncmp(fmt, estr, strlen(estr)) &&
	    strncmp(fmt, wstr, strlen(wstr)) &&
	    strncmp(fmt, nstr, strlen(nstr))) {
		flag++;
		(void) fprintf(stderr, "    ");
	}
	/*
	 * NOTE: internationalization in next line REQUIRES that caller of
	 * this routine be in the same internationalization domain
	 * as this library.
	 */
	(void) vsprintf(buffer, fmt, ap);

	va_end(ap);

	for (pt = buffer; *pt; pt++) {
		(void) putc(*pt, stderr);
		if (flag && (*pt == '\n') && pt[1])
			(void) fprintf(stderr, "    ");
	}
	(void) putc('\n', stderr);
}
