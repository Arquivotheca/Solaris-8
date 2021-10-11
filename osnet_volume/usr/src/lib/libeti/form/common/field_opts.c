/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)field_opts.c	1.5	97/09/17 SMI" /* SVr4.0 1.2 */

/*LINTLIBRARY*/

#include <sys/types.h>
#include "utility.h"

int
set_field_opts(FIELD *f, OPTIONS opts)
{
	return (_sync_opts(Field(f), opts));
}

OPTIONS
field_opts(FIELD *f)
{
	return (Field(f) -> opts);
}

int
field_opts_on(FIELD *f, OPTIONS opts)
{
	FIELD *x = Field(f);
	return (_sync_opts(x, x->opts | opts));
}


int
field_opts_off(FIELD *f, OPTIONS opts)
{
	FIELD *x = Field(f);
	return (_sync_opts(x, x->opts & ~opts));
}
