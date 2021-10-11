/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ffs.c	1.7	96/11/15 SMI"	/* SVr4.0 1.3	*/

/* LINTLIBRARY */

#pragma weak ffs = _ffs

#include "synonyms.h"
#include <sys/types.h>
#include <string.h>

int
ffs(int field)
{
	int	idx = 1;

	if (field == 0)
		return (0);
	for (;;) {
		if (field & 1)
			return (idx);
		field >>= 1;
		++idx;
	}
}
