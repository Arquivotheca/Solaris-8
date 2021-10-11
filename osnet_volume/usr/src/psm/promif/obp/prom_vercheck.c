/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_vercheck.c	1.3	98/01/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Check if the prom is 64-bit ready.
 *
 * If it's ready (or the test doesn't apply), return 0.
 */
int
prom_version_check(char *buf, size_t buflen, dnode_t *nodeid)
{
	if (nodeid)
		*nodeid = (dnode_t)0;
	if (buf && buflen)
		*buf = '\0';
	return (0);
}
