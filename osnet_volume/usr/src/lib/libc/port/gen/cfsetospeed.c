/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cfsetospeed.c	1.9	96/10/15 SMI"	/* SVr4.0 1.1	*/

/* LINTLIBRARY */

#pragma weak cfsetospeed = _cfsetospeed
#include "synonyms.h"
#include <sys/types.h>
#include <sys/termios.h>

/*
 * sets the output baud rate stored in c_cflag to speed
 */

int
cfsetospeed(struct termios *termios_p, speed_t speed)
{
	if (speed > CBAUD) {
		termios_p->c_cflag |= CBAUDEXT;
		speed -= (CBAUD + 1);
	} else
		termios_p->c_cflag &= ~CBAUDEXT;

	termios_p->c_cflag =
	    (termios_p->c_cflag & ~CBAUD) | (speed & CBAUD);
	return (0);
}
