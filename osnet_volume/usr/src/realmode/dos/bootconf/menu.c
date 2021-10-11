/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * menu.c -- routines to display menus & collect input from user
 */

#ident "@(#)menu.c   1.43   97/04/10 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "debug.h"
#include "err.h"
#include "gettext.h"
#include "help.h"
#include "main.h"
#include "menu.h"
#include "tty.h"
#include "vsprintf.h"

static int lb_inc_factor;
static int lb_inc_loc;
static int lb_start_row;
static int lb_last_size = -1;		/* smaller than any possible message */
static int Escmode;	/* labels have "Esc-n" instead of "Fn" */
void lb_inc_menu();
struct sel_block *Selection_list;

/*
 * Module function prototypes
 */
static int cmn_menu(const char *help, const char *posttext,
    struct menu_options *options, int noptions,
    int (*draw)(void *arg, int nli, int nco, int c), void *arg,
    const char *fmt, va_list ap);
static int text_draw(void *arg, int c, int nli, int nco);
static int select_draw(void *arg, int c, int nli, int nco);
static int input_draw(void *arg, int c, int nli, int nco);
/*
 * cmn_menu -- common code for all menu types
 */

#define	MAXTITLE 80

static int
cmn_menu(const char *help, const char *posttext,
    struct menu_options *options, int noptions,
    int (*draw)(void *arg, int nli, int nco, int c), void *arg,
    const char *fmt, va_list ap)
{
	int maxli;
	int maxco;
	int saveli;
	int saveco;
	int nli;
	int nco;
	int c;
	enum menu_action action;
	int i, x, n;
	int esclast = 0;
	char title[MAXTITLE];
	static char *blanks = "     ";
	char *ptr;
	int helpfits;

	if (!done_init_tty) {
		init_tty();
	}

	maxli = maxli_tty();
	maxco = maxco_tty();

	if (fmt) {
		/* print the title and body */
		fmt = gettext(fmt);
		(void) vsnprintf(title, MAXTITLE, fmt, ap);
		if (!(ptr = strchr(title, '\n'))) ptr = &title[MAXTITLE-1];
		*ptr = '\0';
	}

redraw_all:
	if (fmt) {
		/*
		 *  A null "fmt" means that this is an options-only menu so
		 *  we don't want to clear the screen!
		 */

		clear_tty();
		(void) vprintf_tty(fmt, ap);
	}

	/*
	 * print the "standard" message for this menu type
	 * the posttext may contain %s where the title should
	 * be re-iterated.
	 */
	if (posttext) {
		printf_tty("\n\n");
		printf_tty(gettext(posttext), title);
	}

	/* remember the starting point for drawing the specialized part */
	saveli = curli_tty();
	saveco = curco_tty();
	nli = maxli - saveli;
	nco = maxco - saveco - 2;

	/* draw the footer */
redraw_footer:
	for (n = i = 0, x = 2; i < noptions; i++) {
		if (options[i].label) {
			x += (strlen(gettext(options[i].label))
			    + strlen(keyname_tty(options[i].c, Escmode))
			    + 2); /* for '_' and minimum of 1 space */
			n++;
		}
	}
	/* Only add the help label if it will fit */
	helpfits = 0;
	if (help) {
		int len = strlen(gettext("Help")) +
		    strlen(keyname_tty(FKEY(6), Escmode)) + 1;

		if (x + len <= MAXTITLE) {
			n++;
			x += len;
			helpfits = 1;
		}
	}
	/* Calculate spacing between labels */
	if ((x = (n ? ((MAXTITLE - x) / n) : 1)) < 1) x = 1;
	if (x > 4) x = 4;
	lico_tty(maxli, 0);
	for (i = 0; i < noptions; i++) {
		if ((options[i].c > FKEY(6)) && helpfits) {
			helpfits = 0;
			printf_tty("%s_%s%.*s", keyname_tty(FKEY(6), Escmode),
			    gettext("Help"), x, blanks);
		}
		if (options[i].label)
			printf_tty("%s_%s%.*s", keyname_tty(options[i].c,
			    Escmode), gettext(options[i].label), x, blanks);
	}
	if (helpfits) {
		printf_tty("%s_%s%.*s", keyname_tty(FKEY(6), Escmode),
		    gettext("Help"), x, blanks);
	}
	(void) putc_tty('\n');		/* make sure rest of line is clear */

	/* draw the specialized part */
	lico_tty(saveli, saveco);
	if (draw)
		(*draw)(arg, 0, nli, nco);
	refresh_tty(0);

	while ((c = getc_tty()) != EOF) {

		/* hacks for weird CUI esc-kay behavior */
		if (esclast) {
			esclast = 0;
			if (c == 'f') {
				/* back to Fn-type labels */
				Escmode = 0;
				goto redraw_footer;
			} else if ((c >= '1') && (c <= '9'))
				c = FKEY(c - '0');
			else if (c == '<')
				c = TTYCHAR_HOME;
			else if (c == '>')
				c = TTYCHAR_END;
		}

		/* wait for user input */
		switch (c) {
		case '\022':
			action = MA_REFRESH;
			break;

		case '\033':
			Escmode = 1;
			esclast = 1;
			goto redraw_footer;

		case FKEY(1):	/* the standard dos help key */
		case FKEY(6):	/* the "standard" CUI help key */
		case '?':	/* additional help key */
			/* the standard "help" characters */
			action = MA_HELP;
			break;

		default:
			action = MA_DRAW;
			for (i = 0; i < noptions; i++)
				if (options[i].c == c)
					action = options[i].action;
			break;
		}

		switch (action) {
		case MA_REFRESH:
			/* force a redraw */
			refresh_tty(1);
			break;

		case MA_HELP:
			if (help == NULL)
				beep_tty();
			else {
				menu_help(help);
				goto redraw_all;
			}
			break;

		case MA_RETURN:
			return (c);

		case MA_DRAW:
			lico_tty(saveli, saveco);
			if (draw && (*draw)(arg, c, nli, nco))
				refresh_tty(0);
			else
				beep_tty();
			break;

		case MA_BEEP:
		default:
			beep_tty();
			break;
		}
	}
	fatal("cmn_menu: error from tty_getc");
	/*NOTREACHED*/
}

