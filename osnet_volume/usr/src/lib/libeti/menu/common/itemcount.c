/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)itemcount.c	1.4	97/07/09 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

int
item_count(MENU *m)
{
	if (m) {
		return (Nitems(m));
	}
	return (-1);
}
