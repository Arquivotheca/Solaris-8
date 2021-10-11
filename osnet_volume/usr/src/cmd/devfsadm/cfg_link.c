/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cfg_link.c	1.4	99/09/23 SMI"

#include <devfsadm.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>

#define	SCSI_CFG_LINK_RE	"^cfg/c[0-9]+$"
#define	CFG_DIRNAME		"cfg"

static int scsi_cfg_creat_cb(di_minor_t minor, di_node_t node);

/*
 * NOTE: The CREATE_DEFER flag is private to this module.
 *	 NOT to be used by other modules
 */
static devfsadm_create_t cfg_create_cbt[] = {
	{ "attachment-point", "ddi_ctl:attachment_point:scsi", NULL,
	    TYPE_EXACT | CREATE_DEFER, ILEVEL_0, scsi_cfg_creat_cb
	}
};

DEVFSADM_CREATE_INIT_V0(cfg_create_cbt);

/*
 * No hot auto-cleanup
 */
static devfsadm_remove_t cfg_remove_cbt[] = {
	{ "attachment-point", SCSI_CFG_LINK_RE, RM_POST,
		ILEVEL_0, devfsadm_rm_all
	}
};

DEVFSADM_REMOVE_INIT_V0(cfg_remove_cbt);

static int
scsi_cfg_creat_cb(di_minor_t minor, di_node_t node)
{
	char path[PATH_MAX + 1];
	char *c_num = NULL, *devfs_path, *mn;
	devfsadm_enumerate_t rules[3] = {
	    {"^r?dsk$/^c([0-9]+)", 1, MATCH_PARENT},
	    {"^cfg$/^c([0-9]+)$", 1, MATCH_ADDR},
	    {"^scsi$/^.+$/^c([0-9]+)", 1, MATCH_PARENT}
	};

	mn = di_minor_name(minor);

	if ((devfs_path = di_devfs_path(node)) == NULL) {
		return (DEVFSADM_CONTINUE);
	}
	(void) strcpy(path, devfs_path);
	(void) strcat(path, ":");
	(void) strcat(path, mn);
	di_devfs_path_free(devfs_path);

	if (devfsadm_enumerate_int(path, 1, &c_num, rules, 3)
	    == DEVFSADM_FAILURE) {
		/*
		 * Unlike the disks module we don't retry on failure.
		 * If we have multiple "c" numbers for a single physical
		 * controller due to bug 4045879, we will not assign a
		 * c-number/symlink for the controller.
		 */
		return (DEVFSADM_CONTINUE);
	}

	(void) strcpy(path, CFG_DIRNAME);
	(void) strcat(path, "/c");
	(void) strcat(path, c_num);

	free(c_num);

	(void) devfsadm_mklink(path, node, minor, 0);

	return (DEVFSADM_CONTINUE);
}
