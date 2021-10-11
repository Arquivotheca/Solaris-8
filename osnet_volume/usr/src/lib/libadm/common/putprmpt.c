/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)putprmpt.c	1.6	97/07/22 SMI"	/* SVr4.0 1.1 */
/*LINTLIBRARY*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "libadm.h"

void
putprmpt(FILE *fp, char *prompt, char *choices[], char *defstr)
{
	char buffer[1024];	 /* NB: code should prevent overflow... */
	int i, n;

	(void) fputc('\n', fp);
	if (prompt == NULL) {
		(void) strcpy(buffer, defstr ? defstr : "No default prompt.");
	} else if (n = (int)strlen(prompt)) {
		if (defstr == NULL)
			defstr = "";
		if (prompt[0] == '~')
			(void) sprintf(buffer, "%s%s", defstr, prompt + 1);
		else if (prompt[n-1] == '~')
			(void) sprintf(buffer, "%.*s%s",
				n - 1, prompt, defstr);
		else
			(void) strcpy(buffer, prompt);
	}
	if (!n || !strchr(":?", prompt[n-1])) {
		(void) strcat(buffer, "\\ [");
		for (i = 0; choices && choices[i]; ++i) {
			(void) strcat(buffer, choices[i]);
			(void) strcat(buffer, ",");
		}
		(void) strcat(buffer, ckquit ? "?,q] " : "?] ");
	} else
		(void) strcat(buffer, " ");

	(void) puttext(fp, buffer, 0, ckwidth);
}
