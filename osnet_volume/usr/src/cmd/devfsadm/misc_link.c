/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)misc_link.c	1.17	99/08/30 SMI"

#include <regex.h>
#include <devfsadm.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>


static int display(di_minor_t minor, di_node_t node);
static int parallel(di_minor_t minor, di_node_t node);
static int node_slash_minor(di_minor_t minor, di_node_t node);
static int driver_minor(di_minor_t minor, di_node_t node);
static int spif(di_minor_t minor, di_node_t node);
static int node_name(di_minor_t minor, di_node_t node);
static int minor_name(di_minor_t minor, di_node_t node);
static int conskbd(di_minor_t minor, di_node_t node);
static int consms(di_minor_t minor, di_node_t node);
static int power_button(di_minor_t minor, di_node_t node);
static int fc_port(di_minor_t minor, di_node_t node);

static devfsadm_create_t misc_cbt[] = {
	{ "pseudo", "ddi_pseudo", "(^pts$)|(^sad$)",
	    TYPE_EXACT | DRV_RE, ILEVEL_0, node_slash_minor
	},
	{ "pseudo", "ddi_pseudo", "zsh",
	    TYPE_EXACT | DRV_EXACT, ILEVEL_0, driver_minor
	},
	{ "network", "ddi_network", NULL,
	    TYPE_EXACT, ILEVEL_0, minor_name
	},
	{ "display", "ddi_display", NULL,
	    TYPE_EXACT, ILEVEL_0, display
	},
	{ "parallel", "ddi_parallel", NULL,
	    TYPE_EXACT, ILEVEL_0, parallel
	},
	{ "pseudo", "ddi_pseudo", "stc",
	    TYPE_EXACT | DRV_EXACT, ILEVEL_0, spif
	},
	{ "pseudo", "ddi_pseudo", "(^winlock$)|(^pm$)|(^sx_cmem$)",
	    TYPE_EXACT | DRV_RE, ILEVEL_0, node_name
	},
	{ "pseudo", "ddi_pseudo", "conskbd",
	    TYPE_EXACT | DRV_EXACT, ILEVEL_0, conskbd
	},
	{ "pseudo", "ddi_pseudo", "consms",
	    TYPE_EXACT | DRV_EXACT, ILEVEL_0, consms
	},
	{ "pseudo", "ddi_pseudo",
	    "(^lockstat$)|(^SUNW,rtvc$)|(^vol$)|(^log$)|(^sy$)|"
	    "(^ksyms$)|(^bpp$)|(^clone$)|(^tl$)|(^sx$)|(^tnf$)|(^kstat$)|"
	    "(^eeprom$)|(^ptsl$)|(^mm$)|(^wc$)|(^dump$)|(^cn$)|(^lo$)|(^ptm$)|"
	    "(^ptc$)|(^openeepr$)|(^poll$)|(^sysmsg$)",
	    TYPE_EXACT | DRV_RE, ILEVEL_1, minor_name
	},
	{ "pseudo", "ddi_pseudo",
	    "(^ip$)|(^tcp$)|(^udp$)|(^icmp$)|"
	    "(^ip6$)|(^tcp6$)|(^udp6$)|(^icmp6$)|"
	    "(^rts$)|(^arp$)|(^ipsecah$)|(^ipsecesp$)|(^keysock$)",
	    TYPE_EXACT | DRV_RE, ILEVEL_1, minor_name
	},
	{ "pseudo", "ddi_pseudo",
	    "(^kdmouse$)|(^logi$)|(^rootprop$)|(^msm$)",
	    TYPE_EXACT | DRV_RE, ILEVEL_0, node_name
	},
	{ "pseudo", "ddi_pseudo", "tod",
	    TYPE_EXACT | DRV_EXACT, ILEVEL_0, node_name
	},
	{ "pseudo", "ddi_pseudo", "envctrl(two)?",
	    TYPE_EXACT | DRV_RE, ILEVEL_1, minor_name,
	},
	{ "power_button", "ddi_power_button", NULL,
	    TYPE_EXACT, ILEVEL_0, power_button,
	},
	{ "FC port", "ddi_ctl:devctl", "fp",
	    TYPE_EXACT | DRV_EXACT, ILEVEL_0, fc_port
	},
};

DEVFSADM_CREATE_INIT_V0(misc_cbt);

static devfsadm_remove_t misc_remove_cbt[] = {
	{ "pseudo", "^profile$",
	    RM_PRE | RM_ALWAYS, ILEVEL_0, devfsadm_rm_all },
};

DEVFSADM_REMOVE_INIT_V0(misc_remove_cbt);

/*
 * Handles minor node type "ddi_display".
 *
 * type=ddi_display fbs/\M0 fb\N0
 */
static int
display(di_minor_t minor, di_node_t node)
{
	char l_path[PATH_MAX + 1], contents[PATH_MAX + 1], *buf;
	devfsadm_enumerate_t rules[1] = {"^fb([0-9]+)$", 1, MATCH_ALL};
	char *mn = di_minor_name(minor);

	/* create fbs/\M0 primary link */
	(void) strcpy(l_path, "fbs/");
	(void) strcat(l_path, mn);
	(void) devfsadm_mklink(l_path, node, minor, 0);

	/* create fb\N0 which links to fbs/\M0 */
	if (devfsadm_enumerate_int(l_path, 0, &buf, rules, 1)) {
		return (DEVFSADM_CONTINUE);
	}
	(void) strcpy(contents, l_path);
	(void) strcpy(l_path, "fb");
	(void) strcat(l_path, buf);
	free(buf);
	(void) devfsadm_secondary_link(l_path, contents, 0);
	return (DEVFSADM_CONTINUE);
}

