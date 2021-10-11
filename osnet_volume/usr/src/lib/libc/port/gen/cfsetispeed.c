/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)cfsetispeed.c	1.9	96/10/15 SMI"	/* SVr4.0 1.1	*/

/* LINTLIBRARY */

#pragma weak cfsetispeed = _cfsetispeed
#include "synonyms.h"
#include <sys/types.h>
#include <sys/termios.h>

/*
 * sets the input baud rate stored in c_cflag to speed
 */

int
cfsetispeed(struct termios *termios_p, speed_t speed)
{
	/*
	 * If the input speed is zero, set it to output speed
	 */
	if (speed == 0) {
		speed = termios_p->c_cflag & CBAUD;
		if (termios_p->c_cflag & CBAUDEXT)
			speed += (CBAUD + 1);
	}

	if ((speed << 16) > CIBAUD) {
		termios_p->c_cflag |= CIBAUDEXT;
		speed -= ((CIBAUD >> 16) + 1);
	} else
		termios_p->c_cflag &= ~CIBAUDEXT;
	termios_p->c_cflag =
	    (termios_p->c_cflag & ~CIBAUD) | ((speed << 16) & CIBAUD);
	return (0);
}
