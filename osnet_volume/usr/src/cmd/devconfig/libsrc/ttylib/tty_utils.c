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

#pragma	ident	"@(#)tty_utils.c 1.11 94/02/17"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libintl.h>
#include <ctype.h>
#include <stdarg.h>
#include "tty_utils.h"
#include "tty_color.h"

int	curses_on = FALSE;

static int	erasech;
static int	killch;

extern char	**format_text(char *, int);
extern char	*xstrdup(char *);

extern void	wfooter_init(int);	/* XXX */

int
start_curses(void)
{
	if (curses_on == TRUE)
		return (0);

	if (initscr()) {
		/*
		 * init color stuff.
		 */
		if (start_color() == OK)
			wcolor_init();

		(void) cbreak();
		(void) noecho();
		(void) leaveok(stdscr, FALSE);
		(void) scrollok(stdscr, FALSE);
		(void) keypad(stdscr, TRUE);
		(void) typeahead(-1);

		erasech = erasechar();
		killch = killchar();

		curses_on = TRUE;
		return (0);
	}
	return (-1);
}

void
end_curses(int do_clear, int top)
{
	if (curses_on == FALSE)
		return;

	if (has_colors() != 0)
		wcolor_set_bkgd(stdscr, NORMAL);

	if (do_clear != 0)
		(void) clear();

	(void) nocbreak();
	(void) echo();

	if (top != 0)
		(void) move(stdscr->_maxy, 0);
	else
		(void) wmove(stdscr, 0, 0);

	(void) refresh();
	(void) endwin();
	(void) fflush(stdout);
	(void) fflush(stderr);

	curses_on = FALSE;
}

int
wzgetch(WINDOW *w, u_long fkeys)
{
	static	int	esc_mode = 0;
	int	ready = 0;
	int	c = 0;
	int	x, y;

	if (curses_on == TRUE) {
		(void) getsyx(y, x);

		(void) wnoutrefresh(w);

		while (!ready) {
			(void) doupdate();
			c = getch();

			if (c == '\014') {
				(void) touchwin(w);
				(void) wnoutrefresh(w);
				(void) setsyx(y, x);
			} else if (c == ESCAPE) {
				if (!esc_mode) {
					wfooter_init(1);
					/* notimeout(w, TRUE); */
					wfooter(w, fkeys); /* XXX  w == ??? */
					(void) setsyx(y, x);
					esc_mode = 1;
				}
				/* else just consume the ESCAPE */
			} else if (esc_mode) {
				switch (c) {
				case '1':
					c = KEY_F(1);
					break;
				case '2':
					c = KEY_F(2);
					break;
				case '3':
					c = KEY_F(3);
					break;
				case '4':
					c = KEY_F(4);
					break;
				case '5':
					c = KEY_F(5);
					break;
				case '6':
					c = KEY_F(6);
					break;
				case '7':
					c = KEY_F(7);
					break;
				case '8':
					c = KEY_F(8);
					break;
				case '9':
					c = KEY_F(9);
					break;
				case 'f':	/* turn off escape mode */
				case 'F':
					c = ESCAPE;	/* ??? */
					wfooter_init(0);
					wfooter(w, fkeys);
					(void) setsyx(y, x);
					break;
				default:
					/* By default, don't change "c" */
					break;
				}
				/* notimeout(w, FALSE); */
				esc_mode = 0;
				ready = 1;
			} else
				ready = 1;
		}
		(void) setsyx(y, x);
	} else {

		(void) fflush(stdout);
		(void) fflush(stderr);

		c = getchar();
	}

	return (c);
}

/*
 * mvwgets -- move to specified spot in specified window and retrieve
 *	a line of input.  Like gets, mvwgets throws away the trailing
 *	newline character.  The cursor is placed following the initial
 *	value in 'buf' (buf must contain a null-terminated string).
 *	The user's erase character backspaces and the line kill character
 *	re-starts the input line.  The Escape key aborts the routine
 *	leaving the original input buffer unchanged and restoring the
 *	value on the screen to the initial contents of the buffer.
 *	The returned string is guaranteed to be null-terminated (and
 *	thus "buf" should be 1 larger than "len").
 */
