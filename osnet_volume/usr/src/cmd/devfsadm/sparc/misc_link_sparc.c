/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)misc_link_sparc.c	1.7	99/07/09 SMI"

#include <regex.h>
#include <devfsadm.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>

static int node_name(di_minor_t minor, di_node_t node);
static int ddi_other(di_minor_t minor, di_node_t node);
static int diskette(di_minor_t minor, di_node_t node);

static devfsadm_create_t pseudo_cbt[] = {
	{ "other", "ddi_other", NULL,
	    TYPE_EXACT, ILEVEL_0, ddi_other
	},
	{ "disk",  "ddi_block:diskette", NULL,
	    TYPE_EXACT, ILEVEL_1, diskette
	}
};

DEVFSADM_CREATE_INIT_V0(pseudo_cbt);

/*
 * Handles minor node type "ddi_other"
 * type=ddi_other;name=SUNW,pmc    pmc
 * type=ddi_other;name=SUNW,mic    mic\M0
 */
static int
ddi_other(di_minor_t minor, di_node_t node)
{
	char path[PATH_MAX + 1];
	char *nn = di_node_name(node);
	char *mn = di_minor_name(minor);

	if (strcmp(nn, "SUNW,pmc") == 0) {
		(void) devfsadm_mklink("pcm", node, minor, 0);
	} else if (strcmp(nn, "SUNW,mic") == 0) {
		(void) strcpy(path, "mic");
		(void) strcat(path, mn);
		(void) devfsadm_mklink(path, node, minor, 0);
	}

	return (DEVFSADM_CONTINUE);
}

/*
 * This function is called for diskette nodes
 */
static int
diskette(di_minor_t minor, di_node_t node)
{
	char *mn = di_minor_name(minor);
	if (strcmp(mn, "c") == 0) {
		(void) devfsadm_mklink("diskette", node, minor, 0);
		(void) devfsadm_mklink("diskette0", node, minor, 0);

	} else if (strcmp(mn, "c,raw") == 0) {
		(void) devfsadm_mklink("rdiskette", node, minor, 0);
		(void) devfsadm_mklink("rdiskette0", node, minor, 0);

	}
	return (DEVFSADM_CONTINUE);
}

/*
 * Handles links of the form:
 * type=ddi_pseudo;name=xyz  \D
 */
static int
node_name(di_minor_t minor, di_node_t node)
{
	(void) devfsadm_mklink(di_node_name(node), node, minor, 0);
	return (DEVFSADM_CONTINUE);
}
