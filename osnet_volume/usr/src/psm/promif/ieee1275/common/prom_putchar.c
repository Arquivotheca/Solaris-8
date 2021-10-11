/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_putchar.c	1.9	95/03/28 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

void
prom_putchar(char c)
{
	while (prom_mayput(c) == -1)
		;
}

int
prom_mayput(char c)
{
	/*
	 * prom_write returns the number of bytes that were written
	 */
	if (prom_write(prom_stdout_ihandle(), &c, 1, 0, BYTE) > 0)
		return (0);
	else
		return (-1);
}
