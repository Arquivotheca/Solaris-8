/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)devinfo.c	1.3	99/11/19 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/dditypes.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddipropdefs.h>
#include <sys/modctl.h>

#include <ctype.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_ks.h>

#define	DEVINFO_TREE_INDENT	4	/* Indent for devs one down in tree */
#define	DEVINFO_PROP_INDENT	2	/* Indent for properties */

/*
 * Options for prtconf/devinfo dcmd.
 */
#define	DEVINFO_VERBOSE	0x1

/*
 * Internal flags for prtconf().  Not used by dcmds.
 */
#define	DEVINFO_PARENT	0x2
#define	DEVINFO_CHILD	0x4

void
prtconf_help(void)
{
	mdb_printf("Prints the devinfo tree from a given node.\n"
	    "Without the address of a \"struct devinfo\" given, "
	    "prints from the root;\n"
	    "with an address, prints the parents of, "
	    "and all children of, that address.\n\n"
	    "Switches:\n"
	    "  -v   be verbose - print device property lists\n"
	    "  -p   only print the ancestors of the given node\n"
	    "  -c   only print the children of the given node\n");
}

uintptr_t devinfo_root;		/* Address of root of devinfo tree */

/*
 * Devinfo walker.
 */

typedef struct {
	/*
	 * The "struct dev_info" must be the first thing in this structure.
	 */
	struct dev_info din_dev;

	/*
	 * This is for the benefit of prtconf().
	 */
	int din_depth;
} devinfo_node_t;

typedef struct devinfo_parents_walk_data {
	devinfo_node_t dip_node;
#define	dip_dev dip_node.din_dev
#define	dip_depth dip_node.din_depth
	struct dev_info *dip_end;

	/*
	 * The following three elements are for walking the parents of a node:
	 * "dip_base_depth" is the depth of the given node from the root.
	 *   This starts at 1 (if we're walking devinfo_root), because
	 *   it's the size of the dip_parent_{nodes,addresses} arrays,
	 *   and has to include the given node.
	 * "dip_parent_nodes" is a collection of the parent node structures,
	 *   already read in via mdb_vread().  dip_parent_nodes[0] is the
	 *   root, dip_parent_nodes[1] is a child of the root, etc.
	 * "dip_parent_addresses" holds the vaddrs of all the parent nodes.
	 */
	int dip_base_depth;
	devinfo_node_t *dip_parent_nodes;
	uintptr_t *dip_parent_addresses;
} devinfo_parents_walk_data_t;

int
devinfo_parents_walk_init(mdb_walk_state_t *wsp)
{
	devinfo_parents_walk_data_t *dip;
	uintptr_t addr;
	int i;

	if (wsp->walk_addr == NULL)
		wsp->walk_addr = devinfo_root;
	addr = wsp->walk_addr;

	dip = mdb_alloc(sizeof (devinfo_parents_walk_data_t), UM_SLEEP);
	wsp->walk_data = dip;

	dip->dip_end = (struct dev_info *)wsp->walk_addr;
	dip->dip_depth = 0;
	dip->dip_base_depth = 1;

	do {
		if (mdb_vread(&dip->dip_dev, sizeof (dip->dip_dev),
		    addr) == -1) {
			mdb_warn("failed to read devinfo at %p", addr);
			mdb_free(dip, sizeof (devinfo_parents_walk_data_t));
			wsp->walk_data = NULL;
			return (WALK_ERR);
		}
		addr = (uintptr_t)dip->dip_dev.devi_parent;
		if (addr != 0)
			dip->dip_base_depth++;
	} while (addr != 0);

	addr = wsp->walk_addr;

	dip->dip_parent_nodes = mdb_alloc(
	    dip->dip_base_depth * sizeof (devinfo_node_t), UM_SLEEP);
	dip->dip_parent_addresses = mdb_alloc(
	    dip->dip_base_depth * sizeof (uintptr_t), UM_SLEEP);
	for (i = dip->dip_base_depth - 1; i >= 0; i--) {
		if (mdb_vread(&dip->dip_parent_nodes[i].din_dev,
		    sizeof (struct dev_info), addr) == -1) {
			mdb_warn("failed to read devinfo at %p", addr);
			return (WALK_ERR);
		}
		dip->dip_parent_nodes[i].din_depth = i;
		dip->dip_parent_addresses[i] = addr;
		addr = (uintptr_t)
		    dip->dip_parent_nodes[i].din_dev.devi_parent;
	}

	return (WALK_NEXT);
}

