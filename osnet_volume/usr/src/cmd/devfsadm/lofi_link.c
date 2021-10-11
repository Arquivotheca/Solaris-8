/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lofi_link.c	1.1	99/07/27 SMI"

#include <regex.h>
#include <devfsadm.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/mkdev.h>
#include <sys/lofi.h>


static int lofi(di_minor_t minor, di_node_t node);

static devfsadm_create_t lofi_cbt[] = {
	{ "pseudo", "ddi_pseudo", LOFI_DRIVER_NAME,
	    TYPE_EXACT | DRV_EXACT, ILEVEL_0, lofi,
	},
};

DEVFSADM_CREATE_INIT_V0(lofi_cbt);

/*
 * For the master device:
 *	/dev/lofictl -> /devices/pseudo/lofi@0:ctl
 * For each other device
 *	/dev/lofi/1 -> /devices/pseudo/lofi@0:1
 *	/dev/rlofi/1 -> /devices/pseudo/lofi@0:1,raw
 */
static int
lofi(di_minor_t minor, di_node_t node)
{
	dev_t	dev;
	char mn[MAXNAMELEN + 1];
	char blkname[MAXNAMELEN + 1];
	char rawname[MAXNAMELEN + 1];
	char path[PATH_MAX + 1];

	(void) strcpy(mn, di_minor_name(minor));

	if (strcmp(mn, "ctl") == 0) {
		(void) devfsadm_mklink(LOFI_CTL_NAME, node, minor, 0);
	} else {
		dev = di_minor_devt(minor);
		(void) snprintf(blkname, sizeof (blkname), "%d",
		    minor(dev));
		(void) snprintf(rawname, sizeof (rawname), "%d,raw",
		    minor(dev));

		if (strcmp(mn, blkname) == 0) {
			(void) snprintf(path, sizeof (path), "%s/%s",
			    LOFI_BLOCK_NAME, blkname);
		} else if (strcmp(mn, rawname) == 0) {
			(void) snprintf(path, sizeof (path), "%s/%s",
			    LOFI_CHAR_NAME, blkname);
		} else {
			return (DEVFSADM_CONTINUE);
		}

		(void) devfsadm_mklink(path, node, minor, 0);
	}
	return (DEVFSADM_CONTINUE);
}
