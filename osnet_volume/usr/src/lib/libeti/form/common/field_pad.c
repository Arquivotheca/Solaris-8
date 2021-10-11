/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)field_pad.c	1.5	97/09/17 SMI" /* SVr4.0 1.1 */

/*LINTLIBRARY*/

#include <sys/types.h>
#include "utility.h"

int
set_field_pad(FIELD *f, int pad)
{
	if (!(isascii(pad) && isprint(pad)))
		return (E_BAD_ARGUMENT);

	f = Field(f);

	if (Pad(f) != pad) {
		Pad(f) = pad;
		return (_sync_attrs(f));
	}
	return (E_OK);
}

int
field_pad(FIELD *f)
{
	return (Pad(Field(f)));
}