int
devinfo_parents_walk_step(mdb_walk_state_t *wsp)
{
	devinfo_parents_walk_data_t *dip = wsp->walk_data;
	int status;

	if (dip->dip_depth == dip->dip_base_depth)
		return (WALK_DONE);

	status = wsp->walk_callback(
	    dip->dip_parent_addresses[dip->dip_depth],
	    &dip->dip_parent_nodes[dip->dip_depth],
	    wsp->walk_cbdata);

	dip->dip_depth++;
	return (status);
}

void
devinfo_parents_walk_fini(mdb_walk_state_t *wsp)
{
	devinfo_parents_walk_data_t *dip = wsp->walk_data;

	mdb_free(dip->dip_parent_nodes,
	    dip->dip_base_depth * sizeof (devinfo_node_t));
	mdb_free(dip->dip_parent_addresses,
	    dip->dip_base_depth * sizeof (uintptr_t));
	mdb_free(wsp->walk_data, sizeof (devinfo_parents_walk_data_t));
}


typedef struct devinfo_children_walk_data {
	devinfo_node_t dic_node;
#define	dic_dev dic_node.din_dev
#define	dic_depth dic_node.din_depth
	struct dev_info *dic_end;
	int dic_print_first_node;
} devinfo_children_walk_data_t;

int
devinfo_children_walk_init(mdb_walk_state_t *wsp)
{
	devinfo_children_walk_data_t *dic;

	if (wsp->walk_addr == NULL)
		wsp->walk_addr = devinfo_root;

	dic = mdb_alloc(sizeof (devinfo_children_walk_data_t), UM_SLEEP);
	wsp->walk_data = dic;
	dic->dic_end = (struct dev_info *)wsp->walk_addr;

	/*
	 * This could be set by devinfo_walk_init().
	 */
	if (wsp->walk_arg != NULL) {
		dic->dic_depth = (*(int *)wsp->walk_arg - 1);
		dic->dic_print_first_node = 0;
	} else {
		dic->dic_depth = 0;
		dic->dic_print_first_node = 1;
	}

	return (WALK_NEXT);
}

int
devinfo_children_walk_step(mdb_walk_state_t *wsp)
{
	devinfo_children_walk_data_t *dic = wsp->walk_data;
	struct dev_info *v;
	devinfo_node_t *cur;
	uintptr_t addr = wsp->walk_addr;
	int status = WALK_NEXT;

	if (wsp->walk_addr == NULL)
		return (WALK_DONE);

	if (mdb_vread(&dic->dic_dev, sizeof (dic->dic_dev), addr) == -1) {
		mdb_warn("failed to read devinfo at %p", addr);
		return (WALK_DONE);
	}
	cur = &dic->dic_node;

	if (dic->dic_print_first_node == 0)
		dic->dic_print_first_node = 1;
	else
		status = wsp->walk_callback(addr, cur, wsp->walk_cbdata);

	/*
	 * "v" is always a virtual address pointer,
	 *  i.e. can't be deref'ed.
	 */
	v = (struct dev_info *)addr;

	if (dic->dic_dev.devi_child != NULL) {
		v = dic->dic_dev.devi_child;
		dic->dic_depth++;
	} else if (dic->dic_dev.devi_sibling != NULL && v != dic->dic_end) {
		v = dic->dic_dev.devi_sibling;
	} else {
		while (v != NULL && v != dic->dic_end &&
		    dic->dic_dev.devi_sibling == NULL) {
			v = dic->dic_dev.devi_parent;
			if (v == NULL)
				break;

			mdb_vread(&dic->dic_dev,
			    sizeof (struct dev_info), (uintptr_t)v);
			dic->dic_depth--;
		}
		if (v != NULL && v != dic->dic_end)
			v = dic->dic_dev.devi_sibling;
		if (v == dic->dic_end)
			v = NULL;	/* Done */
	}

	wsp->walk_addr = (uintptr_t)v;
	return (status);
}

