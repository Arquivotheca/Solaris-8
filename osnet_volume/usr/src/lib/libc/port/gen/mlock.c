/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)mlock.c	1.9	97/02/12 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/
#pragma weak mlock = _mlock

#include "synonyms.h"
#include <sys/types.h>
#include <sys/mman.h>

/*
 * Function to lock address range in memory.
 */
int
mlock(caddr_t addr, size_t len)
{
	return (memcntl(addr, len, MC_LOCK, 0, 0, 0));
}
