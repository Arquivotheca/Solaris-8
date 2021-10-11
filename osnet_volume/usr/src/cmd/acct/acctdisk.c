/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)acctdisk.c	1.6	97/10/14 SMI"	/* SVr4.0 1.8	*/
/*
 *	acctdisk <dtmp >dtacct
 *	reads std.input & converts to tacct.h format, writes to output
 *	input:
 *	uid	name	#blocks
 */

#include <sys/types.h>
#include "acctdef.h"
#include <stdio.h>

struct	tacct	tb;
char	ntmp[NSZ+1];

int
main(argc, argv)
char **argv;
{
	int rc;

	tb.ta_dc = 1;
	while ((rc = scanf("%ld\t%s\t%f",
		&tb.ta_uid,
		ntmp,
		&tb.ta_du)) == 3) {

		CPYN(tb.ta_name, ntmp);
		fwrite(&tb, sizeof (tb), 1, stdout);
	}

	if (rc != EOF) {
		fprintf(stderr, "\nacctdisk: incorrect input format.\n");
		exit(1);
	} else {
		exit(0);
	}
}
