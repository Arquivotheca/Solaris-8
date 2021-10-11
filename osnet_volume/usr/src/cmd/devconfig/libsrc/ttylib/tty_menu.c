/*LINTLIBRARY*/
/*
 * Copyright (c) 1993 Sun Microsystems, Inc.  All Rights Reserved.
 * Sun considers its source code as an unpublished, proprietary trade secret,
 * and it is available only under strict license provisions.  This copyright
 * notice is placed here only to protect Sun in the event the source is
 * deemed a published work.  Dissassembly, decompilation, or other means of
 * reducing the object code to human readable form is prohibited by the
 * license agreement under which this code is provided to the user or company
 * in possession of this copy.
 *
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the Government
 * is subject to restrictions as set forth in subparagraph (c)(1)(ii) of the
 * Rights in Technical Data and Computer Software clause at DFARS 52.227-7013
 * and in similar clauses in the FAR and NASA FAR Supplement.
 */

#pragma	ident	"@(#)tty_menu.c 1.6 94/02/17"

#include <stdio.h>
#include <ctype.h>
#include <libintl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/bitmap.h>
#include <curses.h>
#include <term.h>
#include <sys/ttychars.h>
#include "tty_utils.h"
#include "tty_color.h"

#define	MENU_SELECT_AREA	4	/* "[x] " */
#define	MENU_PAD		4	/* scroll bar + 3 spaces */

#define	BACK_KEYS	(F_GOBACK | F_MAININDEX | F_TOPICS | F_HOWTO | F_REFER)
#define	CONT_KEYS	(F_CONTINUE | F_GOTO)

static void	show_items(WINDOW *, int, int, int, int,
		    char **, size_t, int, int, void *, char *, int);
static int	find_item(char **, size_t, int, char *);
static int	find_first(size_t, size_t, int);

