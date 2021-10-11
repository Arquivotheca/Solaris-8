/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PROM_EMUL_H
#define	_SYS_PROM_EMUL_H

#pragma ident	"@(#)prom_emul.h	1.4	99/06/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The following structure describes a property attached to a node
 * in the in-kernel copy of the PROM device tree.
 */
struct prom_prop {
	struct prom_prop *pp_next;
	char		 *pp_name;
	int		 pp_len;
	void		 *pp_val;
};

/*
 * The following structure describes a node in the in-kernel copy
 * of the PROM device tree.
 */
struct prom_node {
	dnode_t	pn_nodeid;
	struct prom_prop *pn_propp;
	struct prom_node *pn_child;
	struct prom_node *pn_sibling;
};

typedef struct prom_node prom_node_t;

/*
 * These are promif emulation functions, intended only for promif use
 */
extern void promif_create_device_tree(void);

extern dnode_t promif_findnode_byname(dnode_t n, char *name);
extern dnode_t promif_nextnode(dnode_t n);
extern dnode_t promif_childnode(dnode_t n);

extern int promif_getproplen(dnode_t n, char *name);
extern int promif_getprop(dnode_t n,  char *name, void *value);
extern int promif_bounded_getprop(dnode_t, char *name, void *value, int len);
char *promif_nextprop(dnode_t n, char *previous, char *next);

/*
 * XXX: The following functions are unsafe and unecessary, and should be
 * XXX: removed. OS created nodes belong in the OS copy of the device tree.
 * XXX: The OS should not be creating nodes in the prom's device tree!
 */
extern dnode_t promif_add_child(dnode_t parent, dnode_t child, char *name);
extern void promif_create_prop_external(dnode_t, char *name, void *, int);

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROM_EMUL_H */
