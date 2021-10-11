/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)misc_link_i386.c	1.8	99/08/30 SMI"

#include <regex.h>
#include <devfsadm.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

static int lp(di_minor_t minor, di_node_t node);
static int serial_dialout(di_minor_t minor, di_node_t node);
static int serial(di_minor_t minor, di_node_t node);
static int diskette(di_minor_t minor, di_node_t node);
static int vt00(di_minor_t minor, di_node_t node);
static int kdmouse(di_minor_t minor, di_node_t node);
static int pcihpc_creat_cb(di_minor_t minor, di_node_t node);

static devfsadm_create_t pseudo_cbt[] = {
	{ "vt00", "ddi_display", NULL,
	    TYPE_EXACT, ILEVEL_0,	vt00
	},
	{ "mouse", "ddi_mouse", "mouse8042",
	    TYPE_EXACT | DRV_EXACT, ILEVEL_0, kdmouse
	},
	{ "disk",  "ddi_block:diskette", NULL,
	    TYPE_EXACT, ILEVEL_1, diskette
	},
	{ "parallel",  "ddi_pseudo", "lp",
	    TYPE_EXACT | DRV_EXACT, ILEVEL_1, lp
	},
	{ "serial", "ddi_serial:mb", NULL,
	    TYPE_EXACT, ILEVEL_1, serial
	},
	{ "serial",  "ddi_serial:dialout,mb", NULL,
	    TYPE_EXACT, ILEVEL_1, serial_dialout
	},
	{ "pcihpc", "ddi_ctl:pcihpc", NULL,
	    TYPE_EXACT, ILEVEL_0, pcihpc_creat_cb
	}
};

DEVFSADM_CREATE_INIT_V0(pseudo_cbt);

static devfsadm_remove_t misc_remove_cbt[] = {
	{ "vt", "vt[0-9][0-9]", RM_PRE|RM_ALWAYS,
		ILEVEL_0, devfsadm_rm_all
	},
	{ "pcihpc", "^pcihpc/[0-9]+$", RM_POST,
		ILEVEL_0, devfsadm_rm_all
	}
};

DEVFSADM_REMOVE_INIT_V0(misc_remove_cbt);

/*
 * Handles minor node type "ddi_display", in addition to generic processing
 * done by display().
 *
 * This creates a /dev/vt00 link to /dev/fb, for backwards compatibility.
 */
/* ARGSUSED */
int
vt00(di_minor_t minor, di_node_t node)
{
	(void) devfsadm_secondary_link("vt00", "fb", 0);
	return (DEVFSADM_CONTINUE);
}

/*
 * type=ddi_block:diskette;addr=0,0;minor=c        diskette
 * type=ddi_block:diskette;addr=0,0;minor=c,raw    rdiskette
 * type=ddi_block:diskette;addr1=0;minor=c diskette\A2
 * type=ddi_block:diskette;addr1=0;minor=c,raw     rdiskette\A2
 */
static int
diskette(di_minor_t minor, di_node_t node)
{
	char *a2;
	char link[PATH_MAX];
	char *addr = di_bus_addr(node);
	char *mn = di_minor_name(minor);

	if (strcmp(addr, "0,0") == 0) {
		if (strcmp(mn, "c") == 0) {
			(void) devfsadm_mklink("diskette", node, minor, 0);
		} else if (strcmp(mn, "c,raw") == 0) {
			(void) devfsadm_mklink("rdiskette", node, minor, 0);
		}

	}

	if (addr[0] == '0') {
		if ((a2 = strchr(addr, ',')) != NULL) {
			a2++;
			if (strcmp(mn, "c") == 0) {
				(void) strcpy(link, "diskette");
				(void) strcat(link, a2);
				(void) devfsadm_mklink(link, node, minor, 0);
			} else if (strcmp(mn, "c,raw") == 0) {
				(void) strcpy(link, "rdiskette");
				(void) strcat(link, a2);
				(void) devfsadm_mklink(link, node, minor, 0);
			}
		}
	}

	return (DEVFSADM_CONTINUE);
}

/*
 * type=ddi_pseudo;name=lp;addr=1,3bc      lp0
 * type=ddi_pseudo;name=lp;addr=1,378      lp1
 * type=ddi_pseudo;name=lp;addr=1,278      lp2
 */
