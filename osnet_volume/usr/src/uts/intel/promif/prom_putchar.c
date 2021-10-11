/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_putchar.c	1.13	99/05/04 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/bootsvcs.h>

/*ARGSUSED*/
int
prom_mayput(char c)
{
	prom_putchar(c);
	return (1);
}

void
prom_putchar(char c)
{
	static int bhcharpos = 0;

	if (c == '\t') {
			do {
				putchar(' ');
			} while (++bhcharpos % 8);
			return;
	} else  if (c == '\n' || c == '\r') {
			bhcharpos = 0;
			putchar(c);
			return;
	} else if (c == '\b') {
			if (bhcharpos)
				bhcharpos--;
			putchar(c);
			return;
	}

	bhcharpos++;
	putchar(c);
}
