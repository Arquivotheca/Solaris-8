/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)itemvalue.c	1.4	97/07/09 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "private.h"

int
set_item_value(ITEM *i, int v)
{
	/* Values can only be set on active values */
	if (i) {
		if (!Selectable(i) || (Imenu(i) && OneValue(Imenu(i)))) {
			return (E_REQUEST_DENIED);
		}
		if (Value(i) != v) {
			Value(i) = v;
			if (Imenu(i) && Posted(Imenu(i))) {
				_move_post_item(Imenu(i), i);
				_show(Imenu(i));
			}
		}
	} else {
		Value(Dfl_Item) = v;
	}
	return (E_OK);
}

int
item_value(ITEM *i)
{
	return (Value(i ? i : Dfl_Item));
}
