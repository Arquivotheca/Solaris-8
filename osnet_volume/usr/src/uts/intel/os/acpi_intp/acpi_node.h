/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_NODE_H
#define	_ACPI_NODE_H

#pragma ident	"@(#)acpi_node.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* tree node data structure interface */

typedef struct node {
	struct node *parent;
	struct node *child;
	struct node *prev;
	struct node *next;
	void *data;
	int elem;
} node_t;


extern node_t *node_new(int elem);
extern void node_free(node_t *np);
extern void node_free_subtree(node_t *np);
extern void node_free_list(node_t *np);

extern int node_add_child(node_t *parent, node_t *child);
extern int node_add_sibling(node_t *np, node_t *sib);
extern void node_unlink_subtree(node_t *np);


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_NODE_H */
