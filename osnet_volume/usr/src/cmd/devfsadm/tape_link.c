/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tape_link.c	1.4	99/08/30 SMI"

#include <devfsadm.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>


static int tape_process(di_minor_t minor, di_node_t node);

static devfsadm_create_t tape_cbt[] = {
	{ "tape", "ddi_byte:tape", NULL,
	TYPE_EXACT, ILEVEL_0,	tape_process
	},
};

DEVFSADM_CREATE_INIT_V0(tape_cbt);

#define	TAPE_LINK_RE "^rmt/[0-9][cbhlmnu]*"

static devfsadm_remove_t tape_remove_cbt[] = {
	{ "tape", TAPE_LINK_RE, RM_PRE, ILEVEL_0, devfsadm_rm_all
	}
};

DEVFSADM_REMOVE_INIT_V0(tape_remove_cbt);


/*
 * This function is called for every tape minor node.
 * Calls enumerate to assign a logical tape id, and then
 * devfsadm_mklink to make the link.
 */
static int
tape_process(di_minor_t minor, di_node_t node)
{
	char l_path[PATH_MAX + 1];
	char *buf;
	char *mn;
	char *devfspath;
	devfsadm_enumerate_t rules[1] = {"rmt/([0-9]+)", 1, MATCH_ADDR};

	mn = di_minor_name(minor);


	if ((mn != NULL) && (*mn >= '0') && (*mn <= '9')) {
		/*
		 * first character cannot be a digit as it would combine
		 * with the tape instance number to make an ambiguous quantity.
		 */
		return (DEVFSADM_CONTINUE);
	}

	devfspath = di_devfs_path(node);

	(void) strcpy(l_path, devfspath);
	(void) strcat(l_path, ":");
	(void) strcat(l_path, mn);

	di_devfs_path_free(devfspath);

	/*
	 *  devfsadm_enumerate finds the logical tape id from the physical path,
	 *  omitting minor name field. The logical tape id is returned in buf.
	 */
	if (devfsadm_enumerate_int(l_path, 0, &buf, rules, 1)) {
		return (DEVFSADM_CONTINUE);
	}

	(void) strcpy(l_path, "rmt/");
	(void) strcat(l_path, buf);
	(void) strcat(l_path, mn);
	free(buf);

	(void) devfsadm_mklink(l_path, node, minor, 0);

	return (DEVFSADM_CONTINUE);
}
