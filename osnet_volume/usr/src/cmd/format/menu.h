
/*
 * Copyright (c) 1991-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MENU_H
#define	_MENU_H

#pragma ident	"@(#)menu.h	1.9	96/11/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains declarations pertaining to the menus.
 */
/*
 * This structure defines a menu entry.  It consists of a command
 * name, the function to run the command, and a function to determine
 * if the menu entry is enabled at this particular state in the program.
 * The descriptive text that appears after the command name in the menus
 * is actually part of the command name to the program.  Since
 * abbreviation is allowed for commands, the user never notices the
 * extra characters.
 */
struct menu_item {
	char	*menu_cmd;
	int	(*menu_func)();
	int	(*menu_state)();
};


/*
 *	Prototypes for ANSI C compilers
 */

char	**create_menu_list(struct menu_item *menu);
int	(*find_enabled_menu_item())(struct menu_item *menu, int item);
void	display_menu_list(char **list);
void	redisplay_menu_list(char **list);
void	run_menu(struct menu_item *, char *, char *, int);
int	true(void);
int	embedded_scsi(void);
int	not_embedded_scsi(void);
int	not_scsi(void);
int	scsi(void);
int	scsi_expert(void);
int	expert(void);
int	old_driver(void);
int	developer(void);
int	support_fdisk_on_sparc(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _MENU_H */
