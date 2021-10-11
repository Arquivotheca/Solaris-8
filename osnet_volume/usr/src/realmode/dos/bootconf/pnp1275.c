/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pnp1275.c -- routines to build the solaris device tree for pnp isa
 * device nodes.
 */

#ident "@(#)pnp1275.c   1.18   99/10/07 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <names.h>
#include <befext.h>
#include "types.h"
#include <dos.h>

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
#include "pnp1275.h"
#include "probe.h"
#include "tree.h"

/*
 * Module data
 */

/*
 * Names are of the form:-
 *
 *   /[e]isa/pnp<function eisa id>@<board eisa id>,<serial no>,<function>
 *
 *   eg /isa/pnpCTL,2011@CTL,0042,23456789,2
 */
struct path_bits_pnp1275 {
	u_long func_id;
	u_long board_id;
	u_long serial;
	u_char func;
	u_char valid;		/* set if valid */
};

static struct path_bits_pnp1275 boot_bits;

#define	ID_LEN 11
#define	COMMA_OFF 6

/*
 * Module local prototypes
 */
int build_child_node_pnp(Board *bp);
u_long str_to_id_pnp1275(char *name);

int
build_node_pnp1275(Board *bp)
{
	debug(D_FLOW, "Building device tree isa pnp node\n");

	ASSERT(bp->bustype == RES_BUS_PNPISA);

	build_bus_node_isa1275(ffbs(Main_bus));

	return (build_child_node_pnp(bp));
}

/*
 * Create the device node using the boot interpreter commands.
 * For example, the ata controller on the pnp sound blaster:-
 *
 *   mknod /isa/pnpCTL,2011
 *	1<compressed CTL0042 eisa id>, <serial no>, func,
 *	1,0x1e0,0x8,1,0x3e6,0x2
 *   setprop $at CTL,0042,23456789,2
 *   setbinprop interrupts 11
 *   setbinprop pnp-csn 2
 *   setprop model IDE
 *   setprop compatible pnpCTL,0042,2 pnpCTL,2011 ata <compat ids>
 */
