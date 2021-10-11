/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)menuserptr.c	1.4	97/07/09 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

int
set_menu_userptr(MENU *m, char *c)
{
	if (m) {
		Muserptr(m) = c;
	} else {
		Muserptr(Dfl_Menu) = c;
	}
	return (E_OK);
}

char *
menu_userptr(MENU *m)
{
	return (Muserptr(m ? m : Dfl_Menu));
}
