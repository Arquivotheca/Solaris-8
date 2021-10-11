/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * menu.h -- public definitions for menu module
 */

#ifndef	_MENU_H
#define	_MENU_H

#ident "@(#)menu.h   1.15   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct menu_list {
	char *string;		/* string displayed to user */
	void *datum;		/* caller's private datum */
	int flags;
	int lines;		/* no of lines this item occupies */
};

#define	MF_SELECTED	0x01	/* list item is currently selected */
#define	MF_CURSOR	0x02	/* cursor is sitting on this item */
#define	MF_TOP		0x04	/* this item is at top of display area */
#define	MF_UNSELABLE	0x08	/* line is not selectable */

enum menu_action {
	MA_RETURN,
	MA_HELP,
	MA_DRAW,
	MA_REFRESH,
	MA_BEEP
};

struct menu_options {
	int c;
	enum menu_action action;
	char *label;
};

enum menu_input_type {
	MI_ANY,		 /* allow anything in the input field */
	MI_NUMERIC,	 /* allow only [0-9] */
	MI_ALPHANUMONLY, /* allow only [0-9][A-Z][a-z] */
	MI_READABLE	 /* MI_ALPHANUMONLY plus space not in 1st character */
};

enum menu_selection_type {
	MS_ANY,		/* allow any number of items (including zero) */
	MS_ZERO_ONE,	/* allow zero or one item to be selected */
	MS_ONE,		/* require exactly one to be selected */
	MS_ONE_MORE	/* require one or more to be selected */
};

struct sel_block {
	struct menu_list *list;		/* the list to be displayed */
	int nitems;			/* number of items in the list */
	enum menu_selection_type type;	/* type of select menu */
	int cursor;			/* cursor sits on this one */
	int top;			/* this one is at top of area */
};

extern struct sel_block *Selection_list;
extern struct menu_options Continue_options[];	/* Option list for "F2 to   */
extern int NC_options;				/* .. continue screen!	    */

void status_menu(const char *footer, const char *fmt, ...);
void enter_menu(const char *help, const char *fmt, ...);
void list_menu(const char *help, struct menu_list *list, int nitems,
    const char *fmt, ...);
void option_menu(const char *help, struct menu_options *options, int noptions);
int text_menu(const char *help, struct menu_options *options, int noptions,
    const char *text, const char *fmt, ...);
int select_menu(const char *help, struct menu_options *options, int noptions,
    struct menu_list *list, int nitems, enum menu_selection_type type,
    const char *fmt, ...);
int input_menu(const char *help, struct menu_options *options, int noptions,
    char *buffer, int buflen, enum menu_input_type type, const char *fmt, ...);
struct menu_list *get_selection_menu(struct menu_list *list, int nitems);
void clear_selection_menu(struct menu_list *list, int nitems);
void lb_init_menu(int rows);
void lb_scale_menu(int items);
void lb_info_menu(const char *fmt, ...);
void lb_inc_menu(void);
void lb_finish_menu(const char *fmt, ...);

#ifdef	__cplusplus
}
#endif

#endif	/* _MENU_H */