/*ARGSUSED*/
int
mvwgets(WINDOW		*w,
	int		starty,
	int		startx,
	int		ncols,		/* width of input area */
	Callback_proc	*help_proc,
	void		*help_data,
	char		*buf,
	int		len,		/* NB: should be sizeof (buf) - 1 */
	int		type_to_wipe,
	u_long		keys)
{
	char	scratch[1024];	/* user input buffer */
	char	*bp;		/* buffer pointer */
	char	*sp;		/* [scrolling] buffer pointer */
	int	c;		/* current input character */
	int	count;		/* current character count */
	int	curx;		/* cursor x coordinate */
	int	done;

	if (len > sizeof (scratch))
		len = sizeof (scratch) - 1;

	if (buf != (char *)0) {
		(void) strncpy(scratch, buf, len);
		scratch[len] = '\0';
	} else
		scratch[0] = '\0';

	if (type_to_wipe) {
		bp = sp = scratch;
		curx = startx;
	} else {
		bp = &scratch[strlen(scratch)];
		if ((bp - scratch) > ncols) {
			sp = (bp - ncols) + 1;	/* one extra for '<' */
			curx = startx + strlen(sp) + 1;
		} else {
			sp = scratch;
			curx = startx + strlen(sp);
		}
	}
	count = len - (bp - scratch);

	done = 0;

	while (!done) {
		/*
		 * Output section
		 */
		wcolor_on(w, CURSOR);
		if (sp != scratch)
			(void) mvwprintw(w, starty, startx, "<%-*.*s",
				ncols - 1, ncols - 1, sp);
		else
			(void) mvwprintw(w, starty, startx, "%-*.*s",
				ncols, ncols, sp);
		wcolor_off(w, CURSOR);
		(void) wclrtoeol(w);
		(void) wmove(w, starty, curx);
		(void) wrefresh(w);

		c = wzgetch(w, keys);

		if (c == ERR) {
			done = 1;
		} else if (c == erasech || c == CTRL_H) {
			if (bp != scratch) {
				if (sp > scratch + 2)
					--sp;
				else {
					if (sp == scratch)
						--curx;
					else
						sp = scratch;
				}
				*--bp = '\0';
				++count;
			} else if (type_to_wipe && bp[0]) {
				*bp = '\0';
				curx = startx;
				count = len;
			} else
				beep();
		} else if (c == killch) {
			if (bp != scratch) {
				bp = sp = scratch;
				*bp = '\0';
				curx = startx;
				count = len;
			} else if (type_to_wipe && bp[0]) {
				*bp = '\0';
				curx = startx;
				count = len;
			} else
				beep();
		} else if ((keys & F_HELP) && is_help(c)) {
			if (help_proc != (Callback_proc *)0)
				(void) (*help_proc)(help_data, (void *)0);
			else
				beep();
#ifdef notdef
		} else if (is_reset(c)) {
			(void) strncpy(scratch, buf, len);
			scratch[len] = '\0';
			bp = sp = scratch;
			count = len;
			curx = startx;	/* type to wipe */
#endif
		} else if (((keys & F_CONTINUE) && is_continue(c)) ||
		    fwd_cmd(c) || bkw_cmd(c) || sel_cmd(c)) {
			if (buf != (char *)0) {
				(void) strncpy(buf, scratch, len);
				buf[len] = '\0';
			}
			done = 1;
		} else if (((keys & F_CANCEL) && is_cancel(c)) ||
		    ((keys & F_CHANGE) && is_change(c))) {
			done = 1;
		} else if (!is_escape(c) && !nav_cmd(c) && buf != (char *)0) {
			if (--count < 0) {
				count = 0;
				beep();
			} else if (isspace(c) && bp == scratch) {
				beep();
			} else {
				*bp++ = (char)c;
				*bp = '\0';
				if ((curx + 1) > startx + ncols) {
					if (sp == scratch)
						sp = scratch + 2;
					else
						sp++;
				} else
					curx++;	/* advance cursor */
			}
		} else if (!esc_cmd(c))
			beep();
	}
	if (sp != scratch)
		(void) mvwprintw(w, starty, startx, "<%-*.*s",
			ncols - 1, ncols - 1, sp);
	else
		(void) mvwprintw(w, starty, startx, "%-*.*s",
			ncols, ncols, sp);
	return (c);
}

