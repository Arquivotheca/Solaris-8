/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)global.c	1.5	97/09/17 SMI"	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "menu.h"

MENU _Default_Menu = {
	16,				/* height */
	1,				/* width */
	16,				/* rows */
	1,				/* cols */
	16,				/* frows */
	1,				/* fcols */
	0,				/* namelen */
	0,				/* desclen */
	1,				/* marklen */
	1,				/* itemlen */
	(char *) NULL,			/* pattern */
	0,				/* pindex */
	(WINDOW *) NULL,		/* win */
	(WINDOW *) NULL,		/* sub */
	(WINDOW *) NULL,		/* userwin */
	(WINDOW *) NULL,		/* usersub */
	(ITEM **) NULL,			/* items */
	0,				/* nitems */
	(ITEM *) NULL,			/* curitem */
	0,				/* toprow */
	' ',				/* pad */
	A_STANDOUT,			/* fore */
	A_NORMAL,			/* back */
	A_UNDERLINE,			/* grey */
	(PTF_void) NULL,		/* menuinit */
	(PTF_void) NULL,		/* menuterm */
	(PTF_void) NULL,		/* iteminit */
	(PTF_void) NULL,		/* itemterm */
	(char *) NULL,			/* userptr */
	"-",				/* mark */
	O_ONEVALUE|O_SHOWDESC|
	O_ROWMAJOR|O_IGNORECASE|
	O_SHOWMATCH|O_NONCYCLIC,	/* opt */
	0				/* status */
};

ITEM _Default_Item = {
	(char *) NULL,			/* name.str */
	0,				/* name.length */
	(char *) NULL,			/* description.str */
	0,				/* description.length */
	0,				/* index */
	0,				/* imenu */
	FALSE,				/* value */
	(char *) NULL,			/* userptr */
	O_SELECTABLE,			/* opt */
	0,				/* status */
	0,				/* y */
	0,				/* x */
	0,				/* up */
	0,				/* down */
	0,				/* left */
	0				/* right */
};
