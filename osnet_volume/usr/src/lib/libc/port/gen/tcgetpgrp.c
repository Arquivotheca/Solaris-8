/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)tcgetpgrp.c	1.8	96/10/15 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#pragma weak tcgetpgrp = _tcgetpgrp

#include "synonyms.h"
#include <sys/termios.h>
#include <sys/types.h>
#include <unistd.h>

pid_t
tcgetpgrp(int fd)
{
	pid_t pgrp;

	if (tcgetsid(fd) < 0 || ioctl(fd, TIOCGPGRP, &pgrp) < 0)
		return (-1);
	return (pgrp);
}