/*
 * status_menu -- display a status message in the style of a menu
 *
 * this isn't really a menu as it doesn't wait for any user input.  it is
 * just a way of displaying a status message (i.e. "please wait...") in the
 * same style as the other menus.
 */

void
status_menu(const char *footer, const char *fmt, ...)
{
	va_list ap;
	int saveli;
	int saveco;

	if (!done_init_tty) {
		init_tty();
	}

	va_start(ap, fmt);

	/* print the title and body */
	clear_tty();
	if (fmt)
		(void) vprintf_tty(gettext(fmt), ap);

	saveli = curli_tty();
	saveco = curco_tty();
	/* print the footer */
	if (footer) {
		lico_tty(maxli_tty(), 0);
		printf_tty("%s\n", gettext(footer));
	}
	lico_tty(saveli, saveco);
	refresh_tty(0);
}

/* options for an "enter" menu */
static struct menu_options Enter_options[] = {
	{ '\n',		MA_RETURN,	"Continue" },
	{ FKEY(2),	MA_RETURN,	NULL },
};

#define	NENTER_OPTIONS	(sizeof (Enter_options) / sizeof (*Enter_options))

/*
 * enter_menu -- display a message and wait for user to hit return
 *
 * no options or return value.  the user can only hit <enter> or see help.
 */

void
enter_menu(const char *help, const char *fmt, ...)
{
	va_list ap;
	ASSERT(fmt != 0);

	va_start(ap, fmt);
	(void) cmn_menu(help, "MENU_SHORTHELP_ENTER", Enter_options,
	    NENTER_OPTIONS,
	    (int (*)(void *, int, int, int))0,
	    NULL, fmt, ap);
}

/*
 * list_menu -- display a menu_list and wait for user to hit return
 *
 * no options or return value.  the user can only hit <enter> or see help.
 */

