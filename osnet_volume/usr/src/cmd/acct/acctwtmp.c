/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/
/*	Copyright (c) 1999 by Sun Microsystems, Inc. */
/*	All rights reserved. */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)acctwtmp.c	1.10	99/02/18 SMI"	/* SVr4.0 1.9	*/
/*
 *	acctwtmp reason /var/adm/wtmpx
 *	writes utmpx.h record (with current time) to specific file
 *	acctwtmp `uname` /var/adm/wtmpx as part of startup
 *	acctwtmp pm /var/adm/wtmpx  (taken down for pm, for example)
 */
#include <stdio.h>
#include <sys/types.h>
#include "acctdef.h"
#include <utmpx.h>
#include <strings.h>

struct	utmpx	wb;

int
main(int argc, char **argv)
{
	struct utmpx *p;

	if (argc < 3)
		(void) fprintf(stderr, "Usage: %s reason wtmpx_file\n",
			argv[0]), exit(1);

	(void) strncpy(wb.ut_line, argv[1], sizeof (wb.ut_line));
	wb.ut_line[11] = NULL;
	wb.ut_type = ACCOUNTING;
	time(&wb.ut_xtime);
	utmpxname(argv[2]);
	setutxent();

	if (pututxline(&wb) == NULL)
		printf("acctwtmp - pututxline failed\n");
	endutxent();
	exit(0);
}