/*
 * mvwfmtw -- like mvwprintw, except that is neatly formats
 *	blocks of output by wrapping lines at the end of
 *	the display and using the x value as the righ-hand
 *	margain.  Returns the number of lines output.
 *
 *	Note:  like printf and mvwprintw, this routine does
 *	not flush output or refresh the screen.
 */
int
mvwfmtw(WINDOW *w, int row, int col, int width, char *fmt, ...)
{
	va_list	ap;
	int	n;

	if (fmt == (char *)0)
		return (0);

	va_start(ap, fmt);
	n = vmvwfmtw(w, row, col, width, fmt, ap);
	va_end(ap);

	return (n);
}


int
vmvwfmtw(WINDOW *w, int row, int col, int width, char *fmt, va_list ap)
{
	char	buf[BUFSIZ];
	char	**msg_array, **p;
	int	n, y;

	if (curses_on)
		(void) wmove(w, row, col);

	if (fmt == (char *)0)
		return (0);

	(void) vsprintf(buf, fmt, ap);

	msg_array = format_text(buf, width);

	for (p = msg_array, y = row, n = 0; *p != (char *)0; p++, y++, n++) {
		if (curses_on) {
			(void) mvwprintw(w, y, col, "%s", *p);
			(void) wclrtoeol(w);
		} else
			(void) printf("\n%s", *p);
	}

	free(msg_array[0]);
	free(msg_array);

	return (n);
}

void
werror(WINDOW *w, int row, int col, int width, char *fmt, ...)
{
	va_list ap;
	int n;

	if (curses_on == TRUE) {
		(void) wclear(w);
		(void) wrefresh(w);
	}

	if (fmt != (char *)0) {
		va_start(ap, fmt);
		n = vmvwfmtw(w, row, col, width, fmt, ap) + 2;
		va_end(ap);
	} else
		n = 0;

	(void) mvwfmtw(w, row + n, col, width,
				gettext("Press Return to continue"));

	while (wzgetch(w, 0) != '\n')
		;

	if (curses_on == TRUE) {
		(void) wclear(w);
		(void) wrefresh(w);
	}
}

void
scroll_prompts(WINDOW *w,	/* window to scroll prompts in */
	int	top,		/* top row of menu */
	int	col,		/* column to scroll propmts in */
	int	scr,		/* scrolled? index of item in top row */
	int	max,		/* total number of items */
	int	npp)   		/* n items possible per page */
{
	int	last = top + npp - 1;

	/* draw `cable' and anchors */
	if (max > npp) {
		(void) wmove(w, top + 1, col);
		(void) wvline(w, ACS_VLINE, last - top - 1);

		if (scr) {
			(void) mvwaddch(w, top, col, ACS_UARROW /* '^' */);
		} else {
			(void) mvwaddch(w, top, col, '-');
		}

		if ((scr + npp + 1) <= max) {
			(void) mvwaddch(w, last, col, ACS_DARROW /* 'v' */);
		} else {
			(void) mvwaddch(w, last, col, '-');
		}
	}
	(void) wnoutrefresh(w);
}

void
flush_input(void)
{
	/*
	 * try to flush any pending input
	 */
	(void) tcflush(0, TCIFLUSH);
	if (curses_on)
		(void) flushinp();
}

void
wheader(WINDOW *w, const char *title)
{
	if (has_colors() == 0) {
		(void) wmove(w, 0, 0);
		(void) whline(w, ACS_HLINE, 1);

		(void) mvwprintw(w, 0, 1, " %-.*s ", COLS - 3, title);

		(void) whline(w, ACS_HLINE, COLS - strlen(title) - 3);
	} else {
		wcolor_on(w, TITLE);
		(void) mvwprintw(w, 0, 0, "  %-*.*s ",
					COLS - 3, COLS - 3, title);
		wcolor_off(w, TITLE);
	}
	(void) wnoutrefresh(w);
}

typedef struct f_key Fkey;
struct f_key {
	char	*f_keycap;
	char	*f_special;
	char	*f_fallback;
	char	*f_func;	/* dynamically initialized */
	char	*f_label;	/* dynamically initialized */
};

