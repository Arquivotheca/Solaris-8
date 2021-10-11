/*
 * Copyright (c) 1992-1994,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_panic.c	1.12	99/08/19 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

#ifdef I386BOOT
#ifdef i86pc
#include <sys/bootsvcs.h>
#endif
#else
#include <sys/bootsvcs.h>
#endif

void
prom_panic(char *s)
{
#ifdef I386BOOT
	if (!s)
		s = "unknown panic";
	prom_printf("prom_panic: %s\n", s);

	while (1)
		;
#endif

#ifdef KADB
	(printf(s));
	while (1) {
		prom_getchar();
		printf(s);
	}

#endif

#if !defined(KADB) && !defined(I386BOOT)
	prom_printf(s);
	while (goany())
		int20();
#endif
}
