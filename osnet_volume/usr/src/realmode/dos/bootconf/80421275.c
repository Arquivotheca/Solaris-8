/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)80421275.c	1.10	99/10/26 SMI"

/*
 * Support for i8042 keyboard/mouse controller.
 */

#include "types.h"
#include "devdb.h"
#include "boot.h"
#include "enum.h"
#include "tree.h"
#include "80421275.h"

static char i8042_bus_path[100];
static char i8042_unit_address[] = "1,60";
static char i8042_name[] = "i8042";
static char i8042_reg[] = "1,0x60,1,1,0x64,1";

static char *node_name_i8042_1275(Board *bp);
static char *unit_address_i8042_1275(Board *bp);

void
reset_i8042_1275(void)
{
	memset(i8042_bus_path, 0, sizeof (i8042_bus_path));
}

void
init_i8042_1275(void)
{
	reset_i8042_1275();
}

void
build_bus_node_i8042_1275(void)
{
	char buf[100];
	char buf2[100];

	if (i8042_bus_path[0] != '\0')
		return;

	(void) sprintf(buf, "/%s/%s", busen[ffbs(Main_bus)], i8042_name);
	(void) sprintf(i8042_bus_path, "%s@%s", buf, i8042_unit_address);
	(void) sprintf(buf2, "mknod %s %s\n", buf, i8042_reg);
	out_bop(buf2);
	(void) sprintf(buf, "setprop \\$at %s\n", i8042_unit_address);
	out_bop(buf);

	out_bop("setbinprop interrupts 1,12\n");
}

int
build_node_i8042_1275(Board *bp)
{
	char buf[100];
	char *node_name;
	char *unit_address;
	devtrans *dtp = bp->dbentryp;

	build_bus_node_i8042_1275();

	node_name = node_name_i8042_1275(bp);
	unit_address = unit_address_i8042_1275(bp);

	(void) sprintf(buf, "mknod %s/%s %s\n",
		i8042_bus_path, node_name, unit_address);
	out_bop(buf);

	(void) sprintf(buf, "setprop \\$at \"%s\"\n", unit_address);
	out_bop(buf);

	/*
	 * Set the descriptive name (from the master file)
	 * in the model property
	 */
	if (dtp) {
		(void) sprintf(buf, "setprop model \"%s\"\n", dtp->dname);
		out_bop(buf);
	}
	return (0);
}

int
parse_bootpath_i8042_1275(char *path, char **rest)
{
	printf("parse_bootpath_i8042_1275:  shouldn't be here\n");
	return (1);
}

int
is_bdp_bootpath_i8042_1275(bef_dev *bdp)
{
	printf("is_bdp_bootpath_i8042_1275:  shouldn't be here\n");
	return (FALSE);
}

int
is_bp_bootpath_i8042_1275(Board *bp, struct bdev_info *dip)
{
	printf("is_bp_bootpath_i8042_1275:  shouldn't be here\n");
	return (FALSE);
}

void
get_path_from_bdp_i8042_1275(bef_dev *bdp, char *path, int compat)
{
	Board *bp = bdp->bp;

	(void) sprintf(path, "/%s/%s@%s/%s@%s",
		busen[ffbs(Main_bus)],
		i8042_name, i8042_unit_address,
		node_name_i8042_1275(bp), unit_address_i8042_1275(bp));
}

char *
bp_to_desc_i8042_1275(Board *bp, int verbose)
{
	devtrans *dtp = bp->dbentryp;
	static char buf[80];

	if (dtp != NULL && dtp->dname != NULL) {
		if (bp->flags & BRDF_ACPI)
			sprintf(buf, "ISA: ACPI: %s", dtp->dname);
		else
			sprintf(buf, "ISA: %s", dtp->dname);
		return (buf);
	} else
		return ("ISA: Unknown keyboard/mouse device");
}

static char *
unit_address_i8042_1275(Board *bp)
{
	static char buf[(sizeof (bp->slot)) * 3 + 1];
	sprintf(buf, "%u", bp->slot);
	return (buf);
}

static char *
node_name_i8042_1275(Board *bp)
{
	devtrans *dtp = bp->dbentryp;

	if (dtp != NULL && dtp->unix_driver != NULL)
		return (dtp->unix_driver);
	else
		return ("device");
}
