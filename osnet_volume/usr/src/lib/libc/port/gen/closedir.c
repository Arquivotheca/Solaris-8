/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)closedir.c	1.18	96/10/15 SMI"	/* SVr4.0 1.10	*/

/* LINTLIBRARY */

/*
 *	closedir -- C library extension routine
 */

#pragma weak closedir = _closedir
#include "synonyms.h"
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>


int
closedir(DIR *dirp)
{
	int tmp_fd = dirp->dd_fd;

	free((char *)dirp);
	return (close(tmp_fd));
}
