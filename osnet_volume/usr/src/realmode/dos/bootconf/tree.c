/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * tree.c -- routines to build the solaris device tree
 */

#ident "@(#)tree.c   1.42   99/11/03 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "types.h"
#include <dos.h>

#include "boards.h"
#include "boot.h"
#include "bop.h"
#include "bus.h"
#include "debug.h"
#include "devdb.h"
#include <dev_info.h>
#include "enum.h"
#include "err.h"
#include "gettext.h"
#include "isa1275.h"
#include "80421275.h"
#include "menu.h"
#include "pci1275.h"
#include "resmgmt.h"
#include "tree.h"
#include "tty.h"

/*
 * Module local prototypes
 */
void reset_tree(void);
void add_boot_interface(bef_dev *bdp);

void
init_tree()
{
	init_pci1275();
	init_isa1275();
	init_i8042_1275();
}

/*
 *  reset_tree -- prune device tree left over from last time
 */
void
reset_tree(void)
{
	out_bop("resetdtree\n");
	reset_pci1275();
	reset_isa1275();
	reset_i8042_1275();
}

/*
 * For each device create the device node by calling out to bus specific
 * device creation code. Then add any bus independent properties.
 */
void
build_tree(bef_dev *boot_bdp)
{
	Board *bp;
	devtrans *dtp;
	u_char busid;
	char pbuf[256]; /* general output buffer */
	devprop *dpp;


	debug(D_FLOW, "Building device tree\n");

	reset_tree();

	for (bp = Head_board; bp != NULL; bp = bp->link) {
		if (bp->flags & BRDF_NOTREE) {
			continue;
		}
		if (weak_binding_tree(bp)) {
			continue;
		}

		busid = bp->bustype;

		/*
		 * Call out to the bus specific code to create
		 * the device nodes.
		 */
		if ((*bus_ops[ffbs(busid)].build_node)(bp)) {
			continue;
		}

		/*
		 * setprop compatible ISY0200
		 */
		if (((bp->devid != 0) && (busid != RES_BUS_PNPISA)) &&
		    (busid != RES_BUS_PCI)) {
			(void) sprintf(pbuf, "setprop compatible %s\n",
			    GetDeviceId_devdb(bp, 0));
			out_bop(pbuf);
		}

		/*
		 * Add any extra properties from the device database
		 */
		dtp = bp->dbentryp;
		if (dtp != DB_NODB) {
			for (dpp = dtp->proplist; dpp != NULL;
			    dpp = dpp->next) {
				if (*dpp->name != '$') {
					(void) sprintf(pbuf, "setprop %s %s\n",
					    dpp->name, prop_value(dpp));
					out_bop(pbuf);
				}
			}
		}

		if (bp->flags & BRDF_NOLBA) {
			debug(D_LBA, "Marking node %s as no-bef-lba-access\n",
			    GetDeviceName_devdb(bp, TRUE));
			(void) sprintf(pbuf, "setprop no-bef-lba-access\n");
			out_bop(pbuf);
		}

		/*
		 * setbinprop boot-interface 0xfb,0x11
		 */
		if (boot_bdp->bp == bp) {
			add_boot_interface(boot_bdp);
		}

		/*
		 * Add any miscellaneous properties from the board
		 * property list that the bef's might have added.
		 */
		for (dpp = bp->prop; dpp != NULL; dpp = dpp->next) {
			if (dpp->bin) {
				unsigned long far *bp =
					(unsigned long far*)prop_value(dpp);
				int i;

				(void) sprintf(pbuf,
					"setbinprop %s 0x%lx", dpp->name,
					bp[0]);
				out_bop(pbuf);
				for (i = 1; i < (dpp->len >> 2); i++) {
					(void) sprintf(pbuf, ",0x%lx", bp[i]);
					out_bop(pbuf);
				}
				out_bop("\n");
			} else {
				(void) sprintf(pbuf, "setprop %s %s\n",
					dpp->name, prop_value(dpp));
				out_bop(pbuf);
			}

		}

		/*
		 * Add a readability comment
		 */
		out_bop("#\n");
	}
}

/*
 * add boot device interface properties
 * 	<interface type>, <type specific data>
 *
 * scsi: 0x13, <target, lun, bios_drive>
 * net : 0xfb, <bios_drive>
 */
void
add_boot_interface(bef_dev *bdp)
{
	char pbuf[80]; /* general output buffer */

	switch (bdp->info->dev_type) {
	case MDB_NET_CARD:
		(void) sprintf(pbuf, "setbinprop boot-interface 0xfb,0x%x\n",
		    bdp->info->bios_dev);
		out_bop(pbuf);
		break;

	case MDB_SCSI_HBA:
		(void) sprintf(pbuf,
		    "setbinprop boot-interface 0x13,0x%x,0x%x,0x%x\n",
		    bdp->info->MDBdev.scsi.targ,
		    bdp->info->MDBdev.scsi.lun,
		    bdp->info->bios_dev);
		out_bop(pbuf);
		break;
	}
}

/*
 * Return bit position of least significant bit set in argument,
 * starting numbering from 1.
 *
 * From uts/common/os/subr.c:
 *	(Which was a crock, so I re-wrote it -- Reg)
 */
int
ffbs(long x)
{
	int j, n = (x != 0);

	static unsigned long mask[5] = {
	    0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF
	};

	for (j = 5; x && j--; ) {

		unsigned long m = mask[j];
		if (x & m) x &= m;
		else n += (1 << j);
	}

	return (n);
}

int
weak_binding_tree(Board *tbp)
{
	Board *prev, *bp;
	int result;

	/*
	 * First temporarily unlink the target bp from the Head_board
	 */
	for (prev = 0, bp = Head_board; bp; prev = bp, bp = bp->link) {
		if (bp == tbp) {
			if (prev) {
				prev->link = bp->link;
			} else {
				Head_board = bp->link;
			}
			break;
		}
	}
	ASSERT(bp != 0);

	/*
	 * Now for each resource of the target board check for conflict
	 */
	if (board_conflict_resmgmt(tbp, 1, 0)) {
		result = 1;
	} else {
		result = 0;
	}

	/*
	 * Finally restore the unlinked target board
	 */
	if (prev) {
		prev->link = bp;
	} else {
		Head_board = bp;
	}
	return (result);
}
