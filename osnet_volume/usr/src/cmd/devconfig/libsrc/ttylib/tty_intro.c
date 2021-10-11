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

#pragma	ident	"@(#)tty_intro.c 1.1 93/11/18"

#include <unistd.h>
#include <libintl.h>

#define	EXTERNAL		/* XXX */

#ifndef EXTERNAL
#include "tty_generic.h"
#include "tty_util.h"
#else
#include "tty_utils.h"
#endif

#include "tty_help.h"

#define	PARADE_INTRO_FILE	"/tmp/.run_install_intro"

#define	PARADE_INTRO_TITLE	gettext("The Solaris Installation Program")
#define	PARADE_INTRO_TEXT	gettext(\
	"You are now interacting with the Solaris installation program.  " \
	"The program is divided into a series of short sections.  At the " \
	"end of each section, you will see a summary of the choices you've " \
	"made, and be given the opportunity to make changes.\n\n" \
	"As you work with the program, you will complete one or more of " \
	"the following tasks:\n\n" \
	"  1 - Identify peripheral devices\n" \
	"  2 - Identify your system\n" \
	"  3 - Install Solaris software\n\n" \
	"About navigation...\n\n" \
	"  - The mouse cannot be used\n\n" \
	"  - If your keyboard does not have function keys, or they do not " \
	"respond,\n    press ESC; the legend at the bottom of the screen " \
	"will change to show the ESC keys to use for navigation.")


void
wintro(Callback_proc *help_proc, void *help_data)
{
	u_long	keys;
	int	ch;

	if (unlink(PARADE_INTRO_FILE) == 0) {

		(void) start_curses();
		wclear(stdscr);

		wheader(stdscr, PARADE_INTRO_TITLE);

#ifdef EXTERNAL
		(void) mvwfmtw(stdscr, 2, 2, 75, PARADE_INTRO_TEXT);
#else
		(void) wword_wrap(stdscr, 2, 2, 75, PARADE_INTRO_TEXT);
#endif

		keys = F_CONTINUE;
		if (help_proc != (Callback_proc *)0)
			keys |= F_HELP;

		wfooter(stdscr, keys);
		wcursor_hide(stdscr);

		for (;;) {
			ch = wzgetch(stdscr, keys);

			if (is_continue(ch))
				break;

			if (is_help(ch) && help_proc != (Callback_proc *)0)
				(*help_proc)(help_data, (void *)0);
			else
				beep();
		}
	}
}