/*
 * Handles minor node type "ddi_parallel".
 *
 * type=ddi_parallel;name=mcpp     mcpp\N0
 * type=ddi_parallel;name=SUNW,spif;minor=stclp    printers/\N0
 */
static int
parallel(di_minor_t minor, di_node_t node)
{
	char p_path[PATH_MAX + 1], l_path[PATH_MAX + 1], *buf;
	char *ptr, *nn = di_node_name(node), *mn;

	if (NULL == (ptr = di_devfs_path(node))) {
		return (DEVFSADM_CONTINUE);
	}

	mn = di_minor_name(minor);

	(void) strcpy(p_path, ptr);
	(void) strcat(p_path, ":");
	(void) strcat(p_path, mn);
	di_devfs_path_free(ptr);

	if (strcmp(nn, "mcpp") == 0) {
		devfsadm_enumerate_t rules[1] =
		    {"mcpp([0-9]+)$", 1, MATCH_ALL};

		if (devfsadm_enumerate_int(p_path, 0, &buf, rules, 1)) {
			return (DEVFSADM_CONTINUE);
		}
		(void) strcpy(l_path, "mcpp");
		(void) strcat(l_path, buf);

	} else if ((strcmp(nn, "SUNW,spif") == 0) &&
			(strcmp(mn, "stclp") == 0)) {
		devfsadm_enumerate_t rules[1] =
		    {"printers/([0-9]+)$", 1, MATCH_ALL};


		if (devfsadm_enumerate_int(p_path, 0, &buf, rules, 1)) {
			return (DEVFSADM_CONTINUE);
		}
		(void) strcpy(l_path, "printers/");
		(void) strcat(l_path, buf);
	} else {
		return (DEVFSADM_CONTINUE);
	}
	free(buf);
	(void) devfsadm_mklink(l_path, node, minor, 0);
	return (DEVFSADM_CONTINUE);
}

static int
node_slash_minor(di_minor_t minor, di_node_t node)
{

	char path[PATH_MAX + 1];

	(void) strcpy(path, di_node_name(node));
	(void) strcat(path, "/");
	(void) strcat(path, di_minor_name(minor));
	(void) devfsadm_mklink(path, node, minor, 0);
	return (DEVFSADM_CONTINUE);
}

static int
driver_minor(di_minor_t minor, di_node_t node)
{
	char path[PATH_MAX + 1];

	(void) strcpy(path, di_driver_name(node));
	(void) strcat(path, di_minor_name(minor));
	(void) devfsadm_mklink(path, node, minor, 0);
	return (DEVFSADM_CONTINUE);
}

/*
 * type=ddi_pseudo;name=SUNW,spif;minor=stc        sad/stc\N0
 */
int
spif(di_minor_t minor, di_node_t node)
{

	devfsadm_enumerate_t rules[1] = {"sad/stc([0-9]+)$", 1, MATCH_ALL};
	char *buf, path[PATH_MAX + 1];
	char *ptr;

	if (NULL == (ptr = di_devfs_path(node))) {
		return (DEVFSADM_CONTINUE);
	}

	(void) strcpy(path, ptr);
	(void) strcat(path, ":");
	(void) strcat(path, di_minor_name(minor));

	di_devfs_path_free(ptr);

	if (devfsadm_enumerate_int(path, 0, &buf, rules, 1) != 0) {
		return (DEVFSADM_CONTINUE);
	}
	(void) strcpy(path, "sad/stc");
	(void) strcat(path, buf);
	free(buf);
	(void) devfsadm_mklink(path, node, minor, 0);
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

/*
 * Handles links of the form:
 * type=ddi_pseudo;name=xyz  \M0
 */
static int
minor_name(di_minor_t minor, di_node_t node)
{
	char *mn = di_minor_name(minor);

	(void) devfsadm_mklink(mn, node, minor, 0);
	if (strcmp(mn, "icmp") == 0) {
		(void) devfsadm_mklink("rawip", node, minor, 0);
	}
	if (strcmp(mn, "icmp6") == 0) {
		(void) devfsadm_mklink("rawip6", node, minor, 0);
	}
	return (DEVFSADM_CONTINUE);
}


static int
conskbd(di_minor_t minor, di_node_t node)
{
	(void) devfsadm_mklink("kbd", node, minor, 0);
	return (DEVFSADM_CONTINUE);
}

static int
consms(di_minor_t minor, di_node_t node)
{
	(void) devfsadm_mklink("mouse", node, minor, 0);
	return (DEVFSADM_CONTINUE);
}

static int
power_button(di_minor_t minor, di_node_t node)
{
	(void) devfsadm_mklink("power_button", node, minor, 0);
	return (DEVFSADM_CONTINUE);
}

static int
fc_port(di_minor_t minor, di_node_t node)
{
	devfsadm_enumerate_t rules[1] = {"fc/fp([0-9]+)$", 1, MATCH_ALL};
	char *buf, path[PATH_MAX + 1];
	char *ptr;

	if (NULL == (ptr = di_devfs_path(node))) {
		return (DEVFSADM_CONTINUE);
	}

	(void) strcpy(path, ptr);
	(void) strcat(path, ":");
	(void) strcat(path, di_minor_name(minor));

	di_devfs_path_free(ptr);

	if (devfsadm_enumerate_int(path, 0, &buf, rules, 1) != 0) {
		return (DEVFSADM_CONTINUE);
	}

	(void) strcpy(path, "fc/fp");
	(void) strcat(path, buf);
	free(buf);

	(void) devfsadm_mklink(path, node, minor, 0);
	return (DEVFSADM_CONTINUE);
}
