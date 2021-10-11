/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)mbtowc.c 1.4 97/08/22 SMI"

/*LINTLIBRARY*/

#include <widec.h>
#include "synonyms.h"
#include <ctype.h>
#include <sys/types.h>
#include "curses_wchar.h"

int
_curs_mbtowc(wchar_t *wchar, const char *s, size_t n)
{
	int length, c;
	wchar_t intcode;
	char *olds = (char *)s;
	wchar_t mask;

	if (s == (char *)0)
		return (0);
	if (n == 0)
		return (-1);
	c = (unsigned char)*s++;
	if (c < 0200) {
		if (wchar)
			*wchar = c;
		return (c ? 1 : 0);
	}
	intcode = 0;
	if (c == SS2) {
		if ((length = eucw2) == 0)
			goto lab1;
		mask = P01;
		goto lab2;
	} else if (c == SS3) {
		if ((length = eucw3) == 0)
			goto lab1;
		mask = P10;
		goto lab2;
	}
lab1:
	if (iscntrl(c)) {
		if (wchar)
			*wchar = c;
		return (1);
	}
	length = eucw1 - 1;
	mask = P11;
	intcode = c & 0177;
lab2:
	if (length + 1 > n || length < 0)
		return (-1);
	while (length--) {
		if ((c = (unsigned char)*s++) < 0200 || iscntrl(c))
			return (-1);
		intcode = (intcode << 7) | (c & 0x7F);
	}
	if (wchar)
		*wchar = intcode | mask;
	/*LINTED*/
	return ((int) (s - olds));
}	