/*
 * Maybe make this static to wfooter?
 *
 * Note:  for i18n, the "Description" strings must be encapsulated
 * in calls to gettext() somewhere else in the program in order for
 * xgettext to automatically extract them.
 *
 * Entries are grouped by function key with groups in presentation order.
 * Entries within a group are obviously mutually exclusive...
 */
static Fkey	f_keys[] = {
/*    Terminfo DB	Special Name	Fallback	Label */
	{ "kf2",	"F2",		"Esc-2" },	/* OK */
	{ "kf2",	"F2",		"Esc-2" },	/* Spare1 */
	{ "kf2",	"F2",		"Esc-2" },	/* Spare2 */
	{ "kf2",	"F2",		"Esc-2" },	/* Continue */
	{ "kf2",	"F2",		"Esc-2" },	/* Begin */
	{ "kf2",	"F2",		"Esc-2" },	/* Exit Install */
	{ "kf2",	"F2",		"Esc-2" },	/* Upgrade System */
	{ "kf3",	"F3",		"Esc-3" },	/* Go Back */
	{ "kf3",	"F3",		"Esc-3" },	/* Test Mount */
	{ "kf3",	"F3",		"Esc-3" },	/* Delete */
	{ "kf4",	"F4",		"Esc-4" },	/* Preserve */
	{ "kf4",	"F4",		"Esc-4" },	/* Bypass */
	{ "kf4",	"F4",		"Esc-4" },	/* Show Exports */
	{ "kf4",	"F4",		"Esc-4" },	/* Change */
	{ "kf4",	"F4",		"Esc-4" },	/* Change Type */
	{ "kf4",	"F4",		"Esc-4" },	/* Remote Mounts */
	{ "kf4",	"F4",		"Esc-4" },	/* Customize */
	{ "kf4",	"F4",		"Esc-4" },	/* Edit */
	{ "kf4",	"F4",		"Esc-4" },	/* Create */
	{ "kf4",	"F4",		"Esc-4" },	/* More */
	{ "kf4",	"F4",		"Esc-4" },	/* New Install */
	{ "kf5",	"F5",		"Esc-5" },	/* Add New */
	{ "kf5",	"F5",		"Esc-5" },	/* Spare4 */
	{ "kf5",	"F5",		"Esc-5" },	/* Cancel */
	{ "kf5",	"F5",		"Esc-5" },	/* Exit */
	{ "kf6",	"F6",		"Esc-6" },	/* Help */
	{ "kf2",	"F2",		"Esc-2" },	/* Go To */
	{ "kf3",	"F3",		"Esc-3" },	/* Top Level */
	{ "kf3",	"F3",		"Esc-3" },	/* Topics */
	{ "kf3",	"F3",		"Esc-3" },	/* Subjects */
	{ "kf3",	"F3",		"Esc-3" },	/* How To */
	{ "kf5",	"F5",		"Esc-5" },	/* Exit Help */
};
#define	MAXFUNC		(sizeof (f_keys) / sizeof (Fkey))

static int
index(u_long bits)
{
	u_long	b;
	int	i;

	if (bits == 0LL)		/* sanity check */
		return (0);
	b = 1;
	for (i = 0; i < 32; i++) {
		if (b == bits)
			break;
		b <<= 1;
	}
	return (i);
}

