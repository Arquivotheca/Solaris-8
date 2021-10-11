/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * main.c -- main routines & menus for x86 boot config program
 */

#ident	"@(#)main.c	1.81	99/06/02 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>
#include "types.h"
#include <sys/stat.h>

#include "menu.h"
#include "boot.h"
#include "biosprim.h"
#include "boards.h"
#include "bop.h"
#include "befinst.h"
#include "cfname.h"
#include "debug.h"
#include "devdb.h"
#include "dir.h"
#include "enum.h"
#include "eprintf.h"
#include "escd.h"
#include "err.h"
#include "gettext.h"
#include "kbd.h"
#include "open.h"
#include "main.h"
#include "menu.h"
#include "probe.h"
#include "tty.h"
#include "adv.h"
#include "prop.h"
#include "bios.h"

extern void ask_for_itu(void);
extern void output_drvinfo(void);

/*
 * flags set by command-line arguments
 */
int Floppy;	/* -f we are running of the floppy */
char *Script;	/* -p<script> (keystroke script file) */
int Nomenu;	/* -n non-interactive mode */

static struct menu_options config_opts[] = {
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Back" },
	{ FKEY(4), MA_RETURN, "Device Tasks" },
};
#define	NCONFIG_OPTS (sizeof (config_opts) / sizeof (struct menu_options))

#ifdef	__lint
void
do_lintcalls(void)
{
	/*
	 * All of these routines are reference in data structures only.
	 * For some reason lint can't seem to figure this out and complains
	 * that the routines are defined but never used. I'm making the
	 * refernces here to shut lint up.
	 */

	extern void do_config();

	save_cfname();
	delete_cfname();
	auto_boot_cfname();
	do_config();
	ask_kbd();
	do_selected_probe();
	menu_prop();
}
#endif

static struct menu_options Probe_all_opts[] = {
	/* Function key list for the main "probe" screen */

	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Specific Scan" },
	{ FKEY(4), MA_RETURN, "Add Driver"},
	{ '\n', MA_RETURN, 0 },
};

#define	NPROBE_ALL_OPTIONS (sizeof (Probe_all_opts) / sizeof (*Probe_all_opts))

/* max isa (isa.0-99) & plat befs for partial scan */
#define	PSCAN_BEF_MAX	30

/*
 * main -- main routine, initialize everything, parse args, and proceed
 */

