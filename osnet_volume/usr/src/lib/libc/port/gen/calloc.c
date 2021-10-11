/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)calloc.c	1.11	99/04/06 SMI"	/* SVr4.0 1.14	*/

/*LINTLIBRARY*/
#include "synonyms.h"
#include "shlib.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#undef calloc		/* XXX	huh? */

/*
 * calloc - allocate and clear memory block
 */
void *
calloc(size_t num, size_t size)
{
	void *mp;
	size_t total;

	if (num == 0 || size == 0)
		total = 0;
	else {
		total = num * size;

		/* check for overflow */
		if (total / num != size) {
		    errno = ENOMEM;
		    return (0);
		}
	}
	return ((mp = malloc(total)) ? memset(mp, 0, total) : mp);
}
