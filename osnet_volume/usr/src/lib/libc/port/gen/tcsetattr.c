/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)tcsetattr.c	1.9	96/10/15 SMI"	/* SVr4.0 1.2 */

/*LINTLIBRARY*/

#pragma weak tcsetattr = _tcsetattr

#include "synonyms.h"
#include <sys/types.h>
#include <sys/termios.h>
#include <errno.h>
#include <unistd.h>

/*
 * set parameters associated with termios
 */

int
tcsetattr(int fildes, int optional_actions, const struct termios *termios_p)
{

	int rval;

	switch (optional_actions) {

		case TCSANOW:

			rval = ioctl(fildes, TCSETS, termios_p);
			break;

		case TCSADRAIN:

			rval = ioctl(fildes, TCSETSW, termios_p);
			break;

		case TCSAFLUSH:

			rval = ioctl(fildes, TCSETSF, termios_p);
			break;

		default:

			rval = -1;
			errno = EINVAL;
	}
	return (rval);
}
