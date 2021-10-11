/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audio_link.c	1.7	99/12/10 SMI"

#include <regex.h>
#include <devfsadm.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#define	MAX_AUDIO_LINK 100
#define	RE_SIZE 64

static void check_audio_link(char *secondary_link,
				char *primary_link_format);
static int audio_process(di_minor_t minor, di_node_t node);

static devfsadm_create_t audio_cbt[] = {
	{ "audio", "ddi_audio", NULL,
	TYPE_EXACT, ILEVEL_0, audio_process
	}
};

DEVFSADM_CREATE_INIT_V0(audio_cbt);

/*
 * the following can't be one big RE with a bunch of alterations "|"
 * because recurse_dev_re() would not work.
 */
static devfsadm_remove_t audio_remove_cbt[] = {
	{ "audio",
	"^sound/[0-9]+.*$",
	RM_PRE, ILEVEL_0, devfsadm_rm_all
	},
	{ "audio",
	"^isdn/[0-9]+/mgt$",
	RM_PRE, ILEVEL_0, devfsadm_rm_all
	},
	{ "audio",
	"^isdn/[0-9]+/aux/0(ctl)?$",
	RM_PRE, ILEVEL_0, devfsadm_rm_all
	},
	{ "audio",
	"^isdn/[0-9]+/(nt)|(te)/((dtrace)|(mgt)|(b1)|(b2)|(d))$",
	RM_PRE, ILEVEL_0, devfsadm_rm_all
	},
	{ "audio",
	"^audio(ctl)?$",
	RM_POST|RM_ALWAYS|RM_HOT, ILEVEL_0, devfsadm_rm_link
	}
};

DEVFSADM_REMOVE_INIT_V0(audio_remove_cbt);

static regex_t isdn_re;

#define	ISDN_RE "((nt)|(te)|(aux))\\,((0)|(0ctl)|(d)|(b1)|(b2)|(mgt)|(dtrace))"
#define	F 1
#define	S 5

int
minor_init(void)
{
	if (0 != regcomp(&isdn_re, ISDN_RE, REG_EXTENDED)) {
		devfsadm_errprint("SUNW_audio_link: minor_init: regular "
				    "expression bad: '%s'\n", ISDN_RE);
		return (DEVFSADM_FAILURE);
	} else {
		return (DEVFSADM_SUCCESS);
	}
}

int
minor_fini(void)
{
	regfree(&isdn_re);
	check_audio_link("audio", "sound/%d");
	check_audio_link("audioctl", "sound/%dctl");
	return (DEVFSADM_SUCCESS);
}


#define	COPYSUB(to, from, pm, pos) (void) strncpy(to, &from[pm[pos].rm_so], \
		    pm[pos].rm_eo - pm[pos].rm_so); \
		    to[pm[pos].rm_eo - pm[pos].rm_so] = 0;

/*
 * This function is called for every audio node.
 * Calls enumerate to assign a logical unit id, and then
 * devfsadm_mklink to make the link.
 */
static int
audio_process(di_minor_t minor, di_node_t node)
{
	char path[PATH_MAX + 1];
	char *buf;
	char *mn;
	char m1[10];
	char m2[10];
	char *devfspath;
	char re_string[RE_SIZE+1];
	devfsadm_enumerate_t rules[1] = {NULL};
	regmatch_t pmatch[12];
	char *au_mn;


	mn = di_minor_name(minor);

	if ((devfspath = di_devfs_path(node)) == NULL) {
		return (DEVFSADM_CONTINUE);
	}
	(void) strcpy(path, devfspath);
	(void) strcat(path, ":");
	(void) strcat(path, mn);
	di_devfs_path_free(devfspath);

	if (strstr(mn, "sound,") != NULL) {
		(void) snprintf(re_string, RE_SIZE, "%s", "^sound$/^([0-9]+)");
	} else {
		(void) strcpy(re_string, "isdn/([0-9]+)");
	}

	/*
	 * We want a match against the physical path
	 * without the minor name component.
	 */
	rules[0].re = re_string;
	rules[0].subexp = 1;
	rules[0].flags = MATCH_ADDR;

	/*
	 * enumerate finds the logical audio id, and stuffs
	 * it in buf
	 */
	if (devfsadm_enumerate_int(path, 0, &buf, rules, 1)) {
		return (DEVFSADM_CONTINUE);
	}

	path[0] = '\0';

	if (strstr(mn, "sound,") != NULL) {
		(void) strcpy(path, "sound/");
		(void) strcat(path, buf);

		/* if this is a minor node, tack on the correct suffix */
		au_mn = strchr(mn, ',');
		if (strcmp(++au_mn, "audio") != 0 &&
		    strcmp(au_mn, "sbpro") != 0) {

			/*
			 * audioctl/sbproctl are special cases. They are handled
			 * by stripping off the audio/sbpro from the node name
			 */
			if (strcmp(au_mn, "audioctl") == 0 ||
			    strcmp(au_mn, "sbproctl") == 0)
				au_mn = strstr(au_mn, "ctl");
			(void) strcat(path, au_mn);
		}
	}

	if (regexec(&isdn_re, mn, sizeof (pmatch) / sizeof (pmatch[0]),
	    pmatch, 0) == 0) {
		COPYSUB(m1, mn, pmatch, F);
		COPYSUB(m2, mn, pmatch, S);
		(void) strcpy(path, "isdn/");
		(void) strcat(path, buf);
		(void) strcat(path, "/");
		(void) strcat(path, m1);
		(void) strcat(path, "/");
		(void) strcat(path, m2);
	}

	if (strstr("mgt,mgt", mn) != NULL) {
		(void) strcpy(path, "isdn/");
		(void) strcat(path, buf);
		(void) strcat(path, "/mgt");
	}

	free(buf);

	if (path[0] == '\0') {
		devfsadm_errprint("SUNW_audio_link: audio_process: can't find"
			" match for'%s'\n", mn);
		return (DEVFSADM_CONTINUE);
	}

	(void) devfsadm_mklink(path, node, minor, 0);
	return (DEVFSADM_CONTINUE);
}


static void
check_audio_link(char *secondary_link, char *primary_link_format)
{
	char primary_link[PATH_MAX + 1];
	int i;

	/* if link is present, return */
	if (devfsadm_link_valid(secondary_link) == DEVFSADM_TRUE) {
		return;
	}

	for (i = 0; i < MAX_AUDIO_LINK; i++) {
		(void) sprintf(primary_link, primary_link_format, i);
		if (devfsadm_link_valid(primary_link) == DEVFSADM_TRUE) {
			(void) devfsadm_secondary_link(secondary_link,
						primary_link, 0);
			break;
		}
	}
}
