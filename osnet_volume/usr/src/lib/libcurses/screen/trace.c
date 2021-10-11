/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)trace.c	1.7	97/06/25 SMI"	/* SVr4.0 1.8	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <stdio.h>
#include "curses_inc.h"

int
traceon(void)
{
#ifdef DEBUG
	if (outf == NULL) {
		outf = fopen("trace", "a");
		if (outf == NULL) {
			perror("trace");
			exit(-1);
		}
		fprintf(outf, "trace turned on\n");
	}
#endif /* DEBUG */
	return (OK);
}

int
traceoff(void)
{
#ifdef DEBUG
	if (outf != NULL) {
		fprintf(outf, "trace turned off\n");
		fclose(outf);
		outf = NULL;
	}
#endif /* DEBUG */
	return (OK);
}

#ifdef DEBUG
#include <ctype.h>

char *
_asciify(char *str)
{
	static	char	string[1024];
	char	*p1 = string;
	char	*p2;
	char	c;

	while (c = *str++) {
		p2 = unctrl(c);
		while (*p1 = *p2++)
			p1++;
	}
	return (string);
}
#endif /* DEBUG */
