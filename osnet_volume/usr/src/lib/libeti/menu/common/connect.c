/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)connect.c	1.5	97/09/17 SMI"	/* SVr4.0 1.8	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <stdlib.h>
#include "private.h"

/* Connect and disconnect an item list from a menu */


/* Find the maximum length name and description */

static void
maxlengths(MENU *m)
{
	int maxn, maxd;
	ITEM **ip;

	maxn = maxd = 0;
	for (ip = Items(m); *ip; ip++) {
		if (NameLen(*ip) > maxn) {
			maxn = NameLen(*ip);
		}
		if (DescriptionLen(*ip) > maxd) {
			maxd = DescriptionLen(*ip);
		}
	}
	MaxName(m) = maxn;
	MaxDesc(m) = maxd;
}

int
_connect(MENU *m, ITEM **items)
{
	ITEM **ip;
	int i;

	/* Is the list of items connected to any other menu? */
	for (ip = items; *ip; ip++) {
		/* Return Null if item points to a menu */
		if (Imenu(*ip)) {
			return (FALSE);
		}
	}

	for (i = 0, ip = items; *ip; ip++) {
		/* Return FALSE if this item is a prevoious item */
		if (Imenu(*ip)) {
			for (ip = items; *ip; ip++) {
				/* Reset index and menu pointers */
				Index(*ip) = 0;
				Imenu(*ip) = (MENU *) NULL;
			}
			return (FALSE);
		}
		if (OneValue(m)) {
			/* Set all values to FALSE if selection not allowed */
			Value(*ip) = FALSE;
		}
		Index(*ip) = i++;
		Imenu(*ip) = m;
	}

	Nitems(m) = i;
	Items(m) = items;

	/* Go pick up the sizes of names and descriptions */
	maxlengths(m);

	/* Set up match buffer */
	if ((Pattern(m) = (char *)malloc((unsigned)MaxName(m)+1)) ==
	    (char *)0) {
		return (FALSE);
	}

	IthPattern(m, 0) = '\0';
	Pindex(m) = 0;
	(void) set_menu_format(m, FRows(m), FCols(m));
	Current(m) = IthItem(m, 0);
	Top(m) = 0;
	return (TRUE);
}

void
_disconnect(MENU *m)
{
	ITEM **ip;

	for (ip = Items(m); *ip; ip++) {
		/* Release items for another menu */
		Imenu(*ip) = (MENU *) NULL;
	}
	free(Pattern(m));
	Pattern(m) = NULL;
	Items(m) = (ITEM **) NULL;
	Nitems(m) = 0;
}
