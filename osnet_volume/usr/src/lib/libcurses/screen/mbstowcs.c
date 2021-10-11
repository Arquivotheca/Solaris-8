/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)mbstowcs.c 1.3 97/06/25 SMI"

/*LINTLIBRARY*/

#include <widec.h>
#include <sys/types.h>
#include "synonyms.h"
#include <stdlib.h>
#include "curses_inc.h"

size_t
_curs_mbstowcs(wchar_t *pwcs, const char *s, size_t n)
{
	int	i, val;

	for (i = 0; i < n; i++) {
		if ((val = _curs_mbtowc(pwcs++, s, MB_CUR_MAX)) == -1)
			return (val);
		if (val == 0)
			break;
		s += val;
	}
	return (i);
}
