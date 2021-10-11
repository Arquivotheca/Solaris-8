/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_emul.c	1.1	99/06/06 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/prom_emul.h>
#include <sys/obpdefs.h>
#include <sys/kmem.h>
#include <sys/bootconf.h>

#if !defined(KADB) && !defined(I386BOOT) && !defined(IA64BOOT)

static prom_node_t *promif_top;

static prom_node_t *promif_find_node(dnode_t nodeid);
static int getproplen(prom_node_t *pnp, char *name);
static void *getprop(prom_node_t *pnp, char *name);

extern void bcopy(const void *, void *, size_t);

/*
 * Routines for walking the PROMs devinfo tree
 */

static prom_node_t *
promif_top_node(void)
{
	return (promif_top);
}

/*
 * promif_create_prop: Add a property to our copy of the device tree.
 * promif_* functions are internal to promif! This function assumes
 * that it can use val, len directly (the property memory must be
 * allocated by the caller.)
 */
static void
promif_create_prop(prom_node_t *pnp, char *name, void *val, int len)
{
	struct prom_prop *p, *q;

	q = kmem_zalloc(sizeof (*q), KM_SLEEP);
	q->pp_name = kmem_zalloc(prom_strlen(name) + 1, KM_SLEEP);
	(void) prom_strcpy(q->pp_name, name);
	q->pp_val = val;
	q->pp_len = len;

	if (pnp->pn_propp == NULL) {
		pnp->pn_propp = q;
		return;
	}

	for (p = pnp->pn_propp; p->pp_next != NULL; p = p->pp_next)
		/* empty */;

	p->pp_next = q;
}

/*
 * XXX: External version of promif_create_prop ... this is a bug
 * XXX: The OS should not be creating properties in the prom device tree.
 * This function allocates memory for the property value, if necessary
 */
void
promif_create_prop_external(dnode_t nodeid, char *name, void *val, int len)
{
	prom_node_t *pnp;
	void *p = NULL;

	if (!prom_is_p1275())
		return;		/* no tree to get info from */

	pnp = promif_find_node(nodeid);
	if (pnp == NULL)
		return;

	if (len) {
		p = kmem_zalloc(len, KM_SLEEP);
		bcopy(val, p, len);
	}
	promif_create_prop(pnp, name, p, len);
}

/*
 * promif_create_node: create a single prom node from boot's device tree.
 * Copy the properties from boot's copy of the device tree while boot
 * is still in memory.
 */
static prom_node_t *
promif_create_node(dnode_t n)
{
	prom_node_t *pnp;
	char *prvname;
	char propname[OBP_MAXPROPNAME];
	int proplen;
	void *propval;

	pnp = kmem_zalloc(sizeof (prom_node_t), KM_SLEEP);
	pnp->pn_nodeid = n;

	prvname = NULL;

	/*CONSTANTCONDITION*/
	while (1) {
		if (BOP1275_NEXTPROP(bootops, n, prvname, propname) <= 0)
			break;
		if ((proplen = BOP1275_GETPROPLEN(bootops, n, propname)) == -1)
			continue;
		propval = NULL;
		if (proplen != 0) {
			propval = kmem_zalloc(proplen, KM_SLEEP);
			(void) BOP1275_GETPROP(bootops, n, propname, propval,
			    proplen);
		}
		promif_create_prop(pnp, propname, propval, proplen);
		prvname = propname;
	}
	return (pnp);
}

static void promif_create_peers(prom_node_t *pnp, dnode_t n);

static void
promif_create_children(prom_node_t *pnp, dnode_t p)
{
	dnode_t q;
	prom_node_t *qnp;

	/*CONSTANTCONDITION*/
	while (1) {
		q = BOP1275_CHILD(bootops, p);
		if (q == 0)
			break;
		if (BOP1275_GETPROPLEN(bootops, q, "name") <= 0) {
			p = q;
			continue;
		}
		qnp = promif_create_node(q);
		pnp->pn_child = qnp;
		promif_create_peers(qnp, q);
		pnp = qnp;
		p = q;
	}
}

static void
promif_create_peers(prom_node_t *pnp, dnode_t p)
{
	dnode_t q;
	prom_node_t *qnp;

	/*CONSTANTCONDITION*/
	while (1) {
		q = BOP1275_PEER(bootops, p);
		if (q == 0)
			break;
		if (BOP1275_GETPROPLEN(bootops, q, "name") <= 0) {
			p = q;
			continue;
		}
		qnp = promif_create_node(q);
		pnp->pn_sibling = qnp;
		promif_create_children(qnp, q);
		pnp = qnp;
		p = q;
	}
}

/*
 * Create a promif-private copy of boot's device tree.
 */
void
promif_create_device_tree(void)
{
	dnode_t n;
	prom_node_t *pnp;

	if (!prom_is_p1275())
		return;

	n = BOP1275_PEER(bootops, NULL);
	promif_top = pnp = promif_create_node(n);

	promif_create_peers(pnp, n);
	promif_create_children(pnp, n);
}

static prom_node_t *
find_node_work(prom_node_t *pnp, dnode_t n)
{
	prom_node_t *qnp;

	if (pnp->pn_nodeid == n)
		return (pnp);

	if (pnp->pn_child)
		if ((qnp = find_node_work(pnp->pn_child, n)) != NULL)
			return (qnp);

	if (pnp->pn_sibling)
		if ((qnp = find_node_work(pnp->pn_sibling, n)) != NULL)
			return (qnp);

	return (NULL);
}

static prom_node_t *
promif_find_node(dnode_t nodeid)
{

	if (nodeid == OBP_NONODE)
		return (promif_top_node());

	if (promif_top_node() == NULL)
		return (NULL);

	return (find_node_work(promif_top_node(), nodeid));
}

