/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)tcdrain.c	1.10	96/10/15 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/termios.h>
#include <unistd.h>
#include <sys/types.h>

/*
 * wait until all output on the filedes is drained
 */
#pragma weak	_libc_tcdrain = _tcdrain

int
tcdrain(int fildes)
{
	return (ioctl(fildes, TCSBRK, 1));
}