void
devinfo_children_walk_fini(mdb_walk_state_t *wsp)
{
	mdb_free(wsp->walk_data, sizeof (devinfo_children_walk_data_t));
}

typedef struct devinfo_walk_data {
	mdb_walk_state_t diw_parent, diw_child;
	enum { DIW_PARENT, DIW_CHILD, DIW_DONE } diw_mode;
} devinfo_walk_data_t;

int
devinfo_walk_init(mdb_walk_state_t *wsp)
{
	devinfo_walk_data_t *diw;
	devinfo_parents_walk_data_t *dip;

	diw = mdb_alloc(sizeof (devinfo_walk_data_t), UM_SLEEP);
	diw->diw_parent = *wsp;
	diw->diw_child = *wsp;
	wsp->walk_data = diw;

	diw->diw_mode = DIW_PARENT;

	if (devinfo_parents_walk_init(&diw->diw_parent) == -1) {
		mdb_free(diw, sizeof (devinfo_walk_data_t));
		return (WALK_ERR);
	}

	/*
	 * This is why the "devinfo" walker needs to be marginally
	 * complicated - the child walker needs this initialization
	 * data, and the best way to get it is out of the parent walker.
	 */
	dip = diw->diw_parent.walk_data;
	diw->diw_child.walk_arg = &dip->dip_base_depth;

	if (devinfo_children_walk_init(&diw->diw_child) == -1) {
		devinfo_parents_walk_fini(&diw->diw_parent);
		mdb_free(diw, sizeof (devinfo_walk_data_t));
		return (WALK_ERR);
	}

	return (WALK_NEXT);
}

int
devinfo_walk_step(mdb_walk_state_t *wsp)
{
	devinfo_walk_data_t *diw = wsp->walk_data;
	int status = WALK_NEXT;

	if (diw->diw_mode == DIW_PARENT) {
		status = devinfo_parents_walk_step(&diw->diw_parent);
		if (status != WALK_NEXT) {
			/*
			 * Keep on going even if the parents walk hit an error.
			 */
			diw->diw_mode = DIW_CHILD;
			status = WALK_NEXT;
		}
	} else if (diw->diw_mode == DIW_CHILD) {
		status = devinfo_children_walk_step(&diw->diw_child);
		if (status != WALK_NEXT) {
			diw->diw_mode = DIW_DONE;
			status = WALK_DONE;
		}
	} else
		status = WALK_DONE;

	return (status);
}

void
devinfo_walk_fini(mdb_walk_state_t *wsp)
{
	devinfo_walk_data_t *diw = wsp->walk_data;

	devinfo_children_walk_fini(&diw->diw_child);
	devinfo_parents_walk_fini(&diw->diw_parent);
	mdb_free(diw, sizeof (devinfo_walk_data_t));
}

typedef struct devnames_walk {
	struct devnames *dnw_names;
	int dnw_ndx;
	int dnw_devcnt;
	uintptr_t dnw_base;
	uintptr_t dnw_size;
} devnames_walk_t;

