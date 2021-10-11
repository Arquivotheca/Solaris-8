
#ifndef lint
#pragma ident	"@(#)menu.c	1.16	97/11/22 SMI"
#endif	lint

/*
 * Copyright (c) 1991-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This file contains routines relating to running the menus.
 */
#include <string.h>
#include "global.h"
#include "menu.h"
#include "misc.h"

static char	cur_title[MAXPATHLEN];

/*
 * This routine takes a menu struct and concatenates the
 * command names into an array of strings describing the menu.
 * All menus have a 'quit' command at the bottom to exit the menu.
 */
char **
create_menu_list(menu)
	struct	menu_item *menu;
{
	register struct menu_item *mptr;
	register char	**cpptr;
	register char	**list;
	int		nitems;

	/*
	 * A minimum list consists of the quit command, followed
	 * by a terminating null.
	 */
	nitems = 2;
	/*
	 * Count the number of active commands in the menu and allocate
	 * space for the array of pointers.
	 */
	for (mptr = menu; mptr->menu_cmd != NULL; mptr++) {
		if ((*mptr->menu_state)())
			nitems++;
	}
	list = (char **)zalloc(nitems * sizeof (char *));
	cpptr = list;
	/*
	 * Fill in the array with the names of the menu commands.
	 */
	for (mptr = menu; mptr->menu_cmd != NULL; mptr++) {
		if ((*mptr->menu_state)()) {
			*cpptr++ = mptr->menu_cmd;
		}
	}
	/*
	 * Add the 'quit' command to the end.
	 */
	*cpptr = "quit";
	return (list);
}

/*
 * This routine takes a menu list created by the above routine and
 * prints it nicely on the screen.
 */
void
display_menu_list(list)
	char	**list;
{
	register char **str;

	for (str = list; *str != NULL; str++)
		fmt_print("        %s\n", *str);
}

/*
 * Find the "i"th enabled menu in a menu list.  This depends
 * on menu_state() returning the same status as when the
 * original list of enabled commands was constructed.
 */
int (*
find_enabled_menu_item(menu, item))()
	struct menu_item	*menu;
	int			item;
{
	struct menu_item	*mp;

	for (mp = menu; mp->menu_cmd != NULL; mp++) {
		if ((*mp->menu_state)()) {
			if (item-- == 0) {
				return (mp->menu_func);
			}
		}
	}

	return (NULL);
}

/*
 * This routine 'runs' a menu.  It repeatedly requests a command and
 * executes the command chosen.  It exits when the 'quit' command is
 * executed.
 */
/*ARGSUSED*/
void
run_menu(menu, title, prompt, display_flag)
	struct	menu_item *menu;
	char	*title;
	char	*prompt;
	int	display_flag;
{
	char		**list;
	int		i;
	struct		env env;
	u_ioparam_t	ioparam;
	int		(*f)();


	/*
	 * Create the menu list and display it.
	 */
	list = create_menu_list(menu);
	strcpy(cur_title, title);
	fmt_print("\n\n%s MENU:\n", title);
	display_menu_list(list);
	/*
	 * Save the environment so a ctrl-C out of a command lands here.
	 */
	saveenv(env);
	for (;;) {
		/*
		 * Ask the user which command they want to run.
		 */
		ioparam.io_charlist = list;
		i = input(FIO_MSTR, prompt, '>', &ioparam,
		    (int *)NULL, CMD_INPUT);
		/*
		 * If they choose 'quit', the party's over.
		 */
		if ((f = find_enabled_menu_item(menu, i)) == NULL)
			break;

		/*
		 * Mark the saved environment active so the user can now
		 * do a ctrl-C to get out of the command.
		 */
		useenv();
		/*
		 * Run the command.  If it returns an error and we are
		 * running out of a command file, the party's really over.
		 */
		if ((*f)() && option_f)
			fullabort();
		/*
		 * Mark the saved environment inactive so ctrl-C doesn't
		 * work at the menu itself.
		 */
		unuseenv();
		/*
		 * Since menu items are dynamic, some commands
		 * cause changes to occur.  Destroy the old menu,
		 * and rebuild it, so we're always up-to-date.
		 */
		destroy_data((char *)list);
		list = create_menu_list(menu);
		/*
		 * Redisplay menu, if we're returning to this one.
		 */
		if (cur_menu != last_menu) {
			last_menu = cur_menu;
			strcpy(cur_title, title);
			fmt_print("\n\n%s MENU:\n", title);
			display_menu_list(list);
		}
	}
	/*
	 * Clean up the environment stack and throw away the menu list.
	 */
	clearenv();
	destroy_data((char *)list);
}

/*
 * re-display the screen after exiting from shell escape
 *
 */
void
redisplay_menu_list(list)
char **list;
{
	fmt_print("\n\n%s MENU:\n", cur_title);
	display_menu_list(list);
}


/*
 * Glue to always return true.  Used for menu items which
 * are always enabled.
 */
int
true()
{
	return (1);
}

/*
 * Note: The following functions are used to enable the inclusion
 * of device specific options (see init_menus.c). But when we are
 * running non interactively with commands taken from a script file,
 * current disk (cur_disk, cur_type, cur_ctype) may not be defined.
 * They get defined when the script selects a disk using "disk" option
 * in the main menu. However, in the normal interactive mode, the disk
 * selection happens before entering the main menu.
 */
/*
 * Return true for menu items enabled only for embedded SCSI controllers
 */
int
embedded_scsi()
{
	if (cur_ctype == NULL && option_f)
		return (0);
	return (EMBEDDED_SCSI);
}

/*
 * Return false for menu items disabled only for embedded SCSI controllers
 */
int
not_embedded_scsi()
{
	if (cur_ctype == NULL && option_f)
		return (0);
	return (!EMBEDDED_SCSI);
}

/*
 * Return false for menu items disabled for both md21 and scsi controllers
 */
int
not_scsi()
{
	if (cur_ctype == NULL && option_f)
		return (0);
	return (!SCSI);
}


/*
 * Return true for menu items enabled for both md21 and scsi controllers
 */
int
scsi()
{
	if (cur_ctype == NULL && option_f)
		return (0);
	return (SCSI);
}


/*
 * Return true for menu items enabled if expert mode is enabled
 */
int
scsi_expert()
{
	if (cur_ctype == NULL && option_f)
		return (0);
	return (SCSI && expert_mode);
}


/*
 * Return true for menu items enabled if expert mode is enabled
 */
int
expert()
{
	return (expert_mode);
}


/*
 * Return true for menu items enabled if it's an old driver
 */
int
old_driver()
{
	if (cur_ctype == NULL && option_f)
		return (0);
	return (OLD_DRIVER);
}


/*
 * Return true for menu items enabled if developer mode is enabled
 */
int
developer()
{
	return (dev_expert);
}

/*
 * For x86, always return true for menu items enabled
 *	since fdisk is already supported on these two platforms.
 * For Sparc, only return true for menu items enabled
 *	if a PCATA disk is selected.
 */
int
support_fdisk_on_sparc()
{
#if defined(sparc)
	/*
	 * If it's a SCSI disk then we don't support fdisk and we
	 * don't need to know the type cause we can ask the disk,
	 * therefore we return true only if we *KNOW* it's an ATA
	 * disk.
	 */
	if (cur_ctype && cur_ctype->ctype_ctype == DKC_PCMCIA_ATA) {
		return (1);
	} else {
		return (0);
	}
#elif defined(i386)
	return (1);
#else
#error  No Platform defined
#endif /* defined(sparc) */

}
