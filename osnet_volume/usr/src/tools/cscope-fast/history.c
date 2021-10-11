/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)history.c	1.1	99/01/11 SMI"

/*
 *	cscope - interactive C symbol or text cross-reference
 *
 *	command history
 */

#include <stdio.h>
#include "global.h"

HISTORY *head, *tail, *current;

/* add a cmd to the history list */
void
addcmd(int f, char *s)
{
	HISTORY *h;

	h = (HISTORY *)mymalloc(sizeof (HISTORY));
	if (tail) {
		tail->next = h;
		h->next = 0;
		h->previous = tail;
		tail = h;
	} else {
		head = tail = h;
		h->next = h->previous = 0;
	}
	h->field = f;
	h->text = stralloc(s);
	current = 0;
}

/* return previous history item */

HISTORY *
prevcmd(void)
{
	if (current) {
		if (current->previous)	/* stay on first item */
			return (current = current->previous);
		else
			return (current);
	} else if (tail)
		return (current = tail);
	else
		return (NULL);
}

/* return next history item */

HISTORY *
nextcmd(void)
{
	if (current) {
		if (current->next)	/* stay on first item */
			return (current = current->next);
		else
			return (current);
	} else
		return (NULL);
}

/* reset current to tail */

void
resetcmd(void)
{
	current = 0;
}

HISTORY *
currentcmd(void)
{
	return (current);
}
