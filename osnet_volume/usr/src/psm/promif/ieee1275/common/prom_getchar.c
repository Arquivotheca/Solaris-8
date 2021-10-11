/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_getchar.c	1.11	97/06/30 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

u_char
prom_getchar(void)
{
	int c;

	while ((c = prom_mayget()) == -1)
		;

	return ((u_char)c);
}

int
prom_mayget(void)
{
	ssize_t rv;
	char c;

	rv = prom_read(prom_stdin_ihandle(), &c, 1, 0, BYTE);
	return (rv == 1 ? (int)c : -1);
}
