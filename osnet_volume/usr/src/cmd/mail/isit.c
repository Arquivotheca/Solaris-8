/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)isit.c	1.4	95/01/30 SMI" 	/* SVr4.0 1.	*/
#include "mail.h"

/*
 * isit(lp, type) --  match "name" portion of 
 *		"name: value" pair
 *	lp	->	pointer to line to check
 *	type	->	type of header line to match
 * returns
 *	TRUE	-> 	lp matches header type (case independent)
 *	FALSE	->	no match
 *
 *  Execpt for H_FORM type, matching is case insensitive (bug 1173101)
 */
int
isit(lp, type)
register char 	*lp;
register int	type;
{
	register char	*p;

	switch (type) {
	case H_FROM:
		for (p = header[type].tag; *lp && *p; lp++, p++) {
			if (*p != *lp)  {
				return(FALSE);
			}
		}
		break;
	default:
		for (p = header[type].tag; *lp && *p; lp++, p++) {
			if (toupper(*p) != toupper(*lp))  {
				return(FALSE);
			}
		}
		break;
	}
	if (*p == NULL) {
		return(TRUE);
	}
	return(FALSE);
}
