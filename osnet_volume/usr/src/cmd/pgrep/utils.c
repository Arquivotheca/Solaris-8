/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)utils.c	1.2	98/04/19 SMI"

#include <libintl.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <wchar.h>

#include "pgrep.h"

static const char PNAME_FMT[] = "%s: ";
static const char ERRNO_FMT[] = ": %s\n";

static const char *pname;

/*PRINTFLIKE1*/
void
warn(const char *format, ...)
{
	int err = errno;
	va_list alist;

	if (pname != NULL)
		(void) fprintf(stderr, gettext(PNAME_FMT), pname);

	va_start(alist, format);
	(void) vfprintf(stderr, format, alist);
	va_end(alist);

	if (strrchr(format, '\n') == NULL)
		(void) fprintf(stderr, gettext(ERRNO_FMT), strerror(err));
}

/*PRINTFLIKE1*/
void
die(const char *format, ...)
{
	int err = errno;
	va_list alist;

	if (pname != NULL)
		(void) fprintf(stderr, gettext(PNAME_FMT), pname);

	va_start(alist, format);
	(void) vfprintf(stderr, format, alist);
	va_end(alist);

	if (strrchr(format, '\n') == NULL)
		(void) fprintf(stderr, gettext(ERRNO_FMT), strerror(err));

	exit(E_ERROR);
}

const char *
getpname(const char *arg0)
{
	const char *p = strrchr(arg0, '/');

	if (p == NULL)
		p = arg0;
	else
		p++;

	pname = p;
	return (p);
}

char *
mbstrip(char *buf, size_t nbytes)
{
	wchar_t wc;
	char *p;
	int n;

	buf[nbytes - 1] = '\0';
	p = buf;

	while (*p != '\0') {
		n = mbtowc(&wc, p, MB_LEN_MAX);

		if (n < 0 || !iswprint(wc)) {
			if (n < 0)
				n = sizeof (char);

			if (nbytes <= n) {
				*p = '\0';
				break;
			}

			(void) memmove(p, p + n, nbytes - n);

		} else {
			nbytes -= n;
			p += n;
		}
	}

	return (buf);
}
