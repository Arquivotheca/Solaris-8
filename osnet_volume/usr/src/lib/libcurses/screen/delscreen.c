/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)delscreen.c	1.8	97/06/25 SMI"
		/* SVr4.0 1.4.1.1	*/
/*LINTLIBRARY*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	"curses_inc.h"

void
delscreen(SCREEN *screen)
{
#ifdef	DEBUG
	if (outf)
		fprintf(outf, "delscreen: screen %x\n", screen);
#endif	/* DEBUG */
	/*
	* All these variables are tested first because we may be called
	* by newscreen which hasn't yet allocated them.
	*/
	if (screen->tcap)
		(void) delterm(screen->tcap);
	if (screen->cur_scr)
		(void) delwin(screen->cur_scr);
	if (screen->std_scr)
		(void) delwin(screen->std_scr);
	if (screen->virt_scr)
		(void) delwin(screen->virt_scr);
	if (screen->slk) {
		if (screen->slk->_win)
			(void) delwin(screen->slk->_win);
		free(screen->slk);
	}
	if (screen->_mks) {
		if (*screen->_mks)
			free(*screen->_mks);
		free((char *) screen->_mks);
	}
	if (screen->cur_hash)
		free((char *) screen->cur_hash);
	free((char *) screen);
}
