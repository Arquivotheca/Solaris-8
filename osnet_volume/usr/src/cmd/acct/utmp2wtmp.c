/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/
/*	Copyright (c) 1999 by Sun Microsystems, Inc. */
/*	All rights reserved. */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)utmp2wtmp.c	1.9	99/02/18 SMI"	/* SVr4.0 1.2	*/
/*
 *	create entries for users who are still logged on when accounting
 *	is being run. Look at utmpx, and update the time stamp. New info
 *	goes to wtmpx. Called by runacct. 
 */

#include <stdio.h>
#include <sys/types.h>
#include <utmpx.h>
#include <time.h>
#include <string.h>
#include <errno.h>

int
main(int argc, char **argv)
{
	struct utmpx *utmpx;
	FILE *fp;

	fp = fopen(WTMPX_FILE, "a+");
	if (fp == NULL) {
		fprintf(stderr, "%s: %s: %s\n", argv[0],
		    WTMPX_FILE, strerror(errno));
		exit(1);
	}

	while ((utmpx = getutxent()) != NULL) {
		if ((utmpx->ut_type == USER_PROCESS) && !(nonuser(*utmpx))) {
			time(&utmpx->ut_xtime);
			fwrite(utmpx, sizeof(*utmpx), 1, fp);
		}
	}
	fclose(fp);
	return;
}
