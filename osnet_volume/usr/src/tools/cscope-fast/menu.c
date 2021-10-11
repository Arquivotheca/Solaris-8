/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)menu.c	1.1	99/01/11 SMI"

/*
 *	cscope - interactive C symbol cross-reference
 *
 *	mouse menu functions
 */

#include "global.h"	/* changing */

static	MOUSEMENU mainmenu[] = {
	"Send",		"##\033s##\r",
	"Repeat",	"\01",
	"Rebuild",	"\022",
	"Help",		"?",
	"Exit",		"\04",
	0,		0
};

static	MOUSEMENU changemenu[] = {
	"Mark Screen",	"*",
	"Mark All",	"a",
	"Change",	"\04",
	"Continue",	"\b",	/* key that will be ignored at Select prompt */
	"Help",		"?",
	"Exit",		"\r",
	0,		0
};

/* initialize the mouse menu */

void
initmenu(void)
{
	if (changing == YES) {
		downloadmenu(changemenu);
	} else {
		downloadmenu(mainmenu);
	}
}
