/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)form_opts.c	1.5	97/09/17 SMI" /* SVr4.0 1.2 */

/*LINTLIBRARY*/

#include <sys/types.h>
#include "utility.h"

int
set_form_opts(FORM *f, OPTIONS opts)
{
	Form(f)->opts = opts;
	return (E_OK);
}

OPTIONS
form_opts(FORM *f)
{
	return (Form(f) -> opts);
}

int
form_opts_on(FORM *f, OPTIONS opts)
{
	Form(f)->opts |= opts;
	return (E_OK);
}

int
form_opts_off(FORM *f, OPTIONS opts)
{
	Form(f)-> opts &= ~opts;
	return (E_OK);
}
