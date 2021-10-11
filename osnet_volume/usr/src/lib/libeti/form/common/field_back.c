/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)field_back.c	1.5	97/09/17 SMI" /* SVr4.0 1.2 */

/*LINTLIBRARY*/

#include <sys/types.h>
#include "utility.h"

int
set_field_back(FIELD *f, chtype back)
{
	f = Field(f);

	if ((back & (chtype) A_ATTRIBUTES) != back)
		return (E_BAD_ARGUMENT);

	if (Back(f) != back) {
		Back(f) = back;
		return (_sync_attrs(f));
	}
	return (E_OK);
}

chtype
field_back(FIELD *f)
{
	return (Back(Field(f)));
}
