#pragma ident	"@(#)error.c	1.10	96/10/27 SMI"

/*
 * Copyright 1989, Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <stdarg.h>

#define	NO_PERROR	0
#define	PERROR		1

char *progname;

void
setprogname(char *name)
{
	register char *p = name, c;

	if (p)
		while (c = *p++)
			if (c == '/')
				name = p;

	progname = name;
}

/* _error([no_perror, ] fmt [, arg ...]) */
/*VARARGS*/
int
_error(int do_perror, char *fmt, ...)
{
	int saved_errno;
	va_list ap;
	extern int errno;

	saved_errno = errno;

	/*
	 * flush all buffers
	 */
	(void) fflush(NULL);
	if (progname)
		(void) fprintf(stderr, "%s: ", progname);

	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (do_perror == NO_PERROR)
		(void) fprintf(stderr, "\n");
	else {
		(void) fprintf(stderr, ": ");
		errno = saved_errno;
		perror("");
	}

	return (1);
}
