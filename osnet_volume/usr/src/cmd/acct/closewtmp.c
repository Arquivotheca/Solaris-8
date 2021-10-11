/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/
/*	Copyright (c) 1999 by Sun Microsystems, Inc. */
/*	All rights reserved. */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)closewtmp.c	1.6	99/02/18 SMI"	/* SVr4.0 1.2	*/

/*
 *	fudge an entry to wtmpx for each user who is still logged on when
 *	acct is being run. This entry marks a DEAD_PROCESS, and the
 *	current time as time stamp. This should be done before connect
 *	time is processed. Called by runacct.
 */

#include <stdio.h>
#include <sys/types.h>
#include <utmpx.h>

int
main(int argc, char **argv)
{
	struct utmpx *utmpx;

	setutxent();
	while ((utmpx = getutxent()) != NULL) {
		if (utmpx->ut_type == USER_PROCESS) {
			utmpx->ut_type = DEAD_PROCESS;
			time(&utmpx->ut_xtime);
			(void) pututxline(utmpx);
		}
	}
	endutxent();
	return (0);
}
