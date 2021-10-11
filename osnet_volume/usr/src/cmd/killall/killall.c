/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#ident	"@(#)killall.c	1.7	98/12/14 SMI"	/* SVr4.0 1.21	*/

#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <utmpx.h>
#include <stdio.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int sig;
	struct utmpx *u;
	char usage[] = "usage: killall [signal]\n";
	char perm[] = "permission denied\n";

	if (geteuid() != 0) {
		(void) write(2, perm, sizeof (perm)-1);
		return (1);
	}
	switch (argc) {
		case 1:
			sig = SIGTERM;
			break;
		case 2:
			if (str2sig(argv[1], &sig) == 0)
				break;
		default:
			(void) write(2, usage, sizeof (usage)-1);
			return (1);
	}

	while ((u = getutxent()) != NULL) {
		if ((u->ut_type == LOGIN_PROCESS) ||
		    (u->ut_type == USER_PROCESS))
			(void) kill(u->ut_pid, sig);
	}
	endutxent();

	return (0);
}
