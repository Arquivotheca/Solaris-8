/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_info_devt.c	1.16	99/06/04 SMI"

/*
 * The dr_info_*.c files determine devices on a specified board and
 * their current usage by the system.
 *
 * This file deals with reading the dev-info tree for a specified board
 * and creating/freeing a dr_io_t tree which represents it.
 */

#include <string.h>
#include <kvm.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>

#include "dr_info.h"

/* Local Data */

#define	DSIZE	(sizeof (struct dev_info))
#define	KNAMEBUFSIZE 256

/* Local Routines */
int build_notnet_leaf(dr_leafp_t dmpt, char *minor_name);
int read_dev_info(struct dev_info *kaddr,
				    struct dev_info *userp);
char *getkname(char *kaddr);

/*
 * build_notnet_leaf
 *
 * Fill in the leaf structure for a non-network device type.
 * We use the minor name to form the /devices path.
 *
 * Input: dmpt	- leaf entry to fill in
 *	  minor_name - address of the minor name.
 *
 * Function return value: 0 - success, 1 errors
 */
int
build_notnet_leaf(dr_leafp_t dmpt, char *minor_name)
{
	int		length;
	struct stat	sst;

	/*
	 * pathname of the device is:  current_devices_path:ddm_name\0
	 */
	length = strlen(devices_path) + strlen(minor_name) + 2;

	dmpt->devices_name = (char *)malloc(length);
	if (dmpt->devices_name == NULL) {
		return (1);
	}

	sprintf(dmpt->devices_name, "%s:%s", devices_path, minor_name);

	/*
	 * verify that the /devices file in fact
	 * exists and has a real underlying device.
	 * If not, then don't save
	 * this entry since it is meaningless.
	 *
	 * The name may not exist for 2 reasons:  drvconfig
	 * needs to be run or it's real /devices name is
	 * created in an unconventional manner (like the
	 * network entries which have /devices/pseudo entries).
	 */
	if (stat(dmpt->devices_name, &sst) != 0) {

		/* file does not exist or no underlying device */
		free(dmpt->devices_name);
		dmpt->devices_name = NULL;
	} else {
		dmpt->device_id = sst.st_rdev;
	}


	return (0);
}

/*
 * dr_leaf_malloc
 *
 * malloc/clear out dr_leaf_t data structures.
 */
dr_leafp_t
dr_leaf_malloc(void)
{
	dr_leafp_t	p;

	p = (dr_leafp_t)calloc(1, sizeof (dr_leaf_t));
	if (p == NULL)
		dr_logerr(DRV_FAIL, errno, "malloc failed (leaf)");

	num_leaves++;
	return (p);
}

/*
 * dr_dev_malloc
 *
 * malloc/clear out dr_io_t data structures.
 */
dr_iop_t
dr_dev_malloc(void)
{
	dr_iop_t	p;

	p = (dr_iop_t)calloc(1, sizeof (dr_io_t));
	if (p == NULL)
		dr_logerr(DRV_FAIL, 0, "malloc failed (dr_io)");

	return (p);
}

/*
 * free_dr_device_tree
 *
 * Free the given dr_io_tree and all the associated strings and
 * structures attached to it.
 */
void
free_dr_device_tree(dr_iop_t dp)
{
	dr_leafp_t	dmp, tdmp;

	/* Sanity Check */
	if (dp == NULL)
		return;

	/* free up the attached data */
	if (dp->dv_name) free(dp->dv_name);
	if (dp->dv_addr) free(dp->dv_addr);

	/* free up the minor structure */
	dmp = dp->dv_leaf;
	while (dmp) {

		if (dp->dv_node_type != DEVICE_NET) {
			if (dmp->devices_name) free(dmp->devices_name);
			if (dmp->dev_name) free(dmp->dev_name);
			if (dmp->mount_point) free(dmp->mount_point);
			if (dmp->ap_metaname) free(dmp->ap_metaname);
		}

		tdmp = dmp->next;
		free(dmp);
		dmp = tdmp;
	}

	if (dp->dv_child)
		free_dr_device_tree(dp->dv_child);
	if (dp->dv_sibling)
		free_dr_device_tree(dp->dv_sibling);

	free(dp);
}
