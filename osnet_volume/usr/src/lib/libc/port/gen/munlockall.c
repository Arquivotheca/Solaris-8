/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)munlockall.c	1.8	96/11/20 SMI"	/* SVr4.0 1.3	*/
/*LINTLIBRARY*/

#pragma weak munlockall = _munlockall

#include "synonyms.h"
#include <sys/types.h>
#include <sys/mman.h>

/*
 * Function to unlock address space from memory.
 */

int
munlockall(void)
{

	return (memcntl(0, 0, MC_UNLOCKAS, 0, 0, 0));
}
