/*
 * Copyright (c) 1989-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)madvise.c	1.6	97/02/12 SMI"

/*LINTLIBRARY*/

#pragma weak madvise = _madvise

#include "synonyms.h"
#include <sys/types.h>
#include <sys/mman.h>

/*
 * Function to provide advise to vm system to optimize its
 * management of the memory resources of a particular application.
 */
int
madvise(caddr_t addr, size_t len, int advice)
{
	return (memcntl(addr, len, MC_ADVISE, (caddr_t)advice, 0, 0));
}
