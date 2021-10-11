/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)wgetch.c	1.12	97/08/22 SMI"	/* SVr4.0 1.8	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"
#ifdef	DEBUG
#include	<ctype.h>
#endif	/* DEBUG */

/*
 * This routine reads in a character from the window.
 *
 * wgetch MUST return an int, not a char, because it can return
 * things like ERR, meta characters, and function keys > 256.
 */

int
wgetch(WINDOW *win)
{
	int	inp;
	bool		weset = FALSE;

#ifdef	DEBUG
	if (outf) {
		fprintf(outf, "WGETCH: SP->fl_echoit = %c\n",
		    SP->fl_echoit ? 'T' : 'F');
		fprintf(outf, "_use_keypad %d, kp_state %d\n",
		    win->_use_keypad, SP->kp_state);
		fprintf(outf, "file %x fd %d\n", SP->input_file,
		    fileno(SP->input_file));
	}
#endif	/* DEBUG */

	if (SP->fl_echoit && cur_term->_fl_rawmode == 0) {
		(void) cbreak();
		weset++;
	}

	/* Make sure we are in proper nodelay state and not */
	/* in halfdelay state */
	if (cur_term->_delay <= 0 && cur_term->_delay != win->_delay)
		(void) ttimeout(win->_delay);

	if ((win->_flags & (_WINCHANGED | _WINMOVED)) && !(win->_flags &
	    _ISPAD))
		(void) wrefresh(win);

	if ((cur_term->_ungotten == 0) && (req_for_input)) {
		(void) tputs(req_for_input, 1, _outch);
		(void) fflush(SP->term_file);
	}
	inp = (int) tgetch((int) (win->_use_keypad ? 1 + win->_notimeout : 0));

	/* echo the key out to the screen */
	if (SP->fl_echoit && (inp < 0200) && (inp >= 0) && !(win->_flags &
	    _ISPAD))
		(void) wechochar(win, (chtype) inp);

	/*
	* Do nl() mapping. nl() affects both input and output. Since
	* we turn off input mapping of CR->NL to not affect input
	* virtualization, we do the mapping here in software.
	*/
	if (inp == '\r' && !SP->fl_nonl)
		inp = '\n';

	if (weset)
		(void) nocbreak();

	return (inp);
}
