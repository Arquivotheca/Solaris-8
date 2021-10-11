#include	<stdio.h>
#include	<stdlib.h>
#include	<varargs.h>

#ident	"@(#)errprintf.c	1.4	93/07/20 SMI"

/*
 * errprintf will print a message to standard error channel,
 * and return the same string in the given message buffer.
 * We add an extra return character in case the console is in raw mode.
 */
void
errprintf(va_alist)
	va_dcl
{
	va_list	args;
	char *err;
	char *fmt;

	va_start(args);

	err = va_arg(args, char *);
	fmt = va_arg(args, char *);

	if (err) {
		vsprintf(err, fmt, args);
		va_end(args);
		fprintf(stderr,"%s\r",err);
	} else {
		fprintf(stderr, fmt, args);
		fprintf(stderr, "\r");
	}
}

