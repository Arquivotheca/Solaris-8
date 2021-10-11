/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)erase.c	1.7	97/09/25 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

#include "con.h"

void
erase(void)
{
	int i;
	for (i = 0; i < 11 * (VERTRESP / VERTRES); i++)
		spew(DOWN);
}
