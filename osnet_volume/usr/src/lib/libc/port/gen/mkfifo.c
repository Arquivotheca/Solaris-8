/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)mkfifo.c	1.9	96/11/27 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

/*
 * mkfifo(3c) - create a named pipe (FIFO). This code provides
 * a POSIX mkfifo function.
 *
 */

#pragma weak mkfifo = _mkfifo

#include "synonyms.h"
#include <sys/types.h>
#include <sys/stat.h>

int
mkfifo(const char *path, mode_t mode)
{
	mode &= 0777;		/* only allow file access permissions */
	mode |= S_IFIFO;	/* creating a FIFO	*/
	return (mknod(path, mode, 0));
}
