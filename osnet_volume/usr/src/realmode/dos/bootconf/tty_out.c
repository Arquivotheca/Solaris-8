/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * tty_out.c -- tty output handling routines
 */

#ident "@(#)tty_out.c   1.34   97/10/29 SMI"

/*
 *      this file contains tty output routines with a minimal curses-type
 *      interface.  much simpler than curses and, therefor, much smaller
 *      and viable for use in stand-alone real-mode programs run by the
 *      booting system.
 *
 *      this tty package isn't very general.  in fact, many things are
 *      hardwired in order to keep it small.  for example, the screen always
 *      displays with the top line red, the bottom line green, and the rest
 *      blue.  this simplifies the calling code and allows us to keep the
 *      program size at a minimum, but at the cost of generality.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include "types.h"
#include <biosmap.h>


#include "debug.h"
#include "err.h"
#include "eprintf.h"
#include "gettext.h"
#include "main.h"
#include "tty.h"
#include "prop.h"
#include "menu.h"
#include "cfname.h"
#include "bop.h"

int Screen_active = 1;
int done_init_tty = 0;

#define	STANDED 0x80	/* standout bit limits us to 7-bit ASCII */

/*
 * terminal escape sequences, initialized to the ANSI/x86 defaults
 */
static char *T_cd = "\033[J";		/* clear to end of display */
static char *T_cm = "\033[%d;%dH";	/* move cursor to x,y */
static char *T_title = "\033[37;41m";	/* title attributes */
static char *T_body = "\033[37;44m";	/* body attributes */
static char *T_footer = "\033[30;42m";	/* footer attributes */
static char *T_colo_so = "\033[30;46m";	/* color version of standout */
static char *T_mono_so = "\033[7m";	/* mono version of standout */
static char *T_normal = "\033[0m";	/* normal attributes */
static int T_li = 25;			/* number of lines */
static int T_co = 80;			/* number of columns */
static int T_cm_xoff = 1;		/* cm x offset */
static int T_cm_yoff = 1;		/* cm y offset */
static int T_mono = 0;			/* monochrome tty */
static int T_lastcol = 0;		/* stays in last col when written */

static char *Standout;			/* sequence to use for standout */
unsigned char *Cur_screen;		/* current screen state */
unsigned char *Ecur_screen;		/* end of Cur */
static unsigned char *Nxt;		/* next screen state */
static unsigned char *Enxt;		/* end of Nxt */
static int Cur_li;			/* current cursor line */
static int Cur_co;			/* current cursor column */
static int Nxt_li;			/* next cursor line */
static int Nxt_co;			/* next cursor column */
static int Nxt_so;			/* in standout mode */

int Bef_printfs_done_tty;		/* Flag for optimising debug */

/*
 * We have three options under DOS:
 *
 *   1.  If console I/O is being redirected to a serial line, assume
 *	 the lowest common demoninator (mono, 24 lines).
 *
 *   2.  If console is color VGA, use full color, 25 lines.
 *
 *   3.  If console is mono VGA, use mono, 25 lines.
 *
 * Line size is always 80 columns (the default).
 */
static void
setwindowsize()
{
	char *sp;

	sp = read_prop("output-device", "options");
	if (sp != NULL && strncmp(sp, "screen", 6) != 0) {
		/*
		 *  Console is redirected!  Reset window size etc.
		 */
		Standout = T_mono_so;
		T_mono = 1;
		T_li = 24;
	} else {
		if (bdap->MonoChrome) {
			Standout = T_mono_so;
			T_mono = 1;
			T_li = 25;
		} else {
			Standout = T_colo_so;
			T_mono = 0;
			T_li = 25;
		}
	}
}

/*
 * finalize -- finalize the state of the tty
 */

/*ARGSUSED0*/
static void
finalize(void *arg, int exitcode)
{
	int i;

	if (Screen_active) {
		/* fix up screen state */
		refresh_tty(0);
		printf(T_cm, 0 + T_cm_yoff, 0 + T_cm_xoff);
		printf("%s%s", T_normal, T_cd);
		printf(T_cm, T_li - 1 + T_cm_yoff, 0 + T_cm_xoff);
		printf("%s", T_normal);
		for (i = 0; i < T_co; i++)
			putc(' ', stdout);
		printf("\n\r");
		fflush(stdout);

		/* free our screen buffers */
	}
	if (Cur_screen)
		free(Cur_screen);
	done_init_tty = 0;
}