void
list_menu(const char *help, struct menu_list *list, int nitems,
    const char *fmt, ...)
{
	va_list ap;
	struct sel_block s;
	int i, nl;
	char *cp;

	ASSERT(fmt != 0);
	s.list = list;
	s.nitems = nitems;
	s.type = MS_ANY;
	s.cursor = 0;
	s.top = 0;

	/*
	 * calculate the number of lines each menu_list struct occupies
	 */
	for (i = 0; i < nitems; i++) {
		for (nl = 1, cp = list[i].string; cp = strchr(cp, '\n'); cp++) {
			nl++;
		}
		list[i].lines = nl;
	}

	va_start(ap, fmt);
	(void) cmn_menu(help, 0,  Enter_options, NENTER_OPTIONS,
	    select_draw, (void *)&s, fmt, ap);
}

/*
 * text_draw -- the "draw" callback routine for a text menu
 */

static int
/*ARGSUSED2*/
text_draw(void *arg, int c, int nli, int nco)
{
	switch (c) {
	case 0:
		if (arg)
			printf_tty("%s", (char *)arg);
		return (1);	/* return "character handled" status */

	default:
		return (0);
	}
}

/*
 * text_menu -- display a menu full of (possibly scrollable) text
 */

int
text_menu(const char *help, struct menu_options *options, int noptions,
    const char *text, const char *fmt, ...)
{
	va_list ap;
	ASSERT(fmt != 0);

	va_start(ap, fmt);
	return (cmn_menu(help, "MENU_SHORTHELP_TEXT", options, noptions,
	    text_draw, (void *)gettext(text), fmt, ap));
}

/*
 * select_draw -- the "draw" callback routine for a select menu
 */

