/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)valloc.c	1.6	96/12/04 SMI"

/*LINTLIBRARY*/

#pragma weak valloc = _valloc

#include "synonyms.h"
#include <stdlib.h>
#include <unistd.h>

void *
valloc(size_t size)
{
	static unsigned pagesize;

	if (!pagesize)
		pagesize = sysconf(_SC_PAGESIZE);

	return memalign(pagesize, size);
}
