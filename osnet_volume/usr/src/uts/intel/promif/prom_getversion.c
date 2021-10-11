/*
 * Copyright (c) 1995,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_getversion.c	1.3	99/05/04 SMI"
#define	PROM_VERSION_NUMBER 3   /* Fake version number (1275-like) */

int
prom_getversion(void)
{
	return (PROM_VERSION_NUMBER);
}
