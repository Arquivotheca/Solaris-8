/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)eaccess.c	1.9	97/03/28 SMI"	/* SVr4.0 2.2.4.3 */

/*LINTLIBRARY*/

/*
 * Determine if the effective user id has the appropriate permission
 * on a file.
*/

#pragma weak eaccess = _eaccess

#include "synonyms.h"
#include <sys/types.h>
#include <libgen.h>
#include <unistd.h>

int
eaccess(const char *path, int amode)
{
	/* Use effective id bits */
	return (access(path, 010|amode));
}
