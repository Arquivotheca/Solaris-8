/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)usb_link.c	1.7	99/10/07 SMI"

#include <devfsadm.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

static int usb_process(di_minor_t minor, di_node_t node);

/* Rules for creating links */
static devfsadm_create_t usb_cbt[] = {
	{ "usb", NULL, "ohci",		DRV_EXACT, ILEVEL_0, usb_process },
	{ "usb", NULL, "uhci",		DRV_EXACT, ILEVEL_0, usb_process },
	{ "usb", NULL, "hid",		DRV_EXACT, ILEVEL_0, usb_process },
	{ "usb", NULL, "hubd",		DRV_EXACT, ILEVEL_0, usb_process },
	{ "usb", NULL, "usb_mid",	DRV_EXACT, ILEVEL_0, usb_process },
	{ "usb", DDI_NT_SCSI_NEXUS, "scsa2usb",	DRV_EXACT|TYPE_EXACT,
		ILEVEL_0, usb_process },
};

/* For debug printing (-V filter) */
static char *debug_mid = "usb_mid";

DEVFSADM_CREATE_INIT_V0(usb_cbt);

/* USB device links */
#define	USB_LINK_RE_HUB		"^usb/hub[0-9]+$"
#define	USB_LINK_RE_HID		"^usb/hid[0-9]+$"
#define	USB_LINK_RE_DEVICE	"^usb/device[0-9]+$"
#define	USB_LINK_RE_MASS_STORE	"^usb/mass-storage[0-9]+$"

/* Rules for removing links */
static devfsadm_remove_t usb_remove_cbt[] = {
	{ "usb", USB_LINK_RE_DEVICE, RM_POST | RM_HOT, ILEVEL_0,
			devfsadm_rm_all },
	{ "usb", USB_LINK_RE_HUB, RM_POST | RM_HOT, ILEVEL_0, devfsadm_rm_all },
	{ "usb", USB_LINK_RE_MASS_STORE, RM_POST | RM_HOT | RM_ALWAYS,
			ILEVEL_0, devfsadm_rm_all },
	{ "usb", USB_LINK_RE_HID, RM_POST | RM_HOT | RM_ALWAYS, ILEVEL_0,
			devfsadm_rm_all },
};

/* Rules for different USB devices */
static devfsadm_enumerate_t hub_rules[1] =
	{"^usb$/^hub([0-9]+)$", 1, MATCH_ALL};
static devfsadm_enumerate_t hid_rules[1] =
	{"^usb$/^hid([0-9]+)$", 1, MATCH_ALL};
static devfsadm_enumerate_t device_rules[1] =
	{"^usb$/^device([0-9]+)$", 1, MATCH_ALL};
static devfsadm_enumerate_t mass_storage_rules[1] =
	{"^usb$/^mass-storage([0-9]+)$", 1, MATCH_ALL};

DEVFSADM_REMOVE_INIT_V0(usb_remove_cbt);

int
minor_init(void)
{
	devfsadm_print(debug_mid, "usb_link: minor_init\n");
	return (DEVFSADM_SUCCESS);
}

int
minor_fini(void)
{
	devfsadm_print(debug_mid, "usb_link: minor_fini\n");
	return (DEVFSADM_SUCCESS);
}

/*
 * This function is called for every usb minor node.
 * Calls enumerate to assign a logical usb id, and then
 * devfsadm_mklink to make the link.
 */
static int
usb_process(di_minor_t minor, di_node_t node)
{
	devfsadm_enumerate_t rules[1];
	char l_path[PATH_MAX], p_path[PATH_MAX], *buf, *devfspath;
	char *minor_nm, *drvr_nm, *name = (char *)NULL;

	minor_nm = di_minor_name(minor);
	drvr_nm = di_driver_name(node);
	if ((minor_nm == NULL) || (drvr_nm == NULL)) {
		return (DEVFSADM_CONTINUE);
	}

	devfsadm_print(debug_mid, "usb_process: minor=%s node=%s\n",
		minor_nm, di_node_name(node));

	devfspath = di_devfs_path(node);

	(void) strcpy(p_path, devfspath);
	(void) strcat(p_path, ":");
	(void) strcat(p_path, minor_nm);
	di_devfs_path_free(devfspath);

	devfsadm_print(debug_mid, "usb_process: path %s\n", p_path);

	/* Figure out which rules to apply */
	if ((strcmp(drvr_nm, "hubd") == 0) ||
	    (strcmp(drvr_nm, "ohci") == 0) ||
	    (strcmp(drvr_nm, "uhci") == 0)) {
		rules[0] = hub_rules[0];	/* For HUBs */
		name = "hub";
	} else if (strcmp(drvr_nm, "hid") == 0) {
		rules[0] = hid_rules[0];
		name = "hid";			/* For HIDs */
	} else if (strcmp(drvr_nm, "usb_mid") == 0) {
		rules[0] = device_rules[0];
		name = "device";		/* For other USB devices */
	} else if (strcmp(drvr_nm, "scsa2usb") == 0) {
		rules[0] = mass_storage_rules[0];
		name = "mass-storage";		/* For mass-storage devices */
	} else {
		devfsadm_print(debug_mid, "usb_process: unknown driver=%s\n",
		    drvr_nm);
		return (DEVFSADM_CONTINUE);
	}

	/*
	 *  build the physical path from the components.
	 *  find the logical usb id, and stuff it in buf
	 */
	if (devfsadm_enumerate_int(p_path, 0, &buf, rules, 1)) {
		devfsadm_print(debug_mid, "usb_process: exit/continue\n");
		return (DEVFSADM_CONTINUE);
	}

	(void) sprintf(l_path, "usb/%s%s", name, buf);

	devfsadm_print(debug_mid, "usb_process: p_path=%s buf=%s\n",
	    p_path, buf);

	free(buf);

	devfsadm_print(debug_mid, "mklink %s -> %s\n", l_path, p_path);

	(void) devfsadm_mklink(l_path, node, minor, 0);

	return (DEVFSADM_CONTINUE);
}
