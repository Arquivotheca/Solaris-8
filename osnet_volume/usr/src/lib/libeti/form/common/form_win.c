/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)form_win.c	1.4	97/09/17 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "utility.h"

int
set_form_win(FORM *f, WINDOW *window)
{
	if (Status(f, POSTED))
		return (E_POSTED);

	Form(f)->win = window;
	return (E_OK);
}

WINDOW *
form_win(FORM *f)
{
	return (Form(f)->win);
}