int
devnames_walk_init(mdb_walk_state_t *wsp)
{
	devnames_walk_t *dnw;
	int devcnt;
	uintptr_t devnamesp;

	if (wsp->walk_addr != NULL) {
		mdb_warn("devnames walker only supports global walks\n");
		return (WALK_ERR);
	}

	if (mdb_readvar(&devcnt, "devcnt") == -1) {
		mdb_warn("failed to read 'devcnt'");
		return (WALK_ERR);
	}

	if (mdb_readvar(&devnamesp, "devnamesp") == -1) {
		mdb_warn("failed to read 'devnamesp'");
		return (WALK_ERR);
	}

	dnw = mdb_zalloc(sizeof (devnames_walk_t), UM_SLEEP);
	dnw->dnw_size = sizeof (struct devnames) * devcnt;
	dnw->dnw_devcnt = devcnt;
	dnw->dnw_base = devnamesp;
	dnw->dnw_names = mdb_alloc(dnw->dnw_size, UM_SLEEP);

	if (mdb_vread(dnw->dnw_names, dnw->dnw_size, dnw->dnw_base) == -1) {
		mdb_warn("couldn't read devnames array at %p", devnamesp);
		return (WALK_ERR);
	}

	wsp->walk_data = dnw;
	return (WALK_NEXT);
}

int
devnames_walk_step(mdb_walk_state_t *wsp)
{
	devnames_walk_t *dnw = wsp->walk_data;
	int status;

	if (dnw->dnw_ndx == dnw->dnw_devcnt)
		return (WALK_DONE);

	status = wsp->walk_callback(dnw->dnw_ndx * sizeof (struct devnames) +
	    dnw->dnw_base, &dnw->dnw_names[dnw->dnw_ndx], wsp->walk_cbdata);

	dnw->dnw_ndx++;
	return (status);
}

void
devnames_walk_fini(mdb_walk_state_t *wsp)
{
	devnames_walk_t *dnw = wsp->walk_data;

	mdb_free(dnw->dnw_names, dnw->dnw_size);
	mdb_free(dnw, sizeof (devnames_walk_t));
}

int
devi_next_walk_step(mdb_walk_state_t *wsp)
{
	struct dev_info di;
	int status;

	if (wsp->walk_addr == NULL)
		return (WALK_DONE);

	if (mdb_vread(&di, sizeof (di), wsp->walk_addr) == -1)
		return (WALK_DONE);

	status = wsp->walk_callback(wsp->walk_addr, &di, wsp->walk_cbdata);
	wsp->walk_addr = (uintptr_t)di.devi_next;
	return (status);
}

/*
 * Helper functions.
 */

static int
is_printable_string(char *prop_value)
{
	while (*prop_value != 0)
		if (!isprint(*prop_value++))
			return (0);
	return (1);
}

static void
devinfo_print_props(char *name, ddi_prop_t *p)
{
	if (p == NULL)
		return;

	mdb_inc_indent((sizeof (uintptr_t) * 2) + 1);

	mdb_printf("%s properties at %a:\n", name, p);
	mdb_inc_indent(DEVINFO_PROP_INDENT);
	while (p != NULL) {
		ddi_prop_t prop;
		char prop_name[128];

		if (mdb_vread(&prop, sizeof (prop), (uintptr_t)p) == -1) {
			mdb_warn("couldn't read property");
			break;
		}
		if (mdb_readstr(prop_name, sizeof (prop_name),
		    (uintptr_t)prop.prop_name) == -1) {
			mdb_warn("couldn't read property name");
			goto next;
		}

		mdb_printf("%s", prop_name);

		if (prop.prop_len > 0) {
			int flags, i;
			char *prop_value;
			mdb_printf(": ");

			prop_value = mdb_alloc(prop.prop_len, UM_SLEEP);
			if (mdb_vread(prop_value, prop.prop_len,
				    (uintptr_t)prop.prop_val) == -1) {
				mdb_warn("couldn't read value");
				mdb_free(prop_value, prop.prop_len);
				goto next;
			}
			flags = prop.prop_flags & DDI_PROP_TYPE_MASK;
			if ((prop_value[prop.prop_len - 1] == 0) &&
			    ((flags == DDI_PROP_TYPE_STRING) ||
			    ((flags & DDI_PROP_TYPE_BYTE) &&
			    is_printable_string(prop_value)))) {
				mdb_printf("\"%s\"", prop_value);
			} else if ((flags == DDI_PROP_TYPE_INT) &&
			    (prop.prop_len == sizeof (int))) {
				/* LINTED - alignment */
				mdb_printf("0x%x", *(int *)prop_value);
			} else {
				mdb_printf("<");
				for (i = 0; i < prop.prop_len; i++) {
					mdb_printf("%02x%s",
					    (unsigned char)prop_value[i],
					    ((i & 3) == 3) ? "." : "");
				}
				mdb_printf(">");
			}
			mdb_free(prop_value, prop.prop_len);
		}

		if (prop.prop_dev != DDI_DEV_T_NONE) {
			mdb_printf(" (device: ");
			if (prop.prop_dev == DDI_DEV_T_ANY)
				mdb_printf("any");
			else if (prop.prop_dev == DDI_MAJOR_T_UNKNOWN)
				mdb_printf("unknown");
			else
				mdb_printf("<0x%x/0x%08x>",
				    getmajor(prop.prop_dev),
				    getminor(prop.prop_dev));
			mdb_printf(")");
		}
next:
		mdb_printf("\n");
		p = prop.prop_next;
	}

	mdb_dec_indent(DEVINFO_PROP_INDENT + (sizeof (uintptr_t) * 2) + 1);
}

