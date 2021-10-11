/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * isa1275.c -- routines to build the solaris device tree for isa and
 * eisa device nodes.
 *
 * Note, this code does NOT generate 1275 nodes yet. When the kernel can
 * accept and understand full 1275 nodes then this code should be changed.
 * In addition the 1275 spec (latest available is Draft 0.04) has not been
 * finalised.
 *
 *	/devices/eisa/aha@0,330/...
 *		 ^    ^
 *		 bus  child
 *		 node node
 *
 * Changes needed:-
 * 1) The 1275 bus node is called "at" with a property "device_type" as either
 *    "isa" or "eisa", instead of a bus node of "isa" or "eisa" which is what
 *    we create. Note, this must require kernel changes to combine the 2
 *    bus nexi.
 *
 * 2) On the reg properties, we define the tuple first cell (phys.hi)
 *    to be:-
 *	1 - for io space
 *    Whereas 1275 defines it as:-
 *	1 - for non aliased io space
 *	3 - for aliases io space
 *
 * 3) The 1275 child node name is of the form ata@it1f0, where i means
 *    io space and the optional t means io alias. A devices with only
 *    memory space would be of the form ...@dc000. Our current naming
 *    is <name>@<1st memory addr>,<1st io addr> eg aha@dc000,330.
 *
 * 4) We name the 1275 bus child property "dma" as "dma-channels".
 *
 * 5) Support the additional (at least mandatory) child node properties:-
 *	bus-interface - either "isa", "eisa" or "pnp" : easy to provide.
 *	interrupt-type - i don't think this is available
 *	dma-type - ditto
 *	dma-count - ditto
 *	dma-xfer - ditto
 *	bus-mastering - ditto
 *	slot-name - Definitely not available
 *
 * 6) We will never set the "error" (on probe) bus child property,
 *    which must be non-mandatory :-)
 *
 * 7) 1275 Micro-channel hasn't yet been (or will never be?) defined.
 *    I assume it will be close to the isa 1275 bindings, so is currently
 *    defined to be the same.
 * 8) We add additional nodes that 1275 doesn't define. These could present
 *    problems if 1275 later defines properties with the same names.
 *    Our properties are:-
 *	compatible - eisa style name eg ISY0040
 *	boot-interface - bios device # to boot device map
 *	slot - eisa bus slot number
 */

#ident	"@(#)isa1275.c	1.38	99/10/07 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <names.h>
#include <memory.h>
#include <befext.h>
#include <dos.h>
#include "types.h"

#include "menu.h"
#include "boot.h"
#include "bop.h"
#include "bus.h"
#include "debug.h"
#include "devdb.h"
#include "enum.h"
#include "escd.h"
#include "gettext.h"
#include "isa1275.h"
#include "pci1275.h"
#include "pnp1275.h"
#include "probe.h"
#include "tree.h"

/*
 * Module data
 */
static int bus_type;
char bus_node_created[RES_BUS_NO];

/*
 * Module local prototypes
 */
int build_child_node(Board *bp);

void
reset_isa1275()
{
	memset(bus_node_created, 0, sizeof (bus_node_created));
}

void
init_isa1275()
{
	reset_isa1275();
	bus_type = ffbs(Main_bus);
}

/*
 * Create the device node using the boot interpreter commands.
 * For example:-
 *
 *   mknod /isa/aha 1,0x330,0x4,0,0xdc000,0x4000
 *   setbinprop interrupts 11
 *   setbinprop dma-channels 5
 */
int
build_node_isa1275(Board *bp)
{
	debug(D_FLOW, "Building device tree isa/eisa node\n");

	ASSERT(bp->bustype & (RES_BUS_ISA | RES_BUS_EISA));

	build_bus_node_isa1275(bus_type);
	return (build_child_node(bp));
}

