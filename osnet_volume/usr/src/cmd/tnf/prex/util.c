/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma ident  "@(#)util.c 1.8 94/08/26 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libintl.h>

void
err_fatal(char *s, ...)
{
	va_list		 ap;

	va_start(ap, s);
	(void) vfprintf(stderr, s, ap);
	(void) fprintf(stderr, gettext("\n"));
	va_end(ap);
	exit(1);
}

#if 0
void
err_warning(char *s, ...)
{
	va_list		 ap;

	va_start(ap, s);
	(void) vfprintf(stderr, s, ap);
	(void) fprintf(stderr, gettext("\n"));
	va_end(ap);
}
#endif