/*
 * init_tty -- initialize the tty state
 */

void
init_tty(void)
{
	if (done_init_tty) {
		return;
	}
	done_init_tty = 1;

	/* tell stdio not to buffer our input */
	setbuf(stdin, NULL);

	if (Script != 0) {
		/* In record/playback mode; open the script file */
		Script_file = fopen(Script, "r");
		if (!Script_file) fatal("%s: %!", Script);
	}

	/* arrange for finalize() to be called when program completes */
	ondoneadd(finalize, 0, CB_EVEN_FATAL);

	setwindowsize();

	/* allocate our screen buffers */
	if ((Cur_screen = malloc(T_li * T_co * 3)) == NULL) MemFailure();
	Nxt = Ecur_screen = &Cur_screen[T_li * T_co];
	Enxt = &Nxt[T_li * T_co];

	if (Screen_active) {
		/* initialize the screen state */
		printf(T_cm, 0 + T_cm_yoff, 0 + T_cm_xoff);
		printf("%s", T_normal);
		fflush(stdout);
	}
	Cur_li = Cur_co = 0;
	clear_tty();
	refresh_tty(1);

}

/*
 * clear -- clear the screen, leaving the cursor at 0,0
 */

void
clear_tty(void)
{
	lico_tty(0, 0);
	memset(Nxt, ' ', Enxt - Nxt);
	Nxt_so = 0;	/* clearing the screen takes us out of standout mode */
}

/*
 * lico -- move the cursor to given position
 *
 * the function name "lico" is meant to remind the caller that lines
 * is the first argument and columns is the second argument.
 */
void
lico_tty(int line, int column)
{
	if (line < 0)
		Nxt_li = 0;
	else if (line > T_li - 1)
		Nxt_li = T_li - 1;
	else
		Nxt_li = line;

	if (column < 0)
		Nxt_co = 0;
	else if (column > T_co - 1)
		Nxt_co = T_co - 1;
	else
		Nxt_co = column;

	Nxt_so = 0;	/* cursor motion takes us out of standout mode */
}

/*
 * standout_tty -- enter "standout" mode
 */

void
standout_tty(void)
{
	Nxt_so = STANDED;
}

/*
 * standend_tty -- end "standout" mode
 */

void
standend_tty(void)
{
	Nxt_so = 0;
}

/*
 * match_length -- return the number of chars in l1 that match l2
 */

static int
match_length(unsigned char *l1, unsigned char *l2, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (*l1++ != *l2++)
			break;

	return (i);
}

/*
 * refresh_tty -- update the screen to match our internal state
 */

