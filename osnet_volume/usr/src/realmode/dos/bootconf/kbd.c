/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * kbd.c -- kdb-related routines
 */

#ident	"@(#)kbd.c	1.26	99/04/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dostypes.h>
#include <dos.h>
#include "types.h"

#include "boards.h"
#include "boot.h"
#include "bop.h"
#include "befinst.h"
#include "cfname.h"
#include "debug.h"
#include "devdb.h"
#include "dir.h"
#include "enum.h"
#include "escd.h"
#include "err.h"
#include "gettext.h"
#include "open.h"
#include "menu.h"
#include "prop.h"
#include "resmgmt.h"
#include "tty.h"

/*
 * Note, keyboard names may NOT have embedded spaces.
 * this is because they are saved as properties which
 * can't have them either.
 */
static char *lang[] = {
	"Czech",
	"Danish",
	"Dutch",
	"French",
	"French-Canadian",
	"German",
	"Greek",
	"Hungarian",
	"Italian",
	"Japanese(106)",
	"Japanese(J3100)",
	"Korean",
	"Latvian",
	"Lithuanian",
	"Norwegian",
	"Polish",
	"Russian",
	"Spanish",
	"Swedish",
	"Swiss-French",
	"Swiss-German",
	"Taiwanese",
	"Turkish",
	"UK-English",
	"US-English",
};

#define	NKBD_LIST (sizeof (lang) / sizeof (char *))

static struct menu_list Kbd_list[NKBD_LIST];

/*
 * input options for main menu
 */
static struct menu_options Kbd_options[] = {
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },
};

#define	NKBD_OPTIONS (sizeof (Kbd_options) / sizeof (*Kbd_options))

/*
 * Answers to Windows(TM) keys question
 */
static struct menu_list wkeys_list[] = {
	{ "with Windows keys", (void *)1, 0 },
	{ "without Windows keys", (void *)0, 0 },
};

#define	NWKEYS_LIST (sizeof (wkeys_list) / sizeof (struct menu_list))

/*
 * ask_kbd -- determine keyboard nationality and whether a Windows(TM)
 *            keyboard is used
 */
void
ask_kbd(void)
{
	struct menu_list *mlp, *choice;
	char *Nationality;
	Board *keybp;
	int i;
	long nkeys;

	for (i = 0; i < NKBD_LIST; i++) {
		Kbd_list[i].string = (char *)gettext(lang[i]);
	}
	while (select_menu("MENU_HELP_KBD_TYPE", Kbd_options, NKBD_OPTIONS,
	    Kbd_list, NKBD_LIST, MS_ZERO_ONE, "MENU_KBD_TYPE") == FKEY(2)) {
		if ((mlp = get_selection_menu(Kbd_list, NKBD_LIST)) == NULL) {
			beep_tty();
			continue;
		}
		Nationality = lang[mlp - Kbd_list];
		mlp->flags = MF_SELECTED;
		store_prop(Machenv_name, "kbd-type", Nationality, TRUE);
		/*
		 * Find the board record for the keyboard and update the
		 * keyboard-type and keyboard-nkeys properties.  We find
		 * the proper record by finding the board record with irq 1.
		 */
		keybp = Query_resmgmt(Head_board, RESF_Irq, 1, 1);
		(void) SetDevProp_devdb(&keybp->prop, "keyboard-type",
			Nationality, strlen(Nationality)+1, PROP_CHAR);
		switch (select_menu("MENU_HELP_WKEYS", Kbd_options,
			NKBD_OPTIONS, wkeys_list, NWKEYS_LIST, MS_ZERO_ONE,
			"MENU_WKEYS")) {
		case FKEY(2):
			if ((choice = get_selection_menu(wkeys_list,
				NWKEYS_LIST)) == NULL) {
				beep_tty();
				continue;
			}
			choice->flags &= ~MF_SELECTED;
			if (choice->datum) {
				store_prop(Machenv_name, "kbd-wkeys",
				    "true", TRUE);
				nkeys = 104;
			} else {
				store_prop(Machenv_name, "kbd-wkeys",
				    "false", TRUE);
				nkeys = 101;
			}
			(void) SetDevProp_devdb(&keybp->prop,
				"keyboard-nkeys", &nkeys, 4, PROP_BIN);
			break;
		case FKEY(3):
			return;
		}
		return;
	}
}

/*
 * Check whether the properties that govern the keyboard nationality and the
 * number of keys are set. If not, set them to the default values of
 * kbd-type=US-English and kbd-wkeys=true (104 keys).
 */
void
check_kbd(void)
{
	char *buf;
	int i, has_wkeys;

	buf = read_prop("kbd-type", "options");
	if (buf == NULL || buf[0] == 0 || buf[0] == '\n' || buf[0] == '\r') {
		/*
		 * No "kbd-type" property, set default.
		 */
		store_prop(Machenv_name, "kbd-type", "US-English", FALSE);
	}
	/*
	 * Turn on appropriate selected flag.
	 */
	for (i = 0; i < NKBD_LIST; i++) {
		Kbd_list[i].flags = 0;
		if (_strnicmp(lang[i], buf, strlen(lang[i])) == 0) {
			Kbd_list[i].flags = MF_SELECTED;
		}
	}
	buf = read_prop("kbd-wkeys", "options");
	if (buf == NULL || buf[0] == 0 || buf[0] == '\n' || buf[0] == '\r') {
		/*
		 * No "kbd-wkeys" property, set default.
		 */
		buf = "true";
		store_prop(Machenv_name, "kbd-wkeys", buf, FALSE);
	}
	has_wkeys = (_strnicmp("true", buf, 4) == 0);
	/*
	 * Turn on appropriate selected flag.
	 */
	for (i = 0; i < NWKEYS_LIST; i++) {
		wkeys_list[i].flags = 0;
		if (wkeys_list[i].datum == (void *)has_wkeys)
			wkeys_list[i].flags = MF_SELECTED;
	}
}
