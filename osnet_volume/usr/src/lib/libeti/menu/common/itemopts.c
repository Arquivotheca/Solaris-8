/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)itemopts.c	1.4	97/07/09 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

int
set_item_opts(ITEM *i, OPTIONS opt)
{
	if (i) {
		if (Iopt(i) != opt) {
			Iopt(i) = opt;
			/* If the item is being deactivated then unselect it */
			if ((opt & O_SELECTABLE) == 0) {
				if (Value(i)) {
					Value(i) = FALSE;
				}
			}
			if (Imenu(i) && Posted(Imenu(i))) {
				_move_post_item(Imenu(i), i);
				_show(Imenu(i));
			}
		}
	} else {
		Iopt(Dfl_Item) = opt;
	}
	return (E_OK);
}

int
item_opts_off(ITEM *i, OPTIONS opt)
{
	return (set_item_opts(i, (Iopt(i ? i : Dfl_Item)) & ~opt));
}

int
item_opts_on(ITEM *i, OPTIONS opt)
{
	return (set_item_opts(i, (Iopt(i ? i : Dfl_Item)) | opt));
}

int
item_opts(ITEM *i)
{
	return (Iopt(i ? i : Dfl_Item));
}