static int
select_draw(void *arg, int c, int nli, int nco)
{
	struct sel_block *sp = (struct sel_block *)arg;
	int i, j, cursave = sp->cursor;
	int saveli;
	int saveco;
	int needsb;
	int unused;
	char *cp;
	char ch;

	switch (c) {
	case '\n':
	case ' ':
	case 'x':
	case 'X':
		if (sp->list[sp->cursor].flags & MF_UNSELABLE)
			return (0);

		/* toggle selection on this line */
		sp->list[sp->cursor].flags ^= MF_SELECTED;

		/* clear other selections if multiple selections not allowed */
		if ((sp->list[sp->cursor].flags & MF_SELECTED) &&
		    (sp->type != MS_ANY))
			for (i = 0; i < sp->nitems; i++)
				if ((sp->list[i].flags & MF_SELECTED) &&
				    (i != sp->cursor))
					sp->list[i].flags &= ~MF_SELECTED;
		goto redraw;

	case TTYCHAR_PGDOWN:
		/*
		 * Return if last item already displayed.
		 */
		for (j = 0, i = sp->top; i <= (sp->nitems - 1); i++) {
			j += sp->list[i].lines;
			if (j > nli) {
				break;
			}
		}
		if (i > (sp->nitems - 1)) {
			return (0);
		}

		/*
		 * Set sp->top
		 */
		for (j = 0, i = sp->top; j <= nli; i++) {
			j += sp->list[i].lines;
		}
		sp->top = i - 1;

		/*
		 * Check whether we can display a whole screen
		 */
		for (j = 0, i = sp->top; j <= nli; i++) {
			j += sp->list[i].lines;
			if (i == (sp->nitems - 1)) {
				break;
			}
		}
		if (j > nli) {
			sp->cursor = sp->top;
			goto redraw;
		}
		/*FALLTHROUGH*/

	case TTYCHAR_END:
		if (sp->cursor >= (sp->nitems - 1))
			return (0);
		sp->cursor = sp->nitems;

		/*
		 * Calculate the new top taking into account the newlines
		 * embedded in strings
		 */
		for (j = nli, i = (sp->cursor - 1); i >= 0; i--) {
			j -= sp->list[i].lines;
			if (j < 0) {
				break;
			}
		}
		sp->top = ++i;
		/*FALLTHROUGH*/

	case TTYCHAR_UP:
	case 'k':
		if (sp->cursor == 0) {
			sp->cursor = cursave;
			return (0);
		}
		sp->cursor--;
		if (sp->cursor < sp->top) {
			sp->top = sp->cursor;
		}
		goto redraw;

	case TTYCHAR_PGUP:
		/* Check if top already displayed */
		if (sp->top == 0) {
			return (0);
		}

		/* Check whether we can display a whole screen */
		for (j = nli, i = sp->top - 1; i; i--) {
			j -= sp->list[i].lines;
			if (j == 0) {
				sp->top = i;
				break;
			}
			if (j < 0) {
				sp->top = i + 1;
				break;
			}
		}
		if (i != 0) {
			sp->cursor = sp->top;
			goto redraw;
		}
		/*FALLTHROUGH*/

	case TTYCHAR_HOME:
		if (sp->cursor == 0)
			return (0);
		sp->cursor = sp->top = 0;
		goto redraw;

	case TTYCHAR_DOWN:
	case 'j':
		if (sp->cursor >= (sp->nitems - 1)) {
			sp->cursor = cursave;
			return (0);
		}
		sp->cursor++;
		/* Move the top if we've moved out of the window */
		for (j = nli, i = sp->top; i <= sp->cursor; i++) {
			j -= sp->list[i].lines;
			if (j < 0) {
				break;
			}
		}
		if (j < 0) {
			/* Calculate the new top */
			for (j = nli, i = sp->cursor; i >= 0; i--) {
				j -= sp->list[i].lines;
				if (j == 0) {
					sp->top = i;
					break;
				}
				if (j < 0) {
					sp->top = i + 1;
					break;
				}
			}
		}
		/* FALLTHRUOUGH */

	case 0:
	redraw:
		/* determine if we will need a scrollbar */
		for (i = 0, j = 0, needsb = 0; i < sp->nitems; i++) {
			j += sp->list[i].lines;
			if (j > nli) {
				needsb = 1;
				break;
			}
		}

		/*
		 * If the selected item isn't on the screen,
		 * then make it so
		 */
		for (j = 0, i = sp->top; i <= sp->cursor; i++) {
			j += sp->list[i].lines;
			if (j > nli) {
				break;
			}
		}
		if (j > nli) {
			/* First work forward from cursor */
			for (i = sp->cursor, j = 0; i < sp->nitems; i++) {
				j += sp->list[i].lines;
				if (j > nli) {
					break;
				}
			}
			/* Now we work backwards */
			if (j > nli) {
				sp->top = sp->cursor;
			} else {
				for (i = sp->cursor - 1; i > 0; i--) {
					j += sp->list[i].lines;
					if (j > nli) {
						sp->top = i + 1;
						break;
					}
				}
			}
		}

		/* calculate the exact number of lines used. */
		for (i = sp->top, j = 0; i < sp->nitems; i++) {
			j += sp->list[i].lines;
			if (j > nli) {
				j -= sp->list[i].lines;
				break;
			}
		}
		unused = nli - j;
		nli = j;

		/* print the list of selections */
		for (i = sp->top; i < sp->nitems; i++) {
			/*
			 * Check if next item will fit on screen.
			 */
			if (nli < sp->list[i].lines) {
				break;
			}
			nli--;

			/* draw scrollbar if required */
			if (needsb) {
				if (i == sp->top) {
					if (i == 0) {
						ch = '-';
					} else {
						ch = '^';
					}
				} else if (nli == 0) {
					if (i < (sp->nitems - 1)) {
						ch = 'v';
					} else {
						ch = '-';
					}
				} else {
					ch =  '|';
				}
			} else {
				ch = ' ';
			}
			printf_tty("%c ", ch);

			if (i == sp->cursor) {
				if ((sp->list[i].flags & MF_UNSELABLE) == 0)
					standout_tty();
				saveli = curli_tty();
				saveco = curco_tty();
			}
			if (sp->list[i].flags & MF_UNSELABLE)
				printf_tty("   ");
			else
				printf_tty("[%c]",
				    (sp->list[i].flags & MF_SELECTED) ?
				    'X' : ' ');

			if (i == sp->cursor) {
				if ((sp->list[i].flags & MF_UNSELABLE) == 0)
					standend_tty();
			}

			/* Print text */
			j = nco - 7; /* controls line truncation */
			printf_tty("  ");
			for (cp = sp->list[i].string; *cp; cp++) {
				if (*cp == '\n') {
					nli--;
					j = nco - 7;
					(void) putc_tty('\n');
					if (needsb) {
						if (nli == 0) {
							if (i <
							    (sp->nitems - 1)) {
								ch = 'v';
							} else {
								ch = '-';
							}
						} else {
							ch =  '|';
						}
					} else {
						ch = ' ';
					}
					printf_tty("%c      ", ch);
				} else if (j-- > 0) {
					(void) putc_tty(*cp);
				}
			}
			(void) putc_tty('\n');
		}
		for (i = 0; i < unused; i++) {
			(void) putc_tty('\n');
		}

		/* put cursor on current "cursor" item */
		lico_tty(saveli, saveco);

		return (1);	/* return "character handled" status */

	default:
		return (0);
	}
}

