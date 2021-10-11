/*
 * Copyright (c) 1989-1991, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)sprintf.c	1.3	92/07/14 SMI"

#include <sys/types.h>
#include <sys/varargs.h>
#include <sys/promif.h>

/*VARARGS2*/
char *
sprintf(char *s, char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	s = prom_vsprintf(s, fmt, adx);
	va_end(adx);
	return (s);
}
