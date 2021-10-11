/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)tell.c	1.14	97/06/21 SMI"	/* SVr4.0 1.9 */

/*LINTLIBRARY*/
/*
 * return offset in file.
 */

#include <sys/feature_tests.h>
#include <sys/isa_defs.h>

#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#pragma weak tell64 = _tell64
#else
#pragma weak tell = _tell
#endif

#include "synonyms.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#if !defined(_LP64) && _FILE_OFFSET_BITS == 64

off64_t
tell64(int f)
{
	return (lseek64(f, 0, SEEK_CUR));
}

#else

off_t
tell(int f)
{
	return (lseek(f, 0, SEEK_CUR));
}

#endif  /* _FILE_OFFSET_BITS == 64 */
