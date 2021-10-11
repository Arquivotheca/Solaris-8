/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)mbgetwidth.c 1.3 97/06/25 SMI"

/*LINTLIBRARY*/

#include	"curses_inc.h"
#include	<sys/types.h>
#include	<ctype.h>

#define	CSWIDTH	514

short		cswidth[4] = {-1, 1, 1, 1};	/* character length */
short		_curs_scrwidth[4] = {1, 1, 1, 1};	/* screen width */

/*
 * This function is called only once in a program.
 * Before cgetwidth() is called, setlocale() must be called.
 */

void
mbgetwidth(void)
{
	unsigned char *cp = &__ctype[CSWIDTH];

	cswidth[0] = cp[0];
	cswidth[1] = cp[1];
	cswidth[2] = cp[2];
	_curs_scrwidth[0] = cp[3];
	_curs_scrwidth[1] = cp[4];
	_curs_scrwidth[2] = cp[5];

}

int
mbeucw(int c)
{
	c &= 0xFF;

	if (c & 0x80) {
		if (c == SS2) {
			return (cswidth[1]);
		} else if (c == SS3) {
			return (cswidth[2]);
		}
		return (cswidth[0]);
	}
	return (1);
}

int
mbscrw(int c)
{
	c &= 0xFF;

	if (c & 0x80) {
		if (c == SS2) {
			return (_curs_scrwidth[1]);
		} else if (c == SS3) {
			return (_curs_scrwidth[2]);
		}
		return (_curs_scrwidth[0]);
	}
	return (1);
}

int
wcscrw(wchar_t wc)
{
	int	rv;

	switch (wc & EUCMASK) {
	case	P11:	/* Code set 1 */
		rv = _curs_scrwidth[0];
		break;
	case	P01:	/* Code set 2 */
		rv = _curs_scrwidth[1];
		break;
	case	P10:	/* Code set 3 */
		rv = _curs_scrwidth[2];
		break;
	default	:	/* Code set 0 */
		rv = 1;
		break;
	}

	return (rv);
}
