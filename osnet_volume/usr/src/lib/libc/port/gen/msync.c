/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)msync.c	1.11	97/02/12 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <sys/mman.h>

#pragma weak	_libc_msync = _msync

int
msync(caddr_t addr, size_t len, int flags)
{
	return (memcntl(addr, len, MC_SYNC, (caddr_t)(unsigned)flags, 0, 0));
}
