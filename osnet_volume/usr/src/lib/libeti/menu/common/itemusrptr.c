/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)itemusrptr.c	1.4	97/07/09 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

int
set_item_userptr(ITEM *i, char *u)
{
	if (i) {
		Iuserptr(i) = u;
	} else {
		Iuserptr(Dfl_Item) = u;
	}
	return (E_OK);
}

char *
item_userptr(ITEM *i)
{
	return (Iuserptr(i ? i : Dfl_Item));
}