/*
 * option_menu -- change the options line on current screen & wait for reply
 */

struct menu_options Continue_options[] = {
	/*  Option list for holding screen after scrolling iprintf ..	    */

	{ FKEY(2), MA_RETURN, "Continue" },
	{ '\n', MA_RETURN, 0 },
};

int NC_options = sizeof (Continue_options)/sizeof (*Continue_options);

void
option_menu(const char *help, struct menu_options *options, int noptions)
{
	(void) cmn_menu(help, 0, options, noptions, 0, 0, 0, 0);
}

/*
 * select_menu -- display a menu full of (possibly scrollable) selections
 */

int
select_menu(const char *help, struct menu_options *options, int noptions,
    struct menu_list *list, int nitems, enum menu_selection_type type,
    const char *fmt, ...)
{
	va_list ap;
	struct sel_block s;
	int i;
	int retval;
	char *cp;
	int nl;

	ASSERT(fmt != 0);
	s.list = list;
	s.nitems = nitems;
	s.type = type;

	/*
	 * go through and find cursor and top.  default to zero.
	 * Also calculate the number of lines each menu_list struct occupies
	 */
	s.cursor = 0;
	s.top = 0;
	for (i = 0; i < nitems; i++) {
		if (list[i].flags & MF_SELECTED)
			s.cursor = i;
		if (list[i].flags & MF_TOP)
			s.top = i;
		for (nl = 1, cp = list[i].string; cp = strchr(cp, '\n'); cp++) {
			nl++;
		}
		list[i].lines = nl;
	}

	va_start(ap, fmt);
	for (;;) {
		Selection_list = &s;
		retval = cmn_menu(help, 0, options,
		    noptions, select_draw, (void *)&s, fmt, ap);
		Selection_list = 0;
		if ((type == MS_ONE) || (type == MS_ONE_MORE)) {
			for (i = 0; i < nitems; i++)
				if (list[i].flags & MF_SELECTED)
					return (retval);
			beep_tty();
		} else
			return (retval);
	}
	/*NOTREACHED*/
}

/* struct used to pass state to input_draw */
struct inp_block {
	char *buffer;
	int buflen;
	enum menu_input_type type;
	int count;
};

/*
 * input_draw -- the "draw" callback routine for a input menu
 */

static int
/*ARGSUSED2*/
input_draw(void *arg, int c, int nli, int nco)
{
	struct inp_block *ip = (struct inp_block *)arg;
	int saveli;
	int saveco;

	if (c != '\0') {
		if (c == '\025')
			ip->count = 0;
		else if ((c == TTYCHAR_LEFT) && (ip->count))
			ip->count--;
		else if (ip->count >= (ip->buflen - 1))
			return (0);
		else if ((ip->type == MI_NUMERIC) && ((c < '0') || (c > '9')))
			return (0);
		else if (((ip->type == MI_ALPHANUMONLY) ||
		    (ip->type == MI_READABLE)) &&
		    ((c < 'a') || (c > 'z')) && ((c < 'A') || (c > 'Z')) &&
		    ((c < '0') || (c > '9')) && (c != ' '))
			return (0);
		else if ((ip->type == MI_ALPHANUMONLY) && (c == ' '))
			return (0);
		else if ((ip->type == MI_READABLE) && (c == ' ') &&
		    (ip->count == 0))
			return (0);
		else if ((c >= '\177') || ((c != '\n') && (c < ' ')))
			return (0);
		else
			ip->buffer[ip->count++] = c;
	}
	ip->buffer[ip->count] = '\0';
	printf_tty("%s", ip->buffer);
	saveli = curli_tty();
	saveco = curco_tty();
	(void) putc_tty('\n');
	lico_tty(saveli, saveco);
	return (1);	/* return "character handled" status */
}

