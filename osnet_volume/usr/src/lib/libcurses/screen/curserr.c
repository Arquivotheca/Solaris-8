/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)curserr.c	1.9	97/08/22 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#include	<stdlib.h>
#include	<sys/types.h>
#include 	"curses_inc.h"
#include 	"_curs_gettext.h"

char	*curs_err_strings[4];
static int  first_curs_err_message = 0;

void
curserr(void)
{
	if (first_curs_err_message == 0) {
		first_curs_err_message = 1;
		curs_err_strings[0] =
	_curs_gettext("I don't know how to deal with your \"%s\" terminal");
		curs_err_strings[1] =
	_curs_gettext("I need to know a more specific terminal type "
	    "than \"%s\"");
		curs_err_strings[2] =
#ifdef DEBUG
		"malloc returned NULL in function \"%s\"";
#else
	_curs_gettext("malloc returned NULL");
#endif /* DEBUG */
	}

	(void) fprintf(stderr, _curs_gettext("Sorry, "));
	(void) fprintf(stderr, curs_err_strings[curs_errno], curs_parm_err);
	(void) fprintf(stderr, ".\r\n");
}