static dnode_t
findnode_byname(prom_node_t *pnp, char *name)
{
	char *s;
	dnode_t n;

	if (pnp == NULL)
		return (OBP_NONODE);

	if ((s = getprop(pnp, "name")) != NULL)
		if (prom_strcmp(s, name) == 0)
			return (pnp->pn_nodeid);

	if ((n = findnode_byname(pnp->pn_child, name)) != OBP_NONODE)
		return (n);

	if ((n = findnode_byname(pnp->pn_sibling, name)) != OBP_NONODE)
		return (n);

	return (OBP_NONODE);
}

dnode_t
promif_findnode_byname(dnode_t n, char *name)
{
	prom_node_t *pnp;

	pnp = promif_find_node(n);
	if (pnp == NULL)
		return (OBP_NONODE);

	return (findnode_byname(pnp, name));
}

dnode_t
promif_nextnode(dnode_t nodeid)
{
	prom_node_t *pnp;

	/*
	 * Note: next(0) returns the root node
	 */
	pnp = promif_find_node(nodeid);
	if (pnp && (nodeid == OBP_NONODE))
		return (pnp->pn_nodeid);
	if (pnp && pnp->pn_sibling)
		return (pnp->pn_sibling->pn_nodeid);

	return (OBP_NONODE);
}

dnode_t
promif_childnode(dnode_t nodeid)
{
	prom_node_t *pnp;

	pnp = promif_find_node(nodeid);
	if (pnp && pnp->pn_child)
		return (pnp->pn_child->pn_nodeid);

	return (OBP_NONODE);
}

/*
 * promif_add_child:  Create a child of the given parent node.
 * XXX: This is unsafe and is here only for historical reasons.
 * XXX: DO NOT USE.
 */

dnode_t
promif_add_child(dnode_t parent, dnode_t child, char *name)
{
	prom_node_t *pnp, *qnp;

	pnp = promif_find_node(parent);
	if (pnp == NULL)
		return (OBP_NONODE);

	qnp = kmem_zalloc(sizeof (prom_node_t), KM_SLEEP);
	qnp->pn_nodeid = child;

	if (pnp->pn_child == NULL)
		pnp->pn_child = qnp;
	else {
		for (pnp = pnp->pn_child; pnp->pn_sibling != NULL;
		    pnp = pnp->pn_sibling)
			/* empty */;
	}

	pnp->pn_sibling = qnp;
	promif_create_prop_external(child, "name", name, prom_strlen(name) + 1);
	return (child);
}

/*
 * Retrieve a PROM property (len and value)
 */

static int
getproplen(prom_node_t *pnp, char *name)
{
	struct prom_prop *propp;

	for (propp = pnp->pn_propp; propp != NULL; propp = propp->pp_next)
		if (prom_strcmp(propp->pp_name, name) == 0)
			return (propp->pp_len);

	return (-1);
}

int
promif_getproplen(dnode_t nodeid, char *name)
{
	prom_node_t *pnp;

	if (!prom_is_p1275())
		return (-1); /* no tree to get info from */

	pnp = promif_find_node(nodeid);
	if (pnp == NULL)
		return (-1);

	return (getproplen(pnp, name));
}

static void *
getprop(prom_node_t *pnp, char *name)
{
	struct prom_prop *propp;

	for (propp = pnp->pn_propp; propp != NULL; propp = propp->pp_next)
		if (prom_strcmp(propp->pp_name, name) == 0)
			return (propp->pp_val);

	return (NULL);
}

int
promif_getprop(dnode_t nodeid, char *name, void *value)
{
	prom_node_t *pnp;
	void *v;
	int len;

	if (!prom_is_p1275())
		return (-1); /* no tree to get info from */

	pnp = promif_find_node(nodeid);
	if (pnp == NULL)
		return (-1);

	len = getproplen(pnp, name);
	if (len > 0) {
		v = getprop(pnp, name);
		bcopy(v, value, len);
	}
	return (len);
}

static char *
nextprop(prom_node_t *pnp, char *name)
{
	struct prom_prop *propp;

	/*
	 * getting next of NULL or a null string returns the first prop name
	 */
	if (name == NULL || *name == '\0')
		if (pnp->pn_propp)
			return (pnp->pn_propp->pp_name);

	for (propp = pnp->pn_propp; propp != NULL; propp = propp->pp_next)
		if (prom_strcmp(propp->pp_name, name) == 0)
			if (propp->pp_next)
				return (propp->pp_next->pp_name);

	return (NULL);
}

char *
promif_nextprop(dnode_t nodeid, char *name, char *next)
{
	prom_node_t *pnp;
	char *s;

	next[0] = '\0';

	if (!prom_is_p1275())
		return (NULL); /* no tree to get info from */

	pnp = promif_find_node(nodeid);
	if (pnp == NULL)
		return (NULL);

	s = nextprop(pnp, name);
	if (s == NULL)
		return (next);

	(void) prom_strcpy(next, s);
	return (next);
}

#define	min(a, b)	(((a) < (b)) ? (a) : (b))

int
promif_bounded_getprop(dnode_t nodeid, char *name, void *value, int len)
{
	prom_node_t *pnp;
	int plen;
	void *p;

	if (!prom_is_p1275())
		return (-1); /* no tree to get info from */

	if (len > 0)
		*(char *)value = '\0';

	pnp = promif_find_node(nodeid);
	if (pnp == NULL)
		return (-1);

	plen = getproplen(pnp, name);
	if (plen > 0) {
		p = getprop(pnp, name);
		bcopy(p, value, min(len, plen));
	}
	return (plen);
}
#endif
