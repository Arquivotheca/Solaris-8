/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)bzero.c	1.2	96/12/18 SMI"

#include <sys/salib.h>

void
bzero(void *p, size_t n)
{
	char	zeero	= 0;
	char	*cp	= p;

	while (n != 0)
		*cp++ = zeero, n--;	/* Avoid clr for 68000, still... */
}