typedef struct devinfo_cb_data {
	uintptr_t di_base;
	uint_t di_flags;
} devinfo_cb_data_t;

/*
 * Yet to be added:
 *
 * struct devnames *devnamesp;
 *   <sys/autoconf.h>:26		- type definition
 *   uts/common/os/modctl.c:106		- devnamesp definition
 *
 * int devcnt;
 *   uts/common/io/conf.c:62		- devcnt definition
 *
 * "devnamesp" is an array of "devcnt" "devnames" structures,
 *   indexed by major number.  This gets us "prtconf -D".
 */
static int
devinfo_print(uintptr_t addr, struct dev_info *dev, devinfo_cb_data_t *data)
{
	/*
	 * We know the walker passes us extra data after the dev_info.
	 */
	devinfo_node_t *din = (devinfo_node_t *)dev;
	char binding_name[128];

	if (mdb_readstr(binding_name, sizeof (binding_name),
	    (uintptr_t)dev->devi_binding_name) == -1) {
		mdb_warn("failed to read binding_name");
		return (WALK_ERR);
	}
	mdb_inc_indent(din->din_depth * DEVINFO_TREE_INDENT);
	if (addr == data->di_base)
		mdb_printf("%<b>");
	mdb_printf("%-0?p %s", addr, binding_name);
	if (addr == data->di_base)
		mdb_printf("%</b>");
	if (dev->devi_instance >= 0)
		mdb_printf(", instance #%d", dev->devi_instance);
	if (dev->devi_ops == NULL)
		mdb_printf(" (driver not attached)");
	mdb_printf("\n");
	if (data->di_flags & DEVINFO_VERBOSE) {
		devinfo_print_props("Driver", dev->devi_drv_prop_ptr);
		devinfo_print_props("System", dev->devi_sys_prop_ptr);
		devinfo_print_props("Hardware", dev->devi_hw_prop_ptr);
	}
	mdb_dec_indent(din->din_depth * DEVINFO_TREE_INDENT);
	return (WALK_NEXT);
}

