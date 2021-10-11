/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)mlockall.c	1.9	96/11/26 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/
#pragma weak mlockall = _mlockall

#include "synonyms.h"
#include <sys/types.h>
#include <sys/mman.h>

/*
 * Function to lock address space in memory.
 */

mlockall(int flags)
{

	return (memcntl(0, 0, MC_LOCKAS, (caddr_t) (unsigned) flags, 0, 0));
}