static int
lp(di_minor_t minor, di_node_t node)
{
	char *addr = di_bus_addr(node);

	if (strcmp(addr, "1,3bc") == 0) {
		(void) devfsadm_mklink("lp0", node, minor, 0);

	} else if (strcmp(addr, "1,378") == 0) {
		(void) devfsadm_mklink("lp1", node, minor, 0);

	} else if (strcmp(addr, "1,278") == 0) {
		(void) devfsadm_mklink("lp2", node, minor, 0);
	}

	return (DEVFSADM_CONTINUE);
}

/*
 * type=ddi_serial:mb;minor=a      tty00
 * type=ddi_serial:mb;minor=b      tty01
 * type=ddi_serial:mb;minor=c      tty02
 * type=ddi_serial:mb;minor=d      tty03
 * type=ddi_serial:mb;minor=a      term/a
 * type=ddi_serial:mb;minor=b      term/b
 * type=ddi_serial:mb;minor=c      term/c
 * type=ddi_serial:mb;minor=d      term/d
 */
static int
serial(di_minor_t minor, di_node_t node)
{

	char *mn = di_minor_name(minor);
	char link[PATH_MAX];

	(void) strcpy(link, "tty");
	(void) strcat(link, mn);
	(void) devfsadm_mklink(link, node, minor, 0);

	if (strcmp(mn, "a") == 0) {
		(void) devfsadm_mklink("tty00", node, minor, 0);
		(void) devfsadm_mklink("term/a", node, minor, 0);

	} else if (strcmp(mn, "b") == 0) {
		(void) devfsadm_mklink("tty01", node, minor, 0);
		(void) devfsadm_mklink("term/b", node, minor, 0);

	} else if (strcmp(mn, "c") == 0) {
		(void) devfsadm_mklink("tty02", node, minor, 0);
		(void) devfsadm_mklink("term/c", node, minor, 0);

	} else if (strcmp(mn, "d") == 0) {
		(void) devfsadm_mklink("tty03", node, minor, 0);
		(void) devfsadm_mklink("term/d", node, minor, 0);
	}
	return (DEVFSADM_CONTINUE);
}

/*
 * type=ddi_serial:dialout,mb;minor=a,cu   ttyd0
 * type=ddi_serial:dialout,mb;minor=b,cu   ttyd1
 * type=ddi_serial:dialout,mb;minor=c,cu   ttyd2
 * type=ddi_serial:dialout,mb;minor=d,cu   ttyd3
 */
static int
serial_dialout(di_minor_t minor, di_node_t node)
{
	char *mn = di_minor_name(minor);

	if (strcmp(mn, "a,cu") == 0) {
		(void) devfsadm_mklink("ttyd0", node, minor, 0);
		(void) devfsadm_mklink("cua0", node, minor, 0);

	} else if (strcmp(mn, "b,cu") == 0) {
		(void) devfsadm_mklink("ttyd1", node, minor, 0);
		(void) devfsadm_mklink("cua1", node, minor, 0);

	} else if (strcmp(mn, "c,cu") == 0) {
		(void) devfsadm_mklink("ttyd2", node, minor, 0);
		(void) devfsadm_mklink("cua2", node, minor, 0);

	} else if (strcmp(mn, "d,cu") == 0) {
		(void) devfsadm_mklink("ttyd3", node, minor, 0);
		(void) devfsadm_mklink("cua3", node, minor, 0);
	}
	return (DEVFSADM_CONTINUE);
}

static int
kdmouse(di_minor_t minor, di_node_t node)
{
	(void) devfsadm_mklink("kdmouse", node, minor, 0);
	return (DEVFSADM_CONTINUE);
}

#define	PCIHPC_DIRNAME	"pcihpc"

static int
pcihpc_creat_cb(di_minor_t minor, di_node_t node)
{
	char path[PATH_MAX + 1];
	char *buf;
	devfsadm_enumerate_t rules[1] = {{"^pcihpc$/^([0-9]+)$", 1,
	    MATCH_ADDR}};

	if ((buf = di_devfs_path(node)) == NULL) {
		return (DEVFSADM_CONTINUE);
	}

	(void) strcpy(path, buf);
	(void) strcat(path, ":");
	(void) strcat(path, di_minor_name(minor));

	di_devfs_path_free(buf);

	if (devfsadm_enumerate_int(path, 0, &buf, rules, 1)
	    == DEVFSADM_FALSE) {
		return (DEVFSADM_CONTINUE);
	}

	(void) sprintf(path, "%s/%s", PCIHPC_DIRNAME, buf);

	free(buf);

	(void) devfsadm_mklink(path, node, minor, 0);

	return (DEVFSADM_CONTINUE);
}