void
main(int argc, char *argv[])
{
	struct menu_list *config_list;
	int nconfig_list, x, partial = 0;
	u_char *altbootpath;
	u_char altboot_path_buffer[120];
	int r;
	int again;

#ifdef	__lint
	do_lintcalls();
#endif

	init_main(argc, argv);

	if (Autoboot) {
		iprintf_tty("\n%s\n%s...\n",
		    gettext("Initializing system"),
		    gettext("Please wait"));
		run_enum(ENUM_ALL);

		auto_boot();

		altbootpath = read_prop("altbootpath", NULL);

		if (altbootpath && altbootpath[0]) {

			iprintf_tty("\n%s...\n",
			    gettext("Boot failed, trying alternate bootpath"));

			(void) sprintf(altboot_path_buffer,
				"setprop bootpath %s\n", altbootpath);
			out_bop("dev /options\n");
			out_bop(altboot_path_buffer);

			auto_boot();
		}
		/*
		 * XXX Workaround for DiskSuite customers with mirrored root:
		 * If non-interactive mode is selected, failing here
		 * will allow boot.rc to retry bootconf.exe invocation
		 * with a different bootpath.
		 */
		if (Nomenu)
			done(0);
		Autoboot = 0;
	}

	/*
	 * The flow through the menus is easiest handled using labels.
	 * Using loops is considerably more messy.
	 */

	if (!No_cfname) {
		run_enum(ENUM_ALL);
		restore_plat_props();
		goto boot;
	}

intro:
	reset_plat_props();
	partial = 0;
	/*
	 * Display the intro screen and ask the user whether he wants
	 * to probe the world.
	 */

	do {
		again = 0;
		r = text_menu("MENU_HELP_PROBE_ALL", Probe_all_opts,
		NPROBE_ALL_OPTIONS, NULL, "MENU_PROBE_ALL");

		switch (r) {

		case FKEY(4):
			status_menu(Please_wait, "MENU_ENUM_BUS");
			run_enum(ENUM_ALL);
			ask_for_itu();
			break;
		case FKEY(3):
			partial = 1;
			if (do_selected_probe() == 0) {
				/*
				 * operation cancelled, let user
				 * pick again
				 */
				partial = 0;
				again = 1;
			}
			break;
		case FKEY(2):
		case '\n':
			status_menu(Please_wait, "MENU_ENUM_BUS");
			run_enum(ENUM_ALL);
			do_all_probe();
		}
	} while (r == FKEY(4) || again);
	output_drvinfo();

devlist:
	/*
	 * produce the abbreviated version of the device list
	 */
	assign_prog_probe();
	menu_list_boards(&config_list, &nconfig_list, 0);
	for (x = 0; x < nconfig_list; x++) {
		config_list[x].flags |= MF_UNSELABLE;
	}

	switch (select_menu("MENU_IDENT_HELP", config_opts, NCONFIG_OPTS,
	    config_list, nconfig_list, MS_ZERO_ONE, "MENU_IDENT_DEVICES")) {
	case FKEY(2):
	case '\n':
		free_boards_list(config_list, nconfig_list);
		save_plat_props();
		goto boot;

	case FKEY(3):
		free_boards_list(config_list, nconfig_list);
		if (partial) {
			if (do_selected_probe() == 0)
				goto intro;
			else
				goto devlist;
		} else
			goto intro;

	case FKEY(4):
		free_boards_list(config_list, nconfig_list);
		menu_adv();
		goto devlist;
	}

boot:
	/*
	 * get user to pick one of the displayed boot devices
	 */
	menu_boot();
	if (!No_cfname) {
		/*
		 * we can't handle re-reading an escd file yet so tell the
		 * user to reboot if he wants to use a different saved
		 * configuration, otherwise return him to the boot menu
		 */
		enter_menu(0, "MENU_REBOOT", 0);
	}
	goto devlist;
}

void
init_main(int argc, char *argv[])
{
	int i;
	char *ptr;

	/* it just makes life easier if we all stick to forward slashes... */
	for (ptr = argv[0]; *ptr; ptr++)
		if (*ptr == '\\')
			*ptr = '/';

	/* very simple arg parsing for now */
	for (i = 1; i < argc; i++) {
		if (strncmp(argv[i], "-d", 2) == 0)
			Debug = strtoul(&argv[i][2], NULL, 0);
		else if (strcmp(argv[i], "-f") == 0)
			Floppy++;
		else if (strcmp(argv[i], "-n") == 0)
			Nomenu++;
		else if (strncmp(argv[i], "-P", 2) == 0)
			Script = &argv[i][2];
		else if (strncmp(argv[i], "-p", 2) == 0) {
			Script = &argv[i][2];
			Screen_active = 0;
		}
	}

	debug(D_FLOW, "init_main: debug mask is %x\n", Debug);

	/* initialize generic i/o related modules (order here is IMPORTANT) */
	init_open();
	/* initialize the debug module (after the -d was processed above) */
	init_debug();
	init_gettext(argv[0]);
	init_bop();
	init_enum();
	init_biosdev();
	init_bioscmos();

	/* initialize device & config file related modules */
	init_devdb();


	/* initialise device tree modules */
	init_boot();

	/* initialize config file names */
	init_cfname();

	if (Autoboot) {
		auto_boot_timeout(); /* can clear Autoboot */
		if (!Autoboot) {
			init_cfname();
		}
	}
}

char *
strdup(const char *source)
{
	/*
	 *  Make a copy of a string:
	 *
	 *  We need our own version of this so that we can issue the MemFailure
	 *  message!
	 */

	char *cp = malloc(strlen(source)+1);
	if (!cp)
		MemFailure();
	strcpy(cp, source);
	return (cp);
}