void
build_bus_node_isa1275(int bust)
{
	char pbuf[80]; /* general output buffer */

	/*
	 * Make bus-node if not already done
	 */
	if (!bus_node_created[bust - 1]) {
		(void) sprintf(pbuf, "mknod /%s\n", busen[bust]);
		out_bop(pbuf);
		(void) sprintf(pbuf, "setprop device_type %s\n#\n",
		    busen[bust]);
		out_bop(pbuf);
		bus_node_created[bust - 1] = 1;
	}
}

int
build_child_node(Board *bp)
{
	Resource *rp;
	devtrans *dtp = bp->dbentryp;
	int j;
	char pbuf[80]; /* general output buffer */
	u_char first = 0;
	u_long first_io = 0;
	u_long first_mem = 0;
	int multi = 0;

	/*
	 * mknod /isa/fdc 1,0x3f2,4,1,0x3f7,1
	 */
	if (dtp != DB_NODB) {
		/*
		 * Check if we want to put this device into
		 * the tree (eg cpu and memory boards)
		 */
		if (*dtp->unix_driver == 0) { /* ie "none" */
			return (1);
		}

		(void) sprintf(pbuf, "mknod /%s/%s", busen[bus_type],
		    dtp->unix_driver);
		out_bop(pbuf);
	} else {
		/*
		 * If device has no resources, don't report it.
		 */
		if (resource_count(bp) == 0) {
			return (1);
		}

		(void) sprintf(pbuf, "mknod /%s/%s", busen[bus_type],
		    GetDeviceId_devdb(bp, 0));
		out_bop(pbuf);
	}

	rp = resource_list(bp);
	for (j = resource_count(bp); j--; rp++) {
		if (RTYPE(rp) == RESF_Port) {
			(void) sprintf(pbuf, "%s1,0x%lx,0x%lx",
			    ((first) ? "," : " "),
			    rp->base, rp->length);
			first = 1;
			out_bop(pbuf);
			if (!first_io) {
				first_io = rp->base;
			}
		}
	}
	rp = resource_list(bp);
	for (j = resource_count(bp); j--; rp++) {
		if (RTYPE(rp) == RESF_Mem) {
			(void) sprintf(pbuf, "%s0,0x%lx,0x%lx",
			    ((first) ? "," : " "),
			    rp->base, rp->length);
			first = 1;
			out_bop(pbuf);
			if (!first_mem) {
				first_mem = rp->base;
			}
		}
	}
	out_bop("\n");

	/*
	 * Create address property
	 * ie 1,<1st io address> or 0, <1st mem address> (if no io space used)
	 */

	if (first_io) {
		(void) sprintf(pbuf, "setprop \\$at 1,%lx\n", first_io);
	} else {
		(void) sprintf(pbuf, "setprop \\$at 0,%lx\n", first_mem);
	}
	out_bop(pbuf);

	/*
	 * setbinprop interrupts 6
	 */
	rp = resource_list(bp);
	for (j = resource_count(bp); j--; rp++) {
		if (RTYPE(rp) == RESF_Irq) {
			(void) sprintf(pbuf, "%s%ld", (multi++ ? ","
			    : "setbinprop interrupts "),
			    rp->base);
			out_bop(pbuf);
		}
	}
	if (multi) out_bop("\n");
	multi = 0;
	rp = resource_list(bp);
	for (j = resource_count(bp); j--; rp++) {
		if (RTYPE(rp) == RESF_Dma) {
			(void) sprintf(pbuf, "%s%ld", (multi++ ? ","
			    : "setbinprop dma-channels "),
			    rp->base);
			out_bop(pbuf);
		}
	}
	if (multi) out_bop("\n");

	/*
	 * setbinprop slot 3
	 * - for devices where the slot is known
	 */
	if (bp->bustype & RES_BUS_EISA) {
		(void) sprintf(pbuf, "setbinprop slot %d\n", bp->slot);
		out_bop(pbuf);
	}

	/*
	 * Set the "eisa" property for true eisa devices
	 */
	if (bp->bustype & RES_BUS_EISA) {
		(void) sprintf(pbuf, "setprop eisa\n");
		out_bop(pbuf);
	}

	/*
	 * Set the descriptive name (from the master file)
	 * in the model property
	 */
	if (dtp) {
		(void) sprintf(pbuf, "setprop model \"%s\"\n", dtp->dname);
		out_bop(pbuf);
	}

	return (0);
}

