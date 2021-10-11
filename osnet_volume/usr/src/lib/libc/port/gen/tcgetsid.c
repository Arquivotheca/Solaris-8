/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)tcgetsid.c	1.8	96/10/15 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#pragma weak tcgetsid = _tcgetsid

#include "synonyms.h"
#include <sys/termios.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

pid_t
tcgetsid(int fd)
{
	pid_t ttysid, mysid;

	if ((ioctl(fd, TIOCGSID, &ttysid)) < 0 ||
	    (mysid = getsid(0)) < 0)
		return (-1);
	if (mysid != ttysid) {
		errno = ENOTTY;
		return (-1);
	}
	return (mysid);
}