void
wfooter_init(int force_alternates)
{
	char		keystr[256];
	int		i;

	/*
	 * Ugly, but the only semi-automated way
	 * of getting i18n'ed strings into the table
	 * such that xgettext can extract them.
	 */
	f_keys[index(F_OKEYDOKEY)].f_func = DESC_F_OKEYDOKEY;
	f_keys[index(F_SPARE1)].f_func = DESC_F_SPARE1;
	f_keys[index(F_SPARE2)].f_func = DESC_F_SPARE2;
	f_keys[index(F_CONTINUE)].f_func = DESC_F_CONTINUE;
	f_keys[index(F_BEGIN)].f_func = DESC_F_BEGIN;
	f_keys[index(F_EXITINSTALL)].f_func = DESC_F_EXITINSTALL;
	f_keys[index(F_UPGRADE)].f_func = DESC_F_UPGRADE;
	f_keys[index(F_GOBACK)].f_func = DESC_F_GOBACK;
	f_keys[index(F_TESTMOUNT)].f_func = DESC_F_TESTMOUNT;
	f_keys[index(F_PRESERVE)].f_func = DESC_F_PRESERVE;
	f_keys[index(F_BYPASS)].f_func = DESC_F_BYPASS;
	f_keys[index(F_SHOWEXPORTS)].f_func = DESC_F_SHOWEXPORTS;
	f_keys[index(F_CHANGE)].f_func = DESC_F_CHANGE;
	f_keys[index(F_CHANGETYPE)].f_func = DESC_F_CHANGETYPE;
	f_keys[index(F_DOREMOTES)].f_func = DESC_F_DOREMOTES;
	f_keys[index(F_CUSTOMIZE)].f_func = DESC_F_CUSTOMIZE;
	f_keys[index(F_EDIT)].f_func = DESC_F_EDIT;
	f_keys[index(F_CREATE)].f_func = DESC_F_CREATE;
	f_keys[index(F_MORE)].f_func = DESC_F_MORE;
	f_keys[index(F_INSTALL)].f_func = DESC_F_INSTALL;
	f_keys[index(F_ADDNEW)].f_func = DESC_F_ADDNEW;
	f_keys[index(F_DELETE)].f_func = DESC_F_DELETE;
	f_keys[index(F_SPARE4)].f_func = DESC_F_SPARE4;
	f_keys[index(F_CANCEL)].f_func = DESC_F_CANCEL;
	f_keys[index(F_EXIT)].f_func = DESC_F_EXIT;
	f_keys[index(F_HELP)].f_func = DESC_F_HELP;

	f_keys[index(F_GOTO)].f_func = DESC_F_GOTO;
	f_keys[index(F_MAININDEX)].f_func = DESC_F_MAININDEX;
	f_keys[index(F_TOPICS)].f_func = DESC_F_TOPICS;
	f_keys[index(F_REFER)].f_func = DESC_F_REFER;
	f_keys[index(F_HOWTO)].f_func = DESC_F_HOWTO;
	f_keys[index(F_EXITHELP)].f_func = DESC_F_EXITHELP;

	/*
	 * Does terminal have all the
	 * appropriate "special" keys?
	 */
	for (i = 0; i < MAXFUNC; i++) {
		if (!force_alternates && tigetstr(f_keys[i].f_keycap)) {
			/* XXX need to gettext f_special */
			(void) sprintf(keystr, "%s_%s",
				f_keys[i].f_special,
				f_keys[i].f_func);
		} else {
			/* XXX need to gettext f_fallback */
			(void) sprintf(keystr, "%s_%s",
				f_keys[i].f_fallback,
				f_keys[i].f_func);
		}
		f_keys[i].f_label = xstrdup(keystr);
	}
}

void
wfooter(WINDOW *w, u_long which_keys)
{
	static int	doinit = 1;
	char		keystr[256];
	int		y = LINES - 2;
	int		i, len, s;

	if (doinit) {
		wfooter_init(0);
		doinit = 0;
	}

	(void) strcpy(keystr, "");
	len = 0;
	for (i = 0; i < MAXFUNC; i++) {
		if ((which_keys & (1 << i)) == 0)
			continue;
		s = strlen(f_keys[i].f_label) + 4;
		if ((len + s) > COLS)
			break;
		(void) sprintf(&keystr[len], "    %s", f_keys[i].f_label);
		len += s;
	}
	(void) wmove(w, y++, 0);

	if (has_colors() == 0)
		(void) whline(w, ACS_HLINE, COLS);
	else
		(void) wclrtoeol(w);

	wcolor_on(w, FOOTER);
	(void) mvwprintw(w, y, 0, "%-*.*s", COLS, COLS, keystr);
	(void) wredrawln(w, y, 1);
	wcolor_off(w, FOOTER);

	(void) wnoutrefresh(w);
}

/*
 * uses reverse-reverse-video to hide block cursor on mono screens
 *
 * by default, hides the cursor at (LINES - 1, 0)
 */
void
wcursor_hide(WINDOW *w)
{

	if (has_colors() == 0) {
		wcolor_on(w, CURSOR);
		(void) mvwprintw(w, LINES - 1, 0, " ");
		wcolor_off(w, CURSOR);
	}
	(void) wmove(w, LINES - 1, 0);
	(void) setsyx(LINES - 1, 0);

}