static struct bdev_info isa_bootpath_bdev;
static char isa_bootpath_slice;

int
parse_bootpath_isa1275(char *path, char **rest)
{
	char *s = path;
	int i;

	/*
	 * Get bus
	 * =======
	 */
	if (*s++ != '/') {
		return (1);
	}

	for (i = 1; i < nbusen; i++) {
		if (strncmp(s, busen[i], strlen(busen[i])) == 0) {
			break;
		}
	}
	if (i == nbusen) {
		return (1);
	}

	s += strlen(busen[i]); /* skip bus */

	if (*s++ != '/') {
		return (1);
	}

	/*
	 * Check if device is ISA pnp
	 */
	if (strncmp(s, "pnp", 3) == 0) {
		return (parse_bootpath_pnp1275(s, rest));
	}

	/*
	 * Skip name
	 * =========
	 */
	s = strchr(s, '@');
	if (*s++ == 0) {
		return (1);
	}

	/*
	 * Get io_port or memory
	 * =====================
	 */
	if (*s++ != '1') {
		return (1);
	}
	if (*s++ != ',') {
		return (1);
	}
	isa_bootpath_bdev.base_port = (u_short) strtol(s, &s, 16);

	parse_target(s, &isa_bootpath_bdev.MDBdev.scsi.targ,
	    &isa_bootpath_bdev.MDBdev.scsi.lun, &isa_bootpath_slice);

	*rest = s;
	return (0); /* success */
}

/*
 * Check if this is the default (or bootpath) device.
 * Further check the target/lun if we are dealing with a disk/cdrom
 */
int
is_bdp_bootpath_isa1275(bef_dev *bdp)
{
	char path[120];

	/*
	 * For net befs we must use the device record
	 */
	if (strcmp("NET ", bdp->dev_type) == 0) {
		return (is_bp_bootpath_isa1275(bdp->bp, NULL));
	}

	get_path_from_bdp_isa1275(bdp, path, 0);
	return (strncmp(path, bootpath_in, strlen(path)) == 0);
}

int
is_bp_bootpath_isa1275(Board *bp, struct bdev_info *dip)
{
	Resource *rp;

	if (((rp = primary_probe(bp)) != 0) && (RTYPE(rp) == RESF_Port) &&
	    (rp->base == (u_long)isa_bootpath_bdev.base_port)) {
		/* yes, it's the path; save info in dip if present */
		if (dip != NULL) {
			dip->base_port = rp->base;
		}
		return (TRUE);
	}
	return (FALSE);
}

/*
 * Constructs the solaris path from the bef device
 */
