/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)llib-lmenu.c	1.3	97/08/13 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/
#include "menu.h"

WINDOW *win;
MENU *menu;
ITEM **items;
ITEM *item;

ITEM	**menu_items(MENU *m) {return items;}
ITEM	*current_item(MENU *m) {return (ITEM *)0;}
ITEM	*new_item(char *n, char *d) {return item;}
MENU	*new_menu(ITEM **i)  {return menu;}
OPTIONS	item_opts(ITEM *i) {return O_SELECTABLE;}
OPTIONS	menu_opts(MENU *m) {return O_ONEVALUE;}
PTF_void	item_init(MENU *m) {return (PTF_void)0;}
PTF_void	item_term(MENU *m) {return (PTF_void)0;}
PTF_void	menu_init(MENU *m) {return (PTF_void)0;}
PTF_void	menu_term(MENU *m) {return (PTF_void)0;}
WINDOW	*menu_sub(MENU *m) {return win;}
WINDOW	*menu_win(MENU *m) {return win;}

char	*item_description(ITEM *i) {return "description";}
char	*item_name(ITEM *i) {return "name";}
char	*item_userptr(ITEM *i) {return "item_userptr";}
char	*menu_mark(MENU *m) {return "-";}
char	*menu_pattern(MENU *m) {return "pattern";}
char	*menu_userptr(MENU *m) {return "menu_userptr";}

chtype	menu_back(MENU *m) {return A_NORMAL;}
chtype	menu_fore(MENU *m) {return A_NORMAL;}
chtype	menu_grey(MENU *m) {return A_NORMAL;}

void	menu_format(MENU *m, int *r, int *c) {}

int	free_item(ITEM *i) {return E_OK;}
int	free_menu(MENU *m) {return E_OK;}
int	item_count(MENU *m) {return 0;}
int	item_index(ITEM *i) {return 0;}
int	item_opts_off(ITEM *i, OPTIONS o) {return 0;}
int	item_opts_on(ITEM *i, OPTIONS o) {return 0;}
int	item_value(ITEM *i) {return 0;}
int	item_visible(ITEM *i) {return TRUE;}
int	menu_driver(MENU *m, int c) {return E_OK;}
int	menu_opts_off(MENU *m, OPTIONS o) {return 0;}
int	menu_opts_on(MENU *m, OPTIONS o) {return 0;}
int	menu_pad(MENU *m) {return ' ';}
int	pos_menu_cursor(MENU *m) {return E_OK;}
int	post_menu(MENU *m) {return E_OK;}
int	scale_menu(MENU *m, int *r, int *c) {return E_OK;}
int	set_current_item(MENU *m, ITEM *i) {return E_OK;}
int	set_item_init(MENU *m, PTF_void f) {return E_OK;}
int	set_item_opts(ITEM *i, OPTIONS o) {return E_OK;}
int	set_item_term(MENU *m, PTF_void f) {return E_OK;}
int	set_item_userptr(ITEM *i, char *u) {return E_OK;}
int	set_item_value(ITEM *i, int v) {return E_OK;}
int	set_menu_back(MENU *m, chtype a) {return E_OK;}
int	set_menu_fore(MENU *m, chtype a) {return E_OK;}
int	set_menu_format(MENU *m, int r, int c) {return E_OK;}
int	set_menu_grey(MENU *m, chtype a) {return E_OK;}
int	set_menu_init(MENU *m, PTF_void f) {return E_OK;}
int	set_menu_items(MENU *m, ITEM **i) {return E_OK;}
int	set_menu_mark(MENU *m, char *s) {return E_OK;}
int	set_menu_opts(MENU *m, OPTIONS o) {return E_OK;}
int	set_menu_pad(MENU *m, int i) {return E_OK;}
int	set_menu_pattern(MENU *m, char *p) {return E_OK;}
int	set_menu_sub(MENU *m, WINDOW *w) {return E_OK;}
int	set_menu_term(MENU *m, PTF_void f) {return E_OK;}
int	set_menu_userptr(MENU *m, char *u) {return E_OK;}
int	set_menu_win(MENU *m, WINDOW *w) {return E_OK;}
int	set_top_row(MENU *m, int i) {return E_OK;}
int	top_row(MENU *m) {return 0;}
int	unpost_menu(MENU *m) {return E_OK;}
