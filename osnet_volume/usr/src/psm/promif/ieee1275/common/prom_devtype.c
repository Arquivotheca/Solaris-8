/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_devtype.c	1.12	94/11/12 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

int
prom_devicetype(dnode_t id, char *type)
{
	register int len;
	char buf[OBP_MAXDRVNAME];

	len = prom_getproplen(id, OBP_DEVICETYPE);
	if (len <= 0 || len >= OBP_MAXDRVNAME)
		return (0);

	(void) prom_getprop(id, OBP_DEVICETYPE, (caddr_t)buf);

	if (prom_strcmp(type, buf) == 0)
		return (1);

	return (0);
}

int
prom_getnode_byname(dnode_t id, char *name)
{
	int len;
	char buf[OBP_MAXDRVNAME];

	len = prom_getproplen(id, OBP_NAME);
	if (len <= 0 || len >= OBP_MAXDRVNAME)
		return (0);

	(void) prom_getprop(id, OBP_NAME, (caddr_t)buf);

	if (prom_strcmp(name, buf) == 0)
		return (1);

	return (0);
}
