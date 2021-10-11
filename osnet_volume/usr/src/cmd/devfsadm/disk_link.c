/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)disk_link.c	1.7	99/09/23 SMI"

#include <devfsadm.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>

#define	DISK_SUBPATH_MAX 30
#define	RM_STALE 0x01
#define	DISK_LINK_RE	"^r?dsk/c[0-9]+(t[0-9]+)?d[0-9]+((s|p))[0-9]+$"
#define	DISK_LINK_TO_UPPER(ch)\
	(((ch) >= 'a' && (ch) <= 'z') ? (ch - 'a' + 'A') : ch)

static int disk_callback_chan(di_minor_t minor, di_node_t node);
static int disk_callback_nchan(di_minor_t minor, di_node_t node);
static int disk_callback_wwn(di_minor_t minor, di_node_t node);
static int disk_callback_fabric(di_minor_t minor, di_node_t node);
static void disk_common(di_minor_t minor, di_node_t node, char *disk,
				int flags);
static char *diskctrl(di_node_t node, di_minor_t minor);
extern void rm_link_from_cache(char *devlink, char *physpath);


static devfsadm_create_t disk_cbt[] = {
	{ "disk", "ddi_block", NULL,
	    TYPE_EXACT, ILEVEL_0, disk_callback_nchan
	},
	{ "disk", "ddi_block:channel", NULL,
	    TYPE_EXACT, ILEVEL_0, disk_callback_chan
	},
	{ "disk", "ddi_block:fabric", NULL,
		TYPE_EXACT, ILEVEL_0, disk_callback_fabric
	},
	{ "disk", "ddi_block:wwn", NULL,
	    TYPE_EXACT, ILEVEL_0, disk_callback_wwn
	},
	{ "disk", "ddi_block:cdrom", NULL,
	    TYPE_EXACT, ILEVEL_0, disk_callback_nchan
	},
	{ "disk", "ddi_block:cdrom:channel", NULL,
	    TYPE_EXACT, ILEVEL_0, disk_callback_chan
	},
};

DEVFSADM_CREATE_INIT_V0(disk_cbt);

/*
 * HOT auto cleanup of disks not desired.
 */
static devfsadm_remove_t disk_remove_cbt[] = {
	{ "disk", DISK_LINK_RE, RM_POST,
		ILEVEL_0, devfsadm_rm_all
	}
};

DEVFSADM_REMOVE_INIT_V0(disk_remove_cbt);

static int
disk_callback_chan(di_minor_t minor, di_node_t node)
{
	char *addr;
	char disk[20];
	uint_t targ;
	uint_t lun;

	addr = di_bus_addr(node);
	(void) sscanf(addr, "%X,%X", &targ, &lun);
	(void) sprintf(disk, "t%dd%d", targ, lun);
	disk_common(minor, node, disk, 0);
	return (DEVFSADM_CONTINUE);

}

static int
disk_callback_nchan(di_minor_t minor, di_node_t node)
{
	char *addr;
	char disk[10];
	uint_t lun;

	addr = di_bus_addr(node);
	(void) sscanf(addr, "%X", &lun);
	(void) sprintf(disk, "d%d", lun);
	disk_common(minor, node, disk, 0);
	return (DEVFSADM_CONTINUE);

}

static int
disk_callback_wwn(di_minor_t minor, di_node_t node)
{
	char disk[10];
	int lun;
	int targ;
	int *intp;

	if (di_prop_lookup_ints(DDI_DEV_T_ANY, node,
		"target", &intp) <= 0) {
		return (DEVFSADM_CONTINUE);
	}
	targ = *intp;
	if (di_prop_lookup_ints(DDI_DEV_T_ANY, node,
		    "lun", &intp) <= 0) {
		    lun = 0;
	} else {
		    lun = *intp;
	}
	(void) sprintf(disk, "t%dd%d", targ, lun);

	disk_common(minor, node, disk, RM_STALE);

	return (DEVFSADM_CONTINUE);
}

