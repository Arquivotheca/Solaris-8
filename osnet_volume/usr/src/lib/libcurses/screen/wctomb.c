/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)wctomb.c 1.5 99/03/31 SMI"

/*LINTLIBRARY*/

#include <sys/types.h>
#include <widec.h>
#include <ctype.h>
#include "curses_wchar.h"

int
_curs_wctomb(char *s, wchar_t wchar)
{
	char *olds = s;
	int size, index;
	unsigned char d;
	if (!s)
		return (0);
	if (wchar <= 0177 || (wchar <= 0377 && (iscntrl((int)wchar) != 0)))  {
		/* LINTED */
		*s++ = (char)wchar;
		return (1);
	}
	switch (wchar & EUCMASK) {

		case P11:
			size = eucw1;
			break;

		case P01:
			/* LINTED */
			*s++ = (char)SS2;
			size = eucw2;
			break;

		case P10:
			/* LINTED */
			*s++ = (char)SS3;
			size = eucw3;
			break;

		default:
			return (-1);
	}
	if ((index = size) <= 0)
		return (-1);
	while (index--) {
		/* LINTED */
		d = wchar | 0200;
		wchar >>= 7;
		if (iscntrl(d))
			return (-1);
		s[index] = d;
	}
	/* LINTED */
	return ((int)(s - olds) + size);
}
