/*
 * Copyright (c) 1994,1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_node.c	1.24	99/05/05 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Routines for walking the PROMs devinfo tree
 */
dnode_t
prom_nextnode(register dnode_t nodeid)
{
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("peer");		/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_dnode2cell(nodeid);	/* Arg1: input phandle */
	ci[4] = p1275_dnode2cell(OBP_NONODE);	/* Res1: Prime result */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (p1275_cell2dnode(ci[4]));	/* Res1: peer phandle */
}

dnode_t
prom_childnode(register dnode_t nodeid)
{
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("child");	/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_dnode2cell(nodeid);	/* Arg1: input phandle */
	ci[4] = p1275_dnode2cell(OBP_NONODE);	/* Res1: Prime result */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (p1275_cell2dnode(ci[4]));	/* Res1: child phandle */
}

#define	ALIGN(x, a)	((a) == 0 ? (uintptr_t)(x) : \
	(((uintptr_t)(x) + (uintptr_t)(a) - 1l) & ~((uintptr_t)(a) - 1l)))
/*
 * Create an object suitable for careening about prom trees
 */
pstack_t *
prom_stack_init(dnode_t *buf, size_t maxstack)
{
	pstack_t *p = (pstack_t *)buf;

	p = (pstack_t *)ALIGN(p, sizeof (dnode_t *));
	p->sp = p->minstack = (void *)(p + 1);
	p->maxstack = buf + maxstack;

	return ((pstack_t *)p);
}

/*
 * Destroy the object
 */
void
prom_stack_fini(pstack_t *ps)
{
	ps->sp = (dnode_t *)0;
	ps->minstack = (dnode_t *)0;
	ps->maxstack = (dnode_t *)0;
}

dnode_t
prom_findnode_bydevtype(dnode_t node, char *type, pstack_t *ps)
{
	int done = 0;

	do {
		while (node != OBP_BADNODE && node != OBP_NONODE) {
			*(ps->sp++) = node;
			if (ps->sp > ps->maxstack)
				prom_panic(
			    "maxstack exceeded in prom_findnode_bydevtype");
			node = prom_childnode(node);
		}

		if (ps->sp > ps->minstack) {
			node = *(--ps->sp);
			if (prom_devicetype(node, type))
				return (node);
			node = prom_nextnode(node);
		} else
			done = 1;

	} while (!done);

	return (OBP_NONODE);
}

dnode_t
prom_findnode_byname(dnode_t node, char *name, pstack_t *ps)
{
	int done = 0;

	do {
		while (node != OBP_BADNODE && node != OBP_NONODE) {
			*(ps->sp)++ = node;
			if (ps->sp > ps->maxstack)
				prom_panic(
			    "maxstack exceeded in prom_findnode_byname");
			node = prom_childnode(node);
		}

		if (ps->sp > ps->minstack) {
			node = *(--ps->sp);
			if (prom_getnode_byname(node, name))
				return (node);
			node = prom_nextnode(node);
		} else
			done = 1;

	} while (!done);

	return (OBP_NONODE);
}

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
prom_parentnode(register dnode_t nodeid)
{
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("parent");	/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_dnode2cell(nodeid);	/* Arg1: input phandle */
	ci[4] = p1275_dnode2cell(OBP_NONODE);	/* Res1: Prime result */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();

	return (p1275_cell2dnode(ci[4]));	/* Res1: parent phandle */
}

dnode_t
prom_finddevice(char *path)
{
	cell_t ci[5];
#ifdef PROM_32BIT_ADDRS
	char *opath = NULL;
	size_t len;
#endif

	promif_preprom();
#ifdef PROM_32BIT_ADDRS
	if ((uintptr_t)path > (uint32_t)-1) {
		opath = path;
		len = prom_strlen(opath) + 1; /* include terminating NUL */
		path = promplat_alloc(len);
		if (path == NULL) {
			promif_postprom();
			return (OBP_BADNODE);
		}
		(void) prom_strcpy(path, opath);
	}
#endif

	ci[0] = p1275_ptr2cell("finddevice");	/* Service name */
	ci[1] = (cell_t)1;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_ptr2cell(path);		/* Arg1: pathname */
	ci[4] = p1275_dnode2cell(OBP_BADNODE);	/* Res1: Prime result */

	(void) p1275_cif_handler(&ci);

#ifdef PROM_32BIT_ADDRS
	if (opath != NULL)
		promplat_free(path, len);
#endif
	promif_postprom();

	return ((dnode_t)p1275_cell2dnode(ci[4])); /* Res1: phandle */
}

dnode_t
prom_chosennode(void)
{
	static dnode_t chosen;
	dnode_t	node;

	if (chosen)
		return (chosen);

	node = prom_finddevice("/chosen");

	if (node != OBP_BADNODE)
		return (chosen = node);

	prom_fatal_error("prom_chosennode: Can't find </chosen>\n");
	/*NOTREACHED*/
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

/*
 * Returns the nodeid of /options.
 * Returns OBP_BADNODE if it doesn't exist.
 */
dnode_t
prom_optionsnode(void)
{
	static dnode_t node;

	if (node == 0)
		node = prom_finddevice("/options");
	return (node);
}