void
refresh_tty(int force)
{
	int line;		/* current line we're working on */
	int nmatch;		/* number of chars matching in line */
	int needco;		/* column where we need to start updating */
	int diff;		/* distance from where we want to be */
	int i;
	unsigned char *curline;
	unsigned char *nxtline;
	char *prevattrib = NULL;
	char *attrib;
	int useic;		/* use "insert char" sequence for end of line */

	if (!Standout) {
		/* Nothing to do until we're set up! */
		return;
	}

	for (line = 0; line < T_li; line++) {
		curline = &Cur_screen[line * T_co];
		nxtline = &Nxt[line * T_co];
		if (force || (nmatch = match_length(curline, nxtline, T_co))
		    != T_co) {
			/* this line needs updating */
			needco = (force) ? 0 : nmatch;
			if (Cur_li == line) {
				/* cursor was already on correct line */
				if ((diff = (Cur_co - needco)) > 0) {
					/*
					 * cursor was to the right.
					 * if we can get close by just
					 * using a return character, do so.
					 * if we can get close with a few
					 * backspaces, use them.  otherwise,
					 * move the cursor there the hard way.
					 */
					if ((needco < 8) || (Cur_co < 8)) {
						if (Screen_active)
							putc('\r', stdout);
						Cur_co = 0;
					} else if (diff < 8) {
						for (i = 0; i < diff; i++) {
							if (Screen_active)
								putc('\b',
								    stdout);
							Cur_co--;
						}
					} else {
						if (Screen_active)
							printf(T_cm,
							    line + T_cm_yoff,
							    needco + T_cm_xoff);
						Cur_co = needco;
					}
				}
			} else {
				if (Screen_active)
					printf(T_cm, line + T_cm_yoff,
						    needco + T_cm_xoff);
				Cur_li = line;
				Cur_co = needco;
			}
			/*
			 * cursor is now on line that needs updating and is
			 * positioned somewhere to the left of the characters
			 * that need updating.
			 */

			diff = T_co - Cur_co;   /* number of chars to write */
			nxtline += Cur_co;	/* point at current position */
			curline += Cur_co;	/* point at current position */

			/*
			 * figure out how many characters at the end of the
			 * line already contain the right stuff and decrement
			 * diff appropriately.
			 */
			if (!force)
				while (curline[diff - 1] == nxtline[diff - 1])
					diff--;

			useic = 0;
			for (i = 0; i < diff; i++) {
				/* figure out attributes for this character */
				if (*nxtline & STANDED)
					attrib = Standout;
				else if (T_mono) {
					if ((Cur_li == 0) ||
					    (Cur_li == T_li - 1))
						attrib = Standout;
					else
						attrib = T_normal;
				} else if (Cur_li == 0)
					attrib = T_title;
				else if (Cur_li == T_li - 1) {
					attrib = T_footer;
					if (!T_lastcol) {
						/*
						 * can't just print the last
						 * char on the screen without
						 * causing it to scroll.  so
						 * we use "insert char" to
						 * slide it in place
						 */
						useic = 1;
					}
				} else
					attrib = T_body;
				/*
				 * attrib and prevattrib are pointers to the
				 * string we send out to get the current
				 * attributes.  if prevattrib points to the
				 * same string as attrib, then there's no
				 * need to put out new attributes.
				 */
				if (attrib != prevattrib) {
					if (Screen_active) printf("%s", attrib);
					prevattrib = attrib;
				}
				if (Screen_active && useic &&
						    (Cur_co == T_co - 2)) {
					/* ugh.  ic doesn't work on DOS! */
					putc(*nxtline & ~STANDED, stdout);
					printf("\033[7l");
					putc(*(nxtline + 1) & ~STANDED, stdout);
					printf("\033[7h");
					*curline++ = *nxtline++;
					Cur_co++;
					*curline++ = *nxtline++;
					Cur_co++;
					putc('\r', stdout);
					break;
				} else {
					/* normal case */
					if (Screen_active)
						putc(*nxtline & ~STANDED,
								    stdout);
					*curline++ =  *nxtline++;
					Cur_co++;
				}
			}

			/*
			 * if this terminal keeps the cursor sitting in the
			 * last column even after it is written, move it to
			 * the beginning of the next line, provided it would
			 * not cause scrolling.
			 */
			if (Cur_co > (T_co - 1)) {
				if (T_lastcol && Screen_active) {
					if (Cur_li >= (T_li - 1))
						putc('\r', stdout);
					else
						putc('\n', stdout);
				}
				Cur_co = 0;
				Cur_li++;
			}
		}
	}

	/* make sure cursor ends up where we want it */
	if ((Cur_li != Nxt_li) || (Cur_co != Nxt_co)) {
		if (Screen_active)
			printf(T_cm, Nxt_li + T_cm_yoff, Nxt_co + T_cm_xoff);
		Cur_li = Nxt_li;
		Cur_co = Nxt_co;
	}
	if (Screen_active) fflush(stdout);
}

/*
 * curco_tty -- return the column of the cursor position
 */

int
curco_tty(void)
{
	return (Nxt_co);
}

/*
 * curli_tty -- return the line of the cursor position
 */

int
curli_tty(void)
{
	return (Nxt_li);
}

/*
 * maxco_tty -- return the maximum possible column for this tty
 */

int
maxco_tty(void)
{
	return (T_co - 1);
}

/*
 * maxli_tty -- return the maximum possible line for this tty
 */

int
maxli_tty(void)
{
	return (T_li - 1);
}

/*
 * catcheye_tty -- display a message which will catch the users eye.
 */
void
catcheye_tty(char *p)
{
	int	x = strlen(p),
		k;
	char	fmt[77];

	for (k = 0; k < 76; fmt[k++] = '-');
	fmt[76] = 0;
	memcpy(&fmt[(76 - x) / 2], p, x);
	iprintf_tty("\n%s\n", fmt);
}

