/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * help.c -- routines to display help menus
 */

#ident "@(#)help.c   1.9   97/03/21 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tty.h"
#include "menu.h"

static struct menu_list Topics[] = {
	{ "Navigating the Menus", "MENU_HELP_NAVIGATE", 0 },
	{ "Device scanning", "MENU_HELP_PROBE_ALL", 0},
	{ "Resource conflicts", "MENU_HELP_CONFLICTS", 0 },
	{ "Booting Solaris", "MENU_HELP_BOOT", 0 },
	{ "Mounting", "MENU_HELP_MOUNT", 0 },
	{ "Saving a configuration", "MENU_HELP_NEW_CONFIG", 0 },
};

#define	NTOPICS	(sizeof (Topics) / sizeof (*Topics))

static struct menu_options Topics_options[] = {
	{ FKEY(2), MA_RETURN, "Goto" },
	{ FKEY(3), MA_RETURN, "Exit Help" },
};

#define	NTOPICS_OPTIONS	(sizeof (Topics_options) / sizeof (*Topics_options))

static struct menu_options Help_options[] = {
	{ FKEY(2), MA_RETURN, "Help Topics" },
	{ FKEY(3), MA_RETURN, "Exit Help" },
};

#define	NHELP_OPTIONS	(sizeof (Help_options) / sizeof (*Help_options))

/*
 * menu_help -- display a help menu
 */

void
menu_help(const char *topic)
{
	struct menu_list *choice;
	/*
	 * let user roam around the help screens until done,
	 * then return to previous menu
	 */
	/*CONSTANTCONDITION*/
	while (1) {
		/* display help on topic */
		if (topic)
			switch (text_menu(NULL, Help_options, NHELP_OPTIONS,
			    NULL, topic)) {
			case FKEY(2):
				/*
				 * user wants topic list,
				 * fall through to HELP_TOPICS menu below
				 */
				break;

			case FKEY(3):
				/* exit help (return to previous menu) */
				return;
			}

		/* display list of help topics */
		clear_selection_menu(Topics, NTOPICS);
again:
		switch (select_menu(NULL, Topics_options, NTOPICS_OPTIONS,
		    Topics, NTOPICS, MS_ZERO_ONE, "MENU_HELP_TOPICS")) {
		case FKEY(2):
			/*
			 * user selected a topic,
			 * go around to topic menu above
			 */
			if ((choice = get_selection_menu(Topics, NTOPICS))
			    == NULL) {
				beep_tty();
				goto again;
			}
			topic = (char *)choice->datum;
			break;

		case FKEY(3):
			/* exit help (return to previous menu) */
			return;
		}
	}
}
