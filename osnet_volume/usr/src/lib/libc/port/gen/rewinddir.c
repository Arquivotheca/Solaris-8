/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)rewinddir.c	1.10	96/10/25 SMI"	/* SVr4.0 1.4 */

/*LINTLIBRARY*/

/*
	rewinddir -- C library extension routine
*/

#pragma weak rewinddir = _rewinddir

#include "synonyms.h"
#include <sys/types.h>
#include <dirent.h>

void
_rewinddir(DIR *dirp)
{
	(void) seekdir(dirp, 0L);
}