/*
 * printf_tty -- printf-style interface to this tty-handling package
 */

int
printf_tty(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	return (vprintf_tty(fmt, ap));
}

/*
 * iprintf_tty -- like printf_tty, except we refresh the screen when done.
 *		   Also, if we run off the end of the screen, put up an
 *	"enter to continue" message before clearing the screen and returning
 *	to the top.
 */

static int
iwrite_tty(void *arg, char *ptr, int len)
{
	/*
	 *  Printf callback for incremental writes:
	 *
	 *  This routine is called from "eprintf" to place the "len"-byte
	 *  string at "ptr" in the output window.  If doing so causes the
	 *  window to fill up we hold the screen, put up an "enter to continue"
	 *  message, and wait for a newline before clearing the window and
	 *  resetting the cursor to the top of the window.
	 */

	int cnt = 0;
	int x = maxli_tty();

#ifdef	__lint
	arg = 0;
#endif
	while (len--) {
		/*
		 *  Process all characters in input string ...
		 */

		if (!Nxt_so && (curli_tty() >= x)) {
			/*
			 *  Falling off the bottom of the screen.  Put up the
			 *  "<enter> to continue" message and await response.
			 */

			char *fp = (char *)gettext("Enter_Continue");
			char bottom[128];
			int n = x*T_co;

			lico_tty(x, 0);
			memcpy(bottom, &Cur_screen[n], T_co);
			memset(&Nxt[n], ' ', Enxt - &Nxt[n]);
			memcpy(&Nxt[n], fp, strlen(fp));
			refresh_tty(0);

			while (getc_tty() != '\n') beep_tty();
			memset(&Nxt[T_co], ' ', Enxt - &Nxt[T_co]);
			memcpy(&Nxt[n], bottom, T_co);
			lico_tty(1, 0);
		}

		if (putc_tty(*ptr++) == EOF) break;
		cnt += 1;
	}

	return (cnt);
}

int
viprintf_tty(const char *fmt, va_list ap)
{
	int rc = eprintf(iwrite_tty, 0, fmt, ap);
	refresh_tty(0);
	return (rc);
}

int
iprintf_tty(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	return (viprintf_tty(fmt, ap));
}

void
iputc_tty(int c)
{
	char cc = c;

	(void) iwrite_tty(0, &cc, 1);
	refresh_tty(0);
}

int
bef_print_tty(int c)
{
	debug(D_BEF_PRINT, "%c", (char)c);
	iputc_tty(c);
	/* set a flag to say a bef_printf has been done */
	Bef_printfs_done_tty = 1;
	return (c);
}

/*
 * our_write -- "write" function passed to eprintf to fill in caller's buffer
 */

static int
our_write(void *arg, char *ptr, int len)
{
	int outcount = 0;

#ifdef	__lint
	arg = 0;
#endif

	/* pass the expanded printf to puc_tty... */
	while (len--) {
		if (putc_tty(*ptr++) == EOF) break;
		outcount++;
	}

	return (outcount);
}

/*
 * vprintf_tty -- vprintf-style interface to this tty-handling package
 */

int
vprintf_tty(const char *fmt, va_list ap)
{
	return (eprintf(our_write, (void *)0, fmt, ap));
}

/*
 * putc_tty -- putc-ish interface to this tty-handling package
 */

int
putc_tty(int c)
{
	/* If we haven't set up yet, feed stuff directly to console */
	if (!Standout) {
		putc(c, stdout);
		return (c);
	}

	/* support the form feed character */
	if (c == '\f') {
		clear_tty();
		return (1);
	}

	/* ignore stuff that falls off the end of the screen */
	if (Nxt_li >= T_li)
		return (EOF);

	/* every line must start with two blanks.  do it automatically here. */
	if (Nxt_co == 0) {
		Nxt[Nxt_li * T_co + Nxt_co++] = ' ';
		Nxt[Nxt_li * T_co + Nxt_co++] = ' ';
	}

	switch (c) {
	case '\n':
		memset(&Nxt[Nxt_li * T_co + Nxt_co], ' ', T_co - Nxt_co);
		Nxt_li++;
		Nxt_co = 0;
		break;

	case '\r':
		Nxt_co = 0;
		break;

	case '\t':
		do {
			(void) putc_tty(' ');
		} while (Nxt_co && (Nxt_co % 8));
		break;

	case '\007':
		beep_tty();
		break;

	default:
		Nxt[Nxt_li * T_co + Nxt_co] = (c | Nxt_so);
		if (++Nxt_co >= T_co) {
			Nxt_co = 0;
			Nxt_li++;
		}
		break;
	}
	return (1);
}

