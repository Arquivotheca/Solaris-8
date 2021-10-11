/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)scrwidth.c	1.8	96/12/03 SMI"

/*LINTLIBRARY*/

#pragma weak scrwidth = _scrwidth

#include	"synonyms.h"
#include	<sys/types.h>
#include	<stdlib.h>
#include	<wchar.h>
#include	<libw.h>

int
scrwidth(wchar_t c)
{
	int ret;

	if (!iswprint(c))
		return (0);

	if (!(c & ~0xff)) {
		return (1);
	} else {
		if ((ret = wcwidth(c)) == -1)
			return (0);
		return (ret);
	}
}
