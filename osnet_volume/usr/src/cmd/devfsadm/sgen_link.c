/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sgen_link.c	1.1	99/09/23 SMI"

#include <devfsadm.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>

#define	SGEN_LINK_RE	"^scsi/.+/c[0-9]+t[0-9]+d[0-9]+$"
#define	SGEN_DIR	"scsi"
#define	SGEN_CLASS	"generic-scsi"

static int sgen_callback(di_minor_t minor, di_node_t node);
static char *find_ctrlr(di_node_t node, di_minor_t minor);


static devfsadm_create_t sgen_create_cbt[] = {
	{ SGEN_CLASS, "ddi_generic:scsi", NULL,
	    TYPE_EXACT | CREATE_DEFER, ILEVEL_0, sgen_callback
	}
};

DEVFSADM_CREATE_INIT_V0(sgen_create_cbt);

/*
 * HOT auto cleanup of sgen links not desired.
 */
static devfsadm_remove_t sgen_remove_cbt[] = {
	{ SGEN_CLASS, SGEN_LINK_RE, RM_POST,
		ILEVEL_0, devfsadm_rm_all
	}
};

DEVFSADM_REMOVE_INIT_V0(sgen_remove_cbt);

static int
sgen_callback(di_minor_t minor, di_node_t node)
{
	char *cp;
	char lpath[PATH_MAX + 1];
	uint_t targ;
	uint_t lun;

	cp = di_bus_addr(node);
	(void) sscanf(cp, "%X,%X", &targ, &lun);

	if ((cp = find_ctrlr(node, minor)) == NULL) {
		return (DEVFSADM_CONTINUE);
	}

	(void) snprintf(lpath, sizeof (lpath), "%s/%s/c%st%dd%d",
	    SGEN_DIR, di_minor_name(minor), cp, targ, lun);

	free(cp);

	(void) devfsadm_mklink(lpath, node, minor, 0);
	return (DEVFSADM_CONTINUE);
}

/* index of enumeration rule applicable to this module */
#define	RULE_INDEX	2

static char *
find_ctrlr(di_node_t node, di_minor_t minor)
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
	 * Use controller (parent) component of device path
	 */
	if (devfsadm_enumerate_int(path, RULE_INDEX, &buf, rules, 3)) {
		/*
		 * Maybe failed because there are multiple logical controller
		 * numbers for a single physical controller.  If we use node
		 * name also for DEVICE paths in the match it should fix this
		 * and only find one logical controller. (See 4045879).
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
