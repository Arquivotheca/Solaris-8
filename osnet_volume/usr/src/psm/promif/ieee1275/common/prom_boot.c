/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_boot.c	1.8	94/11/16 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

char *
prom_bootargs(void)
{
	int length;
	dnode_t node;
	static char *name = "bootargs";
	static char bootargs[OBP_MAXPATHLEN];

	if (bootargs[0] != (char)0)
		return (bootargs);

	node = prom_chosennode();
	if ((node == OBP_NONODE) || (node == OBP_BADNODE))
		node = prom_rootnode();
	length = prom_getproplen(node, name);
	if ((length == -1) || (length == 0))
		return (NULL);
	if (length > OBP_MAXPATHLEN)
		length = OBP_MAXPATHLEN - 1;	/* Null terminator */
	(void) prom_bounded_getprop(node, name, bootargs, length);
	return (bootargs);
}


struct bootparam *
prom_bootparam(void)
{
	PROMIF_DPRINTF(("prom_bootparam on P1275?\n"));
	return ((struct bootparam *)0);
}

char *
prom_bootpath(void)
{
	static char bootpath[OBP_MAXPATHLEN];
	int length;
	dnode_t node;
	static char *name = "bootpath";

	if (bootpath[0] != (char)0)
		return (bootpath);

	node = prom_chosennode();
	if ((node == OBP_NONODE) || (node == OBP_BADNODE))
		node = prom_rootnode();
	length = prom_getproplen(node, name);
	if ((length == -1) || (length == 0))
		return (NULL);
	if (length > OBP_MAXPATHLEN)
		length = OBP_MAXPATHLEN - 1;	/* Null terminator */
	(void) prom_bounded_getprop(node, name, bootpath, length);
	return (bootpath);
}
