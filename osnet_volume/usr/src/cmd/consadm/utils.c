/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)utils.c	1.1	98/12/14 SMI"

#include "utils.h"

static const char PNAME_FMT[] = "%s: ";
static const char ERRNO_FMT[] = ": %s\n";

extern const char *pname;

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

	if (strchr(format, '\n') == NULL)
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

	if (strchr(format, '\n') == NULL)
		(void) fprintf(stderr, gettext(ERRNO_FMT), strerror(err));

	exit(1);
}

/*
 * Concatenate a NULL terminated list of strings into
 * a single string.
 */
char *
strcats(char *s, ...)
{
	char	*cp, *ret, *tmp, *p;
	va_list	ap;

	if ((ret = malloc(MAXNAMELEN + 1)) == NULL)
		return (NULL);
	p = ret;

	va_start(ap, s);
	for (cp = s; cp; cp = va_arg(ap, char *)) {
		for (tmp = cp; *tmp; )
			*p++ = *tmp++;
	}
	va_end(ap);
	*p = '\0';
	return (ret);
}