int
build_child_node_pnp(Board *bp)
{
	Resource *rp;
	devtrans *dtp = bp->dbentryp;
	int j;
	char pbuf[200]; /* general output buffer */
	int multi = 0;
	char *desc;
	char func[5];
	union {
		u_long l;
		u_char c[4];
	} u1, u2;
	char *cp;
	u_long *lp;
	char buf2[12];
	char c;
	int len;

	/*
	 * Create device name, if we have a master file entry mapping
	 * the device to a unix driver, then use the unix driver name,
	 * else make the gobbldygook pnp name. eg:-
	 * mknod /isa/pnpCTL,2011
	 *    1,0x1e0,0x8,1,0x3e6,0x2
	 */
	(void) sprintf(pbuf, "mknod /%s/%s", busen[ffbs(Main_bus)],
	    id_to_str_pnp1275(bp->pnp_func_id));
	out_bop(pbuf);

	/*
	 * Add unit address eg:-
	 * 1<compressed CTL0042 eisa id>, <serial no>, func (+ 24 bits reserved)
	 * Note high bit indicates its PnP isa.
	 * Need to byte swap the 1st 32 bits.
	 */
	u1.l = bp->pnp_board_id;
	u2.c[0] = u1.c[3];
	u2.c[1] = u1.c[2];
	u2.c[2] = u1.c[1];
	u2.c[3] = u1.c[0];

	(void) sprintf(pbuf, " 0x%lx,0x%lx,0x%lx",
	    0x80000000 | u2.l,
	    bp->pnp_serial,
	    ((u_long) bp->pnp_ldn) << 24);
	out_bop(pbuf);

	/*
	 * Add registers eg:-
	 * 1,0x1e0,0x8,1,0x3e6,0x2
	 */
	rp = resource_list(bp);
	for (j = resource_count(bp); j--; rp++) {
		if (RTYPE(rp) == RESF_Port) {
			(void) sprintf(pbuf, ",1,0x%lx,0x%lx",
			    rp->base, rp->length);
			out_bop(pbuf);
		}
	}
	rp = resource_list(bp);
	for (j = resource_count(bp); j--; rp++) {
		if (RTYPE(rp) == RESF_Mem) {
			(void) sprintf(pbuf, ",0,0x%lx,0x%lx",
			    rp->base, rp->length);
			out_bop(pbuf);
		}
	}
	out_bop("\n");

	/*
	 * Create unit address property
	 * setprop $at CTL,0042,23456789,2
	 */
	if (bp->pnp_ldn) {
		(void) sprintf(func, ",%x", bp->pnp_ldn);
	} else {
		func[0] = 0;
	}

	(void) sprintf(pbuf, "setprop \\$at %s,%lx%s\n",
	    id_to_str_pnp1275(bp->pnp_board_id), /* boards' eisa id */
	    bp->pnp_serial,			/* board's serial number */
	    func);				/* function number */
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
	 * Set card select number property eg:-
	 * setbinprop pnp-csn 3
	 */
	(void) sprintf(pbuf, "setbinprop pnp-csn %d\n", bp->pnp_csn);
	out_bop(pbuf);

	/*
	 * Set pnp function description property eg:-
	 * setprop model IDE
	 */
	if (bp->pnp_desc) {
		desc = bp->pnp_desc;
	} else if (dtp != DB_NODB) {
		desc = dtp->dname; /* descriptive name from devicedb/master */
	} else {
		desc = (char *)gettext("Unknown PnP isa device");
	}
	(void) sprintf(pbuf, "setprop model \"%s\"\n", desc);
	out_bop(pbuf);

	/*
	 * Set compatible property eg:-
	 * setprop compatible pnpCTL,0042,2 pnpCTL,2011 ata <compat ids>
	 *
	 * Compatible names must be seperated by nulls, so the only way we
	 * can output these is to use binary properties.
	 *
	 * We must replace all space with nulls
	 * then convert the chanracters to integers.
	 */
	if (bp->pnp_multi_func) {
		(void) sprintf(pbuf, "%s,%d",
		    id_to_str_pnp1275(bp->pnp_board_id),
		    bp->pnp_ldn);
	} else {
		(void) sprintf(pbuf, "%s", id_to_str_pnp1275(bp->pnp_func_id));
	}
	if ((dtp != DB_NODB) && (*dtp->unix_driver) /* ie not "none" */) {
		strcat(pbuf, " ");
		strcat(pbuf, dtp->unix_driver);
	}
	if (bp->pnp_compat_ids) {
		strcat(pbuf, bp->pnp_compat_ids);
	}

	for (cp = pbuf; *cp; cp++) {
		if (*cp == ' ') {
			*cp = 0;
		}
	}
	len = ((cp - pbuf) + 4) >> 2;
	cp++;
	*cp++ = 0;
	*cp++ = 0;
	*cp++ = 0;
	ASSERT((cp - pbuf) < 200);

	out_bop("setbinprop compatible ");
	for (lp = (u_long *) pbuf; len; lp++, len--) {
		if (len == 1) {
			c = '\n';
		} else {
			c = ',';
		}
		(void) sprintf(buf2, "0x%lx%c", *lp, c);
		out_bop(buf2);
	}

	return (0);
}

/*
 * Called from parse_bootpath_isa1275()
 */
int
parse_bootpath_pnp1275(char *path, char **rest)
{
	char *s = path;

	boot_bits.valid = 0;

	/*
	 * Get function id
	 * ===============
	 */
	if (s[COMMA_OFF] != ',') {
		return (1);
	}
	if (s[ID_LEN] != '@') {
		return (2);
	}
	boot_bits.func_id =  str_to_id_pnp1275(s);
	s += ID_LEN + 1; /* skip past name and '@' */

	/*
	 * Get board id
	 * ============
	 */
	if (strncmp(s, "pnp", 3)) {
		return (3);
	}
	if (s[COMMA_OFF] != ',') {
		return (4);
	}
	if (s[ID_LEN] != ',') {
		return (5);
	}
	boot_bits.board_id =  str_to_id_pnp1275(s);
	s += ID_LEN + 1; /* skip past name and ',' */

	/*
	 * Get serial no
	 * =============
	 */
	boot_bits.serial = strtoul(s, &s, 16);

	/*
	 * Get optional function
	 * =====================
	 */
	boot_bits.func = 0;
	if (*s == ',') {
		s++;
		boot_bits.func = strtol(s, &s, 16);
	}

#ifdef notyet
	/* see comment in is_bp_bootpath_pnp1275 */
	parse_target(s, &pnp_bootbdev.MDBdev.scsi.targ, 
	    &pnp_bootbdev.MDBdev.scsi.lun, &pnp_bootpath_slice);
#endif
	
	*rest = s;

	boot_bits.valid = 1;

	return (0); /* success */
}

/*
 * Check if this is the default (or bootpath) device.
 * Further check the target/lun if we are dealing with a disk/cdrom
 */