void
get_path_from_bdp_isa1275(bef_dev *bdp, char *path, int compat)
{
	u_char bootdev_type = bdp->info->dev_type;
	Board *bp = bdp->bp;
	Resource *rp = resource_list(bp);
	devtrans *dtp = bp->dbentryp;
	char *driver = dtp->unix_driver;
	char *bus;
	int j;
	u_long io = 0;
	u_long mem = 0;
	u_char space;
	u_long spval;
	char *ubp = bdp->info->user_bootpath;
	u_char version = bdp->info->version;

	/*
	 * Check for absolute user boot path
	 */
	if ((version >= 1) && (ubp[0] == '/')) {
		strcpy(path, ubp);
		return;
	}

	for (j = resource_count(bp); j--; rp++) {
		/*
		 *  Find first port and memory resources.  These
		 *  are used to mark the device instance.
		 */

		switch (RTYPE(rp)) {
		case RESF_Port: if (!io) io = rp->base; break;
		case RESF_Mem:  if (!mem) mem = rp->base; break;
		}
	}
	if (io) {
		space = 1,
		spval = io;
	} else {
		space = 0;
		spval = mem;
	}

	bus = busen[ffbs(Main_bus)];
	(void) sprintf(path, "/%s/%s@", bus, driver);
	if (compat) {
		/*
		 * For all network cards except the Xircom Parallel Ethernet
		 * set the unit address to 0.
		 */
		if (bootdev_type == MDB_NET_CARD) {
			if (strcmp(driver, "pe")) { /* not pe */
				io = 0;
			}
		} else {
			io = bdp->info->base_port;
		}
		(void) sprintf(path + strlen(path), "%lx,0", io);
	} else {
		(void) sprintf(path + strlen(path), "%d,%lx", space, spval);
	}
	/*
	 * Add the relative portion of the pathname, if any.
	 * This is primarily for SCSI HBA's which support multiple
	 * channels (e.g., the MLX adapter). This "middle" portion
	 * of the pathname may designate the channel on the adapter.
	 *
	 * Specifically test for esa, as this bef has been converted to
	 * use mscsi in 2.6. But prior to 2.6 it wasn't used and so the
	 * compatability path must not include mscsi.
	 */
	if ((version >= 1) && ubp[0] &&
	    (!compat || strcmp(driver, "esa"))) {
		(void) sprintf(path + strlen(path), "/%s", ubp);
	}
	if (bdp->info->dev_type == MDB_SCSI_HBA) {
		(void) sprintf(path + strlen(path), "/%s@%x,%x",
		    determine_scsi_target_driver(bdp),
		    bdp->info->MDBdev.scsi.targ,
		    bdp->info->MDBdev.scsi.lun);
		if (is_bp_bootpath_isa1275(bp, NULL)) {
			sprintf(path + strlen(path), ":%c", isa_bootpath_slice);
		} else {
			strcat(path, ":a");
		}
	}
}

/*
 * Return a description for the board specified.
 * First check for a PnP bios device, then check in the master file
 * database. Finally just return the ascii form of the eisa id.
 */
char *
bp_to_desc_isa1275(Board *bp, int verbose)
{
	static char *buf = NULL;
	static int blen = 0;
	int len;
	char *bus;
	char nameid[8];
	devtrans *dtp;
	char *bdesc;
	const char *notree = NULL;

	if (bp->bustype == RES_BUS_EISA) {
		bus = "EISA";
	} else if (bp->flags & BRDF_ACPI) {
		bus = "ISA: ACPI";
	} else if (bp->flags & BRDF_PNPBIOS) {
		bus = "ISA: PnP bios";
	} else {
		bus = "ISA";
	}
	if (bp->dbentryp->category == DCAT_PLAT)
		bus = "PLAT";
	len = strlen(bus);

	if (bp->flags & BRDF_PNPBIOS) {
		bdesc = get_desc_pci(bp->pnpbios_base_type,
			bp->pnpbios_sub_type, bp->pnpbios_interface_type);
		len += strlen(bdesc) + 8;
		if (bp->flags & BRDF_NOTREE) {
			notree = gettext("extra resources shown in PnP BIOS");
			len += strlen("\n     - ") + strlen(notree) + 8;
		}
	} else if ((dtp = bp->dbentryp) != 0) {
		len += strlen(dtp->dname) + 8;
	} else {
		DecompressName(bp->devid, nameid);
		len += strlen(nameid) + 8;
	}

	/* error check buf overflow */
	if (len > blen) {
		blen = len;
		buf = realloc(buf, blen);
		if (!buf)
			MemFailure();
	}

	if (bp->flags & BRDF_PNPBIOS) {
		(void) sprintf(buf, "%s: %s", bus, bdesc);
		if (bp->flags & BRDF_NOTREE) {
			(void) sprintf(buf + strlen(buf), "\n     - %s",
			    notree);
		}
	} else if ((dtp = bp->dbentryp) != 0) {
		(void) sprintf(buf, "%s: %s", bus, dtp->dname);
	} else {
		(void) sprintf(buf, "%s: %s", bus, nameid);
	}
	return (buf);
}