/*ARGSUSED*/
int
prtconf(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	devinfo_cb_data_t data;
	int status;

	data.di_flags = DEVINFO_PARENT | DEVINFO_CHILD;

	if (mdb_getopts(argc, argv,
	    'v', MDB_OPT_SETBITS, DEVINFO_VERBOSE, &data.di_flags,
	    'p', MDB_OPT_CLRBITS, DEVINFO_CHILD, &data.di_flags,
	    'c', MDB_OPT_CLRBITS, DEVINFO_PARENT, &data.di_flags, NULL) != argc)
		return (DCMD_USAGE);

	if ((flags & DCMD_ADDRSPEC) == 0)
		addr = devinfo_root;

	data.di_base = addr;
	mdb_printf("%<u>%-?s %-50s%</u>\n", "DEVINFO", "NAME");

	if ((data.di_flags & (DEVINFO_PARENT | DEVINFO_CHILD)) ==
	    (DEVINFO_PARENT | DEVINFO_CHILD)) {
		status = mdb_pwalk("devinfo",
		    (mdb_walk_cb_t)devinfo_print, &data, addr);
	} else if (data.di_flags & DEVINFO_PARENT) {
		status = mdb_pwalk("devinfo_parents",
		    (mdb_walk_cb_t)devinfo_print, &data, addr);
	} else if (data.di_flags & DEVINFO_CHILD) {
		status = mdb_pwalk("devinfo_children",
		    (mdb_walk_cb_t)devinfo_print, &data, addr);
	} else {
		devinfo_node_t din;
		if (mdb_vread(&din.din_dev, sizeof (din.din_dev), addr) == -1) {
			mdb_warn("failed to read device");
			return (DCMD_ERR);
		}
		din.din_depth = 0;
		return (devinfo_print(addr, (struct dev_info *)&din, &data));
	}

	if (status == -1) {
		mdb_warn("couldn't walk devinfo tree");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
int
devinfo(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	devinfo_node_t din;
	devinfo_cb_data_t data;

	data.di_flags = DEVINFO_VERBOSE;
	data.di_base = addr;

	if (mdb_getopts(argc, argv, 'q', MDB_OPT_CLRBITS,
	    DEVINFO_VERBOSE, &data.di_flags, NULL) != argc)
		return (DCMD_USAGE);

	if ((flags & DCMD_ADDRSPEC) == 0) {
		mdb_warn(
		    "devinfo doesn't give global information (try prtconf)\n");
		return (DCMD_ERR);
	}

	if (mdb_vread(&din.din_dev, sizeof (din.din_dev), addr) == -1) {
		mdb_warn("failed to read device");
		return (DCMD_ERR);
	}

	din.din_depth = 0;
	return (devinfo_print(addr, (struct dev_info *)&din, &data));
}

/*ARGSUSED*/
int
m2d_walk_dinfo(uintptr_t addr, struct dev_info *di, char *mod_name)
{
	char name[MODMAXNAMELEN];

	if (mdb_readstr(name, MODMAXNAMELEN,
	    (uintptr_t)di->devi_binding_name) == -1) {
		mdb_warn("couldn't read devi_binding_name at %p",
		    di->devi_binding_name);
		return (WALK_ERR);
	}

	if (strcmp(name, mod_name) == 0)
		mdb_printf("%p\n", addr);

	return (WALK_NEXT);
}

/*ARGSUSED*/
int
modctl2devinfo(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	struct modctl modctl;
	char name[MODMAXNAMELEN];

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_vread(&modctl, sizeof (modctl), addr) == -1) {
		mdb_warn("couldn't read modctl at %p", addr);
		return (DCMD_ERR);
	}

	if (mdb_readstr(name, MODMAXNAMELEN,
	    (uintptr_t)modctl.mod_modname) == -1) {
		mdb_warn("couldn't read modname at %p", modctl.mod_modname);
		return (DCMD_ERR);
	}

	if (mdb_walk("devinfo", (mdb_walk_cb_t)m2d_walk_dinfo, name) == -1) {
		mdb_printf("couldn't walk devinfo");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

static int
major_to_addr(major_t major, uintptr_t *vaddr)
{
	uint_t devcnt;
	uintptr_t devnamesp;

	if (mdb_readvar(&devcnt, "devcnt") == -1) {
		mdb_warn("failed to read 'devcnt'");
		return (-1);
	}

	if (mdb_readvar(&devnamesp, "devnamesp") == -1) {
		mdb_warn("failed to read 'devnamesp'");
		return (-1);
	}

	if (major >= devcnt) {
		mdb_warn("%x is out of range [0x0-0x%x]\n", major, devcnt - 1);
		return (-1);
	}

	*vaddr = devnamesp + (major * sizeof (struct devnames));
	return (0);
}

/*ARGSUSED*/
int
devnames(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	static const mdb_bitmask_t dn_flag_bits[] = {
		{ "DN_CONF_PARSED", DN_CONF_PARSED, DN_CONF_PARSED },
		{ "DN_DEVI_MADE", DN_DEVI_MADE,	DN_DEVI_MADE },
		{ "DN_WALKED_TREE", DN_WALKED_TREE, DN_WALKED_TREE },
		{ "DN_DEVS_ATTACHED", DN_DEVS_ATTACHED, DN_DEVS_ATTACHED },
		{ "DN_BUSY_LOADING", DN_BUSY_LOADING, DN_BUSY_LOADING },
		{ "DN_BUSY_UNLOADING", DN_BUSY_UNLOADING, DN_BUSY_UNLOADING },
		{ "DN_BUSY_CHANGING_BITS", DN_BUSY_CHANGING_BITS,
		    DN_BUSY_CHANGING_BITS },
		{ NULL, 0, 0 }
	};

	const mdb_arg_t *argp = NULL;
	uint_t opt_v = FALSE;
	major_t major;
	size_t i;

	char name[MODMAXNAMELEN + 1];
	struct devnames dn;

	if ((i = mdb_getopts(argc, argv,
	    'v', MDB_OPT_SETBITS, TRUE, &opt_v, NULL)) != argc) {
		if (argc - i > 1)
			return (DCMD_USAGE);
		argp = &argv[i];
	}

	if (!(flags & DCMD_ADDRSPEC)) {
		if (argp == NULL) {
			if (mdb_walk_dcmd("devnames", "devnames", argc, argv)) {
				mdb_warn("failed to walk devnames");
				return (DCMD_ERR);
			}
			return (DCMD_OK);
		}

		if (argp->a_type == MDB_TYPE_IMMEDIATE)
			major = (major_t)argp->a_un.a_val;
		else
			major = (major_t)mdb_strtoull(argp->a_un.a_str);

		if (major_to_addr(major, &addr) == -1)
			return (DCMD_ERR);
	}

	if (mdb_vread(&dn, sizeof (struct devnames), addr) == -1) {
		mdb_warn("failed to read devnames struct at %p", addr);
		return (DCMD_ERR);
	}

	if (DCMD_HDRSPEC(flags)) {
		if (opt_v)
			mdb_printf("%<u>%-16s%</u>\n", "NAME");
		else
			mdb_printf("%<u>%-16s %-?s%</u>\n", "NAME", "DN_HEAD");
	}

	if ((flags & DCMD_LOOP) && (dn.dn_name == NULL))
		return (DCMD_OK); /* Skip empty slots if we're printing table */

	if (mdb_readstr(name, sizeof (name), (uintptr_t)dn.dn_name) == -1)
		(void) mdb_snprintf(name, sizeof (name), "0x%p", dn.dn_name);

	if (opt_v) {
		mdb_printf("%<b>%-16s%</b>\n", name);
		mdb_inc_indent(2);

		mdb_printf("          flags %b\n", dn.dn_flags, dn_flag_bits);
		mdb_printf("             pl %p\n", (void *)dn.dn_pl);
		mdb_printf("       circular %d\n", dn.dn_circular);
		mdb_printf("    busy_thread %p\n", dn.dn_busy_thread);
		mdb_printf("           head %p\n", dn.dn_head);
		mdb_printf("       instance %d\n", dn.dn_instance);
		mdb_printf("         inlist %p\n", dn.dn_inlist);
		mdb_printf("global_prop_ptr %p\n", dn.dn_global_prop_ptr);
		devinfo_print_props("", dn.dn_global_prop_ptr);

		mdb_dec_indent(2);
	} else
		mdb_printf("%-16s %-?p\n", name, dn.dn_head);

	return (DCMD_OK);
}

/*ARGSUSED*/
int
name2major(uintptr_t vaddr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	major_t major;

	if (flags & DCMD_ADDRSPEC)
		return (DCMD_USAGE);

	if (argc != 1 || argv->a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	if (mdb_name_to_major(argv->a_un.a_str, &major) != 0) {
		mdb_warn("failed to convert name to major number\n");
		return (DCMD_ERR);
	}

	mdb_printf("0x%x", major);
	return (DCMD_OK);
}

/*ARGSUSED*/
int
major2name(uintptr_t major, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const char *name;

	switch (argc) {
	case 0:
		if (!(flags & DCMD_ADDRSPEC))
			return (DCMD_USAGE);
		break;

	case 1:
		if (flags & DCMD_ADDRSPEC)
			return (DCMD_USAGE);

		if (argv[0].a_type == MDB_TYPE_IMMEDIATE)
			major = (uintptr_t)argv[0].a_un.a_val;
		else
			major = (uintptr_t)mdb_strtoull(argv->a_un.a_str);
		break;

	default:
		return (DCMD_USAGE);
	}

	if ((name = mdb_major_to_name((major_t)major)) == NULL) {
		mdb_warn("failed to convert major number to name\n");
		return (DCMD_ERR);
	}

	mdb_printf("%s", name);
	return (DCMD_OK);
}

/*ARGSUSED*/
int
softstate(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	struct i_ddi_soft_state ss;
	uintptr_t ssaddr;
	void *statep;
	int instance;

	if (argc != 1) {
		return (DCMD_USAGE);
	}

	if (argv[0].a_type == MDB_TYPE_IMMEDIATE)
		instance = (int)argv[0].a_un.a_val;
	else
		instance = (int)mdb_strtoull(argv->a_un.a_str);

	if (mdb_vread(&ssaddr, sizeof (ssaddr), addr) == -1) {
		mdb_warn("couldn't dereference soft state head at %p", addr);
		return (DCMD_ERR);
	}

	if (mdb_vread(&ss, sizeof (ss), ssaddr) == -1) {
		mdb_warn("couldn't read soft state head at %p", ssaddr);
		return (DCMD_ERR);
	}

	if (instance >= ss.n_items) {
		mdb_warn("no instance %d in soft state", instance);
		return (DCMD_ERR);
	}

	if (mdb_vread(&statep, sizeof (statep), (uintptr_t)ss.array +
	    (sizeof (statep) * instance)) == -1) {
		mdb_warn("couldn't read soft state head at %p", addr);
		return (DCMD_ERR);
	}

	mdb_printf("%p\n", statep);
	return (DCMD_OK);
}

/*ARGSUSED*/
int
devbindings(uintptr_t vaddr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	struct devnames dn;
	uintptr_t dn_addr;
	major_t major;

	if ((flags & DCMD_ADDRSPEC) || (argc != 1) ||
	    (argv->a_type != MDB_TYPE_STRING))
		return (DCMD_USAGE);

	if (mdb_name_to_major(argv->a_un.a_str, &major) != 0) {
		mdb_warn("failed to get major number for %s", argv->a_un.a_str);
		return (DCMD_ERR);
	}

	if (major_to_addr(major, &dn_addr) != 0)
		return (DCMD_ERR);

	if (mdb_vread(&dn, sizeof (struct devnames), dn_addr) == -1) {
		mdb_warn("couldn't read devnames array at %p", dn_addr);
		return (DCMD_ERR);
	}

	if (mdb_pwalk_dcmd("devi_next", "devinfo", 0, NULL,
	    (uintptr_t)dn.dn_head) != 0) {
		mdb_warn("couldn't walk the devinfo chain at %p", dn.dn_head);
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}