/*
 * input_menu -- display a menu with an input field
 */

int
input_menu(const char *help, struct menu_options *options, int noptions,
    char *buffer, int buflen, enum menu_input_type type, const char *fmt, ...)
{
	int x;
	va_list ap;
	struct inp_block ib;
	ASSERT(fmt != 0);

	va_start(ap, fmt);
	ib.buffer = buffer;
	ib.buflen = buflen;
	ib.type = type;
	ib.count = strlen(buffer);
	x = cmn_menu(help, NULL, options, noptions,
	    input_draw, (void *)&ib, fmt, ap);
	ib.buffer[ib.count] = '\0';
	return (x);
}

/*
 * get_selection_menu -- find the first selected item in a list
 */

struct menu_list *
get_selection_menu(struct menu_list *list, int nitems)
{
	int i;

	for (i = 0; i < nitems; i++)
		if (list[i].flags & MF_SELECTED) {
			return (&list[i]);
		}

	return (NULL);
}

void
clear_selection_menu(struct menu_list *list, int nitems)
{
	int i;

	/* clear every flag except MF_UNSELABLE */
	for (i = 0; i < nitems; i++)
		list[i].flags &= MF_UNSELABLE;
}

void
lb_init_menu(int row)
{
	lb_inc_factor = 1;
	lb_inc_loc = 15;
	lb_start_row = row;

	lico_tty(lb_start_row + 3, lb_inc_loc);
	iprintf_tty("|        |         |         |         |         |");
	lico_tty(lb_start_row + 4, lb_inc_loc);
	iprintf_tty("0       20        40        60        80       100");
}

void
lb_scale_menu(int items)
{
	int lb_left, lb_adj, new_inc_factor;

	if (!items)
		return;
	lb_left = 50 - (lb_inc_loc - 15);
	new_inc_factor = lb_left / items;
	if (new_inc_factor == 0)
		new_inc_factor = 1;
	lb_adj = lb_left - (new_inc_factor * items);
	if (lb_adj < 0)
		lb_adj = 0;
	for (; lb_adj; lb_adj--)
		lb_inc_menu();
	lb_inc_factor = new_inc_factor;
}

void
lb_info_menu(const char *fmt, ...)
{
	char buf[80];
	int n, i;
	va_list ap;

	va_start(ap, fmt);
	n = vsnprintf(buf, 80, fmt, ap);

	if (lb_last_size > n) {
		/*
		 * need to erase part or most of the last info message
		 * because the new is message is shorter.
		 */

		lico_tty(lb_start_row, 15 + n);
		for (i = 0; i < lb_last_size - n; i++)
			iputc_tty(' ');
	}

	lico_tty(lb_start_row, 15);
	iprintf_tty(buf);

	lb_last_size = n;
}

void
lb_inc_menu(void)
{
	int i;

	lico_tty(lb_start_row + 2, lb_inc_loc);
	standout_tty();
	for (i = lb_inc_factor; i; i--)
		iputc_tty(' ');
	standend_tty();
	lb_inc_loc += lb_inc_factor;
}

void
lb_finish_menu(const char *fmt, ...)
{
	va_list ap;
	char buf[80];
	int n;

	va_start(ap, fmt);

	if (fmt) {
		n = vsnprintf(buf, 80, fmt, ap);
		lico_tty(lb_start_row + 5, (80 - n) / 2);
		iprintf_tty(buf);
	}

	lico_tty(lb_start_row + 2, lb_inc_loc);
	standout_tty();
	for (; lb_inc_loc < 65; lb_inc_loc++)
		iputc_tty(' ');
	standend_tty();
}