/*
 * beep_tty -- beep the tty (happens immediately, no refresh required)
 */

void
beep_tty(void)
{
	if (Script_file) {
		/* We're not supposed to beep in playback mode! */
		fatal("%s: improper cursor movement, line %d",
							    Script,
							    Script_line);
	} else if (Screen_active) {
		putc('\007', stdout);
		fflush(stdout);
	}
}

static struct menu_options console_opts[] = {
	{ FKEY(2), MA_RETURN, "Continue" },
	{ FKEY(3), MA_RETURN, "Cancel" },
};
#define	NCONSOLE_OPTS (sizeof (console_opts) / sizeof (struct menu_options))

/*
 * The forth letter of the three properties is unique and can be used
 * to indentify which device the user selects. The forth letter of the
 * output-device can also be compared to these values to find a match
 * when setting the default device.
 *
 * If the property names ever change just switch the matching code to
 * use full string compares.
 */
#define	SET_SCREEN	'e'
#define	SET_TTYA	'a'
#define	SET_TTYB	'b'

static struct menu_list console_list[] = {
	{ "screen and keyboard", (void *)SET_SCREEN, 0, 0 },
	{ "ttya", (void *)SET_TTYA, 0, 0 },
	{ "ttyb", (void *)SET_TTYB, 0, 0 },
};
#define	NCONSOLE_LIST (sizeof (console_list) / sizeof (struct menu_list))

void
console_tty(void)
{
	struct menu_list *choice, *p;
	char *sp;
	int dev;
	char *ival, *oval;
	char buf[200];

	sp = read_prop("output-device", "options");
	if ((sp != NULL) && (strlen(sp) >= 4)) {
		dev = sp[3];
		for (p = console_list; p < &console_list[NCONSOLE_LIST]; p++) {

			/*
			 * Setup the default device to be selected for the
			 * user. Otherwise clear out those bits in case
			 * this menu as been visited more than once.
			 */
			if ((int)p->datum == dev) {
				p->flags |= MF_SELECTED | MF_CURSOR;
			} else {
				p->flags &= ~(MF_SELECTED | MF_CURSOR);
			}
		}
	}
	for (; ; ) {
		switch (select_menu("MENU_HELP_CONSOLE", console_opts,
		    NCONSOLE_OPTS, console_list, NCONSOLE_LIST, MS_ZERO_ONE,
		    "MENU_CONSOLE")) {
		case FKEY(2):
			choice = get_selection_menu(console_list,
			    NCONSOLE_LIST);
			if (choice == NULL) {
				/* user didn't pick one */
				beep_tty();
			} else {
				switch ((int)choice->datum) {
				case SET_SCREEN:
					oval = "screen";
					ival = "keyboard";
					break;

				case SET_TTYA:
					oval = "ttya";
					ival = "ttya";
					break;

				case SET_TTYB:
					oval = "ttyb";
					ival = "ttyb";
					break;
				}

				/*
				 * Give the user some indication of what
				 * happened on the original screen before
				 * the switch
				 */
				status_menu(0, "STATUS_CONSOLE_SWITCH",
				    choice->string);

				/*
				 * change the output-device now. even though
				 * store_prop will eventually call out_bop
				 * we want the menu updates to occur on the
				 * new output device asap.
				 */
				(void) sprintf(buf,
				    "setprop output-device %s\n", oval);
				out_bop(buf);

				/*
				 * Need to go though the whole reinit process
				 * so that the screen memory is freed and then
				 * reallocated. This is needed because the
				 * screen, if serial, is 24x80 whereas on the
				 * system console it's 25x80.
				 */
				finalize(0, 0);
				init_tty();

				/*
				 * Now change the properties on the disk.
				 * Only have the first store_prop() update
				 * the display.
				 */
				store_prop(Machenv_name, "input-device",
				    ival, TRUE);
				store_prop(Machenv_name, "output-device",
				    oval, FALSE);

				return;
			}
			break;

		case FKEY(3):
			return;
		}
	}
}
