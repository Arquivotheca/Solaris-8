/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)field_just.c	1.5	97/09/17 SMI" /* SVr4.0 1.1 */

/*LINTLIBRARY*/

#include <sys/types.h>
#include "utility.h"

int
set_field_just(FIELD *f, int just)
{
	if (just != NO_JUSTIFICATION &&	just != JUSTIFY_LEFT &&
	    just != JUSTIFY_CENTER &&just != JUSTIFY_RIGHT)
		return (E_BAD_ARGUMENT);

	f = Field(f);

	if (Just(f) != just) {
		Just(f) = just;
		return (_sync_attrs(f));
	}
	return (E_OK);
}

int
field_just(FIELD *f)
{
	return (Just(Field(f)));
}
