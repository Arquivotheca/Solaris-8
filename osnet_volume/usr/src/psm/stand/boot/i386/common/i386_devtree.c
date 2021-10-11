/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)i386_devtree.c	1.3	99/06/22 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/salib.h>
#include "devtree.h"

extern struct bootops bootops;	    /* Ptr to bootop vector		*/
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);

/* BEGIN CSTYLED */
/*
 *  Device Tree Skeleton:
 *
 *     The "standard" device tree nodes are allocated statically.  Tree
 *     linkage is initialized statically as well, but properties are
 *     established via the "setup_devtree" routine.  The initial tree
 *     looks like this:
 *
 *			            [ root ]
 *			               |
 *			               |
 *    +-------+-------+----------+-----+-----+---------+---------+--------+
 *    |       |       |          |           |         |         |        |
 *    |       |       |          |           |         |         |        |
 *  +boot  aliases  chosen  i86pc-memory  i86pc-mmu  openprom  options  packages
 *    |
 *    |
 *  memory
 */
/* END CSTYLED */

struct dnode devtree[DEFAULT_NODES] = {

	{ (struct dnode *)0, &boot_node, (struct dnode *)0 },	/* root */

	{ &root_node, &bootmem_node,	&alias_node	   },	/* +boot */
	{ &boot_node, (struct dnode *)0, (struct dnode *)0 },	/* memory */

	{ &root_node, (struct dnode *)0, &chosen_node	   },	/* aliases */
	{ &root_node, (struct dnode *)0, &mem_node	   },	/* chosen  */
	{ &root_node, (struct dnode *)0, &mmu_node	   },	/* i86-mem */
	{ &root_node, (struct dnode *)0, &prom_node	   },	/* i86-mmu */
	{ &root_node, (struct dnode *)0, &option_node	   },	/* openprom */
	{ &root_node, (struct dnode *)0, &package_node	   },	/* options */
	{ &root_node, (struct dnode *)0, &delayed_node	   },	/* packages */
	{ &root_node, (struct dnode *)0, &itu_node	   },	/* delayed */
	{ &root_node, (struct dnode *)0, (struct dnode *)0 }	/* itu-props */
};

static char *node_names[DEFAULT_NODES] = {
	"", "+boot", "memory", "aliases", "chosen",
	"i86pc-memory", "i86pc-mmu", "openprom",
	"options", "packages", "delayed-writes", "itu-props"
};

void
setup_devtree()
{
	/*
	 *  Initialize the device tree:
	 *
	 *    P1275 "standard" nodes are statically allocated and linked to-
	 *    gether in the tables above.  We name them here.
	 */
	int n;
	struct dnode *dnp;
	extern void acpi_setprop(void);

	for (n = 0; n < DEFAULT_NODES; n++) {
		/* Set up the default node names */
		dnp = &devtree[n];
		dnp->dn_nodeid = dnode2phandle(dnp);
		(void) bsetprop(&bootops, "name", node_names[n],
			strlen(node_names[n]) + 1, dnp->dn_nodeid);
	}

	/* Give the root node its name */
	/* TBD:  We should really be probing the bios to find mfgr/model! */
	(void) bsetprop(&bootops, "name", "i86pc", 6, root_node.dn_nodeid);

	for (n = 0; n < pseudo_prop_count; n++) {
		/*
		 *  Warning: Hack alert!!
		 *
		 *  Set dummy pseudo-properties at selected nodes so that the
		 *  ".properties" commands will appear to display pseudo
		 *  properties associated with these nodes.  These properties
		 *  are not actually stored in the nodes' property lists, but
		 *  are extracted from boot memory by special routines
		 *  (see "bootprop.c").
		 */
		char buf[MAX1275NAME+1];

		(void) sprintf(buf, "%s ", pseudo_props[n].name);
		(void) bsetprop(&bootops, buf, 0, 0,
			pseudo_props[n].node->dn_nodeid);
	}

	/*
	 *  Set default values for some important properties
	 */
	(void) bsetprop(&bootops, "reg", "0,0", 4, mmu_node.dn_nodeid);
	(void) bsetprop(&bootops, "reg", "0,0", 4, package_node.dn_nodeid);

#define	kbd "keyboard"		/* Default input device	*/
#define	scr "screen"		/* Default ouput device */
#define	mod "9600,8,n,1,-"	/* Default serial modes */
#define	compat "i86pc"		/* Default compatible property */

	(void) bsetprop(&bootops, "input-device", kbd, sizeof (kbd),
		option_node.dn_nodeid);
	(void) bsetprop(&bootops, "output-device", scr, sizeof (scr),
		option_node.dn_nodeid);
	(void) bsetprop(&bootops, "ttya-mode", mod, sizeof (mod),
		option_node.dn_nodeid);
	(void) bsetprop(&bootops, "ttyb-mode", mod, sizeof (mod),
		option_node.dn_nodeid);
	(void) bsetprop(&bootops, "compatible", compat, sizeof (compat),
		root_node.dn_nodeid);
	acpi_setprop();
}
