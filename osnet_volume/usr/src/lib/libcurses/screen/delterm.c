/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)delterm.c	1.8	97/08/26 SMI"	/* SVr4.0 1.5.1.1	*/

/*LINTLIBRARY*/

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "curses_inc.h"

/*
 * Relinquish the storage associated with "terminal".
 */
extern	TERMINAL	_first_term;
extern	char		_called_before;
extern	char		_frst_tblstr[];

int
delterm(TERMINAL *terminal)
{
	if (!terminal)
		return (ERR);
	(void) delkeymap(terminal);
	if (terminal->_check_fd >= 0)
		(void) close(terminal->_check_fd);
	if (terminal == &_first_term) {
		/* next setupterm can re-use static areas */
		_called_before = FALSE;
		if (terminal->_strtab != _frst_tblstr)
			free((char *)terminal->_strtab);
	} else {
		free((char *)terminal->_bools);
		free((char *)terminal->_nums);
		free((char *)terminal->_strs);
		free((char *)terminal->_strtab);
		free((char *)terminal);
	}
	if (terminal->_pairs_tbl)
		free((char *) terminal->_pairs_tbl);
	if (terminal->_color_tbl)
		free((char *) terminal->_color_tbl);
	return (OK);
}