static int
disk_callback_fabric(di_minor_t minor, di_node_t node)
{
	char disk[25];
	int lun;
	int count;
	int *intp;
	uchar_t *str;
	uchar_t *wwn;
	uchar_t ascii_wwn[17];

	if (di_prop_lookup_bytes(DDI_DEV_T_ANY, node, "port-wwn", &wwn) <= 0) {
		return (DEVFSADM_CONTINUE);
	}

	if (di_prop_lookup_ints(DDI_DEV_T_ANY, node, "lun", &intp) <= 0) {
		lun = 0;
	} else {
		lun = *intp;
	}

	for (count = 0, str = ascii_wwn; count < 8; count++, str += 2) {
		(void) sprintf((caddr_t)str, "%02x", wwn[count]);
	}
	*str = '\0';

	for (str = ascii_wwn; *str != '\0'; str++) {
		*str = DISK_LINK_TO_UPPER(*str);
	}

	(void) sprintf(disk, "t%sd%d", ascii_wwn, lun);

	disk_common(minor, node, disk, RM_STALE);

	return (DEVFSADM_CONTINUE);
}

/*
 * This function is called for every disk minor node.
 * Calls enumerate to assign a logical controller number, and
 * then devfsadm_mklink to make the link.
 */
static void
disk_common(di_minor_t minor, di_node_t node, char *disk, int flags)
{
	char l_path[PATH_MAX + 1];
	char stale_re[DISK_SUBPATH_MAX];
	char *dir;
	char slice[4];
	char *mn;
	char *ctrl;

	if (strstr(mn = di_minor_name(minor), ",raw")) {
		dir = "rdsk";
	} else {
		dir = "dsk";
	}

	if (mn[0] < 113) {
		(void) sprintf(slice, "s%d", mn[0] - 'a');
	} else {
		(void) sprintf(slice, "p%d", mn[0] - 'q');
	}

	if (NULL == (ctrl = diskctrl(node, minor)))
		return;

	(void) strcpy(l_path, dir);
	(void) strcat(l_path, "/c");
	(void) strcat(l_path, ctrl);
	(void) strcat(l_path, disk);
	(void) strcat(l_path, slice);

	(void) devfsadm_mklink(l_path, node, minor, 0);

	if ((flags & RM_STALE) == RM_STALE) {
		(void) strcpy(stale_re, "^");
		(void) strcat(stale_re, dir);
		(void) strcat(stale_re, "/c");
		(void) strcat(stale_re, ctrl);
		(void) strcat(stale_re, "t[0-9]+d[0-9]+s[0-9]$");
		/*
		 * optimizations are made inside of devfsadm_rm_stale_links
		 * instead of before calling the function, as it always
		 * needs to add the valid link to the cache.
		 */
		devfsadm_rm_stale_links(stale_re, l_path, node, minor);
	}

	free(ctrl);
}


/* index of enumeration rule applicable to this module */
#define	RULE_INDEX	0

static char *
diskctrl(di_node_t node, di_minor_t minor)
{
	char path[PATH_MAX + 1];
	char *devfspath;
	char *buf, *mn;

	devfsadm_enumerate_t rules[3] = {
	    {"^r?dsk$/^c([0-9]+)", 1, MATCH_PARENT},
	    {"^cfg$/^c([0-9]+)$", 1, MATCH_ADDR},
	    {"^scsi$/^.+$/^c([0-9]+)", 1, MATCH_PARENT}
	};

	mn = di_minor_name(minor);

	if ((devfspath = di_devfs_path(node)) == NULL) {
		return (NULL);
	}
	(void) strcpy(path, devfspath);
	(void) strcat(path, ":");
	(void) strcat(path, mn);
	di_devfs_path_free(devfspath);

	/*
	 * Use controller component of disk path
	 */
	if (devfsadm_enumerate_int(path, RULE_INDEX, &buf, rules, 3)) {
		/*
		 * Maybe failed because there are multiple logical controller
		 * numbers for a single physical controller.  If we use node
		 * name also in the match it should fix this and only find one
		 * logical controller. (See 4045879).
		 * NOTE: Rules for controllers are not changed, as there is
		 * no unique controller number for them in this case.
		 *
		 * MATCH_UNCACHED flag is private to the "disks" and "sgen"
		 * modules. NOT to be used by other modules.
		 */
		rules[0].flags = MATCH_NODE | MATCH_UNCACHED; /* disks */
		rules[2].flags = MATCH_NODE | MATCH_UNCACHED; /* generic scsi */
		if (devfsadm_enumerate_int(path, RULE_INDEX, &buf, rules, 3)) {
			return (NULL);
		}
	}

	return (buf);
}
