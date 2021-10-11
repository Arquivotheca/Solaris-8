/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Bus operations callout table and error function definitions
 */

#ident	"@(#)bus.c	1.10	99/10/07 SMI"

#include "types.h"
#include "menu.h"
#include "boot.h"
#include "devdb.h"
#include "bus.h"
#include "isa1275.h"
#include "pci1275.h"
#include "pnp1275.h"
#include "80421275.h"

struct bus_ops_s bus_ops[RES_BUS_NO + 1] = {
	/* NULL */
	{ 0, 0, 0, 0, 0, 0},

	/* ISA */
	{ build_node_isa1275, parse_bootpath_isa1275, is_bdp_bootpath_isa1275,
	    is_bp_bootpath_isa1275, get_path_from_bdp_isa1275,
	    bp_to_desc_isa1275},

	/* EISA */
	{ build_node_isa1275, parse_bootpath_isa1275, is_bdp_bootpath_isa1275,
	    is_bp_bootpath_isa1275, get_path_from_bdp_isa1275,
	    bp_to_desc_isa1275},

	/* PCI */
	{ build_node_pci1275, parse_bootpath_pci1275, is_bdp_bootpath_pci1275,
	    is_bp_bootpath_pci1275, get_path_from_bdp_pci1275,
	    bp_to_desc_pci1275},

	/* PCMCIA */
	{ 0, 0, 0, 0, 0, 0},

	/* PNPISA */
	{ build_node_pnp1275, parse_bootpath_pnp1275, is_bdp_bootpath_pnp1275,
	    is_bp_bootpath_pnp1275, get_path_from_bdp_pnp1275,
	    bp_to_desc_pnp1275},

	/* MCA */
	{ 0, 0, 0, 0, 0, 0},

	/* i8042 (keyboard/mouse) */
	{ build_node_i8042_1275, parse_bootpath_i8042_1275,
	    is_bdp_bootpath_i8042_1275,
	    is_bp_bootpath_i8042_1275, get_path_from_bdp_i8042_1275,
	    bp_to_desc_i8042_1275},
};