int
wmenu(WINDOW	*w,
	int	starty,
	int	startx,
	int	height,
	int	width,
	Callback_proc *help_proc,	/* help callback (can be NULL) */
	void	*help_data,		/* client data argument (can be NULL) */
	Callback_proc *select_proc,	/* selection callback (can be NULL) */
	void	*select_data,		/* client data argument (can be NULL) */
	Callback_proc *deselect_proc,	/* deselection callback (can be NULL) */
	void	*deselect_data,		/* client data argument (can be NULL) */
	char	*label,			/* menu label */
	char	**items,		/* array of pointers, 1 per item */
	size_t	nitems,			/* total number of items */
	void	*selected,		/* bitmap (u_long *) or index (int *) */
	int	flags,
	u_long	keys)
{
	char	searchstr[256];	/* command completion string */
	char	**ip;		/* item pointer */
	int	first;		/* index of first displayed item in list */
	int	focus;		/* index of item with focus */
	int	nselected;	/* number of selected items, or selected item */
	int	dirty;		/* needs item redraw */
	int	ch;		/* input character */
	int	item_height;	/* actual number of rows used by items */
	int	item_width;	/* actual number of columns used by items */
	int	lab_width;	/* number of columns used by label */
	int	i;		/* counter */
	int	cury, curx;	/* cursor coordinates */
	int	item_row;	/* item start row */

	searchstr[0] = '\0';

	item_height = height;

	if (label != (char *)0) {
		item_height -= 2;
		item_row = starty + 2;
		lab_width = strlen(label);
	} else {
		item_row = starty;
		lab_width = 0;
	}

	if (item_height > nitems)
		item_height = nitems;

	ip = items;
	item_width = 0;
	for (i = 0; i < nitems; i++) {
		int len = strlen(*ip);

		if (len > item_width)
			item_width = len;

		len += MENU_SELECT_AREA;
		if (len > lab_width)
			lab_width = len;

		ip++;
	}
	item_width += MENU_PAD + MENU_SELECT_AREA;
	if (item_width > width)
		item_width = width;	/* longest item(s) will be truncated */

	if (lab_width > width)
		lab_width = width;

	focus = 0;			/* focus on first item by default */
	if (flags & M_RADIO) {
		nselected = *(int *)selected;
		if (nselected > 0)
			focus = nselected; /* selection gets focus */
		/* else focus = 0 */
	} else {
		nselected = 0;
		for (i = 0; i < nitems; i++) {
			if (BT_TEST((u_long *)selected, i)) {
				nselected++;
				if (focus == 0)
					focus = i; /* focus on 1st selection */
			}
		}
	}
	first = find_first(nitems, item_height, focus);

	if (label != (char *)0) {
		int	lab_x = startx + MENU_PAD;
		(void) mvwprintw(w, starty, lab_x, "%-*.*s",
				lab_width, lab_width, label);
		(void) wmove(w, starty + 1, lab_x);
		(void) whline(w, ACS_HLINE, lab_width);
	}

	if (flags & M_READ_ONLY) {
		show_items(w, item_row, startx, item_height, item_width,
			items, nitems, first, focus, selected, searchstr,
			flags & M_RADIO);
		return (RETURN);
	}

	dirty = 1;

	for (;;) {
		if (dirty) {
			if (nitems > item_height)
				scroll_prompts(w, item_row, startx,
					first, nitems, item_height);

			show_items(w, item_row, startx, item_height, item_width,
			    items, nitems, first, focus, selected, searchstr,
			    flags & M_RADIO);

			(void) getyx(w, cury, curx);

			dirty = 0;
		}
		(void) wmove(w, cury, curx);
		ch = wzgetch(w, keys);
parse:
		if (fwd_cmd(ch)) {
			searchstr[0] = '\0';
			if (focus < (nitems - 1)) {
				++focus;
				i = focus - first;
				if (i >= item_height)
					++first;
				dirty = 1;
			} else
				beep();	/* at the end */
		} else if (bkw_cmd(ch)) {
			searchstr[0] = '\0';
			if (focus > 0) {
				--focus;
				i = focus - first;
				if (i < 0)
					--first;
				dirty = 1;
			} else
				beep();	/* at the beginning */
		} else if (sel_cmd(ch)) {
			searchstr[0] = '\0';
			if (flags & M_RADIO) {	/* exclusive choice menu */
				if (nselected == focus) {
					/*
					 * Item currently selected; deselect it.
					 */
					if ((flags & M_RADIO_ALWAYS_ONE) == 0) {
						if (deselect_proc == NULL ||
						    (*deselect_proc)(
							    deselect_data,
							    (void *)focus)) {
							*(int *)selected =
							    nselected = -1;
							dirty = 1;
						} else
							beep();
					} else
						beep();
				} else {
					/*
					 * Item currently deselected; select it.
					 */
					if (deselect_proc == NULL ||
					    (*deselect_proc)(deselect_data,
							    (void *)focus)) {
						if (select_proc == NULL ||
						    (*select_proc)(select_data,
							    (void *)focus)) {
							*(int *)selected =
							    nselected = focus;
							dirty = 1;
						} else
							beep();
					} else
						beep();

					if (flags & M_RETURN_TO_CONTINUE) {
						ch = CONTINUE;
						break;
					}
				}
			} else {		/* non-exclusive choice menu */
				/*
				 * Item currently selected; deselect it.
				 */
				if (BT_TEST((u_long *)selected, focus)) {
					if (deselect_proc == NULL ||
					    (*deselect_proc)(deselect_data,
							    (void *)focus)) {
						BT_CLEAR(
						    (u_long *)selected, focus);
						nselected--;
						dirty = 1;
					} else
						beep();
				} else {
					/*
					 * Item currently deselected; select it.
					 */
					if (select_proc == NULL ||
					    (*select_proc)(select_data,
							    (void *)focus)) {
						BT_SET(
						    (u_long *)selected, focus);
						nselected++;
						dirty = 1;
					} else
						beep();
				}
			}
		} else if ((keys & CONT_KEYS) && is_continue(ch)) {
			if (((flags & M_RADIO) && nselected != -1) ||
			    ((flags & M_RADIO) == 0 && nselected > 0) ||
			    (flags & M_CHOICE_REQUIRED) == 0)
				break;
			else
				beep();
		} else if (((keys & F_CANCEL) && is_cancel(ch)) ||
		    ((keys & BACK_KEYS) && is_goback(ch)) ||
		    ((keys & F_EXIT) && is_exit(ch)) ||
		    ((keys & F_EXITHELP) && is_exithelp(ch))) {
			break;
		} else if ((keys & F_HELP) && is_help(ch)) {
			if (help_proc != (Callback_proc *)0)
				(void) (*help_proc)(help_data, (void *)0);
			else
				beep();
		} else if (ch == killchar()) {
			searchstr[0] = '\0';
			dirty = 1;
		} else if (ch == erasechar()) {
			if (searchstr[0] != '\0') {
				searchstr[strlen(searchstr) - 1] = '\0';
				dirty = 1;
			} else
				beep();
		} else if (ch != ESCAPE) {
			u_char	c;
			int	pos = strlen(searchstr);

			c = (u_char)tolower(ch);
			searchstr[pos] = c;
			searchstr[pos + 1] = '\0';
			i = find_item(items, nitems, focus, searchstr);
			if (i != -1) {
				focus = i;
				if (i < first || i >= (first + item_height))
					first = find_first(
					    nitems, item_height, focus);
				dirty = 1;
			} else if (c == 'x' || c == ' ') { /* blecch... */
				ch = RETURN;
				goto parse;
			} else {
				searchstr[pos] = '\0';	/* erase */
				beep();
			}
		}
	}
	return (ch);
}

