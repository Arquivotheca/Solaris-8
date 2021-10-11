/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Mircrosystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)newitem.c	1.5	97/09/17 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <stdlib.h>
#include <strings.h>
#include "private.h"

ITEM *
new_item(char *name, char *desc)
{
	ITEM *item;

	if (item = (ITEM *) calloc(1, sizeof (ITEM))) {
		/* Set all default values */
		*item = *Dfl_Item;

		/* And set user values */
		Name(item) = name;
		Description(item) = desc;

		if (name && *name != '\0') {
			NameLen(item) = strlen(name);
		} else {
			free(item);		/* Can't have a null name */
			return ((ITEM *) NULL);
		}
		if (desc && *desc != '\0') {
			DescriptionLen(item) = strlen(desc);
		} else {
			DescriptionLen(item) = 0;
		}
	}
	return (item);
}

int
free_item(ITEM *i)
{
	if (!i) {
		return (E_BAD_ARGUMENT);
	}
	/* Make sure none of the items have pointers to menus. */
	if (Imenu(i)) {
		return (E_CONNECTED);
	}
	free(i);
	return (E_OK);
}

char *
item_name(ITEM *i)
{
	if (i) {
		return (Name(i));
	}
	return (NULL);
}

char *
item_description(ITEM *i)
{
	if (i) {
		return (Description(i));
	}
	return (NULL);
}
