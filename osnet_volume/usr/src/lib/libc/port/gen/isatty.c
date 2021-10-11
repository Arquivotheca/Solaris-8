/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)isatty.c	1.11	96/11/27 SMI"	/* SVr4.0 1.11	*/

/*LINTLIBRARY*/

#ifndef ABI
#pragma weak isatty = _isatty
#endif
#include "synonyms.h"
#include "shlib.h"
#include <sys/types.h>
#include <sys/termio.h>
#include <errno.h>
#include <unistd.h>

/*
 * Returns 1 iff file is a tty
 */
int
isatty(int f)
{
	struct termio tty;
	int err;

	err = errno;
	if (ioctl(f, TCGETA, &tty) < 0) {
		errno = err;
		return (0);
	}
	return (1);
}
