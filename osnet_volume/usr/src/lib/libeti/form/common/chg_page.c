/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)chg_page.c	1.4	97/09/17 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "utility.h"

#define	first(f)	(0)
#define	last(f)		(f->maxpage - 1)

/* next - return next page after current page(cyclic) */
static int
next(FORM *f)
{
	int p = P(f);

	if (++p > last(f))
		p = first(f);
	return (p);
}

/* prev - return previous page before current page(cyclic) */
static int
prev(FORM *f)
{
	int p = P(f);

	if (--p < first(f))
		p = last(f);
	return (p);
}

int
_next_page(FORM *f)
{
	return (_set_form_page(f, next(f), (FIELD *) 0));
}

int
_prev_page(FORM *f)
{
	return (_set_form_page(f, prev(f), (FIELD *) 0));
}

int
_first_page(FORM *f)
{
	return (_set_form_page(f, first(f), (FIELD *) 0));
}

int
_last_page(FORM *f)
{
	return (_set_form_page(f, last(f), (FIELD *) 0));
}
