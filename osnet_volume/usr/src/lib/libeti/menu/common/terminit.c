/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)terminit.c	1.4	97/07/09 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

int
set_menu_init(MENU *m, PTF_void mi)
{
	if (m) {
		SMinit(m) = mi;
	} else {
		SMinit(Dfl_Menu) = mi;
	}
	return (E_OK);
}

PTF_void
menu_init(MENU *m)
{
	return (SMinit(m ? m : Dfl_Menu));
}

int
set_menu_term(MENU *m, PTF_void mt)
{
	if (m) {
		SMterm(m) = mt;
	} else {
		SMterm(Dfl_Menu) = mt;
	}
	return (E_OK);
}

PTF_void
menu_term(MENU *m)
{
	return (SMterm(m ? m : Dfl_Menu));
}

int
set_item_init(MENU *m, PTF_void ii)
{
	if (m) {
		SIinit(m) = ii;
	} else {
		SIinit(Dfl_Menu) = ii;
	}
	return (E_OK);
}

PTF_void
item_init(MENU *m)
{
	return (SIinit(m ? m : Dfl_Menu));
}

int
set_item_term(MENU *m, PTF_void it)
{
	if (m) {
		SIterm(m) = it;
	} else {
		SIterm(Dfl_Menu) = it;
	}
	return (E_OK);
}

PTF_void
item_term(MENU *m)
{
	return (SIterm(m ? m : Dfl_Menu));
}