int
is_bdp_bootpath_pnp1275(bef_dev *bdp)
{
	char path[120];

	if (!is_bp_bootpath_pnp1275(bdp->bp, NULL)) {
		return (FALSE);
	}

	get_path_from_bdp_pnp1275(bdp, path, 0);
	return (strncmp(path, bootpath_in, strlen(path)) == 0);
}

int
is_bp_bootpath_pnp1275(Board *bp, struct bdev_info *dip)
{
	if (boot_bits.valid &&
	    (bp->pnp_board_id == boot_bits.board_id) &&
	    (bp->pnp_serial == boot_bits.serial) &&
	    (bp->pnp_ldn  == boot_bits.func)) {
		return (TRUE);
#ifdef notyet
		/*
		 * SCSI PnP bootpaths must not be supported yet, because
		 * this code, unlike the other two bus enumerators, has
		 * never set fields in "dev80" as a side-effect of being
		 * called, and so therefore does nothing with dip (now
		 * that dev80 references have been removed)
		 */

		if (dip != NULL) {
			/* XXX */
		}
#endif /* notyet */
	}
	return (FALSE);
}

/*
 * Constructs the solaris path from the bef device
 */
void
get_path_from_bdp_pnp1275(bef_dev *bdp, char *path, int compat)
{
	Board *bp = bdp->bp;
	char *ubp = bdp->info->user_bootpath;
	u_char version = bdp->info->version;

	/*
	 * Check for absolute user boot path
	 */
	if ((version >= 1) && (ubp[0] == '/')) {
		strcpy(path, ubp);
		return;
	}

	if (compat) {
		get_path_from_bdp_isa1275(bdp, path, compat);
		return;
	}
	/*
	 * Seperate the calls to id_to_str_pnp1275() because it returns
	 * a static string
	 */
	(void) sprintf(path, "/%s/%s",
	    busen[ffbs(Main_bus)],
	    id_to_str_pnp1275(bp->pnp_func_id));
	(void) sprintf(path + strlen(path), "@%s,%lx",
	    id_to_str_pnp1275(bp->pnp_board_id),
	    bp->pnp_serial);

	if (bp->pnp_ldn) {
		(void) sprintf(path + strlen(path), ",%x", bp->pnp_ldn);
	}
	/*
	 * Add the relative portion of the pathname, if any.
	 * This is primarily for SCSI HBA's which support multiple
	 * channels (e.g., the MLX adapter). This "middle" portion
	 * of the pathname may designate the channel on the adapter.
	 */
	if ((version >= 1) && ubp[0]) {
		(void) sprintf(path + strlen(path), "/%s", ubp);
	}
	if (bdp->info->dev_type == MDB_SCSI_HBA) {
		(void) sprintf(path + strlen(path), "/%s@%x,%x",
		    determine_scsi_target_driver(bdp),
		    bdp->info->MDBdev.scsi.targ,
		    bdp->info->MDBdev.scsi.lun);
#ifdef notyet
		if (is_bp_bootpath_pnp1275(bp, NULL) {
			sprintf(path + strlen(path), ":%c", pci_bootpath_slice);
		} else {
			strcat(path, ":a");
		}
#else
		strcat(path, ":a");
#endif
	}
}

/*
 * Return a pointer to a pnp name in the form pnpVVV,DDDD eg "pnpADP,1542"
 */
char *
id_to_str_pnp1275(u_long eisaid)
{
	static char n[12] = "pnpVVV,DDDD";

	DecompressName(eisaid, n + 4);
	/*
	 * shift the VVV left 1 to make room for the ","
	 */
	n[3] = n[4]; n[4] = n[5]; n[5] = n[6]; n[6] = ',';

	(void) strlwr(n + 7);

	return (n);
}

/*
 * Return a compressed eisa id from a pnp name in the
 * form pnpVVV,DDDD eg "pnpADP,1542"
 */
u_long
str_to_id_pnp1275(char *n)
{
	char t[8];

	/* copy VVV */
	t[0] = n[3]; t[1] = n[4]; t[2] = n[5];
	/* copy DDDD */
	t[3] = n[7]; t[4] = n[8]; t[5] = n[9]; t[6] = n[10];
	t[7] = 0;

	return (CompressName(t));
}

char *
bp_to_desc_pnp1275(Board *bp, int verbose)
{
	static const char bus[] = "PnP ISA: ";
	static char buf[80];
	char nameid[8];
	devtrans *dtp;

	if ((dtp = bp->dbentryp) != 0) {
		(void) sprintf(buf, "%s%s", bus, dtp->dname);
	} else {
		DecompressName(bp->devid, nameid);
		(void) sprintf(buf, "%s%s", bus, nameid);
	}
	return (buf);
}
