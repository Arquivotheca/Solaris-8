/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)adv.c	1.20	99/06/21 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <names.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adv.h"
#include "boards.h"
#include "boot.h"
#include "cfname.h"
#include "config.h"
#include "debug.h"
#include "devdb.h"
#include "err.h"
#include "gettext.h"
#include "kbd.h"
#include "menu.h"
#include "probe.h"
#include "prop.h"
#include "tty.h"

static struct menu_options adv_opts[] = {
	/*  Function key list for the main "probe" screen ...		    */

	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Back" },
};

#define	NADV_OPTIONS (sizeof (adv_opts) / sizeof (*adv_opts))

static struct menu_list adv_list[] = {
	/*
	 * Device Tasks menu
	 */
	{ "View/Edit Devices", (void *)do_config, 0 },
	{ "Set Keyboard Configuration", (void *)ask_kbd, 0 },
	{ "Save Configuration", (void *)save_cfname, 0 },
	{ "Delete Saved Configuration", (void *)delete_cfname, 0 },
	{ "Set Console Device", (void *)console_tty, 0 },
};

#define	NADV_LIST (sizeof (adv_list) / sizeof (*adv_list))

void netconfig();

static struct menu_list boot_tasks_list[] = {
	{ "View/Edit Autoboot Settings", (void *)auto_boot_cfname, 0 },
	{ "View/Edit Property Settings", (void *)menu_prop, 0 },
	{ "Set Network Configuration Strategy", (void *)netconfig, 0 },
};
#define	BOOT_TASKS_LIST (sizeof (boot_tasks_list) / sizeof (*boot_tasks_list))

/*
 * Advanced menu.
 */
void
menu_adv(void)
{
	struct menu_list *choice;
	struct menu_list *list;
	int nlist;

	nlist = NADV_LIST;
	list = adv_list;

	for (;;) {
		switch (select_menu("MENU_HELP_ADV", adv_opts, NADV_OPTIONS,
		    list, nlist, MS_ZERO_ONE, "MENU_ADV")) {

		case FKEY(2):
			choice = get_selection_menu(list, nlist);
			if (choice == NULL) {
				/* user didn't pick one */
				beep_tty();
			} else {
				(*(void (*)())choice->datum)();
			}
			continue;
		case FKEY(3):
			return;
		}
	}
}

void
boot_tasks_adv(void)
{
	struct menu_list *choice;

	for (;;) {
		switch (select_menu("MENU_HELP_BOOT_TASKS", adv_opts,
		    NADV_OPTIONS, boot_tasks_list, BOOT_TASKS_LIST,
		    MS_ZERO_ONE, "MENU_BOOT_TASKS")) {

		case FKEY(2):
			choice = get_selection_menu(boot_tasks_list,
			    BOOT_TASKS_LIST);
			if (choice == NULL) {
				/* user didn't pick one */
				beep_tty();
			} else {
				(*(void (*)())choice->datum)();
			}
			continue;
		case FKEY(3):
			return;
		}
	}
}


/*
 * options for "Solaris Netboot Configuration Strategy" menu
 */
static struct menu_options netconfig_opts[] = {
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },
};

#define	NNETCONFIG_OPTIONS (sizeof (netconfig_opts) / sizeof (*netconfig_opts))

#define	SET_RARP	0
#define	SET_DHCP	1
static struct menu_list netconfig_list[] = {
	{ "DHCP", (void *)SET_DHCP, 0, 0 },
	{ "RARP", (void *)SET_RARP, 0, 0 },
};

#define	NNETCONFIG_LIST (sizeof (netconfig_list) / sizeof (struct menu_list))

void
netconfig(void)
{
	struct menu_list *choice, *p;
	char *sp;
	char *val;
	char buf[200];
	int found = 0;

	sp = read_prop("net-config-strategy", "options");

	if ((sp != NULL) && (strlen(sp) >= 4)) {
		for (p = netconfig_list; p < &netconfig_list[NNETCONFIG_LIST];
		    p++) {

			/*
			 * Setup the default strategy to be selected for the
			 * user. Otherwise clear out those bits in case
			 * this menu as been visited more than once.
			 */
			if (ncstrcmp(sp, p->string) == 0) {
				p->flags |= MF_SELECTED | MF_CURSOR;
				found = 1;
			} else {
				p->flags &= ~(MF_SELECTED | MF_CURSOR);
			}
		}
	}

	if (!found) {
		enter_menu("MENU_HELP_NETCONFIG", "MENU_BAD_NETCONFIG", sp);
		for (p = netconfig_list; p < &netconfig_list[NNETCONFIG_LIST];
		    p++)
			p->flags &= ~(MF_SELECTED | MF_CURSOR);
	}

	for (;;) {
		switch (select_menu("MENU_HELP_NETCONFIG", netconfig_opts,
		    NNETCONFIG_OPTIONS, netconfig_list, NNETCONFIG_LIST,
		    MS_ZERO_ONE, "MENU_NETCONFIG")) {

		case FKEY(2):
			choice = get_selection_menu(netconfig_list,
			    NNETCONFIG_LIST);
			if (choice == NULL) {
				/* user didn't pick one */
				beep_tty();
			} else {
				switch ((int)choice->datum) {
				case SET_RARP:
					val = "rarp";
					break;

				case SET_DHCP:
					val = "dhcp";
					break;
				}

				/*
				 * Give the user some indication of what
				 * happened on the original screen before
				 * the switch
				 */
				status_menu(Please_wait,
				    "STATUS_NETCONFIG_SWITCH", choice->string);

				/*
				 * change the net-config-strategy now. even
				 * though store_prop will eventually call
				 * out_bop we want the menu updates to occur
				 * on the new net-config-strategy asap.
				 */
				(void) sprintf(buf,
				    "setprop net-config-strategy %s\n", val);
				out_bop(buf);

				/*
				 * Now change the properties on the disk.
				 * Only have the first store_prop() update
				 * the display.
				 */
				store_prop(Machenv_name, "net-config-strategy",
				    val, FALSE);

				return;
			}
			continue;
		case FKEY(3):
			return;
		}
	}
}

ncstrcmp(char *p, char *q)
{
	while (*q) {
		/*
		 *  Keep going until we reach the end of the name
		 *  or until we know the text doesn't match.
		 */

		int c = toupper(*p);
		p++;

		if (c != *q++)
			return (-1);
	}

	return (0);
}
