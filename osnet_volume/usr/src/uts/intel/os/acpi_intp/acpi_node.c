/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_node.c	1.1	99/05/21 SMI"


/* tree node data structure */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#ifdef ACPI_USER
#include <stdlib.h>
#include <strings.h>
#endif

#include "acpi_exc.h"
#include "acpi_node.h"


node_t *
node_new(int elem)
{
	node_t *new;

	if ((new = kmem_alloc(sizeof (node_t), KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	bzero(new, sizeof (node_t));
	new->elem = elem;
	return (new);
}

void
node_free(node_t *np)
{
	if (np)
		kmem_free(np, sizeof (node_t));
}

void
node_free_subtree(node_t *np)
{
	node_t *ptr, *save;

	if (np == NULL)
		return;
	if (np->child)
		for (ptr = np->child; ptr; ptr = save) {
			save = ptr->next;
			node_free_subtree(ptr);
		}
	node_free(np);
}

void
node_free_list(node_t *np)
{
	node_t *save;

	if (np == NULL)
		return;
	for (; np; np = save) {
		save = np->next;
		node_free_subtree(np);
	}
}

/*LINTLIBRARY*/
int
node_add_child(node_t *parent, node_t *child)
{
	if (child->parent)	/* already connected */
		return (exc_code(ACPI_EINTERNAL));
	child->parent = parent;
	if (parent->child)
		return (node_add_sibling(parent->child, child));
	else
		parent->child = child;
	return (ACPI_OK);
}

/* if too slow, keep track of end of list in parent */
int
node_add_sibling(node_t *np, node_t *sib)
{
	node_t *ptr, *trail;

	if (sib->prev || sib->next) /* already connected */
		return (exc_code(ACPI_EINTERNAL));
	for (trail = np, ptr = np->next; ptr; trail = ptr, ptr = ptr->next)
		;
	trail->next = sib;
	sib->prev = trail;
	return (ACPI_OK);
}

void
node_unlink_subtree(node_t *np)
{
	if (np->parent && np->parent->child == np)
		np->parent->child = np->next;
	if (np->prev)
		np->prev->next = np->next;
	if (np->next)
		np->next->prev = np->prev;
}


/* eof */
