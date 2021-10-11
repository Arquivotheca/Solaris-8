/*
 * Copyright (c) 1991-1993, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_node.c	1.11	99/05/05 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Routines for walking the PROMs devinfo tree
 */
dnode_t
prom_nextnode(register dnode_t nodeid)
{
	dnode_t n;

	promif_preprom();
	n = OBP_DEVR_NEXT(nodeid);
	promif_postprom();
	return (n);
}


dnode_t
prom_childnode(register dnode_t nodeid)
{
	dnode_t n;

	promif_preprom();
	n = OBP_DEVR_CHILD(nodeid);
	promif_postprom();
	return (n);
}

/*
 * Create an object suitable for careening about prom trees
 */
pstack_t *
prom_stack_init(dnode_t *buf, size_t maxstack)
{
	pstack_t *p = (pstack_t *)buf;

	p->sp = p->minstack = buf + sizeof (pstack_t) / sizeof (dnode_t *);
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
	ps = (pstack_t *)0;
}

dnode_t
prom_findnode_bydevtype(dnode_t node, char *type, pstack_t *ps)
{
	int done = 0;

	do {
		while (node != OBP_BADNODE && node != OBP_NONODE) {
			*(ps->sp)++ = node;
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
 * Return the root nodeid -- it is only necessary to make this call into
 * the prom once.  Calling prom_nextnode(0) returns the root nodeid.
 */
dnode_t
prom_rootnode(void)
{
	static dnode_t rootnode;

	return (rootnode ? rootnode : (rootnode = prom_nextnode(OBP_NONODE)));
}

/*
 * prom_parentnode():
 *
 * Search the prom tree to find the parent of a given nodeid.
 * Implemented as a breadth-wise search.
 *
 * Need to do this as a rooted search because there is no config_ops
 * function to do this for us.
 *
 * parentnode(): static recursive function used only be prom_parentnode().
 */

static dnode_t
parentnode(dnode_t root, register dnode_t nodeid)
{
	register dnode_t id, child, firstchild;

	if ((firstchild = prom_childnode(root)) == OBP_NONODE)
		return ((dnode_t)0);		/* not on this branch */

	/* Search children */

	for (child = firstchild; child != 0; child = prom_nextnode(child))
		if (child == nodeid)
			return (root);

	/* Search children's children */

	for (child = firstchild; child != 0; child = prom_nextnode(child))
		if ((id = parentnode(child, nodeid)) != OBP_NONODE)
			return (id);

	return (OBP_NONODE);
}

dnode_t
prom_parentnode(register dnode_t nodeid)
{
	if ((nodeid == OBP_NONODE) ||
	    (nodeid == OBP_BADNODE) ||
	    (nodeid == prom_rootnode()))	/* Parent of root node? */
		return (OBP_NONODE);

	return (parentnode(prom_rootnode(), nodeid));
}

/*
 * Implements find-device client interface in obp2.x.
 * There is no equivalent in obp1.x, even if you could safely use interpret.
 */
static char finddevice[] =
	" ['] find-device"		/* ( r-addr buf len xt ) */
	" catch if 2drop true else current-device device-end then"
					/* ( r-addr phandle | r-addr true ) */
	" swap l!";

dnode_t
prom_finddevice(char *path)
{
	dnode_t nodeid;

	switch (obp_romvec_version) {

	case OBP_V0_ROMVEC_VERSION:
		return (OBP_BADNODE);

	}
	prom_interpret(finddevice, (int)prom_strlen(path), (int)path,
	    (int)(&nodeid), 0, 0);
	return (nodeid);
}	    

/*
 * Returns the nodeid of /aliases.
 * /aliases exists in OBP >= 2.4 and in Open Firmware.
 * Returns OBP_BADNODE if it doesn't exist and let the caller
 * use an alternate method.
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
 */
dnode_t
prom_optionsnode(void)
{
	static dnode_t node;

	if (node)
		return (node);

	switch (obp_romvec_version) {

	case OBP_V0_ROMVEC_VERSION: {
		pstack_t *stk;
		dnode_t sp[OBP_STACKDEPTH];

		stk = prom_stack_init(sp, sizeof (sp));
		node = prom_findnode_byname(prom_rootnode(), "options", stk);
		prom_stack_fini(stk);
	}
		break;

	default:
		node = prom_finddevice("/options");
	}

	return (node);
}
