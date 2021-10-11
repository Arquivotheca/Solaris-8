/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_node.c	1.10	99/06/06 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/prom_emul.h>
#include <sys/obpdefs.h>
#include <sys/kmem.h>
#include <sys/bootconf.h>

#if !defined(KADB) && !defined(I386BOOT) && !defined(IA64BOOT)

/*
 * Routines for walking the PROMs devinfo tree
 */

/*
 * Return the root nodeid.
 * Calling prom_nextnode(0) returns the root nodeid.
 */
dnode_t
prom_rootnode(void)
{
	static dnode_t rootnode;

	return (rootnode ? rootnode : (rootnode = prom_nextnode(OBP_NONODE)));
}

dnode_t
prom_findnode_byname(dnode_t n, char *name)
{
	return (promif_findnode_byname(n, name));
}

dnode_t
prom_chosennode(void)
{
	static dnode_t chosen;
	dnode_t	node;

	if (chosen)
		return (chosen);

	node = prom_findnode_byname(prom_rootnode(), "chosen");
	return (chosen = node);
}

dnode_t
prom_optionsnode(void)
{
	static dnode_t options;
	dnode_t	node;

	if (options)
		return (options);

	node = prom_findnode_byname(prom_rootnode(), "options");
	return (options = node);
}

dnode_t
prom_nextnode(dnode_t nodeid)
{
	return (promif_nextnode(nodeid));
}

dnode_t
prom_childnode(dnode_t nodeid)
{

	return (promif_childnode(nodeid));
}

/*
 * Gets a token from a prom pathname, collecting evertyhing till a non-comma
 * seperator is found.
 */
static char *
prom_gettoken(char *tp, char *token)
{
	for (;;) {
		tp = prom_path_gettoken(tp, token);
		token += prom_strlen(token);
		if (*tp == ',') {
			*token++ = *tp++;
			*token = '\0';
			continue;
		}
		break;
	}
	return (tp);
}

/*
 * Get node id of node in prom tree that path identifies
 */
dnode_t
prom_finddevice(char *path)
{
	char name[OBP_MAXPROPNAME];
	char addr[OBP_MAXPROPNAME];
	char pname[OBP_MAXPROPNAME];
	char paddr[OBP_MAXPROPNAME];
	char *tp;
	dnode_t np, device;

	tp = path;
	np = prom_rootnode();
	device = OBP_BADNODE;
	if (*tp++ != '/')
		goto done;
	for (;;) {
		tp = prom_gettoken(tp, name);
		if (*name == '\0')
			break;
		if (*tp == '@') {
			tp++;
			tp = prom_gettoken(tp, addr);
		} else {
			addr[0] = '\0';
		}
		if ((np = prom_childnode(np)) == OBP_NONODE)
			break;
		while (np != OBP_NONODE) {
			if (prom_getprop(np, OBP_NAME, pname) < 0)
				goto done;
			if (prom_getprop(np, "unit-address", paddr) < 0)
				paddr[0] = '\0';
			if (prom_strcmp(name, pname) == 0 &&
				prom_strcmp(addr, paddr) == 0)
				break;
			np = prom_nextnode(np);
		}
		if (np == OBP_NONODE)
			break;
		if (*tp == '\0') {
			device = np;
			break;
		} else {
			tp++;
		}
	}
done:
	return (device);
}

/*
 * Returns the nodeid of /aliases.
 * /aliases exists in OBP >= 2.4 and in Open Firmware.
 * Returns OBP_BADNODE if it doesn't exist.
 */
dnode_t
prom_alias_node(void)
{
	static dnode_t node;

	if (node == 0)
		node = prom_finddevice("/aliases");
	return (node);
}
#endif
