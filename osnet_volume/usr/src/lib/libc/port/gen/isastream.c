/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)isastream.c	1.9	96/11/26 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

/*
 * Check to see if a file descriptor is that of a stream.
 * Return 1 with errno set to 0 if it is. Otherwise,
 * return 0 with errno set to 0.
 * The only error returned is that case of a bad file desc.
 *
 */

#pragma weak isastream = _isastream

#include "synonyms.h"
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <stropts.h>

int
isastream(int fd)
{
	int rval;

	rval = ioctl(fd, I_CANPUT, 0);
	if (rval == -1 && errno == EBADF)
		return (-1);

	errno = 0;
	if (rval == 0 || rval == 1)
		return (1);

	return (0);
}