static void
show_items(WINDOW	*w,
	int		r,
	int		c,
	int		height,
	int		width,
	char		**items,
	size_t		nitems,
	int		first,
	int		focus,
	void		*selected,
	char		*search,
	int		is_radio)
{
	register int	i, row;
	char		**ip;
	char		cursorbuf[16];
	int		last;
	int		cury, curx, x;
	int		item_width = width - MENU_PAD - MENU_SELECT_AREA;

	row = 0;

	last = first + height;
	if (last > nitems)
		last = nitems;
	last -= 1;

	ip = &items[first];
	for (i = first; i <= last; i++) {
		if (is_radio)
			(void) sprintf(cursorbuf, "[%s]",
				(*(int *)selected == i) ? "X" : " ");
		else
			(void) sprintf(cursorbuf, "[%s]",
				BT_TEST((u_long *)selected, i) ? "X" : " ");

		x = c + MENU_PAD;

		if ((focus == i) && search && (*search == '\0')) {

			cury = r + row;
			curx = x;
			(void) wfocus_on(w, r + row, x, cursorbuf);

		} else {

			if (focus == i) {
				(void) wcolor_on(w, CURSOR);
				cury = r + row;
				curx = x + (MENU_SELECT_AREA - 1) +
							strlen(search);
			}

			(void) mvwprintw(w, r + row, x, "%s", cursorbuf);
		}

		wcolor_off(w, CURSOR);
		x += MENU_SELECT_AREA;
		(void) mvwprintw(w, r + row, x, "%-*.*s",
			item_width, item_width, *ip);
		row++;
		ip++;
	}
	(void) wmove(w, cury, curx);
	(void) wnoutrefresh(w);
}

static int
find_item(char **items, size_t nitems, int focus, char *search)
{
	int	check;
	int	i;

	check = focus;
	for (i = 0; i < nitems; i++) {
		if (strncasecmp(search, items[check], strlen(search)) == 0)
			break;
		if (++check >= nitems)
			check = 0;
	}
	return (i < nitems ? check : -1);
}

static int
find_first(size_t nitems, size_t items_per_page, int focus)
{
	int	first;
	int	i;

	if (focus != 0) {
		for (i = nitems; i > 0; i -= items_per_page) {
			if (focus >= (i - items_per_page))
				break;
		}
		first = i - items_per_page;
		if (first < 0)
			first = 0;
	} else
		first = 0;

	return (first);
}
