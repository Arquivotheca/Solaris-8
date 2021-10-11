#ident	"@(#)modsubr.c 1.7 93/10/08 SMI"

/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */


#include <stdio.h>
#include <varargs.h>
#include <sys/modctl.h>
#include <sys/errno.h>

/*VARARGS0*/
void
#ifdef __STDC__
error(char *fmt, ...)
#else
error(fmt, va_alist)
	char *fmt;
	va_dcl
#endif

{
	va_list args;

	extern errno;
	int error;

	error = errno;

#ifdef __STDC__
	va_start(args);
#else
	va_start(args);
#endif

	vfprintf(stderr, fmt, args);
	fprintf(stderr, ": ");
	if (errno == ENOSPC)
		fprintf(stderr, "Out of memory or no room in system tables\n");
	else
		perror("");
	exit(error);
}

/*VARARGS0*/
void
#ifdef __STDC__
fatal(char *fmt, ...)
#else
fatal(va_alist)
	va_dcl
#endif
{
	va_list args;

#ifdef __STDC__
	va_start(args);
#else
	va_start(args);
#endif

	(void) vfprintf(stderr, fmt, args);
	va_end(args);
	exit(-1);
}
