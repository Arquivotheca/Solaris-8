/*
 *	detach.c
 *
 * Copyright (c) 1988-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)detach.c	1.9	98/07/27 SMI"

#include <sys/termios.h>
#include <fcntl.h>

/*
 * detach from tty
 */
detachfromtty()
{
	int tt;

	close(0);
	close(1);
	close(2);
	switch (fork1()) {
	case -1:
		perror("fork1");
		break;
	case 0:
		break;
	default:
		exit(0);
	}

	/* become session leader, and disassociate from controlling tty */
	(void) setsid();

	(void) open("/dev/null", O_RDWR, 0);
	dup(0);
	dup(0);
}
